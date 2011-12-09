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
#include <sys/stat.h>
#include <dirent.h>

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
