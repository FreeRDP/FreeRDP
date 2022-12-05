/**
 * WinPR: Windows Portable Runtime
 * Clipboard Functions: POSIX file handling
 *
 * Copyright 2017 Alexei Lozovsky <a.lozovsky@gmail.com>
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

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif

#define _FILE_OFFSET_BITS 64

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <errno.h>

#include <winpr/wtypes.h>

#include <winpr/crt.h>
#include <winpr/clipboard.h>
#include <winpr/collections.h>
#include <winpr/file.h>
#include <winpr/shell.h>
#include <winpr/string.h>
#include <winpr/wlog.h>
#include <winpr/print.h>

#include "clipboard.h"
#include "synthetic_file.h"

#include "../log.h"
#define TAG WINPR_TAG("clipboard.synthetic.file")

const char* mime_uri_list = "text/uri-list";
const char* mime_FileGroupDescriptorW = "FileGroupDescriptorW";
const char* mime_nautilus_clipboard = "x-special/nautilus-clipboard";
const char* mime_gnome_copied_files = "x-special/gnome-copied-files";
const char* mime_mate_copied_files = "x-special/mate-copied-files";

struct synthetic_file
{
	WCHAR* local_name;
	WCHAR* remote_name;

	HANDLE fd;
	INT64 offset;

	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD nFileSizeHigh;
	DWORD nFileSizeLow;
};

void free_synthetic_file(struct synthetic_file* file);

static struct synthetic_file* make_synthetic_file(const WCHAR* local_name, const WCHAR* remote_name)
{
	struct synthetic_file* file = NULL;
	WIN32_FIND_DATAW fd = { 0 };
	HANDLE hFind;

	WINPR_ASSERT(local_name);
	WINPR_ASSERT(remote_name);

	hFind = FindFirstFileW(local_name, &fd);
	if (INVALID_HANDLE_VALUE == hFind)
	{
		WLog_ERR(TAG, "FindFirstFile failed (%" PRIu32 ")", GetLastError());
		return NULL;
	}
	FindClose(hFind);

	file = calloc(1, sizeof(*file));
	if (!file)
		return NULL;

	file->fd = INVALID_HANDLE_VALUE;
	file->offset = 0;
	file->local_name = _wcsdup(local_name);
	if (!file->local_name)
		goto fail;

	file->remote_name = _wcsdup(remote_name);
	if (!file->remote_name)
		goto fail;

	const size_t len = _wcslen(file->remote_name);
	for (size_t x = 0; x < len; x++)
	{
		if (file->remote_name[x] == '/')
			file->remote_name[x] = '\\';
	}

	file->dwFileAttributes = fd.dwFileAttributes;
	file->ftCreationTime = fd.ftCreationTime;
	file->ftLastWriteTime = fd.ftLastWriteTime;
	file->ftLastAccessTime = fd.ftLastAccessTime;
	file->nFileSizeHigh = fd.nFileSizeHigh;
	file->nFileSizeLow = fd.nFileSizeLow;

	return file;
fail:
	free_synthetic_file(file);
	return NULL;
}

static UINT synthetic_file_read_close(struct synthetic_file* file, BOOL force);

void free_synthetic_file(struct synthetic_file* file)
{
	if (!file)
		return;

	synthetic_file_read_close(file, TRUE);

	free(file->local_name);
	free(file->remote_name);
	free(file);
}

static unsigned char hex_to_dec(char c, BOOL* valid)
{
	WINPR_ASSERT(valid);

	if (('0' <= c) && (c <= '9'))
		return (c - '0');

	if (('a' <= c) && (c <= 'f'))
		return (c - 'a') + 10;

	if (('A' <= c) && (c <= 'F'))
		return (c - 'A') + 10;

	*valid = FALSE;
	return 0;
}

static BOOL decode_percent_encoded_byte(const char* str, const char* end, char* value)
{
	BOOL valid = TRUE;

	WINPR_ASSERT(str);
	WINPR_ASSERT(end);
	WINPR_ASSERT(value);

	if ((end < str) || ((size_t)(end - str) < strlen("%20")))
		return FALSE;

	*value = 0;
	*value |= hex_to_dec(str[1], &valid);
	*value <<= 4;
	*value |= hex_to_dec(str[2], &valid);
	return valid;
}

static char* decode_percent_encoded_string(const char* str, size_t len)
{
	char* buffer = NULL;
	char* next = NULL;
	const char* cur = str;
	const char* lim = str + len;

	WINPR_ASSERT(str);

	/* Percent decoding shrinks data length, so len bytes will be enough. */
	buffer = calloc(len + 1, sizeof(char));

	if (!buffer)
		return NULL;

	next = buffer;

	while (cur < lim)
	{
		if (*cur != '%')
		{
			*next++ = *cur++;
		}
		else
		{
			if (!decode_percent_encoded_byte(cur, lim, next))
			{
				WLog_ERR(TAG, "invalid percent encoding");
				goto error;
			}

			cur += strlen("%20");
			next += 1;
		}
	}

	return buffer;
error:
	free(buffer);
	return NULL;
}

/*
 * Note that the function converts a single file name component,
 * it does not take care of component separators.
 */
