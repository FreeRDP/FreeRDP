/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard Virtual Channel Server Bridge for the Windows Shadow Subsystem
 *
 * Copyright 2026 Mengxuan Cao
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <freerdp/config.h>

#include <winpr/assert.h>

#include "win_shadow_cliprdr.h"

#if defined(CHANNEL_CLIPRDR_SERVER)

/* Include order matters: the NIIF_* macros from shellapi.h collide with the
 * same-named enum members in rail.h, so all FreeRDP headers must come before
 * the Windows/Shell headers. */
#include <winpr/platform.h>

#include <freerdp/log.h>
#include <freerdp/utils/cliprdr_utils.h>

#include <freerdp/channels/cliprdr.h>
#include <freerdp/server/cliprdr.h>

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#define TAG SERVER_TAG("shadow.win.cliprdr")

/* Clipboard semantics are a poor fit for bulk transfers, so file pulls are
 * capped: a single file larger than CLIP_MAX_FILE_BYTES or a total larger
 * than CLIP_MAX_TOTAL_BYTES is rejected. */
#define CLIP_MAX_FILE_BYTES (512ull * 1024 * 1024)
#define CLIP_MAX_TOTAL_BYTES (2ull * 1024 * 1024 * 1024)
#define CLIP_CHUNK (4u * 1024 * 1024)
#define CLIP_MAX_FILES 256

#define FMT_BASE 0xC000 /* base for our self-assigned named format ids */

typedef struct
{
	WCHAR path[MAX_PATH]; /* local absolute path (data source for pulls) */
	WCHAR rel[MAX_PATH];  /* wire cFileName: path relative to drop root */
	DWORD attrs;
	UINT64 size;
} ClipFile;

typedef struct
{
	rdpShadowClient* client;
	CliprdrServerContext* ctx;

	HANDLE thread;  /* clipboard watcher thread */
	HWND hwnd;      /* message-only window owned by the thread */
	HANDLE ready;   /* set once hwnd was created */
	HANDLE stopEvt; /* set on uninit to leave the thread */
	LONG suppress;  /* count of WM_CLIPBOARDUPDATE caused by our own writes */

	UINT32 nextFmt;    /* named format id allocator */
	UINT32 fgdId;      /* our announced FileGroupDescriptorW id */
	UINT32 contentsId; /* FileContents id */
	UINT32 dropId;     /* PreferredDropEffect id */

	ClipFile* files; /* remote clipboard CF_HDROP snapshot (serves client pulls) */
	UINT32 nFiles;

	/* pull-in: files announced by the client, materialized under stageDir and
	 * then presented as a real CF_HDROP */
	FILEDESCRIPTORW* inDesc;
	UINT32 inCount;
	UINT32 inIdx;     /* index of the file currently being pulled */
	UINT64 inSize;    /* total size of the current file */
	UINT64 inGot;     /* bytes received of the current file */
	HANDLE inFile;    /* materialization target of the current file */
	UINT32 inStream;  /* self-incrementing streamId for contents requests */
	BOOL inAwaitSize; /* TRUE = awaiting SIZE response, FALSE = awaiting RANGE data */
	UINT64 inTotal;   /* accumulated bytes of this pull (total cap) */

	UINT32 reqQ[8]; /* pending ServerFormatDataRequest queue (FIFO) */
	UINT32 reqHead, reqTail;
	UINT32 peerFgdId; /* FileGroupDescriptorW id announced by the client */

	/* same-shape announce gate (ping-pong loop breaker) */
	UINT64 lastAnnounceSig;
	UINT64 lastAnnounceTick;

	WCHAR stageDir[MAX_PATH];
	CRITICAL_SECTION lock;
} ShadowClip;

static ShadowClip* clipOf(CliprdrServerContext* ctx)
{
	return (ShadowClip*)ctx->custom;
}

/* The ServerXxx channel send functions are NODISCARD: log send failures
 * instead of dropping them silently. */
static void cliprdr_check_send(UINT rc, const char* what)
{
	if (rc != CHANNEL_RC_OK)
		WLog_WARN(TAG, "%s failed with %" PRIu32, what, rc);
}

/* ---------- remote clipboard I/O (caller: channel thread, synchronous) ---------- */

static BOOL open_clipboard_retry(void)
{
	for (int i = 0; i < 20; ++i)
	{
		if (OpenClipboard(nullptr))
			return TRUE;
		Sleep(25);
	}
	return FALSE;
}

/* suppress must be incremented before writing the remote clipboard; the
 * watcher thread skips the resulting WM_CLIPBOARDUPDATE. */
static void write_remote_text(ShadowClip* clip, const BYTE* utf16, UINT32 len)
{
	HANDLE hMem = GlobalAlloc(GMEM_MOVEABLE, len + 2);
	if (!hMem)
		return;
	void* p = GlobalLock(hMem);
	if (p)
	{
		CopyMemory(p, utf16, len);
		ZeroMemory((BYTE*)p + len, 2); /* terminating NUL for CF_UNICODETEXT */
	}
	GlobalUnlock(hMem);
	InterlockedIncrement(&clip->suppress);
	if (open_clipboard_retry())
	{
		EmptyClipboard();
		if (!SetClipboardData(CF_UNICODETEXT, hMem))
		{
			GlobalFree(hMem);
			InterlockedDecrement(&clip->suppress);
		}
		CloseClipboard();
	}
	else
	{
		GlobalFree(hMem);
		InterlockedDecrement(&clip->suppress);
	}
}

