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

#ifndef _WIN32
#define __USE_LARGEFILE64
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <sys/time.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/file.h>
#include <winpr/stream.h>

#include <freerdp/channels/rdpdr.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#define __USE_GNU /* for O_PATH */
#include <fcntl.h>
#undef __USE_GNU
#endif

#ifdef _WIN32
#pragma comment(lib, "Shlwapi.lib")
#include <Shlwapi.h>
#else
#include <winpr/path.h>
#endif

#include "drive_file.h"

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4244)
#endif

static void drive_file_fix_path(char* path)
{
	int i;
	int length;

	length = (int) strlen(path);

	for (i = 0; i < length; i++)
	{
		if (path[i] == '\\')
			path[i] = '/';
	}

#ifdef WIN32
	if ((length == 3) && (path[1] == ':') && (path[2] == '/'))
		return;
#else
	if ((length == 1) && (path[0] == '/'))
		return;
#endif

	if ((length > 0) && (path[length - 1] == '/'))
		path[length - 1] = '\0';
}

static char* drive_file_combine_fullpath(const char* base_path, const char* path)
{
	char* fullpath;

	fullpath = (char*) malloc(strlen(base_path) + strlen(path) + 1);
	if (!fullpath)
	{
		WLog_ERR(TAG, "malloc failed!");
		return NULL;
	}
	strcpy(fullpath, base_path);
	strcat(fullpath, path);
	drive_file_fix_path(fullpath);

	return fullpath;
}

static BOOL drive_file_remove_dir(const char* path)
{
	DIR* dir;
	char* p;
	struct STAT st;
	struct dirent* pdirent;
	BOOL ret = TRUE;

	dir = opendir(path);

	if (dir == NULL)
		return FALSE;

	pdirent = readdir(dir);

	while (pdirent)
	{
		if (strcmp(pdirent->d_name, ".") == 0 || strcmp(pdirent->d_name, "..") == 0)
		{
			pdirent = readdir(dir);
			continue;
		}

		p = (char*) malloc(strlen(path) + strlen(pdirent->d_name) + 2);
		if (!p)
		{
			WLog_ERR(TAG, "malloc failed!");
			return FALSE;
		}
		sprintf(p, "%s/%s", path, pdirent->d_name);

		if (STAT(p, &st) != 0)
		{
			ret = FALSE;
		}
		else if (S_ISDIR(st.st_mode))
		{
			ret = drive_file_remove_dir(p);
		}
		else if (unlink(p) < 0)
		{
			ret = FALSE;
		}
		else
		{
			ret = TRUE;
		}
		
		free(p);

		if (!ret)
			break;

		pdirent = readdir(dir);
	}

	closedir(dir);

	if (ret)
	{
		if (rmdir(path) < 0)
		{
			ret = FALSE;
		}
	}

	return ret;
}

static void drive_file_set_fullpath(DRIVE_FILE* file, char* fullpath)
{
	free(file->fullpath);
	file->fullpath = fullpath;
	file->filename = strrchr(file->fullpath, '/');

	if (file->filename == NULL)
		file->filename = file->fullpath;
	else
		file->filename += 1;
}

