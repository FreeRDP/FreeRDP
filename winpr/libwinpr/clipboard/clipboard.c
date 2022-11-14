/**
 * WinPR: Windows Portable Runtime
 * Clipboard Functions
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/config.h>

#include <winpr/crt.h>
#include <winpr/collections.h>
#include <winpr/wlog.h>

#include <winpr/clipboard.h>

#include "clipboard.h"

#include "synthetic_file.h"

#include "../log.h"
#define TAG WINPR_TAG("clipboard")

const char* mime_utf8_string = "UTF8_STRING";

/**
 * Clipboard (Windows):
 * msdn.microsoft.com/en-us/library/windows/desktop/ms648709/
 *
 * W3C Clipboard API and events:
 * http://www.w3.org/TR/clipboard-apis/
 */

static const char* CF_STANDARD_STRINGS[CF_MAX] = {
	"CF_RAW",          /* 0 */
	"CF_TEXT",         /* 1 */
	"CF_BITMAP",       /* 2 */
	"CF_METAFILEPICT", /* 3 */
	"CF_SYLK",         /* 4 */
	"CF_DIF",          /* 5 */
	"CF_TIFF",         /* 6 */
	"CF_OEMTEXT",      /* 7 */
	"CF_DIB",          /* 8 */
	"CF_PALETTE",      /* 9 */
	"CF_PENDATA",      /* 10 */
	"CF_RIFF",         /* 11 */
	"CF_WAVE",         /* 12 */
	"CF_UNICODETEXT",  /* 13 */
	"CF_ENHMETAFILE",  /* 14 */
	"CF_HDROP",        /* 15 */
	"CF_LOCALE",       /* 16 */
	"CF_DIBV5"         /* 17 */
};

static wClipboardFormat* ClipboardFindFormat(wClipboard* clipboard, UINT32 formatId,
                                             const char* name)
{
	UINT32 index;
	wClipboardFormat* format = NULL;

	if (!clipboard)
		return NULL;

	if (formatId)
	{
		for (index = 0; index < clipboard->numFormats; index++)
		{
			if (formatId == clipboard->formats[index].formatId)
			{
				format = &clipboard->formats[index];
				break;
			}
		}
	}
	else if (name)
	{
		for (index = 0; index < clipboard->numFormats; index++)
		{
			if (!clipboard->formats[index].formatName)
				continue;

			if (strcmp(name, clipboard->formats[index].formatName) == 0)
			{
				format = &clipboard->formats[index];
				break;
			}
		}
	}
	else
	{
		/* special "CF_RAW" case */
		if (clipboard->numFormats > 0)
		{
			format = &clipboard->formats[0];

			if (format->formatId)
				return NULL;

			if (!format->formatName || (strcmp(format->formatName, CF_STANDARD_STRINGS[0]) == 0))
				return format;
		}
	}

	return format;
}

static wClipboardSynthesizer* ClipboardFindSynthesizer(wClipboardFormat* format, UINT32 formatId)
{
	UINT32 index;
	wClipboardSynthesizer* synthesizer;

	if (!format)
		return NULL;

	if (format->numSynthesizers < 1)
		return NULL;

	for (index = 0; index < format->numSynthesizers; index++)
	{
		synthesizer = &(format->synthesizers[index]);

		if (formatId == synthesizer->syntheticId)
			return synthesizer;
	}

	return NULL;
}

void ClipboardLock(wClipboard* clipboard)
{
	if (!clipboard)
		return;

	EnterCriticalSection(&(clipboard->lock));
}

void ClipboardUnlock(wClipboard* clipboard)
{
	if (!clipboard)
		return;

	LeaveCriticalSection(&(clipboard->lock));
}

BOOL ClipboardEmpty(wClipboard* clipboard)
{
	if (!clipboard)
		return FALSE;

	if (clipboard->data)
	{
		free((void*)clipboard->data);
		clipboard->data = NULL;
	}

	clipboard->size = 0;
	clipboard->formatId = 0;
	clipboard->sequenceNumber++;
	return TRUE;
}

