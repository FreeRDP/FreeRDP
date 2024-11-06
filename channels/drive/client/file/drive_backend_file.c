/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * File System Virtual Channel
 *
 * Copyright 2024 Armin Novak <armin.novak@thincast.com>
 * Copyright 2024 Thincast Technologies GmbH
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

#include <stdbool.h>
#include <winpr/wtsapi.h>
#include <winpr/path.h>

#include <freerdp/config.h>
#include <freerdp/api.h>

#include <freerdp/client/drive.h>
#include <winpr/file.h>
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("drive.client.backend.file")

struct rdp_drive_context
{
	UINT32 dwDesiredAccess;
	UINT32 dwShareMode;
	UINT32 dwCreationDisposition;
	UINT32 dwFlagsAndAttributes;
	HANDLE file_handle;

	HANDLE find_handle;
	WIN32_FIND_DATAW find_data;
	BY_HANDLE_FILE_INFORMATION file_by_handle;
	wLog* log;
	WCHAR* base_path;
	WCHAR* filename;
	size_t filename_len;
	WCHAR* fullpath;
	bool isDirectory;
	rdpContext* context;
};

static BOOL drive_file_fix_path(WCHAR* path, size_t length)
{
	if ((length == 0) || (length > UINT32_MAX))
		return FALSE;

	WINPR_ASSERT(path);

	for (size_t i = 0; i < length; i++)
	{
		if (path[i] == L'\\')
			path[i] = L'/';
	}

#ifdef WIN32

	if ((length == 3) && (path[1] == L':') && (path[2] == L'/'))
		return FALSE;

#else

	if ((length == 1) && (path[0] == L'/'))
		return FALSE;

#endif

	if ((length > 0) && (path[length - 1] == L'/'))
		path[length - 1] = L'\0';

	return TRUE;
}

WINPR_ATTR_MALLOC(free, 1)
static char* drive_file_resolve_path(const char* what)
{
	if (!what)
		return NULL;

	/* Special case: path[0] == '*' -> export all drives
	 * Special case: path[0] == '%' -> user home dir
	 */
	if (strcmp(what, "%") == 0)
		return GetKnownPath(KNOWN_PATH_HOME);
#ifndef WIN32
	if (strcmp(what, "*") == 0)
		return _strdup("/");
#else
	if (strcmp(what, "*") == 0)
	{
		const size_t len = GetLogicalDriveStringsA(0, NULL);
		char* devlist = calloc(len + 1, sizeof(char*));
		if (!devlist)
			return NULL;

		/* Enumerate all devices: */
		DWORD rc = GetLogicalDriveStringsA(len, devlist);
		if (rc != len)
		{
			free(devlist);
			return NULL;
		}

		char* path = NULL;
		for (size_t i = 0;; i++)
		{
			char* dev = &devlist[i * sizeof(char*)];
			if (!*dev)
				break;
			if (*dev <= 'B')
				continue;

			path = _strdup(dev);
			break;
		}
		free(devlist);
		return path;
	}

#endif

	return _strdup(what);
}

WINPR_ATTR_MALLOC(free, 1)
static char* drive_file_resolve_name(const char* path, const char* suggested)
{
	if (!path)
		return NULL;

	if (strcmp(path, "*") == 0)
	{
		if (suggested)
		{
			char* str = NULL;
			size_t len = 0;
			(void)winpr_asprintf(&str, &len, "[%s] %s", suggested, path);
			return str;
		}
		return _strdup(path);
	}
	if (strcmp(path, "%") == 0)
		return GetKnownPath(KNOWN_PATH_HOME);
	if (suggested)
		return _strdup(suggested);

	return _strdup(path);
}

