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

#include <winpr/config.h>

#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/file.h>

#include <winpr/image.h>

#include "lodepng/lodepng.h"
#include <winpr/stream.h>

#include "../log.h"
#define TAG WINPR_TAG("utils.image")

static BOOL writeBitmapFileHeader(wStream* s, const WINPR_BITMAP_FILE_HEADER* bf)
{
	if (!Stream_EnsureRemainingCapacity(s, sizeof(WINPR_BITMAP_FILE_HEADER)))
		return FALSE;

	Stream_Write_UINT8(s, bf->bfType[0]);
	Stream_Write_UINT8(s, bf->bfType[1]);
	Stream_Write_UINT32(s, bf->bfSize);
	Stream_Write_UINT16(s, bf->bfReserved1);
	Stream_Write_UINT16(s, bf->bfReserved2);
	Stream_Write_UINT32(s, bf->bfOffBits);
	return TRUE;
}

static BOOL readBitmapFileHeader(wStream* s, WINPR_BITMAP_FILE_HEADER* bf)
{
	if (!s || !bf || (!Stream_CheckAndLogRequiredLength(TAG, s, sizeof(WINPR_BITMAP_FILE_HEADER))))
		return FALSE;

	Stream_Read_UINT8(s, bf->bfType[0]);
	Stream_Read_UINT8(s, bf->bfType[1]);
	Stream_Read_UINT32(s, bf->bfSize);
	Stream_Read_UINT16(s, bf->bfReserved1);
	Stream_Read_UINT16(s, bf->bfReserved2);
	Stream_Read_UINT32(s, bf->bfOffBits);
	return TRUE;
}

static BOOL writeBitmapInfoHeader(wStream* s, const WINPR_BITMAP_INFO_HEADER* bi)
{
	if (!Stream_EnsureRemainingCapacity(s, sizeof(WINPR_BITMAP_INFO_HEADER)))
		return FALSE;

	Stream_Write_UINT32(s, bi->biSize);
	Stream_Write_INT32(s, bi->biWidth);
	Stream_Write_INT32(s, bi->biHeight);
	Stream_Write_UINT16(s, bi->biPlanes);
	Stream_Write_UINT16(s, bi->biBitCount);
	Stream_Write_UINT32(s, bi->biCompression);
	Stream_Write_UINT32(s, bi->biSizeImage);
	Stream_Write_INT32(s, bi->biXPelsPerMeter);
	Stream_Write_INT32(s, bi->biYPelsPerMeter);
	Stream_Write_UINT32(s, bi->biClrUsed);
	Stream_Write_UINT32(s, bi->biClrImportant);
	return TRUE;
}

static BOOL readBitmapInfoHeader(wStream* s, WINPR_BITMAP_INFO_HEADER* bi)
{
	if (!s || !bi || (!Stream_CheckAndLogRequiredLength(TAG, s, sizeof(WINPR_BITMAP_INFO_HEADER))))
		return FALSE;

	Stream_Read_UINT32(s, bi->biSize);
	Stream_Read_INT32(s, bi->biWidth);
	Stream_Read_INT32(s, bi->biHeight);
	Stream_Read_UINT16(s, bi->biPlanes);
	Stream_Read_UINT16(s, bi->biBitCount);
	Stream_Read_UINT32(s, bi->biCompression);
	Stream_Read_UINT32(s, bi->biSizeImage);
	Stream_Read_INT32(s, bi->biXPelsPerMeter);
	Stream_Read_INT32(s, bi->biYPelsPerMeter);
	Stream_Read_UINT32(s, bi->biClrUsed);
	Stream_Read_UINT32(s, bi->biClrImportant);
	return TRUE;
}

