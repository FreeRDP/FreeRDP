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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/codec/color.h>

#include "rdtk_resources.h"

#include "rdtk_nine_patch.h"

int rdtk_image_copy_alpha_blend(BYTE* pDstData, int nDstStep, int nXDst, int nYDst,
		int nWidth, int nHeight, BYTE* pSrcData, int nSrcStep, int nXSrc, int nYSrc)
{
	int x, y;
	int nSrcPad;
	int nDstPad;
	BYTE* pSrcPixel;
	BYTE* pDstPixel;
	BYTE A, R, G, B;

	nSrcPad = (nSrcStep - (nWidth * 4));
	nDstPad = (nDstStep - (nWidth * 4));

	pSrcPixel = &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 4)];
	pDstPixel = &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

	for (y = 0; y < nHeight; y++)
	{
		pSrcPixel = &pSrcData[((nYSrc + y) * nSrcStep) + (nXSrc * 4)];
		pDstPixel = &pDstData[((nYDst + y) * nDstStep) + (nXDst * 4)];

		for (x = 0; x < nWidth; x++)
		{
			B = pSrcPixel[0];
			G = pSrcPixel[1];
			R = pSrcPixel[2];
			A = pSrcPixel[3];
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

		pSrcPixel += nSrcPad;
		pDstPixel += nDstPad;
	}

	return 1;
}