static WCHAR* convert_local_name_component_to_remote(wClipboard* clipboard, const WCHAR* local_name)
{
	wClipboardDelegate* delegate = ClipboardGetDelegate(clipboard);
	WCHAR* remote_name = NULL;

	WINPR_ASSERT(delegate);

	remote_name = _wcsdup(local_name);

	/*
	 * Some file names are not valid on Windows. Check for these now
	 * so that we won't get ourselves into a trouble later as such names
	 * are known to crash some Windows shells when pasted via clipboard.
	 *
	 * The IsFileNameComponentValid callback can be overridden by the API
	 * user, if it is known, that the connected peer is not on the
	 * Windows platform.
	 */
	if (!delegate->IsFileNameComponentValid(remote_name))
	{
		WLog_ERR(TAG, "invalid file name component: %s", local_name);
		goto error;
	}

	return remote_name;
error:
	free(remote_name);
	return NULL;
}

static WCHAR* concat_file_name(const WCHAR* dir, const WCHAR* file)
{
	size_t len_dir = 0;
	size_t len_file = 0;
	const WCHAR slash = '/';
	WCHAR* buffer = NULL;

	WINPR_ASSERT(dir);
	WINPR_ASSERT(file);

	len_dir = _wcslen(dir);
	len_file = _wcslen(file);
	buffer = calloc(len_dir + 1 + len_file + 2, sizeof(WCHAR));

	if (!buffer)
		return NULL;

	memcpy(buffer, dir, len_dir * sizeof(WCHAR));
	buffer[len_dir] = slash;
	memcpy(buffer + len_dir + 1, file, len_file * sizeof(WCHAR));
	return buffer;
}

static BOOL add_file_to_list(wClipboard* clipboard, const WCHAR* local_name,
                             const WCHAR* remote_name, wArrayList* files);

static BOOL add_directory_entry_to_list(wClipboard* clipboard, const WCHAR* local_dir_name,
                                        const WCHAR* remote_dir_name,
                                        const LPWIN32_FIND_DATAW pFileData, wArrayList* files)
{
	BOOL result = FALSE;
	WCHAR* local_name = NULL;
	WCHAR* remote_name = NULL;
	WCHAR* remote_base_name = NULL;

	const WCHAR dot[2] = { '.', '\0' };
	const WCHAR dotdot[3] = { '.', '.', '\0' };

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(local_dir_name);
	WINPR_ASSERT(remote_dir_name);
	WINPR_ASSERT(pFileData);
	WINPR_ASSERT(files);

	/* Skip special directory entries. */

	if ((_wcscmp(pFileData->cFileName, dot) == 0) || (_wcscmp(pFileData->cFileName, dotdot) == 0))
		return TRUE;

	remote_base_name = convert_local_name_component_to_remote(clipboard, pFileData->cFileName);

	if (!remote_base_name)
		return FALSE;

	local_name = concat_file_name(local_dir_name, pFileData->cFileName);
	remote_name = concat_file_name(remote_dir_name, remote_base_name);

	if (local_name && remote_name)
		result = add_file_to_list(clipboard, local_name, remote_name, files);

	free(remote_base_name);
	free(remote_name);
	free(local_name);
	return result;
}

static BOOL do_add_directory_contents_to_list(wClipboard* clipboard, const WCHAR* local_name,
                                              const WCHAR* remote_name, HANDLE hFind,
                                              wArrayList* files)
{
	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(local_name);
	WINPR_ASSERT(remote_name);
	WINPR_ASSERT(files);

	if (INVALID_HANDLE_VALUE == hFind)
	{
		return FALSE;
	}

	while (TRUE)
	{
		WIN32_FIND_DATAW FileData = { 0 };
		BOOL bRet = FindNextFileW(hFind, &FileData);
		if (!bRet)
		{
			if (ERROR_NO_MORE_FILES == GetLastError())
				return TRUE;
			WLog_WARN(TAG, "FindNextFile failed (%" PRIu32 ")", GetLastError());
			return FALSE;
		}

		if (!add_directory_entry_to_list(clipboard, local_name, remote_name, &FileData, files))
			return FALSE;
	}

	return TRUE;
}

static BOOL add_directory_contents_to_list(wClipboard* clipboard, const WCHAR* local_name,
                                           const WCHAR* remote_name, wArrayList* files)
{
	BOOL result = FALSE;
	HANDLE hFind = NULL;
	WIN32_FIND_DATAW FindData = { 0 };
	const WCHAR wildcard[] = { '/', '*', '\0' };

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(local_name);
	WINPR_ASSERT(remote_name);
	WINPR_ASSERT(files);

	size_t len = _wcslen(local_name);
	WCHAR* namebuf = calloc(len + ARRAYSIZE(wildcard), sizeof(WCHAR));
	if (!namebuf)
		return FALSE;

	memcpy(namebuf, local_name, len * sizeof(WCHAR));
	memcpy(&namebuf[len], wildcard, sizeof(wildcard));

	hFind = FindFirstFileW(namebuf, &FindData);
	free(namebuf);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		WLog_ERR(TAG, "FindFirstFile failed (%" PRIu32 ")", GetLastError());
		return FALSE;
	}

	result = do_add_directory_contents_to_list(clipboard, local_name, remote_name, hFind, files);

	FindClose(hFind);
	return result;
}