WINPR_ATTR_MALLOC(free, 1)
static WCHAR* drive_file_combine_fullpath(const WCHAR* base_path, const WCHAR* path,
                                          size_t PathWCharLength)
{
	BOOL ok = FALSE;
	WCHAR* fullpath = NULL;

	if (!base_path || (!path && (PathWCharLength > 0)))
		goto fail;

	const size_t base_path_length = _wcsnlen(base_path, MAX_PATH);
	const size_t length = base_path_length + PathWCharLength + 1;
	fullpath = (WCHAR*)calloc(length, sizeof(WCHAR));

	if (!fullpath)
		goto fail;

	CopyMemory(fullpath, base_path, base_path_length * sizeof(WCHAR));
	if (path)
		CopyMemory(&fullpath[base_path_length], path, PathWCharLength * sizeof(WCHAR));

	if (!drive_file_fix_path(fullpath, length))
		goto fail;

	WCHAR* normalized = winpr_NormalizePathW(fullpath);
	free(fullpath);
	fullpath = normalized;

	ok = winpr_PathIsRootOfW(base_path, fullpath);
fail:
	if (!ok)
	{
		free(fullpath);
		fullpath = NULL;
	}
	return fullpath;
}

static void drive_free(rdpDriveContext* file)
{
	if (!file)
		return;

	if (file->file_handle != INVALID_HANDLE_VALUE)
		(void)CloseHandle(file->file_handle);

	if (file->find_handle != INVALID_HANDLE_VALUE)
		FindClose(file->find_handle);

	free(file->base_path);
	free(file->filename);
	free(file->fullpath);
	free(file);
}

WINPR_ATTR_MALLOC(drive_free, 1)
static rdpDriveContext* drive_new(rdpContext* context)
{
	WINPR_ASSERT(context);

	rdpDriveContext* file = calloc(1, sizeof(rdpDriveContext));
	if (!file)
		return NULL;

	file->log = WLog_Get(TAG);
	file->context = context;
	file->file_handle = INVALID_HANDLE_VALUE;
	file->find_handle = INVALID_HANDLE_VALUE;
	return file;
}

static bool drive_is_file(rdpDriveContext* context)
{
	if (!context)
		return false;
	if (context->isDirectory)
		return false;
	return true;
}

static bool drive_exists(rdpDriveContext* context)
{
	if (!context)
		return false;
	return PathFileExistsW(context->fullpath);
}

static bool drive_empty(rdpDriveContext* context)
{
	if (!context || !context->isDirectory)
		return false;
	return PathIsDirectoryEmptyW(context->fullpath);
}

static SSIZE_T drive_seek(rdpDriveContext* context, SSIZE_T offset, int whence)
{
	if (!drive_exists(context) || !drive_is_file(context))
		return -1;

	LARGE_INTEGER li = { .QuadPart = offset };
	return SetFilePointerEx(context->file_handle, li, NULL, (DWORD)whence);
}

static SSIZE_T drive_read(rdpDriveContext* context, void* buf, size_t nbyte)
{
	if (!drive_exists(context) || !drive_is_file(context))
		return -1;

	if (nbyte > UINT32_MAX)
		return -1;

	DWORD read = 0;
	if (!ReadFile(context->file_handle, buf, (UINT32)nbyte, &read, NULL))
		return -1;

	return read;
}

static SSIZE_T drive_write(rdpDriveContext* context, const void* buf, size_t nbyte)
{
	SSIZE_T rc = 0;
	if (!drive_exists(context) || !drive_is_file(context))
		return -1;

	do
	{
		const DWORD towrite = (nbyte > UINT32_MAX) ? UINT32_MAX : (DWORD)nbyte;
		DWORD written = 0;

		if (!WriteFile(context->file_handle, buf, towrite, &written, NULL))
			return -1;
		if (written > towrite)
			return -1;
		nbyte -= written;
		rc += written;
	} while (nbyte > 0);

	return rc;
}

static bool drive_remove(rdpDriveContext* context)
{
	if (!context)
		return false;

	if (!PathFileExistsW(context->fullpath))
		return false;

	if (context->isDirectory)
		return winpr_RemoveDirectory_RecursiveW(context->fullpath);

	return DeleteFileW(context->fullpath);
}

static UINT32 drive_getFileAttributes(rdpDriveContext* context)
{
	if (!drive_exists(context))
		return 0;
	return GetFileAttributesW(context->fullpath);
}