/* Once the pull-in files are materialized under stageDir, present a real
 * CF_HDROP on the remote clipboard: Ctrl+V in Explorer then performs an
 * ordinary local file copy. */
static void write_remote_hdrop(ShadowClip* clip)
{
	UINT32 n = 0;
	SIZE_T bytes = sizeof(DROPFILES) + sizeof(WCHAR);
	WLog_INFO(TAG, "write_remote_hdrop: enter inCount=%u", clip->inCount);
	EnterCriticalSection(&clip->lock);
	/* Only top-level entries go into DROPFILES: a top-level directory was
	 * materialized as a whole under stageDir and hands its full tree to
	 * Explorer; nested entries (rel containing '\\') are not listed. */
	for (UINT32 i = 0; i < clip->inCount; ++i)
	{
		if (!wcschr(clip->inDesc[i].cFileName, L'\\'))
		{
			WCHAR full[MAX_PATH];
			_snwprintf_s(full, MAX_PATH, _TRUNCATE, L"%s\\%s", clip->stageDir,
			             clip->inDesc[i].cFileName);
			bytes += (wcslen(full) + 1) * sizeof(WCHAR);
			++n;
		}
	}
	LeaveCriticalSection(&clip->lock);
	if (!n)
		return;

	HANDLE hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
	if (!hMem)
		return;
	DROPFILES* df = (DROPFILES*)GlobalLock(hMem);
	if (!df)
	{
		GlobalFree(hMem);
		return;
	}
	df->pFiles = sizeof(DROPFILES);
	df->fWide = TRUE;
	WCHAR* dst = (WCHAR*)((BYTE*)df + sizeof(DROPFILES));
	SIZE_T remain = bytes; /* includes the trailing NUL; shrinks per item */
	EnterCriticalSection(&clip->lock);
	for (UINT32 i = 0; i < clip->inCount; ++i)
	{
		if (wcschr(clip->inDesc[i].cFileName, L'\\'))
			continue;
		/* The size argument must be the space REMAINING at dst: dst advances
		 * per item, and passing MAX_PATH unconditionally can write past the
		 * allocation once accumulated paths overflow (heap corruption). */
		_snwprintf_s(dst, remain / sizeof(WCHAR), _TRUNCATE, L"%s\\%s", clip->stageDir,
		             clip->inDesc[i].cFileName);
		const SIZE_T wrote = (wcslen(dst) + 1) * sizeof(WCHAR);
		dst += wrote;
		remain -= wrote;
	}
	LeaveCriticalSection(&clip->lock);
	GlobalUnlock(hMem);

	/* PreferredDropEffect=COPY: Explorer treats the drop as a copy, not a move */
	const UINT cfDrop = RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
	HANDLE hEff = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
	if (hEff)
	{
		DWORD* e = (DWORD*)GlobalLock(hEff);
		if (e)
			*e = DROPEFFECT_COPY;
		GlobalUnlock(hEff);
	}

	/* Same-machine loop breaker: when client and server run on the same host,
	 * writing here triggers the client clipboard monitor to re-announce, the
	 * server pulls and writes again -- a full loop was observed closing in
	 * ~250ms. If the clipboard already holds a byte-identical HDROP, skip the
	 * write entirely: no clipboard change event, loop broken at lap one. */
	if (open_clipboard_retry())
	{
		BOOL same = FALSE;
		HANDLE cur = GetClipboardData(CF_HDROP);
		if (cur)
		{
			const SIZE_T csz = GlobalSize(cur);
			const DROPFILES* cdf = (const DROPFILES*)GlobalLock(cur);
			BYTE* nd = (BYTE*)GlobalLock(hMem);
			if (cdf && nd && cdf->fWide && csz == GlobalSize(hMem) &&
			    cdf->pFiles == sizeof(DROPFILES) && memcmp(cdf, nd, csz) == 0)
				same = TRUE;
			if (cdf)
				GlobalUnlock(cur);
			if (nd)
				GlobalUnlock(hMem);
		}
		CloseClipboard();
		if (same)
		{
			GlobalFree(hMem);
			if (hEff)
				GlobalFree(hEff);
			WLog_INFO(TAG, "write_remote_hdrop: skip -- clipboard already current");
			return;
		}
	}

	InterlockedIncrement(&clip->suppress);
	if (open_clipboard_retry())
	{
		EmptyClipboard();
		if (SetClipboardData(CF_HDROP, hMem))
		{
			if (hEff)
				SetClipboardData(cfDrop, hEff);
		}
		else
		{
			GlobalFree(hMem);
			if (hEff)
				GlobalFree(hEff);
			InterlockedDecrement(&clip->suppress);
		}
		CloseClipboard();
		WLog_INFO(TAG, "client files pasted to remote clipboard (%u files)", n);
	}
	else
	{
		GlobalFree(hMem);
		if (hEff)
			GlobalFree(hEff);
		InterlockedDecrement(&clip->suppress);
	}
}

/* ---------- remote clipboard snapshot (taken by the watcher thread) ---------- */

/* Recursive enumeration: directories expand into a flat list with rel storing
 * the path relative to the drop root ("dir\sub\f"). Directory entries carry
 * FILE_ATTRIBUTE_DIRECTORY as placeholders so the receiving side can rebuild
 * the tree. */