static BOOL drive_file_init(DRIVE_FILE* file, UINT32 DesiredAccess, UINT32 CreateDisposition, UINT32 CreateOptions)
{
	struct STAT st;
	BOOL exists;
#ifdef WIN32
	const static int mode = _S_IREAD | _S_IWRITE ;
#else
	const static int mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
	BOOL largeFile = FALSE;
#endif
	int oflag = 0;

	if (STAT(file->fullpath, &st) == 0)
	{
		file->is_dir = (S_ISDIR(st.st_mode) ? TRUE : FALSE);
		if (!file->is_dir && !S_ISREG(st.st_mode))
		{
			file->err = EPERM;
			return TRUE;
		}
#ifndef WIN32
		if (st.st_size > (unsigned long) 0x07FFFFFFF)
			largeFile = TRUE;
#endif
		exists = TRUE;
	}
	else
	{
		file->is_dir = ((CreateOptions & FILE_DIRECTORY_FILE) ? TRUE : FALSE);
		if (file->is_dir)
		{
			/* Should only create the directory if the disposition allows for it */
			if ((CreateDisposition == FILE_OPEN_IF) || (CreateDisposition == FILE_CREATE))
			{
				if (mkdir(file->fullpath, mode) != 0)
				{
					file->err = errno;
					return TRUE;
				}
			}
		}
		exists = FALSE;
	}

	if (file->is_dir)
	{
		file->dir = opendir(file->fullpath);

		if (file->dir == NULL)
		{
			file->err = errno;
			return TRUE;
		}
	}
	else
	{
		switch (CreateDisposition)
		{
			case FILE_SUPERSEDE:
				oflag = O_TRUNC | O_CREAT;
				break;
			case FILE_OPEN:
				break;
			case FILE_CREATE:
				oflag = O_CREAT | O_EXCL;
				break;
			case FILE_OPEN_IF:
				oflag = O_CREAT;
				break;
			case FILE_OVERWRITE:
				oflag = O_TRUNC;
				break;
			case FILE_OVERWRITE_IF:
				oflag = O_TRUNC | O_CREAT;
				break;
			default:
				break;
		}

		if ((CreateOptions & FILE_DELETE_ON_CLOSE) && (DesiredAccess & DELETE))
		{
			file->delete_pending = TRUE;
		}

		if ((DesiredAccess & GENERIC_ALL)
			|| (DesiredAccess & GENERIC_WRITE)
			|| (DesiredAccess & FILE_WRITE_DATA)
			|| (DesiredAccess & FILE_APPEND_DATA))
		{
			oflag |= O_RDWR;
		}
		else
		{
			oflag |= O_RDONLY;
		}
#ifndef WIN32
		if (largeFile)
		{
			oflag |= O_LARGEFILE;
		}
#else
		oflag |= O_BINARY;
#endif
		file->fd = OPEN(file->fullpath, oflag, mode);

		if (file->fd == -1)
		{
			file->err = errno;
			return TRUE;
		}
	}

	return TRUE;
}

DRIVE_FILE* drive_file_new(const char* base_path, const char* path, UINT32 id,
	UINT32 DesiredAccess, UINT32 CreateDisposition, UINT32 CreateOptions)
{
	DRIVE_FILE* file;

	file = (DRIVE_FILE*) calloc(1, sizeof(DRIVE_FILE));
	if (!file)
	{
		WLog_ERR(TAG, "calloc failed!");
		return NULL;
	}

	file->id = id;
	file->basepath = (char*) base_path;
	drive_file_set_fullpath(file, drive_file_combine_fullpath(base_path, path));
	file->fd = -1;

	if (!drive_file_init(file, DesiredAccess, CreateDisposition, CreateOptions))
	{
		drive_file_free(file);
		return NULL;
	}

#if defined(__linux__) && defined(O_PATH)
	if (file->fd < 0 && file->err == EACCES)
	{
		/**
		 * We have no access permissions for the file or directory but if the
		 * peer is only interested in reading the object's attributes we can try
		 * to obtain a file descriptor who's only purpose is to perform
		 * operations that act purely at the file descriptor level.
		 * See open(2)
		 **/
		 {
			if ((file->fd = OPEN(file->fullpath, O_PATH)) >= 0)
			{
				file->err = 0;
			}
		 }
	}
#endif

	return file;
}

void drive_file_free(DRIVE_FILE* file)
{
	if (file->fd != -1)
		close(file->fd);

	if (file->dir != NULL)
		closedir(file->dir);

	if (file->delete_pending)
	{
		if (file->is_dir)
			drive_file_remove_dir(file->fullpath);
		else
			unlink(file->fullpath);
	}

	free(file->pattern);
	free(file->fullpath);
	free(file);
}

