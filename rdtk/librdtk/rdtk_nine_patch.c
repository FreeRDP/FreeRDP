/**
 * RdTk: Remote Desktop Toolkit
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

#include <winpr/config.h>
#include <winpr/assert.h>

#include <rdtk/config.h>

#include "rdtk_resources.h"

#include "rdtk_nine_patch.h"

#if defined(WINPR_WITH_PNG)
#define FILE_EXT "png"
#else
#define FILE_EXT "bmp"
#endif

WINPR_ATTR_NODISCARD
static int rdtk_image_copy_alpha_blend(uint8_t* pDstData, int nDstStep, int nXDst, int nYDst,
                                       int nWidth, int nHeight, const uint8_t* pSrcData,
                                       int nSrcStep, int nXSrc, int nYSrc)
{
	WINPR_ASSERT(pDstData);
	WINPR_ASSERT(pSrcData);

	for (int y = 0; y < nHeight; y++)
	{
		const uint8_t* pSrcPixel = &pSrcData[((nYSrc + y) * nSrcStep) + (nXSrc * 4)];
		uint8_t* pDstPixel = &pDstData[((nYDst + y) * nDstStep) + (nXDst * 4)];

		for (int x = 0; x < nWidth; x++)
		{
			uint8_t B = pSrcPixel[0];
			uint8_t G = pSrcPixel[1];
			uint8_t R = pSrcPixel[2];
			uint8_t A = pSrcPixel[3];
			pSrcPixel += 4;

			if (A == 255)
			{
				pDstPixel[0] = B;
				pDstPixel[1] = G;
				pDstPixel[2] = R;
			}
			else
			{
				R = (R * A) / 255;
				G = (G * A) / 255;
				B = (B * A) / 255;
				pDstPixel[0] = B + (pDstPixel[0] * (255 - A) + (255 / 2)) / 255;
				pDstPixel[1] = G + (pDstPixel[1] * (255 - A) + (255 / 2)) / 255;
				pDstPixel[2] = R + (pDstPixel[2] * (255 - A) + (255 / 2)) / 255;
			}

			pDstPixel[3] = 0xFF;
			pDstPixel += 4;
		}
	}

	return 1;
}

int rdtk_nine_patch_draw(rdtkSurface* surface, int nXDst, int nYDst, int nWidth, int nHeight,
                         rdtkNinePatch* ninePatch)
{
	WINPR_ASSERT(surface);
	WINPR_ASSERT(ninePatch);

	if (nWidth < ninePatch->width)
		nWidth = ninePatch->width;

	if (nHeight < ninePatch->height)
		nHeight = ninePatch->height;

	WINPR_UNUSED(nHeight);

	int scaleWidth = nWidth - (ninePatch->width - ninePatch->scaleWidth);
	int nSrcStep = ninePatch->scanline;
	const uint8_t* pSrcData = ninePatch->data;
	uint8_t* pDstData = surface->data;
	WINPR_ASSERT(surface->scanline <= INT_MAX);
	int nDstStep = (int)surface->scanline;
	/* top */
	int x = 0;
	int y = 0;
	/* top left */
	int nXSrc = 0;
	int nYSrc = 0;
	int width = ninePatch->scaleLeft;
	int height = ninePatch->scaleTop;
	{
		const int rc = rdtk_image_copy_alpha_blend(pDstData, nDstStep, nXDst + x, nYDst + y, width,
		                                           height, pSrcData, nSrcStep, nXSrc, nYSrc);
		if (rc < 0)
			return rc;
	}
	x += width;
	/* top middle (scalable) */
	nXSrc = ninePatch->scaleLeft;
	nYSrc = 0;
	height = ninePatch->scaleTop;

	while (x < (nXSrc + scaleWidth))
	{
		width = (nXSrc + scaleWidth) - x;

		if (width > ninePatch->scaleWidth)
			width = ninePatch->scaleWidth;

		const int rc = rdtk_image_copy_alpha_blend(pDstData, nDstStep, nXDst + x, nYDst + y, width,
		                                           height, pSrcData, nSrcStep, nXSrc, nYSrc);
		if (rc < 0)
			return rc;
		x += width;
	}

	/* top right */
	nXSrc = ninePatch->scaleRight;
	nYSrc = 0;
	width = ninePatch->width - ninePatch->scaleRight;
	height = ninePatch->scaleTop;
	{
		const int rc = rdtk_image_copy_alpha_blend(pDstData, nDstStep, nXDst + x, nYDst + y, width,
		                                           height, pSrcData, nSrcStep, nXSrc, nYSrc);
		if (rc < 0)
			return rc;
	}
	/* middle */
	x = 0;
	y = ninePatch->scaleTop;
	/* middle left */
	nXSrc = 0;
	nYSrc = ninePatch->scaleTop;
	width = ninePatch->scaleLeft;
	height = ninePatch->scaleHeight;
	{
		const int rc = rdtk_image_copy_alpha_blend(pDstData, nDstStep, nXDst + x, nYDst + y, width,
		                                           height, pSrcData, nSrcStep, nXSrc, nYSrc);
		if (rc < 0)
			return rc;
	}
	x += width;
	/* middle (scalable) */
	nXSrc = ninePatch->scaleLeft;
	nYSrc = ninePatch->scaleTop;
	height = ninePatch->scaleHeight;

	while (x < (nXSrc + scaleWidth))
	{
		width = (nXSrc + scaleWidth) - x;

		if (width > ninePatch->scaleWidth)
			width = ninePatch->scaleWidth;

		{
			const int rc =
			    rdtk_image_copy_alpha_blend(pDstData, nDstStep, nXDst + x, nYDst + y, width, height,
			                                pSrcData, nSrcStep, nXSrc, nYSrc);
			if (rc < 0)
				return rc;
		}
		x += width;
	}

	/* middle right */
	nXSrc = ninePatch->scaleRight;
	nYSrc = ninePatch->scaleTop;
	width = ninePatch->width - ninePatch->scaleRight;
	height = ninePatch->scaleHeight;
	{
		const int rc = rdtk_image_copy_alpha_blend(pDstData, nDstStep, nXDst + x, nYDst + y, width,
		                                           height, pSrcData, nSrcStep, nXSrc, nYSrc);
		if (rc < 0)
			return rc;
	}
	/* bottom */
	x = 0;
	y = ninePatch->scaleBottom;
	/* bottom left */
	nXSrc = 0;
	nYSrc = ninePatch->scaleBottom;
	width = ninePatch->scaleLeft;
	height = ninePatch->height - ninePatch->scaleBottom;
	{
		const int rc = rdtk_image_copy_alpha_blend(pDstData, nDstStep, nXDst + x, nYDst + y, width,
		                                           height, pSrcData, nSrcStep, nXSrc, nYSrc);
		if (rc < 0)
			return rc;
	}
	x += width;
	/* bottom middle (scalable) */
	nXSrc = ninePatch->scaleLeft;
	nYSrc = ninePatch->scaleBottom;
	height = ninePatch->height - ninePatch->scaleBottom;

	while (x < (nXSrc + scaleWidth))
	{
		width = (nXSrc + scaleWidth) - x;

		if (width > ninePatch->scaleWidth)
			width = ninePatch->scaleWidth;

		{
			const int rc =
			    rdtk_image_copy_alpha_blend(pDstData, nDstStep, nXDst + x, nYDst + y, width, height,
			                                pSrcData, nSrcStep, nXSrc, nYSrc);
			if (rc < 0)
				return rc;
		}
		x += width;
	}

	/* bottom right */
	nXSrc = ninePatch->scaleRight;
	nYSrc = ninePatch->scaleBottom;
	width = ninePatch->width - ninePatch->scaleRight;
	height = ninePatch->height - ninePatch->scaleBottom;
	return rdtk_image_copy_alpha_blend(pDstData, nDstStep, nXDst + x, nYDst + y, width, height,
	                                   pSrcData, nSrcStep, nXSrc, nYSrc);
}

