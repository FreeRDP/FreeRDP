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

#define WIN32_FILETIME_TO_UNIX_EPOCH UINT64_C(11644473600)

#include <stddef.h>
#include <stdlib.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include "dirent.h"
#define S_ISDIR(x) (((x)&S_IFDIR))
#include <winpr/wtypes.h>
typedef SSIZE_T ssize_t;
#else
#include <dirent.h>
#include <unistd.h>
#endif

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

struct posix_file
{
	char* local_name;
	WCHAR* remote_name;
	BOOL is_directory;
	UINT64 last_write_time;

	int fd;
	INT64 offset;
	INT64 size;
};

static struct posix_file* make_posix_file(const char* local_name, const WCHAR* remote_name)
{
	struct posix_file* file = NULL;
	struct stat statbuf;
	file = calloc(1, sizeof(*file));

	if (!file)
		return NULL;

	file->fd = -1;
	file->offset = 0;
	file->local_name = _strdup(local_name);
	file->remote_name = _wcsdup(remote_name);

	if (!file->local_name || !file->remote_name)
		goto error;

	if (stat(local_name, &statbuf))
	{
		int err = errno;
		WLog_ERR(TAG, "failed to stat %s: %s", local_name, strerror(err));
		goto error;
	}

	file->is_directory = S_ISDIR(statbuf.st_mode);
	file->last_write_time = (statbuf.st_mtime + WIN32_FILETIME_TO_UNIX_EPOCH) * 10 * 1000 * 1000;
	file->size = statbuf.st_size;
	return file;
error:
	free(file->local_name);
	free(file->remote_name);
	free(file);
	return NULL;
}
static UINT posix_file_read_close(struct posix_file* file, BOOL force);

static void free_posix_file(void* the_file)
{
	struct posix_file* file = the_file;

	if (!file)
		return;

	posix_file_read_close(file, TRUE);

	free(file->local_name);
	free(file->remote_name);
	free(file);
}