BOOL drive_file_seek(DRIVE_FILE* file, UINT64 Offset)
{
	if (file->is_dir || file->fd == -1)
		return FALSE;

	if (LSEEK(file->fd, Offset, SEEK_SET) == (off_t)-1)
		return FALSE;

	return TRUE;
}

BOOL drive_file_read(DRIVE_FILE* file, BYTE* buffer, UINT32* Length)
{
	ssize_t r;

	if (file->is_dir || file->fd == -1)
		return FALSE;

	r = read(file->fd, buffer, *Length);

	if (r < 0)
		return FALSE;

	*Length = (UINT32) r;

	return TRUE;
}

BOOL drive_file_write(DRIVE_FILE* file, BYTE* buffer, UINT32 Length)
{
	ssize_t r;

	if (file->is_dir || file->fd == -1)
		return FALSE;

	while (Length > 0)
	{
		r = write(file->fd, buffer, Length);

		if (r == -1)
			return FALSE;

		Length -= r;
		buffer += r;
	}

	return TRUE;
}

BOOL drive_file_query_information(DRIVE_FILE* file, UINT32 FsInformationClass, wStream* output)
{
	struct STAT st;

	if (STAT(file->fullpath, &st) != 0)
	{
		Stream_Write_UINT32(output, 0); /* Length */
		return FALSE;
	}

	switch (FsInformationClass)
	{
		case FileBasicInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232094.aspx */
			if (!Stream_EnsureRemainingCapacity(output, 4 + 36))
				goto out_fail;
			Stream_Write_UINT32(output, 36); /* Length */
			Stream_Write_UINT64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_mtime)); /* CreationTime */
			Stream_Write_UINT64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_atime)); /* LastAccessTime */
			Stream_Write_UINT64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_mtime)); /* LastWriteTime */
			Stream_Write_UINT64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_ctime)); /* ChangeTime */
			Stream_Write_UINT32(output, FILE_ATTR_SYSTEM_TO_RDP(file, st)); /* FileAttributes */
			/* Reserved(4), MUST NOT be added! */
			break;

		case FileStandardInformation:
			/*  http://msdn.microsoft.com/en-us/library/cc232088.aspx */
			if (!Stream_EnsureRemainingCapacity(output, 4 + 22))
				goto out_fail;
			Stream_Write_UINT32(output, 22); /* Length */
			Stream_Write_UINT64(output, st.st_size); /* AllocationSize */
			Stream_Write_UINT64(output, st.st_size); /* EndOfFile */
			Stream_Write_UINT32(output, st.st_nlink); /* NumberOfLinks */
			Stream_Write_UINT8(output, file->delete_pending ? 1 : 0); /* DeletePending */
			Stream_Write_UINT8(output, file->is_dir ? 1 : 0); /* Directory */
			/* Reserved(2), MUST NOT be added! */
			break;

		case FileAttributeTagInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232093.aspx */
			if (!Stream_EnsureRemainingCapacity(output, 4 + 8))
				goto out_fail;
			Stream_Write_UINT32(output, 8); /* Length */
			Stream_Write_UINT32(output, FILE_ATTR_SYSTEM_TO_RDP(file, st)); /* FileAttributes */
			Stream_Write_UINT32(output, 0); /* ReparseTag */
			break;

		default:
			/* Unhandled FsInformationClass */
			Stream_Write_UINT32(output, 0); /* Length */
			return FALSE;
	}
	return TRUE;

out_fail:
	Stream_Write_UINT32(output, 0); /* Length */
	return FALSE;
}

