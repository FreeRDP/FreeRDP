/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Optimized YUV/RGB conversion operations
 *
 * Copyright 2014 Thomas Erbesdobler
 * Copyright 2016-2017 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016-2017 Norbert Federa <norbert.federa@thincast.com>
 * Copyright 2016-2017 Thincast Technologies GmbH
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

#include <winpr/sysinfo.h>
#include <winpr/crt.h>
#include <freerdp/types.h>
#include <freerdp/primitives.h>

#include "prim_internal.h"

#include <emmintrin.h>
#include <tmmintrin.h>

#if !defined(WITH_SSE2)
#error "This file needs WITH_SSE2 enabled!"
#endif

static primitives_t* generic = NULL;

/****************************************************************************/
/* SSSE3 YUV420 -> RGB conversion                                           */
/****************************************************************************/
static __m128i* ssse3_YUV444Pixel(__m128i* dst, __m128i Yraw, __m128i Uraw, __m128i Vraw, UINT8 pos)
{
	/* Visual Studio 2010 doesn't like _mm_set_epi32 in array initializer list */
	/* Note: This also applies to Visual Studio 2013 before Update 4 */
#if !defined(_MSC_VER) || (_MSC_VER > 1600)
	const __m128i mapY[] =
	{
		_mm_set_epi32(0x80800380, 0x80800280, 0x80800180, 0x80800080),
		_mm_set_epi32(0x80800780, 0x80800680, 0x80800580, 0x80800480),
		_mm_set_epi32(0x80800B80, 0x80800A80, 0x80800980, 0x80800880),
		_mm_set_epi32(0x80800F80, 0x80800E80, 0x80800D80, 0x80800C80)
	};
	const __m128i mapUV[] =
	{
		_mm_set_epi32(0x80038002, 0x80018000, 0x80808080, 0x80808080),
		_mm_set_epi32(0x80078006, 0x80058004, 0x80808080, 0x80808080),
		_mm_set_epi32(0x800B800A, 0x80098008, 0x80808080, 0x80808080),
		_mm_set_epi32(0x800F800E, 0x800D800C, 0x80808080, 0x80808080)
	};
	const __m128i mask[] =
	{
		_mm_set_epi32(0x80038080, 0x80028080, 0x80018080, 0x80008080),
		_mm_set_epi32(0x80800380, 0x80800280, 0x80800180, 0x80800080),
		_mm_set_epi32(0x80808003, 0x80808002, 0x80808001, 0x80808000)
	};
#else
	/* Note: must be in little-endian format ! */
	const __m128i mapY[] =
	{
		{ 0x80, 0x00, 0x80, 0x80, 0x80, 0x01, 0x80, 0x80, 0x80, 0x02, 0x80, 0x80, 0x80, 0x03, 0x80, 0x80 },
		{ 0x80, 0x04, 0x80, 0x80, 0x80, 0x05, 0x80, 0x80, 0x80, 0x06, 0x80, 0x80, 0x80, 0x07, 0x80, 0x80 },
		{ 0x80, 0x08, 0x80, 0x80, 0x80, 0x09, 0x80, 0x80, 0x80, 0x0a, 0x80, 0x80, 0x80, 0x0b, 0x80, 0x80 },
		{ 0x80, 0x0c, 0x80, 0x80, 0x80, 0x0d, 0x80, 0x80, 0x80, 0x0e, 0x80, 0x80, 0x80, 0x0f, 0x80, 0x80 }

	};
	const __m128i mapUV[] =
	{
		{ 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x80, 0x01, 0x80, 0x02, 0x80, 0x03, 0x80 },
		{ 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x04, 0x80, 0x05, 0x80, 0x06, 0x80, 0x07, 0x80 },
		{ 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x08, 0x80, 0x09, 0x80, 0x0a, 0x80, 0x0b, 0x80 },
		{ 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x0c, 0x80, 0x0d, 0x80, 0x0e, 0x80, 0x0f, 0x80 }
	};
	const __m128i mask[] =
	{
		{ 0x80, 0x80, 0x00, 0x80, 0x80, 0x80, 0x01, 0x80, 0x80, 0x80, 0x02, 0x80, 0x80, 0x80, 0x03, 0x80 },
		{ 0x80, 0x00, 0x80, 0x80, 0x80, 0x01, 0x80, 0x80, 0x80, 0x02, 0x80, 0x80, 0x80, 0x03, 0x80, 0x80 },
		{ 0x00, 0x80, 0x80, 0x80, 0x01, 0x80, 0x80, 0x80, 0x02, 0x80, 0x80, 0x80, 0x03, 0x80, 0x80, 0x80 }
	};
#endif
	const __m128i c128 = _mm_set1_epi16(128);
	__m128i BGRX = _mm_set_epi32(0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000);
	{
		__m128i C, D, E;
		/* Load Y values and expand to 32 bit */
		{
			C = _mm_shuffle_epi8(Yraw, mapY[pos]); /* Reorder and multiply by 256 */
		}
		/* Load U values and expand to 32 bit */
		{
			const __m128i U = _mm_shuffle_epi8(Uraw, mapUV[pos]); /* Reorder dcba */
			D = _mm_sub_epi16(U, c128); /* D = U - 128 */
		}
		/* Load V values and expand to 32 bit */
		{
			const __m128i V = _mm_shuffle_epi8(Vraw, mapUV[pos]); /* Reorder dcba */
			E = _mm_sub_epi16(V, c128); /* E = V - 128 */
		}
		/* Get the R value */
		{
			const __m128i c403 = _mm_set1_epi16(403);
			const __m128i e403 = _mm_unpackhi_epi16(_mm_mullo_epi16(E, c403), _mm_mulhi_epi16(E, c403));
			const __m128i Rs = _mm_add_epi32(C, e403);
			const __m128i R32 = _mm_srai_epi32(Rs, 8);
			const __m128i R16 = _mm_packs_epi32(R32, _mm_setzero_si128());
			const __m128i R = _mm_packus_epi16(R16, _mm_setzero_si128());
			const __m128i packed = _mm_shuffle_epi8(R, mask[0]);
			BGRX = _mm_or_si128(BGRX, packed);
		}
		/* Get the G value */
		{
			const __m128i c48 = _mm_set1_epi16(48);
			const __m128i d48 = _mm_unpackhi_epi16(_mm_mullo_epi16(D, c48), _mm_mulhi_epi16(D, c48));
			const __m128i c120 = _mm_set1_epi16(120);
			const __m128i e120 = _mm_unpackhi_epi16(_mm_mullo_epi16(E, c120), _mm_mulhi_epi16(E, c120));
			const __m128i de = _mm_add_epi32(d48, e120);
			const __m128i Gs = _mm_sub_epi32(C, de);
			const __m128i G32 = _mm_srai_epi32(Gs, 8);
			const __m128i G16 = _mm_packs_epi32(G32, _mm_setzero_si128());
			const __m128i G = _mm_packus_epi16(G16, _mm_setzero_si128());
			const __m128i packed = _mm_shuffle_epi8(G, mask[1]);
			BGRX = _mm_or_si128(BGRX, packed);
		}
		/* Get the B value */
		{
			const __m128i c475 = _mm_set1_epi16(475);
			const __m128i d475 = _mm_unpackhi_epi16(_mm_mullo_epi16(D, c475), _mm_mulhi_epi16(D, c475));
			const __m128i Bs = _mm_add_epi32(C, d475);
			const __m128i B32 = _mm_srai_epi32(Bs, 8);
			const __m128i B16 = _mm_packs_epi32(B32, _mm_setzero_si128());
			const __m128i B = _mm_packus_epi16(B16, _mm_setzero_si128());
			const __m128i packed = _mm_shuffle_epi8(B, mask[2]);
			BGRX = _mm_or_si128(BGRX, packed);
		}
	}
	_mm_storeu_si128(dst++, BGRX);
	return dst;
}

