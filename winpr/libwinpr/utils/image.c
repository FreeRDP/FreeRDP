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

#include <stdlib.h>

#include <winpr/config.h>

#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/file.h>

#include <winpr/image.h>

#include "image.h"

#if defined(WINPR_UTILS_IMAGE_PNG)
#include <png.h>
#endif

#if defined(WINPR_UTILS_IMAGE_JPEG)
#define INT32 INT32_WINPR
#include <jpeglib.h>
#undef INT32
#endif

#if defined(WINPR_UTILS_IMAGE_WEBP)
#include <webp/encode.h>
#include <webp/decode.h>
#endif

#if defined(WITH_LODEPNG)
#include <lodepng.h>
#endif
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
	WINPR_BITMAP_FILE_HEADER bf = { 0 };
	WINPR_BITMAP_INFO_HEADER bi = { 0 };
	wStream* s = NULL;
	size_t imgSize = 0;

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

static void* winpr_bitmap_write_buffer(const BYTE* data, size_t size, UINT32 width, UINT32 height,
                                       UINT32 stride, UINT32 bpp, UINT32* pSize)
{
	WINPR_ASSERT(data || (size == 0));

	void* result = NULL;
	const size_t bpp_stride = 1ull * width * (bpp / 8);
	wStream* s = Stream_New(NULL, 1024);

	if (stride == 0)
		stride = bpp_stride;

	BYTE* bmp_header = winpr_bitmap_construct_header(width, height, bpp);
	if (!bmp_header)
		goto fail;
	if (!Stream_EnsureRemainingCapacity(s, WINPR_IMAGE_BMP_HEADER_LEN))
		goto fail;
	Stream_Write(s, bmp_header, WINPR_IMAGE_BMP_HEADER_LEN);

	if (!Stream_EnsureRemainingCapacity(s, stride * height * 1ull))
		goto fail;

	for (size_t y = 0; y < height; y++)
	{
		const BYTE* line = &data[stride * y];

		Stream_Write(s, line, stride);
	}

	result = Stream_Buffer(s);
	*pSize = Stream_GetPosition(s);
fail:
	Stream_Free(s, result == NULL);
	free(bmp_header);
	return result;
}

int winpr_bitmap_write(const char* filename, const BYTE* data, size_t width, size_t height,
                       size_t bpp)
{
	return winpr_bitmap_write_ex(filename, data, 0, width, height, bpp);
}

int winpr_bitmap_write_ex(const char* filename, const BYTE* data, size_t stride, size_t width,
                          size_t height, size_t bpp)
{
	FILE* fp = NULL;
	int ret = -1;
	const size_t bpp_stride = width * (bpp / 8);

	if (stride == 0)
		stride = bpp_stride;

	UINT32 bmpsize = 0;
	const size_t size = stride * 1ull * height;
	void* bmpdata = winpr_bitmap_write_buffer(data, size, width, height, stride, bpp, &bmpsize);
	if (!bmpdata)
		goto fail;

	fp = winpr_fopen(filename, "w+b");
	if (!fp)
	{
		WLog_ERR(TAG, "failed to open file %s", filename);
		goto fail;
	}

	if (fwrite(bmpdata, bmpsize, 1, fp) != 1)
		goto fail;

fail:
	if (fp)
		fclose(fp);
	free(bmpdata);
	return ret;
}

static int write_and_free(const char* filename, void* data, size_t size)
{
	int status = -1;
	if (!data)
		goto fail;

	FILE* fp = winpr_fopen(filename, "w+b");
	if (!fp)
		goto fail;

	size_t w = fwrite(data, 1, size, fp);
	fclose(fp);

	status = (w == size) ? 1 : -1;
fail:
	free(data);
	return status;
}

int winpr_image_write(wImage* image, const char* filename)
{
	WINPR_ASSERT(image);
	return winpr_image_write_ex(image, image->type, filename);
}

int winpr_image_write_ex(wImage* image, UINT32 format, const char* filename)
{
	WINPR_ASSERT(image);

	size_t size = 0;
	void* data = winpr_image_write_buffer(image, format, &size);
	if (!data)
		return -1;
	return write_and_free(filename, data, size);
}

