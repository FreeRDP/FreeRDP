/**
 * WinPR: Windows Portable Runtime
 * Image Utils
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/wtypes.h>
#include <winpr/crt.h>

#include <winpr/image.h>

#include "lodepng/lodepng.h"

#include "../log.h"
#define TAG WINPR_TAG("utils.image")

/**
 * Refer to "Compressed Image File Formats: JPEG, PNG, GIF, XBM, BMP" book
 */

int winpr_bitmap_write(const char* filename, BYTE* data, int width, int height, int bpp)
{
	FILE* fp;
	WINPR_BITMAP_FILE_HEADER bf;
	WINPR_BITMAP_INFO_HEADER bi;
	int ret = 1;

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

	if (fwrite((void*) &bf, sizeof(WINPR_BITMAP_FILE_HEADER), 1, fp) != 1 ||
			fwrite((void*) &bi, sizeof(WINPR_BITMAP_INFO_HEADER), 1, fp) != 1 ||
			fwrite((void*) data, bi.biSizeImage, 1, fp) != 1)
		ret = -1;

	fclose(fp);

	return ret;
}

int winpr_image_write(wImage* image, const char* filename)
{
	int status = -1;

	if (image->type == WINPR_IMAGE_BITMAP)
	{
		status = winpr_bitmap_write(filename, image->data, image->width, image->height, image->bitsPerPixel);
	}
	else
	{
		int lodepng_status;

		lodepng_status = lodepng_encode32_file(filename, image->data, image->width, image->height);

		status = (lodepng_status) ? -1 : 1;
	}

	return status;
}

int winpr_image_png_read_fp(wImage* image, FILE* fp)
{
	INT64 size;
	BYTE* data;
	UINT32 width;
	UINT32 height;
	int lodepng_status;

	_fseeki64(fp, 0, SEEK_END);
	size = _ftelli64(fp);
	_fseeki64(fp, 0, SEEK_SET);

	data = (BYTE*) malloc(size);

	if (!data)
		return -1;

	if (fread((void*) data, size, 1, fp) != 1)
	{
		free(data);
		return -1;
	}

	lodepng_status = lodepng_decode32(&(image->data), &width, &height, data, size);

	free(data);

	if (lodepng_status)
		return -1;

	image->width = width;
	image->height = height;

	image->bitsPerPixel = 32;
	image->bytesPerPixel = 4;
	image->scanline = image->bytesPerPixel * image->width;

	return 1;
}

int winpr_image_png_read_buffer(wImage* image, BYTE* buffer, int size)
{
	UINT32 width;
	UINT32 height;
	int lodepng_status;

	lodepng_status = lodepng_decode32(&(image->data), &width, &height, buffer, size);

	if (lodepng_status)
		return -1;

	image->width = width;
	image->height = height;

	image->bitsPerPixel = 32;
	image->bytesPerPixel = 4;
	image->scanline = image->bytesPerPixel * image->width;

	return 1;
}