static pstatus_t ssse3_YUV420ToRGB_BGRX(
    const BYTE** pSrc, const UINT32* srcStep,
    BYTE* pDst, UINT32 dstStep,
    const prim_size_t* roi)
{
	const UINT32 nWidth = roi->width;
	const UINT32 nHeight = roi->height;
	const UINT32 pad = roi->width % 16;
	const __m128i duplicate = _mm_set_epi8(7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0);
	UINT32 y;

	for (y = 0; y < nHeight; y++)
	{
		UINT32 x;
		__m128i* dst = (__m128i*)(pDst + dstStep * y);
		const BYTE* YData = pSrc[0] + y * srcStep[0];
		const BYTE* UData = pSrc[1] + (y / 2) * srcStep[1];
		const BYTE* VData = pSrc[2] + (y / 2) * srcStep[2];

		for (x = 0; x < nWidth - pad; x += 16)
		{
			const __m128i Y = _mm_loadu_si128((__m128i*)YData);
			const __m128i uRaw = _mm_loadu_si128((__m128i*)UData);
			const __m128i vRaw = _mm_loadu_si128((__m128i*)VData);
			const __m128i U = _mm_shuffle_epi8(uRaw, duplicate);
			const __m128i V = _mm_shuffle_epi8(vRaw, duplicate);
			YData += 16;
			UData += 8;
			VData += 8;
			dst = ssse3_YUV444Pixel(dst, Y, U, V, 0);
			dst = ssse3_YUV444Pixel(dst, Y, U, V, 1);
			dst = ssse3_YUV444Pixel(dst, Y, U, V, 2);
			dst = ssse3_YUV444Pixel(dst, Y, U, V, 3);
		}

		for (x = 0; x < pad; x++)
		{
			const BYTE Y = *YData++;
			const BYTE U = *UData;
			const BYTE V = *VData;
			const BYTE r = YUV2R(Y, U, V);
			const BYTE g = YUV2G(Y, U, V);
			const BYTE b = YUV2B(Y, U, V);
			dst = (__m128i*)writePixelBGRX((BYTE*)dst, 4, PIXEL_FORMAT_BGRX32, r, g, b, 0xFF);

			if (x % 2)
			{
				UData++;
				VData++;
			}
		}
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t ssse3_YUV420ToRGB(
    const BYTE** pSrc, const UINT32* srcStep,
    BYTE* pDst, UINT32 dstStep, UINT32 DstFormat,
    const prim_size_t* roi)
{
	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRX32:
		case PIXEL_FORMAT_BGRA32:
			return ssse3_YUV420ToRGB_BGRX(pSrc, srcStep, pDst, dstStep, roi);

		default:
			return generic->YUV420ToRGB_8u_P3AC4R(pSrc, srcStep, pDst, dstStep, DstFormat, roi);
	}
}

static pstatus_t ssse3_YUV444ToRGB_8u_P3AC4R_BGRX(
    const BYTE** pSrc, const UINT32* srcStep,
    BYTE* pDst, UINT32 dstStep,
    const prim_size_t* roi)
{
	const UINT32 nWidth = roi->width;
	const UINT32 nHeight = roi->height;
	const UINT32 pad = roi->width % 16;
	UINT32 y;

	for (y = 0; y < nHeight; y++)
	{
		UINT32 x;
		__m128i* dst = (__m128i*)(pDst + dstStep * y);
		const BYTE* YData = pSrc[0] + y * srcStep[0];
		const BYTE* UData = pSrc[1] + y * srcStep[1];
		const BYTE* VData = pSrc[2] + y * srcStep[2];

		for (x = 0; x < nWidth - pad; x += 16)
		{
			__m128i Y = _mm_load_si128((__m128i*)YData);
			__m128i U = _mm_load_si128((__m128i*)UData);
			__m128i V = _mm_load_si128((__m128i*)VData);
			YData += 16;
			UData += 16;
			VData += 16;
			dst = ssse3_YUV444Pixel(dst, Y, U, V, 0);
			dst = ssse3_YUV444Pixel(dst, Y, U, V, 1);
			dst = ssse3_YUV444Pixel(dst, Y, U, V, 2);
			dst = ssse3_YUV444Pixel(dst, Y, U, V, 3);
		}

		for (x = 0; x < pad; x++)
		{
			const BYTE Y = *YData++;
			const BYTE U = *UData++;
			const BYTE V = *VData++;
			const BYTE r = YUV2R(Y, U, V);
			const BYTE g = YUV2G(Y, U, V);
			const BYTE b = YUV2B(Y, U, V);
			dst = (__m128i*)writePixelBGRX((BYTE*)dst, 4, PIXEL_FORMAT_BGRX32, r, g, b, 0xFF);
		}
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t ssse3_YUV444ToRGB_8u_P3AC4R(const BYTE** pSrc, const UINT32* srcStep,
        BYTE* pDst, UINT32 dstStep, UINT32 DstFormat,
        const prim_size_t* roi)
{
	if ((unsigned long)pSrc[0] % 16 || (unsigned long)pSrc[1] % 16 || (unsigned long)pSrc[2] % 16 ||
	    srcStep[0] % 16 || srcStep[1] % 16 || srcStep[2] % 16)
		return generic->YUV444ToRGB_8u_P3AC4R(pSrc, srcStep, pDst, dstStep, DstFormat, roi);

	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRX32:
		case PIXEL_FORMAT_BGRA32:
			return ssse3_YUV444ToRGB_8u_P3AC4R_BGRX(pSrc, srcStep, pDst, dstStep, roi);

		default:
			return generic->YUV444ToRGB_8u_P3AC4R(pSrc, srcStep, pDst, dstStep, DstFormat, roi);
	}
}

/****************************************************************************/
/* SSSE3 RGB -> YUV420 conversion                                          **/
/****************************************************************************/


/**
 * Note (nfedera):
 * The used forward transformation factors from RGB to YUV are based on the
 * values specified in [Rec. ITU-R BT.709-6] Section 3:
 * http://www.itu.int/rec/R-REC-BT.709-6-201506-I/en
 *
 * Y =  0.21260 * R + 0.71520 * G + 0.07220 * B +   0;
 * U = -0.11457 * R - 0.38543 * G + 0.50000 * B + 128;
 * V =  0.50000 * R - 0.45415 * G - 0.04585 * B + 128;
 *
 * The most accurate integer arithmetic approximation when using 8-bit signed
 * integer factors with 16-bit signed integer intermediate results is:
 *
 * Y = ( ( 27 * R + 92 * G +  9 * B) >> 7 );
 * U = ( (-29 * R - 99 * G + 128 * B) >> 8 ) + 128;
 * V = ( ( 128 * R - 116 * G -  12 * B) >> 8 ) + 128;
 *
 * Due to signed 8bit range being [-128,127] the U and V constants of 128 are
 * rounded to 127
 */

#define BGRX_Y_FACTORS _mm_set_epi8(0, 27, 92, 9, 0, 27, 92, 9, 0, 27, 92, 9, 0, 27, 92, 9)
#define BGRX_U_FACTORS _mm_set_epi8(0, -29, -99, 127, 0, -29, -99, 127, 0, -29, -99, 127, 0, -29, -99, 127)
#define BGRX_V_FACTORS _mm_set_epi8(0, 127, -116, -12, 0, 127, -116, -12, 0, 127, -116, -12, 0, 127, -116, -12)
#define CONST128_FACTORS _mm_set1_epi8(128)

#define Y_SHIFT 7
#define U_SHIFT 8
#define V_SHIFT 8

/*
TODO:
RGB[AX] can simply be supported using the following factors. And instead of loading the
globals directly the functions below could be passed pointers to the correct vectors
depending on the source picture format.

PRIM_ALIGN_128 static const BYTE rgbx_y_factors[] = {
      27,  92,   9,   0,  27,  92,   9,   0,  27,  92,   9,   0,  27,  92,   9,   0
};
PRIM_ALIGN_128 static const BYTE rgbx_u_factors[] = {
     -15, -49,  64,   0, -15, -49,  64,   0, -15, -49,  64,   0, -15, -49,  64,   0
};
PRIM_ALIGN_128 static const BYTE rgbx_v_factors[] = {
      64, -58,  -6,   0,  64, -58,  -6,   0,  64, -58,  -6,   0,  64, -58,  -6,   0
};
*/


/* compute the luma (Y) component from a single rgb source line */

static INLINE void ssse3_RGBToYUV420_BGRX_Y(
    const BYTE* src, BYTE* dst, UINT32 width)
{
	UINT32 x;
	__m128i x0, x1, x2, x3;
	const __m128i y_factors = BGRX_Y_FACTORS;
	const __m128i* argb = (const __m128i*) src;
	__m128i* ydst = (__m128i*) dst;

	for (x = 0; x < width; x += 16)
	{
		/* store 16 rgba pixels in 4 128 bit registers */
		x0 = _mm_load_si128(argb++); // 1st 4 pixels
		x1 = _mm_load_si128(argb++); // 2nd 4 pixels
		x2 = _mm_load_si128(argb++); // 3rd 4 pixels
		x3 = _mm_load_si128(argb++); // 4th 4 pixels
		/* multiplications and subtotals */
		x0 = _mm_maddubs_epi16(x0, y_factors);
		x1 = _mm_maddubs_epi16(x1, y_factors);
		x2 = _mm_maddubs_epi16(x2, y_factors);
		x3 = _mm_maddubs_epi16(x3, y_factors);
		/* the total sums */
		x0 = _mm_hadd_epi16(x0, x1);
		x2 = _mm_hadd_epi16(x2, x3);
		/* shift the results */
		x0 = _mm_srli_epi16(x0, Y_SHIFT);
		x2 = _mm_srli_epi16(x2, Y_SHIFT);
		/* pack the 16 words into bytes */
		x0 = _mm_packus_epi16(x0, x2);
		/* save to y plane */
		_mm_storeu_si128(ydst++, x0);
	}
}

/* compute the chrominance (UV) components from two rgb source lines */

static INLINE void ssse3_RGBToYUV420_BGRX_UV(
    const BYTE* src1, const BYTE* src2,
    BYTE* dst1, BYTE* dst2, UINT32 width)
{
	UINT32 x;
	const __m128i u_factors = BGRX_U_FACTORS;
	const __m128i v_factors = BGRX_V_FACTORS;
	const __m128i vector128 = CONST128_FACTORS;
	__m128i x0, x1, x2, x3, x4, x5;
	const __m128i* rgb1 = (const __m128i*)src1;
	const __m128i* rgb2 = (const __m128i*)src2;
	__m64* udst = (__m64*)dst1;
	__m64* vdst = (__m64*)dst2;

	for (x = 0; x < width; x += 16)
	{
		/* subsample 16x2 pixels into 16x1 pixels */
		x0 = _mm_load_si128(rgb1++);
		x4 = _mm_load_si128(rgb2++);
		x0 = _mm_avg_epu8(x0, x4);
		x1 = _mm_load_si128(rgb1++);
		x4 = _mm_load_si128(rgb2++);
		x1 = _mm_avg_epu8(x1, x4);
		x2 = _mm_load_si128(rgb1++);
		x4 = _mm_load_si128(rgb2++);
		x2 = _mm_avg_epu8(x2, x4);
		x3 = _mm_load_si128(rgb1++);
		x4 = _mm_load_si128(rgb2++);
		x3 = _mm_avg_epu8(x3, x4);
		/* subsample these 16x1 pixels into 8x1 pixels */
		/**
		 * shuffle controls
		 * c = a[0],a[2],b[0],b[2] == 10 00 10 00 = 0x88
		 * c = a[1],a[3],b[1],b[3] == 11 01 11 01 = 0xdd
		 */
		x4 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(x0), _mm_castsi128_ps(x1), 0x88));
		x0 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(x0), _mm_castsi128_ps(x1), 0xdd));
		x0 = _mm_avg_epu8(x0, x4);
		x4 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(x2), _mm_castsi128_ps(x3), 0x88));
		x1 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(x2), _mm_castsi128_ps(x3), 0xdd));
		x1 = _mm_avg_epu8(x1, x4);
		/* multiplications and subtotals */
		x2 = _mm_maddubs_epi16(x0, u_factors);
		x3 = _mm_maddubs_epi16(x1, u_factors);
		x4 = _mm_maddubs_epi16(x0, v_factors);
		x5 = _mm_maddubs_epi16(x1, v_factors);
		/* the total sums */
		x0 = _mm_hadd_epi16(x2, x3);
		x1 = _mm_hadd_epi16(x4, x5);
		/* shift the results */
		x0 = _mm_srai_epi16(x0, U_SHIFT);
		x1 = _mm_srai_epi16(x1, V_SHIFT);
		/* pack the 16 words into bytes */
		x0 = _mm_packs_epi16(x0, x1);
		/* add 128 */
		x0 = _mm_add_epi8(x0, vector128);
		/* the lower 8 bytes go to the u plane */
		_mm_storel_pi(udst++, _mm_castsi128_ps(x0));
		/* the upper 8 bytes go to the v plane */
		_mm_storeh_pi(vdst++, _mm_castsi128_ps(x0));
	}
}

