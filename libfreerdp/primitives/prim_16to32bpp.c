/* prim_16to32bpp.c
 * 16-bit to 32-bit color conversion (widely used)
 * vi:ts=4 sw=4:
 *
 * The general routine was leveraged from freerdp/codec/color.c.
 *
 * Copyright 2010 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * (c) Copyright 2014 Hewlett-Packard Development Company, L.P.
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
#include "prim_16to32bpp.h"

/* ------------------------------------------------------------------------- */
pstatus_t general_RGB565ToARGB_16u32u_C3C4(
	const UINT16* pSrc, INT32 srcStep,
	UINT32* pDst, INT32 dstStep,
	UINT32 width, UINT32 height,
	BOOL alpha, BOOL invert)
{
	const UINT16* src16;
	UINT32* dst32;
	int x,y;
	int srcRowBump, dstRowBump;
	BYTE red, green, blue;

	src16 = pSrc;
	dst32 = pDst;
	srcRowBump = (srcStep - (width * sizeof(UINT16))) / sizeof(UINT16);
	dstRowBump = (dstStep - (width * sizeof(UINT32))) / sizeof(UINT32);

	/* Loops are separated so if-decisions are not made in the loop. */
	if (alpha)
	{
		if (invert)
		{
			for (y=0; y<height; y++)
			{
				for (x=0; x<width; x++)
				{
					UINT32 pixel = (UINT32) *src16++;
					GetBGR16(red, green, blue, pixel);
					pixel = ARGB32(0xFF, red, green, blue);
					*dst32++ = pixel;
				}
				src16 += srcRowBump;
				dst32 += dstRowBump;
			}
		}
		else
		{
			for (y=0; y<height; y++)
			{
				for (x=0; x<width; x++)
				{
					UINT32 pixel = (UINT32) *src16++;
					GetBGR16(red, green, blue, pixel);
					pixel = ABGR32(0xFF, red, green, blue);
					*dst32++ = pixel;
				}
				src16 += srcRowBump;
				dst32 += dstRowBump;
			}
		}
	}
	else
	{
		if (invert)
		{
			for (y=0; y<height; y++)
			{
				for (x=0; x<width; x++)
				{
					UINT32 pixel = (UINT32) *src16++;
					GetBGR16(red, green, blue, pixel);
					pixel = RGB32(red, green, blue);
					*dst32++ = pixel;
				}
				src16 += srcRowBump;
				dst32 += dstRowBump;
			}
		}
		else
		{
			for (y=0; y<height; y++)
			{
				for (x=0; x<width; x++)
				{
					UINT32 pixel = (UINT32) *src16++;
					GetBGR16(red, green, blue, pixel);
					pixel = BGR32(red, green, blue);
					*dst32++ = pixel;
				}
				src16 += srcRowBump;
				dst32 += dstRowBump;
			}
		}
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
void primitives_init_16to32bpp(
	primitives_t *prims)
{
	prims->RGB565ToARGB_16u32u_C3C4 = general_RGB565ToARGB_16u32u_C3C4;

	primitives_init_16to32bpp_opt(prims);
}

/* ------------------------------------------------------------------------- */
void primitives_deinit_16to32bpp(
	primitives_t *prims)
{
	/* Nothing to do. */
}
