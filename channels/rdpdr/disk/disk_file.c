/**
 * FreeRDP: A Remote Desktop Protocol client.
 * File System Virtual Channel
 *
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2010-2011 Vic Lee
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fnmatch.h>
#include <utime.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/svc_plugin.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "rdpdr_constants.h"
#include "rdpdr_types.h"
#include "disk_file.h"

#define FILE_TIME_SYSTEM_TO_RDP(_t) \
	(((uint64)(_t) + 11644473600LL) * 10000000LL)
#define FILE_TIME_RDP_TO_SYSTEM(_t) \
	(((_t) == 0LL || (_t) == (uint64)(-1LL)) ? 0 : (time_t)((_t) / 10000000LL - 11644473600LL))

#define FILE_ATTR_SYSTEM_TO_RDP(_f, _st) ( \
	(S_ISDIR(_st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : 0) | \
	(_f->filename[0] == '.' ? FILE_ATTRIBUTE_HIDDEN : 0) | \
	(_f->delete_pending ? FILE_ATTRIBUTE_TEMPORARY : 0) | \
	(st.st_mode & S_IWUSR ? 0 : FILE_ATTRIBUTE_READONLY))

static char* disk_file_get_fullpath(const char* base_path, const char* path)
{
	char* fullpath;
	int len;
	int i;

	fullpath = xmalloc(strlen(base_path) + strlen(path) + 1);
	strcpy(fullpath, base_path);
	strcat(fullpath, path);
	len = strlen(fullpath);
	for (i = 0; i < len; i++)
	{
		if (fullpath[i] == '\\')
			fullpath[i] = '/';
	}
	if (len > 0 && fullpath[len - 1] == '/')
		fullpath[len - 1] = '\0';

	return fullpath;
}

static boolean disk_file_remove_dir(const char* path)
{
	DIR* dir;
	struct dirent* pdirent;
	struct stat st;
	char* p;
	boolean ret;

	dir = opendir(path);
	if (dir == NULL)
		return False;

	pdirent = readdir(dir);
	while (pdirent)
	{
		if (strcmp(pdirent->d_name, ".") == 0 || strcmp(pdirent->d_name, "..") == 0)
		{
			pdirent = readdir(dir);
			continue;
		}

		p = xmalloc(strlen(path) + strlen(pdirent->d_name) + 2);
		sprintf(p, "%s/%s", path, pdirent->d_name);
		if (stat(p, &st) != 0)
		{
			DEBUG_WARN("stat %s failed.", p);
			ret = False;
		}
		else if (S_ISDIR(st.st_mode))
		{
			ret = disk_file_remove_dir(p);
		}
		else if (unlink(p) < 0)
		{
			DEBUG_WARN("unlink %s failed.", p);
			ret = False;
		}
		else
			ret = True;
		xfree(p);

		if (!ret)
			break;

		pdirent = readdir(dir);
	}

	closedir(dir);
	if (ret)
	{
		if (rmdir(path) < 0)
		{
			DEBUG_WARN("rmdir %s failed.", path);
			ret = False;
		}
	}

	return ret;
}

boolean disk_file_init(DISK_FILE* file, uint32 DesiredAccess, uint32 CreateDisposition, uint32 CreateOptions)
{
	const static int mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
	struct stat st;
	boolean exists;
	int oflag = 0;

	if (stat(file->fullpath, &st) == 0)
	{
		file->is_dir = (S_ISDIR(st.st_mode) ? True : False);
		exists = True;
	}
	else
	{
		file->is_dir = ((CreateOptions & FILE_DIRECTORY_FILE) ? True : False);
		if (file->is_dir)
		{
			if (mkdir(file->fullpath, mode) != 0)
				return False;
		}
		exists = False;
	}
	if (file->is_dir)
	{
		file->dir = opendir(file->fullpath);
		if (file->dir == NULL)
			return False;
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
		if (!exists && (CreateOptions & FILE_DELETE_ON_CLOSE))
			file->delete_pending = 1;

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

		file->fd = open(file->fullpath, oflag, mode);
		if (file->fd == -1)
			return False;
	}

	return True;
}

DISK_FILE* disk_file_new(const char* base_path, const char* path, uint32 id,
	uint32 DesiredAccess, uint32 CreateDisposition, uint32 CreateOptions)
{
	DISK_FILE* file;

	file = xnew(DISK_FILE);
	file->id = id;
	file->fullpath = disk_file_get_fullpath(base_path, path);
	file->filename = strrchr(file->fullpath, '/');
	if (file->filename == NULL)
		file->filename = file->fullpath;
	else
		file->filename += 1;
	file->fd = -1;

	if (!disk_file_init(file, DesiredAccess, CreateDisposition, CreateOptions))
	{
		disk_file_free(file);
		return NULL;
	}

	return file;
}

void disk_file_free(DISK_FILE* file)
{
	if (file->fd != -1)
		close(file->fd);
	if (file->dir != NULL)
		closedir(file->dir);

	if (file->delete_pending)
	{
		if (file->is_dir)
			disk_file_remove_dir(file->fullpath);
		else
			unlink(file->fullpath);
	}

	xfree(file->fullpath);
	xfree(file->pattern);
	xfree(file);
}

boolean disk_file_seek(DISK_FILE* file, uint64 Offset)
{
	if (file->is_dir || file->fd == -1)
		return False;

	if (lseek(file->fd, Offset, SEEK_SET) == (off_t)-1)
		return False;

	return True;
}

boolean disk_file_read(DISK_FILE* file, uint8* buffer, uint32* Length)
{
	ssize_t r;

	if (file->is_dir || file->fd == -1)
		return False;

	r = read(file->fd, buffer, *Length);
	if (r < 0)
		return False;
	*Length = (uint32)r;

	return True;
}

boolean disk_file_write(DISK_FILE* file, uint8* buffer, uint32 Length)
{
	ssize_t r;

	if (file->is_dir || file->fd == -1)
		return False;

	while (Length > 0)
	{
		r = write(file->fd, buffer, Length);
		if (r == -1)
			return False;
		Length -= r;
		buffer += r;
	}

	return True;
}

boolean disk_file_query_information(DISK_FILE* file, uint32 FsInformationClass, STREAM* output)
{
	struct stat st;

	if (stat(file->fullpath, &st) != 0)
	{
		stream_write_uint32(output, 0); /* Length */
		return False;
	}
	switch (FsInformationClass)
	{
		case FileBasicInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232094.aspx */
			stream_write_uint32(output, 40); /* Length */
			stream_check_size(output, 40);
			stream_write_uint64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_mtime)); /* CreationTime */
			stream_write_uint64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_atime)); /* LastAccessTime */
			stream_write_uint64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_mtime)); /* LastWriteTime */
			stream_write_uint64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_ctime)); /* ChangeTime */
			stream_write_uint32(output, FILE_ATTR_SYSTEM_TO_RDP(file, st)); /* FileAttributes */
			stream_write_uint32(output, 0); /* Reserved */
			break;

		case FileStandardInformation:
			/*  http://msdn.microsoft.com/en-us/library/cc232088.aspx */
			stream_write_uint32(output, 24); /* Length */
			stream_check_size(output, 24);
			stream_write_uint64(output, st.st_size); /* AllocationSize */
			stream_write_uint64(output, st.st_size); /* EndOfFile */
			stream_write_uint32(output, st.st_nlink); /* NumberOfLinks */
			stream_write_uint8(output, file->delete_pending ? 1 : 0); /* DeletePending */
			stream_write_uint8(output, file->is_dir ? 1 : 0); /* Directory */
			stream_write_uint16(output, 0); /* Reserved */
			break;

		case FileAttributeTagInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232093.aspx */
			stream_write_uint32(output, 8); /* Length */
			stream_check_size(output, 8);
			stream_write_uint32(output, FILE_ATTR_SYSTEM_TO_RDP(file, st)); /* FileAttributes */
			stream_write_uint32(output, 0); /* ReparseTag */
			break;

		default:
			stream_write_uint32(output, 0); /* Length */
			DEBUG_WARN("invalid FsInformationClass %d", FsInformationClass);
			return False;
	}
	return True;
}