static pstatus_t ssse3_RGBToYUV420_BGRX(
    const BYTE* pSrc, UINT32 srcFormat, UINT32 srcStep,
    BYTE* pDst[3], UINT32 dstStep[3],
    const prim_size_t* roi)
{
	UINT32 y;
	const BYTE* argb = pSrc;
	BYTE* ydst = pDst[0];
	BYTE* udst = pDst[1];
	BYTE* vdst = pDst[2];

	if (roi->height < 1 || roi->width < 1)
	{
		return !PRIMITIVES_SUCCESS;
	}

	if (roi->width % 16 || (unsigned long)pSrc % 16 || srcStep % 16)
	{
		return generic->RGBToYUV420_8u_P3AC4R(pSrc, srcFormat, srcStep, pDst, dstStep, roi);
	}

	for (y = 0; y < roi->height - 1; y += 2)
	{
		const BYTE* line1 = argb;
		const BYTE* line2 = argb + srcStep;
		ssse3_RGBToYUV420_BGRX_UV(line1, line2, udst, vdst, roi->width);
		ssse3_RGBToYUV420_BGRX_Y(line1, ydst, roi->width);
		ssse3_RGBToYUV420_BGRX_Y(line2, ydst + dstStep[0], roi->width);
		argb += 2 * srcStep;
		ydst += 2 * dstStep[0];
		udst += 1 * dstStep[1];
		vdst += 1 * dstStep[2];
	}

	if (roi->height & 1)
	{
		/* pass the same last line of an odd height twice for UV */
		ssse3_RGBToYUV420_BGRX_UV(argb, argb, udst, vdst, roi->width);
		ssse3_RGBToYUV420_BGRX_Y(argb, ydst, roi->width);
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t ssse3_RGBToYUV420(
    const BYTE* pSrc, UINT32 srcFormat, UINT32 srcStep,
    BYTE* pDst[3], UINT32 dstStep[3],
    const prim_size_t* roi)
{
	switch (srcFormat)
	{
		case PIXEL_FORMAT_BGRX32:
		case PIXEL_FORMAT_BGRA32:
			return ssse3_RGBToYUV420_BGRX(pSrc, srcFormat, srcStep, pDst, dstStep, roi);

		default:
			return generic->RGBToYUV420_8u_P3AC4R(pSrc, srcFormat, srcStep, pDst, dstStep, roi);
	}
}


/****************************************************************************/
/* SSSE3 RGB -> AVC444-YUV conversion                                      **/
/****************************************************************************/

static INLINE void ssse3_RGBToAVC444YUV_BGRX_DOUBLE_ROW(
    const BYTE* srcEven, const BYTE* srcOdd, BYTE* b1Even, BYTE* b1Odd, BYTE* b2,
    BYTE* b3, BYTE* b4, BYTE* b5, BYTE* b6, BYTE* b7, UINT32 width)
{
	UINT32 x;
	const __m128i* argbEven = (const __m128i*) srcEven;
	const __m128i* argbOdd = (const __m128i*) srcOdd;
	const __m128i y_factors = BGRX_Y_FACTORS;
	const __m128i u_factors = BGRX_U_FACTORS;
	const __m128i v_factors = BGRX_V_FACTORS;
	const __m128i vector128 = CONST128_FACTORS;

	for (x = 0; x < width; x += 16)
	{
		/* store 16 rgba pixels in 4 128 bit registers */
		const __m128i xe1 = _mm_load_si128(argbEven++); // 1st 4 pixels
		const __m128i xe2 = _mm_load_si128(argbEven++); // 2nd 4 pixels
		const __m128i xe3 = _mm_load_si128(argbEven++); // 3rd 4 pixels
		const __m128i xe4 = _mm_load_si128(argbEven++); // 4th 4 pixels
		const __m128i xo1 = _mm_load_si128(argbOdd++); // 1st 4 pixels
		const __m128i xo2 = _mm_load_si128(argbOdd++); // 2nd 4 pixels
		const __m128i xo3 = _mm_load_si128(argbOdd++); // 3rd 4 pixels
		const __m128i xo4 = _mm_load_si128(argbOdd++); // 4th 4 pixels
		{
			/* Y: multiplications with subtotals and horizontal sums */
			const __m128i ye1 = _mm_srli_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe1, y_factors),
			                                   _mm_maddubs_epi16(xe2, y_factors)), Y_SHIFT);
			const __m128i ye2 = _mm_srli_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe3, y_factors),
			                                   _mm_maddubs_epi16(xe4, y_factors)), Y_SHIFT);
			const __m128i ye = _mm_packus_epi16(ye1, ye2);
			const __m128i yo1 = _mm_srli_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo1, y_factors),
			                                   _mm_maddubs_epi16(xo2, y_factors)), Y_SHIFT);
			const __m128i yo2 = _mm_srli_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo3, y_factors),
			                                   _mm_maddubs_epi16(xo4, y_factors)), Y_SHIFT);
			const __m128i yo = _mm_packus_epi16(yo1, yo2);
			/* store y [b1] */
			_mm_storeu_si128((__m128i*)b1Even, ye);
			b1Even += 16;

			if (b1Odd)
			{
				_mm_storeu_si128((__m128i*)b1Odd, yo);
				b1Odd += 16;
			}
		}
		{
			/* We have now
			   * 16 even U values in ue
			   * 16 odd U values in uo
			   *
			   * We need to split these according to
			   * 3.3.8.3.2 YUV420p Stream Combination for YUV444 mode */
			__m128i ue, uo;
			{
				const __m128i ue1 = _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe1, u_factors),
				                                   _mm_maddubs_epi16(xe2, u_factors)), U_SHIFT);
				const __m128i ue2 = _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe3, u_factors),
				                                   _mm_maddubs_epi16(xe4, u_factors)), U_SHIFT);
				ue = _mm_add_epi8(_mm_packs_epi16(ue1, ue2), vector128);
			}

			if (b1Odd)
			{
				const __m128i uo1 = _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo1, u_factors),
				                                   _mm_maddubs_epi16(xo2, u_factors)), U_SHIFT);
				const __m128i uo2 = _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo3, u_factors),
				                                   _mm_maddubs_epi16(xo4, u_factors)), U_SHIFT);
				uo = _mm_add_epi8(_mm_packs_epi16(uo1, uo2), vector128);
			}

			/* Now we need the following storage distribution:
			 * 2x   2y    -> b2
			 * x    2y+1  -> b4
			 * 2x+1 2y    -> b6 */
			if (b1Odd) /* b2 */
			{
				const __m128i ueh = _mm_unpackhi_epi8(ue, _mm_setzero_si128());
				const __m128i uoh = _mm_unpackhi_epi8(uo, _mm_setzero_si128());
				const __m128i hi = _mm_add_epi16(ueh, uoh);
				const __m128i uel = _mm_unpacklo_epi8(ue, _mm_setzero_si128());
				const __m128i uol = _mm_unpacklo_epi8(uo, _mm_setzero_si128());
				const __m128i lo = _mm_add_epi16(uel, uol);
				const __m128i added = _mm_hadd_epi16(lo, hi);
				const __m128i avg16 = _mm_srai_epi16(added, 2);
				const __m128i avg = _mm_packus_epi16(avg16, avg16);
				_mm_storel_epi64((__m128i*)b2, avg);
			}
			else
			{
				const __m128i mask = _mm_set_epi8(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				                                  14, 12, 10, 8, 6, 4, 2, 0);
				const __m128i ud = _mm_shuffle_epi8(ue, mask);
				_mm_storel_epi64((__m128i*)b2, ud);
			}

			b2 += 8;

			if (b1Odd) /* b4 */
			{
				_mm_store_si128((__m128i*)b4, uo);
				b4 += 16;
			}

			{
				/* b6 */
				const __m128i mask = _mm_set_epi8(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				                                  15, 13, 11, 9, 7, 5, 3, 1);
				const __m128i ude = _mm_shuffle_epi8(ue, mask);
				_mm_storel_epi64((__m128i*)b6, ude);
				b6 += 8;
			}
		}
		{
			/* We have now
			   * 16 even V values in ue
			   * 16 odd V values in uo
			   *
			   * We need to split these according to
			   * 3.3.8.3.2 YUV420p Stream Combination for YUV444 mode */
			__m128i ve, vo;
			{
				const __m128i ve1 = _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe1, v_factors),
				                                   _mm_maddubs_epi16(xe2, v_factors)), V_SHIFT);
				const __m128i ve2 = _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe3, v_factors),
				                                   _mm_maddubs_epi16(xe4, v_factors)), V_SHIFT);
				ve = _mm_add_epi8(_mm_packs_epi16(ve1, ve2), vector128);
			}

			if (b1Odd)
			{
				const __m128i vo1 = _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo1, v_factors),
				                                   _mm_maddubs_epi16(xo2, v_factors)), V_SHIFT);
				const __m128i vo2 = _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo3, v_factors),
				                                   _mm_maddubs_epi16(xo4, v_factors)), V_SHIFT);
				vo = _mm_add_epi8(_mm_packs_epi16(vo1, vo2), vector128);
			}

			/* Now we need the following storage distribution:
			 * 2x   2y    -> b3
			 * x    2y+1  -> b5
			 * 2x+1 2y    -> b7 */
			if (b1Odd) /* b3 */
			{
				const __m128i veh = _mm_unpackhi_epi8(ve, _mm_setzero_si128());
				const __m128i voh = _mm_unpackhi_epi8(vo, _mm_setzero_si128());
				const __m128i hi = _mm_add_epi16(veh, voh);
				const __m128i vel = _mm_unpacklo_epi8(ve, _mm_setzero_si128());
				const __m128i vol = _mm_unpacklo_epi8(vo, _mm_setzero_si128());
				const __m128i lo = _mm_add_epi16(vel, vol);
				const __m128i added = _mm_hadd_epi16(lo, hi);
				const __m128i avg16 = _mm_srai_epi16(added, 2);
				const __m128i avg = _mm_packus_epi16(avg16, avg16);
				_mm_storel_epi64((__m128i*)b3, avg);
			}
			else
			{
				const __m128i mask = _mm_set_epi8(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				                                  14, 12, 10, 8, 6, 4, 2, 0);
				const __m128i vd = _mm_shuffle_epi8(ve, mask);
				_mm_storel_epi64((__m128i*)b3, vd);
			}

			b3 += 8;

			if (b1Odd) /* b5 */
			{
				_mm_store_si128((__m128i*)b5, vo);
				b5 += 16;
			}

			{
				/* b7 */
				const __m128i mask = _mm_set_epi8(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				                                  15, 13, 11, 9, 7, 5, 3, 1);
				const __m128i vde = _mm_shuffle_epi8(ve, mask);
				_mm_storel_epi64((__m128i*)b7, vde);
				b7 += 8;
			}
		}
	}
}


