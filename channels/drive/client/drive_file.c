/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * File System Virtual Channel
 *
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2010-2011 Vic Lee
 * Copyright 2012 Gerald Richter
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2016 Inuvika Inc.
 * Copyright 2016 David PHAM-VAN <d.phamvan@inuvika.com>
 * Copyright 2017 Armin Novak <armin.novak@thincast.com>
 * Copyright 2017 Thincast Technologies GmbH
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/file.h>
#include <winpr/stream.h>

#include <freerdp/channels/rdpdr.h>

#include "drive_file.h"

#ifdef WITH_DEBUG_RDPDR
#define DEBUG_WSTR(msg, wstr)                              \
	do                                                     \
	{                                                      \
		char lpstr[1024] = { 0 };                          \
		ConvertWCharToUtf8(wstr, lpstr, ARRAYSIZE(lpstr)); \
		WLog_DBG(TAG, msg, lpstr);                         \
	} while (0)
#else
#define DEBUG_WSTR(msg, wstr) \
	do                        \
	{                         \
	} while (0)
#endif

static BOOL drive_file_fix_path(WCHAR* path, size_t length)
{
	size_t i;

	if ((length == 0) || (length > UINT32_MAX))
		return FALSE;

	WINPR_ASSERT(path);

	for (i = 0; i < length; i++)
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

static WCHAR* drive_file_combine_fullpath(const WCHAR* base_path, const WCHAR* path,
                                          size_t PathWCharLength)
{
	BOOL ok = FALSE;
	WCHAR* fullpath = NULL;
	size_t length;

	if (!base_path || (!path && (PathWCharLength > 0)))
		goto fail;

	const size_t base_path_length = _wcsnlen(base_path, MAX_PATH);
	length = base_path_length + PathWCharLength + 1;
	fullpath = (WCHAR*)calloc(length, sizeof(WCHAR));

	if (!fullpath)
		goto fail;

	CopyMemory(fullpath, base_path, base_path_length * sizeof(WCHAR));
	if (path)
		CopyMemory(&fullpath[base_path_length], path, PathWCharLength * sizeof(WCHAR));

	if (!drive_file_fix_path(fullpath, length))
		goto fail;

	/* Ensure the path does not contain sequences like '..' */
	const WCHAR dotdot[] = { '.', '.', '\0' };
	if (_wcsstr(&fullpath[base_path_length], dotdot))
	{
		char abuffer[MAX_PATH] = { 0 };
		ConvertWCharToUtf8(&fullpath[base_path_length], abuffer, ARRAYSIZE(abuffer));

		WLog_WARN(TAG, "[rdpdr] received invalid file path '%s' from server, aborting!",
		          &abuffer[base_path_length]);
		goto fail;
	}

	ok = TRUE;
fail:
	if (!ok)
	{
		free(fullpath);
		fullpath = NULL;
	}
	return fullpath;
}

static BOOL drive_file_remove_dir(const WCHAR* path)
{
	WIN32_FIND_DATAW findFileData = { 0 };
	BOOL ret = TRUE;
	HANDLE dir = INVALID_HANDLE_VALUE;
	WCHAR* fullpath = NULL;
	WCHAR* path_slash = NULL;
	size_t base_path_length = 0;

	if (!path)
		return FALSE;

	base_path_length = _wcslen(path);
	path_slash = (WCHAR*)calloc(base_path_length + 3, sizeof(WCHAR));

	if (!path_slash)
	{
		WLog_ERR(TAG, "malloc failed!");
		return FALSE;
	}

	CopyMemory(path_slash, path, base_path_length * sizeof(WCHAR));
	path_slash[base_path_length] = L'/';
	path_slash[base_path_length + 1] = L'*';
	DEBUG_WSTR("Search in %s", path_slash);
	dir = FindFirstFileW(path_slash, &findFileData);

	if (dir == INVALID_HANDLE_VALUE)
	{
		free(path_slash);
		return FALSE;
	}

	do
	{
		const size_t len = _wcsnlen(findFileData.cFileName, ARRAYSIZE(findFileData.cFileName));

		if ((len == 1 && findFileData.cFileName[0] == L'.') ||
		    (len == 2 && findFileData.cFileName[0] == L'.' && findFileData.cFileName[1] == L'.'))
		{
			continue;
		}

		fullpath = drive_file_combine_fullpath(path_slash, findFileData.cFileName, len);
		DEBUG_WSTR("Delete %s", fullpath);

		if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			ret = drive_file_remove_dir(fullpath);
		}
		else
		{
			ret = DeleteFileW(fullpath);
		}

		free(fullpath);

		if (!ret)
			break;
	} while (ret && FindNextFileW(dir, &findFileData) != 0);

	FindClose(dir);

	if (ret)
	{
		if (!RemoveDirectoryW(path))
		{
			ret = FALSE;
		}
	}

	free(path_slash);
	return ret;
}