static unsigned char hex_to_dec(char c, BOOL* valid)
{
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
static WCHAR* convert_local_name_component_to_remote(wClipboard* clipboard, const char* local_name)
{
	wClipboardDelegate* delegate = ClipboardGetDelegate(clipboard);
	WCHAR* remote_name = NULL;

	WINPR_ASSERT(delegate);

	/*
	 * Note that local file names are not actually guaranteed to be
	 * encoded in UTF-8. Filesystems and users can use whatever they
	 * want. The OS does not care, aside from special treatment of
	 * '\0' and '/' bytes. But we need to make some decision here.
	 * Assuming UTF-8 is currently the most sane thing.
	 */
	if (!ConvertToUnicode(CP_UTF8, 0, local_name, -1, &remote_name, 0))
	{
		WLog_ERR(TAG, "Unicode conversion failed for %s", local_name);
		goto error;
	}

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

static char* concat_local_name(const char* dir, const char* file)
{
	size_t len_dir = 0;
	size_t len_file = 0;
	char* buffer = NULL;
	len_dir = strlen(dir);
	len_file = strlen(file);
	buffer = calloc(len_dir + 1 + len_file + 1, sizeof(char));

	if (!buffer)
		return NULL;

	memcpy(buffer, dir, len_dir * sizeof(char));
	buffer[len_dir] = '/';
	memcpy(buffer + len_dir + 1, file, len_file * sizeof(char));
	return buffer;
}

static WCHAR* concat_remote_name(const WCHAR* dir, const WCHAR* file)
{
	size_t len_dir = 0;
	size_t len_file = 0;
	WCHAR* buffer = NULL;
	len_dir = _wcslen(dir);
	len_file = _wcslen(file);
	buffer = calloc(len_dir + 1 + len_file + 2, sizeof(WCHAR));

	if (!buffer)
		return NULL;

	memcpy(buffer, dir, len_dir * sizeof(WCHAR));
	buffer[len_dir] = L'\\';
	memcpy(buffer + len_dir + 1, file, len_file * sizeof(WCHAR));
	return buffer;
}

static BOOL add_file_to_list(wClipboard* clipboard, const char* local_name,
                             const WCHAR* remote_name, wArrayList* files);

static BOOL add_directory_entry_to_list(wClipboard* clipboard, const char* local_dir_name,
                                        const WCHAR* remote_dir_name, const struct dirent* entry,
                                        wArrayList* files)
{
	BOOL result = FALSE;
	char* local_name = NULL;
	WCHAR* remote_name = NULL;
	WCHAR* remote_base_name = NULL;

	/* Skip special directory entries. */
	if ((strcmp(entry->d_name, ".") == 0) || (strcmp(entry->d_name, "..") == 0))
		return TRUE;

	remote_base_name = convert_local_name_component_to_remote(clipboard, entry->d_name);

	if (!remote_base_name)
		return FALSE;

	local_name = concat_local_name(local_dir_name, entry->d_name);
	remote_name = concat_remote_name(remote_dir_name, remote_base_name);

	if (local_name && remote_name)
		result = add_file_to_list(clipboard, local_name, remote_name, files);

	free(remote_base_name);
	free(remote_name);
	free(local_name);
	return result;
}

static BOOL do_add_directory_contents_to_list(wClipboard* clipboard, const char* local_name,
                                              const WCHAR* remote_name, DIR* dirp,
                                              wArrayList* files)
{
	/*
	 * For some reason POSIX does not require readdir() to be thread-safe.
	 * However, readdir_r() has really insane interface and is pretty bad
	 * replacement for it. Fortunately, most C libraries guarantee thread-
	 * safety of readdir() when it is used for distinct directory streams.
	 *
	 * Thus we can use readdir() in multithreaded applications if we are
	 * sure that it will not corrupt some global data. It would be nice
	 * to have a compile-time check for this here, but some C libraries
	 * do not provide a #define because of reasons (I'm looking at you,
	 * musl). We should not be breaking people's builds because of that,
	 * so we do nothing and proceed with fingers crossed.
	 */
	for (;;)
	{
		struct dirent* entry = NULL;
		errno = 0;
		entry = readdir(dirp);

		if (!entry)
		{
			int err = errno;

			if (!err)
				break;

			WLog_ERR(TAG, "failed to read directory: %s", strerror(err));
			return FALSE;
		}

		if (!add_directory_entry_to_list(clipboard, local_name, remote_name, entry, files))
			return FALSE;
	}

	return TRUE;
}

static BOOL add_directory_contents_to_list(wClipboard* clipboard, const char* local_name,
                                           const WCHAR* remote_name, wArrayList* files)
{
	BOOL result = FALSE;
	DIR* dirp = NULL;
	WLog_VRB(TAG, "adding directory: %s", local_name);
	dirp = opendir(local_name);

	if (!dirp)
	{
		int err = errno;
		WLog_ERR(TAG, "failed to open directory %s: %s", local_name, strerror(err));
		goto out;
	}

	result = do_add_directory_contents_to_list(clipboard, local_name, remote_name, dirp, files);

	if (closedir(dirp))
	{
		int err = errno;
		WLog_WARN(TAG, "failed to close directory: %s", strerror(err));
	}

out:
	return result;
}

static BOOL add_file_to_list(wClipboard* clipboard, const char* local_name,
                             const WCHAR* remote_name, wArrayList* files)
{
	struct posix_file* file = NULL;
	WLog_VRB(TAG, "adding file: %s", local_name);
	file = make_posix_file(local_name, remote_name);

	if (!file)
		return FALSE;

	if (!ArrayList_Append(files, file))
	{
		free_posix_file(file);
		return FALSE;
	}

	if (file->is_directory)
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

static const char* get_basename(const char* name)
{
	const char* c = name;
	const char* last_name = name;

	while (*c++)
	{
		if (*c == '/')
			last_name = c + 1;
	}

	return last_name;
}

static BOOL process_file_name(wClipboard* clipboard, const char* local_name, wArrayList* files)
{
	BOOL result = FALSE;
	const char* base_name = NULL;
	WCHAR* remote_name = NULL;
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

static BOOL is_dos_driver(const char* path, size_t len)
{
	if (len < 2)
		return FALSE;

	if (path[1] == ':' || path[1] == '|')
	{
		if (((path[0] >= 'A') && (path[0] <= 'Z')) || ((path[0] >= 'a') && (path[0] <= 'z')))
			return TRUE;
	}
	return FALSE;
}

#if defined(BUILD_TESTING)
WINPR_API
#else
static
#endif
    char*
    parse_uri_to_local_file(const char* uri, size_t uri_len)
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

			if (is_dos_driver(&uri[prefixLen], uri_len - prefixLen))
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
			if (is_dos_driver(&uri[prefixLen + 1], uri_len - prefixLen - 1))
			{
				// Dos and Windows file URI
				localName = (char*)(uri + prefixLen + 1);
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

		if (is_dos_driver(&localName[1], localLen - 1))
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
		result = process_file_name(clipboard, name, clipboard->localFiles);
		free(name);
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

static BOOL convert_local_file_to_filedescriptor(const struct posix_file* file,
                                                 FILEDESCRIPTORW* descriptor)
{
	size_t remote_len = 0;
	descriptor->dwFlags = FD_ATTRIBUTES | FD_FILESIZE | FD_WRITESTIME | FD_PROGRESSUI;

	if (file->is_directory)
	{
		descriptor->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		descriptor->nFileSizeLow = 0;
		descriptor->nFileSizeHigh = 0;
	}
	else
	{
		descriptor->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
		descriptor->nFileSizeLow = (file->size >> 0) & 0xFFFFFFFF;
		descriptor->nFileSizeHigh = (file->size >> 32) & 0xFFFFFFFF;
	}

	descriptor->ftLastWriteTime.dwLowDateTime = (file->last_write_time >> 0) & 0xFFFFFFFF;
	descriptor->ftLastWriteTime.dwHighDateTime = (file->last_write_time >> 32) & 0xFFFFFFFF;

	remote_len = _wcslen(file->remote_name);

	if (remote_len + 1 > ARRAYSIZE(descriptor->cFileName))
	{
		WLog_ERR(TAG, "file name too long (%" PRIuz " characters)", remote_len);
		return FALSE;
	}

	memcpy(descriptor->cFileName, file->remote_name, remote_len * sizeof(WCHAR));
	return TRUE;
}

static FILEDESCRIPTORW* convert_local_file_list_to_filedescriptors(wArrayList* files)
{
	int i;
	int count = 0;
	FILEDESCRIPTORW* descriptors = NULL;
	count = ArrayList_Count(files);
	descriptors = calloc(count, sizeof(descriptors[0]));

	if (!descriptors)
		goto error;

	for (i = 0; i < count; i++)
	{
		const struct posix_file* file = ArrayList_GetItem(files, i);

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
	FILEDESCRIPTORW* descriptors =
	    convert_local_file_list_to_filedescriptors(clipboard->localFiles);

	WINPR_ASSERT(pSize);

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
	const WCHAR* start = (const WCHAR*)str;
	while (*start)
	{
		if (*start == L'#' || *start == L'?' || *start == L'*' || *start == L'!' || *start == L'%')
		{
			count++;
		}
		start++;
	}
	return count;
}

static const char* stop_at_special_chars(const char* str)
{
	const char* start = (const char*)str;
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
	const FILEDESCRIPTORW* descriptors;
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
		if (_wcschr(descriptors[x].cFileName, L'\\') == NULL)
		{
			size_t curLen = _wcsnlen(descriptors[x].cFileName, ARRAYSIZE(descriptors[x].cFileName));
			alloc += WideCharToMultiByte(CP_UTF8, 0, descriptors[x].cFileName, (int)curLen, NULL, 0,
			                             NULL, NULL);
			/* # (1 char) -> %23 (3 chars) , the first char is replaced inplace */
			alloc += count_special_chars(descriptors[x].cFileName) * 2;
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
		BOOL fail = TRUE;
		if (_wcschr(descriptors[x].cFileName, L'\\') != NULL)
		{
			continue;
		}
		int rc;
		const FILEDESCRIPTORW* cur = &descriptors[x];
		size_t curLen = _wcsnlen(cur->cFileName, ARRAYSIZE(cur->cFileName));
		char* curName = NULL;
		const char* stop_at = NULL;
		const char* previous_at = NULL;
		rc = ConvertFromUnicode(CP_UTF8, 0, cur->cFileName, (int)curLen, &curName, 0, NULL, NULL);
		if (rc < 0)
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
			free(curName);
			return NULL;
		}
		free(curName);

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

	char* pDstData = (char*)convert_filedescriptors_to_file_list(clipboard, formatId, data, pSize,
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
	obj->fnObjectFree = free_posix_file;

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

static UINT posix_file_get_size(const struct posix_file* file, INT64* size)
{
	struct stat statbuf;

	if (stat(file->local_name, &statbuf) < 0)
	{
		int err = errno;
		WLog_ERR(TAG, "failed to stat %s: %s", file->local_name, strerror(err));
		return ERROR_FILE_INVALID;
	}

	*size = statbuf.st_size;
	return NO_ERROR;
}

static UINT posix_file_request_size(wClipboardDelegate* delegate,
                                    const wClipboardFileSizeRequest* request)
{
	UINT error = NO_ERROR;
	INT64 size = 0;
	struct posix_file* file = NULL;

	if (!delegate || !delegate->clipboard || !request)
		return ERROR_BAD_ARGUMENTS;

	if (delegate->clipboard->sequenceNumber != delegate->clipboard->fileListSequenceNumber)
		return ERROR_INVALID_STATE;

	file = ArrayList_GetItem(delegate->clipboard->localFiles, request->listIndex);

	if (!file)
		return ERROR_INDEX_ABSENT;

	error = posix_file_get_size(file, &size);

	if (error)
		error = delegate->ClipboardFileSizeFailure(delegate, request, error);
	else
		error = delegate->ClipboardFileSizeSuccess(delegate, request, size);

	if (error)
		WLog_WARN(TAG, "failed to report file size result: 0x%08X", error);

	return NO_ERROR;
}

static UINT posix_file_read_open(struct posix_file* file)
{
	struct stat statbuf;

	if (file->fd >= 0)
		return NO_ERROR;

	file->fd = open(file->local_name, O_RDONLY);

	if (file->fd < 0)
	{
		int err = errno;
		WLog_ERR(TAG, "failed to open file %s: %s", file->local_name, strerror(err));
		return ERROR_FILE_NOT_FOUND;
	}

	if (fstat(file->fd, &statbuf) < 0)
	{
		int err = errno;
		WLog_ERR(TAG, "failed to stat file: %s", strerror(err));
		return ERROR_FILE_INVALID;
	}

	file->offset = 0;
	file->size = statbuf.st_size;
	WLog_VRB(TAG, "open file %d -> %s", file->fd, file->local_name);
	WLog_VRB(TAG, "file %d size: %" PRIu64 " bytes", file->fd, file->size);
	return NO_ERROR;
}

static UINT posix_file_read_seek(struct posix_file* file, UINT64 offset)
{
	/*
	 * We should avoid seeking when possible as some filesystems (e.g.,
	 * an FTP server mapped via FUSE) may not support seeking. We keep
	 * an accurate account of the current file offset and do not call
	 * lseek() if the client requests file content sequentially.
	 */
	if (offset > INT64_MAX)
		return ERROR_SEEK;

	if (file->offset == (INT64)offset)
		return NO_ERROR;

	WLog_VRB(TAG, "file %d force seeking to %" PRIu64 ", current %" PRIu64, file->fd, offset,
	         file->offset);

	if (lseek(file->fd, (off_t)offset, SEEK_SET) < 0)
	{
		int err = errno;
		WLog_ERR(TAG, "failed to seek file: %s", strerror(err));
		return ERROR_SEEK;
	}

	return NO_ERROR;
}

static UINT posix_file_read_perform(struct posix_file* file, UINT32 size, BYTE** actual_data,
                                    UINT32* actual_size)
{
	BYTE* buffer = NULL;
	ssize_t amount = 0;
	WLog_VRB(TAG, "file %d request read %" PRIu32 " bytes", file->fd, size);
	buffer = malloc(size);

	if (!buffer)
	{
		WLog_ERR(TAG, "failed to allocate %" PRIu32 " buffer bytes", size);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	amount = read(file->fd, buffer, size);

	if (amount < 0)
	{
		int err = errno;
		WLog_ERR(TAG, "failed to read file: %s", strerror(err));
		goto error;
	}

	*actual_data = buffer;
	*actual_size = amount;
	file->offset += amount;
	WLog_VRB(TAG, "file %d actual read %" PRIu32 " bytes (offset %" PRIu64 ")", file->fd, amount,
	         file->offset);
	return NO_ERROR;
error:
	free(buffer);
	return ERROR_READ_FAULT;
}

UINT posix_file_read_close(struct posix_file* file, BOOL force)
{
	if (file->fd < 0)
		return NO_ERROR;

	/* Always force close the file. Clipboard might open hundreds of files
	 * so avoid caching to prevent running out of available file descriptors */
	if ((file->offset >= file->size) || force || TRUE)
	{
		WLog_VRB(TAG, "close file %d", file->fd);

		if (close(file->fd) < 0)
		{
			int err = errno;
			WLog_WARN(TAG, "failed to close fd %d: %s", file->fd, strerror(err));
		}

		file->fd = -1;
	}

	return NO_ERROR;
}

static UINT posix_file_get_range(struct posix_file* file, UINT64 offset, UINT32 size,
                                 BYTE** actual_data, UINT32* actual_size)
{
	UINT error = NO_ERROR;
	error = posix_file_read_open(file);

	if (error)
		goto out;

	error = posix_file_read_seek(file, offset);

	if (error)
		goto out;

	error = posix_file_read_perform(file, size, actual_data, actual_size);

	if (error)
		goto out;

out:

	posix_file_read_close(file, (error != NO_ERROR) && (size > 0));
	return error;
}

static UINT posix_file_request_range(wClipboardDelegate* delegate,
                                     const wClipboardFileRangeRequest* request)
{
	UINT error = 0;
	BYTE* data = NULL;
	UINT32 size = 0;
	UINT64 offset = 0;
	struct posix_file* file = NULL;

	if (!delegate || !delegate->clipboard || !request)
		return ERROR_BAD_ARGUMENTS;

	if (delegate->clipboard->sequenceNumber != delegate->clipboard->fileListSequenceNumber)
		return ERROR_INVALID_STATE;

	file = ArrayList_GetItem(delegate->clipboard->localFiles, request->listIndex);

	if (!file)
		return ERROR_INDEX_ABSENT;

	offset = (((UINT64)request->nPositionHigh) << 32) | ((UINT64)request->nPositionLow);
	error = posix_file_get_range(file, offset, request->cbRequested, &data, &size);

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
	delegate->ClientRequestFileSize = posix_file_request_size;
	delegate->ClipboardFileSizeSuccess = dummy_file_size_success;
	delegate->ClipboardFileSizeFailure = dummy_file_size_failure;
	delegate->ClientRequestFileRange = posix_file_request_range;
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
