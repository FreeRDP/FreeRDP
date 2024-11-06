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
#include <winpr/string.h>
#include <winpr/stream.h>

#include <freerdp/channels/rdpdr.h>
#include <freerdp/client/drive.h>
#include <freerdp/addin.h>

#include "drive_file.h"

struct S_DRIVE_FILE
{
	UINT32 id;
	BOOL is_dir;
	BOOL delete_pending;
	UINT32 FileAttributes;
	UINT32 SharedAccess;
	UINT32 DesiredAccess;
	UINT32 CreateDisposition;
	UINT32 CreateOptions;
	const rdpDriveDriver* backend;
	rdpDriveContext* context;
};

#ifdef WITH_DEBUG_RDPDR
#define DEBUG_WSTR(msg, wstr)                                    \
	do                                                           \
	{                                                            \
		char lpstr[1024] = { 0 };                                \
		(void)ConvertWCharToUtf8(wstr, lpstr, ARRAYSIZE(lpstr)); \
		WLog_DBG(TAG, msg, lpstr);                               \
	} while (0)
#else
#define DEBUG_WSTR(msg, wstr) \
	do                        \
	{                         \
	} while (0)
#endif

static BOOL drive_file_init(DRIVE_FILE* file)
{
	UINT CreateDisposition = 0;

	WINPR_ASSERT(file);
	WINPR_ASSERT(file->context);
	WINPR_ASSERT(file->backend);
	WINPR_ASSERT(file->backend->getFileAttributes);
	const DWORD dwAttr = file->backend->getFileAttributes(file->context);

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
				WINPR_ASSERT(file->backend->createDirectory);
				if (file->backend->createDirectory(file->context) != 0)
					return TRUE;
			}

			SetLastError(ERROR_FILE_NOT_FOUND);
			return FALSE;
		}
	}

	WINPR_ASSERT(file->backend->exists);
	if (!file->backend->exists(file->context))
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

		if (!file->backend->createFile(file->context, file->DesiredAccess, file->SharedAccess,
		                               CreateDisposition, file->FileAttributes))
			return FALSE;
	}

	WINPR_ASSERT(file->backend->exists);
	return file->backend->exists(file->context);
}

DRIVE_FILE* drive_file_new(rdpContext* context, const rdpDriveDriver* backend,
                           const WCHAR* base_path, const WCHAR* path, UINT32 PathWCharLength,
                           UINT32 id, UINT32 DesiredAccess, UINT32 CreateDisposition,
                           UINT32 CreateOptions, UINT32 FileAttributes, UINT32 SharedAccess)
{
	WINPR_ASSERT(backend);
	if (!base_path || (!path && (PathWCharLength > 0)))
		return NULL;

	DRIVE_FILE* file = (DRIVE_FILE*)calloc(1, sizeof(DRIVE_FILE));

	if (!file)
		goto fail;

	file->backend = backend;

	WINPR_ASSERT(backend->new);
	file->context = backend->new(context);
	if (!file->context)
		goto fail;

	file->id = id;
	file->FileAttributes = FileAttributes;
	file->DesiredAccess = DesiredAccess;
	file->CreateDisposition = CreateDisposition;
	file->CreateOptions = CreateOptions;
	file->SharedAccess = SharedAccess;

	WINPR_ASSERT(file->backend);
	WINPR_ASSERT(file->context);
	WINPR_ASSERT(file->backend->setPath);
	if (!file->backend->setPath(file->context, base_path, path, PathWCharLength))
		goto fail;

	if (!drive_file_init(file))
		goto fail;

	return file;

fail:
	drive_file_free(file);
	return NULL;
}

BOOL drive_file_free(DRIVE_FILE* file)
{
	BOOL rc = FALSE;

	if (!file)
		return FALSE;

	WINPR_ASSERT(file->backend);
	if (file->delete_pending)
	{
		WINPR_ASSERT(file->backend->remove);
		if (!file->backend->remove(file->context))
			goto fail;
	}

	rc = TRUE;
fail:
	WINPR_ASSERT(file->backend->free);
	file->backend->free(file->context);

	free(file);
	return rc;
}