static BOOL drive_file_set_fullpath(DRIVE_FILE* file, WCHAR* fullpath)
{
	if (!file || !fullpath)
		return FALSE;

	free(file->fullpath);
	file->fullpath = fullpath;
	file->filename = _wcsrchr(file->fullpath, L'/');

	if (file->filename == NULL)
		file->filename = file->fullpath;
	else
		file->filename += 1;

	return TRUE;
}

static BOOL drive_file_init(DRIVE_FILE* file)
{
	UINT CreateDisposition = 0;
	DWORD dwAttr = GetFileAttributesW(file->fullpath);

	if (dwAttr != INVALID_FILE_ATTRIBUTES)
	{
		/* The file exists */
		file->is_dir = (dwAttr & FILE_ATTRIBUTE_DIRECTORY) != 0;

		if (file->is_dir)
		{
			if (file->CreateDisposition == FILE_CREATE)
			{
				SetLastError(ERROR_ALREADY_EXISTS);
				return FALSE;
			}

			if (file->CreateOptions & FILE_NON_DIRECTORY_FILE)
			{
				SetLastError(ERROR_ACCESS_DENIED);
				return FALSE;
			}

			return TRUE;
		}
		else
		{
			if (file->CreateOptions & FILE_DIRECTORY_FILE)
			{
				SetLastError(ERROR_DIRECTORY);
				return FALSE;
			}
		}
	}
	else
	{
		file->is_dir = ((file->CreateOptions & FILE_DIRECTORY_FILE) ? TRUE : FALSE);

		if (file->is_dir)
		{
			/* Should only create the directory if the disposition allows for it */
			if ((file->CreateDisposition == FILE_OPEN_IF) ||
			    (file->CreateDisposition == FILE_CREATE))
			{
				if (CreateDirectoryW(file->fullpath, NULL) != 0)
				{
					return TRUE;
				}
			}

			SetLastError(ERROR_FILE_NOT_FOUND);
			return FALSE;
		}
	}

	if (file->file_handle == INVALID_HANDLE_VALUE)
	{
		switch (file->CreateDisposition)
		{
			case FILE_SUPERSEDE: /* If the file already exists, replace it with the given file. If
			                        it does not, create the given file. */
				CreateDisposition = CREATE_ALWAYS;
				break;

			case FILE_OPEN: /* If the file already exists, open it instead of creating a new file.
			                   If it does not, fail the request and do not create a new file. */
				CreateDisposition = OPEN_EXISTING;
				break;

			case FILE_CREATE: /* If the file already exists, fail the request and do not create or
			                     open the given file. If it does not, create the given file. */
				CreateDisposition = CREATE_NEW;
				break;

			case FILE_OPEN_IF: /* If the file already exists, open it. If it does not, create the
			                      given file. */
				CreateDisposition = OPEN_ALWAYS;
				break;

			case FILE_OVERWRITE: /* If the file already exists, open it and overwrite it. If it does
			                        not, fail the request. */
				CreateDisposition = TRUNCATE_EXISTING;
				break;

			case FILE_OVERWRITE_IF: /* If the file already exists, open it and overwrite it. If it
			                           does not, create the given file. */
				CreateDisposition = CREATE_ALWAYS;
				break;

			default:
				break;
		}

#ifndef WIN32
		file->SharedAccess = 0;
#endif
		file->file_handle = CreateFileW(file->fullpath, file->DesiredAccess, file->SharedAccess,
		                                NULL, CreateDisposition, file->FileAttributes, NULL);
	}

#ifdef WIN32
	if (file->file_handle == INVALID_HANDLE_VALUE)
	{
		/* Get the error message, if any. */
		DWORD errorMessageID = GetLastError();

		if (errorMessageID != 0)
		{
			LPSTR messageBuffer = NULL;
			size_t size =
			    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
			                       FORMAT_MESSAGE_IGNORE_INSERTS,
			                   NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			                   (LPSTR)&messageBuffer, 0, NULL);
			WLog_ERR(TAG, "Error in drive_file_init: %s %s", messageBuffer, file->fullpath);
			/* Free the buffer. */
			LocalFree(messageBuffer);
			/* restore original error code */
			SetLastError(errorMessageID);
		}
	}
#endif

	return file->file_handle != INVALID_HANDLE_VALUE;
}