static int winpr_image_bitmap_read_buffer(wImage* image, const BYTE* buffer, size_t size)
{
	int rc = -1;
	UINT32 index = 0;
	BOOL vFlip = 0;
	BYTE* pDstData = NULL;
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
	if (!Stream_CheckAndLogRequiredCapacity(TAG, s, bi.biSizeImage))
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
	int status = -1;

	FILE* fp = winpr_fopen(filename, "rb");
	if (!fp)
	{
		WLog_ERR(TAG, "failed to open file %s", filename);
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	INT64 pos = _ftelli64(fp);
	fseek(fp, 0, SEEK_SET);

	if (pos > 0)
	{
		char* buffer = malloc((size_t)pos);
		if (buffer)
		{
			size_t r = fread(buffer, 1, (size_t)pos, fp);
			if (r == (size_t)pos)
			{
				status = winpr_image_read_buffer(image, buffer, (size_t)pos);
			}
		}
		free(buffer);
	}
	fclose(fp);
	return status;
}

int winpr_image_read_buffer(wImage* image, const BYTE* buffer, size_t size)
{
	BYTE sig[12] = { 0 };
	int status = -1;

	if (size < sizeof(sig))
		return -1;

	CopyMemory(sig, buffer, sizeof(sig));

	if ((sig[0] == 'B') && (sig[1] == 'M'))
	{
		image->type = WINPR_IMAGE_BITMAP;
		status = winpr_image_bitmap_read_buffer(image, buffer, size);
	}
	else if ((sig[0] == 'R') && (sig[1] == 'I') && (sig[2] == 'F') && (sig[3] == 'F') &&
	         (sig[8] == 'W') && (sig[9] == 'E') && (sig[10] == 'B') && (sig[11] == 'P'))
	{
		image->type = WINPR_IMAGE_WEBP;
		const SSIZE_T rc =
		    winpr_convert_from_webp((const char*)buffer, size, &image->width, &image->height,
		                            &image->bitsPerPixel, ((char**)&image->data));
		if (rc >= 0)
		{
			image->bytesPerPixel = (image->bitsPerPixel + 7) / 8;
			image->scanline = image->width * image->bytesPerPixel;
			status = 1;
		}
	}
	else if ((sig[0] == 0xFF) && (sig[1] == 0xD8) && (sig[2] == 0xFF) && (sig[3] == 0xE0) &&
	         (sig[6] == 0x4A) && (sig[7] == 0x46) && (sig[8] == 0x49) && (sig[9] == 0x46) &&
	         (sig[10] == 0x00))
	{
		image->type = WINPR_IMAGE_JPEG;
		const SSIZE_T rc =
		    winpr_convert_from_jpeg((const char*)buffer, size, &image->width, &image->height,
		                            &image->bitsPerPixel, ((char**)&image->data));
		if (rc >= 0)
		{
			image->bytesPerPixel = (image->bitsPerPixel + 7) / 8;
			image->scanline = image->width * image->bytesPerPixel;
			status = 1;
		}
	}
	else if ((sig[0] == 0x89) && (sig[1] == 'P') && (sig[2] == 'N') && (sig[3] == 'G') &&
	         (sig[4] == '\r') && (sig[5] == '\n') && (sig[6] == 0x1A) && (sig[7] == '\n'))
	{
		image->type = WINPR_IMAGE_PNG;
		const SSIZE_T rc =
		    winpr_convert_from_png((const char*)buffer, size, &image->width, &image->height,
		                           &image->bitsPerPixel, ((char**)&image->data));
		if (rc >= 0)
		{
			image->bytesPerPixel = (image->bitsPerPixel + 7) / 8;
			image->scanline = image->width * image->bytesPerPixel;
			status = 1;
		}
	}

	return status;
}

wImage* winpr_image_new(void)
{
	wImage* image = (wImage*)calloc(1, sizeof(wImage));

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

void* winpr_convert_to_jpeg(const void* data, size_t size, UINT32 width, UINT32 height,
                            UINT32 stride, UINT32 bpp, UINT32* pSize)
{
	WINPR_ASSERT(data || (size == 0));
	WINPR_ASSERT(pSize);

	*pSize = 0;

#if !defined(WINPR_UTILS_IMAGE_JPEG)
	return NULL;
#else
	BYTE* outbuffer = NULL;
	unsigned long outsize = 0;
	struct jpeg_compress_struct cinfo = { 0 };

	const size_t expect1 = stride * height;
	const size_t bytes = (bpp + 7) / 8;
	const size_t expect2 = width * height * bytes;
	if (expect1 != expect2)
		return NULL;
	if (expect1 != size)
		return NULL;

	/* Set up the error handler. */
	struct jpeg_error_mgr jerr = { 0 };
	cinfo.err = jpeg_std_error(&jerr);

	jpeg_create_compress(&cinfo);
	jpeg_mem_dest(&cinfo, &outbuffer, &outsize);

	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.input_components = (bpp + 7) / 8;
	cinfo.in_color_space = (bpp > 24) ? JCS_EXT_BGRA : JCS_EXT_BGR;
	cinfo.data_precision = 8;

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, 100, TRUE);
	/* Use 4:4:4 subsampling (default is 4:2:0) */
	cinfo.comp_info[0].h_samp_factor = cinfo.comp_info[0].v_samp_factor = 1;

	jpeg_start_compress(&cinfo, TRUE);

	const unsigned char* cdata = data;
	for (size_t x = 0; x < height; x++)
	{
		const JDIMENSION offset = x * stride;
		const JSAMPROW coffset = &cdata[offset];
		if (jpeg_write_scanlines(&cinfo, &coffset, 1) != 1)
			goto fail;
	}

fail:
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	*pSize = outsize;
	return outbuffer;
#endif
}

SSIZE_T winpr_convert_from_jpeg(const char* comp_data, size_t comp_data_bytes, UINT32* width,
                                UINT32* height, UINT32* bpp, char** ppdecomp_data)
{
	WINPR_ASSERT(comp_data || (comp_data_bytes == 0));
	WINPR_ASSERT(width);
	WINPR_ASSERT(height);
	WINPR_ASSERT(bpp);
	WINPR_ASSERT(ppdecomp_data);

#if !defined(WINPR_UTILS_IMAGE_JPEG)
	return -1;
#else
	struct jpeg_decompress_struct cinfo = { 0 };
	struct jpeg_error_mgr jerr;
	SSIZE_T size = -1;
	char* decomp_data = NULL;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	jpeg_mem_src(&cinfo, comp_data, comp_data_bytes);

	if (jpeg_read_header(&cinfo, 1) != JPEG_HEADER_OK)
		goto fail;

	cinfo.out_color_space = cinfo.num_components > 3 ? JCS_EXT_RGBA : JCS_EXT_BGR;

	*width = cinfo.image_width;
	*height = cinfo.image_height;
	*bpp = cinfo.num_components * 8;

	if (!jpeg_start_decompress(&cinfo))
		goto fail;

	size_t stride = cinfo.image_width * cinfo.num_components;

	decomp_data = calloc(stride, cinfo.image_height);
	if (decomp_data)
	{
		while (cinfo.output_scanline < cinfo.image_height)
		{
			JSAMPROW row = &decomp_data[cinfo.output_scanline * stride];
			if (jpeg_read_scanlines(&cinfo, &row, 1) != 1)
				goto fail;
		}
		size = stride * cinfo.image_height;
	}
	jpeg_finish_decompress(&cinfo);

fail:
	jpeg_destroy_decompress(&cinfo);
	*ppdecomp_data = decomp_data;
	return size;
#endif
}

void* winpr_convert_to_webp(const void* data, size_t size, UINT32 width, UINT32 height,
                            UINT32 stride, UINT32 bpp, UINT32* pSize)
{
	WINPR_ASSERT(data || (size == 0));
	WINPR_ASSERT(pSize);

	*pSize = 0;

#if !defined(WINPR_UTILS_IMAGE_WEBP)
	return NULL;
#else
	size_t dstSize = 0;
	uint8_t* pDstData = NULL;
	switch (bpp)
	{
		case 32:
			dstSize = WebPEncodeLosslessBGRA(data, width, height, stride, &pDstData);
			break;
		case 24:
			dstSize = WebPEncodeLosslessBGR(data, width, height, stride, &pDstData);
			break;
		default:
			return NULL;
	}

	void* rc = malloc(dstSize);
	if (rc)
	{
		memcpy(rc, pDstData, dstSize);
		*pSize = dstSize;
	}
	WebPFree(pDstData);
	return rc;
#endif
}

SSIZE_T winpr_convert_from_webp(const char* comp_data, size_t comp_data_bytes, UINT32* width,
                                UINT32* height, UINT32* bpp, char** ppdecomp_data)
{
	WINPR_ASSERT(comp_data || (comp_data_bytes == 0));
	WINPR_ASSERT(width);
	WINPR_ASSERT(height);
	WINPR_ASSERT(bpp);
	WINPR_ASSERT(ppdecomp_data);

	*width = 0;
	*height = 0;
	*bpp = 0;
	*ppdecomp_data = NULL;
#if !defined(WINPR_UTILS_IMAGE_WEBP)
	return -1;
#else

	uint8_t* dst = WebPDecodeBGRA(comp_data, comp_data_bytes, width, height);
	if (!dst)
		return -1;

	*bpp = 32;
	*ppdecomp_data = dst;
	return (*width) * (*height) * 4;
#endif
}

#if defined(WINPR_UTILS_IMAGE_PNG)
struct png_mem_encode
{
	char* buffer;
	size_t size;
};

static void png_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
	/* with libpng15 next line causes pointer deference error; use libpng12 */
	struct png_mem_encode* p =
	    (struct png_mem_encode*)png_get_io_ptr(png_ptr); /* was png_ptr->io_ptr */
	size_t nsize = p->size + length;

	/* allocate or grow buffer */
	if (p->buffer)
		p->buffer = realloc(p->buffer, nsize);
	else
		p->buffer = malloc(nsize);

	if (!p->buffer)
		png_error(png_ptr, "Write Error");

	/* copy new bytes to end of buffer */
	memcpy(p->buffer + p->size, data, length);
	p->size += length;
}