static void clipfiles_push(ClipFile** list, UINT32* n, UINT32* cap, const WCHAR* full,
                           const WCHAR* rel)
{
	DWORD a = GetFileAttributesW(full);
	if (a == INVALID_FILE_ATTRIBUTES || *n >= CLIP_MAX_FILES)
		return;
	if (*n >= *cap)
	{
		*cap = *cap ? *cap * 2 : 64;
		ClipFile* nl = (ClipFile*)realloc(*list, *cap * sizeof(ClipFile));
		if (!nl)
		{
			free(*list);
			*list = nullptr;
			*n = *cap = 0;
			return;
		}
		*list = nl;
	}
	ClipFile* cf = &(*list)[(*n)++];
	wcscpy_s(cf->path, MAX_PATH, full);
	wcscpy_s(cf->rel, MAX_PATH, rel);
	cf->attrs = a;
	cf->size = 0;
	if (!(a & FILE_ATTRIBUTE_DIRECTORY))
	{
		HANDLE f = CreateFileW(full, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
		                       OPEN_EXISTING, 0, nullptr);
		if (f != INVALID_HANDLE_VALUE)
		{
			LARGE_INTEGER sz;
			if (GetFileSizeEx(f, &sz))
				cf->size = (UINT64)sz.QuadPart;
			CloseHandle(f);
		}
		return;
	}
	/* Directory: keep the entry itself, then recurse into its children */
	WCHAR pat[MAX_PATH];
	_snwprintf_s(pat, MAX_PATH, _TRUNCATE, L"%s\\*", full);
	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW(pat, &fd);
	if (h == INVALID_HANDLE_VALUE)
		return;
	do
	{
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
		    (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")))
			continue;
		WCHAR sub[MAX_PATH], srel[MAX_PATH];
		_snwprintf_s(sub, MAX_PATH, _TRUNCATE, L"%s\\%s", full, fd.cFileName);
		_snwprintf_s(srel, MAX_PATH, _TRUNCATE, L"%s\\%s", rel, fd.cFileName);
		clipfiles_push(list, n, cap, sub, srel);
	} while (FindNextFileW(h, &fd) && *n < CLIP_MAX_FILES);
	FindClose(h);
}

static void snapshot_remote_files(ShadowClip* clip)
{
	HANDLE h = GetClipboardData(CF_HDROP); /* OpenClipboard already held by caller */
	if (!h)
		return;
	HDROP drop = (HDROP)h;
	UINT cnt = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
	ClipFile* list = nullptr;
	UINT32 real = 0, cap = 0;
	for (UINT i = 0; i < cnt && real < CLIP_MAX_FILES; ++i)
	{
		WCHAR path[MAX_PATH];
		if (!DragQueryFileW(drop, i, path, MAX_PATH))
			continue;
		const WCHAR* base = wcsrchr(path, L'\\');
		clipfiles_push(&list, &real, &cap, path, base ? base + 1 : path);
	}
	EnterCriticalSection(&clip->lock);
	free(clip->files);
	clip->files = list;
	clip->nFiles = real;
	LeaveCriticalSection(&clip->lock);
}

/* ---------- announce remote clipboard to the client (watcher thread) ---------- */

static void announce_local(ShadowClip* clip)
{
	if (!open_clipboard_retry())
		return;
	const BOOL hasText =
	    IsClipboardFormatAvailable(CF_UNICODETEXT) || IsClipboardFormatAvailable(CF_TEXT);
	const BOOL hasFiles = IsClipboardFormatAvailable(CF_HDROP);
	if (hasFiles)
		snapshot_remote_files(clip);
	CloseClipboard();

	/* Same-shape time gate: skip the announce if the format shape (text/files
	 * combination) did not change within 10s. With multiple clients plus local
	 * write-backs (write_remote_text/hdrop) the announces would ping-pong --
	 * every server announce resets the client's signature memory, so the gate
	 * has to live at the source. */
	{
		const UINT64 now = GetTickCount64();
		const UINT64 sig = ((UINT64)hasText << 1) | (UINT64)hasFiles;
		if (sig == clip->lastAnnounceSig && now - clip->lastAnnounceTick < 10000)
		{
			WLog_INFO(TAG, "announce_local: same shape within 10s -- skip");
			return;
		}
		clip->lastAnnounceSig = sig;
		clip->lastAnnounceTick = now;
	}

	CLIPRDR_FORMAT fmts[4];
	UINT32 n = 0;
	if (hasText)
	{
		fmts[n].formatId = CF_UNICODETEXT;
		fmts[n].formatName = nullptr;
		++n;
	}
	if (hasFiles && clip->nFiles)
	{
		fmts[n].formatId = clip->fgdId;
		fmts[n].formatName = _strdup("FileGroupDescriptorW");
		++n;
		fmts[n].formatId = clip->contentsId;
		fmts[n].formatName = _strdup("FileContents");
		++n;
		fmts[n].formatId = clip->dropId;
		fmts[n].formatName = _strdup("PreferredDropEffect");
		++n;
	}
	if (!n)
		return;
	const CLIPRDR_FORMAT_LIST list = {
		.common = { .msgType = CB_FORMAT_LIST, .msgFlags = 0, .dataLen = 0 },
		.numFormats = n,
		.formats = fmts
	};
	cliprdr_check_send(clip->ctx->ServerFormatList(clip->ctx, &list), "ServerFormatList");
	for (UINT32 i = 0; i < n; ++i)
		free(fmts[i].formatName);
}

/* ---------- pull-in: client files materialized remotely (channel thread) ---------- */

static void pull_abort(ShadowClip* clip)
{
	if (clip->inFile != INVALID_HANDLE_VALUE && clip->inFile)
		CloseHandle(clip->inFile);
	clip->inFile = nullptr;
	free(clip->inDesc);
	clip->inDesc = nullptr;
	clip->inCount = clip->inIdx = 0;
	clip->inSize = clip->inGot = clip->inTotal = 0;
	clip->inAwaitSize = FALSE;
}

/* Pull complete: hand the materialized files to the remote clipboard as a
 * real CF_HDROP. */
static void finish_pull(ShadowClip* clip)
{
	write_remote_hdrop(clip);
}

static void rm_tree(const WCHAR* dir)
{
	WCHAR pat[MAX_PATH];
	_snwprintf_s(pat, MAX_PATH, _TRUNCATE, L"%s\\*", dir);
	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW(pat, &fd);
	if (h == INVALID_HANDLE_VALUE)
		return;
	do
	{
		if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L".."))
			continue;
		WCHAR full[MAX_PATH];
		_snwprintf_s(full, MAX_PATH, _TRUNCATE, L"%s\\%s", dir, fd.cFileName);
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			rm_tree(full);
			RemoveDirectoryW(full);
		}
		else
		{
			DeleteFileW(full);
		}
	} while (FindNextFileW(h, &fd));
	FindClose(h);
}