DRIVE_FILE* drive_file_new(const WCHAR* base_path, const WCHAR* path, UINT32 PathWCharLength,
                           UINT32 id, UINT32 DesiredAccess, UINT32 CreateDisposition,
                           UINT32 CreateOptions, UINT32 FileAttributes, UINT32 SharedAccess)
{
	DRIVE_FILE* file;

	if (!base_path || (!path && (PathWCharLength > 0)))
		return NULL;

	file = (DRIVE_FILE*)calloc(1, sizeof(DRIVE_FILE));

	if (!file)
	{
		WLog_ERR(TAG, "calloc failed!");
		return NULL;
	}

	file->file_handle = INVALID_HANDLE_VALUE;
	file->find_handle = INVALID_HANDLE_VALUE;
	file->id = id;
	file->basepath = base_path;
	file->FileAttributes = FileAttributes;
	file->DesiredAccess = DesiredAccess;
	file->CreateDisposition = CreateDisposition;
	file->CreateOptions = CreateOptions;
	file->SharedAccess = SharedAccess;
	drive_file_set_fullpath(file, drive_file_combine_fullpath(base_path, path, PathWCharLength));

	if (!drive_file_init(file))
	{
		DWORD lastError = GetLastError();
		drive_file_free(file);
		SetLastError(lastError);
		return NULL;
	}

	return file;
}

BOOL drive_file_free(DRIVE_FILE* file)
{
	BOOL rc = FALSE;

	if (!file)
		return FALSE;

	if (file->file_handle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(file->file_handle);
		file->file_handle = INVALID_HANDLE_VALUE;
	}

	if (file->find_handle != INVALID_HANDLE_VALUE)
	{
		FindClose(file->find_handle);
		file->find_handle = INVALID_HANDLE_VALUE;
	}

	if (file->delete_pending)
	{
		if (file->is_dir)
		{
			if (!drive_file_remove_dir(file->fullpath))
				goto fail;
		}
		else if (!DeleteFileW(file->fullpath))
			goto fail;
	}

	rc = TRUE;
fail:
	DEBUG_WSTR("Free %s", file->fullpath);
	free(file->fullpath);
	free(file);
	return rc;
}

BOOL drive_file_seek(DRIVE_FILE* file, UINT64 Offset)
{
	LARGE_INTEGER loffset;

	if (!file)
		return FALSE;

	if (Offset > INT64_MAX)
		return FALSE;

	loffset.QuadPart = (LONGLONG)Offset;
	return SetFilePointerEx(file->file_handle, loffset, NULL, FILE_BEGIN);
}

BOOL drive_file_read(DRIVE_FILE* file, BYTE* buffer, UINT32* Length)
{
	DWORD read;

	if (!file || !buffer || !Length)
		return FALSE;

	DEBUG_WSTR("Read file %s", file->fullpath);

	if (ReadFile(file->file_handle, buffer, *Length, &read, NULL))
	{
		*Length = read;
		return TRUE;
	}

	return FALSE;
}

BOOL drive_file_write(DRIVE_FILE* file, BYTE* buffer, UINT32 Length)
{
	DWORD written;

	if (!file || !buffer)
		return FALSE;

	DEBUG_WSTR("Write file %s", file->fullpath);

	while (Length > 0)
	{
		if (!WriteFile(file->file_handle, buffer, Length, &written, NULL))
			return FALSE;

		Length -= written;
		buffer += written;
	}

	return TRUE;
}