static pstatus_t ssse3_RGBToAVC444YUV_BGRX(
    const BYTE* pSrc, UINT32 srcFormat, UINT32 srcStep,
    BYTE* pDst1[3], const UINT32 dst1Step[3],
    BYTE* pDst2[3], const UINT32 dst2Step[3],
    const prim_size_t* roi)
{
	UINT32 y;
	const BYTE* pMaxSrc = pSrc + (roi->height - 1) * srcStep;

	if (roi->height < 1 || roi->width < 1)
		return !PRIMITIVES_SUCCESS;

	if (roi->width % 16 || (unsigned long)pSrc % 16 || srcStep % 16)
		return generic->RGBToAVC444YUV(pSrc, srcFormat, srcStep, pDst1, dst1Step, pDst2, dst2Step, roi);

	for (y = 0; y < roi->height; y += 2)
	{
		const BOOL last = (y >= (roi->height - 1));
		const BYTE* srcEven = y < roi->height ? pSrc + y * srcStep : pMaxSrc;
		const BYTE* srcOdd = !last ? pSrc + (y + 1) * srcStep : pMaxSrc;
		const UINT32 i = y >> 1;
		const UINT32 n = (i & ~7) + i;
		BYTE* b1Even = pDst1[0] + y * dst1Step[0];
		BYTE* b1Odd = !last ? (b1Even + dst1Step[0]) : NULL;
		BYTE* b2 = pDst1[1] + (y / 2) * dst1Step[1];
		BYTE* b3 = pDst1[2] + (y / 2) * dst1Step[2];
		BYTE* b4 = pDst2[0] + dst2Step[0] * n;
		BYTE* b5 = b4 + 8 * dst2Step[0];
		BYTE* b6 = pDst2[1] + (y / 2) * dst2Step[1];
		BYTE* b7 = pDst2[2] + (y / 2) * dst2Step[2];
		ssse3_RGBToAVC444YUV_BGRX_DOUBLE_ROW(srcEven, srcOdd, b1Even, b1Odd, b2, b3, b4, b5, b6, b7,
		                                     roi->width);
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t ssse3_RGBToAVC444YUV(
    const BYTE* pSrc, UINT32 srcFormat, UINT32 srcStep,
    BYTE* pDst1[3], const UINT32 dst1Step[3],
    BYTE* pDst2[3], const UINT32 dst2Step[3],
    const prim_size_t* roi)
{
	switch (srcFormat)
	{
		case PIXEL_FORMAT_BGRX32:
		case PIXEL_FORMAT_BGRA32:
			return ssse3_RGBToAVC444YUV_BGRX(pSrc, srcFormat, srcStep, pDst1, dst1Step, pDst2, dst2Step, roi);

		default:
			return generic->RGBToAVC444YUV(pSrc, srcFormat, srcStep, pDst1, dst1Step, pDst2, dst2Step, roi);
	}
}

/* Mapping of arguments:
 *
 * b1 [even lines] -> yLumaDstEven
 * b1 [odd lines]  -> yLumaDstOdd
 * b2              -> uLumaDst
 * b3              -> vLumaDst
 * b4              -> yChromaDst1
 * b5              -> yChromaDst2
 * b6              -> uChromaDst1
 * b7              -> uChromaDst2
 * b8              -> vChromaDst1
 * b9              -> vChromaDst2
 */
static INLINE void ssse3_RGBToAVC444YUVv2_BGRX_DOUBLE_ROW(
    const BYTE* srcEven, const BYTE* srcOdd,
    BYTE* yLumaDstEven, BYTE* yLumaDstOdd,
    BYTE* uLumaDst, BYTE* vLumaDst,
    BYTE* yEvenChromaDst1, BYTE* yEvenChromaDst2,
    BYTE* yOddChromaDst1, BYTE* yOddChromaDst2,
    BYTE* uChromaDst1, BYTE* uChromaDst2,
    BYTE* vChromaDst1, BYTE* vChromaDst2,
    UINT32 width)
{
	UINT32 x;
	const __m128i vector128 = CONST128_FACTORS;
	const __m128i* argbEven = (const __m128i*) srcEven;
	const __m128i* argbOdd = (const __m128i*) srcOdd;

	for (x = 0; x < width; x += 16)
	{
		/* store 16 rgba pixels in 4 128 bit registers
		 * for even and odd rows.
		 */
		const __m128i xe1 = _mm_load_si128(argbEven++); /* 1st 4 pixels */
		const __m128i xe2 = _mm_load_si128(argbEven++); /* 2nd 4 pixels */
		const __m128i xe3 = _mm_load_si128(argbEven++); /* 3rd 4 pixels */
		const __m128i xe4 = _mm_load_si128(argbEven++); /* 4th 4 pixels */
		const __m128i xo1 = _mm_load_si128(argbOdd++); /* 1st 4 pixels */
		const __m128i xo2 = _mm_load_si128(argbOdd++); /* 2nd 4 pixels */
		const __m128i xo3 = _mm_load_si128(argbOdd++); /* 3rd 4 pixels */
		const __m128i xo4 = _mm_load_si128(argbOdd++); /* 4th 4 pixels */
		{
			/* Y: multiplications with subtotals and horizontal sums */
			const __m128i y_factors = BGRX_Y_FACTORS;
			const __m128i ye1 = _mm_srli_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe1, y_factors),
			                                   _mm_maddubs_epi16(xe2, y_factors)), Y_SHIFT);
			const __m128i ye2 = _mm_srli_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe3, y_factors),
			                                   _mm_maddubs_epi16(xe4, y_factors)), Y_SHIFT);
			const __m128i ye = _mm_packus_epi16(ye1, ye2);
			/* store y [b1] */
			_mm_storeu_si128((__m128i*)yLumaDstEven, ye);
			yLumaDstEven += 16;
		}

		if (yLumaDstOdd)
		{
			const __m128i y_factors = BGRX_Y_FACTORS;
			const __m128i yo1 = _mm_srli_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo1, y_factors),
			                                   _mm_maddubs_epi16(xo2, y_factors)), Y_SHIFT);
			const __m128i yo2 = _mm_srli_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo3, y_factors),
			                                   _mm_maddubs_epi16(xo4, y_factors)), Y_SHIFT);
			const __m128i yo = _mm_packus_epi16(yo1, yo2);
			_mm_storeu_si128((__m128i*)yLumaDstOdd, yo);
			yLumaDstOdd += 16;
		}

		{
			/* We have now
			   * 16 even U values in ue
			   * 16 odd U values in uo
			   *
			   * We need to split these according to
			   * 3.3.8.3.3 YUV420p Stream Combination for YUV444v2 mode */
			/* U: multiplications with subtotals and horizontal sums */
			__m128i ue, uo, uavg;
			{
				const __m128i u_factors = BGRX_U_FACTORS;
				const __m128i ue1 = _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe1, u_factors),
				                                   _mm_maddubs_epi16(xe2, u_factors)), U_SHIFT);
				const __m128i ue2 = _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe3, u_factors),
				                                   _mm_maddubs_epi16(xe4, u_factors)), U_SHIFT);
				const __m128i ueavg = _mm_hadd_epi16(ue1, ue2);
				ue = _mm_add_epi8(_mm_packs_epi16(ue1, ue2), vector128);
				uavg = ueavg;
			}
			{
				const __m128i u_factors = BGRX_U_FACTORS;
				const __m128i uo1 = _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo1, u_factors),
				                                   _mm_maddubs_epi16(xo2, u_factors)), U_SHIFT);
				const __m128i uo2 = _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo3, u_factors),
				                                   _mm_maddubs_epi16(xo4, u_factors)), U_SHIFT);
				const __m128i uoavg = _mm_hadd_epi16(uo1, uo2);
				uo = _mm_add_epi8(_mm_packs_epi16(uo1, uo2), vector128);
				uavg = _mm_add_epi16(uavg, uoavg);
				uavg = _mm_srai_epi16(uavg, 2);
				uavg = _mm_packs_epi16(uavg, uoavg);
				uavg = _mm_add_epi8(uavg, vector128);
			}
			/* Now we need the following storage distribution:
			 * 2x   2y    -> uLumaDst
			 * 2x+1  y    -> yChromaDst1
			 * 4x   2y+1  -> uChromaDst1
			 * 4x+2 2y+1  -> vChromaDst1 */
			{
				const __m128i mask = _mm_set_epi8(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				                                  15, 13, 11, 9, 7, 5, 3, 1);
				const __m128i ude = _mm_shuffle_epi8(ue, mask);
				_mm_storel_epi64((__m128i*)yEvenChromaDst1, ude);
				yEvenChromaDst1 += 8;
			}

			if (yLumaDstOdd)
			{
				const __m128i mask = _mm_set_epi8(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				                                  15, 13, 11, 9, 7, 5, 3, 1);
				const __m128i udo = _mm_shuffle_epi8(uo, mask);
				_mm_storel_epi64((__m128i*)yOddChromaDst1, udo);
				yOddChromaDst1 += 8;
			}

			if (yLumaDstOdd)
			{
				const __m128i mask = _mm_set_epi8(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				                                  14, 10, 6, 2, 12, 8, 4, 0);
				const __m128i ud = _mm_shuffle_epi8(uo, mask);
				int* uDst1 = (int*)uChromaDst1;
				int* vDst1 = (int*)vChromaDst1;
				const int* src = (const int*)&ud;
				_mm_stream_si32(uDst1, src[0]);
				_mm_stream_si32(vDst1, src[1]);
				uChromaDst1 += 4;
				vChromaDst1 += 4;
			}

			if (yLumaDstOdd)
			{
				_mm_storel_epi64((__m128i*)uLumaDst, uavg);
				uLumaDst += 8;
			}
			else
			{
				const __m128i mask = _mm_set_epi8(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				                                  14, 12, 10, 8, 6, 4, 2, 0);
				const __m128i ud = _mm_shuffle_epi8(ue, mask);
				_mm_storel_epi64((__m128i*)uLumaDst, ud);
				uLumaDst += 8;
			}
		}

		{
			/* V: multiplications with subtotals and horizontal sums */
			__m128i ve, vo, vavg;
			{
				const __m128i v_factors = BGRX_V_FACTORS;
				const __m128i ve1 = _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe1, v_factors),
				                                   _mm_maddubs_epi16(xe2, v_factors)), V_SHIFT);
				const __m128i ve2 = _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe3, v_factors),
				                                   _mm_maddubs_epi16(xe4, v_factors)), V_SHIFT);
				const __m128i veavg = _mm_hadd_epi16(ve1, ve2);
				ve = _mm_add_epi8(_mm_packs_epi16(ve1, ve2), vector128);
				vavg = veavg;
			}
			{
				const __m128i v_factors = BGRX_V_FACTORS;
				const __m128i vo1 = _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo1, v_factors),
				                                   _mm_maddubs_epi16(xo2, v_factors)), V_SHIFT);
				const __m128i vo2 = _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo3, v_factors),
				                                   _mm_maddubs_epi16(xo4, v_factors)), V_SHIFT);
				const __m128i voavg = _mm_hadd_epi16(vo1, vo2);
				vo = _mm_add_epi8(_mm_packs_epi16(vo1, vo2), vector128);
				vavg = _mm_add_epi16(vavg, voavg);
				vavg = _mm_srai_epi16(vavg, 2);
				vavg = _mm_packs_epi16(vavg, voavg);
				vavg = _mm_add_epi8(vavg, vector128);
			}
			/* Now we need the following storage distribution:
			 * 2x   2y    -> vLumaDst
			 * 2x+1  y    -> yChromaDst2
			 * 4x   2y+1  -> uChromaDst2
			 * 4x+2 2y+1  -> vChromaDst2 */
			{
				const __m128i mask = _mm_set_epi8(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				                                  15, 13, 11, 9, 7, 5, 3, 1);
				__m128i vde = _mm_shuffle_epi8(ve, mask);
				_mm_storel_epi64((__m128i*)yEvenChromaDst2, vde);
				yEvenChromaDst2 += 8;
			}

			if (yLumaDstOdd)
			{
				const __m128i mask = _mm_set_epi8(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				                                  15, 13, 11, 9, 7, 5, 3, 1);
				__m128i vdo = _mm_shuffle_epi8(vo, mask);
				_mm_storel_epi64((__m128i*)yOddChromaDst2, vdo);
				yOddChromaDst2 += 8;
			}

			if (yLumaDstOdd)
			{
				const __m128i mask = _mm_set_epi8(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				                                  14, 10, 6, 2, 12, 8, 4, 0);
				const __m128i vd = _mm_shuffle_epi8(vo, mask);
				int* uDst2 = (int*)uChromaDst2;
				int* vDst2 = (int*)vChromaDst2;
				const int* src = (const int*)&vd;
				_mm_stream_si32(uDst2, src[0]);
				_mm_stream_si32(vDst2, src[1]);
				uChromaDst2 += 4;
				vChromaDst2 += 4;
			}

			if (yLumaDstOdd)
			{
				_mm_storel_epi64((__m128i*)vLumaDst, vavg);
				vLumaDst += 8;
			}
			else
			{
				const __m128i mask = _mm_set_epi8(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				                                  14, 12, 10, 8, 6, 4, 2, 0);
				__m128i vd = _mm_shuffle_epi8(ve, mask);
				_mm_storel_epi64((__m128i*)vLumaDst, vd);
				vLumaDst += 8;
			}
		}
	}
}

