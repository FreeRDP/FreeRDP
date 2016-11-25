/* FreeRDP: A Remote Desktop Protocol Client
 * YCoCg<->RGB Color conversion operations.
 * vi:ts=4 sw=4:
 *
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

#include "prim_internal.h"

#ifndef MINMAX
#define MINMAX(_v_, _l_, _h_) \
	((_v_) < (_l_) ? (_l_) : ((_v_) > (_h_) ? (_h_) : (_v_)))
#endif /* !MINMAX */

/* ------------------------------------------------------------------------- */
static INLINE BYTE* writePixelBGRX(BYTE* dst, DWORD formatSize, UINT32 format,
                                   BYTE R, BYTE G, BYTE B, BYTE A)
{
	dst[0] = B;
	dst[1] = G;
	dst[2] = R;
	dst[3] = A;
	return dst + formatSize;
}

static INLINE BYTE* writePixelRGBX(BYTE* dst, DWORD formatSize, UINT32 format,
                                   BYTE R, BYTE G, BYTE B, BYTE A)
{
	dst[0] = R;
	dst[1] = G;
	dst[2] = B;
	dst[3] = A;
	return dst + formatSize;
}

static INLINE BYTE* writePixelXBGR(BYTE* dst, DWORD formatSize, UINT32 format,
                                   BYTE R, BYTE G, BYTE B, BYTE A)
{
	dst[0] = A;
	dst[1] = B;
	dst[2] = G;
	dst[3] = R;
	return dst + formatSize;
}

static INLINE BYTE* writePixelXRGB(BYTE* dst, DWORD formatSize, UINT32 format,
                                   BYTE R, BYTE G, BYTE B, BYTE A)
{
	dst[0] = A;
	dst[1] = R;
	dst[2] = G;
	dst[3] = B;
	return dst + formatSize;
}

static INLINE BYTE* writePixelGeneric(BYTE* dst, DWORD formatSize, UINT32 format,
                                      BYTE R, BYTE G, BYTE B, BYTE A)
{
	UINT32 color = GetColor(format, R, G, B, A);
	WriteColor(dst, format, color);
	return dst + formatSize;
}

typedef BYTE* (*fkt_writePixel)(BYTE*, DWORD, UINT32, BYTE, BYTE, BYTE, BYTE);

static INLINE fkt_writePixel getWriteFunction(DWORD format)
{
	switch (format)
	{
		case PIXEL_FORMAT_ARGB32:
		case PIXEL_FORMAT_XRGB32:
			return writePixelXRGB;

		case PIXEL_FORMAT_ABGR32:
		case PIXEL_FORMAT_XBGR32:
			return writePixelXBGR;

		case PIXEL_FORMAT_RGBA32:
		case PIXEL_FORMAT_RGBX32:
			return writePixelRGBX;

		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return writePixelBGRX;

		default:
			return writePixelGeneric;
	}
}

static pstatus_t general_YCoCgToRGB_8u_AC4R(
    const BYTE* pSrc, INT32 srcStep,
    BYTE* pDst, UINT32 DstFormat, INT32 dstStep,
    UINT32 width, UINT32 height,
    UINT8 shift,
    BOOL withAlpha)
{
	BYTE A;
	UINT32 x, y;
	BYTE* dptr = pDst;
	const BYTE* sptr = pSrc;
	INT16 Cg, Co, Y, T, R, G, B;
	const DWORD formatSize = GetBytesPerPixel(DstFormat);
	fkt_writePixel writePixel = getWriteFunction(DstFormat);
	int cll = shift - 1;  /* -1 builds in the /2's */
	UINT32 srcPad = srcStep - (width * 4);
	UINT32 dstPad = dstStep - (width * formatSize);

	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
			UINT32 color;
			/* Note: shifts must be done before sign-conversion. */
			Cg = (INT16)((INT8)((*sptr++) << cll));
			Co = (INT16)((INT8)((*sptr++) << cll));
			Y = (INT16)(*sptr++);	/* UINT8->INT16 */
			A = *sptr++;

			if (!withAlpha)
				A = 0xFFU;

			T  = Y - Cg;
			R  = T + Co;
			G  = Y + Cg;
			B  = T - Co;
			dptr = (*writePixel)(dptr, formatSize, DstFormat, MINMAX(R, 0, 255),
			                     MINMAX(G, 0, 255), MINMAX(B, 0, 255), A);
		}

		sptr += srcPad;
		dptr += dstPad;
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
void primitives_init_YCoCg(primitives_t* prims)
{
	prims->YCoCgToRGB_8u_AC4R = general_YCoCgToRGB_8u_AC4R;
}