static BOOL drive_file_query_from_handle_information(const DRIVE_FILE* file,
                                                     const BY_HANDLE_FILE_INFORMATION* info,
                                                     UINT32 FsInformationClass, wStream* output)
{
	switch (FsInformationClass)
	{
		case FileBasicInformation:

			/* http://msdn.microsoft.com/en-us/library/cc232094.aspx */
			if (!Stream_EnsureRemainingCapacity(output, 4 + 36))
				return FALSE;

			Stream_Write_UINT32(output, 36);                                    /* Length */
			Stream_Write_UINT32(output, info->ftCreationTime.dwLowDateTime);    /* CreationTime */
			Stream_Write_UINT32(output, info->ftCreationTime.dwHighDateTime);   /* CreationTime */
			Stream_Write_UINT32(output, info->ftLastAccessTime.dwLowDateTime);  /* LastAccessTime */
			Stream_Write_UINT32(output, info->ftLastAccessTime.dwHighDateTime); /* LastAccessTime */
			Stream_Write_UINT32(output, info->ftLastWriteTime.dwLowDateTime);   /* LastWriteTime */
			Stream_Write_UINT32(output, info->ftLastWriteTime.dwHighDateTime);  /* LastWriteTime */
			Stream_Write_UINT32(output, info->ftLastWriteTime.dwLowDateTime);   /* ChangeTime */
			Stream_Write_UINT32(output, info->ftLastWriteTime.dwHighDateTime);  /* ChangeTime */
			Stream_Write_UINT32(output, info->dwFileAttributes);                /* FileAttributes */
			/* Reserved(4), MUST NOT be added! */
			break;

		case FileStandardInformation:

			/*  http://msdn.microsoft.com/en-us/library/cc232088.aspx */
			if (!Stream_EnsureRemainingCapacity(output, 4 + 22))
				return FALSE;

			Stream_Write_UINT32(output, 22);                          /* Length */
			Stream_Write_UINT32(output, info->nFileSizeLow);          /* AllocationSize */
			Stream_Write_UINT32(output, info->nFileSizeHigh);         /* AllocationSize */
			Stream_Write_UINT32(output, info->nFileSizeLow);          /* EndOfFile */
			Stream_Write_UINT32(output, info->nFileSizeHigh);         /* EndOfFile */
			Stream_Write_UINT32(output, info->nNumberOfLinks);        /* NumberOfLinks */
			Stream_Write_UINT8(output, file->delete_pending ? 1 : 0); /* DeletePending */
			Stream_Write_UINT8(output, info->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY
			                               ? TRUE
			                               : FALSE); /* Directory */
			/* Reserved(2), MUST NOT be added! */
			break;

		case FileAttributeTagInformation:

			/* http://msdn.microsoft.com/en-us/library/cc232093.aspx */
			if (!Stream_EnsureRemainingCapacity(output, 4 + 8))
				return FALSE;

			Stream_Write_UINT32(output, 8);                      /* Length */
			Stream_Write_UINT32(output, info->dwFileAttributes); /* FileAttributes */
			Stream_Write_UINT32(output, 0);                      /* ReparseTag */
			break;

		default:
			/* Unhandled FsInformationClass */
			return FALSE;
	}

	return TRUE;
}

static BOOL drive_file_query_from_attributes(const DRIVE_FILE* file,
                                             const WIN32_FILE_ATTRIBUTE_DATA* attrib,
                                             UINT32 FsInformationClass, wStream* output)
{
	switch (FsInformationClass)
	{
		case FileBasicInformation:

			/* http://msdn.microsoft.com/en-us/library/cc232094.aspx */
			if (!Stream_EnsureRemainingCapacity(output, 4 + 36))
				return FALSE;

			Stream_Write_UINT32(output, 36);                                    /* Length */
			Stream_Write_UINT32(output, attrib->ftCreationTime.dwLowDateTime);  /* CreationTime */
			Stream_Write_UINT32(output, attrib->ftCreationTime.dwHighDateTime); /* CreationTime */
			Stream_Write_UINT32(output,
			                    attrib->ftLastAccessTime.dwLowDateTime); /* LastAccessTime */
			Stream_Write_UINT32(output,
			                    attrib->ftLastAccessTime.dwHighDateTime);       /* LastAccessTime */
			Stream_Write_UINT32(output, attrib->ftLastWriteTime.dwLowDateTime); /* LastWriteTime */
			Stream_Write_UINT32(output, attrib->ftLastWriteTime.dwHighDateTime); /* LastWriteTime */
			Stream_Write_UINT32(output, attrib->ftLastWriteTime.dwLowDateTime);  /* ChangeTime */
			Stream_Write_UINT32(output, attrib->ftLastWriteTime.dwHighDateTime); /* ChangeTime */
			Stream_Write_UINT32(output, attrib->dwFileAttributes); /* FileAttributes */
			/* Reserved(4), MUST NOT be added! */
			break;

		case FileStandardInformation:

			/*  http://msdn.microsoft.com/en-us/library/cc232088.aspx */
			if (!Stream_EnsureRemainingCapacity(output, 4 + 22))
				return FALSE;

			Stream_Write_UINT32(output, 22);                          /* Length */
			Stream_Write_UINT32(output, attrib->nFileSizeLow);        /* AllocationSize */
			Stream_Write_UINT32(output, attrib->nFileSizeHigh);       /* AllocationSize */
			Stream_Write_UINT32(output, attrib->nFileSizeLow);        /* EndOfFile */
			Stream_Write_UINT32(output, attrib->nFileSizeHigh);       /* EndOfFile */
			Stream_Write_UINT32(output, 0);                           /* NumberOfLinks */
			Stream_Write_UINT8(output, file->delete_pending ? 1 : 0); /* DeletePending */
			Stream_Write_UINT8(output, attrib->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY
			                               ? TRUE
			                               : FALSE); /* Directory */
			/* Reserved(2), MUST NOT be added! */
			break;

		case FileAttributeTagInformation:

			/* http://msdn.microsoft.com/en-us/library/cc232093.aspx */
			if (!Stream_EnsureRemainingCapacity(output, 4 + 8))
				return FALSE;

			Stream_Write_UINT32(output, 8);                        /* Length */
			Stream_Write_UINT32(output, attrib->dwFileAttributes); /* FileAttributes */
			Stream_Write_UINT32(output, 0);                        /* ReparseTag */
			break;

		default:
			/* Unhandled FsInformationClass */
			return FALSE;
	}

	return TRUE;
}