static BOOL add_file_to_list(wClipboard* clipboard, const WCHAR* local_name,
                             const WCHAR* remote_name, wArrayList* files)
{
	struct synthetic_file* file = NULL;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(local_name);
	WINPR_ASSERT(remote_name);
	WINPR_ASSERT(files);

	WLog_VRB(TAG, "adding file: %s", local_name);
	file = make_synthetic_file(local_name, remote_name);

	if (!file)
		return FALSE;

	if (!ArrayList_Append(files, file))
	{
		free_synthetic_file(file);
		return FALSE;
	}

	if (file->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		/*
		 * This is effectively a recursive call, but we do not track
		 * recursion depth, thus filesystem loops can cause a crash.
		 */
		if (!add_directory_contents_to_list(clipboard, local_name, remote_name, files))
			return FALSE;
	}

	return TRUE;
}

static const WCHAR* get_basename(const WCHAR* name)
{
	const WCHAR* c = name;
	const WCHAR* last_name = name;
	const WCHAR slash = '/';

	WINPR_ASSERT(name);

	while (*c++)
	{
		if (*c == slash)
			last_name = c + 1;
	}

	return last_name;
}

static BOOL process_file_name(wClipboard* clipboard, const WCHAR* local_name, wArrayList* files)
{
	BOOL result = FALSE;
	const WCHAR* base_name = NULL;
	WCHAR* remote_name = NULL;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(local_name);
	WINPR_ASSERT(files);

	/*
	 * Start with the base name of the file. text/uri-list contains the
	 * exact files selected by the user, and we want the remote files
	 * to have names relative to that selection.
	 */
	base_name = get_basename(local_name);
	remote_name = convert_local_name_component_to_remote(clipboard, base_name);

	if (!remote_name)
		return FALSE;

	result = add_file_to_list(clipboard, local_name, remote_name, files);
	free(remote_name);
	return result;
}

static BOOL process_uri(wClipboard* clipboard, const char* uri, size_t uri_len)
{
	// URI is specified by RFC 8089: https://datatracker.ietf.org/doc/html/rfc8089
	BOOL result = FALSE;
	char* name = NULL;
	char* localName = NULL;

	WINPR_ASSERT(clipboard);

	localName = parse_uri_to_local_file(uri, uri_len);
	if (localName)
	{
		name = decode_percent_encoded_string(localName, strlen(localName));
		free(localName);
	}
	if (name)
	{
		WCHAR* wname = NULL;
		/*
		 * Note that local file names are not actually guaranteed to be
		 * encoded in UTF-8. Filesystems and users can use whatever they
		 * want. The OS does not care, aside from special treatment of
		 * '\0' and '/' bytes. But we need to make some decision here.
		 * Assuming UTF-8 is currently the most sane thing.
		 */
		wname = ConvertUtf8ToWCharAlloc(name, NULL);
		if (wname)
			result = process_file_name(clipboard, wname, clipboard->localFiles);

		free(name);
		free(wname);
	}

	return result;
}

static BOOL process_uri_list(wClipboard* clipboard, const char* data, size_t length)
{
	const char* cur = data;
	const char* lim = data + length;
	const char* start = NULL;
	const char* stop = NULL;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(data);

	WLog_VRB(TAG, "processing URI list:\n%.*s", length, data);
	ArrayList_Clear(clipboard->localFiles);

	/*
	 * The "text/uri-list" Internet Media Type is specified by RFC 2483.
	 *
	 * While the RFCs 2046 and 2483 require the lines of text/... formats
	 * to be terminated by CRLF sequence, be prepared for those who don't
	 * read the spec, use plain LFs, and don't leave the trailing CRLF.
	 */

	while (cur < lim)
	{
		BOOL comment = (*cur == '#');
		start = cur;

		for (stop = cur; stop < lim; stop++)
		{
			if (*stop == '\r')
			{
				if ((stop + 1 < lim) && (*(stop + 1) == '\n'))
					cur = stop + 2;
				else
					cur = stop + 1;

				break;
			}

			if (*stop == '\n')
			{
				cur = stop + 1;
				break;
			}
		}

		if (stop == lim)
			cur = lim;

		if (comment)
			continue;

		if (!process_uri(clipboard, start, stop - start))
			return FALSE;
	}

	return TRUE;
}

static BOOL convert_local_file_to_filedescriptor(const struct synthetic_file* file,
                                                 FILEDESCRIPTORW* descriptor)
{
	size_t remote_len = 0;

	WINPR_ASSERT(file);
	WINPR_ASSERT(descriptor);

	descriptor->dwFlags = FD_ATTRIBUTES | FD_FILESIZE | FD_WRITESTIME | FD_PROGRESSUI;

	if (file->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		descriptor->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		descriptor->nFileSizeLow = 0;
		descriptor->nFileSizeHigh = 0;
	}
	else
	{
		descriptor->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
		descriptor->nFileSizeLow = file->nFileSizeLow;
		descriptor->nFileSizeHigh = file->nFileSizeHigh;
	}

	descriptor->ftLastWriteTime = file->ftLastWriteTime;

	remote_len = _wcsnlen(file->remote_name, ARRAYSIZE(descriptor->cFileName));

	if (remote_len >= ARRAYSIZE(descriptor->cFileName))
	{
		WLog_ERR(TAG, "file name too long (%" PRIuz " characters)", remote_len);
		return FALSE;
	}

	memcpy(descriptor->cFileName, file->remote_name, remote_len * sizeof(WCHAR));
	return TRUE;
}