static void wipe_stage_dir(ShadowClip* clip)
{
	CreateDirectoryW(clip->stageDir, nullptr);
	rm_tree(clip->stageDir);
}

/* Creates the directory chain under stageDir matching rel ("dir\sub\f");
 * with includeLast=FALSE the last segment is treated as a file name and only
 * the parent directories are created. Shared by directory descriptors and
 * file materialization. */
static void mkdirs_under(ShadowClip* clip, const WCHAR* rel, BOOL includeLast)
{
	WCHAR acc[MAX_PATH];
	wcscpy_s(acc, MAX_PATH, clip->stageDir);
	const WCHAR* p = rel;
	while (*p)
	{
		const WCHAR* bs = wcschr(p, L'\\');
		const size_t n = bs ? (size_t)(bs - p) : wcslen(p);
		if (!bs && !includeLast)
			break;
		const size_t l = wcslen(acc);
		if (l + 1 + n >= MAX_PATH)
			break;
		acc[l] = L'\\';
		wcsncpy_s(acc + l + 1, MAX_PATH - l - 1, p, n);
		acc[l + 1 + n] = 0;
		CreateDirectoryW(acc, nullptr);
		if (!bs)
			break;
		p = bs + 1;
	}
}

/* Advances the pull cursor: directory descriptors create their directory and
 * are skipped; the cursor stops at the next file descriptor. */
