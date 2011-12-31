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

#ifndef _WIN32
#include <sys/time.h>
#endif

#include "config.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
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


static boolean disk_file_wildcard_match(const char* pattern, const char* filename)
{
	const char *p = pattern, *f = filename;
	char c;

	/*
	 * TODO: proper wildcard rules per msft's File System Behavior Overview
	 * Simple cases for now.
	 */
	f = filename;
	while ((c = *p++))
	{
		if (c == '*')
		{
			c = *p++;
			if (!c)	/* shortcut */
				return true;
			/* TODO: skip to tail comparison */
		}
		if (c != *f++)
			return false;
	}

	if (!*f)
		return true;

	return false;
}

static void disk_file_fix_path(char* path)
{
	int len;
	int i;

	len = strlen(path);
	for (i = 0; i < len; i++)
	{
		if (path[i] == '\\')
			path[i] = '/';
	}
	if (len > 0 && path[len - 1] == '/')
		path[len - 1] = '\0';
}

static char* disk_file_combine_fullpath(const char* base_path, const char* path)
{
	char* fullpath;

	fullpath = xmalloc(strlen(base_path) + strlen(path) + 1);
	strcpy(fullpath, base_path);
	strcat(fullpath, path);
	disk_file_fix_path(fullpath);

	return fullpath;
}

static boolean disk_file_remove_dir(const char* path)
{
	DIR* dir;
	struct dirent* pdirent;
	struct stat st;
	char* p;
	boolean ret = true;

	dir = opendir(path);
	if (dir == NULL)
		return false;

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
			ret = false;
		}
		else if (S_ISDIR(st.st_mode))
		{
			ret = disk_file_remove_dir(p);
		}
		else if (unlink(p) < 0)
		{
			DEBUG_WARN("unlink %s failed.", p);
			ret = false;
		}
		else
			ret = true;
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
			ret = false;
		}
	}

	return ret;
}

static void disk_file_set_fullpath(DISK_FILE* file, char* fullpath)
{
	xfree(file->fullpath);
	file->fullpath = fullpath;
	file->filename = strrchr(file->fullpath, '/');
	if (file->filename == NULL)
		file->filename = file->fullpath;
	else
		file->filename += 1;
}

static boolean disk_file_init(DISK_FILE* file, uint32 DesiredAccess, uint32 CreateDisposition, uint32 CreateOptions)
{
	const static int mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
	struct stat st;
	boolean exists;
	int oflag = 0;

	if (stat(file->fullpath, &st) == 0)
	{
		file->is_dir = (S_ISDIR(st.st_mode) ? true : false);
		exists = true;
	}
	else
	{
		file->is_dir = ((CreateOptions & FILE_DIRECTORY_FILE) ? true : false);
		if (file->is_dir)
		{
			if (mkdir(file->fullpath, mode) != 0)
			{
				file->err = errno;
				return true;
			}
		}
		exists = false;
	}
	if (file->is_dir)
	{
		file->dir = opendir(file->fullpath);
		if (file->dir == NULL)
		{
			file->err = errno;
			return true;
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

		if (CreateOptions & FILE_DELETE_ON_CLOSE && DesiredAccess & DELETE)
		{
			file->delete_pending = true;
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

		file->fd = open(file->fullpath, oflag, mode);
		if (file->fd == -1)
		{
			file->err = errno;
			return true;
		}
	}

	return true;
}

DISK_FILE* disk_file_new(const char* base_path, const char* path, uint32 id,
	uint32 DesiredAccess, uint32 CreateDisposition, uint32 CreateOptions)
{
	DISK_FILE* file;

	file = xnew(DISK_FILE);
	file->id = id;
	file->basepath = (char*) base_path;
	disk_file_set_fullpath(file, disk_file_combine_fullpath(base_path, path));
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

	xfree(file->pattern);
	xfree(file->fullpath);
	xfree(file);
}

boolean disk_file_seek(DISK_FILE* file, uint64 Offset)
{
	if (file->is_dir || file->fd == -1)
		return false;

	if (lseek(file->fd, Offset, SEEK_SET) == (off_t)-1)
		return false;

	return true;
}

boolean disk_file_read(DISK_FILE* file, uint8* buffer, uint32* Length)
{
	ssize_t r;

	if (file->is_dir || file->fd == -1)
		return false;

	r = read(file->fd, buffer, *Length);
	if (r < 0)
		return false;
	*Length = (uint32)r;

	return true;
}

boolean disk_file_write(DISK_FILE* file, uint8* buffer, uint32 Length)
{
	ssize_t r;

	if (file->is_dir || file->fd == -1)
		return false;

	while (Length > 0)
	{
		r = write(file->fd, buffer, Length);
		if (r == -1)
			return false;
		Length -= r;
		buffer += r;
	}

	return true;
}

boolean disk_file_query_information(DISK_FILE* file, uint32 FsInformationClass, STREAM* output)
{
	struct stat st;

	if (stat(file->fullpath, &st) != 0)
	{
		stream_write_uint32(output, 0); /* Length */
		return false;
	}
	switch (FsInformationClass)
	{
		case FileBasicInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232094.aspx */
			stream_write_uint32(output, 36); /* Length */
			stream_check_size(output, 36);
			stream_write_uint64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_mtime)); /* CreationTime */
			stream_write_uint64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_atime)); /* LastAccessTime */
			stream_write_uint64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_mtime)); /* LastWriteTime */
			stream_write_uint64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_ctime)); /* ChangeTime */
			stream_write_uint32(output, FILE_ATTR_SYSTEM_TO_RDP(file, st)); /* FileAttributes */
			/* Reserved(4), MUST NOT be added! */
			break;

		case FileStandardInformation:
			/*  http://msdn.microsoft.com/en-us/library/cc232088.aspx */
			stream_write_uint32(output, 22); /* Length */
			stream_check_size(output, 22);
			stream_write_uint64(output, st.st_size); /* AllocationSize */
			stream_write_uint64(output, st.st_size); /* EndOfFile */
			stream_write_uint32(output, st.st_nlink); /* NumberOfLinks */
			stream_write_uint8(output, file->delete_pending ? 1 : 0); /* DeletePending */
			stream_write_uint8(output, file->is_dir ? 1 : 0); /* Directory */
			/* Reserved(2), MUST NOT be added! */
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
			return false;
	}
	return true;
}