int rdtk_nine_patch_draw(rdtkSurface* surface, int nXDst, int nYDst, int nWidth, int nHeight, rdtkNinePatch* ninePatch)
{
	int x, y;
	int width;
	int height;
	int nXSrc;
	int nYSrc;
	int nSrcStep;
	int nDstStep;
	BYTE* pSrcData;
	BYTE* pDstData;
	int scaleWidth;
	int scaleHeight;

	if (nWidth < ninePatch->width)
		nWidth = ninePatch->width;

	if (nHeight < ninePatch->height)
		nHeight = ninePatch->height;

	scaleWidth = nWidth - (ninePatch->width - ninePatch->scaleWidth);
	scaleHeight = nHeight - (ninePatch->height - ninePatch->scaleHeight);

	nSrcStep = ninePatch->scanline;
	pSrcData = ninePatch->data;

	pDstData = surface->data;
	nDstStep = surface->scanline;

	/* top */

	x = 0;
	y = 0;

	/* top left */

	nXSrc = 0;
	nYSrc = 0;
	width = ninePatch->scaleLeft;
	height = ninePatch->scaleTop;

	rdtk_image_copy_alpha_blend(pDstData, nDstStep, nXDst + x, nYDst + y,
			width, height, pSrcData, nSrcStep, nXSrc, nYSrc);

	x += width;

	/* top middle (scalable) */

	nXSrc = ninePatch->scaleLeft;
	nYSrc = 0;
	width = ninePatch->scaleWidth;
	height = ninePatch->scaleTop;

	while (x < (nXSrc + scaleWidth))
	{
		width = (nXSrc + scaleWidth) - x;

		if (width > ninePatch->scaleWidth)
			width = ninePatch->scaleWidth;

		rdtk_image_copy_alpha_blend(pDstData, nDstStep, nXDst + x, nYDst + y,
				width, height, pSrcData, nSrcStep, nXSrc, nYSrc);

		x += width;
	}

	/* top right */

	nXSrc = ninePatch->scaleRight;
	nYSrc = 0;
	width = ninePatch->width - ninePatch->scaleRight;
	height = ninePatch->scaleTop;

	rdtk_image_copy_alpha_blend(pDstData, nDstStep, nXDst + x, nYDst + y,
			width, height, pSrcData, nSrcStep, nXSrc, nYSrc);

	/* middle */

	x = 0;
	y = ninePatch->scaleTop;

	/* middle left */

	nXSrc = 0;
	nYSrc = ninePatch->scaleTop;
	width = ninePatch->scaleLeft;
	height = ninePatch->scaleHeight;

	rdtk_image_copy_alpha_blend(pDstData, nDstStep, nXDst + x, nYDst + y,
			width, height, pSrcData, nSrcStep, nXSrc, nYSrc);

	x += width;

	/* middle (scalable) */

	nXSrc = ninePatch->scaleLeft;
	nYSrc = ninePatch->scaleTop;
	width = ninePatch->scaleWidth;
	height = ninePatch->scaleHeight;

	while (x < (nXSrc + scaleWidth))
	{
		width = (nXSrc + scaleWidth) - x;

		if (width > ninePatch->scaleWidth)
			width = ninePatch->scaleWidth;

		rdtk_image_copy_alpha_blend(pDstData, nDstStep, nXDst + x, nYDst + y,
				width, height, pSrcData, nSrcStep, nXSrc, nYSrc);

		x += width;
	}

	/* middle right */

	nXSrc = ninePatch->scaleRight;
	nYSrc = ninePatch->scaleTop;
	width = ninePatch->width - ninePatch->scaleRight;
	height = ninePatch->scaleHeight;

	rdtk_image_copy_alpha_blend(pDstData, nDstStep, nXDst + x, nYDst + y,
			width, height, pSrcData, nSrcStep, nXSrc, nYSrc);

	/* bottom */

	x = 0;
	y = ninePatch->scaleBottom;

	/* bottom left */

	nXSrc = 0;
	nYSrc = ninePatch->scaleBottom;
	width = ninePatch->scaleLeft;
	height = ninePatch->height - ninePatch->scaleBottom;

	rdtk_image_copy_alpha_blend(pDstData, nDstStep, nXDst + x, nYDst + y,
			width, height, pSrcData, nSrcStep, nXSrc, nYSrc);

	x += width;

	/* bottom middle (scalable) */

	nXSrc = ninePatch->scaleLeft;
	nYSrc = ninePatch->scaleBottom;
	width = ninePatch->scaleWidth;
	height = ninePatch->height - ninePatch->scaleBottom;

	while (x < (nXSrc + scaleWidth))
	{
		width = (nXSrc + scaleWidth) - x;

		if (width > ninePatch->scaleWidth)
			width = ninePatch->scaleWidth;

		rdtk_image_copy_alpha_blend(pDstData, nDstStep, nXDst + x, nYDst + y,
				width, height, pSrcData, nSrcStep, nXSrc, nYSrc);

		x += width;
	}

	/* bottom right */

	nXSrc = ninePatch->scaleRight;
	nYSrc = ninePatch->scaleBottom;
	width = ninePatch->width - ninePatch->scaleRight;
	height = ninePatch->height - ninePatch->scaleBottom;

	rdtk_image_copy_alpha_blend(pDstData, nDstStep, nXDst + x, nYDst + y,
			width, height, pSrcData, nSrcStep, nXSrc, nYSrc);

	return 1;
}

