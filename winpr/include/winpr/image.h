/**
 * WinPR: Windows Portable Runtime
 * Image Utils
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_IMAGE_H
#define WINPR_IMAGE_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

/*
 * Bitmap data structures
 */

struct _WINPR_BITMAP_MAGIC
{
	BYTE magic[2];
};
typedef struct _WINPR_BITMAP_MAGIC WINPR_BITMAP_MAGIC;

struct _WINPR_BITMAP_CORE_HEADER
{
	UINT32 filesz;
	UINT16 creator1;
	UINT16 creator2;
	UINT32 bmp_offset;
};
typedef struct _WINPR_BITMAP_CORE_HEADER WINPR_BITMAP_CORE_HEADER;

struct _WINPR_BITMAP_INFO_HEADER
{
	UINT32 header_sz;
	INT32 width;
	INT32 height;
	UINT16 nplanes;
	UINT16 bitspp;
	UINT32 compress_type;
	UINT32 bmp_bytesz;
	INT32 hres;
	INT32 vres;
	UINT32 ncolors;
	UINT32 nimpcolors;
};
typedef struct _WINPR_BITMAP_INFO_HEADER WINPR_BITMAP_INFO_HEADER;

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API int winpr_bitmap_write(char* filename, BYTE* data, int width, int height, int bpp);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_IMAGE_H */
