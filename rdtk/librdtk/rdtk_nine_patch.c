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

int rdtk_nine_patch_draw(rdtkSurface* surface, int nXDst, int nYDst, int nWidth, int nHeight, rdtkNinePatch* ninePatch)
{
	int x, y;
	int nXSrc;
	int nYSrc;
	int nSrcStep;
	int nDstStep;
	int nSrcPad;
	int nDstPad;
	BYTE* pSrcData;
	BYTE* pSrcPixel;
	BYTE* pDstData;
	BYTE* pDstPixel;
	BYTE A, R, G, B;
	wImage* image = ninePatch->image;

	nXSrc = 0;
	nYSrc = 0;

	nWidth = image->width;
	nHeight = image->height;

	nSrcStep = image->scanline;
	pSrcData = image->data;

	pDstData = surface->data;
	nDstStep = surface->scanline;

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
	wImage* image;
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
				ninePatch->image = image;
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
				ninePatch->image = image;
		}
	}

	return 1;
}
