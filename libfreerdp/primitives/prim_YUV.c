/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#include <freerdp/types.h>
#include <freerdp/primitives.h>
#include <freerdp/codec/color.h>

#include "prim_internal.h"
#include "prim_YUV.h"

static INLINE BYTE clip(int x)
{
	if (x < 0) return 0;
	if (x > 255) return 255;
	return (BYTE) x;
}

static INLINE UINT32 YUV_to_RGB(BYTE Y, BYTE U, BYTE V)
{
	BYTE R, G, B;
	int Yp, Up, Vp;

	Yp = Y * 256;
	Up = U - 128;
	Vp = V - 128;

	R = clip((Yp + (403 * Vp)) >> 8);
	G = clip((Yp - (48 * Up) - (120 * Vp)) >> 8);
	B = clip((Yp + (475 * Up)) >> 8);

	return ARGB32(0xFF, R, G, B);
}

pstatus_t general_YUV420ToRGB_8u_P3AC4R(const BYTE* pSrc[3], int srcStep[3],
		BYTE* pDst, int dstStep, const prim_size_t* roi)
{
	int x, y;
	BYTE Y, U, V;
	const BYTE* pY;
	const BYTE* pU;
	const BYTE* pV;
	BYTE* pRGB = pDst;

	pY = pSrc[0];

	for (y = 0; y < roi->height; y++)
	{
		pU = pSrc[1] + (y / 2) * srcStep[1];
		pV = pSrc[2] + (y / 2) * srcStep[2];

		for (x = 0; x < roi->width; x++)
		{
			Y = *pY;
			U = pU[x / 2];
			V = pV[x / 2];

			*((UINT32*) pRGB) = YUV_to_RGB(Y, U, V);

			pRGB += 4;
			pY++;
		}

		pRGB += (dstStep - (roi->width * 4));
		pY += (srcStep[0] - roi->width);
	}

	return PRIMITIVES_SUCCESS;
}

void primitives_init_YUV(primitives_t* prims)
{
	prims->YUV420ToRGB_8u_P3AC4R = general_YUV420ToRGB_8u_P3AC4R;
}

void primitives_deinit_YUV(primitives_t* prims)
{

}