static void pull_skip_dirs(ShadowClip* clip)
{
	while (clip->inIdx < clip->inCount)
	{
		if (!(clip->inDesc[clip->inIdx].dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			break;
		mkdirs_under(clip, clip->inDesc[clip->inIdx].cFileName, TRUE);
		clip->inIdx++;
	}
}

/* Sends the next FileContentsRequest; FALSE means the pull is finished. */
static BOOL pull_step(ShadowClip* clip)
{
	if (clip->inIdx >= clip->inCount)
		return FALSE;
	CLIPRDR_FILE_CONTENTS_REQUEST req = { .common = { .msgType = CB_FILECONTENTS_REQUEST,
		                                              .msgFlags = 0 },
		                                  .streamId = ++clip->inStream,
		                                  .listIndex = clip->inIdx,
		                                  .dwFlags = 0,
		                                  .nPositionLow = 0,
		                                  .nPositionHigh = 0,
		                                  .cbRequested = 0,
		                                  .haveClipDataId = FALSE,
		                                  .clipDataId = 0 };
	if (clip->inGot == 0)
	{
		req.dwFlags = FILECONTENTS_SIZE;
		req.cbRequested = 8;
		clip->inAwaitSize = TRUE;
	}
	else
	{
		req.dwFlags = FILECONTENTS_RANGE;
		const UINT64 left = clip->inSize - clip->inGot;
		const UINT64 want = left < CLIP_CHUNK ? left : CLIP_CHUNK;
		req.nPositionLow = (UINT32)(clip->inGot & 0xFFFFFFFFull);
		req.nPositionHigh = (UINT32)(clip->inGot >> 32);
		req.cbRequested = (UINT32)want;
		clip->inAwaitSize = FALSE;
	}
	cliprdr_check_send(clip->ctx->ServerFileContentsRequest(clip->ctx, &req),
	                   "ServerFileContentsRequest");
	return TRUE;
}

/* Current file complete -> next file; all done -> place the CF_HDROP. */
static void pull_advance(ShadowClip* clip)
{
	if (clip->inFile && clip->inFile != INVALID_HANDLE_VALUE)
		CloseHandle(clip->inFile);
	clip->inFile = nullptr;
	clip->inIdx++;
	clip->inSize = clip->inGot = 0;
	pull_skip_dirs(clip);
	if (clip->inIdx >= clip->inCount)
	{
		finish_pull(clip);
		pull_abort(clip); /* release state, desc is consumed */
		return;
	}
	pull_step(clip);
}

static void on_client_file_contents(ShadowClip* clip, const CLIPRDR_FILE_CONTENTS_RESPONSE* r)
{
	if (!clip->inDesc || (r->common.msgFlags & CB_RESPONSE_FAIL))
	{
		WLog_WARN(TAG, "filecontents response fail/idle (flags=0x%x)", r->common.msgFlags);
		pull_abort(clip);
		return;
	}
	if (clip->inAwaitSize)
	{
		clip->inAwaitSize = FALSE; /* must clear before any early return below:
		                              otherwise a pending SIZE response would be
		                              re-parsed as RANGE data */
		if (r->common.dataLen < 8 || !r->requestedData)
		{
			pull_abort(clip);
			return;
		}
		clip->inSize = *(const UINT64*)r->requestedData;
		WLog_INFO(TAG, "pull: SIZE idx=%u size=%llu dataLen=%u", clip->inIdx,
		          (unsigned long long)clip->inSize, r->common.dataLen);
		if (clip->inSize > CLIP_MAX_FILE_BYTES ||
		    clip->inTotal + clip->inSize > CLIP_MAX_TOTAL_BYTES)
		{
			WLog_WARN(TAG, "file too large for clipboard transfer (%llu bytes)",
			          (unsigned long long)clip->inSize);
			pull_abort(clip);
			return;
		}
		mkdirs_under(clip, clip->inDesc[clip->inIdx].cFileName, FALSE);
		WCHAR full[MAX_PATH];
		_snwprintf_s(full, MAX_PATH, _TRUNCATE, L"%s\\%s", clip->stageDir,
		             clip->inDesc[clip->inIdx].cFileName);
		clip->inFile = CreateFileW(full, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		                           FILE_ATTRIBUTE_NORMAL, nullptr);
		if (clip->inFile == INVALID_HANDLE_VALUE)
		{
			WLog_WARN(TAG, "stage create failed err=%lu path=%ls", GetLastError(), full);
			pull_abort(clip);
			return;
		}
		if (clip->inSize == 0)
		{
			pull_advance(clip);
			return;
		}
		clip->inGot = 0;
		/* SIZE is in hand -> send the RANGE request right away instead of
		 * waiting for another response cycle. */
		CLIPRDR_FILE_CONTENTS_REQUEST req = {
			.common = { .msgType = CB_FILECONTENTS_REQUEST, .msgFlags = 0 },
			.streamId = ++clip->inStream,
			.listIndex = clip->inIdx,
			.dwFlags = FILECONTENTS_RANGE,
			.nPositionLow = 0,
			.nPositionHigh = 0,
			.cbRequested = (UINT32)(clip->inSize < CLIP_CHUNK ? clip->inSize : CLIP_CHUNK),
			.haveClipDataId = FALSE,
			.clipDataId = 0
		};
		cliprdr_check_send(clip->ctx->ServerFileContentsRequest(clip->ctx, &req),
		                   "ServerFileContentsRequest");
		return;
	}
	/* RANGE data chunk */
	if (clip->inFile && clip->inFile != INVALID_HANDLE_VALUE && r->requestedData)
	{
		DWORD wr = 0;
		LARGE_INTEGER off;
		off.QuadPart = (LONGLONG)clip->inGot;
		SetFilePointerEx(clip->inFile, off, nullptr, FILE_BEGIN);
		WriteFile(clip->inFile, r->requestedData, r->common.dataLen, &wr, nullptr);
		clip->inGot += wr;
		clip->inTotal += wr;
	}
	if (clip->inGot >= clip->inSize)
		pull_advance(clip);
	else
		pull_step(clip);
	WLog_INFO(TAG, "pull: RANGE idx=%u got=%u/%u", clip->inIdx, (unsigned)clip->inGot,
	          (unsigned)clip->inSize);
}

/* ---------- cliprdr server callbacks (cliprdr channel thread) ---------- */

static UINT srv_client_format_list(CliprdrServerContext* ctx, const CLIPRDR_FORMAT_LIST* list)
{
	ShadowClip* clip = clipOf(ctx);
	UINT32 textId = 0;
	UINT32 fgdId = 0;
	WLog_INFO(TAG, "format_list: %u formats", list->numFormats);
	for (UINT32 i = 0; i < list->numFormats; ++i)
	{
		const CLIPRDR_FORMAT* f = &list->formats[i];
		if (!f->formatName && (f->formatId == CF_UNICODETEXT || f->formatId == CF_TEXT))
			textId = f->formatId;
		else if (f->formatName && strcmp(f->formatName, "FileGroupDescriptorW") == 0)
			fgdId = f->formatId;
	}
	clip->peerFgdId = fgdId;
	/* Requests are queued: responses carry no formatId, so they are matched
	 * against the queue in send order. */
	if (textId)
	{
		clip->reqQ[clip->reqTail++ % 8] = textId;
		const CLIPRDR_FORMAT_DATA_REQUEST req = { .common = { .msgType = CB_FORMAT_DATA_REQUEST,
			                                                  .msgFlags = 0 },
			                                      .requestedFormatId = textId };
		cliprdr_check_send(ctx->ServerFormatDataRequest(ctx, &req), "ServerFormatDataRequest");
	}
	if (fgdId && ctx->streamFileClipEnabled)
	{
		clip->reqQ[clip->reqTail++ % 8] = fgdId;
		const CLIPRDR_FORMAT_DATA_REQUEST req = { .common = { .msgType = CB_FORMAT_DATA_REQUEST,
			                                                  .msgFlags = 0 },
			                                      .requestedFormatId = fgdId };
		cliprdr_check_send(ctx->ServerFormatDataRequest(ctx, &req), "ServerFormatDataRequest");
	}
	const CLIPRDR_FORMAT_LIST_RESPONSE resp = { .common = { .msgType = CB_FORMAT_LIST_RESPONSE,
		                                                    .msgFlags = CB_RESPONSE_OK } };
	cliprdr_check_send(ctx->ServerFormatListResponse(ctx, &resp), "ServerFormatListResponse");
	return CHANNEL_RC_OK;
}

static UINT srv_client_format_data_response(CliprdrServerContext* ctx,
                                            const CLIPRDR_FORMAT_DATA_RESPONSE* r)
{
	ShadowClip* clip = clipOf(ctx);
	UINT32 id = 0;
	if (clip->reqHead != clip->reqTail)
		id = clip->reqQ[clip->reqHead++ % 8];
	if (!(r->common.msgFlags & CB_RESPONSE_OK) || !r->requestedFormatData)
		return CHANNEL_RC_OK;
	if (id == CF_UNICODETEXT || id == CF_TEXT)
	{
		write_remote_text(clip, r->requestedFormatData, r->common.dataLen);
		return CHANNEL_RC_OK;
	}
	if (id == clip->peerFgdId && id != 0)
	{
		/* client file list -> start the materialization state machine */
		WLog_INFO(TAG, "pull: fgd response, start");
		pull_abort(clip);
		wipe_stage_dir(clip);
		if (cliprdr_parse_file_list(r->requestedFormatData, r->common.dataLen, &clip->inDesc,
		                            &clip->inCount) != NO_ERROR)
		{
			/* the parser keeps the partially filled array allocated on
			 * failure -- release it here */
			free(clip->inDesc);
			clip->inDesc = nullptr;
			clip->inCount = 0;
			return CHANNEL_RC_OK;
		}
		if (clip->inCount > CLIP_MAX_FILES)
			clip->inCount = CLIP_MAX_FILES;
		clip->inIdx = 0;
		clip->inSize = clip->inGot = clip->inTotal = 0;
		clip->inFile = nullptr;
		pull_skip_dirs(clip);
		if (clip->inIdx < clip->inCount)
			pull_step(clip);
		else
		{
			/* only directory descriptors (e.g. empty folders): nothing to
			 * stream, still place the materialized tree on the clipboard */
			finish_pull(clip);
			pull_abort(clip);
		}
		return CHANNEL_RC_OK;
	}
	return CHANNEL_RC_OK;
}

/* The client pulls data we announced (text / file descriptor list / drop
 * effect). */
static UINT srv_client_format_data_request(CliprdrServerContext* ctx,
                                           const CLIPRDR_FORMAT_DATA_REQUEST* req)
{
	ShadowClip* clip = clipOf(ctx);
	CLIPRDR_FORMAT_DATA_RESPONSE resp = { .common = { .msgType = CB_FORMAT_DATA_RESPONSE,
		                                              .msgFlags = CB_RESPONSE_FAIL,
		                                              .dataLen = 0 },
		                                  .requestedFormatData = nullptr };
	BYTE* buf = nullptr;
	if (req->requestedFormatId == CF_UNICODETEXT || req->requestedFormatId == CF_TEXT)
	{
		if (open_clipboard_retry())
		{
			HANDLE h = GetClipboardData(CF_UNICODETEXT);
			if (h)
			{
				const WCHAR* s = (const WCHAR*)GlobalLock(h);
				if (s)
				{
					const SIZE_T len = (wcslen(s) + 1) * sizeof(WCHAR);
					buf = (BYTE*)malloc(len);
					if (buf)
						CopyMemory(buf, s, len);
				}
				GlobalUnlock(h);
			}
			CloseClipboard();
			if (buf)
			{
				resp.common.msgFlags = CB_RESPONSE_OK;
				resp.common.dataLen = (UINT32)((wcslen((const WCHAR*)buf) + 1) * sizeof(WCHAR));
				resp.requestedFormatData = buf;
			}
		}
	}
	else if (req->requestedFormatId == clip->fgdId)
	{
		UINT32 len = 0;
		BYTE* data = nullptr;
		EnterCriticalSection(&clip->lock);
		FILEDESCRIPTORW* desc = (FILEDESCRIPTORW*)calloc(clip->nFiles, sizeof(FILEDESCRIPTORW));
		if (desc)
		{
			for (UINT32 i = 0; i < clip->nFiles; ++i)
			{
				const BOOL isDir = (clip->files[i].attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
				desc[i].dwFlags = FD_ATTRIBUTES | FD_PROGRESSUI | (isDir ? 0 : FD_FILESIZE);
				desc[i].dwFileAttributes = clip->files[i].attrs;
				desc[i].nFileSizeHigh = (DWORD)(clip->files[i].size >> 32);
				desc[i].nFileSizeLow = (DWORD)(clip->files[i].size & 0xFFFFFFFFu);
				wcscpy_s(desc[i].cFileName, sizeof(desc[i].cFileName) / sizeof(WCHAR),
				         clip->files[i].rel);
			}
			if (cliprdr_serialize_file_list(desc, clip->nFiles, &data, &len) == NO_ERROR)
			{
				buf = data;
				resp.common.msgFlags = CB_RESPONSE_OK;
				resp.common.dataLen = len;
				resp.requestedFormatData = buf;
			}
			free(desc);
		}
		LeaveCriticalSection(&clip->lock);
	}
	else if (req->requestedFormatId == clip->dropId)
	{
		buf = (BYTE*)malloc(sizeof(DWORD));
		if (buf)
		{
			*(DWORD*)buf = DROPEFFECT_COPY;
			resp.common.msgFlags = CB_RESPONSE_OK;
			resp.common.dataLen = sizeof(DWORD);
			resp.requestedFormatData = buf;
		}
	}
	cliprdr_check_send(ctx->ServerFormatDataResponse(ctx, &resp), "ServerFormatDataResponse");
	free(buf);
	return CHANNEL_RC_OK;
}

/* The client pulls SIZE/RANGE chunks of the files we announced. */
static UINT srv_client_file_contents_request(CliprdrServerContext* ctx,
                                             const CLIPRDR_FILE_CONTENTS_REQUEST* req)
{
	ShadowClip* clip = clipOf(ctx);
	CLIPRDR_FILE_CONTENTS_RESPONSE resp = { .common = { .msgType = CB_FILECONTENTS_RESPONSE,
		                                                .msgFlags = CB_RESPONSE_FAIL,
		                                                .dataLen = 0 },
		                                    .streamId = req->streamId,
		                                    .cbRequested = 0,
		                                    .requestedData = nullptr };
	BYTE* buf = nullptr;
	EnterCriticalSection(&clip->lock);
	if (req->listIndex < clip->nFiles && clip->files)
	{
		ClipFile* cf = &clip->files[req->listIndex];
		if (req->dwFlags & FILECONTENTS_SIZE)
		{
			buf = (BYTE*)malloc(8);
			if (buf)
			{
				*(UINT64*)buf = cf->size;
				resp.common.msgFlags = CB_RESPONSE_OK;
				resp.common.dataLen = 8;
				resp.cbRequested = 8;
				resp.requestedData = buf;
			}
		}
		else if (req->dwFlags & FILECONTENTS_RANGE)
		{
			HANDLE f = CreateFileW(cf->path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
			                       nullptr, OPEN_EXISTING, 0, nullptr);
			if (f != INVALID_HANDLE_VALUE)
			{
				LARGE_INTEGER off;
				off.QuadPart = ((LONGLONG)req->nPositionHigh << 32) | req->nPositionLow;
				SetFilePointerEx(f, off, nullptr, FILE_BEGIN);
				buf = (BYTE*)malloc(req->cbRequested);
				DWORD rd = 0;
				if (buf && ReadFile(f, buf, req->cbRequested, &rd, nullptr) && rd)
				{
					resp.common.msgFlags = CB_RESPONSE_OK;
					resp.common.dataLen = rd;
					resp.cbRequested = rd;
					resp.requestedData = buf;
				}
				CloseHandle(f);
			}
		}
	}
	LeaveCriticalSection(&clip->lock);
	cliprdr_check_send(ctx->ServerFileContentsResponse(ctx, &resp), "ServerFileContentsResponse");
	free(buf);
	return CHANNEL_RC_OK;
}

static UINT srv_client_file_contents_response(CliprdrServerContext* ctx,
                                              const CLIPRDR_FILE_CONTENTS_RESPONSE* r)
{
	on_client_file_contents(clipOf(ctx), r);
	return CHANNEL_RC_OK;
}

static UINT srv_client_capabilities(CliprdrServerContext* ctx, const CLIPRDR_CAPABILITIES* caps)
{
	WLog_INFO(TAG, "client cliprdr caps received (%u sets)", caps ? caps->cCapabilitiesSets : 0);
	return CHANNEL_RC_OK;
}

static UINT srv_monitor_ready(CliprdrServerContext* ctx, const CLIPRDR_MONITOR_READY* ready)
{
	ShadowClip* clip = clipOf(ctx);
	WLog_INFO(TAG, "cliprdr ready -- announcing current remote clipboard");
	announce_local(clip);
	return CHANNEL_RC_OK;
}

static UINT srv_temp_directory(CliprdrServerContext* ctx, const CLIPRDR_TEMP_DIRECTORY* td)
{
	WLog_DBG(TAG, "client temp dir: %s", td->szTempDir);
	return CHANNEL_RC_OK;
}

/* ---------- clipboard watcher thread (owns the message-only HWND) ---------- */

static LRESULT CALLBACK clip_wndproc(HWND h, UINT m, WPARAM w, LPARAM l)
{
	if (m == WM_CLIPBOARDUPDATE)
	{
		ShadowClip* clip = (ShadowClip*)GetWindowLongPtrW(h, GWLP_USERDATA);
		if (clip)
		{
			if (InterlockedDecrement(&clip->suppress) >= 0)
				return 0; /* our own write, skip to prevent an echo */
			InterlockedExchange(&clip->suppress, 0);
			if (WaitForSingleObject(clip->stopEvt, 0) != WAIT_OBJECT_0)
				announce_local(clip);
		}
		return 0;
	}
	return DefWindowProcW(h, m, w, l);
}

static DWORD WINAPI clip_watch_thread(LPVOID arg)
{
	ShadowClip* clip = (ShadowClip*)arg;
	WNDCLASSW wc = WINPR_C_ARRAY_INIT;
	wc.lpfnWndProc = clip_wndproc;
	wc.hInstance = GetModuleHandleW(nullptr);
	wc.lpszClassName = L"FreeRDPShadowClip";
	RegisterClassW(&wc);
	clip->hwnd = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
	                             wc.hInstance, nullptr);
	if (clip->hwnd)
	{
		SetWindowLongPtrW(clip->hwnd, GWLP_USERDATA, (LONG_PTR)clip);
		AddClipboardFormatListener(clip->hwnd);
	}
	SetEvent(clip->ready);
	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0) > 0)
	{
		if (WaitForSingleObject(clip->stopEvt, 0) == WAIT_OBJECT_0)
			break;
		DispatchMessageW(&msg);
	}
	if (clip->hwnd)
	{
		RemoveClipboardFormatListener(clip->hwnd);
		DestroyWindow(clip->hwnd);
		clip->hwnd = nullptr;
	}
	return 0;
}