boolean disk_file_set_information(DISK_FILE* file, uint32 FsInformationClass, uint32 Length, STREAM* input)
{
	char* s;
	mode_t m;
	uint64 size;
	char* fullpath;
	struct stat st;
	UNICONV* uniconv;
	struct timeval tv[2];
	uint64 LastWriteTime;
	uint32 FileAttributes;
	uint32 FileNameLength;

	switch (FsInformationClass)
	{
		case FileBasicInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232094.aspx */
			stream_seek_uint64(input); /* CreationTime */
			stream_seek_uint64(input); /* LastAccessTime */
			stream_read_uint64(input, LastWriteTime);
			stream_seek_uint64(input); /* ChangeTime */
			stream_read_uint32(input, FileAttributes);

			if (fstat(file->fd, &st) != 0)
				return false;

			tv[0].tv_sec = st.st_atime;
			tv[0].tv_usec = 0;
			tv[1].tv_sec = (LastWriteTime > 0 ? FILE_TIME_RDP_TO_SYSTEM(LastWriteTime) : st.st_mtime);
			tv[1].tv_usec = 0;
			futimes(file->fd, tv);

			if (FileAttributes > 0)
			{
				m = st.st_mode;
				if ((FileAttributes & FILE_ATTRIBUTE_READONLY) == 0)
					m |= S_IWUSR;
				else
					m &= ~S_IWUSR;
				if (m != st.st_mode)
					fchmod(file->fd, st.st_mode);
			}
			break;

		case FileEndOfFileInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232067.aspx */
		case FileAllocationInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232076.aspx */
			stream_read_uint64(input, size);
			if (ftruncate(file->fd, size) != 0)
				return false;
			break;

		case FileDispositionInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232098.aspx */
			/* http://msdn.microsoft.com/en-us/library/cc241371.aspx */
			if (Length)
				stream_read_uint8(input, file->delete_pending);
			else
				file->delete_pending = 1;
			break;

		case FileRenameInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232085.aspx */
			stream_seek_uint8(input); /* ReplaceIfExists */
			stream_seek_uint8(input); /* RootDirectory */
			stream_read_uint32(input, FileNameLength);
			uniconv = freerdp_uniconv_new();
			s = freerdp_uniconv_in(uniconv, stream_get_tail(input), FileNameLength);
			freerdp_uniconv_free(uniconv);

			fullpath = disk_file_combine_fullpath(file->basepath, s);
			xfree(s);

			if (rename(file->fullpath, fullpath) == 0)
			{
				DEBUG_SVC("renamed %s to %s", file->fullpath, fullpath);
				disk_file_set_fullpath(file, fullpath);
			}
			else
			{
				DEBUG_WARN("rename %s to %s failed", file->fullpath, fullpath);
				free(fullpath);
				return false;
			}

			break;

		default:
			DEBUG_WARN("invalid FsInformationClass %d", FsInformationClass);
			return false;
	}

	return true;
}