static FILEDESCRIPTORW* convert_local_file_list_to_filedescriptors(wArrayList* files)
{
	size_t i;
	size_t count = 0;
	FILEDESCRIPTORW* descriptors = NULL;

	count = ArrayList_Count(files);

	descriptors = calloc(count, sizeof(FILEDESCRIPTORW));

	if (!descriptors)
		goto error;

	for (i = 0; i < count; i++)
	{
		const struct synthetic_file* file = ArrayList_GetItem(files, i);

		if (!convert_local_file_to_filedescriptor(file, &descriptors[i]))
			goto error;
	}

	return descriptors;
error:
	free(descriptors);
	return NULL;
}

static void* convert_any_uri_list_to_filedescriptors(wClipboard* clipboard, UINT32 formatId,
                                                     UINT32* pSize)
{
	FILEDESCRIPTORW* descriptors;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(pSize);

	descriptors = convert_local_file_list_to_filedescriptors(clipboard->localFiles);
	*pSize = 0;
	if (!descriptors)
		return NULL;

	*pSize = (UINT32)ArrayList_Count(clipboard->localFiles) * sizeof(FILEDESCRIPTORW);
	clipboard->fileListSequenceNumber = clipboard->sequenceNumber;
	return descriptors;
}

static void* convert_uri_list_to_filedescriptors(wClipboard* clipboard, UINT32 formatId,
                                                 const void* data, UINT32* pSize)
{
	const UINT32 expected = ClipboardGetFormatId(clipboard, mime_uri_list);
	if (formatId != expected)
		return NULL;
	if (!process_uri_list(clipboard, (const char*)data, *pSize))
		return NULL;
	return convert_any_uri_list_to_filedescriptors(clipboard, formatId, pSize);
}

static BOOL process_files(wClipboard* clipboard, const char* data, UINT32 pSize, const char* prefix)
{
	WINPR_ASSERT(prefix);

	const size_t prefix_len = strlen(prefix);

	WINPR_ASSERT(clipboard);

	ArrayList_Clear(clipboard->localFiles);

	if (!data || (pSize < prefix_len))
		return FALSE;
	if (strncmp(data, prefix, prefix_len) != 0)
		return FALSE;
	data += prefix_len;
	pSize -= prefix_len;

	BOOL rc = FALSE;
	char* copy = strndup(data, pSize);
	if (!copy)
		goto fail;

	char* endptr = NULL;
	char* tok = strtok_s(copy, "\n", &endptr);
	while (tok)
	{
		size_t tok_len = strnlen(tok, pSize);
		if (!process_uri(clipboard, tok, tok_len))
			goto fail;
		pSize -= tok_len;
		tok = strtok_s(NULL, "\n", &endptr);
	}
	rc = TRUE;

fail:
	free(copy);
	return rc;
}

static BOOL process_gnome_copied_files(wClipboard* clipboard, const char* data, UINT32 pSize)
{
	return process_files(clipboard, data, pSize, "copy\n");
}

static BOOL process_mate_copied_files(wClipboard* clipboard, const char* data, UINT32 pSize)
{
	return process_files(clipboard, data, pSize, "copy\n");
}

static BOOL process_nautilus_clipboard(wClipboard* clipboard, const char* data, UINT32 pSize)
{
	return process_files(clipboard, data, pSize, "x-special/nautilus-clipboard\ncopy\n");
}

static void* convert_gnome_copied_files_to_filedescriptors(wClipboard* clipboard, UINT32 formatId,
                                                           const void* data, UINT32* pSize)
{
	const UINT32 expected = ClipboardGetFormatId(clipboard, mime_gnome_copied_files);
	if (formatId != expected)
		return NULL;
	if (!process_gnome_copied_files(clipboard, (const char*)data, *pSize))
		return NULL;
	return convert_any_uri_list_to_filedescriptors(clipboard, formatId, pSize);
}

static void* convert_mate_copied_files_to_filedescriptors(wClipboard* clipboard, UINT32 formatId,
                                                          const void* data, UINT32* pSize)
{
	const UINT32 expected = ClipboardGetFormatId(clipboard, mime_mate_copied_files);
	if (formatId != expected)
		return NULL;

	if (!process_mate_copied_files(clipboard, (const char*)data, *pSize))
		return NULL;

	return convert_any_uri_list_to_filedescriptors(clipboard, formatId, pSize);
}

static void* convert_nautilus_clipboard_to_filedescriptors(wClipboard* clipboard, UINT32 formatId,
                                                           const void* data, UINT32* pSize)
{
	const UINT32 expected = ClipboardGetFormatId(clipboard, mime_nautilus_clipboard);
	if (formatId != expected)
		return NULL;

	if (!process_nautilus_clipboard(clipboard, (const char*)data, *pSize))
		return NULL;

	return convert_any_uri_list_to_filedescriptors(clipboard, formatId, pSize);
}

static size_t count_special_chars(const WCHAR* str)
{
	size_t count = 0;
	const WCHAR* start = str;

	WINPR_ASSERT(str);
	while (*start)
	{
		const WCHAR sharp = '#';
		const WCHAR questionmark = '?';
		const WCHAR star = '*';
		const WCHAR exclamationmark = '!';
		const WCHAR percent = '%';

		if ((*start == sharp) || (*start == questionmark) || (*start == star) ||
		    (*start == exclamationmark) || (*start == percent))
		{
			count++;
		}
		start++;
	}
	return count;
}