int dir_empty(const char *path)
{
#ifdef _WIN32
	return PathIsDirectoryEmptyA(path);
#else
	struct dirent *dp;
	int empty = 1;

	DIR *dir = opendir(path);
	if (dir == NULL) //Not a directory or doesn't exist
		return 1;

	while ((dp = readdir(dir)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
			continue;    /* Skip . and .. */

		empty = 0;
		break;
	}
	closedir(dir);
	return empty;
#endif
}
BOOL drive_file_set_information(DRIVE_FILE* file, UINT32 FsInformationClass, UINT32 Length, wStream* input)
{
	char* s = NULL;
	mode_t m;
	INT64 size;
	int status;
	char* fullpath;
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
	HANDLE hFd;
	LARGE_INTEGER liSize;

	m = 0;

	switch (FsInformationClass)
	{
		case FileBasicInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232094.aspx */
			Stream_Read_UINT64(input, liCreationTime.QuadPart);
			Stream_Read_UINT64(input, liLastAccessTime.QuadPart);
			Stream_Read_UINT64(input, liLastWriteTime.QuadPart);
			Stream_Read_UINT64(input, liChangeTime.QuadPart);
			Stream_Read_UINT32(input, FileAttributes);

			if (!PathFileExistsA(file->fullpath))
				return FALSE;
			hFd = CreateFileA(file->fullpath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hFd == INVALID_HANDLE_VALUE)
			{
				WLog_ERR(TAG, "Unable to set file time %s to %d", file->fullpath);
				return FALSE;
			}
			if (liCreationTime.QuadPart != 0)
			{
				ftCreationTime.dwHighDateTime = liCreationTime.HighPart;
				ftCreationTime.dwLowDateTime = liCreationTime.LowPart;
				pftCreationTime = &ftCreationTime;
			}
			if (liLastAccessTime.QuadPart != 0)
			{
				ftLastAccessTime.dwHighDateTime = liLastAccessTime.HighPart;
				ftLastAccessTime.dwLowDateTime = liLastAccessTime.LowPart;
				pftLastAccessTime = &ftLastAccessTime;
			}
			if (liLastWriteTime.QuadPart != 0)
			{
				ftLastWriteTime.dwHighDateTime = liLastWriteTime.HighPart;
				ftLastWriteTime.dwLowDateTime = liLastWriteTime.LowPart;
				pftLastWriteTime = &ftLastWriteTime;
			}
			if (liChangeTime.QuadPart != 0 && liChangeTime.QuadPart > liLastWriteTime.QuadPart)
			{
				ftLastWriteTime.dwHighDateTime = liChangeTime.HighPart;
				ftLastWriteTime.dwLowDateTime = liChangeTime.LowPart;
				pftLastWriteTime = &ftLastWriteTime;
			}
			if (!SetFileTime(hFd, pftCreationTime, pftLastAccessTime, pftLastWriteTime))
			{
				WLog_ERR(TAG, "Unable to set file time %s to %d", file->fullpath);
				CloseHandle(hFd);
				return FALSE;
			}
			CloseHandle(hFd);
			break;

		case FileEndOfFileInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232067.aspx */
		case FileAllocationInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232076.aspx */
			Stream_Read_INT64(input, size);

			hFd = CreateFileA(file->fullpath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hFd == INVALID_HANDLE_VALUE)
			{
				WLog_ERR(TAG, "Unable to truncate %s to %d", file->fullpath, size);
				return FALSE;
			}
			liSize.QuadPart = size;
			if (SetFilePointer(hFd, liSize.LowPart, &liSize.HighPart, FILE_BEGIN) == 0)
			{
				WLog_ERR(TAG, "Unable to truncate %s to %d", file->fullpath, size);
				CloseHandle(hFd);
				return FALSE;
			}
			CloseHandle(hFd);
			break;

		case FileDispositionInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232098.aspx */
			/* http://msdn.microsoft.com/en-us/library/cc241371.aspx */
			if (file->is_dir && !dir_empty(file->fullpath))
				break;

			if (Length)
				Stream_Read_UINT8(input, file->delete_pending);
			else
				file->delete_pending = 1;
			break;

		case FileRenameInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232085.aspx */
			Stream_Seek_UINT8(input); /* ReplaceIfExists */
			Stream_Seek_UINT8(input); /* RootDirectory */
			Stream_Read_UINT32(input, FileNameLength);

			status = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) Stream_Pointer(input),
					FileNameLength / 2, &s, 0, NULL, NULL);

			if (status < 1)
				if (!(s = (char*) calloc(1, 1)))
				{
					WLog_ERR(TAG, "calloc failed!");
					return FALSE;
				}

			fullpath = drive_file_combine_fullpath(file->basepath, s);
			if (!fullpath)
			{
				WLog_ERR(TAG, "drive_file_combine_fullpath failed!");
				free (s);
				return FALSE;
			}
			free(s);

#ifdef _WIN32
			if (file->fd)
				close(file->fd);
#endif
			if (rename(file->fullpath, fullpath) == 0)
			{
				drive_file_set_fullpath(file, fullpath);
#ifdef _WIN32
				file->fd = OPEN(fullpath, O_RDWR | O_BINARY);
#endif
			}
			else
			{
				free(fullpath);
				return FALSE;
			}

			break;

		default:
			return FALSE;
	}

	return TRUE;
}