boolean disk_file_query_directory(DISK_FILE* file, uint32 FsInformationClass, uint8 InitialQuery,
	const char* path, STREAM* output)
{
	struct dirent* ent;
	char* ent_path;
	struct stat st;
	UNICONV* uniconv;
	size_t len;
	boolean ret;

	DEBUG_SVC("path %s FsInformationClass %d InitialQuery %d", path, FsInformationClass, InitialQuery);

	if (!file->dir)
	{
		stream_write_uint32(output, 0); /* Length */
		stream_write_uint8(output, 0); /* Padding */
		return false;
	}

	if (InitialQuery != 0)
	{
		rewinddir(file->dir);
		xfree(file->pattern);

		if (path[0])
			file->pattern = strdup(strrchr(path, '\\') + 1);
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

			if (disk_file_wildcard_match(file->pattern, ent->d_name))
				break;
		} while (ent);
	}
	else
	{
		ent = readdir(file->dir);
	}

	if (ent == NULL)
	{
		DEBUG_SVC("  pattern %s not found.", file->pattern);
		stream_write_uint32(output, 0); /* Length */
		stream_write_uint8(output, 0); /* Padding */
		return false;
	}

	memset(&st, 0, sizeof(struct stat));
	ent_path = xmalloc(strlen(file->fullpath) + strlen(ent->d_name) + 2);
	sprintf(ent_path, "%s/%s", file->fullpath, ent->d_name);
	if (stat(ent_path, &st) != 0)
	{
		DEBUG_WARN("stat %s failed.", ent_path);
	}
	xfree(ent_path);

	DEBUG_SVC("  pattern %s matched %s\n", file->pattern, ent_path);

	uniconv = freerdp_uniconv_new();
	ent_path = freerdp_uniconv_out(uniconv, ent->d_name, &len);
	freerdp_uniconv_free(uniconv);

	ret = true;
	switch (FsInformationClass)
	{
		case FileDirectoryInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232097.aspx */
			stream_write_uint32(output, 64 + len); /* Length */
			stream_check_size(output, 64 + len);
			stream_write_uint32(output, 0); /* NextEntryOffset */
			stream_write_uint32(output, 0); /* FileIndex */
			stream_write_uint64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_mtime)); /* CreationTime */
			stream_write_uint64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_atime)); /* LastAccessTime */
			stream_write_uint64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_mtime)); /* LastWriteTime */
			stream_write_uint64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_ctime)); /* ChangeTime */
			stream_write_uint64(output, st.st_size); /* EndOfFile */
			stream_write_uint64(output, st.st_size); /* AllocationSize */
			stream_write_uint32(output, FILE_ATTR_SYSTEM_TO_RDP(file, st)); /* FileAttributes */
			stream_write_uint32(output, len); /* FileNameLength */
			stream_write(output, ent_path, len);
			break;

		case FileFullDirectoryInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232068.aspx */
			stream_write_uint32(output, 68 + len); /* Length */
			stream_check_size(output, 68 + len);
			stream_write_uint32(output, 0); /* NextEntryOffset */
			stream_write_uint32(output, 0); /* FileIndex */
			stream_write_uint64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_mtime)); /* CreationTime */
			stream_write_uint64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_atime)); /* LastAccessTime */
			stream_write_uint64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_mtime)); /* LastWriteTime */
			stream_write_uint64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_ctime)); /* ChangeTime */
			stream_write_uint64(output, st.st_size); /* EndOfFile */
			stream_write_uint64(output, st.st_size); /* AllocationSize */
			stream_write_uint32(output, FILE_ATTR_SYSTEM_TO_RDP(file, st)); /* FileAttributes */
			stream_write_uint32(output, len); /* FileNameLength */
			stream_write_uint32(output, 0); /* EaSize */
			stream_write(output, ent_path, len);
			break;

		case FileBothDirectoryInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232095.aspx */
			stream_write_uint32(output, 93 + len); /* Length */
			stream_check_size(output, 93 + len);
			stream_write_uint32(output, 0); /* NextEntryOffset */
			stream_write_uint32(output, 0); /* FileIndex */
			stream_write_uint64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_mtime)); /* CreationTime */
			stream_write_uint64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_atime)); /* LastAccessTime */
			stream_write_uint64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_mtime)); /* LastWriteTime */
			stream_write_uint64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_ctime)); /* ChangeTime */
			stream_write_uint64(output, st.st_size); /* EndOfFile */
			stream_write_uint64(output, st.st_size); /* AllocationSize */
			stream_write_uint32(output, FILE_ATTR_SYSTEM_TO_RDP(file, st)); /* FileAttributes */
			stream_write_uint32(output, len); /* FileNameLength */
			stream_write_uint32(output, 0); /* EaSize */
			stream_write_uint8(output, 0); /* ShortNameLength */
			/* Reserved(1), MUST NOT be added! */
			stream_write_zero(output, 24); /* ShortName */
			stream_write(output, ent_path, len);
			break;

		case FileNamesInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232077.aspx */
			stream_write_uint32(output, 12 + len); /* Length */
			stream_check_size(output, 12 + len);
			stream_write_uint32(output, 0); /* NextEntryOffset */
			stream_write_uint32(output, 0); /* FileIndex */
			stream_write_uint32(output, len); /* FileNameLength */
			stream_write(output, ent_path, len);
			break;

		default:
			stream_write_uint32(output, 0); /* Length */
			stream_write_uint8(output, 0); /* Padding */
			DEBUG_WARN("invalid FsInformationClass %d", FsInformationClass);
			ret = false;
			break;
	}

	xfree(ent_path);

	return ret;
}