static const char* stop_at_special_chars(const char* str)
{
	const char* start = str;
	WINPR_ASSERT(str);

	while (*start)
	{
		if (*start == '#' || *start == '?' || *start == '*' || *start == '!' || *start == '%')
		{
			return start;
		}
		start++;
	}
	return NULL;
}

/* The universal converter from filedescriptors to different file lists */
static void* convert_filedescriptors_to_file_list(wClipboard* clipboard, UINT32 formatId,
                                                  const void* data, UINT32* pSize,
                                                  const char* header, const char* lineprefix,
                                                  const char* lineending, BOOL skip_last_lineending)
{
	const WCHAR backslash = '\\';
	const FILEDESCRIPTORW* descriptors = NULL;
	UINT32 nrDescriptors = 0;
	size_t count, x, alloc, pos, baseLength = 0;
	const char* src = (const char*)data;
	char* dst;
	size_t header_len = strlen(header);
	size_t lineprefix_len = strlen(lineprefix);
	size_t lineending_len = strlen(lineending);
	size_t decoration_len;

	if (!clipboard || !data || !pSize)
		return NULL;

	if (*pSize < sizeof(UINT32))
		return NULL;

	if (clipboard->delegate.basePath)
		baseLength = strnlen(clipboard->delegate.basePath, MAX_PATH);

	if (baseLength < 1)
		return NULL;

	if (clipboard->delegate.ClientRequestFileSize)
		nrDescriptors = (UINT32)(src[3] << 24) | (UINT32)(src[2] << 16) | (UINT32)(src[1] << 8) |
		                (src[0] & 0xFF);

	count = (*pSize - 4) / sizeof(FILEDESCRIPTORW);

	if ((count < 1) || (count != nrDescriptors))
		return NULL;

	descriptors = (const FILEDESCRIPTORW*)&src[4];

	if (formatId != ClipboardGetFormatId(clipboard, mime_FileGroupDescriptorW))
		return NULL;

	/* Plus 1 for '/' between basepath and filename*/
	decoration_len = lineprefix_len + lineending_len + baseLength + 1;
	alloc = header_len;

	/* Get total size of file/folder names under first level folder only */
	for (x = 0; x < count; x++)
	{
		const FILEDESCRIPTORW* dsc = &descriptors[x];

		if (_wcschr(dsc->cFileName, backslash) == NULL)
		{
			alloc += ARRAYSIZE(dsc->cFileName) *
			         8; /* Overallocate, just take the biggest value the result path can have */
			            /* # (1 char) -> %23 (3 chars) , the first char is replaced inplace */
			alloc += count_special_chars(dsc->cFileName) * 2;
			alloc += decoration_len;
		}
	}

	/* Append a prefix file:// and postfix \n for each file */
	/* We need to keep last \n since snprintf is null terminated!!  */
	alloc++;
	dst = calloc(alloc, sizeof(char));

	if (!dst)
		return NULL;

	_snprintf(&dst[0], alloc, "%s", header);

	pos = header_len;

	for (x = 0; x < count; x++)
	{
		const FILEDESCRIPTORW* dsc = &descriptors[x];
		BOOL fail = TRUE;
		if (_wcschr(dsc->cFileName, backslash) != NULL)
		{
			continue;
		}
		int rc = -1;
		char curName[520] = { 0 };
		const char* stop_at = NULL;
		const char* previous_at = NULL;

		if (ConvertWCharNToUtf8(dsc->cFileName, ARRAYSIZE(dsc->cFileName), curName,
		                        ARRAYSIZE(curName)) < 0)
			goto loop_fail;

		rc = _snprintf(&dst[pos], alloc - pos, "%s%s/", lineprefix, clipboard->delegate.basePath);

		if (rc < 0)
			goto loop_fail;

		pos += (size_t)rc;

		previous_at = curName;
		while ((stop_at = stop_at_special_chars(previous_at)) != NULL)
		{
			char* tmp = strndup(previous_at, stop_at - previous_at);
			if (!tmp)
				goto loop_fail;

			rc = _snprintf(&dst[pos], stop_at - previous_at + 1, "%s", tmp);
			free(tmp);
			if (rc < 0)
				goto loop_fail;

			pos += (size_t)rc;
			rc = _snprintf(&dst[pos], 4, "%%%x", *stop_at);
			if (rc < 0)
				goto loop_fail;

			pos += (size_t)rc;
			previous_at = stop_at + 1;
		}

		rc = _snprintf(&dst[pos], alloc - pos, "%s%s", previous_at, lineending);

		fail = FALSE;
	loop_fail:
		if ((rc < 0) || fail)
		{
			free(dst);
			return NULL;
		}

		pos += (size_t)rc;
	}

	if (skip_last_lineending)
	{
		const size_t endlen = strlen(lineending);
		if (alloc > endlen)
		{
			if (memcmp(&dst[alloc - endlen - 1], lineending, endlen) == 0)
			{
				memset(&dst[alloc - endlen - 1], 0, endlen);
				alloc -= endlen;
			}
		}
	}
	winpr_HexDump(TAG, WLOG_DEBUG, (const BYTE*)dst, alloc);
	*pSize = (UINT32)alloc;
	clipboard->fileListSequenceNumber = clipboard->sequenceNumber;
	return dst;
}

