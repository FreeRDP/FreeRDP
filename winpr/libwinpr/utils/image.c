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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include <winpr/image.h>

#include "../log.h"
#define TAG WINPR_TAG("utils.image")

/**
 * Refer to "Compressed Image File Formats: JPEG, PNG, GIF, XBM, BMP" book
 */

#if defined(__APPLE__)
#pragma pack(1)
#else
#pragma pack(push, 1)
#endif

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

#if defined(__APPLE__)
#pragma pack()
#else
#pragma pack(pop)
#endif

int winpr_bitmap_write(const char* filename, BYTE* data, int width, int height, int bpp)
{
	FILE* fp;
	WINPR_BITMAP_FILE_HEADER bf;
	WINPR_BITMAP_INFO_HEADER bi;

	fp = fopen(filename, "w+b");

	if (!fp)
	{
		WLog_ERR(TAG, "failed to open file %s", filename);
		return -1;
	}

	bf.bfType[0] = 'B';
	bf.bfType[1] = 'M';
	bf.bfReserved1 = 0;
	bf.bfReserved2 = 0;
	bf.bfOffBits = sizeof(WINPR_BITMAP_FILE_HEADER) + sizeof(WINPR_BITMAP_INFO_HEADER);
	bi.biSizeImage = width * height * (bpp / 8);
	bf.bfSize = bf.bfOffBits + bi.biSizeImage;

	bi.biWidth = width;
	bi.biHeight = -1 * height;
	bi.biPlanes = 1;
	bi.biBitCount = bpp;
	bi.biCompression = 0;
	bi.biXPelsPerMeter = width;
	bi.biYPelsPerMeter = height;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;
	bi.biSize = sizeof(WINPR_BITMAP_INFO_HEADER);

	fwrite((void*) &bf, sizeof(WINPR_BITMAP_FILE_HEADER), 1, fp);
	fwrite((void*) &bi, sizeof(WINPR_BITMAP_INFO_HEADER), 1, fp);
	fwrite((void*) data, bi.biSizeImage, 1, fp);

	fclose(fp);

	return 1;
}

int winpr_image_write(wImage* image, const char* filename)
{
	return winpr_bitmap_write(filename, image->data, image->width, image->height, image->bitsPerPixel);
}

int winpr_image_read(wImage* image, const char* filename)
{
	FILE* fp;
	int index;
	BOOL vFlip;
	BYTE* pDstData;
	WINPR_BITMAP_FILE_HEADER bf;
	WINPR_BITMAP_INFO_HEADER bi;

	fp = fopen(filename, "r+b");

	if (!fp)
	{
		WLog_ERR(TAG, "failed to open file %s", filename);
		return -1;
	}

	fread((void*) &bf, sizeof(WINPR_BITMAP_FILE_HEADER), 1, fp);

	if ((bf.bfType[0] != 'B') || (bf.bfType[1] != 'M'))
		return -1;

	fread((void*) &bi, sizeof(WINPR_BITMAP_INFO_HEADER), 1, fp);

	if (ftell(fp) != bf.bfOffBits)
	{
		fseek(fp, bf.bfOffBits, SEEK_SET);
	}

	image->width = bi.biWidth;

	if (bi.biHeight < 0)
	{
		vFlip = FALSE;
		image->height = -1 * bi.biHeight;
	}
	else
	{
		vFlip = TRUE;
		image->height = bi.biHeight;
	}

	image->bitsPerPixel = bi.biBitCount;
	image->bytesPerPixel = (image->bitsPerPixel / 8);
	image->scanline = (bi.biSizeImage / bi.biHeight);

	image->data = (BYTE*) malloc(bi.biSizeImage);

	if (!image->data)
		return -1;

	if (!vFlip)
	{
		fread((void*) image->data, bi.biSizeImage, 1, fp);
	}
	else
	{
		pDstData = &(image->data[(image->height - 1) * image->scanline]);

		for (index = 0; index < image->height; index++)
		{
			fread((void*) pDstData, image->scanline, 1, fp);
			pDstData -= image->scanline;
		}
	}

	fclose(fp);

	return 1;
}

wImage* winpr_image_new()
{
	wImage* image;

	image = (wImage*) calloc(1, sizeof(wImage));

	if (!image)
		return NULL;

	return image;
}

void winpr_image_free(wImage* image, BOOL bFreeBuffer)
{
	if (!image)
		return;

	if (bFreeBuffer)
		free(image->data);

	free(image);
}
