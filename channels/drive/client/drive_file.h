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

#ifndef FREERDP_CHANNEL_DRIVE_FILE_H
#define FREERDP_CHANNEL_DRIVE_FILE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <freerdp/channels/log.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include "dirent.h"
#include "statvfs.h"
#else
#include <dirent.h>
#ifdef ANDROID
#include <sys/vfs.h>
#else
#include <sys/statvfs.h>
#endif
#endif

#ifdef _WIN32
#define STAT __stat64
#define OPEN _open
#define close _close
#define read  _read
#define write _write
#define LSEEK _lseeki64
#define FSTAT _fstat64
#define STATVFS statvfs
#define mkdir(a,b) _mkdir(a)
#define rmdir _rmdir
#define unlink(a) _unlink(a)
#define ftruncate(a,b) _chsize(a,b)

typedef UINT32 ssize_t;
typedef UINT32 mode_t;

#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#define STAT stat
#define OPEN open
#define LSEEK lseek
#define FSTAT fstat
#define STATVFS statvfs
#define O_LARGEFILE 0
#elif defined(ANDROID)
#define STAT stat
#define OPEN open
#define LSEEK lseek
#define FSTAT fstat
#define STATVFS statfs
#else
#define STAT stat64
#define OPEN open64
#define LSEEK lseek64
#define FSTAT fstat64
#define STATVFS statvfs64
#endif

#define EPOCH_DIFF 11644473600LL

#define FILE_TIME_SYSTEM_TO_RDP(_t) \
	(((UINT64)(_t) + EPOCH_DIFF) * 10000000LL)

#define FILE_ATTR_SYSTEM_TO_RDP(_f, _st) ( \
	(S_ISDIR(_st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : 0) | \
	(_f->filename[0] == '.' ? FILE_ATTRIBUTE_HIDDEN : 0) | \
	(_f->delete_pending ? FILE_ATTRIBUTE_TEMPORARY : 0) | \
	(st.st_mode & S_IWUSR ? 0 : FILE_ATTRIBUTE_READONLY))

#define TAG CHANNELS_TAG("drive.client")

typedef struct _DRIVE_FILE DRIVE_FILE;

struct _DRIVE_FILE
{
	UINT32 id;
	BOOL is_dir;
	int fd;
	int err;
	DIR* dir;
	char* basepath;
	char* fullpath;
	char* filename;
	char* pattern;
	BOOL delete_pending;
};

DRIVE_FILE* drive_file_new(const char* base_path, const char* path, UINT32 id,
	UINT32 DesiredAccess, UINT32 CreateDisposition, UINT32 CreateOptions);
void drive_file_free(DRIVE_FILE* file);

BOOL drive_file_seek(DRIVE_FILE* file, UINT64 Offset);
BOOL drive_file_read(DRIVE_FILE* file, BYTE* buffer, UINT32* Length);
BOOL drive_file_write(DRIVE_FILE* file, BYTE* buffer, UINT32 Length);
BOOL drive_file_query_information(DRIVE_FILE* file, UINT32 FsInformationClass, wStream* output);
BOOL drive_file_set_information(DRIVE_FILE* file, UINT32 FsInformationClass, UINT32 Length, wStream* input);
BOOL drive_file_query_directory(DRIVE_FILE* file, UINT32 FsInformationClass, BYTE InitialQuery,
	const char* path, wStream* output);
int dir_empty(const char *path);

extern UINT sys_code_page;

#endif /* FREERDP_CHANNEL_DRIVE_FILE_H */