BOOL drive_file_query_information(DRIVE_FILE* file, UINT32 FsInformationClass, wStream* output)
{
	BY_HANDLE_FILE_INFORMATION fileInformation = { 0 };
	BOOL status;
	HANDLE hFile;

	if (!file || !output)
		return FALSE;

	hFile = CreateFileW(file->fullpath, 0, FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
	                    FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		status = GetFileInformationByHandle(hFile, &fileInformation);
		CloseHandle(hFile);
		if (!status)
			goto out_fail;

		if (!drive_file_query_from_handle_information(file, &fileInformation, FsInformationClass,
		                                              output))
			goto out_fail;

		return TRUE;
	}

	/* If we failed before (i.e. if information for a drive is queried) fall back to
	 * GetFileAttributesExW */
	WIN32_FILE_ATTRIBUTE_DATA fileAttributes = { 0 };
	if (!GetFileAttributesExW(file->fullpath, GetFileExInfoStandard, &fileAttributes))
		goto out_fail;

	if (!drive_file_query_from_attributes(file, &fileAttributes, FsInformationClass, output))
		goto out_fail;

	return TRUE;
out_fail:
	Stream_Write_UINT32(output, 0); /* Length */
	return FALSE;
}

BOOL drive_file_set_information(DRIVE_FILE* file, UINT32 FsInformationClass, UINT32 Length,
                                wStream* input)
{
	INT64 size;
	WCHAR* fullpath;
	ULARGE_INTEGER liCreationTime;
	ULARGE_INTEGER liLastAccessTime;
	ULARGE_INTEGER liLastWriteTime;
	ULARGE_INTEGER liChangeTime;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	FILETIME* pftCreationTime = NULL;
	FILETIME* pftLastAccessTime = NULL;
	FILETIME* pftLastWriteTime = NULL;
	UINT32 FileAttributes;
	UINT32 FileNameLength;
	LARGE_INTEGER liSize;
	UINT8 delete_pending;
	UINT8 ReplaceIfExists;
	DWORD attr;

	if (!file || !input)
		return FALSE;

	switch (FsInformationClass)
	{
		case FileBasicInformation:
			if (!Stream_CheckAndLogRequiredLength(TAG, input, 36))
				return FALSE;

			/* http://msdn.microsoft.com/en-us/library/cc232094.aspx */
			Stream_Read_UINT64(input, liCreationTime.QuadPart);
			Stream_Read_UINT64(input, liLastAccessTime.QuadPart);
			Stream_Read_UINT64(input, liLastWriteTime.QuadPart);
			Stream_Read_UINT64(input, liChangeTime.QuadPart);
			Stream_Read_UINT32(input, FileAttributes);

			if (!PathFileExistsW(file->fullpath))
				return FALSE;

			if (file->file_handle == INVALID_HANDLE_VALUE)
			{
				WLog_ERR(TAG, "Unable to set file time %s (%" PRId32 ")", file->fullpath,
				         GetLastError());
				return FALSE;
			}

			if (liCreationTime.QuadPart != 0)
			{
				ftCreationTime.dwHighDateTime = liCreationTime.u.HighPart;
				ftCreationTime.dwLowDateTime = liCreationTime.u.LowPart;
				pftCreationTime = &ftCreationTime;
			}

			if (liLastAccessTime.QuadPart != 0)
			{
				ftLastAccessTime.dwHighDateTime = liLastAccessTime.u.HighPart;
				ftLastAccessTime.dwLowDateTime = liLastAccessTime.u.LowPart;
				pftLastAccessTime = &ftLastAccessTime;
			}

			if (liLastWriteTime.QuadPart != 0)
			{
				ftLastWriteTime.dwHighDateTime = liLastWriteTime.u.HighPart;
				ftLastWriteTime.dwLowDateTime = liLastWriteTime.u.LowPart;
				pftLastWriteTime = &ftLastWriteTime;
			}

			if (liChangeTime.QuadPart != 0 && liChangeTime.QuadPart > liLastWriteTime.QuadPart)
			{
				ftLastWriteTime.dwHighDateTime = liChangeTime.u.HighPart;
				ftLastWriteTime.dwLowDateTime = liChangeTime.u.LowPart;
				pftLastWriteTime = &ftLastWriteTime;
			}

			DEBUG_WSTR("SetFileTime %s", file->fullpath);

			SetFileAttributesW(file->fullpath, FileAttributes);
			if (!SetFileTime(file->file_handle, pftCreationTime, pftLastAccessTime,
			                 pftLastWriteTime))
			{
				WLog_ERR(TAG, "Unable to set file time to %s", file->fullpath);
				return FALSE;
			}

			break;

		case FileEndOfFileInformation:

		/* http://msdn.microsoft.com/en-us/library/cc232067.aspx */
		case FileAllocationInformation:
			if (!Stream_CheckAndLogRequiredLength(TAG, input, 8))
				return FALSE;

			/* http://msdn.microsoft.com/en-us/library/cc232076.aspx */
			Stream_Read_INT64(input, size);

			if (file->file_handle == INVALID_HANDLE_VALUE)
			{
				WLog_ERR(TAG, "Unable to truncate %s to %" PRId64 " (%" PRId32 ")", file->fullpath,
				         size, GetLastError());
				return FALSE;
			}

			liSize.QuadPart = size;

			if (!SetFilePointerEx(file->file_handle, liSize, NULL, FILE_BEGIN))
			{
				WLog_ERR(TAG, "Unable to truncate %s to %" PRId64 " (%" PRId32 ")", file->fullpath,
				         size, GetLastError());
				return FALSE;
			}

			DEBUG_WSTR("Truncate %s", file->fullpath);

			if (SetEndOfFile(file->file_handle) == 0)
			{
				WLog_ERR(TAG, "Unable to truncate %s to %" PRId64 " (%" PRId32 ")", file->fullpath,
				         size, GetLastError());
				return FALSE;
			}

			break;

		case FileDispositionInformation:

			/* http://msdn.microsoft.com/en-us/library/cc232098.aspx */
			/* http://msdn.microsoft.com/en-us/library/cc241371.aspx */
			if (file->is_dir && !PathIsDirectoryEmptyW(file->fullpath))
				break; /* TODO: SetLastError ??? */

			if (Length)
			{
				if (!Stream_CheckAndLogRequiredLength(TAG, input, 1))
					return FALSE;

				Stream_Read_UINT8(input, delete_pending);
			}
			else
				delete_pending = 1;

			if (delete_pending)
			{
				DEBUG_WSTR("SetDeletePending %s", file->fullpath);
				attr = GetFileAttributesW(file->fullpath);

				if (attr & FILE_ATTRIBUTE_READONLY)
				{
					SetLastError(ERROR_ACCESS_DENIED);
					return FALSE;
				}
			}

			file->delete_pending = delete_pending;
			break;

		case FileRenameInformation:
			if (!Stream_CheckAndLogRequiredLength(TAG, input, 6))
				return FALSE;

			/* http://msdn.microsoft.com/en-us/library/cc232085.aspx */
			Stream_Read_UINT8(input, ReplaceIfExists);
			Stream_Seek_UINT8(input); /* RootDirectory */
			Stream_Read_UINT32(input, FileNameLength);

			if (!Stream_CheckAndLogRequiredLength(TAG, input, FileNameLength))
				return FALSE;

			fullpath = drive_file_combine_fullpath(file->basepath, (WCHAR*)Stream_Pointer(input),
			                                       FileNameLength / sizeof(WCHAR));

			if (!fullpath)
				return FALSE;

#ifdef _WIN32

			if (file->file_handle != INVALID_HANDLE_VALUE)
			{
				CloseHandle(file->file_handle);
				file->file_handle = INVALID_HANDLE_VALUE;
			}

#endif
			DEBUG_WSTR("MoveFileExW %s", file->fullpath);

			if (MoveFileExW(file->fullpath, fullpath,
			                MOVEFILE_COPY_ALLOWED |
			                    (ReplaceIfExists ? MOVEFILE_REPLACE_EXISTING : 0)))
			{
				if (!drive_file_set_fullpath(file, fullpath))
					return FALSE;
			}
			else
			{
				free(fullpath);
				return FALSE;
			}

#ifdef _WIN32
			drive_file_init(file);
#endif
			break;

		default:
			return FALSE;
	}

	return TRUE;
}