UINT32 ClipboardCountRegisteredFormats(wClipboard* clipboard)
{
	if (!clipboard)
		return 0;

	return clipboard->numFormats;
}

UINT32 ClipboardGetRegisteredFormatIds(wClipboard* clipboard, UINT32** ppFormatIds)
{
	UINT32 index;
	UINT32* pFormatIds;
	wClipboardFormat* format;

	if (!clipboard)
		return 0;

	if (!ppFormatIds)
		return 0;

	pFormatIds = *ppFormatIds;

	if (!pFormatIds)
	{
		pFormatIds = calloc(clipboard->numFormats, sizeof(UINT32));

		if (!pFormatIds)
			return 0;

		*ppFormatIds = pFormatIds;
	}

	for (index = 0; index < clipboard->numFormats; index++)
	{
		format = &(clipboard->formats[index]);
		pFormatIds[index] = format->formatId;
	}

	return clipboard->numFormats;
}

UINT32 ClipboardRegisterFormat(wClipboard* clipboard, const char* name)
{
	wClipboardFormat* format;

	if (!clipboard)
		return 0;

	format = ClipboardFindFormat(clipboard, 0, name);

	if (format)
		return format->formatId;

	if ((clipboard->numFormats + 1) >= clipboard->maxFormats)
	{
		UINT32 numFormats = clipboard->maxFormats * 2;
		wClipboardFormat* tmpFormat;
		tmpFormat =
		    (wClipboardFormat*)realloc(clipboard->formats, numFormats * sizeof(wClipboardFormat));

		if (!tmpFormat)
			return 0;

		clipboard->formats = tmpFormat;
		clipboard->maxFormats = numFormats;
	}

	format = &(clipboard->formats[clipboard->numFormats]);
	ZeroMemory(format, sizeof(wClipboardFormat));

	if (name)
	{
		format->formatName = _strdup(name);

		if (!format->formatName)
			return 0;
	}

	format->formatId = clipboard->nextFormatId++;
	clipboard->numFormats++;
	return format->formatId;
}

BOOL ClipboardRegisterSynthesizer(wClipboard* clipboard, UINT32 formatId, UINT32 syntheticId,
                                  CLIPBOARD_SYNTHESIZE_FN pfnSynthesize)
{
	UINT32 index;
	wClipboardFormat* format;
	wClipboardSynthesizer* synthesizer;

	if (!clipboard)
		return FALSE;

	format = ClipboardFindFormat(clipboard, formatId, NULL);

	if (!format)
		return FALSE;

	if (format->formatId == syntheticId)
		return FALSE;

	synthesizer = ClipboardFindSynthesizer(format, formatId);

	if (!synthesizer)
	{
		wClipboardSynthesizer* tmpSynthesizer;
		UINT32 numSynthesizers = format->numSynthesizers + 1;
		tmpSynthesizer = (wClipboardSynthesizer*)realloc(
		    format->synthesizers, numSynthesizers * sizeof(wClipboardSynthesizer));

		if (!tmpSynthesizer)
			return FALSE;

		format->synthesizers = tmpSynthesizer;
		format->numSynthesizers = numSynthesizers;
		index = numSynthesizers - 1;
		synthesizer = &(format->synthesizers[index]);
	}

	ZeroMemory(synthesizer, sizeof(wClipboardSynthesizer));
	synthesizer->syntheticId = syntheticId;
	synthesizer->pfnSynthesize = pfnSynthesize;
	return TRUE;
}

UINT32 ClipboardCountFormats(wClipboard* clipboard)
{
	UINT32 count;
	wClipboardFormat* format;

	if (!clipboard)
		return 0;

	format = ClipboardFindFormat(clipboard, clipboard->formatId, NULL);

	if (!format)
		return 0;

	count = 1 + format->numSynthesizers;
	return count;
}

