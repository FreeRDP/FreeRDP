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

#ifndef __DISK_FILE_H
#define __DISK_FILE_H

#include <sys/types.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <dirent.h>

#if defined WIN32
#define STAT stat
#define OPEN open
#define LSEEK lseek
#define FSTAT fstat
#define STATVFS statvfs
#elif defined(__APPLE__) || defined(__FreeBSD__)
#define STAT stat
#define OPEN open
#define LSEEK lseek
#define FSTAT fstat
#define STATVFS statvfs
#define O_LARGEFILE 0
#else
#define STAT stat64
#define OPEN open64
#define LSEEK lseek64
#define FSTAT fstat64
#define STATVFS statvfs64
#endif

#define EPOCH_DIFF 11644473600LL

#define FILE_TIME_SYSTEM_TO_RDP(_t) \
	(((uint64)(_t) + EPOCH_DIFF) * 10000000LL)
#define FILE_TIME_RDP_TO_SYSTEM(_t) \
	(((_t) == 0LL || (_t) == (uint64)(-1LL)) ? 0 : (time_t)((_t) / 10000000LL - EPOCH_DIFF))

#define FILE_ATTR_SYSTEM_TO_RDP(_f, _st) ( \
	(S_ISDIR(_st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : 0) | \
	(_f->filename[0] == '.' ? FILE_ATTRIBUTE_HIDDEN : 0) | \
	(_f->delete_pending ? FILE_ATTRIBUTE_TEMPORARY : 0) | \
	(st.st_mode & S_IWUSR ? 0 : FILE_ATTRIBUTE_READONLY))





typedef struct _DISK_FILE DISK_FILE;
struct _DISK_FILE
{
	uint32 id;
	boolean is_dir;
	int fd;
	int err;
	DIR* dir;
	char* basepath;
	char* fullpath;
	char* filename;
	char* pattern;
	boolean delete_pending;
};

DISK_FILE* disk_file_new(const char* base_path, const char* path, uint32 id,
	uint32 DesiredAccess, uint32 CreateDisposition, uint32 CreateOptions);
void disk_file_free(DISK_FILE* file);

boolean disk_file_seek(DISK_FILE* file, uint64 Offset);
boolean disk_file_read(DISK_FILE* file, uint8* buffer, uint32* Length);
boolean disk_file_write(DISK_FILE* file, uint8* buffer, uint32 Length);
boolean disk_file_query_information(DISK_FILE* file, uint32 FsInformationClass, STREAM* output);
boolean disk_file_set_information(DISK_FILE* file, uint32 FsInformationClass, uint32 Length, STREAM* input);
boolean disk_file_query_directory(DISK_FILE* file, uint32 FsInformationClass, uint8 InitialQuery,
	const char* path, STREAM* output);

#endif /* __DISK_FILE_H */
