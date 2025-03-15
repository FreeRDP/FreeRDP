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

#include <math.h>

#include <freerdp/config.h>
#include <winpr/assert.h>
#include <winpr/cast.h>

#include <freerdp/types.h>
#include <freerdp/primitives.h>
#include <freerdp/codec/color.h>

#include "prim_internal.h"
#include "prim_colors.h"

#ifndef MINMAX
#define MINMAX(_v_, _l_, _h_) ((_v_) < (_l_) ? (_l_) : ((_v_) > (_h_) ? (_h_) : (_v_)))
#endif /* !MINMAX */
/* ------------------------------------------------------------------------- */

/* pregenerated table for ycbcr constants: [0,27]
 *
 * rounded integer values derived from the following formula:
 *
 * { (1.402525f * 2^divisor), (0.714401f * 2^divisor), (0.343730f * 2^divisor), (1.769905f *
 * 2^divisor) }
 */

static const INT32 ycbcr_constants[][4] = { { 1, 1, 0, 2 },
	                                        { 3, 1, 1, 4 },
	                                        { 6, 3, 1, 7 },
	                                        { 11, 6, 3, 14 },
	                                        { 22, 11, 5, 28 },
	                                        { 45, 23, 11, 57 },
	                                        { 90, 46, 22, 113 },
	                                        { 180, 91, 44, 227 },
	                                        { 359, 183, 88, 453 },
	                                        { 718, 366, 176, 906 },
	                                        { 1436, 732, 352, 1812 },
	                                        { 2872, 1463, 704, 3625 },
	                                        { 5745, 2926, 1408, 7250 },
	                                        { 11489, 5852, 2816, 14499 },
	                                        { 22979, 11705, 5632, 28998 },
	                                        { 45958, 23409, 11263, 57996 },
	                                        { 91916, 46819, 22527, 115992 },
	                                        { 183832, 93638, 45053, 231985 },
	                                        { 367664, 187276, 90107, 463970 },
	                                        { 735327, 374552, 180214, 927940 },
	                                        { 1470654, 749104, 360427, 1855880 },
	                                        { 2941308, 1498207, 720854, 3711760 },
	                                        { 5882616, 2996415, 1441708, 7423520 },
	                                        { 11765232, 5992830, 2883416, 14847039 },
	                                        { 23530465, 11985660, 5766832, 29694078 },
	                                        { 47060930, 23971320, 11533665, 59388157 },
	                                        { 94121859, 47942640, 23067330, 118776314 },
	                                        { 188243719, 95885279, 46134660, 237552628 },
	                                        { 376487438, 191770558, 92269319, 475105256 },
	                                        { 752974876, 383541116, 184538639, 950210512 },
	                                        { 1505949752, 767082233, 369077277, 1900421023 } };

