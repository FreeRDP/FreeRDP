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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _FILE_OFFSET_BITS 64

#include <stddef.h>
#include <stdlib.h>
#include <errno.h>

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <winpr/clipboard.h>
#include <winpr/collections.h>
#include <winpr/file.h>
#include <winpr/shell.h>
#include <winpr/string.h>
#include <winpr/wlog.h>

#include "clipboard.h"
#include "posix.h"

#include "../log.h"
#define TAG WINPR_TAG("clipboard.posix")

struct posix_file
{
	char* local_name;
	WCHAR* remote_name;
	BOOL is_directory;

	int fd;
	off_t offset;
	off_t size;
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
	file->size = statbuf.st_size;

	return file;

error:
	free(file->local_name);
	free(file->remote_name);
	free(file);

	return NULL;
}

static void free_posix_file(void* the_file)
{
	struct posix_file* file = the_file;

	if (!file)
		return;

	if (file->fd >= 0)
	{
		if (close(file->fd) < 0)
		{
			int err = errno;
			WLog_WARN(TAG, "failed to close fd %d: %s", file->fd, strerror(err));
		}
	}

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

	if ((end < str) || (end - str < strlen("%20")))
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
static WCHAR* convert_local_name_component_to_remote(const char* local_name)
{
	WCHAR* remote_name = NULL;

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
	 */
	if (!ValidFileNameComponent(remote_name))
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

	buffer = calloc(len_dir + 1 + len_file + 1, sizeof(WCHAR));
	if (!buffer)
		return NULL;

	memcpy(buffer, dir, len_dir * sizeof(WCHAR));
	buffer[len_dir] = L'\\';
	memcpy(buffer + len_dir + 1, file, len_file * sizeof(WCHAR));

	return buffer;
}

static BOOL add_file_to_list(const char* local_name, const WCHAR* remote_name, wArrayList* files);

static BOOL add_directory_entry_to_list(const char* local_dir_name, const WCHAR* remote_dir_name,
		const struct dirent* entry, wArrayList* files)
{
	BOOL result = FALSE;
	char* local_name = NULL;
	WCHAR* remote_name = NULL;
	WCHAR* remote_base_name = NULL;

	/* Skip special directory entries. */
	if ((strcmp(entry->d_name, ".") == 0) || (strcmp(entry->d_name, "..") == 0))
		return TRUE;

	remote_base_name = convert_local_name_component_to_remote(entry->d_name);
	if (!remote_base_name)
		return FALSE;

	local_name = concat_local_name(local_dir_name, entry->d_name);
	remote_name = concat_remote_name(remote_dir_name, remote_base_name);

	if (local_name && remote_name)
		result = add_file_to_list(local_name, remote_name, files);

	free(remote_base_name);
	free(remote_name);
	free(local_name);

	return result;
}

static BOOL do_add_directory_contents_to_list(const char* local_name, const WCHAR* remote_name,
		DIR* dirp, wArrayList* files)
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

		if (!add_directory_entry_to_list(local_name, remote_name, entry, files))
			return FALSE;
	}

	return TRUE;
}

static BOOL add_directory_contents_to_list(const char* local_name, const WCHAR* remote_name,
		wArrayList* files)
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

	result = do_add_directory_contents_to_list(local_name, remote_name, dirp, files);

	if (closedir(dirp))
	{
		int err = errno;
		WLog_WARN(TAG, "failed to close directory: %s", strerror(err));
	}
out:
	return result;
}

static BOOL add_file_to_list(const char* local_name, const WCHAR* remote_name, wArrayList* files)
{
	struct posix_file* file = NULL;

	WLog_VRB(TAG, "adding file: %s", local_name);

	file = make_posix_file(local_name, remote_name);
	if (!file)
		return FALSE;

	if (ArrayList_Add(files, file) < 0)
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
		if (!add_directory_contents_to_list(local_name, remote_name, files))
			return FALSE;
	}

	return TRUE;
}

static const char* basename(const char* name)
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

static BOOL process_file_name(const char* local_name, wArrayList* files)
{
	BOOL result = FALSE;
	const char* base_name = NULL;
	WCHAR* remote_name = NULL;

	/*
	 * Start with the base name of the file. text/uri-list contains the
	 * exact files selected by the user, and we want the remote files
	 * to have names relative to that selection.
	 */
	base_name = basename(local_name);

	remote_name = convert_local_name_component_to_remote(base_name);
	if (!remote_name)
		return FALSE;

	result = add_file_to_list(local_name, remote_name, files);

	free(remote_name);

	return result;
}

static BOOL process_uri(const char* uri, size_t uri_len, wArrayList* files)
{
	BOOL result = FALSE;
	char* name = NULL;

	WLog_VRB(TAG, "processing URI: %.*s", uri_len, uri);

	if ((uri_len < strlen("file://")) || strncmp(uri, "file://", strlen("file://")))
	{
		WLog_ERR(TAG, "non-'file://' URI schemes are not supported");
		goto out;
	}

	name = decode_percent_encoded_string(uri + strlen("file://"), uri_len - strlen("file://"));
	if (!name)
		goto out;

	result = process_file_name(name, files);
out:
	free(name);

	return result;
}

static BOOL process_uri_list(const char* data, size_t length, wArrayList* files)
{
	const char* cur = data;
	const char* lim = data + length;
	const char* start = NULL;
	const char* stop = NULL;

	WLog_VRB(TAG, "processing URI list:\n%.*s", length, data);

	ArrayList_Clear(files);

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
		stop = cur;

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

		if (!process_uri(start, stop - start, files))
			return FALSE;
	}

	return TRUE;
}