BOOL drive_file_query_directory(DRIVE_FILE* file, UINT32 FsInformationClass, BYTE InitialQuery,
	const char* path, wStream* output)
{
	int length;
	BOOL ret;
	WCHAR* ent_path;
	struct STAT st;
	struct dirent* ent;

	if (!file->dir)
	{
		Stream_Write_UINT32(output, 0); /* Length */
		Stream_Write_UINT8(output, 0); /* Padding */
		return FALSE;
	}

	if (InitialQuery != 0)
	{
		rewinddir(file->dir);
		free(file->pattern);

		if (path[0])
		{
			if (!(file->pattern = _strdup(strrchr(path, '\\') + 1)))
			{
				WLog_ERR(TAG, "_strdup failed!");
				return FALSE;
			}
		}
		else
			file->pattern = NULL;
	}

	if (file->pattern)
	{
		do
		{
			ent = readdir(file->dir);

			if (ent == NULL)
				continue;

			if (FilePatternMatchA(ent->d_name, file->pattern))
				break;

		}
		while (ent);
	}
	else
	{
		ent = readdir(file->dir);
	}

	if (!ent)
	{
		Stream_Write_UINT32(output, 0); /* Length */
		Stream_Write_UINT8(output, 0); /* Padding */
		return FALSE;
	}

	memset(&st, 0, sizeof(struct STAT));
	ent_path = (WCHAR*) malloc(strlen(file->fullpath) + strlen(ent->d_name) + 2);
	if (!ent_path)
	{
		WLog_ERR(TAG, "malloc failed!");
		return FALSE;
	}
	sprintf((char*) ent_path, "%s/%s", file->fullpath, ent->d_name);

	if (STAT((char*) ent_path, &st) != 0)
	{

	}

	free(ent_path);
	ent_path = NULL;

	length = ConvertToUnicode(sys_code_page, 0, ent->d_name, -1, &ent_path, 0) * 2;

	ret = TRUE;

	switch (FsInformationClass)
	{
		case FileDirectoryInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232097.aspx */
			if (!Stream_EnsureRemainingCapacity(output, 4 + 64 + length))
				goto out_fail;
			Stream_Write_UINT32(output, 64 + length); /* Length */
			Stream_Write_UINT32(output, 0); /* NextEntryOffset */
			Stream_Write_UINT32(output, 0); /* FileIndex */
			Stream_Write_UINT64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_mtime)); /* CreationTime */
			Stream_Write_UINT64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_atime)); /* LastAccessTime */
			Stream_Write_UINT64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_mtime)); /* LastWriteTime */
			Stream_Write_UINT64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_ctime)); /* ChangeTime */
			Stream_Write_UINT64(output, st.st_size); /* EndOfFile */
			Stream_Write_UINT64(output, st.st_size); /* AllocationSize */
			Stream_Write_UINT32(output, FILE_ATTR_SYSTEM_TO_RDP(file, st)); /* FileAttributes */
			Stream_Write_UINT32(output, length); /* FileNameLength */
			Stream_Write(output, ent_path, length);
			break;

		case FileFullDirectoryInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232068.aspx */
			if (!Stream_EnsureRemainingCapacity(output, 4 + 68 + length))
				goto out_fail;
			Stream_Write_UINT32(output, 68 + length); /* Length */
			Stream_Write_UINT32(output, 0); /* NextEntryOffset */
			Stream_Write_UINT32(output, 0); /* FileIndex */
			Stream_Write_UINT64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_mtime)); /* CreationTime */
			Stream_Write_UINT64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_atime)); /* LastAccessTime */
			Stream_Write_UINT64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_mtime)); /* LastWriteTime */
			Stream_Write_UINT64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_ctime)); /* ChangeTime */
			Stream_Write_UINT64(output, st.st_size); /* EndOfFile */
			Stream_Write_UINT64(output, st.st_size); /* AllocationSize */
			Stream_Write_UINT32(output, FILE_ATTR_SYSTEM_TO_RDP(file, st)); /* FileAttributes */
			Stream_Write_UINT32(output, length); /* FileNameLength */
			Stream_Write_UINT32(output, 0); /* EaSize */
			Stream_Write(output, ent_path, length);
			break;

		case FileBothDirectoryInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232095.aspx */
			if (!Stream_EnsureRemainingCapacity(output, 4 + 93 + length))
				goto out_fail;
			Stream_Write_UINT32(output, 93 + length); /* Length */
			Stream_Write_UINT32(output, 0); /* NextEntryOffset */
			Stream_Write_UINT32(output, 0); /* FileIndex */
			Stream_Write_UINT64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_mtime)); /* CreationTime */
			Stream_Write_UINT64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_atime)); /* LastAccessTime */
			Stream_Write_UINT64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_mtime)); /* LastWriteTime */
			Stream_Write_UINT64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_ctime)); /* ChangeTime */
			Stream_Write_UINT64(output, st.st_size); /* EndOfFile */
			Stream_Write_UINT64(output, st.st_size); /* AllocationSize */
			Stream_Write_UINT32(output, FILE_ATTR_SYSTEM_TO_RDP(file, st)); /* FileAttributes */
			Stream_Write_UINT32(output, length); /* FileNameLength */
			Stream_Write_UINT32(output, 0); /* EaSize */
			Stream_Write_UINT8(output, 0); /* ShortNameLength */
			/* Reserved(1), MUST NOT be added! */
			Stream_Zero(output, 24); /* ShortName */
			Stream_Write(output, ent_path, length);
			break;

		case FileNamesInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232077.aspx */
			if (!Stream_EnsureRemainingCapacity(output, 4 + 12 + length))
				goto out_fail;
			Stream_Write_UINT32(output, 12 + length); /* Length */
			Stream_Write_UINT32(output, 0); /* NextEntryOffset */
			Stream_Write_UINT32(output, 0); /* FileIndex */
			Stream_Write_UINT32(output, length); /* FileNameLength */
			Stream_Write(output, ent_path, length);
			break;

		default:
			/* Unhandled FsInformationClass */
			Stream_Write_UINT32(output, 0); /* Length */
			Stream_Write_UINT8(output, 0); /* Padding */
			ret = FALSE;
			break;
	}

	free(ent_path);
	return ret;

out_fail:
	free(ent_path);
	Stream_Write_UINT32(output, 0); /* Length */
	Stream_Write_UINT8(output, 0); /* Padding */
	return FALSE;
}

#ifdef _WIN32
#pragma warning(pop)
#endif