static bool drive_setFileAttributesAndTimes(rdpDriveContext* context, UINT64 CreationTime,
                                            UINT64 LastAccessTime, UINT64 LastWriteTime,
                                            UINT64 ChangeTime, DWORD FileAttributes)
{
	if (!drive_exists(context))
		return false;

	if (!SetFileAttributesW(context->fullpath, FileAttributes))
		return false;

	union
	{
		UINT64 u64;
		FILETIME ft;
	} c, a, w;
	c.u64 = CreationTime;
	a.u64 = LastAccessTime;
	w.u64 = LastWriteTime;

	FILETIME* pc = (CreationTime > 0) ? &c.ft : NULL;
	FILETIME* pa = (LastAccessTime > 0) ? &a.ft : NULL;
	FILETIME* pw = (LastWriteTime > 0) ? &w.ft : NULL;

	WINPR_UNUSED(ChangeTime);
	return SetFileTime(context->file_handle, pc, pa, pw);
}

static bool drive_setSize(rdpDriveContext* context, INT64 size)
{
	WINPR_ASSERT(context);

	if (!drive_exists(context) || !drive_is_file(context))
	{
		WLog_Print(context->log, WLOG_ERROR, "Unable to truncate %s to %" PRId64 " (%" PRIu32 ")",
		           context->fullpath, size, GetLastError());
		return false;
	}

	const LARGE_INTEGER liSize = { .QuadPart = size };

	if (!SetFilePointerEx(context->file_handle, liSize, NULL, FILE_BEGIN))
	{
		WLog_Print(context->log, WLOG_ERROR, "Unable to truncate %s to %" PRId64 " (%" PRIu32 ")",
		           context->fullpath, size, GetLastError());
		return false;
	}

	if (SetEndOfFile(context->file_handle) == 0)
	{
		WLog_Print(context->log, WLOG_ERROR, "Unable to truncate %s to %" PRId64 " (%" PRIu32 ")",
		           context->fullpath, size, GetLastError());
		return false;
	}

	return true;
}

static const WIN32_FIND_DATAW* drive_first(rdpDriveContext* context, const WCHAR* query,
                                           size_t numCharacters)
{
	if (!context || !query)
		return NULL;

	const size_t len = _wcsnlen(query, numCharacters + 1);
	if (len > numCharacters)
		return NULL;

	if (context->find_handle != INVALID_HANDLE_VALUE)
		CloseHandle(context->find_handle);
	WCHAR* ent_path = drive_file_combine_fullpath(context->base_path, query, numCharacters);
	if (!ent_path)
		return NULL;

	context->find_handle = FindFirstFileW(ent_path, &context->find_data);
	free(ent_path);
	if (context->find_handle == INVALID_HANDLE_VALUE)
		return NULL;
	return &context->find_data;
}

static const WIN32_FIND_DATAW* drive_next(rdpDriveContext* context)
{
	if (!context || (context->find_handle == INVALID_HANDLE_VALUE))
		return NULL;
	if (!FindNextFileW(context->find_handle, &context->find_data))
		return NULL;
	return &context->find_data;
}

static bool drive_check_existing(rdpDriveContext* context)
{
	WINPR_ASSERT(context);
	if (!context->fullpath)
		return false;

	if (PathFileExistsW(context->fullpath))
		return false;

	return true;
}

static bool drive_createDirectory(rdpDriveContext* context)
{
	if (!drive_check_existing(context))
		return false;

	if (!CreateDirectoryW(context->fullpath, NULL))
		return false;
	context->isDirectory = true;
	return true;
}

static bool drive_createFile(rdpDriveContext* context, UINT32 dwDesiredAccess, UINT32 dwShareMode,
                             UINT32 dwCreationDisposition, UINT32 dwFlagsAndAttributes)
{
	WINPR_ASSERT(context);
	if (!context->fullpath || context->isDirectory)
		return false;

	context->file_handle = CreateFileW(context->fullpath, dwDesiredAccess, dwShareMode, NULL,
	                                   dwCreationDisposition, dwFlagsAndAttributes, NULL);
	return context->file_handle != INVALID_HANDLE_VALUE;
}

static bool drive_update_path(rdpDriveContext* context)
{
	WINPR_ASSERT(context);
	free(context->fullpath);
	context->fullpath = NULL;
	if (!context->base_path || !context->filename)
		return false;

	context->fullpath =
	    drive_file_combine_fullpath(context->base_path, context->filename, context->filename_len);
	return context->fullpath != NULL;
}