UINT32 ClipboardGetFormatIds(wClipboard* clipboard, UINT32** ppFormatIds)
{
	UINT32 index;
	UINT32 count;
	UINT32* pFormatIds;
	wClipboardFormat* format;
	wClipboardSynthesizer* synthesizer;

	if (!clipboard)
		return 0;

	format = ClipboardFindFormat(clipboard, clipboard->formatId, NULL);

	if (!format)
		return 0;

	count = 1 + format->numSynthesizers;

	if (!ppFormatIds)
		return 0;

	pFormatIds = *ppFormatIds;

	if (!pFormatIds)
	{
		pFormatIds = calloc(count, sizeof(UINT32));

		if (!pFormatIds)
			return 0;

		*ppFormatIds = pFormatIds;
	}

	pFormatIds[0] = format->formatId;

	for (index = 1; index < count; index++)
	{
		synthesizer = &(format->synthesizers[index - 1]);
		pFormatIds[index] = synthesizer->syntheticId;
	}

	return count;
}

static BOOL ClipboardInitFormats(wClipboard* clipboard)
{
	UINT32 formatId = 0;
	wClipboardFormat* format;

	if (!clipboard)
		return FALSE;

	for (formatId = 0; formatId < CF_MAX; formatId++, clipboard->numFormats++)
	{
		format = &(clipboard->formats[clipboard->numFormats]);
		ZeroMemory(format, sizeof(wClipboardFormat));
		format->formatId = formatId;
		format->formatName = _strdup(CF_STANDARD_STRINGS[formatId]);

		if (!format->formatName)
			goto error;
	}

	if (!ClipboardInitSynthesizers(clipboard))
		goto error;

	return TRUE;
error:

	for (formatId = 0; formatId < clipboard->numFormats; formatId++)
	{
		free(clipboard->formats[formatId].formatName);
		free(clipboard->formats[formatId].synthesizers);
	}

	return FALSE;
}

UINT32 ClipboardGetFormatId(wClipboard* clipboard, const char* name)
{
	wClipboardFormat* format;

	if (!clipboard)
		return 0;

	format = ClipboardFindFormat(clipboard, 0, name);

	if (!format)
		return 0;

	return format->formatId;
}

const char* ClipboardGetFormatName(wClipboard* clipboard, UINT32 formatId)
{
	wClipboardFormat* format;

	if (!clipboard)
		return NULL;

	format = ClipboardFindFormat(clipboard, formatId, NULL);

	if (!format)
		return NULL;

	return format->formatName;
}

void* ClipboardGetData(wClipboard* clipboard, UINT32 formatId, UINT32* pSize)
{
	UINT32 SrcSize = 0;
	UINT32 DstSize = 0;
	void* pSrcData = NULL;
	void* pDstData = NULL;
	wClipboardFormat* format;
	wClipboardSynthesizer* synthesizer;

	if (!clipboard)
		return NULL;

	if (!pSize)
		return NULL;

	*pSize = 0;
	format = ClipboardFindFormat(clipboard, clipboard->formatId, NULL);

	if (!format)
		return NULL;

	SrcSize = clipboard->size;
	pSrcData = (void*)clipboard->data;

	if (formatId == format->formatId)
	{
		DstSize = SrcSize;
		pDstData = malloc(DstSize);

		if (!pDstData)
			return NULL;

		CopyMemory(pDstData, pSrcData, SrcSize);
		*pSize = DstSize;
	}
	else
	{
		synthesizer = ClipboardFindSynthesizer(format, formatId);

		if (!synthesizer || !synthesizer->pfnSynthesize)
			return NULL;

		DstSize = SrcSize;
		pDstData = synthesizer->pfnSynthesize(clipboard, format->formatId, pSrcData, &DstSize);
		if (pDstData)
			*pSize = DstSize;
	}

	return pDstData;
}