/* This is optional but included to show how png_set_write_fn() is called */
static void png_flush(png_structp png_ptr)
{
}

static SSIZE_T save_png_to_buffer(UINT32 bpp, UINT32 width, UINT32 height, const uint8_t* data,
                                  size_t size, void** pDstData)
{
	int rc = -1;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_byte** row_pointers = NULL;
	struct png_mem_encode state = { 0 };

	*pDstData = NULL;

	if (!data || (size == 0))
		return 0;

	WINPR_ASSERT(pDstData);

	const size_t bytes_per_pixel = (bpp + 7) / 8;
	const size_t bytes_per_row = width * bytes_per_pixel;
	if (size < bytes_per_row * height)
		goto fail;

	/* Initialize the write struct. */
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL)
		goto fail;

	/* Initialize the info struct. */
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
		goto fail;

	/* Set up error handling. */
	if (setjmp(png_jmpbuf(png_ptr)))
		goto fail;

	/* Set image attributes. */
	int colorType = PNG_COLOR_TYPE_PALETTE;
	if (bpp > 8)
		colorType = PNG_COLOR_TYPE_RGB;
	if (bpp > 16)
		colorType = PNG_COLOR_TYPE_RGB;
	if (bpp > 24)
		colorType = PNG_COLOR_TYPE_RGBA;

	png_set_IHDR(png_ptr, info_ptr, width, height, 8, colorType, PNG_INTERLACE_NONE,
	             PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	/* Initialize rows of PNG. */
	row_pointers = png_malloc(png_ptr, height * sizeof(png_byte*));
	for (size_t y = 0; y < height; ++y)
	{
		uint8_t* row = png_malloc(png_ptr, sizeof(uint8_t) * bytes_per_row);
		row_pointers[y] = (png_byte*)row;
		for (size_t x = 0; x < width; ++x)
		{

			*row++ = *data++;
			if (bpp > 8)
				*row++ = *data++;
			if (bpp > 16)
				*row++ = *data++;
			if (bpp > 24)
				*row++ = *data++;
		}
	}

	/* Actually write the image data. */
	png_set_write_fn(png_ptr, &state, png_write_data, png_flush);
	png_set_rows(png_ptr, info_ptr, row_pointers);
	png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_BGR, NULL);

	/* Cleanup. */
	for (size_t y = 0; y < height; y++)
		png_free(png_ptr, row_pointers[y]);
	png_free(png_ptr, row_pointers);

	/* Finish writing. */
	rc = state.size;
	*pDstData = state.buffer;