static pstatus_t ssse3_RGBToAVC444YUVv2_BGRX(
    const BYTE* pSrc, UINT32 srcFormat, UINT32 srcStep,
    BYTE* pDst1[3], const UINT32 dst1Step[3],
    BYTE* pDst2[3], const UINT32 dst2Step[3],
    const prim_size_t* roi)
{
	UINT32 y;

	if (roi->height < 1 || roi->width < 1)
		return !PRIMITIVES_SUCCESS;

	if (roi->width % 16 || (unsigned long)pSrc % 16 || srcStep % 16)
		return generic->RGBToAVC444YUVv2(pSrc, srcFormat, srcStep, pDst1, dst1Step, pDst2, dst2Step, roi);

	for (y = 0; y < roi->height; y += 2)
	{
		const BYTE* srcEven = (pSrc + y * srcStep);
		const BYTE* srcOdd = (srcEven + srcStep);
		BYTE* dstLumaYEven = (pDst1[0] + y * dst1Step[0]);
		BYTE* dstLumaYOdd = (y < roi->height - 1) ? (dstLumaYEven + dst1Step[0]) : NULL;
		BYTE* dstLumaU = (pDst1[1] + (y / 2) * dst1Step[1]);
		BYTE* dstLumaV = (pDst1[2] + (y / 2) * dst1Step[2]);
		BYTE* dstEvenChromaY1 = (pDst2[0] + y * dst2Step[0]);
		BYTE* dstEvenChromaY2 = dstEvenChromaY1 + roi->width / 2;
		BYTE* dstOddChromaY1 = dstEvenChromaY1 + dst2Step[0];
		BYTE* dstOddChromaY2 = dstEvenChromaY2 + dst2Step[0];
		BYTE* dstChromaU1 = (pDst2[1] + (y / 2) * dst2Step[1]);
		BYTE* dstChromaV1 = (pDst2[2] + (y / 2) * dst2Step[2]);
		BYTE* dstChromaU2 = dstChromaU1 + roi->width / 4;
		BYTE* dstChromaV2 = dstChromaV1 + roi->width / 4;
		ssse3_RGBToAVC444YUVv2_BGRX_DOUBLE_ROW(srcEven, srcOdd, dstLumaYEven,
		                                       dstLumaYOdd, dstLumaU, dstLumaV,
		                                       dstEvenChromaY1, dstEvenChromaY2,
		                                       dstOddChromaY1, dstOddChromaY2,
		                                       dstChromaU1, dstChromaU2,
		                                       dstChromaV1, dstChromaV2,
		                                       roi->width);
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t ssse3_RGBToAVC444YUVv2(
    const BYTE* pSrc, UINT32 srcFormat, UINT32 srcStep,
    BYTE* pDst1[3], const UINT32 dst1Step[3],
    BYTE* pDst2[3], const UINT32 dst2Step[3],
    const prim_size_t* roi)
{
	switch (srcFormat)
	{
		case PIXEL_FORMAT_BGRX32:
		case PIXEL_FORMAT_BGRA32:
			return ssse3_RGBToAVC444YUVv2_BGRX(pSrc, srcFormat, srcStep, pDst1, dst1Step, pDst2, dst2Step, roi);

		default:
			return generic->RGBToAVC444YUVv2(pSrc, srcFormat, srcStep, pDst1, dst1Step, pDst2, dst2Step, roi);
	}
}

static pstatus_t ssse3_LumaToYUV444(const BYTE* pSrcRaw[3], const UINT32 srcStep[3],
                                    BYTE* pDstRaw[3], const UINT32 dstStep[3],
                                    const RECTANGLE_16* roi)
{
	UINT32 x, y;
	const UINT32 nWidth = roi->right - roi->left;
	const UINT32 nHeight = roi->bottom - roi->top;
	const UINT32 halfWidth = (nWidth + 1) / 2;
	const UINT32 halfPad = halfWidth % 16;
	const UINT32 halfHeight = (nHeight + 1) / 2;
	const UINT32 oddY = 1;
	const UINT32 evenY = 0;
	const UINT32 oddX = 1;
	const UINT32 evenX = 0;
	const BYTE* pSrc[3] =
	{
		pSrcRaw[0] + roi->top* srcStep[0] + roi->left,
		pSrcRaw[1] + roi->top / 2 * srcStep[1] + roi->left / 2,
		pSrcRaw[2] + roi->top / 2 * srcStep[2] + roi->left / 2
	};
	BYTE* pDst[3] =
	{
		pDstRaw[0] + roi->top* dstStep[0] + roi->left,
		pDstRaw[1] + roi->top* dstStep[1] + roi->left,
		pDstRaw[2] + roi->top* dstStep[2] + roi->left
	};

	/* Y data is already here... */
	/* B1 */
	for (y = 0; y < nHeight; y++)
	{
		const BYTE* Ym = pSrc[0] + srcStep[0] * y;
		BYTE* pY = pDst[0] + dstStep[0] * y;
		memcpy(pY, Ym, nWidth);
	}

	/* The first half of U, V are already here part of this frame. */
	/* B2 and B3 */
	for (y = 0; y < halfHeight; y++)
	{
		const UINT32 val2y = (2 * y + evenY);
		const UINT32 val2y1 = val2y + oddY;
		const BYTE* Um = pSrc[1] + srcStep[1] * y;
		const BYTE* Vm = pSrc[2] + srcStep[2] * y;
		BYTE* pU = pDst[1] + dstStep[1] * val2y;
		BYTE* pV = pDst[2] + dstStep[2] * val2y;
		BYTE* pU1 = pDst[1] + dstStep[1] * val2y1;
		BYTE* pV1 = pDst[2] + dstStep[2] * val2y1;

		for (x = 0; x < halfWidth - halfPad; x += 16)
		{
			const __m128i unpackHigh = _mm_set_epi8(7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0);
			const __m128i unpackLow = _mm_set_epi8(15, 15, 14, 14, 13, 13, 12, 12, 11, 11, 10, 10, 9, 9, 8, 8);
			{
				const __m128i u = _mm_loadu_si128((__m128i*)&Um[x]);
				const __m128i uHigh = _mm_shuffle_epi8(u, unpackHigh);
				const __m128i uLow = _mm_shuffle_epi8(u, unpackLow);
				_mm_storeu_si128((__m128i*)&pU[2 * x], uHigh);
				_mm_storeu_si128((__m128i*)&pU[2 * x + 16], uLow);
				_mm_storeu_si128((__m128i*)&pU1[2 * x], uHigh);
				_mm_storeu_si128((__m128i*)&pU1[2 * x + 16], uLow);
			}
			{
				const __m128i u = _mm_loadu_si128((__m128i*)&Vm[x]);
				const __m128i uHigh = _mm_shuffle_epi8(u, unpackHigh);
				const __m128i uLow = _mm_shuffle_epi8(u, unpackLow);
				_mm_storeu_si128((__m128i*)&pV[2 * x], uHigh);
				_mm_storeu_si128((__m128i*)&pV[2 * x + 16], uLow);
				_mm_storeu_si128((__m128i*)&pV1[2 * x], uHigh);
				_mm_storeu_si128((__m128i*)&pV1[2 * x + 16], uLow);
			}
		}

		for (; x < halfWidth; x++)
		{
			const UINT32 val2x = 2 * x + evenX;
			const UINT32 val2x1 = val2x + oddX;
			pU[val2x] = Um[x];
			pV[val2x] = Vm[x];
			pU[val2x1] = Um[x];
			pV[val2x1] = Vm[x];
			pU1[val2x] = Um[x];
			pV1[val2x] = Vm[x];
			pU1[val2x1] = Um[x];
			pV1[val2x1] = Vm[x];
		}
	}

	return PRIMITIVES_SUCCESS;
}

static INLINE void ssse3_filter(BYTE* pSrcDst, const BYTE* pSrc2)
{
	const __m128i even = _mm_set_epi8(0x80, 14, 0x80, 12, 0x80, 10, 0x80, 8, 0x80, 6, 0x80, 4, 0x80, 2,
	                                  0x80, 0);
	const __m128i odd = _mm_set_epi8(0x80, 15, 0x80, 13, 0x80, 11, 0x80, 9, 0x80, 7, 0x80, 5, 0x80, 3,
	                                 0x80, 1);
	const __m128i interleave = _mm_set_epi8(15, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 0);
	const __m128i u = _mm_loadu_si128((__m128i*)pSrcDst);
	const __m128i u1 = _mm_loadu_si128((__m128i*)pSrc2);
	const __m128i uEven = _mm_shuffle_epi8(u, even);
	const __m128i uEven4 = _mm_slli_epi16(uEven, 2);
	const __m128i uOdd = _mm_shuffle_epi8(u, odd);
	const __m128i u1Even = _mm_shuffle_epi8(u1, even);
	const __m128i u1Odd = _mm_shuffle_epi8(u1, odd);
	const __m128i tmp1 = _mm_add_epi16(uOdd, u1Even);
	const __m128i tmp2 = _mm_add_epi16(tmp1, u1Odd);
	const __m128i result = _mm_sub_epi16(uEven4, tmp2);
	const __m128i packed = _mm_packus_epi16(result, uOdd);
	const __m128i interleaved = _mm_shuffle_epi8(packed, interleave);
	_mm_storeu_si128((__m128i*)pSrcDst, interleaved);
}

static pstatus_t ssse3_ChromaFilter(BYTE* pDst[3], const UINT32 dstStep[3],
                                    const RECTANGLE_16* roi)
{
	const UINT32 oddY = 1;
	const UINT32 evenY = 0;
	const UINT32 nWidth = roi->right - roi->left;
	const UINT32 nHeight = roi->bottom - roi->top;
	const UINT32 halfHeight = (nHeight + 1) / 2;
	const UINT32 halfWidth = (nWidth + 1) / 2;
	const UINT32 halfPad = halfWidth % 16;
	UINT32 x, y;

	/* Filter */
	for (y = roi->top; y < halfHeight + roi->top; y++)
	{
		const UINT32 val2y = (y * 2 + evenY);
		const UINT32 val2y1 = val2y + oddY;
		BYTE* pU1 = pDst[1] + dstStep[1] * val2y1;
		BYTE* pV1 = pDst[2] + dstStep[2] * val2y1;
		BYTE* pU = pDst[1] + dstStep[1] * val2y;
		BYTE* pV = pDst[2] + dstStep[2] * val2y;

		if (val2y1 > nHeight)
			continue;

		for (x = roi->left; x < halfWidth + roi->left - halfPad; x += 16)
		{
			ssse3_filter(&pU[2 * x], &pU1[2 * x]);
			ssse3_filter(&pV[2 * x], &pV1[2 * x]);
		}

		for (; x < halfWidth + roi->left; x++)
		{
			const UINT32 val2x = (x * 2);
			const UINT32 val2x1 = val2x + 1;
			const INT32 up = pU[val2x] * 4;
			const INT32 vp = pV[val2x] * 4;
			INT32 u2020;
			INT32 v2020;

			if (val2x1 > nWidth)
				continue;

			u2020 = up - pU[val2x1] - pU1[val2x] - pU1[val2x1];
			v2020 = vp - pV[val2x1] - pV1[val2x] - pV1[val2x1];
			pU[val2x] = CLIP(u2020);
			pV[val2x] = CLIP(v2020);
		}
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t ssse3_ChromaV1ToYUV444(const BYTE* pSrcRaw[3], const UINT32 srcStep[3],
                                        BYTE* pDstRaw[3], const UINT32 dstStep[3],
                                        const RECTANGLE_16* roi)
{
	const UINT32 mod = 16;
	UINT32 uY = 0;
	UINT32 vY = 0;
	UINT32 x, y;
	const UINT32 nWidth = roi->right - roi->left;
	const UINT32 nHeight = roi->bottom - roi->top;
	const UINT32 halfWidth = (nWidth + 1) / 2;
	const UINT32 halfPad = halfWidth % 16;
	const UINT32 halfHeight = (nHeight + 1) / 2;
	const UINT32 oddY = 1;
	const UINT32 evenY = 0;
	const UINT32 oddX = 1;
	/* The auxilary frame is aligned to multiples of 16x16.
	 * We need the padded height for B4 and B5 conversion. */
	const UINT32 padHeigth = nHeight + 16 - nHeight % 16;
	const BYTE* pSrc[3] =
	{
		pSrcRaw[0] + roi->top* srcStep[0] + roi->left,
		pSrcRaw[1] + roi->top / 2 * srcStep[1] + roi->left / 2,
		pSrcRaw[2] + roi->top / 2 * srcStep[2] + roi->left / 2
	};
	BYTE* pDst[3] =
	{
		pDstRaw[0] + roi->top* dstStep[0] + roi->left,
		pDstRaw[1] + roi->top* dstStep[1] + roi->left,
		pDstRaw[2] + roi->top* dstStep[2] + roi->left
	};
	const __m128i zero = _mm_setzero_si128();
	const __m128i mask = _mm_set_epi8(0, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0,
	                                  0x80);

	/* The second half of U and V is a bit more tricky... */
	/* B4 and B5 */
	for (y = 0; y < padHeigth; y++)
	{
		const BYTE* Ya = pSrc[0] + srcStep[0] * y;
		BYTE* pX;

		if ((y) % mod < (mod + 1) / 2)
		{
			const UINT32 pos = (2 * uY++ + oddY);

			if (pos >= nHeight)
				continue;

			pX = pDst[1] + dstStep[1] * pos;
		}
		else
		{
			const UINT32 pos = (2 * vY++ + oddY);

			if (pos >= nHeight)
				continue;

			pX = pDst[2] + dstStep[2] * pos;
		}

		memcpy(pX, Ya, nWidth);
	}

	/* B6 and B7 */
	for (y = 0; y < halfHeight; y++)
	{
		const UINT32 val2y = (y * 2 + evenY);
		const BYTE* Ua = pSrc[1] + srcStep[1] * y;
		const BYTE* Va = pSrc[2] + srcStep[2] * y;
		BYTE* pU = pDst[1] + dstStep[1] * val2y;
		BYTE* pV = pDst[2] + dstStep[2] * val2y;

		for (x = 0; x < halfWidth - halfPad; x += 16)
		{
			{
				const __m128i u = _mm_loadu_si128((__m128i*)&Ua[x]);
				const __m128i u2 = _mm_unpackhi_epi8(u, zero);
				const __m128i u1 = _mm_unpacklo_epi8(u, zero);
				_mm_maskmoveu_si128(u1, mask, (char*)&pU[2 * x]);
				_mm_maskmoveu_si128(u2, mask, (char*)&pU[2 * x + 16]);
			}
			{
				const __m128i u = _mm_loadu_si128((__m128i*)&Va[x]);
				const __m128i u2 = _mm_unpackhi_epi8(u, zero);
				const __m128i u1 = _mm_unpacklo_epi8(u, zero);
				_mm_maskmoveu_si128(u1, mask, (char*)&pV[2 * x]);
				_mm_maskmoveu_si128(u2, mask, (char*)&pV[2 * x + 16]);
			}
		}

		for (; x < halfWidth; x++)
		{
			const UINT32 val2x1 = (x * 2 + oddX);
			pU[val2x1] = Ua[x];
			pV[val2x1] = Va[x];
		}
	}

	/* Filter */
	return ssse3_ChromaFilter(pDst, dstStep, roi);
}

static pstatus_t ssse3_ChromaV2ToYUV444(const BYTE* pSrc[3], const UINT32 srcStep[3],
                                        UINT32 nTotalWidth, UINT32 nTotalHeight,
                                        BYTE* pDst[3], const UINT32 dstStep[3],
                                        const RECTANGLE_16* roi)
{
	UINT32 x, y;
	const UINT32 nWidth = roi->right - roi->left;
	const UINT32 nHeight = roi->bottom - roi->top;
	const UINT32 halfWidth = (nWidth + 1) / 2;
	const UINT32 halfPad = halfWidth % 16;
	const UINT32 halfHeight = (nHeight + 1) / 2;
	const UINT32 quaterWidth = (nWidth + 3) / 4;
	const UINT32 quaterPad = quaterWidth % 16;
	const __m128i zero = _mm_setzero_si128();
	const __m128i mask = _mm_set_epi8(0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0,
	                                  0x80, 0);
	const __m128i mask2 = _mm_set_epi8(0, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0,
	                                   0x80);
	const __m128i shuffle1 = _mm_set_epi8(0x80, 15, 0x80, 14, 0x80, 13, 0x80, 12, 0x80, 11, 0x80, 10,
	                                      0x80, 9, 0x80, 8);
	const __m128i shuffle2 = _mm_set_epi8(0x80, 7, 0x80, 6, 0x80, 5, 0x80, 4, 0x80, 3, 0x80, 2, 0x80, 1,
	                                      0x80, 0);

	/* B4 and B5: odd UV values for width/2, height */
	for (y = 0; y < nHeight; y++)
	{
		const UINT32 yTop = y + roi->top;
		const BYTE* pYaU = pSrc[0] + srcStep[0] * yTop + roi->left / 2;
		const BYTE* pYaV = pYaU + nTotalWidth / 2;
		BYTE* pU = pDst[1] + dstStep[1] * yTop + roi->left;
		BYTE* pV = pDst[2] + dstStep[2] * yTop + roi->left;

		for (x = 0; x < halfWidth - halfPad; x += 16)
		{
			{
				const __m128i u = _mm_loadu_si128((__m128i*)&pYaU[x]);
				const __m128i u2 = _mm_unpackhi_epi8(zero, u);
				const __m128i u1 = _mm_unpacklo_epi8(zero, u);
				_mm_maskmoveu_si128(u1, mask, (char*)&pU[2 * x]);
				_mm_maskmoveu_si128(u2, mask, (char*)&pU[2 * x + 16]);
			}
			{
				const __m128i v = _mm_loadu_si128((__m128i*)&pYaV[x]);
				const __m128i v2 = _mm_unpackhi_epi8(zero, v);
				const __m128i v1 = _mm_unpacklo_epi8(zero, v);
				_mm_maskmoveu_si128(v1, mask, (char*)&pV[2 * x]);
				_mm_maskmoveu_si128(v2, mask, (char*)&pV[2 * x + 16]);
			}
		}

		for (; x < halfWidth; x++)
		{
			const UINT32 odd = 2 * x + 1;
			pU[odd] = pYaU[x];
			pV[odd] = pYaV[x];
		}
	}

	/* B6 - B9 */
	for (y = 0; y < halfHeight; y++)
	{
		const BYTE* pUaU = pSrc[1] + srcStep[1] * (y + roi->top / 2) + roi->left / 4;
		const BYTE* pUaV = pUaU + nTotalWidth / 4;
		const BYTE* pVaU = pSrc[2] + srcStep[2] * (y + roi->top / 2) + roi->left / 4;
		const BYTE* pVaV = pVaU + nTotalWidth / 4;
		BYTE* pU = pDst[1] + dstStep[1] * (2 * y + 1 + roi->top) + roi->left;
		BYTE* pV = pDst[2] + dstStep[2] * (2 * y + 1 + roi->top) + roi->left;

		for (x = 0; x < quaterWidth - quaterPad; x += 16)
		{
			{
				const __m128i uU = _mm_loadu_si128((__m128i*)&pUaU[x]);
				const __m128i uV = _mm_loadu_si128((__m128i*)&pVaU[x]);
				const __m128i uHigh = _mm_unpackhi_epi8(uU, uV);
				const __m128i uLow = _mm_unpacklo_epi8(uU, uV);
				const __m128i u1 = _mm_shuffle_epi8(uLow, shuffle2);
				const __m128i u2 = _mm_shuffle_epi8(uLow, shuffle1);
				const __m128i u3 = _mm_shuffle_epi8(uHigh, shuffle2);
				const __m128i u4 = _mm_shuffle_epi8(uHigh, shuffle1);
				_mm_maskmoveu_si128(u1, mask2, (char*)&pU[4 * x + 0]);
				_mm_maskmoveu_si128(u2, mask2, (char*)&pU[4 * x + 16]);
				_mm_maskmoveu_si128(u3, mask2, (char*)&pU[4 * x + 32]);
				_mm_maskmoveu_si128(u4, mask2, (char*)&pU[4 * x + 48]);
			}
			{
				const __m128i vU = _mm_loadu_si128((__m128i*)&pUaV[x]);
				const __m128i vV = _mm_loadu_si128((__m128i*)&pVaV[x]);
				const __m128i vHigh = _mm_unpackhi_epi8(vU, vV);
				const __m128i vLow = _mm_unpacklo_epi8(vU, vV);
				const __m128i v1 = _mm_shuffle_epi8(vLow, shuffle2);
				const __m128i v2 = _mm_shuffle_epi8(vLow, shuffle1);
				const __m128i v3 = _mm_shuffle_epi8(vHigh, shuffle2);
				const __m128i v4 = _mm_shuffle_epi8(vHigh, shuffle1);
				_mm_maskmoveu_si128(v1, mask2, (char*)&pV[4 * x + 0]);
				_mm_maskmoveu_si128(v2, mask2, (char*)&pV[4 * x + 16]);
				_mm_maskmoveu_si128(v3, mask2, (char*)&pV[4 * x + 32]);
				_mm_maskmoveu_si128(v4, mask2, (char*)&pV[4 * x + 48]);
			}
		}

		for (; x < quaterWidth; x++)
		{
			pU[4 * x + 0] = pUaU[x];
			pV[4 * x + 0] = pUaV[x];
			pU[4 * x + 2] = pVaU[x];
			pV[4 * x + 2] = pVaV[x];
		}
	}

	return ssse3_ChromaFilter(pDst, dstStep, roi);
}

static pstatus_t ssse3_YUV420CombineToYUV444(
    avc444_frame_type type,
    const BYTE* pSrc[3], const UINT32 srcStep[3],
    UINT32 nWidth, UINT32 nHeight,
    BYTE* pDst[3], const UINT32 dstStep[3],
    const RECTANGLE_16* roi)
{
	if (!pSrc || !pSrc[0] || !pSrc[1] || !pSrc[2])
		return -1;

	if (!pDst || !pDst[0] || !pDst[1] || !pDst[2])
		return -1;

	if (!roi)
		return -1;

	switch (type)
	{
		case AVC444_LUMA:
			return ssse3_LumaToYUV444(pSrc, srcStep, pDst, dstStep, roi);

		case AVC444_CHROMAv1:
			return ssse3_ChromaV1ToYUV444(pSrc, srcStep, pDst, dstStep, roi);

		case AVC444_CHROMAv2:
			return ssse3_ChromaV2ToYUV444(pSrc, srcStep, nWidth, nHeight, pDst, dstStep, roi);

		default:
			return -1;
	}
}

void primitives_init_YUV_opt(primitives_t* prims)
{
	generic = primitives_get_generic();
	primitives_init_YUV(prims);

	if (IsProcessorFeaturePresentEx(PF_EX_SSSE3)
	    && IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE))
	{
		prims->RGBToYUV420_8u_P3AC4R = ssse3_RGBToYUV420;
		prims->RGBToAVC444YUV = ssse3_RGBToAVC444YUV;
		prims->RGBToAVC444YUVv2 = ssse3_RGBToAVC444YUVv2;
		prims->YUV420ToRGB_8u_P3AC4R = ssse3_YUV420ToRGB;
		prims->YUV444ToRGB_8u_P3AC4R = ssse3_YUV444ToRGB_8u_P3AC4R;
		prims->YUV420CombineToYUV444 = ssse3_YUV420CombineToYUV444;
	}
}