BOOL drive_file_seek(DRIVE_FILE* file, UINT64 Offset)
{
	if (!file)
		return FALSE;

	if (Offset > INT64_MAX)
		return FALSE;

	WINPR_ASSERT(file->context);
	WINPR_ASSERT(file->backend);
	WINPR_ASSERT(file->backend->seek);

	return file->backend->seek(file->context, (SSIZE_T)Offset, SEEK_SET) >= 0;
}

BOOL drive_file_read(DRIVE_FILE* file, BYTE* buffer, UINT32* Length)
{
	if (!file || !buffer || !Length)
		return FALSE;

	const size_t size = *Length;
	*Length = 0;

	WINPR_ASSERT(file->context);
	WINPR_ASSERT(file->backend);
	WINPR_ASSERT(file->backend->read);
	const SSIZE_T rc = file->backend->read(file->context, buffer, size);
	if (rc < 0)
		return FALSE;

	WINPR_ASSERT((size_t)rc <= size);
	*Length = (UINT32)rc;
	return TRUE;
}

BOOL drive_file_write(DRIVE_FILE* file, const BYTE* buffer, UINT32 Length)
{
	if (!file || !buffer)
		return FALSE;

	WINPR_ASSERT(file->context);
	WINPR_ASSERT(file->backend);
	WINPR_ASSERT(file->backend->write);
	while (Length > 0)
	{
		const SSIZE_T rc = file->backend->write(file->context, buffer, Length);
		if (rc < 0)
			return FALSE;

		Length -= (UINT32)rc;
		buffer += (size_t)rc;
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

BOOL drive_file_query_information(DRIVE_FILE* file, UINT32 FsInformationClass, wStream* output)
{
	if (!file || !output)
		return FALSE;

	WINPR_ASSERT(file->backend);
	WINPR_ASSERT(file->context);

	WINPR_ASSERT(file->backend->getFileAttributeData);
	const BY_HANDLE_FILE_INFORMATION* info = file->backend->getFileAttributeData(file->context);
	if (!info)
		goto out_fail;

	return drive_file_query_from_handle_information(file, info, FsInformationClass, output);

out_fail:
	Stream_Write_UINT32(output, 0); /* Length */
	return FALSE;
}

BOOL drive_file_set_information(DRIVE_FILE* file, UINT32 FsInformationClass, UINT32 Length,
                                wStream* input)
{
	INT64 size = 0;
	UINT32 FileNameLength = 0;
	UINT8 delete_pending = 0;
	UINT8 ReplaceIfExists = 0;

	if (!file || !input)
		return FALSE;

	WINPR_ASSERT(file->backend);
	WINPR_ASSERT(file->context);
	switch (FsInformationClass)
	{
		case FileBasicInformation:
		{
			if (!Stream_CheckAndLogRequiredLength(TAG, input, 36))
				return FALSE;

			/* http://msdn.microsoft.com/en-us/library/cc232094.aspx */
			const UINT64 CreationTime = Stream_Get_UINT64(input);
			const UINT64 LastAccessTime = Stream_Get_UINT64(input);
			const UINT64 LastWriteTime = Stream_Get_UINT64(input);
			const UINT64 ChangeTime = Stream_Get_UINT64(input);
			const UINT32 FileAttributes = Stream_Get_UINT32(input);

			WINPR_ASSERT(file->backend->exists);
			if (!file->backend->exists(file->context))
				return FALSE;

			WINPR_ASSERT(file->backend->setFileAttributesAndTimes);
			if (!file->backend->setFileAttributesAndTimes(file->context, CreationTime,
			                                              LastAccessTime, LastWriteTime, ChangeTime,
			                                              FileAttributes))
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

			WINPR_ASSERT(file->backend->setSize);
			if (!file->backend->setSize(file->context, size))
				return FALSE;

			break;

		case FileDispositionInformation:

			/* http://msdn.microsoft.com/en-us/library/cc232098.aspx */
			/* http://msdn.microsoft.com/en-us/library/cc241371.aspx */
			WINPR_ASSERT(file->backend->empty);
			if (file->is_dir && !file->backend->empty(file->context))
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
				const UINT32 attr = file->backend->getFileAttributes(file->context);

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

			const WCHAR* str = Stream_ConstPointer(input);
			const size_t len = FileNameLength / sizeof(WCHAR);
			WINPR_ASSERT(file->backend->move);
			if (!file->backend->move(file->context, str, len, ReplaceIfExists))
				return FALSE;
			break;

		default:
			return FALSE;
	}

	return TRUE;
}

BOOL drive_file_query_directory(DRIVE_FILE* file, UINT32 FsInformationClass, BYTE InitialQuery,
                                const WCHAR* path, UINT32 PathWCharLength, wStream* output)
{
	if (!file || !path || !output)
		return FALSE;

	WINPR_ASSERT(file->context);
	WINPR_ASSERT(file->backend);

	const WIN32_FIND_DATAW* find_data = NULL;
	if (InitialQuery != 0)
	{
		/* open new search handle and retrieve the first entry */
		WINPR_ASSERT(file->backend->first);
		find_data = file->backend->first(file->context, path, PathWCharLength);
	}
	else
	{
		WINPR_ASSERT(file->backend->next);
		find_data = file->backend->next(file->context);
	}

	if (!find_data)
		goto out_fail;

	const size_t length =
	    _wcsnlen(find_data->cFileName, ARRAYSIZE(find_data->cFileName)) * sizeof(WCHAR);

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
			Stream_Write_UINT32(output, find_data->ftCreationTime.dwLowDateTime); /* CreationTime */
			Stream_Write_UINT32(output,
			                    find_data->ftCreationTime.dwHighDateTime); /* CreationTime */
			Stream_Write_UINT32(output,
			                    find_data->ftLastAccessTime.dwLowDateTime); /* LastAccessTime */
			Stream_Write_UINT32(output,
			                    find_data->ftLastAccessTime.dwHighDateTime); /* LastAccessTime */
			Stream_Write_UINT32(output,
			                    find_data->ftLastWriteTime.dwLowDateTime); /* LastWriteTime */
			Stream_Write_UINT32(output,
			                    find_data->ftLastWriteTime.dwHighDateTime); /* LastWriteTime */
			Stream_Write_UINT32(output, find_data->ftLastWriteTime.dwLowDateTime);  /* ChangeTime */
			Stream_Write_UINT32(output, find_data->ftLastWriteTime.dwHighDateTime); /* ChangeTime */
			Stream_Write_UINT32(output, find_data->nFileSizeLow);                   /* EndOfFile */
			Stream_Write_UINT32(output, find_data->nFileSizeHigh);                  /* EndOfFile */
			Stream_Write_UINT32(output, find_data->nFileSizeLow);          /* AllocationSize */
			Stream_Write_UINT32(output, find_data->nFileSizeHigh);         /* AllocationSize */
			Stream_Write_UINT32(output, find_data->dwFileAttributes);      /* FileAttributes */
			Stream_Write_UINT32(output, (UINT32)length);                   /* FileNameLength */
			Stream_Write(output, find_data->cFileName, length);
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
			Stream_Write_UINT32(output, find_data->ftCreationTime.dwLowDateTime); /* CreationTime */
			Stream_Write_UINT32(output,
			                    find_data->ftCreationTime.dwHighDateTime); /* CreationTime */
			Stream_Write_UINT32(output,
			                    find_data->ftLastAccessTime.dwLowDateTime); /* LastAccessTime */
			Stream_Write_UINT32(output,
			                    find_data->ftLastAccessTime.dwHighDateTime); /* LastAccessTime */
			Stream_Write_UINT32(output,
			                    find_data->ftLastWriteTime.dwLowDateTime); /* LastWriteTime */
			Stream_Write_UINT32(output,
			                    find_data->ftLastWriteTime.dwHighDateTime); /* LastWriteTime */
			Stream_Write_UINT32(output, find_data->ftLastWriteTime.dwLowDateTime);  /* ChangeTime */
			Stream_Write_UINT32(output, find_data->ftLastWriteTime.dwHighDateTime); /* ChangeTime */
			Stream_Write_UINT32(output, find_data->nFileSizeLow);                   /* EndOfFile */
			Stream_Write_UINT32(output, find_data->nFileSizeHigh);                  /* EndOfFile */
			Stream_Write_UINT32(output, find_data->nFileSizeLow);          /* AllocationSize */
			Stream_Write_UINT32(output, find_data->nFileSizeHigh);         /* AllocationSize */
			Stream_Write_UINT32(output, find_data->dwFileAttributes);      /* FileAttributes */
			Stream_Write_UINT32(output, (UINT32)length);                   /* FileNameLength */
			Stream_Write_UINT32(output, 0);                                /* EaSize */
			Stream_Write(output, find_data->cFileName, length);
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
			Stream_Write_UINT32(output, find_data->ftCreationTime.dwLowDateTime); /* CreationTime */
			Stream_Write_UINT32(output,
			                    find_data->ftCreationTime.dwHighDateTime); /* CreationTime */
			Stream_Write_UINT32(output,
			                    find_data->ftLastAccessTime.dwLowDateTime); /* LastAccessTime */
			Stream_Write_UINT32(output,
			                    find_data->ftLastAccessTime.dwHighDateTime); /* LastAccessTime */
			Stream_Write_UINT32(output,
			                    find_data->ftLastWriteTime.dwLowDateTime); /* LastWriteTime */
			Stream_Write_UINT32(output,
			                    find_data->ftLastWriteTime.dwHighDateTime); /* LastWriteTime */
			Stream_Write_UINT32(output, find_data->ftLastWriteTime.dwLowDateTime);  /* ChangeTime */
			Stream_Write_UINT32(output, find_data->ftLastWriteTime.dwHighDateTime); /* ChangeTime */
			Stream_Write_UINT32(output, find_data->nFileSizeLow);                   /* EndOfFile */
			Stream_Write_UINT32(output, find_data->nFileSizeHigh);                  /* EndOfFile */
			Stream_Write_UINT32(output, find_data->nFileSizeLow);          /* AllocationSize */
			Stream_Write_UINT32(output, find_data->nFileSizeHigh);         /* AllocationSize */
			Stream_Write_UINT32(output, find_data->dwFileAttributes);      /* FileAttributes */
			Stream_Write_UINT32(output, (UINT32)length);                   /* FileNameLength */
			Stream_Write_UINT32(output, 0);                                /* EaSize */
			Stream_Write_UINT8(output, 0);                                 /* ShortNameLength */
			/* Reserved(1), MUST NOT be added! */
			Stream_Zero(output, 24); /* ShortName */
			Stream_Write(output, find_data->cFileName, length);
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
			Stream_Write(output, find_data->cFileName, length);
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

uintptr_t drive_file_get_id(DRIVE_FILE* drive)
{
	WINPR_ASSERT(drive);
	return drive->id;
}

BOOL drive_file_is_dir_not_empty(DRIVE_FILE* drive)
{
	if (!drive)
		return FALSE;

	WINPR_ASSERT(drive->context);
	WINPR_ASSERT(drive->backend);
	WINPR_ASSERT(drive->backend->empty);
	return drive->backend->empty(drive->context);
}

WINPR_ATTR_MALLOC(free, 1)
char* drive_file_resolve_path(const rdpDriveDriver* backend, const char* what)
{
	WINPR_ASSERT(backend);
	WINPR_ASSERT(backend->resolve_path);
	return backend->resolve_path(what);
}

WINPR_ATTR_MALLOC(free, 1)
char* drive_file_resolve_name(const rdpDriveDriver* backend, const char* path,
                              const char* suggested)
{
	WINPR_ASSERT(backend);
	WINPR_ASSERT(backend->resolve_name);
	return backend->resolve_name(path, suggested);
}