/* ---------- lifecycle ---------- */

int win_shadow_cliprdr_init(rdpShadowClient* client)
{
	WINPR_ASSERT(client);

	ShadowClip* clip = (ShadowClip*)calloc(1, sizeof(ShadowClip));
	if (!clip)
		return -1;
	clip->client = client;
	clip->ctx = cliprdr_server_context_new(client->vcm);
	if (!clip->ctx)
	{
		free(clip);
		return -1;
	}
	clip->ctx->custom = clip;
	clip->ctx->rdpcontext = &client->context;
	clip->ctx->useLongFormatNames = TRUE;
	clip->ctx->streamFileClipEnabled = TRUE;
	clip->ctx->hasHugeFileSupport = TRUE;
	clip->ctx->ClientCapabilities = srv_client_capabilities;
	clip->ctx->MonitorReady = srv_monitor_ready;
	clip->ctx->TempDirectory = srv_temp_directory;
	clip->ctx->ClientFormatList = srv_client_format_list;
	clip->ctx->ClientFormatDataRequest = srv_client_format_data_request;
	clip->ctx->ClientFormatDataResponse = srv_client_format_data_response;
	clip->ctx->ClientFileContentsRequest = srv_client_file_contents_request;
	clip->ctx->ClientFileContentsResponse = srv_client_file_contents_response;

	InitializeCriticalSection(&clip->lock);
	clip->nextFmt = FMT_BASE;
	clip->fgdId = clip->nextFmt++;
	clip->contentsId = clip->nextFmt++;
	clip->dropId = clip->nextFmt++;
	clip->inFile = nullptr;
	clip->ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	clip->stopEvt = CreateEventW(nullptr, TRUE, FALSE, nullptr);

	/* stageDir: %TEMP%\freerdp-shadow-clip\<pid> -- both levels must be
	 * created explicitly, CreateDirectoryW does not fill in intermediates.
	 * The directory is per-process: with several concurrent viewers pulls
	 * race for it (deliberate last-writer-wins, see #13891). */
	DWORD tl = GetTempPathW(MAX_PATH, clip->stageDir);
	if (tl && tl < MAX_PATH - 32)
	{
		WCHAR parent[MAX_PATH];
		_snwprintf_s(parent, MAX_PATH, _TRUNCATE, L"%sfreerdp-shadow-clip", clip->stageDir);
		CreateDirectoryW(parent, nullptr);
		_snwprintf_s(clip->stageDir + wcslen(clip->stageDir), MAX_PATH - wcslen(clip->stageDir),
		             _TRUNCATE, L"freerdp-shadow-clip\\%lu", GetCurrentProcessId());
		CreateDirectoryW(clip->stageDir, nullptr);
	}

	client->cliprdr = clip->ctx;

	const UINT rc = clip->ctx->Start(clip->ctx);
	if (rc != CHANNEL_RC_OK)
	{
		win_shadow_cliprdr_uninit(client);
		return -1;
	}
	clip->thread = CreateThread(nullptr, 0, clip_watch_thread, clip, 0, nullptr);
	WLog_INFO(TAG, "cliprdr channel mounted (text+file sync enabled)");
	return 1;
}