/* Prepend header of kde dolphin format to file list
 * See:
 *   GTK: https://docs.gtk.org/glib/struct.Uri.html
 *   uri syntax: https://www.rfc-editor.org/rfc/rfc3986#section-3
 *   uri-lists format: https://www.rfc-editor.org/rfc/rfc2483#section-5
 */
static void* convert_filedescriptors_to_uri_list(wClipboard* clipboard, UINT32 formatId,
                                                 const void* data, UINT32* pSize)
{
	return convert_filedescriptors_to_file_list(clipboard, formatId, data, pSize, "",
	                                            "file:", "\r\n", FALSE);
}

/* Prepend header of common gnome format to file list*/
static void* convert_filedescriptors_to_gnome_copied_files(wClipboard* clipboard, UINT32 formatId,
                                                           const void* data, UINT32* pSize)
{
	return convert_filedescriptors_to_file_list(clipboard, formatId, data, pSize, "copy\n",
	                                            "file://", "\n", TRUE);
}

/* Prepend header of nautilus based filemanager's format to file list*/
static void* convert_filedescriptors_to_nautilus_clipboard(wClipboard* clipboard, UINT32 formatId,
                                                           const void* data, UINT32* pSize)
{
	/* Here Nemo (and Caja) have different behavior. They encounter error with the last \n . but
	   nautilus needs it. So user have to skip Nemo's error dialog to continue. Caja has different
	   TARGET , so it's easy to fix. see convert_filedescriptors_to_mate_copied_files

	   The text based "x-special/nautilus-clipboard" type was introduced with GNOME 3.30 and
	   was necessary for the desktop icons extension, as gnome-shell at that time only
	   supported text based mime types for gnome extensions. With GNOME 3.38, gnome-shell got
	   support for non-text based mime types for gnome extensions. With GNOME 40, nautilus reverted
	   the mime type change to "x-special/gnome-copied-files" and removed support for the text based
	   mime type. So, in the near future, change this behaviour in favor for Nemo and Caja.
	*/
	/*	see nautilus/src/nautilus-clipboard.c:convert_selection_data_to_str_list
	    see nemo/libnemo-private/nemo-clipboard.c:nemo_clipboard_get_uri_list_from_selection_data
	*/

	return convert_filedescriptors_to_file_list(clipboard, formatId, data, pSize,
	                                            "x-special/nautilus-clipboard\ncopy\n", "file://",
	                                            "\n", FALSE);
}

static void* convert_filedescriptors_to_mate_copied_files(wClipboard* clipboard, UINT32 formatId,
                                                          const void* data, UINT32* pSize)
{

	char* pDstData = convert_filedescriptors_to_file_list(clipboard, formatId, data, pSize,
	                                                      "copy\n", "file://", "\n", FALSE);
	if (!pDstData)
	{
		return pDstData;
	}
	/*  Replace last \n with \0
	    see
	   mate-desktop/caja/libcaja-private/caja-clipboard.c:caja_clipboard_get_uri_list_from_selection_data
	*/

	pDstData[*pSize - 1] = '\0';
	*pSize = *pSize - 1;
	return pDstData;
}

static void array_free_synthetic_file(void* the_file)
{
	struct synthetic_file* file = the_file;
	free_synthetic_file(file);
}

