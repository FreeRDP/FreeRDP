/* FreeRDP: A Remote Desktop Protocol Client
 * Color conversion operations.
 * vi:ts=4 sw=4:
 *
 * Copyright 2011 Stephen Erisman
 * Copyright 2011 Norbert Federa <norbert.federa@thincast.com>
 * Copyright 2011 Martin Fleisz <martin.fleisz@thincast.com>
 * (c) Copyright 2012 Hewlett-Packard Development Company, L.P.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0.
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/types.h>
#include <freerdp/primitives.h>
#include <freerdp/codec/color.h>

#include "prim_internal.h"

#ifndef MINMAX
#define MINMAX(_v_, _l_, _h_) \
	((_v_) < (_l_) ? (_l_) : ((_v_) > (_h_) ? (_h_) : (_v_)))
#endif /* !MINMAX */

/* ------------------------------------------------------------------------- */
static INLINE BYTE* writePixel(BYTE* dst, UINT32 format, BYTE r, BYTE g, BYTE b)
{
	UINT32 color = GetColor(format, r, g, b, 0);
	WriteColor(dst, format, color);
	return dst + GetBytesPerPixel(format);
}


static pstatus_t general_yCbCrToRGB_16s8u_P3AC4R(
    const INT16* pSrc[3], UINT32 srcStep,
    BYTE* pDst, UINT32 DstFormat, UINT32 dstStep,
    const prim_size_t* roi)
{
	UINT32 x, y;
	INT16 R, G, B;
	float Y, Cb, Cr;
	BYTE* pRGB = pDst;
	const INT16* pY  = pSrc[0];
	const INT16* pCb = pSrc[1];
	const INT16* pCr = pSrc[2];
	int srcPad = (srcStep - (roi->width * 2)) / 2;
	int dstPad = (dstStep - (roi->width * 4)) / 4;

	for (y = 0; y < roi->height; y++)
	{
		for (x = 0; x < roi->width; x++)
		{
			Y = (float)(pY[0] + 4096);
			Cb = (float)(pCb[0]);
			Cr = (float)(pCr[0]);
			R = ((INT16)(((Cr * 1.402525f) + Y + 16.0f)) >> 5);
			G = ((INT16)((Y - (Cb * 0.343730f) - (Cr * 0.714401f) + 16.0f)) >> 5);
			B = ((INT16)(((Cb * 1.769905f) + Y + 16.0f)) >> 5);

			if (R < 0)
				R = 0;
			else if (R > 255)
				R = 255;

			if (G < 0)
				G = 0;
			else if (G > 255)
				G = 255;

			if (B < 0)
				B = 0;
			else if (B > 255)
				B = 255;

			pRGB = writePixel(pRGB, DstFormat, R, G, B);
			pY++;
			pCb++;
			pCr++;
		}

		pY += srcPad;
		pCb += srcPad;
		pCr += srcPad;
		pRGB += dstPad;
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t general_yCbCrToBGR_16s8u_P3AC4R(
    const INT16* pSrc[3], UINT32 srcStep,
    BYTE* pDst, UINT32 DstFormat, UINT32 dstStep,
    const prim_size_t* roi)
{
	UINT32 x, y;
	INT16 R, G, B;
	float Y, Cb, Cr;
	BYTE* pRGB = pDst;
	const INT16* pY  = pSrc[0];
	const INT16* pCb = pSrc[1];
	const INT16* pCr = pSrc[2];
	UINT32 srcPad = (srcStep - (roi->width * 2)) / 2;
	UINT32 dstPad = (dstStep - (roi->width * 4)) / 4;

	for (y = 0; y < roi->height; y++)
	{
		for (x = 0; x < roi->width; x++)
		{
			Y = (float)(pY[0] + 4096);
			Cb = (float)(pCb[0]);
			Cr = (float)(pCr[0]);
			R = ((INT16)(((Cr * 1.402525f) + Y + 16.0f)) >> 5);
			G = ((INT16)((Y - (Cb * 0.343730f) - (Cr * 0.714401f) + 16.0f)) >> 5);
			B = ((INT16)(((Cb * 1.769905f) + Y + 16.0f)) >> 5);

			if (R < 0)
				R = 0;
			else if (R > 255)
				R = 255;

			if (G < 0)
				G = 0;
			else if (G > 255)
				G = 255;

			if (B < 0)
				B = 0;
			else if (B > 255)
				B = 255;

			pRGB = writePixel(pRGB, DstFormat, R, G, B);
			pY++;
			pCb++;
			pCr++;
		}

		pY += srcPad;
		pCb += srcPad;
		pCr += srcPad;
		pRGB += dstPad;
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */

static pstatus_t general_yCbCrToRGB_16s16s_P3P3(
    const INT16* pSrc[3],  INT32 srcStep,
    INT16* pDst[3],  INT32 dstStep,
    const prim_size_t* roi)	/* region of interest */
{
	/**
	 * The decoded YCbCr coeffectients are represented as 11.5 fixed-point
	 * numbers:
	 *
	 * 1 sign bit + 10 integer bits + 5 fractional bits
	 *
	 * However only 7 integer bits will be actually used since the value range
	 * is [-128.0, 127.0].  In other words, the decoded coefficients are scaled
	 * by << 5 when interpreted as INT16.
	 * It was scaled in the quantization phase, so we must scale it back here.
	 */
	const INT16* yptr  = pSrc[0];
	const INT16* cbptr = pSrc[1];
	const INT16* crptr = pSrc[2];
	INT16* rptr = pDst[0];
	INT16* gptr = pDst[1];
	INT16* bptr = pDst[2];
	UINT32 srcbump = (srcStep - (roi->width * sizeof(UINT16))) / sizeof(UINT16);
	UINT32 dstbump = (dstStep - (roi->width * sizeof(UINT16))) / sizeof(UINT16);
	UINT32 y;

	for (y = 0; y < roi->height; y++)
	{
		UINT32 x;

		for (x = 0; x < roi->width; ++x)
		{
			/* INT32 is used intentionally because we calculate
			 * with shifted factors!
			 */
			INT32 y  = (INT32)(*yptr++);
			INT32 cb = (INT32)(*cbptr++);
			INT32 cr = (INT32)(*crptr++);
			INT32 r, g, b;
			/*
			 * This is the slow floating point version kept here for reference.
			 * y = y + 4096; // 128<<5=4096 so that we can scale the sum by>>5
			 * r = y + cr*1.403f;
			 * g = y - cb*0.344f - cr*0.714f;
			 * b = y + cb*1.770f;
			 * y_r_buf[i]  = MINMAX(r>>5, 0, 255);
			 * cb_g_buf[i] = MINMAX(g>>5, 0, 255);
			 * cr_b_buf[i] = MINMAX(b>>5, 0, 255);
			 */
			/*
			 * We scale the factors by << 16 into 32-bit integers in order to
			 * avoid slower floating point multiplications.  Since the final
			 * result needs to be scaled by >> 5 we will extract only the
			 * upper 11 bits (>> 21) from the final sum.
			 * Hence we also have to scale the other terms of the sum by << 16.
			 * R: 1.403 << 16 = 91947
			 * G: 0.344 << 16 = 22544, 0.714 << 16 = 46792
			 * B: 1.770 << 16 = 115998
			 */
			y = (y + 4096) << 16;
			r = y + cr * 91947;
			g = y - cb * 22544 - cr * 46792;
			b = y + cb * 115998;
			*rptr++ = MINMAX(r >> 21, 0, 255);
			*gptr++ = MINMAX(g >> 21, 0, 255);
			*bptr++ = MINMAX(b >> 21, 0, 255);
		}

		yptr  += srcbump;
		cbptr += srcbump;
		crptr += srcbump;
		rptr += dstbump;
		gptr += dstbump;
		bptr += dstbump;
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
static pstatus_t general_RGBToYCbCr_16s16s_P3P3(
    const INT16* pSrc[3],  INT32 srcStep,
    INT16* pDst[3],  INT32 dstStep,
    const prim_size_t* roi)	/* region of interest */
{
	/* The encoded YCbCr coefficients are represented as 11.5 fixed-point
	 * numbers:
	 *
	 * 1 sign bit + 10 integer bits + 5 fractional bits
	 *
	 * However only 7 integer bits will be actually used since the value
	 * range is [-128.0, 127.0].  In other words, the encoded coefficients
	 * is scaled by << 5 when interpreted as INT16.
	 * It will be scaled down to original during the quantization phase.
	 */
	const INT16* rptr = pSrc[0];
	const INT16* gptr = pSrc[1];
	const INT16* bptr = pSrc[2];
	INT16* yptr  = pDst[0];
	INT16* cbptr = pDst[1];
	INT16* crptr = pDst[2];
	UINT32 srcbump = (srcStep - (roi->width * sizeof(UINT16))) / sizeof(UINT16);
	UINT32 dstbump = (dstStep - (roi->width * sizeof(UINT16))) / sizeof(UINT16);
	UINT32 y;

	for (y = 0; y < roi->height; y++)
	{
		UINT32 x;

		for (x = 0; x < roi->width; ++x)
		{
			/* INT32 is used intentionally because we calculate with
			 * shifted factors!
			 */
			INT32 r = (INT32)(*rptr++);
			INT32 g = (INT32)(*gptr++);
			INT32 b = (INT32)(*bptr++);
			/* We scale the factors by << 15 into 32-bit integers in order
			 * to avoid slower floating point multiplications.  Since the
			 * terms need to be scaled by << 5 we simply scale the final
			 * sum by >> 10
			 *
			 * Y:  0.299000 << 15 = 9798,  0.587000 << 15 = 19235,
			 *     0.114000 << 15 = 3735
			 * Cb: 0.168935 << 15 = 5535,  0.331665 << 15 = 10868,
			 *     0.500590 << 15 = 16403
			 * Cr: 0.499813 << 15 = 16377, 0.418531 << 15 = 13714,
			 *     0.081282 << 15 = 2663
			 */
			INT32 y  = (r *  9798 + g *  19235 + b *  3735) >> 10;
			INT32 cb = (r * -5535 + g * -10868 + b * 16403) >> 10;
			INT32 cr = (r * 16377 + g * -13714 + b * -2663) >> 10;
			*yptr++  = (INT16) MINMAX(y - 4096, -4096, 4095);
			*cbptr++ = (INT16) MINMAX(cb, -4096, 4095);
			*crptr++ = (INT16) MINMAX(cr, -4096, 4095);
		}

		yptr  += srcbump;
		cbptr += srcbump;
		crptr += srcbump;
		rptr += dstbump;
		gptr += dstbump;
		bptr += dstbump;
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
static pstatus_t general_RGBToRGB_16s8u_P3AC4R(
    const INT16* const pSrc[3],	/* 16-bit R,G, and B arrays */
    UINT32 srcStep,			/* bytes between rows in source data */
    BYTE* pDst,			/* 32-bit interleaved ARGB (ABGR?) data */
    UINT32 dstStep,			/* bytes between rows in dest data */
    UINT32 DstFormat,
    const prim_size_t* roi)	/* region of interest */
{
	const INT16* r  = pSrc[0];
	const INT16* g  = pSrc[1];
	const INT16* b  = pSrc[2];
	BYTE* dst = pDst;
	UINT32 x, y;
	UINT32 srcbump = (srcStep - (roi->width * sizeof(UINT16))) / sizeof(UINT16);
	UINT32 dstbump = (dstStep - (roi->width * sizeof(UINT32)));

	for (y = 0; y < roi->height; ++y)
	{
		for (x = 0; x < roi->width; ++x)
			dst = writePixel(dst, DstFormat, *r++, *g++, *b++);

		dst += dstbump;
		r += srcbump;
		g += srcbump;
		b += srcbump;
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
void primitives_init_colors(primitives_t* prims)
{
	prims->yCbCrToRGB_16s8u_P3AC4R = general_yCbCrToRGB_16s8u_P3AC4R;
	prims->yCbCrToBGR_16s8u_P3AC4R = general_yCbCrToBGR_16s8u_P3AC4R;
	prims->yCbCrToRGB_16s16s_P3P3 = general_yCbCrToRGB_16s16s_P3P3;
	prims->RGBToYCbCr_16s16s_P3P3 = general_RGBToYCbCr_16s16s_P3P3;
	prims->RGBToRGB_16s8u_P3AC4R  = general_RGBToRGB_16s8u_P3AC4R;
}