fail:
	png_destroy_write_struct(&png_ptr, &info_ptr);
	if (rc < 0)
		free(state.buffer);
	return rc;
}

typedef struct
{
	png_bytep buffer;
	png_uint_32 bufsize;
	png_uint_32 current_pos;
} MEMORY_READER_STATE;

static void read_data_memory(png_structp png_ptr, png_bytep data, size_t length)
{
	MEMORY_READER_STATE* f = png_get_io_ptr(png_ptr);
	if (length > (f->bufsize - f->current_pos))
		png_error(png_ptr, "read error in read_data_memory (loadpng)");
	else
	{
		memcpy(data, f->buffer + f->current_pos, length);
		f->current_pos += length;
	}
}

static void* winpr_read_png_from_buffer(const void* data, size_t SrcSize, size_t* pSize,
                                        UINT32* pWidth, UINT32* pHeight, UINT32* pBpp)
{
	void* rc = NULL;
	png_uint_32 width = 0;
	png_uint_32 height = 0;
	int bit_depth = 0;
	int color_type = 0;
	int interlace_type = 0;
	int transforms = PNG_TRANSFORM_IDENTITY;
	MEMORY_READER_STATE memory_reader_state = { 0 };
	png_bytepp row_pointers = NULL;
	png_infop info_ptr = NULL;
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
		goto fail;
	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
		goto fail;

	memory_reader_state.buffer = (png_bytep)data;
	memory_reader_state.bufsize = SrcSize;
	memory_reader_state.current_pos = 0;

	png_set_read_fn(png_ptr, &memory_reader_state, read_data_memory);

	transforms |= PNG_TRANSFORM_BGR;
	png_read_png(png_ptr, info_ptr, transforms, NULL);

	if (png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type,
	                 NULL, NULL) != 1)
		goto fail;

	size_t bpp = PNG_IMAGE_PIXEL_SIZE(color_type);

	row_pointers = png_get_rows(png_ptr, info_ptr);
	if (row_pointers)
	{
		const size_t stride = width * bpp;
		const size_t png_stride = png_get_rowbytes(png_ptr, info_ptr);
		const size_t size = width * height * bpp;
		const size_t copybytes = stride > png_stride ? png_stride : stride;

		rc = malloc(size);
		if (rc)
		{
			char* cur = rc;
			for (int i = 0; i < height; i++)
			{
				memcpy(cur, row_pointers[i], copybytes);
				cur += stride;
			}
			*pSize = size;
			*pWidth = width;
			*pHeight = height;
			*pBpp = bpp * 8;
		}
	}