static bool drive_setPath(rdpDriveContext* context, const WCHAR* base_path, const WCHAR* filename,
                          size_t nbFilenameChar)
{
	WINPR_ASSERT(context);
	WCHAR* cbase_path = NULL;
	WCHAR* cfilename = NULL;
	if (base_path)
		cbase_path = _wcsdup(base_path);
	if (filename)
		cfilename = wcsndup(filename, nbFilenameChar);

	free(context->base_path);
	free(context->filename);
	context->base_path = cbase_path;
	context->filename = cfilename;
	context->filename_len = nbFilenameChar;

	return drive_update_path(context);
}

static const BY_HANDLE_FILE_INFORMATION* drive_getFileAttributeData(rdpDriveContext* context)
{
	if (!drive_exists(context))
		return NULL;
	HANDLE hFile = CreateFileW(context->fullpath, 0, FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
	                           FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		const BOOL rc = GetFileInformationByHandle(hFile, &context->file_by_handle);
		CloseHandle(hFile);
		if (rc)
			goto out;
	}

	WIN32_FILE_ATTRIBUTE_DATA data = { 0 };
	if (!GetFileAttributesExW(context->fullpath, GetFileExInfoStandard, &data))
		return NULL;
	const BY_HANDLE_FILE_INFORMATION empty = { 0 };
	context->file_by_handle = empty;
	context->file_by_handle.dwFileAttributes = data.dwFileAttributes;
	context->file_by_handle.ftCreationTime = data.ftCreationTime;
	context->file_by_handle.ftLastAccessTime = data.ftLastAccessTime;
	context->file_by_handle.ftLastWriteTime = data.ftLastWriteTime;
	context->file_by_handle.nFileSizeHigh = data.nFileSizeHigh;
	context->file_by_handle.nFileSizeLow = data.nFileSizeLow;

out:
	return &context->file_by_handle;
}

static bool drive_move(rdpDriveContext* context, const WCHAR* newName, size_t numCharacters,
                       bool replaceIfExists)
{
	WINPR_ASSERT(context);
	if (!newName || (numCharacters == 0))
		return false;

	const size_t len = _wcsnlen(newName, numCharacters + 1);
	if (len > numCharacters)
		return false;

	bool reopen = false;
	if (!context->isDirectory)
	{
		if (context->file_handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(context->file_handle);
			context->file_handle = INVALID_HANDLE_VALUE;
			reopen = true;
		}
	}

	WCHAR* newpath = drive_file_combine_fullpath(context->base_path, newName, numCharacters);
	if (!newpath)
		return false;

	const DWORD flags = replaceIfExists ? MOVEFILE_REPLACE_EXISTING : 0;
	const BOOL res = MoveFileExW(context->fullpath, newpath, flags);
	free(newpath);
	if (!res)
		return false;

	if (!drive_setPath(context, context->base_path, newName, numCharacters))
		return false;
	if (reopen)
		return drive_createFile(context, context->dwDesiredAccess, context->dwShareMode,
		                        context->dwCreationDisposition, context->dwFlagsAndAttributes);
	return true;
}

static const rdpDriveDriver s_driver = { .resolve_path = drive_file_resolve_path,
	                                     .resolve_name = drive_file_resolve_name,
	                                     .createDirectory = drive_createDirectory,
	                                     .createFile = drive_createFile,
	                                     .new = drive_new,
	                                     .free = drive_free,
	                                     .seek = drive_seek,
	                                     .read = drive_read,
	                                     .write = drive_write,
	                                     .remove = drive_remove,
	                                     .move = drive_move,
	                                     .exists = drive_exists,
	                                     .empty = drive_empty,
	                                     .setSize = drive_setSize,
	                                     .getFileAttributes = drive_getFileAttributes,
	                                     .setFileAttributesAndTimes =
	                                         drive_setFileAttributesAndTimes,
	                                     .setPath = drive_setPath,
	                                     .first = drive_first,
	                                     .next = drive_next,
	                                     .getFileAttributeData = drive_getFileAttributeData };

FREERDP_ENTRY_POINT(UINT VCAPITYPE file_freerdp_drive_client_subsystem_entry(void* arg))
{
	const rdpDriveDriver** ppDriver = (const rdpDriveDriver**)arg;
	if (!ppDriver)
		return ERROR_INVALID_PARAMETER;

	*ppDriver = &s_driver;
	return CHANNEL_RC_OK;
}