void win_shadow_cliprdr_uninit(rdpShadowClient* client)
{
	WINPR_ASSERT(client);
	ShadowClip* clip = client->cliprdr ? clipOf(client->cliprdr) : nullptr;
	if (clip)
	{
		if (clip->stopEvt)
			SetEvent(clip->stopEvt);
		if (clip->hwnd)
			PostMessageW(clip->hwnd, WM_QUIT, 0, 0);
		if (clip->thread)
		{
			WaitForSingleObject(clip->thread, 3000);
			CloseHandle(clip->thread);
		}
		pull_abort(clip);
		free(clip->files);
		if (clip->ready)
			CloseHandle(clip->ready);
		if (clip->stopEvt)
			CloseHandle(clip->stopEvt);
		DeleteCriticalSection(&clip->lock);
		free(clip);
	}
	if (client->cliprdr)
	{
		client->cliprdr->Stop(client->cliprdr);
		cliprdr_server_context_free(client->cliprdr);
		client->cliprdr = nullptr;
	}
}

#else /* !CHANNEL_CLIPRDR_SERVER */

/* The cliprdr server channel is not built: no clipboard support. The call
 * sites in win_shadow.c are guarded by the same define, so these stubs are
 * not reached in practice. */
int win_shadow_cliprdr_init(rdpShadowClient* client)
{
	WINPR_ASSERT(client);
	return 0;
}

void win_shadow_cliprdr_uninit(rdpShadowClient* client)
{
	WINPR_ASSERT(client);
}

#endif /* CHANNEL_CLIPRDR_SERVER */