static pstatus_t general_yCbCrToRGB_16s8u_P3AC4R_BGRX(const INT16* WINPR_RESTRICT pSrc[3],
                                                      UINT32 srcStep, BYTE* WINPR_RESTRICT pDst,
                                                      UINT32 dstStep, UINT32 DstFormat,
                                                      const prim_size_t* WINPR_RESTRICT roi)
{
	BYTE* pRGB = pDst;
	const INT16* pY = pSrc[0];
	const INT16* pCb = pSrc[1];
	const INT16* pCr = pSrc[2];
	const size_t srcPad = (srcStep - (roi->width * 2)) / 2;
	const size_t dstPad = (dstStep - (roi->width * 4));
	const DWORD formatSize = FreeRDPGetBytesPerPixel(DstFormat);

	for (UINT32 y = 0; y < roi->height; y++)
	{
		for (UINT32 x = 0; x < roi->width; x++)
		{
			const INT32 divisor = 16;
			const INT32 Y = (INT32)((UINT32)((*pY++) + 4096) << divisor);
			const INT32 Cb = (*pCb++);
			const INT32 Cr = (*pCr++);

			const INT32 CrR = WINPR_ASSERTING_INT_CAST(
			    int32_t, Cr* ycbcr_constants[divisor][0]); //(1.402525f * 2^divisor);
			const INT32 CrG = WINPR_ASSERTING_INT_CAST(
			    int32_t, Cr* ycbcr_constants[divisor][1]); //(0.714401f * 2^divisor);
			const INT32 CbG = WINPR_ASSERTING_INT_CAST(
			    int32_t, Cb* ycbcr_constants[divisor][2]); //(0.343730f * 2^divisor);
			const INT32 CbB = WINPR_ASSERTING_INT_CAST(
			    int32_t, Cb* ycbcr_constants[divisor][3]); //(1.769905f * 2^divisor);
			const INT16 R = WINPR_ASSERTING_INT_CAST(int16_t, ((CrR + Y) >> divisor) >> 5);
			const INT16 G = WINPR_ASSERTING_INT_CAST(int16_t, ((Y - CbG - CrG) >> divisor) >> 5);
			const INT16 B = WINPR_ASSERTING_INT_CAST(int16_t, ((CbB + Y) >> divisor) >> 5);
			pRGB = writePixelBGRX(pRGB, formatSize, DstFormat, CLIP(R), CLIP(G), CLIP(B), 0);
		}

		pY += srcPad;
		pCb += srcPad;
		pCr += srcPad;
		pRGB += dstPad;
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t general_yCbCrToRGB_16s8u_P3AC4R_general(const INT16* WINPR_RESTRICT pSrc[3],
                                                         UINT32 srcStep, BYTE* WINPR_RESTRICT pDst,
                                                         UINT32 dstStep, UINT32 DstFormat,
                                                         const prim_size_t* WINPR_RESTRICT roi)
{
	BYTE* pRGB = pDst;
	const INT16* pY = pSrc[0];
	const INT16* pCb = pSrc[1];
	const INT16* pCr = pSrc[2];
	const size_t srcPad = (srcStep - (roi->width * 2)) / 2;
	const size_t dstPad = (dstStep - (roi->width * 4));
	const fkt_writePixel writePixel = getPixelWriteFunction(DstFormat, FALSE);
	const DWORD formatSize = FreeRDPGetBytesPerPixel(DstFormat);

	for (UINT32 y = 0; y < roi->height; y++)
	{
		for (UINT32 x = 0; x < roi->width; x++)
		{
			const INT32 divisor = 16;
			const INT32 Y = (INT32)((UINT32)((*pY++) + 4096) << divisor);
			const INT32 Cb = (*pCb++);
			const INT32 Cr = (*pCr++);
			const INT32 CrR = Cr * ycbcr_constants[divisor][0];
			const INT32 CrG = Cr * ycbcr_constants[divisor][1];
			const INT32 CbG = Cb * ycbcr_constants[divisor][2];
			const INT32 CbB = Cb * ycbcr_constants[divisor][3];
			const INT32 R = (CrR + Y) >> (divisor + 5);
			const INT32 G = (Y - CbG - CrG) >> (divisor + 5);
			const INT32 B = (CbB + Y) >> (divisor + 5);
			pRGB = writePixel(pRGB, formatSize, DstFormat, CLIP(R), CLIP(G), CLIP(B), 0);
		}

		pY += srcPad;
		pCb += srcPad;
		pCr += srcPad;
		pRGB += dstPad;
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t general_yCbCrToRGB_16s8u_P3AC4R(const INT16* WINPR_RESTRICT pSrc[3],
                                                 UINT32 srcStep, BYTE* WINPR_RESTRICT pDst,
                                                 UINT32 dstStep, UINT32 DstFormat,
                                                 const prim_size_t* WINPR_RESTRICT roi)
{
	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return general_yCbCrToRGB_16s8u_P3AC4R_BGRX(pSrc, srcStep, pDst, dstStep, DstFormat,
			                                            roi);

		default:
			return general_yCbCrToRGB_16s8u_P3AC4R_general(pSrc, srcStep, pDst, dstStep, DstFormat,
			                                               roi);
	}
}

/* ------------------------------------------------------------------------- */

static pstatus_t
general_yCbCrToRGB_16s16s_P3P3(const INT16* WINPR_RESTRICT pSrc[3], INT32 srcStep,
                               INT16* WINPR_RESTRICT pDst[3], INT32 dstStep,
                               const prim_size_t* WINPR_RESTRICT roi) /* region of interest */
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
	const INT16* yptr = pSrc[0];
	const INT16* cbptr = pSrc[1];
	const INT16* crptr = pSrc[2];
	INT16* rptr = pDst[0];
	INT16* gptr = pDst[1];
	INT16* bptr = pDst[2];
	UINT32 srcbump = (WINPR_ASSERTING_INT_CAST(uint32_t, srcStep) - (roi->width * sizeof(UINT16))) /
	                 sizeof(UINT16);
	UINT32 dstbump = (WINPR_ASSERTING_INT_CAST(uint32_t, dstStep) - (roi->width * sizeof(UINT16))) /
	                 sizeof(UINT16);

	for (UINT32 y = 0; y < roi->height; y++)
	{
		for (UINT32 x = 0; x < roi->width; ++x)
		{
			/* INT32 is used intentionally because we calculate
			 * with shifted factors!
			 */
			INT32 cy = (INT32)(*yptr++);
			INT32 cb = (INT32)(*cbptr++);
			INT32 cr = (INT32)(*crptr++);
			INT64 r = 0;
			INT64 g = 0;
			INT64 b = 0;
			/*
			 * This is the slow floating point version kept here for reference.
			 * y = y + 4096; // 128<<5=4096 so that we can scale the sum by>>5
			 * r = y + cr*1.403f;
			 * g = y - cb*0.344f - cr*0.714f;
			 * b = y + cb*1.770f;
			 * y_r_buf[i]  = CLIP(r>>5);
			 * cb_g_buf[i] = CLIP(g>>5);
			 * cr_b_buf[i] = CLIP(b>>5);
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
			cy = (INT32)((UINT32)(cy + 4096) << 16);

			r = 1LL * cy + 1LL * cr * ycbcr_constants[16][0];
			g = 1LL * cy - 1LL * cb * ycbcr_constants[16][1] - 1LL * cr * ycbcr_constants[16][2];
			b = 1LL * cy + 1LL * cb * ycbcr_constants[16][3];
			*rptr++ = CLIP(r >> 21);
			*gptr++ = CLIP(g >> 21);
			*bptr++ = CLIP(b >> 21);
		}

		yptr += srcbump;
		cbptr += srcbump;
		crptr += srcbump;
		rptr += dstbump;
		gptr += dstbump;
		bptr += dstbump;
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
static pstatus_t
general_RGBToYCbCr_16s16s_P3P3(const INT16* WINPR_RESTRICT pSrc[3], INT32 srcStep,
                               INT16* WINPR_RESTRICT pDst[3], INT32 dstStep,
                               const prim_size_t* WINPR_RESTRICT roi) /* region of interest */
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
	INT16* yptr = pDst[0];
	INT16* cbptr = pDst[1];
	INT16* crptr = pDst[2];
	UINT32 srcbump = (WINPR_ASSERTING_INT_CAST(uint32_t, srcStep) - (roi->width * sizeof(UINT16))) /
	                 sizeof(UINT16);
	UINT32 dstbump = (WINPR_ASSERTING_INT_CAST(uint32_t, dstStep) - (roi->width * sizeof(UINT16))) /
	                 sizeof(UINT16);

	for (UINT32 y = 0; y < roi->height; y++)
	{
		for (UINT32 x = 0; x < roi->width; ++x)
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
			INT32 cy = (r * 9798 + g * 19235 + b * 3735) >> 10;
			INT32 cb = (r * -5535 + g * -10868 + b * 16403) >> 10;
			INT32 cr = (r * 16377 + g * -13714 + b * -2663) >> 10;
			*yptr++ = (INT16)MINMAX(cy - 4096, -4096, 4095);
			*cbptr++ = (INT16)MINMAX(cb, -4096, 4095);
			*crptr++ = (INT16)MINMAX(cr, -4096, 4095);
		}

		yptr += srcbump;
		cbptr += srcbump;
		crptr += srcbump;
		rptr += dstbump;
		gptr += dstbump;
		bptr += dstbump;
	}

	return PRIMITIVES_SUCCESS;
}

static INLINE void writeScanlineGeneric(BYTE* dst, DWORD formatSize, UINT32 DstFormat,
                                        const INT16* r, const INT16* g, const INT16* b, DWORD width)
{
	fkt_writePixel writePixel = getPixelWriteFunction(DstFormat, FALSE);

	for (UINT32 x = 0; x < width; x++)
	{
		const INT16 pr = *r++;
		const INT16 pg = *g++;
		const INT16 pb = *b++;

		dst =
		    writePixel(dst, formatSize, DstFormat, WINPR_ASSERTING_INT_CAST(UINT8, pr),
		               WINPR_ASSERTING_INT_CAST(UINT8, pg), WINPR_ASSERTING_INT_CAST(UINT8, pb), 0);
	}
}

static INLINE void writeScanlineRGB(BYTE* dst, DWORD formatSize, UINT32 DstFormat, const INT16* r,
                                    const INT16* g, const INT16* b, DWORD width)
{
	WINPR_UNUSED(formatSize);
	WINPR_UNUSED(DstFormat);

	for (UINT32 x = 0; x < width; x++)
	{
		const BYTE R = CLIP(*r++);
		const BYTE G = CLIP(*g++);
		const BYTE B = CLIP(*b++);
		*dst++ = R;
		*dst++ = G;
		*dst++ = B;
	}
}

static INLINE void writeScanlineBGR(BYTE* dst, DWORD formatSize, UINT32 DstFormat, const INT16* r,
                                    const INT16* g, const INT16* b, DWORD width)
{
	WINPR_UNUSED(formatSize);
	WINPR_UNUSED(DstFormat);

	for (UINT32 x = 0; x < width; x++)
	{
		const BYTE R = CLIP(*r++);
		const BYTE G = CLIP(*g++);
		const BYTE B = CLIP(*b++);
		*dst++ = B;
		*dst++ = G;
		*dst++ = R;
	}
}

static INLINE void writeScanlineBGRX(BYTE* dst, DWORD formatSize, UINT32 DstFormat, const INT16* r,
                                     const INT16* g, const INT16* b, DWORD width)
{
	WINPR_UNUSED(formatSize);
	WINPR_UNUSED(DstFormat);

	for (UINT32 x = 0; x < width; x++)
	{
		const BYTE R = CLIP(*r++);
		const BYTE G = CLIP(*g++);
		const BYTE B = CLIP(*b++);
		*dst++ = B;
		*dst++ = G;
		*dst++ = R;
		*dst++ = 0xFF;
	}
}

static INLINE void writeScanlineRGBX(BYTE* dst, DWORD formatSize, UINT32 DstFormat, const INT16* r,
                                     const INT16* g, const INT16* b, DWORD width)
{
	WINPR_UNUSED(formatSize);
	WINPR_UNUSED(DstFormat);

	for (UINT32 x = 0; x < width; x++)
	{
		const BYTE R = CLIP(*r++);
		const BYTE G = CLIP(*g++);
		const BYTE B = CLIP(*b++);
		*dst++ = R;
		*dst++ = G;
		*dst++ = B;
		*dst++ = 0xFF;
	}
}

static INLINE void writeScanlineXBGR(BYTE* dst, DWORD formatSize, UINT32 DstFormat, const INT16* r,
                                     const INT16* g, const INT16* b, DWORD width)
{
	WINPR_UNUSED(formatSize);
	WINPR_UNUSED(DstFormat);

	for (UINT32 x = 0; x < width; x++)
	{
		const BYTE R = CLIP(*r++);
		const BYTE G = CLIP(*g++);
		const BYTE B = CLIP(*b++);
		*dst++ = 0xFF;
		*dst++ = B;
		*dst++ = G;
		*dst++ = R;
	}
}

static INLINE void writeScanlineXRGB(BYTE* dst, DWORD formatSize, UINT32 DstFormat, const INT16* r,
                                     const INT16* g, const INT16* b, DWORD width)
{
	WINPR_UNUSED(formatSize);
	WINPR_UNUSED(DstFormat);

	for (UINT32 x = 0; x < width; x++)
	{
		const BYTE R = CLIP(*r++);
		const BYTE G = CLIP(*g++);
		const BYTE B = CLIP(*b++);
		*dst++ = 0xFF;
		*dst++ = R;
		*dst++ = G;
		*dst++ = B;
	}
}

typedef void (*fkt_writeScanline)(BYTE*, DWORD, UINT32, const INT16*, const INT16*, const INT16*,
                                  DWORD);

static INLINE fkt_writeScanline getScanlineWriteFunction(DWORD format)
{
	switch (format)
	{
		case PIXEL_FORMAT_ARGB32:
		case PIXEL_FORMAT_XRGB32:
			return writeScanlineXRGB;

		case PIXEL_FORMAT_ABGR32:
		case PIXEL_FORMAT_XBGR32:
			return writeScanlineXBGR;

		case PIXEL_FORMAT_RGBA32:
		case PIXEL_FORMAT_RGBX32:
			return writeScanlineRGBX;

		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return writeScanlineBGRX;

		case PIXEL_FORMAT_BGR24:
			return writeScanlineBGR;

		case PIXEL_FORMAT_RGB24:
			return writeScanlineRGB;

		default:
			return writeScanlineGeneric;
	}
}

/* ------------------------------------------------------------------------- */
static pstatus_t general_RGBToRGB_16s8u_P3AC4R_general(
    const INT16* WINPR_RESTRICT pSrc[3], /* 16-bit R,G, and B arrays */
    UINT32 srcStep,                      /* bytes between rows in source data */
    BYTE* WINPR_RESTRICT pDst,           /* 32-bit interleaved ARGB (ABGR?) data */
    UINT32 dstStep,                      /* bytes between rows in dest data */
    UINT32 DstFormat, const prim_size_t* WINPR_RESTRICT roi) /* region of interest */
{
	const INT16* r = pSrc[0];
	const INT16* g = pSrc[1];
	const INT16* b = pSrc[2];
	const DWORD srcAdd = srcStep / sizeof(INT16);
	fkt_writeScanline writeScanline = getScanlineWriteFunction(DstFormat);
	const DWORD formatSize = FreeRDPGetBytesPerPixel(DstFormat);

	for (UINT32 y = 0; y < roi->height; ++y)
	{
		(*writeScanline)(pDst, formatSize, DstFormat, r, g, b, roi->width);
		pDst += dstStep;
		r += srcAdd;
		g += srcAdd;
		b += srcAdd;
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t general_RGBToRGB_16s8u_P3AC4R_BGRX(
    const INT16* WINPR_RESTRICT pSrc[3], /* 16-bit R,G, and B arrays */
    UINT32 srcStep,                      /* bytes between rows in source data */
    BYTE* WINPR_RESTRICT pDst,           /* 32-bit interleaved ARGB (ABGR?) data */
    UINT32 dstStep,                      /* bytes between rows in dest data */
    UINT32 DstFormat, const prim_size_t* WINPR_RESTRICT roi) /* region of interest */
{
	const INT16* r = pSrc[0];
	const INT16* g = pSrc[1];
	const INT16* b = pSrc[2];
	const DWORD srcAdd = srcStep / sizeof(INT16);
	const DWORD formatSize = FreeRDPGetBytesPerPixel(DstFormat);

	for (UINT32 y = 0; y < roi->height; ++y)
	{
		writeScanlineBGRX(pDst, formatSize, DstFormat, r, g, b, roi->width);
		pDst += dstStep;
		r += srcAdd;
		g += srcAdd;
		b += srcAdd;
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t
general_RGBToRGB_16s8u_P3AC4R(const INT16* WINPR_RESTRICT pSrc[3], /* 16-bit R,G, and B arrays */
                              UINT32 srcStep,            /* bytes between rows in source data */
                              BYTE* WINPR_RESTRICT pDst, /* 32-bit interleaved ARGB (ABGR?) data */
                              UINT32 dstStep,            /* bytes between rows in dest data */
                              UINT32 DstFormat,
                              const prim_size_t* WINPR_RESTRICT roi) /* region of interest */
{
	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return general_RGBToRGB_16s8u_P3AC4R_BGRX(pSrc, srcStep, pDst, dstStep, DstFormat, roi);

		default:
			return general_RGBToRGB_16s8u_P3AC4R_general(pSrc, srcStep, pDst, dstStep, DstFormat,
			                                             roi);
	}
}
/* ------------------------------------------------------------------------- */
void primitives_init_colors(primitives_t* WINPR_RESTRICT prims)
{
	prims->yCbCrToRGB_16s8u_P3AC4R = general_yCbCrToRGB_16s8u_P3AC4R;
	prims->yCbCrToRGB_16s16s_P3P3 = general_yCbCrToRGB_16s16s_P3P3;
	prims->RGBToYCbCr_16s16s_P3P3 = general_RGBToYCbCr_16s16s_P3P3;
	prims->RGBToRGB_16s8u_P3AC4R = general_RGBToRGB_16s8u_P3AC4R;
}

/* ------------------------------------------------------------------------- */
void primitives_init_colors_opt(primitives_t* WINPR_RESTRICT prims)
{
	primitives_init_colors(prims);
	primitives_init_colors_sse2(prims);
	primitives_init_colors_neon(prims);
}