BYTE* winpr_bitmap_construct_header(size_t width, size_t height, size_t bpp)
{
	BYTE* result = NULL;
	WINPR_BITMAP_FILE_HEADER bf;
	WINPR_BITMAP_INFO_HEADER bi;
	wStream* s;
	size_t imgSize;

	imgSize = width * height * (bpp / 8);
	if ((width > INT32_MAX) || (height > INT32_MAX) || (bpp > UINT16_MAX) || (imgSize > UINT32_MAX))
		return NULL;

	s = Stream_New(NULL, WINPR_IMAGE_BMP_HEADER_LEN);
	if (!s)
		return NULL;

	bf.bfType[0] = 'B';
	bf.bfType[1] = 'M';
	bf.bfReserved1 = 0;
	bf.bfReserved2 = 0;
	bf.bfOffBits = (UINT32)sizeof(WINPR_BITMAP_FILE_HEADER) + sizeof(WINPR_BITMAP_INFO_HEADER);
	bi.biSizeImage = (UINT32)imgSize;
	bf.bfSize = bf.bfOffBits + bi.biSizeImage;
	bi.biWidth = (INT32)width;
	bi.biHeight = -1 * (INT32)height;
	bi.biPlanes = 1;
	bi.biBitCount = (UINT16)bpp;
	bi.biCompression = 0;
	bi.biXPelsPerMeter = (INT32)width;
	bi.biYPelsPerMeter = (INT32)height;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;
	bi.biSize = (UINT32)sizeof(WINPR_BITMAP_INFO_HEADER);

	if (!writeBitmapFileHeader(s, &bf))
		goto fail;

	if (!writeBitmapInfoHeader(s, &bi))
		goto fail;

	result = Stream_Buffer(s);
fail:
	Stream_Free(s, result == 0);
	return result;
}

/**
 * Refer to "Compressed Image File Formats: JPEG, PNG, GIF, XBM, BMP" book
 */

int winpr_bitmap_write(const char* filename, const BYTE* data, size_t width, size_t height,
                       size_t bpp)
{
	FILE* fp;
	BYTE* bmp_header = NULL;
	size_t img_size = width * height * (bpp / 8);

	int ret = -1;
	fp = winpr_fopen(filename, "w+b");

	if (!fp)
	{
		WLog_ERR(TAG, "failed to open file %s", filename);
		return -1;
	}

	bmp_header = winpr_bitmap_construct_header(width, height, bpp);
	if (!bmp_header)
		goto fail;

	if (fwrite(bmp_header, WINPR_IMAGE_BMP_HEADER_LEN, 1, fp) != 1 ||
	    fwrite((const void*)data, img_size, 1, fp) != 1)
		goto fail;

	ret = 1;
fail:
	fclose(fp);
	free(bmp_header);
	return ret;
}

int winpr_image_write(wImage* image, const char* filename)
{
	int status = -1;

	if (image->type == WINPR_IMAGE_BITMAP)
	{
		status = winpr_bitmap_write(filename, image->data, image->width, image->height,
		                            image->bitsPerPixel);
	}
	else
	{
		unsigned lodepng_status;
		lodepng_status = lodepng_encode32_file(filename, image->data, image->width, image->height);
		status = (lodepng_status) ? -1 : 1;
	}

	return status;
}