static BOOL convert_local_file_to_filedescriptor(const struct posix_file* file,
		FILEDESCRIPTOR* descriptor)
{
	size_t remote_len = 0;

	descriptor->dwFlags = FD_ATTRIBUTES | FD_FILESIZE | FD_SHOWPROGRESSUI;

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

	remote_len = _wcslen(file->remote_name);
	if (remote_len + 1 > ARRAYSIZE(descriptor->cFileName))
	{
		WLog_ERR(TAG, "file name too long (%"PRIuz" characters)", remote_len);
		return FALSE;
	}

	memcpy(descriptor->cFileName, file->remote_name, remote_len * sizeof(WCHAR));

	return TRUE;
}

static FILEDESCRIPTOR* convert_local_file_list_to_filedescriptors(wArrayList* files)
{
	int i;
	int count = 0;
	FILEDESCRIPTOR* descriptors = NULL;

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

static void* convert_uri_list_to_filedescriptors(wClipboard* clipboard, UINT32 formatId,
		const void* data, UINT32* pSize)
{
	FILEDESCRIPTOR* descriptors = NULL;

	if (!clipboard || !data || !pSize)
		return NULL;

	if (formatId != ClipboardGetFormatId(clipboard, "text/uri-list"))
		return NULL;

	if (!process_uri_list((const char*) data, *pSize, clipboard->localFiles))
		return NULL;

	descriptors = convert_local_file_list_to_filedescriptors(clipboard->localFiles);
	if (!descriptors)
		return NULL;

	*pSize = ArrayList_Count(clipboard->localFiles) * sizeof(FILEDESCRIPTOR);

	clipboard->fileListSequenceNumber = clipboard->sequenceNumber;

	return descriptors;
}

static BOOL register_file_formats_and_synthesizers(wClipboard* clipboard)
{
	UINT32 file_group_format_id;
	UINT32 local_file_format_id;

	file_group_format_id = ClipboardRegisterFormat(clipboard, "FileGroupDescriptorW");
	local_file_format_id = ClipboardRegisterFormat(clipboard, "text/uri-list");
	if (!file_group_format_id || !local_file_format_id)
		goto error;

	clipboard->localFiles = ArrayList_New(FALSE);
	if (!clipboard->localFiles)
		goto error;

	ArrayList_Object(clipboard->localFiles)->fnObjectFree = free_posix_file;

	if (!ClipboardRegisterSynthesizer(clipboard,
			local_file_format_id, file_group_format_id,
			convert_uri_list_to_filedescriptors))
		goto error_free_local_files;

	return TRUE;

error_free_local_files:
	ArrayList_Free(clipboard->localFiles);
	clipboard->localFiles = NULL;
error:
	return FALSE;
}

static UINT posix_file_get_size(const struct posix_file* file, off_t* size)
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
	off_t size = 0;
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
	WLog_VRB(TAG, "file %d size: %"PRIu64" bytes", file->fd, file->size);

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
	if (file->offset == offset)
		return NO_ERROR;

	WLog_VRB(TAG, "file %d force seeking to %"PRIu64", current %"PRIu64, file->fd,
		offset, file->offset);

	if (lseek(file->fd, offset, SEEK_SET) < 0)
	{
		int err = errno;
		WLog_ERR(TAG, "failed to seek file: %s", strerror(err));
		return ERROR_SEEK;
	}

	return NO_ERROR;
}

static UINT posix_file_read_perform(struct posix_file* file, UINT32 size,
		BYTE** actual_data, UINT32* actual_size)
{
	BYTE* buffer = NULL;
	ssize_t amount = 0;

	WLog_VRB(TAG, "file %d request read %"PRIu32" bytes", file->fd, size);

	buffer = malloc(size);
	if (!buffer)
	{
		WLog_ERR(TAG, "failed to allocate %"PRIu32" buffer bytes", size);
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

	WLog_VRB(TAG, "file %d actual read %"PRIu32" bytes (offset %"PRIu64")", file->fd,
		amount, file->offset);

	return NO_ERROR;

error:
	free(buffer);

	return ERROR_READ_FAULT;
}

static UINT posix_file_read_close(struct posix_file* file)
{
	if (file->fd < 0)
		return NO_ERROR;

	if (file->offset == file->size)
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

	error = posix_file_read_close(file);
	if (error)
		goto out;
out:
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

	offset = (((UINT64) request->nPositionHigh) << 32) | ((UINT64) request->nPositionLow);
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

static UINT dummy_file_size_success(wClipboardDelegate* delegate, const wClipboardFileSizeRequest* request, UINT64 fileSize)
{
	return ERROR_NOT_SUPPORTED;
}

static UINT dummy_file_size_failure(wClipboardDelegate* delegate, const wClipboardFileSizeRequest* request, UINT errorCode)
{
	return ERROR_NOT_SUPPORTED;
}

static UINT dummy_file_range_success(wClipboardDelegate* delegate, const wClipboardFileRangeRequest* request, const BYTE* data, UINT32 size)
{
	return ERROR_NOT_SUPPORTED;
}

static UINT dummy_file_range_failure(wClipboardDelegate* delegate, const wClipboardFileRangeRequest* request, UINT errorCode)
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
}

BOOL ClipboardInitPosixFileSubsystem(wClipboard* clipboard)
{
	if (!clipboard)
		return FALSE;

	if (!register_file_formats_and_synthesizers(clipboard))
		return FALSE;

	setup_delegate(&clipboard->delegate);

	return TRUE;
}