int winpr_image_bitmap_read_fp(wImage* image, FILE* fp)
{
	int index;
	BOOL vFlip;
	BYTE* pDstData;
	WINPR_BITMAP_FILE_HEADER bf;
	WINPR_BITMAP_INFO_HEADER bi;

	if (fread((void*) &bf, sizeof(WINPR_BITMAP_FILE_HEADER), 1, fp) != 1)
		return -1;

	if ((bf.bfType[0] != 'B') || (bf.bfType[1] != 'M'))
		return -1;

	image->type = WINPR_IMAGE_BITMAP;

	if (fread((void*) &bi, sizeof(WINPR_BITMAP_INFO_HEADER), 1, fp) != 1)
		return -1;

	if (_ftelli64(fp) != bf.bfOffBits)
	{
		_fseeki64(fp, bf.bfOffBits, SEEK_SET);
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
	image->scanline = (bi.biSizeImage / image->height);

	image->data = (BYTE*) malloc(bi.biSizeImage);

	if (!image->data)
		return -1;

	if (!vFlip)
	{
		if (fread((void*) image->data, bi.biSizeImage, 1, fp) != 1)
		{
			free(image->data);
			image->data = NULL;
			return -1;
		}
	}
	else
	{
		pDstData = &(image->data[(image->height - 1) * image->scanline]);

		for (index = 0; index < image->height; index++)
		{
			if (fread((void*) pDstData, image->scanline, 1, fp) != 1)
			{
				free(image->data);
				image->data = NULL;
				return -1;
			}
			pDstData -= image->scanline;
		}
	}

	return 1;
}

int winpr_image_bitmap_read_buffer(wImage* image, BYTE* buffer, int size)
{
	int index;
	BOOL vFlip;
	BYTE* pSrcData;
	BYTE* pDstData;
	WINPR_BITMAP_FILE_HEADER bf;
	WINPR_BITMAP_INFO_HEADER bi;

	pSrcData = buffer;

	CopyMemory(&bf, pSrcData, sizeof(WINPR_BITMAP_FILE_HEADER));
	pSrcData += sizeof(WINPR_BITMAP_FILE_HEADER);

	if ((bf.bfType[0] != 'B') || (bf.bfType[1] != 'M'))
		return -1;

	image->type = WINPR_IMAGE_BITMAP;

	CopyMemory(&bi, pSrcData, sizeof(WINPR_BITMAP_INFO_HEADER));
	pSrcData += sizeof(WINPR_BITMAP_INFO_HEADER);

	if ((pSrcData - buffer) != bf.bfOffBits)
	{
		pSrcData = &buffer[bf.bfOffBits];
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
	image->scanline = (bi.biSizeImage / image->height);

	image->data = (BYTE*) malloc(bi.biSizeImage);

	if (!image->data)
		return -1;

	if (!vFlip)
		CopyMemory(image->data, pSrcData, bi.biSizeImage);
	else
	{
		pDstData = &(image->data[(image->height - 1) * image->scanline]);

		for (index = 0; index < image->height; index++)
		{
			CopyMemory(pDstData, pSrcData, image->scanline);
			pSrcData += image->scanline;
			pDstData -= image->scanline;
		}
	}

	return 1;
}

int winpr_image_read(wImage* image, const char* filename)
{
	FILE* fp;
	BYTE sig[8];
	int status = -1;

	fp = fopen(filename, "rb");

	if (!fp)
	{
		WLog_ERR(TAG, "failed to open file %s", filename);
		return -1;
	}

	if (fread((void*) &sig, sizeof(sig), 1, fp) != 1 || _fseeki64(fp, 0, SEEK_SET) < 0)
	{
		fclose(fp);
		return -1;
	}

	if ((sig[0] == 'B') && (sig[1] == 'M'))
	{
		image->type = WINPR_IMAGE_BITMAP;
		status = winpr_image_bitmap_read_fp(image, fp);
	}
	else if ((sig[0] == 0x89) && (sig[1] == 'P') && (sig[2] == 'N') && (sig[3] == 'G') &&
			(sig[4] == '\r') && (sig[5] == '\n') && (sig[6] == 0x1A) && (sig[7] == '\n'))
	{
		image->type = WINPR_IMAGE_PNG;
		status = winpr_image_png_read_fp(image, fp);
	}
	fclose(fp);

	return status;
}

int winpr_image_read_buffer(wImage* image, BYTE* buffer, int size)
{
	BYTE sig[8];
	int status = -1;

	if (size < 8)
		return -1;

	CopyMemory(sig, buffer, 8);

	if ((sig[0] == 'B') && (sig[1] == 'M'))
	{
		image->type = WINPR_IMAGE_BITMAP;
		status = winpr_image_bitmap_read_buffer(image, buffer, size);
	}
	else if ((sig[0] == 0x89) && (sig[1] == 'P') && (sig[2] == 'N') && (sig[3] == 'G') &&
			(sig[4] == '\r') && (sig[5] == '\n') && (sig[6] == 0x1A) && (sig[7] == '\n'))
	{
		image->type = WINPR_IMAGE_PNG;
		status = winpr_image_png_read_buffer(image, buffer, size);
	}

	return status;
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