static int winpr_image_png_read_fp(wImage* image, FILE* fp)
{
	INT64 size;
	BYTE* data;
	UINT32 width;
	UINT32 height;
	unsigned lodepng_status;
	_fseeki64(fp, 0, SEEK_END);
	size = _ftelli64(fp);
	_fseeki64(fp, 0, SEEK_SET);
	if (size < 0)
		return -1;

	data = (BYTE*)malloc((size_t)size);

	if (!data)
		return -1;

	if (fread((void*)data, (size_t)size, 1, fp) != 1)
	{
		free(data);
		return -1;
	}

	lodepng_status = lodepng_decode32(&(image->data), &width, &height, data, (size_t)size);
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

static int winpr_image_png_read_buffer(wImage* image, const BYTE* buffer, size_t size)
{
	UINT32 width;
	UINT32 height;
	unsigned lodepng_status = lodepng_decode32(&(image->data), &width, &height, buffer, size);

	if (lodepng_status)
		return -1;

	image->width = width;
	image->height = height;
	image->bitsPerPixel = 32;
	image->bytesPerPixel = 4;
	image->scanline = image->bytesPerPixel * image->width;
	return 1;
}

static int winpr_image_bitmap_read_fp(wImage* image, FILE* fp)
{
	int rc = -1;
	UINT32 index;
	BOOL vFlip;
	BYTE* pDstData;
	wStream* s;
	WINPR_BITMAP_FILE_HEADER bf;
	WINPR_BITMAP_INFO_HEADER bi;

	if (!image || !fp)
		return -1;

	image->data = NULL;

	s = Stream_New(NULL, sizeof(WINPR_BITMAP_FILE_HEADER) + sizeof(WINPR_BITMAP_INFO_HEADER));

	if (!s)
		return -1;

	if (fread(Stream_Buffer(s), Stream_Capacity(s), 1, fp) != 1)
		goto fail;

	if (!readBitmapFileHeader(s, &bf) || !readBitmapInfoHeader(s, &bi))
		goto fail;

	if ((bf.bfType[0] != 'B') || (bf.bfType[1] != 'M'))
		goto fail;

	image->type = WINPR_IMAGE_BITMAP;

	if (_ftelli64(fp) != bf.bfOffBits)
		_fseeki64(fp, bf.bfOffBits, SEEK_SET);

	if (bi.biWidth < 0)
		goto fail;

	image->width = (UINT32)bi.biWidth;

	if (bi.biHeight < 0)
	{
		vFlip = FALSE;
		image->height = (UINT32)(-1 * bi.biHeight);
	}
	else
	{
		vFlip = TRUE;
		image->height = (UINT32)bi.biHeight;
	}

	image->bitsPerPixel = bi.biBitCount;
	image->bytesPerPixel = (image->bitsPerPixel / 8);
	image->scanline = (bi.biSizeImage / image->height);
	image->data = (BYTE*)malloc(bi.biSizeImage);

	if (!image->data)
		goto fail;

	if (!vFlip)
	{
		if (fread((void*)image->data, bi.biSizeImage, 1, fp) != 1)
			goto fail;
	}
	else
	{
		pDstData = &(image->data[(image->height - 1) * image->scanline]);

		for (index = 0; index < image->height; index++)
		{
			if (fread((void*)pDstData, image->scanline, 1, fp) != 1)
				goto fail;

			pDstData -= image->scanline;
		}
	}

	rc = 1;
fail:

	if (rc < 0)
	{
		free(image->data);
		image->data = NULL;
	}

	Stream_Free(s, TRUE);
	return 1;
}

static int winpr_image_bitmap_read_buffer(wImage* image, const BYTE* buffer, size_t size)
{
	int rc = -1;
	UINT32 index;
	BOOL vFlip;
	BYTE* pDstData;
	WINPR_BITMAP_FILE_HEADER bf;
	WINPR_BITMAP_INFO_HEADER bi;
	wStream sbuffer = { 0 };
	wStream* s = Stream_StaticConstInit(&sbuffer, buffer, size);

	if (!s)
		return -1;

	if (!readBitmapFileHeader(s, &bf) || !readBitmapInfoHeader(s, &bi))
		goto fail;

	if ((bf.bfType[0] != 'B') || (bf.bfType[1] != 'M'))
		goto fail;

	image->type = WINPR_IMAGE_BITMAP;

	if (Stream_GetPosition(s) > bf.bfOffBits)
		goto fail;
	if (!Stream_SafeSeek(s, bf.bfOffBits - Stream_GetPosition(s)))
		goto fail;
	if (Stream_GetRemainingCapacity(s) < bi.biSizeImage)
		goto fail;

	if (bi.biWidth < 0)
		goto fail;

	image->width = (UINT32)bi.biWidth;

	if (bi.biHeight < 0)
	{
		vFlip = FALSE;
		image->height = (UINT32)(-1 * bi.biHeight);
	}
	else
	{
		vFlip = TRUE;
		image->height = (UINT32)bi.biHeight;
	}

	image->bitsPerPixel = bi.biBitCount;
	image->bytesPerPixel = (image->bitsPerPixel / 8);
	image->scanline = (bi.biSizeImage / image->height);
	image->data = (BYTE*)malloc(bi.biSizeImage);

	if (!image->data)
		goto fail;

	if (!vFlip)
		Stream_Read(s, image->data, bi.biSizeImage);
	else
	{
		pDstData = &(image->data[(image->height - 1) * image->scanline]);

		for (index = 0; index < image->height; index++)
		{
			Stream_Read(s, pDstData, image->scanline);
			pDstData -= image->scanline;
		}
	}

	rc = 1;
fail:

	if (rc < 0)
	{
		free(image->data);
		image->data = NULL;
	}

	return rc;
}

int winpr_image_read(wImage* image, const char* filename)
{
	FILE* fp;
	BYTE sig[8];
	int status = -1;

	fp = winpr_fopen(filename, "rb");

	if (!fp)
	{
		WLog_ERR(TAG, "failed to open file %s", filename);
		return -1;
	}

	if (fread((void*)&sig, sizeof(sig), 1, fp) != 1 || _fseeki64(fp, 0, SEEK_SET) < 0)
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

int winpr_image_read_buffer(wImage* image, const BYTE* buffer, size_t size)
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

wImage* winpr_image_new(void)
{
	wImage* image;
	image = (wImage*)calloc(1, sizeof(wImage));

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