BOOL drive_file_query_directory(DRIVE_FILE* file, UINT32 FsInformationClass, BYTE InitialQuery,
                                const WCHAR* path, UINT32 PathWCharLength, wStream* output)
{
	size_t length;
	WCHAR* ent_path;

	if (!file || !path || !output)
		return FALSE;

	if (InitialQuery != 0)
	{
		/* release search handle */
		if (file->find_handle != INVALID_HANDLE_VALUE)
			FindClose(file->find_handle);

		ent_path = drive_file_combine_fullpath(file->basepath, path, PathWCharLength);
		/* open new search handle and retrieve the first entry */
		file->find_handle = FindFirstFileW(ent_path, &file->find_data);
		free(ent_path);

		if (file->find_handle == INVALID_HANDLE_VALUE)
			goto out_fail;
	}
	else if (!FindNextFileW(file->find_handle, &file->find_data))
		goto out_fail;

	length = _wcslen(file->find_data.cFileName) * 2;

	switch (FsInformationClass)
	{
		case FileDirectoryInformation:

			/* http://msdn.microsoft.com/en-us/library/cc232097.aspx */
			if (!Stream_EnsureRemainingCapacity(output, 4 + 64 + length))
				goto out_fail;

			if (length > UINT32_MAX - 64)
				goto out_fail;

			Stream_Write_UINT32(output, (UINT32)(64 + length)); /* Length */
			Stream_Write_UINT32(output, 0);                     /* NextEntryOffset */
			Stream_Write_UINT32(output, 0);                     /* FileIndex */
			Stream_Write_UINT32(output,
			                    file->find_data.ftCreationTime.dwLowDateTime); /* CreationTime */
			Stream_Write_UINT32(output,
			                    file->find_data.ftCreationTime.dwHighDateTime); /* CreationTime */
			Stream_Write_UINT32(
			    output, file->find_data.ftLastAccessTime.dwLowDateTime); /* LastAccessTime */
			Stream_Write_UINT32(
			    output, file->find_data.ftLastAccessTime.dwHighDateTime); /* LastAccessTime */
			Stream_Write_UINT32(output,
			                    file->find_data.ftLastWriteTime.dwLowDateTime); /* LastWriteTime */
			Stream_Write_UINT32(output,
			                    file->find_data.ftLastWriteTime.dwHighDateTime); /* LastWriteTime */
			Stream_Write_UINT32(output,
			                    file->find_data.ftLastWriteTime.dwLowDateTime); /* ChangeTime */
			Stream_Write_UINT32(output,
			                    file->find_data.ftLastWriteTime.dwHighDateTime); /* ChangeTime */
			Stream_Write_UINT32(output, file->find_data.nFileSizeLow);           /* EndOfFile */
			Stream_Write_UINT32(output, file->find_data.nFileSizeHigh);          /* EndOfFile */
			Stream_Write_UINT32(output, file->find_data.nFileSizeLow);     /* AllocationSize */
			Stream_Write_UINT32(output, file->find_data.nFileSizeHigh);    /* AllocationSize */
			Stream_Write_UINT32(output, file->find_data.dwFileAttributes); /* FileAttributes */
			Stream_Write_UINT32(output, (UINT32)length);                   /* FileNameLength */
			Stream_Write(output, file->find_data.cFileName, length);
			break;

		case FileFullDirectoryInformation:

			/* http://msdn.microsoft.com/en-us/library/cc232068.aspx */
			if (!Stream_EnsureRemainingCapacity(output, 4 + 68 + length))
				goto out_fail;

			if (length > UINT32_MAX - 68)
				goto out_fail;

			Stream_Write_UINT32(output, (UINT32)(68 + length)); /* Length */
			Stream_Write_UINT32(output, 0);                     /* NextEntryOffset */
			Stream_Write_UINT32(output, 0);                     /* FileIndex */
			Stream_Write_UINT32(output,
			                    file->find_data.ftCreationTime.dwLowDateTime); /* CreationTime */
			Stream_Write_UINT32(output,
			                    file->find_data.ftCreationTime.dwHighDateTime); /* CreationTime */
			Stream_Write_UINT32(
			    output, file->find_data.ftLastAccessTime.dwLowDateTime); /* LastAccessTime */
			Stream_Write_UINT32(
			    output, file->find_data.ftLastAccessTime.dwHighDateTime); /* LastAccessTime */
			Stream_Write_UINT32(output,
			                    file->find_data.ftLastWriteTime.dwLowDateTime); /* LastWriteTime */
			Stream_Write_UINT32(output,
			                    file->find_data.ftLastWriteTime.dwHighDateTime); /* LastWriteTime */
			Stream_Write_UINT32(output,
			                    file->find_data.ftLastWriteTime.dwLowDateTime); /* ChangeTime */
			Stream_Write_UINT32(output,
			                    file->find_data.ftLastWriteTime.dwHighDateTime); /* ChangeTime */
			Stream_Write_UINT32(output, file->find_data.nFileSizeLow);           /* EndOfFile */
			Stream_Write_UINT32(output, file->find_data.nFileSizeHigh);          /* EndOfFile */
			Stream_Write_UINT32(output, file->find_data.nFileSizeLow);     /* AllocationSize */
			Stream_Write_UINT32(output, file->find_data.nFileSizeHigh);    /* AllocationSize */
			Stream_Write_UINT32(output, file->find_data.dwFileAttributes); /* FileAttributes */
			Stream_Write_UINT32(output, (UINT32)length);                   /* FileNameLength */
			Stream_Write_UINT32(output, 0);                                /* EaSize */
			Stream_Write(output, file->find_data.cFileName, length);
			break;

		case FileBothDirectoryInformation:

			/* http://msdn.microsoft.com/en-us/library/cc232095.aspx */
			if (!Stream_EnsureRemainingCapacity(output, 4 + 93 + length))
				goto out_fail;

			if (length > UINT32_MAX - 93)
				goto out_fail;

			Stream_Write_UINT32(output, (UINT32)(93 + length)); /* Length */
			Stream_Write_UINT32(output, 0);                     /* NextEntryOffset */
			Stream_Write_UINT32(output, 0);                     /* FileIndex */
			Stream_Write_UINT32(output,
			                    file->find_data.ftCreationTime.dwLowDateTime); /* CreationTime */
			Stream_Write_UINT32(output,
			                    file->find_data.ftCreationTime.dwHighDateTime); /* CreationTime */
			Stream_Write_UINT32(
			    output, file->find_data.ftLastAccessTime.dwLowDateTime); /* LastAccessTime */
			Stream_Write_UINT32(
			    output, file->find_data.ftLastAccessTime.dwHighDateTime); /* LastAccessTime */
			Stream_Write_UINT32(output,
			                    file->find_data.ftLastWriteTime.dwLowDateTime); /* LastWriteTime */
			Stream_Write_UINT32(output,
			                    file->find_data.ftLastWriteTime.dwHighDateTime); /* LastWriteTime */
			Stream_Write_UINT32(output,
			                    file->find_data.ftLastWriteTime.dwLowDateTime); /* ChangeTime */
			Stream_Write_UINT32(output,
			                    file->find_data.ftLastWriteTime.dwHighDateTime); /* ChangeTime */
			Stream_Write_UINT32(output, file->find_data.nFileSizeLow);           /* EndOfFile */
			Stream_Write_UINT32(output, file->find_data.nFileSizeHigh);          /* EndOfFile */
			Stream_Write_UINT32(output, file->find_data.nFileSizeLow);     /* AllocationSize */
			Stream_Write_UINT32(output, file->find_data.nFileSizeHigh);    /* AllocationSize */
			Stream_Write_UINT32(output, file->find_data.dwFileAttributes); /* FileAttributes */
			Stream_Write_UINT32(output, (UINT32)length);                   /* FileNameLength */
			Stream_Write_UINT32(output, 0);                                /* EaSize */
			Stream_Write_UINT8(output, 0);                                 /* ShortNameLength */
			/* Reserved(1), MUST NOT be added! */
			Stream_Zero(output, 24); /* ShortName */
			Stream_Write(output, file->find_data.cFileName, length);
			break;

		case FileNamesInformation:

			/* http://msdn.microsoft.com/en-us/library/cc232077.aspx */
			if (!Stream_EnsureRemainingCapacity(output, 4 + 12 + length))
				goto out_fail;

			if (length > UINT32_MAX - 12)
				goto out_fail;

			Stream_Write_UINT32(output, (UINT32)(12 + length)); /* Length */
			Stream_Write_UINT32(output, 0);                     /* NextEntryOffset */
			Stream_Write_UINT32(output, 0);                     /* FileIndex */
			Stream_Write_UINT32(output, (UINT32)length);        /* FileNameLength */
			Stream_Write(output, file->find_data.cFileName, length);
			break;

		default:
			WLog_ERR(TAG, "unhandled FsInformationClass %" PRIu32, FsInformationClass);
			/* Unhandled FsInformationClass */
			goto out_fail;
	}

	return TRUE;
out_fail:
	Stream_Write_UINT32(output, 0); /* Length */
	Stream_Write_UINT8(output, 0);  /* Padding */
	return FALSE;
}
