/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Persistent Bitmap Cache
 *
 * Copyright 2016 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_PERSISTENT_CACHE_H
#define FREERDP_PERSISTENT_CACHE_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/update.h>
#include <freerdp/freerdp.h>

#include <winpr/crt.h>
#include <winpr/stream.h>

typedef struct rdp_persistent_cache rdpPersistentCache;

#pragma pack(push, 1)

/* 12 bytes */

struct _PERSISTENT_CACHE_HEADER_V3
{
	BYTE sig[8];
	UINT32 flags; /* 0x00000003, 0x00000006 */
};
typedef struct _PERSISTENT_CACHE_HEADER_V3 PERSISTENT_CACHE_HEADER_V3;

/* 12 bytes */

struct _PERSISTENT_CACHE_ENTRY_V3
{
	UINT64 key64;
	UINT16 width;
	UINT16 height;
};
typedef struct _PERSISTENT_CACHE_ENTRY_V3 PERSISTENT_CACHE_ENTRY_V3;

/* 20 bytes */

struct _PERSISTENT_CACHE_ENTRY_V2
{
	UINT64 key64;
	UINT16 width;
	UINT16 height;
	UINT32 size;
	UINT32 flags; /* 0x00000011 */
};
typedef struct _PERSISTENT_CACHE_ENTRY_V2 PERSISTENT_CACHE_ENTRY_V2;

#pragma pack(pop)

struct _PERSISTENT_CACHE_ENTRY
{
	UINT64 key64;
	UINT16 width;
	UINT16 height;
	UINT32 size;
	UINT32 flags;
	BYTE* data;
};
typedef struct _PERSISTENT_CACHE_ENTRY PERSISTENT_CACHE_ENTRY;

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API int persistent_cache_get_version(rdpPersistentCache* persistent);
	FREERDP_API int persistent_cache_get_count(rdpPersistentCache* persistent);

	FREERDP_API int persistent_cache_read_entry(rdpPersistentCache* persistent,
	                                            PERSISTENT_CACHE_ENTRY* entry);
	FREERDP_API int persistent_cache_write_entry(rdpPersistentCache* persistent,
	                                             const PERSISTENT_CACHE_ENTRY* entry);

	FREERDP_API int persistent_cache_open(rdpPersistentCache* persistent, const char* filename,
	                                      BOOL write, UINT32 version);
	FREERDP_API int persistent_cache_close(rdpPersistentCache* persistent);

	FREERDP_API rdpPersistentCache* persistent_cache_new(void);
	FREERDP_API void persistent_cache_free(rdpPersistentCache* persistent);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_PERSISTENT_CACHE_H */