fail:

	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	return rc;
}
#endif

void* winpr_convert_to_png(const void* data, size_t size, UINT32 width, UINT32 height,
                           UINT32 stride, UINT32 bpp, UINT32* pSize)
{
	WINPR_ASSERT(data || (size == 0));
	WINPR_ASSERT(pSize);

	*pSize = 0;

#if defined(WINPR_UTILS_IMAGE_PNG)
	void* dst = NULL;
	SSIZE_T rc = save_png_to_buffer(bpp, width, height, data, size, &dst);
	if (rc <= 0)
		return NULL;
	*pSize = (UINT32)rc;
	return dst;
#elif defined(WITH_LODEPNG)
	{
		BYTE* dst = NULL;
		size_t dstsize = 0;
		unsigned rc = 1;

		switch (bpp)
		{
			case 32:
				rc = lodepng_encode32(&dst, &dstsize, data, width, height);
				break;
			case 24:
				rc = lodepng_encode24(&dst, &dstsize, data, width, height);
				break;
			default:
				break;
		}
		if (rc)
			return NULL;
		*pSize = (UINT32)dstsize;
		return dst;
	}
#else
	return NULL;
#endif
}

SSIZE_T winpr_convert_from_png(const char* comp_data, size_t comp_data_bytes, UINT32* width,
                               UINT32* height, UINT32* bpp, char** ppdecomp_data)
{
#if defined(WINPR_UTILS_IMAGE_PNG)
	size_t len = 0;
	*ppdecomp_data =
	    winpr_read_png_from_buffer(comp_data, comp_data_bytes, &len, width, height, bpp);
	if (!*ppdecomp_data)
		return -1;
	return (SSIZE_T)len;
#elif defined(WITH_LODEPNG)
	*bpp = 32;
	return lodepng_decode32((unsigned char**)ppdecomp_data, width, height, comp_data,
	                        comp_data_bytes);
#else
	return -1;
#endif
}