static BOOL register_file_formats_and_synthesizers(wClipboard* clipboard)
{
	wObject* obj;
	UINT32 file_group_format_id;
	UINT32 local_file_format_id;
	UINT32 local_gnome_file_format_id;
	UINT32 local_mate_file_format_id;
	UINT32 local_nautilus_file_format_id;

	/*
	    1. Gnome Nautilus based file manager (Nautilus only with version >= 3.30 AND < 40):
	        TARGET: UTF8_STRING
	        format: x-special/nautilus-clipboard\copy\n\file://path\n\0
	    2. Kde Dolpin and Qt:
	        TARGET: text/uri-list
	        format: file:path\r\n\0
	        See:
	          GTK: https://docs.gtk.org/glib/struct.Uri.html
	          uri syntax: https://www.rfc-editor.org/rfc/rfc3986#section-3
	          uri-lists fomat: https://www.rfc-editor.org/rfc/rfc2483#section-5
	    3. Gnome and others (Unity/XFCE/Nautilus < 3.30/Nautilus >= 40):
	        TARGET: x-special/gnome-copied-files
	        format: copy\nfile://path\n\0
	    4. Mate Caja:
	        TARGET: x-special/mate-copied-files
	        format: copy\nfile://path\n

	    TODO: other file managers do not use previous targets and formats.
	*/

	local_gnome_file_format_id = ClipboardRegisterFormat(clipboard, mime_gnome_copied_files);
	local_mate_file_format_id = ClipboardRegisterFormat(clipboard, mime_mate_copied_files);
	local_nautilus_file_format_id = ClipboardRegisterFormat(clipboard, mime_utf8_string);
	file_group_format_id = ClipboardRegisterFormat(clipboard, mime_FileGroupDescriptorW);
	local_file_format_id = ClipboardRegisterFormat(clipboard, mime_uri_list);

	if (!file_group_format_id || !local_file_format_id || !local_gnome_file_format_id ||
	    !local_mate_file_format_id || !local_nautilus_file_format_id)
		goto error;

	clipboard->localFiles = ArrayList_New(FALSE);

	if (!clipboard->localFiles)
		goto error;

	obj = ArrayList_Object(clipboard->localFiles);
	obj->fnObjectFree = array_free_synthetic_file;

	if (!ClipboardRegisterSynthesizer(clipboard, local_file_format_id, file_group_format_id,
	                                  convert_uri_list_to_filedescriptors))
		goto error_free_local_files;

	if (!ClipboardRegisterSynthesizer(clipboard, file_group_format_id, local_file_format_id,
	                                  convert_filedescriptors_to_uri_list))
		goto error_free_local_files;

	if (!ClipboardRegisterSynthesizer(clipboard, local_gnome_file_format_id, file_group_format_id,
	                                  convert_gnome_copied_files_to_filedescriptors))
		goto error_free_local_files;

	if (!ClipboardRegisterSynthesizer(clipboard, file_group_format_id, local_gnome_file_format_id,
	                                  convert_filedescriptors_to_gnome_copied_files))
		goto error_free_local_files;

	if (!ClipboardRegisterSynthesizer(clipboard, local_mate_file_format_id, file_group_format_id,
	                                  convert_mate_copied_files_to_filedescriptors))
		goto error_free_local_files;

	if (!ClipboardRegisterSynthesizer(clipboard, file_group_format_id, local_mate_file_format_id,
	                                  convert_filedescriptors_to_mate_copied_files))
		goto error_free_local_files;

	if (!ClipboardRegisterSynthesizer(clipboard, local_nautilus_file_format_id,
	                                  file_group_format_id,
	                                  convert_nautilus_clipboard_to_filedescriptors))
		goto error_free_local_files;

	if (!ClipboardRegisterSynthesizer(clipboard, file_group_format_id,
	                                  local_nautilus_file_format_id,
	                                  convert_filedescriptors_to_nautilus_clipboard))
		goto error_free_local_files;

	return TRUE;
error_free_local_files:
	ArrayList_Free(clipboard->localFiles);
	clipboard->localFiles = NULL;
error:
	return FALSE;
}

static UINT file_get_size(const struct synthetic_file* file, UINT64* size)
{
	UINT64 s = 0;

	if (!file || !size)
		return E_INVALIDARG;

	s = file->nFileSizeHigh;
	s <<= 32;
	s |= file->nFileSizeLow;
	*size = s;
	return NO_ERROR;
}

static UINT delegate_file_request_size(wClipboardDelegate* delegate,
                                       const wClipboardFileSizeRequest* request)
{
	UINT error = NO_ERROR;
	UINT64 size = 0;
	struct synthetic_file* file = NULL;

	if (!delegate || !delegate->clipboard || !request)
		return ERROR_BAD_ARGUMENTS;

	if (delegate->clipboard->sequenceNumber != delegate->clipboard->fileListSequenceNumber)
		return ERROR_INVALID_STATE;

	file = ArrayList_GetItem(delegate->clipboard->localFiles, request->listIndex);

	if (!file)
		return ERROR_INDEX_ABSENT;

	error = file_get_size(file, &size);

	if (error)
		error = delegate->ClipboardFileSizeFailure(delegate, request, error);
	else
		error = delegate->ClipboardFileSizeSuccess(delegate, request, size);

	if (error)
		WLog_WARN(TAG, "failed to report file size result: 0x%08X", error);

	return NO_ERROR;
}

UINT synthetic_file_read_close(struct synthetic_file* file, BOOL force)
{
	if (!file || INVALID_HANDLE_VALUE == file->fd)
		return NO_ERROR;

	/* Always force close the file. Clipboard might open hundreds of files
	 * so avoid caching to prevent running out of available file descriptors */
	UINT64 size = 0;
	file_get_size(file, &size);
	if ((file->offset < 0) || ((UINT64)file->offset >= size) || force)
	{
		WLog_VRB(TAG, "close file %d", file->fd);
		if (CloseHandle(file->fd) < 0)
		{
			WLog_WARN(TAG, "failed to close fd %d: %" PRIu32, file->fd, GetLastError());
		}

		file->fd = INVALID_HANDLE_VALUE;
	}

	return NO_ERROR;
}