BOOL ClipboardSetData(wClipboard* clipboard, UINT32 formatId, const void* data, UINT32 size)
{
	wClipboardFormat* format;

	if (!clipboard)
		return FALSE;

	format = ClipboardFindFormat(clipboard, formatId, NULL);

	if (!format)
		return FALSE;

	free((void*)clipboard->data);
	clipboard->data = malloc(size);

	if (!clipboard->data)
		return FALSE;

	memcpy(clipboard->data, data, size);
	clipboard->size = size;
	clipboard->formatId = formatId;
	clipboard->sequenceNumber++;
	return TRUE;
}

UINT64 ClipboardGetOwner(wClipboard* clipboard)
{
	if (!clipboard)
		return 0;

	return clipboard->ownerId;
}

void ClipboardSetOwner(wClipboard* clipboard, UINT64 ownerId)
{
	if (!clipboard)
		return;

	clipboard->ownerId = ownerId;
}

wClipboardDelegate* ClipboardGetDelegate(wClipboard* clipboard)
{
	if (!clipboard)
		return NULL;

	return &clipboard->delegate;
}

static void ClipboardInitLocalFileSubsystem(wClipboard* clipboard)
{
	/*
	 * There can be only one local file subsystem active.
	 * Return as soon as initialization succeeds.
	 */
	if (ClipboardInitSyntheticFileSubsystem(clipboard))
	{
		WLog_DBG(TAG, "initialized synthetic local file subsystem");
		return;
	}
	else
	{
		WLog_WARN(TAG, "failed to initialize synthetic local file subsystem");
	}

	WLog_INFO(TAG, "failed to initialize local file subsystem, file transfer not available");
}

wClipboard* ClipboardCreate(void)
{
	wClipboard* clipboard;
	clipboard = (wClipboard*)calloc(1, sizeof(wClipboard));

	if (!clipboard)
		return NULL;

	clipboard->nextFormatId = 0xC000;
	clipboard->sequenceNumber = 0;

	if (!InitializeCriticalSectionAndSpinCount(&(clipboard->lock), 4000))
		goto error_free_clipboard;

	clipboard->numFormats = 0;
	clipboard->maxFormats = 64;
	clipboard->formats = (wClipboardFormat*)calloc(clipboard->maxFormats, sizeof(wClipboardFormat));

	if (!clipboard->formats)
		goto error_free_lock;

	if (!ClipboardInitFormats(clipboard))
		goto error_free_formats;

	clipboard->delegate.clipboard = clipboard;
	ClipboardInitLocalFileSubsystem(clipboard);
	return clipboard;
error_free_formats:
	free(clipboard->formats);
error_free_lock:
	DeleteCriticalSection(&(clipboard->lock));
error_free_clipboard:
	free(clipboard);
	return NULL;
}

void ClipboardDestroy(wClipboard* clipboard)
{
	UINT32 index;
	wClipboardFormat* format;

	if (!clipboard)
		return;

	ArrayList_Free(clipboard->localFiles);
	clipboard->localFiles = NULL;

	for (index = 0; index < clipboard->numFormats; index++)
	{
		format = &(clipboard->formats[index]);
		free((void*)format->formatName);

		if (format->synthesizers)
		{
			free(format->synthesizers);
			format->synthesizers = NULL;
			format->numSynthesizers = 0;
		}
	}

	free((void*)clipboard->data);
	clipboard->data = NULL;
	clipboard->size = 0;
	clipboard->numFormats = 0;
	free(clipboard->formats);
	DeleteCriticalSection(&(clipboard->lock));
	free(clipboard);
}

static BOOL is_dos_drive(const char* path, size_t len)
{
	if (len < 2)
		return FALSE;

	WINPR_ASSERT(path);
	if (path[1] == ':' || path[1] == '|')
	{
		if (((path[0] >= 'A') && (path[0] <= 'Z')) || ((path[0] >= 'a') && (path[0] <= 'z')))
			return TRUE;
	}
	return FALSE;
}