int rdtk_nine_patch_set_image(rdtkNinePatch* ninePatch, wImage* image)
{
	int x, y;
	BYTE* data;
	int beg, end;
	int scanline;
	UINT32* pixel;
	int width, height;

	ninePatch->image = image;

	width = image->width;
	height = image->height;
	scanline = image->scanline;
	data = image->data;

	/* parse scalable area */

	beg = end = -1;
	pixel = (UINT32*) &data[4]; /* (1, 0) */

	for (x = 1; x < width - 1; x++)
	{
		if (beg < 0)
		{
			if (*pixel)
			{
				beg = x;
			}
		}
		else if (end < 0)
		{
			if (!(*pixel))
			{
				end = x;
				break;
			}
		}

		pixel++;
	}

	ninePatch->scaleLeft = beg - 1;
	ninePatch->scaleRight = end - 1;
	ninePatch->scaleWidth = ninePatch->scaleRight - ninePatch->scaleLeft;

	beg = end = -1;
	pixel = (UINT32*) &data[scanline]; /* (0, 1) */

	for (y = 1; y < height - 1; y++)
	{
		if (beg < 0)
		{
			if (*pixel)
			{
				beg = y;
			}
		}
		else if (end < 0)
		{
			if (!(*pixel))
			{
				end = y;
				break;
			}
		}

		pixel = (UINT32*) &((BYTE*) pixel)[scanline];
	}

	ninePatch->scaleTop = beg - 1;
	ninePatch->scaleBottom = end - 1;
	ninePatch->scaleHeight = ninePatch->scaleBottom - ninePatch->scaleTop;

	/* parse fillable area */

	beg = end = -1;
	pixel = (UINT32*) &data[((height - 1) * scanline) + 4]; /* (1, height - 1) */

	for (x = 1; x < width - 1; x++)
	{
		if (beg < 0)
		{
			if (*pixel)
			{
				beg = x;
			}
		}
		else if (end < 0)
		{
			if (!(*pixel))
			{
				end = x;
				break;
			}
		}

		pixel++;
	}

	ninePatch->fillLeft = beg - 1;
	ninePatch->fillRight = end - 1;
	ninePatch->fillWidth = ninePatch->fillRight - ninePatch->fillLeft;

	beg = end = -1;
	pixel = (UINT32*) &data[((width - 1) * 4) + scanline]; /* (width - 1, 1) */

	for (y = 1; y < height - 1; y++)
	{
		if (beg < 0)
		{
			if (*pixel)
			{
				beg = y;
			}
		}
		else if (end < 0)
		{
			if (!(*pixel))
			{
				end = y;
				break;
			}
		}

		pixel = (UINT32*) &((BYTE*) pixel)[scanline];
	}

	ninePatch->fillTop = beg - 1;
	ninePatch->fillBottom = end - 1;
	ninePatch->fillHeight = ninePatch->fillBottom - ninePatch->fillTop;

	/* cut out borders from image */

	ninePatch->width = width - 2;
	ninePatch->height = height - 2;
	ninePatch->data = &data[scanline + 4]; /* (1, 1) */
	ninePatch->scanline = scanline;

#if 0
	printf("width: %d height: %d\n", ninePatch->width, ninePatch->height);

	printf("scale: left: %d right: %d top: %d bottom: %d\n",
			ninePatch->scaleLeft, ninePatch->scaleRight,
			ninePatch->scaleTop, ninePatch->scaleBottom);

	printf("fill:  left: %d right: %d top: %d bottom: %d\n",
			ninePatch->fillLeft, ninePatch->fillRight,
			ninePatch->fillTop, ninePatch->fillBottom);
#endif

	return 1;
}

rdtkNinePatch* rdtk_nine_patch_new(rdtkEngine* engine)
{
	rdtkNinePatch* ninePatch;

	ninePatch = (rdtkNinePatch*) calloc(1, sizeof(rdtkNinePatch));

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
	int status;
	wImage* image = NULL;
	rdtkNinePatch* ninePatch;

	if (!engine->button9patch)
	{
		int size;
		BYTE* data;

		status = -1;

		size = rdtk_get_embedded_resource_file("btn_default_normal.9.png", &data);

		if (size > 0)
		{
			image = winpr_image_new();

			if (image)
				status = winpr_image_read_buffer(image, data, size);
		}

		if (status > 0)
		{
			ninePatch = engine->button9patch = rdtk_nine_patch_new(engine);

			if (ninePatch)
				rdtk_nine_patch_set_image(ninePatch, image);
		}
	}

	if (!engine->textField9patch)
	{
		int size;
		BYTE* data;

		status = -1;

		size = rdtk_get_embedded_resource_file("textfield_default.9.png", &data);

		if (size > 0)
		{
			image = winpr_image_new();

			if (image)
				status = winpr_image_read_buffer(image, data, size);
		}

		if (status > 0)
		{
			ninePatch = engine->textField9patch = rdtk_nine_patch_new(engine);

			if (ninePatch)
				rdtk_nine_patch_set_image(ninePatch, image);
		}
	}

	return 1;
}

int rdtk_nine_patch_engine_uninit(rdtkEngine* engine)
{
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