static UINT file_get_range(struct synthetic_file* file, UINT64 offset, UINT32 size,
                           BYTE** actual_data, UINT32* actual_size)
{
	UINT error = NO_ERROR;
	DWORD dwLow = 0;
	DWORD dwHigh = 0;
	BYTE* buffer = NULL;

	WINPR_ASSERT(file);
	WINPR_ASSERT(actual_data);
	WINPR_ASSERT(actual_size);

	if (INVALID_HANDLE_VALUE == file->fd)
	{
		BY_HANDLE_FILE_INFORMATION FileInfo = { 0 };

		file->fd = CreateFileW(file->local_name, GENERIC_READ, 0, NULL, OPEN_EXISTING,
		                       FILE_ATTRIBUTE_NORMAL, NULL);
		if (INVALID_HANDLE_VALUE == file->fd)
		{
			error = GetLastError();
			WLog_ERR(TAG, "failed to open file %s: %s", file->local_name, error);
			return error;
		}

		if (!GetFileInformationByHandle(file->fd, &FileInfo))
		{
			file->fd = INVALID_HANDLE_VALUE;
			CloseHandle(file->fd);
			error = GetLastError();
			WLog_ERR(TAG, "Get file [%s] information fail: %d", file->local_name, error);
			return error;
		}

		file->offset = 0;
		file->nFileSizeHigh = FileInfo.nFileSizeHigh;
		file->nFileSizeLow = FileInfo.nFileSizeLow;

		/*
		{
		    UINT64 s = 0;
		    file_get_size(file, &s);
		    WLog_DBG(TAG, "open file %d -> %s", file->fd, file->local_name);
		    WLog_DBG(TAG, "file %d size: %" PRIu64 " bytes", file->fd, s);
		} //*/
	}

	do
	{
		/*
		 * We should avoid seeking when possible as some filesystems (e.g.,
		 * an FTP server mapped via FUSE) may not support seeking. We keep
		 * an accurate account of the current file offset and do not call
		 * lseek() if the client requests file content sequentially.
		 */
		if (offset > INT64_MAX)
		{
			WLog_ERR(TAG, "offset [%" PRIu64 "] > INT64_MAX", offset);
			error = ERROR_SEEK;
			break;
		}

		if (file->offset != (INT64)offset)
		{
			WLog_DBG(TAG, "file %d force seeking to %" PRIu64 ", current %" PRIu64, file->fd,
			         offset, file->offset);

			dwHigh = offset >> 32;
			dwLow = offset & 0xFFFFFFFF;
			if (INVALID_SET_FILE_POINTER ==
			    SetFilePointer(file->fd, dwLow, (PLONG)&dwHigh, FILE_BEGIN))
			{
				error = GetLastError();
				break;
			}
		}

		buffer = malloc(size);
		if (!buffer)
		{
			error = ERROR_NOT_ENOUGH_MEMORY;
			break;
		}
		if (!ReadFile(file->fd, buffer, size, (LPDWORD)actual_size, NULL))
		{
			error = GetLastError();
			break;
		}

		*actual_data = buffer;
		file->offset += *actual_size;
		WLog_VRB(TAG, "file %d actual read %" PRIu32 " bytes (offset %" PRIu64 ")", file->fd,
		         *actual_size, file->offset);
	} while (0);

	if (NO_ERROR != error)
	{
		free(buffer);
	}
	synthetic_file_read_close(file, (error != NO_ERROR) && (size > 0));
	return error;
}

static UINT delegate_file_request_range(wClipboardDelegate* delegate,
                                        const wClipboardFileRangeRequest* request)
{
	UINT error = 0;
	BYTE* data = NULL;
	UINT32 size = 0;
	UINT64 offset = 0;
	struct synthetic_file* file = NULL;

	if (!delegate || !delegate->clipboard || !request)
		return ERROR_BAD_ARGUMENTS;

	if (delegate->clipboard->sequenceNumber != delegate->clipboard->fileListSequenceNumber)
		return ERROR_INVALID_STATE;

	file = ArrayList_GetItem(delegate->clipboard->localFiles, request->listIndex);

	if (!file)
		return ERROR_INDEX_ABSENT;

	offset = (((UINT64)request->nPositionHigh) << 32) | ((UINT64)request->nPositionLow);
	error = file_get_range(file, offset, request->cbRequested, &data, &size);

	if (error)
		error = delegate->ClipboardFileRangeFailure(delegate, request, error);
	else
		error = delegate->ClipboardFileRangeSuccess(delegate, request, data, size);

	if (error)
		WLog_WARN(TAG, "failed to report file range result: 0x%08X", error);

	free(data);
	return NO_ERROR;
}

static UINT dummy_file_size_success(wClipboardDelegate* delegate,
                                    const wClipboardFileSizeRequest* request, UINT64 fileSize)
{
	return ERROR_NOT_SUPPORTED;
}

static UINT dummy_file_size_failure(wClipboardDelegate* delegate,
                                    const wClipboardFileSizeRequest* request, UINT errorCode)
{
	return ERROR_NOT_SUPPORTED;
}

static UINT dummy_file_range_success(wClipboardDelegate* delegate,
                                     const wClipboardFileRangeRequest* request, const BYTE* data,
                                     UINT32 size)
{
	return ERROR_NOT_SUPPORTED;
}

static UINT dummy_file_range_failure(wClipboardDelegate* delegate,
                                     const wClipboardFileRangeRequest* request, UINT errorCode)
{
	return ERROR_NOT_SUPPORTED;
}

static void setup_delegate(wClipboardDelegate* delegate)
{
	WINPR_ASSERT(delegate);

	delegate->ClientRequestFileSize = delegate_file_request_size;
	delegate->ClipboardFileSizeSuccess = dummy_file_size_success;
	delegate->ClipboardFileSizeFailure = dummy_file_size_failure;
	delegate->ClientRequestFileRange = delegate_file_request_range;
	delegate->ClipboardFileRangeSuccess = dummy_file_range_success;
	delegate->ClipboardFileRangeFailure = dummy_file_range_failure;
	delegate->IsFileNameComponentValid = ValidFileNameComponent;
}

BOOL ClipboardInitSyntheticFileSubsystem(wClipboard* clipboard)
{
	if (!clipboard)
		return FALSE;

	if (!register_file_formats_and_synthesizers(clipboard))
		return FALSE;

	setup_delegate(&clipboard->delegate);
	return TRUE;
}
