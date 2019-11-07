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

#pragma pack(push, 1)

struct _WINPR_BITMAP_FILE_HEADER
{
	BYTE bfType[2];
	UINT32 bfSize;
	UINT16 bfReserved1;
	UINT16 bfReserved2;
	UINT32 bfOffBits;
};
typedef struct _WINPR_BITMAP_FILE_HEADER WINPR_BITMAP_FILE_HEADER;

struct _WINPR_BITMAP_INFO_HEADER
{
	UINT32 biSize;
	INT32 biWidth;
	INT32 biHeight;
	UINT16 biPlanes;
	UINT16 biBitCount;
	UINT32 biCompression;
	UINT32 biSizeImage;
	INT32 biXPelsPerMeter;
	INT32 biYPelsPerMeter;
	UINT32 biClrUsed;
	UINT32 biClrImportant;
};
typedef struct _WINPR_BITMAP_INFO_HEADER WINPR_BITMAP_INFO_HEADER;

struct _WINPR_BITMAP_CORE_HEADER
{
	UINT32 bcSize;
	UINT16 bcWidth;
	UINT16 bcHeight;
	UINT16 bcPlanes;
	UINT16 bcBitCount;
};
typedef struct _WINPR_BITMAP_CORE_HEADER WINPR_BITMAP_CORE_HEADER;

#pragma pack(pop)

#define WINPR_IMAGE_BITMAP 0
#define WINPR_IMAGE_PNG 1

struct _wImage
{
	int type;
	int width;
	int height;
	BYTE* data;
	int scanline;
	int bitsPerPixel;
	int bytesPerPixel;
};
typedef struct _wImage wImage;

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API int winpr_bitmap_write(const char* filename, const BYTE* data, int width, int height,
	                                 int bpp);

	WINPR_API int winpr_image_write(wImage* image, const char* filename);
	WINPR_API int winpr_image_read(wImage* image, const char* filename);

	WINPR_API int winpr_image_read_buffer(wImage* image, const BYTE* buffer, int size);

	WINPR_API wImage* winpr_image_new();
	WINPR_API void winpr_image_free(wImage* image, BOOL bFreeBuffer);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_IMAGE_H */