WINPR_ATTR_NODISCARD
static uint32_t rdtk_get_image_pixel(const wImage* image, size_t y, size_t x)
{
	WINPR_ASSERT(image);
	WINPR_ASSERT(y < image->height);
	WINPR_ASSERT(x < image->width);
	WINPR_ASSERT(image->bytesPerPixel > 0);
	WINPR_ASSERT(image->bytesPerPixel <= 4);
	WINPR_ASSERT(image->scanline >= image->width * image->bytesPerPixel);

	const size_t offset = 1ull * image->scanline * y;
	const BYTE* data = &image->data[offset + 1ull * image->bytesPerPixel * x];

	uint32_t result = 0;
	for (size_t i = 0; i < image->bytesPerPixel; i++)
	{
		result <<= 8;
		result |= data[i];
	}
	return result;
}

WINPR_ATTR_NODISCARD
static BOOL rdtk_nine_patch_get_scale_lr(rdtkNinePatch* ninePatch, wImage* image)
{
	WINPR_ASSERT(image);
	WINPR_ASSERT(ninePatch);

	WINPR_ASSERT(image->data);
	WINPR_ASSERT(image->width > 0);

	int64_t beg = -1;
	int64_t end = -1;

	for (uint32_t y = 0; y < image->height; y++)
	{
		for (uint32_t x = 1; x < image->width - 1; x++)
		{
			const uint32_t pixel = rdtk_get_image_pixel(image, y, x); /* (1, 0) */
			if (beg < 0)
			{
				if (pixel != 0)
					beg = x;
			}
			else if (end < 0)
			{
				if (pixel == 0)
				{
					end = x;
					break;
				}
			}
		}
	}

	if ((beg <= 0) || (end <= 0))
		return FALSE;

	WINPR_ASSERT(beg <= INT32_MAX);
	WINPR_ASSERT(end <= INT32_MAX);
	ninePatch->scaleLeft = (int32_t)beg - 1;
	ninePatch->scaleRight = (int32_t)end - 1;
	ninePatch->scaleWidth = ninePatch->scaleRight - ninePatch->scaleLeft;

	return TRUE;
}