char* parse_uri_to_local_file(const char* uri, size_t uri_len)
{
	// URI is specified by RFC 8089: https://datatracker.ietf.org/doc/html/rfc8089
	const char prefix[] = "file:";
	const char prefixTraditional[] = "file://";
	const char* localName = NULL;
	size_t localLen = 0;
	char* buffer = NULL;
	const size_t prefixLen = strnlen(prefix, sizeof(prefix));
	const size_t prefixTraditionalLen = strnlen(prefixTraditional, sizeof(prefixTraditional));

	WINPR_ASSERT(uri || (uri_len == 0));

	WLog_VRB(TAG, "processing URI: %.*s", uri_len, uri);

	if ((uri_len <= prefixLen) || strncmp(uri, prefix, prefixLen))
	{
		WLog_ERR(TAG, "non-'file:' URI schemes are not supported");
		return NULL;
	}

	do
	{
		/* https://datatracker.ietf.org/doc/html/rfc8089#appendix-F
		 * - The minimal representation of a local file in a DOS- or Windows-
		 *   based environment with no authority field and an absolute path
		 *   that begins with a drive letter.
		 *
		 *   "file:c:/path/to/file"
		 *
		 * - Regular DOS or Windows file URIs with vertical line characters in
		 *   the drive letter construct.
		 *
		 *   "file:c|/path/to/file"
		 *
		 */
		if (uri[prefixLen] != '/')
		{

			if (is_dos_drive(&uri[prefixLen], uri_len - prefixLen))
			{
				// Dos and Windows file URI
				localName = &uri[prefixLen];
				localLen = uri_len - prefixLen;
				break;
			}
			else
			{
				WLog_ERR(TAG, "URI format are not supported: %s", uri);
				return NULL;
			}
		}

		/*
		 * - The minimal representation of a local file with no authority field
		 *   and an absolute path that begins with a slash "/".  For example:
		 *
		 *   "file:/path/to/file"
		 *
		 */
		else if ((uri_len > prefixLen + 1) && (uri[prefixLen + 1] != '/'))
		{
			if (is_dos_drive(&uri[prefixLen + 1], uri_len - prefixLen - 1))
			{
				// Dos and Windows file URI
				localName = (const char*)(uri + prefixLen + 1);
				localLen = uri_len - prefixLen - 1;
			}
			else
			{
				localName = &uri[prefixLen];
				localLen = uri_len - prefixLen;
			}
			break;
		}

		/*
		 * - A traditional file URI for a local file with an empty authority.
		 *
		 *   "file:///path/to/file"
		 */
		if ((uri_len < prefixTraditionalLen) ||
		    strncmp(uri, prefixTraditional, prefixTraditionalLen))
		{
			WLog_ERR(TAG, "non-'file:' URI schemes are not supported");
			return NULL;
		}

		localName = &uri[prefixTraditionalLen];
		localLen = uri_len - prefixTraditionalLen;

		if (localLen < 1)
		{
			WLog_ERR(TAG, "empty 'file:' URI schemes are not supported");
			return NULL;
		}

		/*
		 * "file:///c:/path/to/file"
		 * "file:///c|/path/to/file"
		 */
		if (localName[0] != '/')
		{
			WLog_ERR(TAG, "URI format are not supported: %s", uri);
			return NULL;
		}

		if (is_dos_drive(&localName[1], localLen - 1))
		{
			localName++;
			localLen--;
		}

	} while (0);

	buffer = calloc(localLen + 1, sizeof(char));
	if (buffer)
	{
		memcpy(buffer, localName, localLen);
		if (buffer[1] == '|' &&
		    ((buffer[0] >= 'A' && buffer[0] <= 'Z') || (buffer[0] >= 'a' && buffer[0] <= 'z')))
			buffer[1] = ':';
		return buffer;
	}

	return NULL;
}
