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

#ifndef FREERDP_CHANNEL_DRIVE_CLIENT_FILE_H
#define FREERDP_CHANNEL_DRIVE_CLIENT_FILE_H

#include <winpr/stream.h>
#include <freerdp/channels/log.h>
#include <freerdp/client/drive.h>

#define TAG CHANNELS_TAG("drive.client")

typedef struct S_DRIVE_FILE DRIVE_FILE;

BOOL drive_file_free(DRIVE_FILE* file);

WINPR_ATTR_MALLOC(drive_file_free, 1)
DRIVE_FILE* drive_file_new(rdpContext* context, const rdpDriveDriver* backend,
                           const WCHAR* base_path, const WCHAR* path, UINT32 PathWCharLength,
                           UINT32 id, UINT32 DesiredAccess, UINT32 CreateDisposition,
                           UINT32 CreateOptions, UINT32 FileAttributes, UINT32 SharedAccess);

uintptr_t drive_file_get_id(DRIVE_FILE* drive);
BOOL drive_file_is_dir_not_empty(DRIVE_FILE* drive);

char* drive_file_resolve_path(const rdpDriveDriver* backend, const char* what);
char* drive_file_resolve_name(const rdpDriveDriver* backend, const char* path,
                              const char* suggested);

BOOL drive_file_open(DRIVE_FILE* file);
BOOL drive_file_seek(DRIVE_FILE* file, UINT64 Offset);
BOOL drive_file_read(DRIVE_FILE* file, BYTE* buffer, UINT32* Length);
BOOL drive_file_write(DRIVE_FILE* file, const BYTE* buffer, UINT32 Length);
BOOL drive_file_query_information(DRIVE_FILE* file, UINT32 FsInformationClass, wStream* output);
BOOL drive_file_set_information(DRIVE_FILE* file, UINT32 FsInformationClass, UINT32 Length,
                                wStream* input);
BOOL drive_file_query_directory(DRIVE_FILE* file, UINT32 FsInformationClass, BYTE InitialQuery,
                                const WCHAR* path, UINT32 PathWCharLength, wStream* output);

#endif /* FREERDP_CHANNEL_DRIVE_FILE_H */