BOOL winpr_image_format_is_supported(UINT32 format)
{
	switch (format)
	{
		case WINPR_IMAGE_BITMAP:
#if defined(WINPR_UTILS_IMAGE_PNG) || defined(WITH_LODEPNG)
		case WINPR_IMAGE_PNG:
#endif
#if defined(WINPR_UTILS_IMAGE_JPEG)
		case WINPR_IMAGE_JPEG:
#endif
#if defined(WINPR_UTILS_IMAGE_WEBP)
		case WINPR_IMAGE_WEBP:
#endif
			return TRUE;
		default:
			return FALSE;
	}
}

BOOL winpr_image_equal(const wImage* imageA, const wImage* imageB)
{
	if (imageA == imageB)
		return TRUE;
	if (!imageA || !imageB)
		return FALSE;

	if (imageA->bitsPerPixel != imageB->bitsPerPixel)
		return FALSE;
	if (imageA->bytesPerPixel != imageB->bytesPerPixel)
		return FALSE;
	if (imageA->height != imageB->height)
		return FALSE;
	if (imageA->width != imageB->width)
		return FALSE;
	if (imageA->scanline != imageB->scanline)
		return FALSE;

	const size_t sizeA = 1ull * imageA->scanline * imageA->height;
	for (size_t x = 0; x < sizeA; x++)
	{
		const BYTE a = imageA->data[x];
		const BYTE b = imageB->data[x];
		if (a != b)
		{
			/* filter out quantization errors */
			if (abs((int)a - (int)b) > 6)
				return FALSE;
		}
	}
	return TRUE;
}

const char* winpr_image_format_mime(UINT32 format)
{
	switch (format)
	{
		case WINPR_IMAGE_BITMAP:
			return "image/bmp";
		case WINPR_IMAGE_PNG:
			return "image/png";
		case WINPR_IMAGE_WEBP:
			return "image/webp";
		case WINPR_IMAGE_JPEG:
			return "image/jpeg";
		default:
			return NULL;
	}
}

const char* winpr_image_format_extension(UINT32 format)
{
	switch (format)
	{
		case WINPR_IMAGE_BITMAP:
			return "bmp";
		case WINPR_IMAGE_PNG:
			return "png";
		case WINPR_IMAGE_WEBP:
			return "webp";
		case WINPR_IMAGE_JPEG:
			return "jpg";
		default:
			return NULL;
	}
}

void* winpr_image_write_buffer(wImage* image, UINT32 format, size_t* psize)
{
	WINPR_ASSERT(image);
	switch (format)
	{
		case WINPR_IMAGE_BITMAP:
		{
			UINT32 outsize = 0;
			size_t size = 1ull * image->height * image->scanline;
			void* data = winpr_bitmap_write_buffer(image->data, size, image->width, image->height,
			                                       image->scanline, image->bitsPerPixel, &outsize);
			*psize = outsize;
			return data;
		}
		break;
		case WINPR_IMAGE_WEBP:
		{
			UINT32 outsize = 0;
			size_t size = 1ull * image->height * image->scanline;
			void* data = winpr_convert_to_webp(image->data, size, image->width, image->height,
			                                   image->scanline, image->bitsPerPixel, &outsize);
			*psize = outsize;
			return data;
		}
		break;
		case WINPR_IMAGE_JPEG:
		{
			UINT32 outsize = 0;
			size_t size = 1ull * image->height * image->scanline;
			void* data = winpr_convert_to_jpeg(image->data, size, image->width, image->height,
			                                   image->scanline, image->bitsPerPixel, &outsize);
			*psize = outsize;
			return data;
		}
		break;
		case WINPR_IMAGE_PNG:
		{
			UINT32 outsize = 0;
			size_t size = 1ull * image->height * image->scanline;
			void* data = winpr_convert_to_png(image->data, size, image->width, image->height,
			                                  image->scanline, image->bitsPerPixel, &outsize);
			*psize = outsize;
			return data;
		}
		break;
		default:
			*psize = 0;
			return NULL;
	}
}