static bool lineValid(const wImage* image, size_t y)
{
	for (size_t x = 0; x < image->width; x++)
	{
		const uint32_t p = rdtk_get_image_pixel(image, y, x);
		if (p != 0)
			return true;
	}

	return false;
}

WINPR_ATTR_NODISCARD
static BOOL rdtk_nine_patch_get_scale_ht(rdtkNinePatch* ninePatch, wImage* image)
{
	WINPR_ASSERT(image);
	WINPR_ASSERT(ninePatch);

	WINPR_ASSERT(image->data);
	WINPR_ASSERT(image->height > 0);

	int64_t beg = -1;
	int64_t end = -1;

	for (uint32_t y = 1; y < image->height - 1; y++)
	{
		if (beg < 0)
		{
			if (lineValid(image, y))
				beg = y;
		}
		else if (end < 0)
		{
			if (!lineValid(image, y))
			{
				end = y;
				break;
			}
		}
	}

	if ((beg <= 0) || (end <= 0))
		return FALSE;

	WINPR_ASSERT(beg <= INT32_MAX);
	WINPR_ASSERT(end <= INT32_MAX);
	ninePatch->scaleTop = (int32_t)beg - 1;
	ninePatch->scaleBottom = (int32_t)end - 1;
	ninePatch->scaleHeight = ninePatch->scaleBottom - ninePatch->scaleTop;

	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rdtk_nine_patch_get_fill_lr(rdtkNinePatch* ninePatch, wImage* image)
{
	WINPR_ASSERT(image);
	WINPR_ASSERT(ninePatch);

	WINPR_ASSERT(image->data);
	WINPR_ASSERT(image->width > 0);
	WINPR_ASSERT(image->height > 0);

	int64_t beg = -1;
	int64_t end = -1;

	for (uint32_t y = 0; y < image->height; y++)
	{
		for (uint32_t x = 1; x < image->width - 1; x++)
		{
			const uint32_t pixel =
			    rdtk_get_image_pixel(image, image->height - 1 - y, x); /* (1, height - 1) */
			if (beg < 0)
			{
				if (pixel != 0)
					beg = x;
			}
			else if (end < 0)
			{
				if (pixel == 0)
				{
					end = x;
					break;
				}
			}
		}
	}

	if ((beg <= 0) || (end <= 0))
		return FALSE;

	WINPR_ASSERT(beg <= INT32_MAX);
	WINPR_ASSERT(end <= INT32_MAX);

	ninePatch->fillLeft = (int32_t)beg - 1;
	ninePatch->fillRight = (int32_t)end - 1;
	ninePatch->fillWidth = ninePatch->fillRight - ninePatch->fillLeft;

	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rdtk_nine_patch_get_fill_ht(rdtkNinePatch* ninePatch, wImage* image)
{
	WINPR_ASSERT(image);
	WINPR_ASSERT(ninePatch);

	WINPR_ASSERT(image->data);
	WINPR_ASSERT(image->width > 0);
	WINPR_ASSERT(image->height > 0);

	int64_t beg = -1;
	int64_t end = -1;

	for (uint32_t y = 1; y < image->height - 1; y++)
	{
		for (uint32_t x = 0; x < image->width; x++)
		{
			const uint32_t pixel =
			    rdtk_get_image_pixel(image, y, image->width - 1 - x); /* (width - 1, 1) */
			if (beg < 0)
			{
				if (pixel != 0)
					beg = y;
			}
			else if (end < 0)
			{
				if (pixel == 0)
				{
					end = y;
					break;
				}
			}
		}
	}

	if ((beg <= 0) || (end <= 0))
		return FALSE;

	WINPR_ASSERT(beg <= INT32_MAX);
	WINPR_ASSERT(end <= INT32_MAX);
	ninePatch->scaleTop = (int32_t)beg - 1;
	ninePatch->scaleBottom = (int32_t)end - 1;
	ninePatch->scaleHeight = ninePatch->scaleBottom - ninePatch->scaleTop;

	return TRUE;
}

int rdtk_nine_patch_set_image(rdtkNinePatch* ninePatch, wImage* image)
{
	WINPR_ASSERT(image);
	WINPR_ASSERT(ninePatch);

	ninePatch->image = image;

	/* parse scalable area */
	if (!rdtk_nine_patch_get_scale_lr(ninePatch, image))
		return -1;

	if (!rdtk_nine_patch_get_scale_ht(ninePatch, image))
		return -1;

	/* parse fillable area */
	if (!rdtk_nine_patch_get_fill_lr(ninePatch, image))
		return -1;

	if (!rdtk_nine_patch_get_fill_ht(ninePatch, image))
		return -1;

	/* cut out borders from image */
	WINPR_ASSERT(image->width >= 2);
	WINPR_ASSERT(image->height >= 2);
	WINPR_ASSERT(image->scanline > 0);
	WINPR_ASSERT(image->width <= INT32_MAX);
	WINPR_ASSERT(image->height <= INT32_MAX);
	WINPR_ASSERT(image->scanline <= INT32_MAX);
	WINPR_ASSERT(image->data);

	ninePatch->width = (int32_t)image->width - 2;
	ninePatch->height = (int32_t)image->height - 2;
	ninePatch->data = &image->data[image->scanline + 4]; /* (1, 1) */
	ninePatch->scanline = (int32_t)image->scanline;

	return 1;
}

rdtkNinePatch* rdtk_nine_patch_new(rdtkEngine* engine)
{
	WINPR_ASSERT(engine);
	rdtkNinePatch* ninePatch = (rdtkNinePatch*)calloc(1, sizeof(rdtkNinePatch));

	if (!ninePatch)
		return NULL;

	ninePatch->engine = engine;
	return ninePatch;
}

void rdtk_nine_patch_free(rdtkNinePatch* ninePatch)
{
	if (!ninePatch)
		return;

	winpr_image_free(ninePatch->image, TRUE);
	free(ninePatch);
}

int rdtk_nine_patch_engine_init(rdtkEngine* engine)
{
	int status = 0;
	rdtkNinePatch* ninePatch = NULL;

	WINPR_ASSERT(engine);

	if (!engine->button9patch)

	{
		SSIZE_T size = 0;
		const uint8_t* data = NULL;
		wImage* image = NULL;

		status = -1;
		size = rdtk_get_embedded_resource_file("btn_default_normal.9." FILE_EXT, &data);

		if (size > 0)
		{
			image = winpr_image_new();

			if (image)
				status = winpr_image_read_buffer(image, data, (size_t)size);
		}

		if (status > 0)
		{
			ninePatch = engine->button9patch = rdtk_nine_patch_new(engine);

			if (ninePatch)
			{
				const int rc = rdtk_nine_patch_set_image(ninePatch, image);
				if (rc < 0)
					return rc;
			}
			else
				winpr_image_free(image, TRUE);
		}
		else
			winpr_image_free(image, TRUE);
	}

	if (!engine->textField9patch)
	{
		const uint8_t* data = NULL;
		status = -1;
		const SSIZE_T size =
		    rdtk_get_embedded_resource_file("textfield_default.9." FILE_EXT, &data);
		wImage* image = NULL;

		if (size > 0)
		{
			image = winpr_image_new();

			if (image)
				status = winpr_image_read_buffer(image, data, (size_t)size);
		}

		if (status > 0)
		{
			ninePatch = engine->textField9patch = rdtk_nine_patch_new(engine);

			if (ninePatch)
			{
				const int rc = rdtk_nine_patch_set_image(ninePatch, image);
				if (rc < 0)
					return rc;
			}
			else
				winpr_image_free(image, TRUE);
		}
		else
			winpr_image_free(image, TRUE);
	}

	return 1;
}

int rdtk_nine_patch_engine_uninit(rdtkEngine* engine)
{
	WINPR_ASSERT(engine);
	if (engine->button9patch)
	{
		rdtk_nine_patch_free(engine->button9patch);
		engine->button9patch = NULL;
	}

	if (engine->textField9patch)
	{
		rdtk_nine_patch_free(engine->textField9patch);
		engine->textField9patch = NULL;
	}

	return 1;
}
