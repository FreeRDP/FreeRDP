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

#include <winpr/wtypes.h>
#include <freerdp/config.h>

#include <winpr/sysinfo.h>
#include <winpr/crt.h>
#include <freerdp/types.h>
#include <freerdp/primitives.h>

#include "prim_internal.h"
#include "prim_avxsse.h"
#include "prim_YUV.h"

#if defined(SSE_AVX_INTRINSICS_ENABLED)
#include <emmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>

static primitives_t* generic = NULL;

/****************************************************************************/
/* sse41 YUV420 -> RGB conversion                                           */
/****************************************************************************/
static inline __m128i* sse41_YUV444Pixel(__m128i* WINPR_RESTRICT dst, __m128i Yraw, __m128i Uraw,
                                         __m128i Vraw, UINT8 pos)
{
	const __m128i mapY[] = { mm_set_epu32(0x80800380, 0x80800280, 0x80800180, 0x80800080),
		                     mm_set_epu32(0x80800780, 0x80800680, 0x80800580, 0x80800480),
		                     mm_set_epu32(0x80800B80, 0x80800A80, 0x80800980, 0x80800880),
		                     mm_set_epu32(0x80800F80, 0x80800E80, 0x80800D80, 0x80800C80) };
	const __m128i mapUV[] = { mm_set_epu32(0x80038002, 0x80018000, 0x80808080, 0x80808080),
		                      mm_set_epu32(0x80078006, 0x80058004, 0x80808080, 0x80808080),
		                      mm_set_epu32(0x800B800A, 0x80098008, 0x80808080, 0x80808080),
		                      mm_set_epu32(0x800F800E, 0x800D800C, 0x80808080, 0x80808080) };
	const __m128i mask[] = { mm_set_epu32(0x80038080, 0x80028080, 0x80018080, 0x80008080),
		                     mm_set_epu32(0x80800380, 0x80800280, 0x80800180, 0x80800080),
		                     mm_set_epu32(0x80808003, 0x80808002, 0x80808001, 0x80808000) };
	const __m128i c128 = _mm_set1_epi16(128);
	__m128i BGRX = _mm_and_si128(LOAD_SI128(dst),
	                             mm_set_epu32(0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000));
	{
		__m128i C;
		__m128i D;
		__m128i E;
		/* Load Y values and expand to 32 bit */
		{
			C = _mm_shuffle_epi8(Yraw, mapY[pos]); /* Reorder and multiply by 256 */
		}
		/* Load U values and expand to 32 bit */
		{
			const __m128i U = _mm_shuffle_epi8(Uraw, mapUV[pos]); /* Reorder dcba */
			D = _mm_sub_epi16(U, c128);                           /* D = U - 128 */
		}
		/* Load V values and expand to 32 bit */
		{
			const __m128i V = _mm_shuffle_epi8(Vraw, mapUV[pos]); /* Reorder dcba */
			E = _mm_sub_epi16(V, c128);                           /* E = V - 128 */
		}
		/* Get the R value */
		{
			const __m128i c403 = _mm_set1_epi16(403);
			const __m128i e403 =
			    _mm_unpackhi_epi16(_mm_mullo_epi16(E, c403), _mm_mulhi_epi16(E, c403));
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
			const __m128i d48 =
			    _mm_unpackhi_epi16(_mm_mullo_epi16(D, c48), _mm_mulhi_epi16(D, c48));
			const __m128i c120 = _mm_set1_epi16(120);
			const __m128i e120 =
			    _mm_unpackhi_epi16(_mm_mullo_epi16(E, c120), _mm_mulhi_epi16(E, c120));
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
			const __m128i d475 =
			    _mm_unpackhi_epi16(_mm_mullo_epi16(D, c475), _mm_mulhi_epi16(D, c475));
			const __m128i Bs = _mm_add_epi32(C, d475);
			const __m128i B32 = _mm_srai_epi32(Bs, 8);
			const __m128i B16 = _mm_packs_epi32(B32, _mm_setzero_si128());
			const __m128i B = _mm_packus_epi16(B16, _mm_setzero_si128());
			const __m128i packed = _mm_shuffle_epi8(B, mask[2]);
			BGRX = _mm_or_si128(BGRX, packed);
		}
	}
	STORE_SI128(dst++, BGRX);
	return dst;
}

static inline pstatus_t sse41_YUV420ToRGB_BGRX(const BYTE* WINPR_RESTRICT pSrc[],
                                               const UINT32* WINPR_RESTRICT srcStep,
                                               BYTE* WINPR_RESTRICT pDst, UINT32 dstStep,
                                               const prim_size_t* WINPR_RESTRICT roi)
{
	const UINT32 nWidth = roi->width;
	const UINT32 nHeight = roi->height;
	const UINT32 pad = roi->width % 16;
	const __m128i duplicate = _mm_set_epi8(7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0);

	for (size_t y = 0; y < nHeight; y++)
	{
		__m128i* dst = (__m128i*)(pDst + dstStep * y);
		const BYTE* YData = pSrc[0] + y * srcStep[0];
		const BYTE* UData = pSrc[1] + (y / 2) * srcStep[1];
		const BYTE* VData = pSrc[2] + (y / 2) * srcStep[2];

		for (UINT32 x = 0; x < nWidth - pad; x += 16)
		{
			const __m128i Y = LOAD_SI128(YData);
			const __m128i uRaw = LOAD_SI128(UData);
			const __m128i vRaw = LOAD_SI128(VData);
			const __m128i U = _mm_shuffle_epi8(uRaw, duplicate);
			const __m128i V = _mm_shuffle_epi8(vRaw, duplicate);
			YData += 16;
			UData += 8;
			VData += 8;
			dst = sse41_YUV444Pixel(dst, Y, U, V, 0);
			dst = sse41_YUV444Pixel(dst, Y, U, V, 1);
			dst = sse41_YUV444Pixel(dst, Y, U, V, 2);
			dst = sse41_YUV444Pixel(dst, Y, U, V, 3);
		}

		for (UINT32 x = 0; x < pad; x++)
		{
			const BYTE Y = *YData++;
			const BYTE U = *UData;
			const BYTE V = *VData;
			dst = (__m128i*)writeYUVPixel((BYTE*)dst, PIXEL_FORMAT_BGRX32, Y, U, V, writePixelBGRX);

			if (x % 2)
			{
				UData++;
				VData++;
			}
		}
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t sse41_YUV420ToRGB(const BYTE* WINPR_RESTRICT pSrc[3], const UINT32 srcStep[3],
                                   BYTE* WINPR_RESTRICT pDst, UINT32 dstStep, UINT32 DstFormat,
                                   const prim_size_t* WINPR_RESTRICT roi)
{
	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRX32:
		case PIXEL_FORMAT_BGRA32:
			return sse41_YUV420ToRGB_BGRX(pSrc, srcStep, pDst, dstStep, roi);

		default:
			return generic->YUV420ToRGB_8u_P3AC4R(pSrc, srcStep, pDst, dstStep, DstFormat, roi);
	}
}

static inline void BGRX_fillRGB(size_t offset, BYTE* WINPR_RESTRICT pRGB[2],
                                const BYTE* WINPR_RESTRICT pY[2], const BYTE* WINPR_RESTRICT pU[2],
                                const BYTE* WINPR_RESTRICT pV[2], BOOL filter)
{
	WINPR_ASSERT(pRGB);
	WINPR_ASSERT(pY);
	WINPR_ASSERT(pU);
	WINPR_ASSERT(pV);

	const UINT32 DstFormat = PIXEL_FORMAT_BGRX32;
	const UINT32 bpp = 4;

	for (size_t i = 0; i < 2; i++)
	{
		for (size_t j = 0; j < 2; j++)
		{
			const BYTE Y = pY[i][offset + j];
			BYTE U = pU[i][offset + j];
			BYTE V = pV[i][offset + j];
			if ((i == 0) && (j == 0) && filter)
			{
				const INT32 avgU =
				    4 * pU[0][offset] - pU[0][offset + 1] - pU[1][offset] - pU[1][offset + 1];
				const INT32 avgV =
				    4 * pV[0][offset] - pV[0][offset + 1] - pV[1][offset] - pV[1][offset + 1];

				U = CONDITIONAL_CLIP(avgU, pU[0][offset]);
				V = CONDITIONAL_CLIP(avgV, pV[0][offset]);
			}

			writeYUVPixel(&pRGB[i][(j + offset) * bpp], DstFormat, Y, U, V, writePixelBGRX);
		}
	}
}

/* input are uint16_t vectors */
static inline __m128i sse41_yuv2x_single(const __m128i Y, __m128i U, __m128i V, const short iMulU,
                                         const short iMulV)
{
	const __m128i zero = _mm_set1_epi8(0);

	__m128i Ylo = _mm_unpacklo_epi16(Y, zero);
	__m128i Yhi = _mm_unpackhi_epi16(Y, zero);
	if (iMulU != 0)
	{
		const __m128i addX = _mm_set1_epi16(128);
		const __m128i D = _mm_sub_epi16(U, addX);
		const __m128i mulU = _mm_set1_epi16(iMulU);
		const __m128i mulDlo = _mm_mullo_epi16(D, mulU);
		const __m128i mulDhi = _mm_mulhi_epi16(D, mulU);
		const __m128i Dlo = _mm_unpacklo_epi16(mulDlo, mulDhi);
		Ylo = _mm_add_epi32(Ylo, Dlo);

		const __m128i Dhi = _mm_unpackhi_epi16(mulDlo, mulDhi);
		Yhi = _mm_add_epi32(Yhi, Dhi);
	}
	if (iMulV != 0)
	{
		const __m128i addX = _mm_set1_epi16(128);
		const __m128i E = _mm_sub_epi16(V, addX);
		const __m128i mul = _mm_set1_epi16(iMulV);
		const __m128i mulElo = _mm_mullo_epi16(E, mul);
		const __m128i mulEhi = _mm_mulhi_epi16(E, mul);
		const __m128i Elo = _mm_unpacklo_epi16(mulElo, mulEhi);
		const __m128i esumlo = _mm_add_epi32(Ylo, Elo);

		const __m128i Ehi = _mm_unpackhi_epi16(mulElo, mulEhi);
		const __m128i esumhi = _mm_add_epi32(Yhi, Ehi);
		Ylo = esumlo;
		Yhi = esumhi;
	}

	const __m128i rYlo = _mm_srai_epi32(Ylo, 8);
	const __m128i rYhi = _mm_srai_epi32(Yhi, 8);
	const __m128i rY = _mm_packs_epi32(rYlo, rYhi);
	return rY;
}

/* Input are uint8_t vectors */
static inline __m128i sse41_yuv2x(const __m128i Y, __m128i U, __m128i V, const short iMulU,
                                  const short iMulV)
{
	const __m128i zero = _mm_set1_epi8(0);

	/* Ylo = Y * 256
	 * Ulo = uint8_t -> uint16_t
	 * Vlo = uint8_t -> uint16_t
	 */
	const __m128i Ylo = _mm_unpacklo_epi8(zero, Y);
	const __m128i Ulo = _mm_unpacklo_epi8(U, zero);
	const __m128i Vlo = _mm_unpacklo_epi8(V, zero);
	const __m128i preslo = sse41_yuv2x_single(Ylo, Ulo, Vlo, iMulU, iMulV);

	const __m128i Yhi = _mm_unpackhi_epi8(zero, Y);
	const __m128i Uhi = _mm_unpackhi_epi8(U, zero);
	const __m128i Vhi = _mm_unpackhi_epi8(V, zero);
	const __m128i preshi = sse41_yuv2x_single(Yhi, Uhi, Vhi, iMulU, iMulV);
	const __m128i res = _mm_packus_epi16(preslo, preshi);

	return res;
}

/* const INT32 r = ((256L * C(Y) + 0L * D(U) + 403L * E(V))) >> 8; */
static inline __m128i sse41_yuv2r(const __m128i Y, __m128i U, __m128i V)
{
	return sse41_yuv2x(Y, U, V, 0, 403);
}

/*  const INT32 g = ((256L * C(Y) - 48L * D(U) - 120L * E(V))) >> 8; */
static inline __m128i sse41_yuv2g(const __m128i Y, __m128i U, __m128i V)
{
	return sse41_yuv2x(Y, U, V, -48, -120);
}

/* const INT32 b = ((256L * C(Y) + 475L * D(U) + 0L * E(V))) >> 8; */
static inline __m128i sse41_yuv2b(const __m128i Y, __m128i U, __m128i V)
{
	return sse41_yuv2x(Y, U, V, 475, 0);
}

static inline void sse41_BGRX_fillRGB_pixel(BYTE* WINPR_RESTRICT pRGB, __m128i Y, __m128i U,
                                            __m128i V)
{
	const __m128i zero = _mm_set1_epi8(0);
	/* Y * 256 */
	const __m128i r = sse41_yuv2r(Y, U, V);
	const __m128i rx[2] = { _mm_unpackhi_epi8(r, zero), _mm_unpacklo_epi8(r, zero) };

	const __m128i g = sse41_yuv2g(Y, U, V);
	const __m128i b = sse41_yuv2b(Y, U, V);

	const __m128i bg[2] = { _mm_unpackhi_epi8(b, g), _mm_unpacklo_epi8(b, g) };

	const __m128i mask = mm_set_epu8(0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF,
	                                 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF);

	__m128i* rgb = (__m128i*)pRGB;
	const __m128i bgrx0 = _mm_unpacklo_epi16(bg[1], rx[1]);
	_mm_maskmoveu_si128(bgrx0, mask, (char*)&rgb[0]);
	const __m128i bgrx1 = _mm_unpackhi_epi16(bg[1], rx[1]);
	_mm_maskmoveu_si128(bgrx1, mask, (char*)&rgb[1]);
	const __m128i bgrx2 = _mm_unpacklo_epi16(bg[0], rx[0]);
	_mm_maskmoveu_si128(bgrx2, mask, (char*)&rgb[2]);
	const __m128i bgrx3 = _mm_unpackhi_epi16(bg[0], rx[0]);
	_mm_maskmoveu_si128(bgrx3, mask, (char*)&rgb[3]);
}

static inline __m128i odd1sum(__m128i u1)
{
	const __m128i zero = _mm_set1_epi8(0);
	const __m128i u1hi = _mm_unpackhi_epi8(u1, zero);
	const __m128i u1lo = _mm_unpacklo_epi8(u1, zero);
	return _mm_hadds_epi16(u1lo, u1hi);
}

static inline __m128i odd0sum(__m128i u0, __m128i u1sum)
{
	/* Mask out even bytes, extend uint8_t to uint16_t by filling in zero bytes,
	 * horizontally add the values */
	const __m128i mask = mm_set_epu8(0x80, 0x0F, 0x80, 0x0D, 0x80, 0x0B, 0x80, 0x09, 0x80, 0x07,
	                                 0x80, 0x05, 0x80, 0x03, 0x80, 0x01);
	const __m128i u0odd = _mm_shuffle_epi8(u0, mask);
	return _mm_adds_epi16(u1sum, u0odd);
}

static inline __m128i calcavg(__m128i u0even, __m128i sum)
{
	const __m128i u4zero = _mm_slli_epi16(u0even, 2);
	const __m128i uavg = _mm_sub_epi16(u4zero, sum);
	const __m128i zero = _mm_set1_epi8(0);
	const __m128i savg = _mm_packus_epi16(uavg, zero);
	const __m128i smask = mm_set_epu8(0x80, 0x07, 0x80, 0x06, 0x80, 0x05, 0x80, 0x04, 0x80, 0x03,
	                                  0x80, 0x02, 0x80, 0x01, 0x80, 0x00);
	return _mm_shuffle_epi8(savg, smask);
}

static inline __m128i diffmask(__m128i avg, __m128i u0even)
{
	/* Check for values >= 30 to apply the avg value to
	 * use int16 for calculations to avoid issues with signed 8bit integers
	 */
	const __m128i diff = _mm_subs_epi16(u0even, avg);
	const __m128i absdiff = _mm_abs_epi16(diff);
	const __m128i val30 = _mm_set1_epi16(30);
	return _mm_cmplt_epi16(absdiff, val30);
}

static inline void sse41_filter(__m128i pU[2])
{
	const __m128i u1sum = odd1sum(pU[1]);
	const __m128i sum = odd0sum(pU[0], u1sum);

	/* Mask out the odd bytes. We don´t need to do anything to make the uint8_t to uint16_t */
	const __m128i emask = mm_set_epu8(0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
	                                  0x00, 0xff, 0x00, 0xff, 0x00, 0xff);
	const __m128i u0even = _mm_and_si128(pU[0], emask);
	const __m128i avg = calcavg(u0even, sum);
	const __m128i umask = diffmask(avg, u0even);

	const __m128i u0orig = _mm_and_si128(u0even, umask);
	const __m128i u0avg = _mm_andnot_si128(umask, avg);
	const __m128i evenresult = _mm_or_si128(u0orig, u0avg);
	const __m128i omask = mm_set_epu8(0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	                                  0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00);
	const __m128i u0odd = _mm_and_si128(pU[0], omask);
	const __m128i result = _mm_or_si128(evenresult, u0odd);
	pU[0] = result;
}

static inline void sse41_BGRX_fillRGB(BYTE* WINPR_RESTRICT pRGB[2], const __m128i pY[2],
                                      __m128i pU[2], __m128i pV[2])
{
	WINPR_ASSERT(pRGB);
	WINPR_ASSERT(pY);
	WINPR_ASSERT(pU);
	WINPR_ASSERT(pV);

	sse41_filter(pU);
	sse41_filter(pV);

	for (size_t i = 0; i < 2; i++)
	{
		sse41_BGRX_fillRGB_pixel(pRGB[i], pY[i], pU[i], pV[i]);
	}
}

static inline pstatus_t sse41_YUV444ToRGB_8u_P3AC4R_BGRX_DOUBLE_ROW(
    BYTE* WINPR_RESTRICT pDst[2], const BYTE* WINPR_RESTRICT YData[2],
    const BYTE* WINPR_RESTRICT UData[2], const BYTE* WINPR_RESTRICT VData[2], UINT32 nWidth)
{
	WINPR_ASSERT((nWidth % 2) == 0);
	const UINT32 pad = nWidth % 16;

	size_t x = 0;
	for (; x < nWidth - pad; x += 16)
	{
		const __m128i Y[] = { LOAD_SI128(&YData[0][x]), LOAD_SI128(&YData[1][x]) };
		__m128i U[] = { LOAD_SI128(&UData[0][x]), LOAD_SI128(&UData[1][x]) };
		__m128i V[] = { LOAD_SI128(&VData[0][x]), LOAD_SI128(&VData[1][x]) };

		BYTE* dstp[] = { &pDst[0][x * 4], &pDst[1][x * 4] };
		sse41_BGRX_fillRGB(dstp, Y, U, V);
	}

	for (; x < nWidth; x += 2)
	{
		BGRX_fillRGB(x, pDst, YData, UData, VData, TRUE);
	}

	return PRIMITIVES_SUCCESS;
}

static inline void BGRX_fillRGB_single(size_t offset, BYTE* WINPR_RESTRICT pRGB,
                                       const BYTE* WINPR_RESTRICT pY, const BYTE* WINPR_RESTRICT pU,
                                       const BYTE* WINPR_RESTRICT pV, WINPR_ATTR_UNUSED BOOL filter)
{
	WINPR_ASSERT(pRGB);
	WINPR_ASSERT(pY);
	WINPR_ASSERT(pU);
	WINPR_ASSERT(pV);

	const UINT32 bpp = 4;

	for (size_t j = 0; j < 2; j++)
	{
		const BYTE Y = pY[offset + j];
		BYTE U = pU[offset + j];
		BYTE V = pV[offset + j];

		writeYUVPixel(&pRGB[(j + offset) * bpp], PIXEL_FORMAT_BGRX32, Y, U, V, writePixelBGRX);
	}
}

static inline pstatus_t sse41_YUV444ToRGB_8u_P3AC4R_BGRX_SINGLE_ROW(
    BYTE* WINPR_RESTRICT pDst, const BYTE* WINPR_RESTRICT YData, const BYTE* WINPR_RESTRICT UData,
    const BYTE* WINPR_RESTRICT VData, UINT32 nWidth)
{
	WINPR_ASSERT((nWidth % 2) == 0);

	for (size_t x = 0; x < nWidth; x += 2)
	{
		BGRX_fillRGB_single(x, pDst, YData, UData, VData, TRUE);
	}

	return PRIMITIVES_SUCCESS;
}

static inline pstatus_t sse41_YUV444ToRGB_8u_P3AC4R_BGRX(const BYTE* WINPR_RESTRICT pSrc[],
                                                         const UINT32 srcStep[],
                                                         BYTE* WINPR_RESTRICT pDst, UINT32 dstStep,
                                                         const prim_size_t* WINPR_RESTRICT roi)
{
	const UINT32 nWidth = roi->width;
	const UINT32 nHeight = roi->height;

	size_t y = 0;
	for (; y < nHeight - nHeight % 2; y += 2)
	{
		BYTE* dst[] = { (pDst + dstStep * y), (pDst + dstStep * (y + 1)) };
		const BYTE* YData[] = { pSrc[0] + y * srcStep[0], pSrc[0] + (y + 1) * srcStep[0] };
		const BYTE* UData[] = { pSrc[1] + y * srcStep[1], pSrc[1] + (y + 1) * srcStep[1] };
		const BYTE* VData[] = { pSrc[2] + y * srcStep[2], pSrc[2] + (y + 1) * srcStep[2] };

		const pstatus_t rc =
		    sse41_YUV444ToRGB_8u_P3AC4R_BGRX_DOUBLE_ROW(dst, YData, UData, VData, nWidth);
		if (rc != PRIMITIVES_SUCCESS)
			return rc;
	}
	for (; y < nHeight; y++)
	{
		BYTE* dst = (pDst + dstStep * y);
		const BYTE* YData = pSrc[0] + y * srcStep[0];
		const BYTE* UData = pSrc[1] + y * srcStep[1];
		const BYTE* VData = pSrc[2] + y * srcStep[2];
		const pstatus_t rc =
		    sse41_YUV444ToRGB_8u_P3AC4R_BGRX_SINGLE_ROW(dst, YData, UData, VData, nWidth);
		if (rc != PRIMITIVES_SUCCESS)
			return rc;
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t sse41_YUV444ToRGB_8u_P3AC4R(const BYTE* WINPR_RESTRICT pSrc[],
                                             const UINT32 srcStep[], BYTE* WINPR_RESTRICT pDst,
                                             UINT32 dstStep, UINT32 DstFormat,
                                             const prim_size_t* WINPR_RESTRICT roi)
{
	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRX32:
		case PIXEL_FORMAT_BGRA32:
			return sse41_YUV444ToRGB_8u_P3AC4R_BGRX(pSrc, srcStep, pDst, dstStep, roi);

		default:
			return generic->YUV444ToRGB_8u_P3AC4R(pSrc, srcStep, pDst, dstStep, DstFormat, roi);
	}
}

/****************************************************************************/
/* sse41 RGB -> YUV420 conversion                                          **/
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
#define BGRX_U_FACTORS \
	_mm_set_epi8(0, -29, -99, 127, 0, -29, -99, 127, 0, -29, -99, 127, 0, -29, -99, 127)
#define BGRX_V_FACTORS \
	_mm_set_epi8(0, 127, -116, -12, 0, 127, -116, -12, 0, 127, -116, -12, 0, 127, -116, -12)
#define CONST128_FACTORS _mm_set1_epi8(-128)

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

static inline void sse41_BGRX_TO_YUV(const BYTE* WINPR_RESTRICT pLine1, BYTE* WINPR_RESTRICT pYLine,
                                     BYTE* WINPR_RESTRICT pULine, BYTE* WINPR_RESTRICT pVLine)
{
	const BYTE r1 = pLine1[2];
	const BYTE g1 = pLine1[1];
	const BYTE b1 = pLine1[0];

	if (pYLine)
		pYLine[0] = RGB2Y(r1, g1, b1);
	if (pULine)
		pULine[0] = RGB2U(r1, g1, b1);
	if (pVLine)
		pVLine[0] = RGB2V(r1, g1, b1);
}

/* compute the luma (Y) component from a single rgb source line */

static INLINE void sse41_RGBToYUV420_BGRX_Y(const BYTE* WINPR_RESTRICT src, BYTE* dst, UINT32 width)
{
	const __m128i y_factors = BGRX_Y_FACTORS;
	const __m128i* argb = (const __m128i*)src;
	__m128i* ydst = (__m128i*)dst;

	UINT32 x = 0;

	for (; x < width - width % 16; x += 16)
	{
		/* store 16 rgba pixels in 4 128 bit registers */
		__m128i x0 = LOAD_SI128(argb++); // 1st 4 pixels
		{
			x0 = _mm_maddubs_epi16(x0, y_factors);

			__m128i x1 = LOAD_SI128(argb++); // 2nd 4 pixels
			x1 = _mm_maddubs_epi16(x1, y_factors);
			x0 = _mm_hadds_epi16(x0, x1);
			x0 = _mm_srli_epi16(x0, Y_SHIFT);
		}

		__m128i x2 = LOAD_SI128(argb++); // 3rd 4 pixels
		{
			x2 = _mm_maddubs_epi16(x2, y_factors);

			__m128i x3 = LOAD_SI128(argb++); // 4th 4 pixels
			x3 = _mm_maddubs_epi16(x3, y_factors);
			x2 = _mm_hadds_epi16(x2, x3);
			x2 = _mm_srli_epi16(x2, Y_SHIFT);
		}

		x0 = _mm_packus_epi16(x0, x2);
		/* save to y plane */
		STORE_SI128(ydst++, x0);
	}

	for (; x < width; x++)
	{
		sse41_BGRX_TO_YUV(&src[4ULL * x], &dst[x], NULL, NULL);
	}
}

/* compute the chrominance (UV) components from two rgb source lines */

static INLINE void sse41_RGBToYUV420_BGRX_UV(const BYTE* WINPR_RESTRICT src1,
                                             const BYTE* WINPR_RESTRICT src2,
                                             BYTE* WINPR_RESTRICT dst1, BYTE* WINPR_RESTRICT dst2,
                                             UINT32 width)
{
	const __m128i u_factors = BGRX_U_FACTORS;
	const __m128i v_factors = BGRX_V_FACTORS;
	const __m128i vector128 = CONST128_FACTORS;

	size_t x = 0;

	for (; x < width - width % 16; x += 16)
	{
		const __m128i* rgb1 = (const __m128i*)&src1[4ULL * x];
		const __m128i* rgb2 = (const __m128i*)&src2[4ULL * x];
		__m64* udst = (__m64*)&dst1[x / 2];
		__m64* vdst = (__m64*)&dst2[x / 2];

		/* subsample 16x2 pixels into 16x1 pixels */
		__m128i x0 = LOAD_SI128(&rgb1[0]);
		__m128i x4 = LOAD_SI128(&rgb2[0]);
		x0 = _mm_avg_epu8(x0, x4);

		__m128i x1 = LOAD_SI128(&rgb1[1]);
		x4 = LOAD_SI128(&rgb2[1]);
		x1 = _mm_avg_epu8(x1, x4);

		__m128i x2 = LOAD_SI128(&rgb1[2]);
		x4 = LOAD_SI128(&rgb2[2]);
		x2 = _mm_avg_epu8(x2, x4);

		__m128i x3 = LOAD_SI128(&rgb1[3]);
		x4 = LOAD_SI128(&rgb2[3]);
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
		__m128i x5 = _mm_maddubs_epi16(x1, v_factors);
		/* the total sums */
		x0 = _mm_hadd_epi16(x2, x3);
		x1 = _mm_hadd_epi16(x4, x5);
		/* shift the results */
		x0 = _mm_srai_epi16(x0, U_SHIFT);
		x1 = _mm_srai_epi16(x1, V_SHIFT);
		/* pack the 16 words into bytes */
		x0 = _mm_packs_epi16(x0, x1);
		/* add 128 */
		x0 = _mm_sub_epi8(x0, vector128);
		/* the lower 8 bytes go to the u plane */
		_mm_storel_pi(udst, _mm_castsi128_ps(x0));
		/* the upper 8 bytes go to the v plane */
		_mm_storeh_pi(vdst, _mm_castsi128_ps(x0));
	}

	for (; x < width - width % 2; x += 2)
	{
		BYTE u[4] = { 0 };
		BYTE v[4] = { 0 };
		sse41_BGRX_TO_YUV(&src1[4ULL * x], NULL, &u[0], &v[0]);
		sse41_BGRX_TO_YUV(&src1[4ULL * (1ULL + x)], NULL, &u[1], &v[1]);
		sse41_BGRX_TO_YUV(&src2[4ULL * x], NULL, &u[2], &v[2]);
		sse41_BGRX_TO_YUV(&src2[4ULL * (1ULL + x)], NULL, &u[3], &v[3]);
		const INT16 u4 = WINPR_ASSERTING_INT_CAST(INT16, (INT16)u[0] + u[1] + u[2] + u[3]);
		const INT16 uu = WINPR_ASSERTING_INT_CAST(INT16, u4 / 4);
		const BYTE u8 = CLIP(uu);
		dst1[x / 2] = u8;

		const INT16 v4 = WINPR_ASSERTING_INT_CAST(INT16, (INT16)v[0] + v[1] + v[2] + v[3]);
		const INT16 vu = WINPR_ASSERTING_INT_CAST(INT16, v4 / 4);
		const BYTE v8 = CLIP(vu);
		dst2[x / 2] = v8;
	}
}

static pstatus_t sse41_RGBToYUV420_BGRX(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcStep,
                                        BYTE* WINPR_RESTRICT pDst[], const UINT32 dstStep[],
                                        const prim_size_t* WINPR_RESTRICT roi)
{
	if (roi->height < 1 || roi->width < 1)
	{
		return !PRIMITIVES_SUCCESS;
	}

	size_t y = 0;
	for (; y < roi->height - roi->height % 2; y += 2)
	{
		const BYTE* line1 = &pSrc[y * srcStep];
		const BYTE* line2 = &pSrc[(1ULL + y) * srcStep];
		BYTE* ydst1 = &pDst[0][y * dstStep[0]];
		BYTE* ydst2 = &pDst[0][(1ULL + y) * dstStep[0]];
		BYTE* udst = &pDst[1][y / 2 * dstStep[1]];
		BYTE* vdst = &pDst[2][y / 2 * dstStep[2]];

		sse41_RGBToYUV420_BGRX_UV(line1, line2, udst, vdst, roi->width);
		sse41_RGBToYUV420_BGRX_Y(line1, ydst1, roi->width);
		sse41_RGBToYUV420_BGRX_Y(line2, ydst2, roi->width);
	}

	for (; y < roi->height; y++)
	{
		const BYTE* line = &pSrc[y * srcStep];
		BYTE* ydst = &pDst[0][1ULL * y * dstStep[0]];
		sse41_RGBToYUV420_BGRX_Y(line, ydst, roi->width);
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t sse41_RGBToYUV420(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcFormat,
                                   UINT32 srcStep, BYTE* WINPR_RESTRICT pDst[],
                                   const UINT32 dstStep[], const prim_size_t* WINPR_RESTRICT roi)
{
	switch (srcFormat)
	{
		case PIXEL_FORMAT_BGRX32:
		case PIXEL_FORMAT_BGRA32:
			return sse41_RGBToYUV420_BGRX(pSrc, srcStep, pDst, dstStep, roi);

		default:
			return generic->RGBToYUV420_8u_P3AC4R(pSrc, srcFormat, srcStep, pDst, dstStep, roi);
	}
}

/****************************************************************************/
/* sse41 RGB -> AVC444-YUV conversion                                      **/
/****************************************************************************/

static INLINE void sse41_RGBToAVC444YUV_BGRX_DOUBLE_ROW(
    const BYTE* WINPR_RESTRICT srcEven, const BYTE* WINPR_RESTRICT srcOdd,
    BYTE* WINPR_RESTRICT b1Even, BYTE* WINPR_RESTRICT b1Odd, BYTE* WINPR_RESTRICT b2,
    BYTE* WINPR_RESTRICT b3, BYTE* WINPR_RESTRICT b4, BYTE* WINPR_RESTRICT b5,
    BYTE* WINPR_RESTRICT b6, BYTE* WINPR_RESTRICT b7, UINT32 width)
{
	const __m128i* argbEven = (const __m128i*)srcEven;
	const __m128i* argbOdd = (const __m128i*)srcOdd;
	const __m128i y_factors = BGRX_Y_FACTORS;
	const __m128i u_factors = BGRX_U_FACTORS;
	const __m128i v_factors = BGRX_V_FACTORS;
	const __m128i vector128 = CONST128_FACTORS;

	UINT32 x = 0;
	for (; x < width - width % 16; x += 16)
	{
		/* store 16 rgba pixels in 4 128 bit registers */
		const __m128i xe1 = LOAD_SI128(argbEven++); // 1st 4 pixels
		const __m128i xe2 = LOAD_SI128(argbEven++); // 2nd 4 pixels
		const __m128i xe3 = LOAD_SI128(argbEven++); // 3rd 4 pixels
		const __m128i xe4 = LOAD_SI128(argbEven++); // 4th 4 pixels
		const __m128i xo1 = LOAD_SI128(argbOdd++);  // 1st 4 pixels
		const __m128i xo2 = LOAD_SI128(argbOdd++);  // 2nd 4 pixels
		const __m128i xo3 = LOAD_SI128(argbOdd++);  // 3rd 4 pixels
		const __m128i xo4 = LOAD_SI128(argbOdd++);  // 4th 4 pixels
		{
			/* Y: multiplications with subtotals and horizontal sums */
			const __m128i ye1 = _mm_srli_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe1, y_factors),
			                                                  _mm_maddubs_epi16(xe2, y_factors)),
			                                   Y_SHIFT);
			const __m128i ye2 = _mm_srli_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe3, y_factors),
			                                                  _mm_maddubs_epi16(xe4, y_factors)),
			                                   Y_SHIFT);
			const __m128i ye = _mm_packus_epi16(ye1, ye2);
			const __m128i yo1 = _mm_srli_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo1, y_factors),
			                                                  _mm_maddubs_epi16(xo2, y_factors)),
			                                   Y_SHIFT);
			const __m128i yo2 = _mm_srli_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo3, y_factors),
			                                                  _mm_maddubs_epi16(xo4, y_factors)),
			                                   Y_SHIFT);
			const __m128i yo = _mm_packus_epi16(yo1, yo2);
			/* store y [b1] */
			STORE_SI128(b1Even, ye);
			b1Even += 16;

			if (b1Odd)
			{
				STORE_SI128(b1Odd, yo);
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
			__m128i ue;
			__m128i uo = { 0 };
			{
				const __m128i ue1 =
				    _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe1, u_factors),
				                                  _mm_maddubs_epi16(xe2, u_factors)),
				                   U_SHIFT);
				const __m128i ue2 =
				    _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe3, u_factors),
				                                  _mm_maddubs_epi16(xe4, u_factors)),
				                   U_SHIFT);
				ue = _mm_sub_epi8(_mm_packs_epi16(ue1, ue2), vector128);
			}

			if (b1Odd)
			{
				const __m128i uo1 =
				    _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo1, u_factors),
				                                  _mm_maddubs_epi16(xo2, u_factors)),
				                   U_SHIFT);
				const __m128i uo2 =
				    _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo3, u_factors),
				                                  _mm_maddubs_epi16(xo4, u_factors)),
				                   U_SHIFT);
				uo = _mm_sub_epi8(_mm_packs_epi16(uo1, uo2), vector128);
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
				const __m128i mask =
				    _mm_set_epi8((char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80,
				                 (char)0x80, (char)0x80, (char)0x80, 14, 12, 10, 8, 6, 4, 2, 0);
				const __m128i ud = _mm_shuffle_epi8(ue, mask);
				_mm_storel_epi64((__m128i*)b2, ud);
			}

			b2 += 8;

			if (b1Odd) /* b4 */
			{
				STORE_SI128(b4, uo);
				b4 += 16;
			}

			{
				/* b6 */
				const __m128i mask =
				    _mm_set_epi8((char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80,
				                 (char)0x80, (char)0x80, (char)0x80, 15, 13, 11, 9, 7, 5, 3, 1);
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
			__m128i ve;
			__m128i vo = { 0 };
			{
				const __m128i ve1 =
				    _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe1, v_factors),
				                                  _mm_maddubs_epi16(xe2, v_factors)),
				                   V_SHIFT);
				const __m128i ve2 =
				    _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe3, v_factors),
				                                  _mm_maddubs_epi16(xe4, v_factors)),
				                   V_SHIFT);
				ve = _mm_sub_epi8(_mm_packs_epi16(ve1, ve2), vector128);
			}

			if (b1Odd)
			{
				const __m128i vo1 =
				    _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo1, v_factors),
				                                  _mm_maddubs_epi16(xo2, v_factors)),
				                   V_SHIFT);
				const __m128i vo2 =
				    _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo3, v_factors),
				                                  _mm_maddubs_epi16(xo4, v_factors)),
				                   V_SHIFT);
				vo = _mm_sub_epi8(_mm_packs_epi16(vo1, vo2), vector128);
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
				const __m128i mask =
				    _mm_set_epi8((char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80,
				                 (char)0x80, (char)0x80, (char)0x80, 14, 12, 10, 8, 6, 4, 2, 0);
				const __m128i vd = _mm_shuffle_epi8(ve, mask);
				_mm_storel_epi64((__m128i*)b3, vd);
			}

			b3 += 8;

			if (b1Odd) /* b5 */
			{
				STORE_SI128(b5, vo);
				b5 += 16;
			}

			{
				/* b7 */
				const __m128i mask =
				    _mm_set_epi8((char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80,
				                 (char)0x80, (char)0x80, (char)0x80, 15, 13, 11, 9, 7, 5, 3, 1);
				const __m128i vde = _mm_shuffle_epi8(ve, mask);
				_mm_storel_epi64((__m128i*)b7, vde);
				b7 += 8;
			}
		}
	}

	general_RGBToAVC444YUV_BGRX_DOUBLE_ROW(x, srcEven, srcOdd, b1Even, b1Odd, b2, b3, b4, b5, b6,
	                                       b7, width);
}

static pstatus_t sse41_RGBToAVC444YUV_BGRX(const BYTE* WINPR_RESTRICT pSrc,
                                           WINPR_ATTR_UNUSED UINT32 srcFormat, UINT32 srcStep,
                                           BYTE* WINPR_RESTRICT pDst1[], const UINT32 dst1Step[],
                                           BYTE* WINPR_RESTRICT pDst2[], const UINT32 dst2Step[],
                                           const prim_size_t* WINPR_RESTRICT roi)
{
	if (roi->height < 1 || roi->width < 1)
		return !PRIMITIVES_SUCCESS;

	size_t y = 0;
	for (; y < roi->height - roi->height % 2; y += 2)
	{
		const BYTE* srcEven = pSrc + y * srcStep;
		const BYTE* srcOdd = pSrc + (y + 1) * srcStep;
		const size_t i = y >> 1;
		const size_t n = (i & (size_t)~7) + i;
		BYTE* b1Even = pDst1[0] + y * dst1Step[0];
		BYTE* b1Odd = (b1Even + dst1Step[0]);
		BYTE* b2 = pDst1[1] + (y / 2) * dst1Step[1];
		BYTE* b3 = pDst1[2] + (y / 2) * dst1Step[2];
		BYTE* b4 = pDst2[0] + 1ULL * dst2Step[0] * n;
		BYTE* b5 = b4 + 8ULL * dst2Step[0];
		BYTE* b6 = pDst2[1] + (y / 2) * dst2Step[1];
		BYTE* b7 = pDst2[2] + (y / 2) * dst2Step[2];
		sse41_RGBToAVC444YUV_BGRX_DOUBLE_ROW(srcEven, srcOdd, b1Even, b1Odd, b2, b3, b4, b5, b6, b7,
		                                     roi->width);
	}

	for (; y < roi->height; y++)
	{
		const BYTE* srcEven = pSrc + y * srcStep;
		BYTE* b1Even = pDst1[0] + y * dst1Step[0];
		BYTE* b2 = pDst1[1] + (y / 2) * dst1Step[1];
		BYTE* b3 = pDst1[2] + (y / 2) * dst1Step[2];
		BYTE* b6 = pDst2[1] + (y / 2) * dst2Step[1];
		BYTE* b7 = pDst2[2] + (y / 2) * dst2Step[2];
		general_RGBToAVC444YUV_BGRX_DOUBLE_ROW(0, srcEven, NULL, b1Even, NULL, b2, b3, NULL, NULL,
		                                       b6, b7, roi->width);
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t sse41_RGBToAVC444YUV(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcFormat,
                                      UINT32 srcStep, BYTE* WINPR_RESTRICT pDst1[],
                                      const UINT32 dst1Step[], BYTE* WINPR_RESTRICT pDst2[],
                                      const UINT32 dst2Step[],
                                      const prim_size_t* WINPR_RESTRICT roi)
{
	switch (srcFormat)
	{
		case PIXEL_FORMAT_BGRX32:
		case PIXEL_FORMAT_BGRA32:
			return sse41_RGBToAVC444YUV_BGRX(pSrc, srcFormat, srcStep, pDst1, dst1Step, pDst2,
			                                 dst2Step, roi);

		default:
			return generic->RGBToAVC444YUV(pSrc, srcFormat, srcStep, pDst1, dst1Step, pDst2,
			                               dst2Step, roi);
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
static INLINE void sse41_RGBToAVC444YUVv2_BGRX_DOUBLE_ROW(
    const BYTE* WINPR_RESTRICT srcEven, const BYTE* WINPR_RESTRICT srcOdd,
    BYTE* WINPR_RESTRICT yLumaDstEven, BYTE* WINPR_RESTRICT yLumaDstOdd,
    BYTE* WINPR_RESTRICT uLumaDst, BYTE* WINPR_RESTRICT vLumaDst,
    BYTE* WINPR_RESTRICT yEvenChromaDst1, BYTE* WINPR_RESTRICT yEvenChromaDst2,
    BYTE* WINPR_RESTRICT yOddChromaDst1, BYTE* WINPR_RESTRICT yOddChromaDst2,
    BYTE* WINPR_RESTRICT uChromaDst1, BYTE* WINPR_RESTRICT uChromaDst2,
    BYTE* WINPR_RESTRICT vChromaDst1, BYTE* WINPR_RESTRICT vChromaDst2, UINT32 width)
{
	const __m128i vector128 = CONST128_FACTORS;
	const __m128i* argbEven = (const __m128i*)srcEven;
	const __m128i* argbOdd = (const __m128i*)srcOdd;

	UINT32 x = 0;
	for (; x < width - width % 16; x += 16)
	{
		/* store 16 rgba pixels in 4 128 bit registers
		 * for even and odd rows.
		 */
		const __m128i xe1 = LOAD_SI128(argbEven++); /* 1st 4 pixels */
		const __m128i xe2 = LOAD_SI128(argbEven++); /* 2nd 4 pixels */
		const __m128i xe3 = LOAD_SI128(argbEven++); /* 3rd 4 pixels */
		const __m128i xe4 = LOAD_SI128(argbEven++); /* 4th 4 pixels */
		const __m128i xo1 = LOAD_SI128(argbOdd++);  /* 1st 4 pixels */
		const __m128i xo2 = LOAD_SI128(argbOdd++);  /* 2nd 4 pixels */
		const __m128i xo3 = LOAD_SI128(argbOdd++);  /* 3rd 4 pixels */
		const __m128i xo4 = LOAD_SI128(argbOdd++);  /* 4th 4 pixels */
		{
			/* Y: multiplications with subtotals and horizontal sums */
			const __m128i y_factors = BGRX_Y_FACTORS;
			const __m128i ye1 = _mm_srli_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe1, y_factors),
			                                                  _mm_maddubs_epi16(xe2, y_factors)),
			                                   Y_SHIFT);
			const __m128i ye2 = _mm_srli_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe3, y_factors),
			                                                  _mm_maddubs_epi16(xe4, y_factors)),
			                                   Y_SHIFT);
			const __m128i ye = _mm_packus_epi16(ye1, ye2);
			/* store y [b1] */
			STORE_SI128(yLumaDstEven, ye);
			yLumaDstEven += 16;
		}

		if (yLumaDstOdd)
		{
			const __m128i y_factors = BGRX_Y_FACTORS;
			const __m128i yo1 = _mm_srli_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo1, y_factors),
			                                                  _mm_maddubs_epi16(xo2, y_factors)),
			                                   Y_SHIFT);
			const __m128i yo2 = _mm_srli_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo3, y_factors),
			                                                  _mm_maddubs_epi16(xo4, y_factors)),
			                                   Y_SHIFT);
			const __m128i yo = _mm_packus_epi16(yo1, yo2);
			STORE_SI128(yLumaDstOdd, yo);
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
			__m128i ue;
			__m128i uo;
			__m128i uavg;
			{
				const __m128i u_factors = BGRX_U_FACTORS;
				const __m128i ue1 =
				    _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe1, u_factors),
				                                  _mm_maddubs_epi16(xe2, u_factors)),
				                   U_SHIFT);
				const __m128i ue2 =
				    _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe3, u_factors),
				                                  _mm_maddubs_epi16(xe4, u_factors)),
				                   U_SHIFT);
				const __m128i ueavg = _mm_hadd_epi16(ue1, ue2);
				ue = _mm_sub_epi8(_mm_packs_epi16(ue1, ue2), vector128);
				uavg = ueavg;
			}
			{
				const __m128i u_factors = BGRX_U_FACTORS;
				const __m128i uo1 =
				    _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo1, u_factors),
				                                  _mm_maddubs_epi16(xo2, u_factors)),
				                   U_SHIFT);
				const __m128i uo2 =
				    _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo3, u_factors),
				                                  _mm_maddubs_epi16(xo4, u_factors)),
				                   U_SHIFT);
				const __m128i uoavg = _mm_hadd_epi16(uo1, uo2);
				uo = _mm_sub_epi8(_mm_packs_epi16(uo1, uo2), vector128);
				uavg = _mm_add_epi16(uavg, uoavg);
				uavg = _mm_srai_epi16(uavg, 2);
				uavg = _mm_packs_epi16(uavg, uoavg);
				uavg = _mm_sub_epi8(uavg, vector128);
			}
			/* Now we need the following storage distribution:
			 * 2x   2y    -> uLumaDst
			 * 2x+1  y    -> yChromaDst1
			 * 4x   2y+1  -> uChromaDst1
			 * 4x+2 2y+1  -> vChromaDst1 */
			{
				const __m128i mask =
				    _mm_set_epi8((char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80,
				                 (char)0x80, (char)0x80, (char)0x80, 15, 13, 11, 9, 7, 5, 3, 1);
				const __m128i ude = _mm_shuffle_epi8(ue, mask);
				_mm_storel_epi64((__m128i*)yEvenChromaDst1, ude);
				yEvenChromaDst1 += 8;
			}

			if (yLumaDstOdd)
			{
				const __m128i mask =
				    _mm_set_epi8((char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80,
				                 (char)0x80, (char)0x80, (char)0x80, 15, 13, 11, 9, 7, 5, 3, 1);
				const __m128i udo /* codespell:ignore udo */ = _mm_shuffle_epi8(uo, mask);
				_mm_storel_epi64((__m128i*)yOddChromaDst1, udo); // codespell:ignore udo
				yOddChromaDst1 += 8;
			}

			if (yLumaDstOdd)
			{
				const __m128i mask =
				    _mm_set_epi8((char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80,
				                 (char)0x80, (char)0x80, (char)0x80, 14, 10, 6, 2, 12, 8, 4, 0);
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
				const __m128i mask =
				    _mm_set_epi8((char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80,
				                 (char)0x80, (char)0x80, (char)0x80, 14, 12, 10, 8, 6, 4, 2, 0);
				const __m128i ud = _mm_shuffle_epi8(ue, mask);
				_mm_storel_epi64((__m128i*)uLumaDst, ud);
				uLumaDst += 8;
			}
		}

		{
			/* V: multiplications with subtotals and horizontal sums */
			__m128i ve;
			__m128i vo;
			__m128i vavg;
			{
				const __m128i v_factors = BGRX_V_FACTORS;
				const __m128i ve1 =
				    _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe1, v_factors),
				                                  _mm_maddubs_epi16(xe2, v_factors)),
				                   V_SHIFT);
				const __m128i ve2 =
				    _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xe3, v_factors),
				                                  _mm_maddubs_epi16(xe4, v_factors)),
				                   V_SHIFT);
				const __m128i veavg = _mm_hadd_epi16(ve1, ve2);
				ve = _mm_sub_epi8(_mm_packs_epi16(ve1, ve2), vector128);
				vavg = veavg;
			}
			{
				const __m128i v_factors = BGRX_V_FACTORS;
				const __m128i vo1 =
				    _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo1, v_factors),
				                                  _mm_maddubs_epi16(xo2, v_factors)),
				                   V_SHIFT);
				const __m128i vo2 =
				    _mm_srai_epi16(_mm_hadd_epi16(_mm_maddubs_epi16(xo3, v_factors),
				                                  _mm_maddubs_epi16(xo4, v_factors)),
				                   V_SHIFT);
				const __m128i voavg = _mm_hadd_epi16(vo1, vo2);
				vo = _mm_sub_epi8(_mm_packs_epi16(vo1, vo2), vector128);
				vavg = _mm_add_epi16(vavg, voavg);
				vavg = _mm_srai_epi16(vavg, 2);
				vavg = _mm_packs_epi16(vavg, voavg);
				vavg = _mm_sub_epi8(vavg, vector128);
			}
			/* Now we need the following storage distribution:
			 * 2x   2y    -> vLumaDst
			 * 2x+1  y    -> yChromaDst2
			 * 4x   2y+1  -> uChromaDst2
			 * 4x+2 2y+1  -> vChromaDst2 */
			{
				const __m128i mask =
				    _mm_set_epi8((char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80,
				                 (char)0x80, (char)0x80, (char)0x80, 15, 13, 11, 9, 7, 5, 3, 1);
				__m128i vde = _mm_shuffle_epi8(ve, mask);
				_mm_storel_epi64((__m128i*)yEvenChromaDst2, vde);
				yEvenChromaDst2 += 8;
			}

			if (yLumaDstOdd)
			{
				const __m128i mask =
				    _mm_set_epi8((char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80,
				                 (char)0x80, (char)0x80, (char)0x80, 15, 13, 11, 9, 7, 5, 3, 1);
				__m128i vdo = _mm_shuffle_epi8(vo, mask);
				_mm_storel_epi64((__m128i*)yOddChromaDst2, vdo);
				yOddChromaDst2 += 8;
			}

			if (yLumaDstOdd)
			{
				const __m128i mask =
				    _mm_set_epi8((char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80,
				                 (char)0x80, (char)0x80, (char)0x80, 14, 10, 6, 2, 12, 8, 4, 0);
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
				const __m128i mask =
				    _mm_set_epi8((char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80,
				                 (char)0x80, (char)0x80, (char)0x80, 14, 12, 10, 8, 6, 4, 2, 0);
				__m128i vd = _mm_shuffle_epi8(ve, mask);
				_mm_storel_epi64((__m128i*)vLumaDst, vd);
				vLumaDst += 8;
			}
		}
	}

	general_RGBToAVC444YUVv2_BGRX_DOUBLE_ROW(x, srcEven, srcOdd, yLumaDstEven, yLumaDstOdd,
	                                         uLumaDst, vLumaDst, yEvenChromaDst1, yEvenChromaDst2,
	                                         yOddChromaDst1, yOddChromaDst2, uChromaDst1,
	                                         uChromaDst2, vChromaDst1, vChromaDst2, width);
}

static pstatus_t sse41_RGBToAVC444YUVv2_BGRX(const BYTE* WINPR_RESTRICT pSrc,
                                             WINPR_ATTR_UNUSED UINT32 srcFormat, UINT32 srcStep,
                                             BYTE* WINPR_RESTRICT pDst1[], const UINT32 dst1Step[],
                                             BYTE* WINPR_RESTRICT pDst2[], const UINT32 dst2Step[],
                                             const prim_size_t* WINPR_RESTRICT roi)
{
	if (roi->height < 1 || roi->width < 1)
		return !PRIMITIVES_SUCCESS;

	size_t y = 0;
	for (; y < roi->height - roi->height % 2; y += 2)
	{
		const BYTE* srcEven = (pSrc + y * srcStep);
		const BYTE* srcOdd = (srcEven + srcStep);
		BYTE* dstLumaYEven = (pDst1[0] + y * dst1Step[0]);
		BYTE* dstLumaYOdd = (dstLumaYEven + dst1Step[0]);
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
		sse41_RGBToAVC444YUVv2_BGRX_DOUBLE_ROW(srcEven, srcOdd, dstLumaYEven, dstLumaYOdd, dstLumaU,
		                                       dstLumaV, dstEvenChromaY1, dstEvenChromaY2,
		                                       dstOddChromaY1, dstOddChromaY2, dstChromaU1,
		                                       dstChromaU2, dstChromaV1, dstChromaV2, roi->width);
	}

	for (; y < roi->height; y++)
	{
		const BYTE* srcEven = (pSrc + y * srcStep);
		BYTE* dstLumaYEven = (pDst1[0] + y * dst1Step[0]);
		BYTE* dstLumaU = (pDst1[1] + (y / 2) * dst1Step[1]);
		BYTE* dstLumaV = (pDst1[2] + (y / 2) * dst1Step[2]);
		BYTE* dstEvenChromaY1 = (pDst2[0] + y * dst2Step[0]);
		BYTE* dstEvenChromaY2 = dstEvenChromaY1 + roi->width / 2;
		BYTE* dstChromaU1 = (pDst2[1] + (y / 2) * dst2Step[1]);
		BYTE* dstChromaV1 = (pDst2[2] + (y / 2) * dst2Step[2]);
		BYTE* dstChromaU2 = dstChromaU1 + roi->width / 4;
		BYTE* dstChromaV2 = dstChromaV1 + roi->width / 4;
		general_RGBToAVC444YUVv2_BGRX_DOUBLE_ROW(0, srcEven, NULL, dstLumaYEven, NULL, dstLumaU,
		                                         dstLumaV, dstEvenChromaY1, dstEvenChromaY2, NULL,
		                                         NULL, dstChromaU1, dstChromaU2, dstChromaV1,
		                                         dstChromaV2, roi->width);
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t sse41_RGBToAVC444YUVv2(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcFormat,
                                        UINT32 srcStep, BYTE* WINPR_RESTRICT pDst1[],
                                        const UINT32 dst1Step[], BYTE* WINPR_RESTRICT pDst2[],
                                        const UINT32 dst2Step[],
                                        const prim_size_t* WINPR_RESTRICT roi)
{
	switch (srcFormat)
	{
		case PIXEL_FORMAT_BGRX32:
		case PIXEL_FORMAT_BGRA32:
			return sse41_RGBToAVC444YUVv2_BGRX(pSrc, srcFormat, srcStep, pDst1, dst1Step, pDst2,
			                                   dst2Step, roi);

		default:
			return generic->RGBToAVC444YUVv2(pSrc, srcFormat, srcStep, pDst1, dst1Step, pDst2,
			                                 dst2Step, roi);
	}
}

static pstatus_t sse41_LumaToYUV444(const BYTE* WINPR_RESTRICT pSrcRaw[], const UINT32 srcStep[],
                                    BYTE* WINPR_RESTRICT pDstRaw[], const UINT32 dstStep[],
                                    const RECTANGLE_16* WINPR_RESTRICT roi)
{
	const UINT32 nWidth = roi->right - roi->left;
	const UINT32 nHeight = roi->bottom - roi->top;
	const UINT32 halfWidth = (nWidth + 1) / 2;
	const UINT32 halfPad = halfWidth % 16;
	const UINT32 halfHeight = (nHeight + 1) / 2;
	const UINT32 oddY = 1;
	const UINT32 evenY = 0;
	const UINT32 oddX = 1;
	const UINT32 evenX = 0;
	const BYTE* pSrc[3] = { pSrcRaw[0] + 1ULL * roi->top * srcStep[0] + roi->left,
		                    pSrcRaw[1] + 1ULL * roi->top / 2 * srcStep[1] + roi->left / 2,
		                    pSrcRaw[2] + 1ULL * roi->top / 2 * srcStep[2] + roi->left / 2 };
	BYTE* pDst[3] = { pDstRaw[0] + 1ULL * roi->top * dstStep[0] + roi->left,
		              pDstRaw[1] + 1ULL * roi->top * dstStep[1] + roi->left,
		              pDstRaw[2] + 1ULL * roi->top * dstStep[2] + roi->left };

	/* Y data is already here... */
	/* B1 */
	for (size_t y = 0; y < nHeight; y++)
	{
		const BYTE* Ym = pSrc[0] + y * srcStep[0];
		BYTE* pY = pDst[0] + y * dstStep[0];
		memcpy(pY, Ym, nWidth);
	}

	/* The first half of U, V are already here part of this frame. */
	/* B2 and B3 */
	for (size_t y = 0; y < halfHeight; y++)
	{
		const size_t val2y = (2 * y + evenY);
		const size_t val2y1 = val2y + oddY;
		const BYTE* Um = pSrc[1] + 1ULL * srcStep[1] * y;
		const BYTE* Vm = pSrc[2] + 1ULL * srcStep[2] * y;
		BYTE* pU = pDst[1] + 1ULL * dstStep[1] * val2y;
		BYTE* pV = pDst[2] + 1ULL * dstStep[2] * val2y;
		BYTE* pU1 = pDst[1] + 1ULL * dstStep[1] * val2y1;
		BYTE* pV1 = pDst[2] + 1ULL * dstStep[2] * val2y1;

		size_t x = 0;
		for (; x < halfWidth - halfPad; x += 16)
		{
			const __m128i unpackHigh = _mm_set_epi8(7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0);
			const __m128i unpackLow =
			    _mm_set_epi8(15, 15, 14, 14, 13, 13, 12, 12, 11, 11, 10, 10, 9, 9, 8, 8);
			{
				const __m128i u = LOAD_SI128(&Um[x]);
				const __m128i uHigh = _mm_shuffle_epi8(u, unpackHigh);
				const __m128i uLow = _mm_shuffle_epi8(u, unpackLow);
				STORE_SI128(&pU[2ULL * x], uHigh);
				STORE_SI128(&pU[2ULL * x + 16], uLow);
				STORE_SI128(&pU1[2ULL * x], uHigh);
				STORE_SI128(&pU1[2ULL * x + 16], uLow);
			}
			{
				const __m128i u = LOAD_SI128(&Vm[x]);
				const __m128i uHigh = _mm_shuffle_epi8(u, unpackHigh);
				const __m128i uLow = _mm_shuffle_epi8(u, unpackLow);
				STORE_SI128(&pV[2 * x], uHigh);
				STORE_SI128(&pV[2 * x + 16], uLow);
				STORE_SI128(&pV1[2 * x], uHigh);
				STORE_SI128(&pV1[2 * x + 16], uLow);
			}
		}

		for (; x < halfWidth; x++)
		{
			const size_t val2x = 2 * x + evenX;
			const size_t val2x1 = val2x + oddX;
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

static pstatus_t sse41_ChromaV1ToYUV444(const BYTE* WINPR_RESTRICT pSrcRaw[3],
                                        const UINT32 srcStep[3], BYTE* WINPR_RESTRICT pDstRaw[3],
                                        const UINT32 dstStep[3],
                                        const RECTANGLE_16* WINPR_RESTRICT roi)
{
	const UINT32 mod = 16;
	UINT32 uY = 0;
	UINT32 vY = 0;
	const UINT32 nWidth = roi->right - roi->left;
	const UINT32 nHeight = roi->bottom - roi->top;
	const UINT32 halfWidth = (nWidth + 1) / 2;
	const UINT32 halfPad = halfWidth % 16;
	const UINT32 halfHeight = (nHeight + 1) / 2;
	const UINT32 oddY = 1;
	const UINT32 evenY = 0;
	const UINT32 oddX = 1;
	/* The auxiliary frame is aligned to multiples of 16x16.
	 * We need the padded height for B4 and B5 conversion. */
	const UINT32 padHeigth = nHeight + 16 - nHeight % 16;
	const BYTE* pSrc[3] = { pSrcRaw[0] + 1ULL * roi->top * srcStep[0] + roi->left,
		                    pSrcRaw[1] + 1ULL * roi->top / 2 * srcStep[1] + roi->left / 2,
		                    pSrcRaw[2] + 1ULL * roi->top / 2 * srcStep[2] + roi->left / 2 };
	BYTE* pDst[3] = { pDstRaw[0] + 1ULL * roi->top * dstStep[0] + roi->left,
		              pDstRaw[1] + 1ULL * roi->top * dstStep[1] + roi->left,
		              pDstRaw[2] + 1ULL * roi->top * dstStep[2] + roi->left };
	const __m128i zero = _mm_setzero_si128();
	const __m128i mask = _mm_set_epi8(0, (char)0x80, 0, (char)0x80, 0, (char)0x80, 0, (char)0x80, 0,
	                                  (char)0x80, 0, (char)0x80, 0, (char)0x80, 0, (char)0x80);

	/* The second half of U and V is a bit more tricky... */
	/* B4 and B5 */
	for (size_t y = 0; y < padHeigth; y++)
	{
		const BYTE* Ya = pSrc[0] + 1ULL * srcStep[0] * y;
		BYTE* pX = NULL;

		if ((y) % mod < (mod + 1) / 2)
		{
			const UINT32 pos = (2 * uY++ + oddY);

			if (pos >= nHeight)
				continue;

			pX = pDst[1] + 1ULL * dstStep[1] * pos;
		}
		else
		{
			const UINT32 pos = (2 * vY++ + oddY);

			if (pos >= nHeight)
				continue;

			pX = pDst[2] + 1ULL * dstStep[2] * pos;
		}

		memcpy(pX, Ya, nWidth);
	}

	/* B6 and B7 */
	for (size_t y = 0; y < halfHeight; y++)
	{
		const size_t val2y = (y * 2 + evenY);
		const BYTE* Ua = pSrc[1] + srcStep[1] * y;
		const BYTE* Va = pSrc[2] + srcStep[2] * y;
		BYTE* pU = pDst[1] + dstStep[1] * val2y;
		BYTE* pV = pDst[2] + dstStep[2] * val2y;

		size_t x = 0;
		for (; x < halfWidth - halfPad; x += 16)
		{
			{
				const __m128i u = LOAD_SI128(&Ua[x]);
				const __m128i u2 = _mm_unpackhi_epi8(u, zero);
				const __m128i u1 = _mm_unpacklo_epi8(u, zero);
				_mm_maskmoveu_si128(u1, mask, (char*)&pU[2 * x]);
				_mm_maskmoveu_si128(u2, mask, (char*)&pU[2 * x + 16]);
			}
			{
				const __m128i u = LOAD_SI128(&Va[x]);
				const __m128i u2 = _mm_unpackhi_epi8(u, zero);
				const __m128i u1 = _mm_unpacklo_epi8(u, zero);
				_mm_maskmoveu_si128(u1, mask, (char*)&pV[2 * x]);
				_mm_maskmoveu_si128(u2, mask, (char*)&pV[2 * x + 16]);
			}
		}

		for (; x < halfWidth; x++)
		{
			const size_t val2x1 = (x * 2ULL + oddX);
			pU[val2x1] = Ua[x];
			pV[val2x1] = Va[x];
		}
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t sse41_ChromaV2ToYUV444(const BYTE* WINPR_RESTRICT pSrc[3], const UINT32 srcStep[3],
                                        UINT32 nTotalWidth, WINPR_ATTR_UNUSED UINT32 nTotalHeight,
                                        BYTE* WINPR_RESTRICT pDst[3], const UINT32 dstStep[3],
                                        const RECTANGLE_16* WINPR_RESTRICT roi)
{
	const UINT32 nWidth = roi->right - roi->left;
	const UINT32 nHeight = roi->bottom - roi->top;
	const UINT32 halfWidth = (nWidth + 1) / 2;
	const UINT32 halfPad = halfWidth % 16;
	const UINT32 halfHeight = (nHeight + 1) / 2;
	const UINT32 quaterWidth = (nWidth + 3) / 4;
	const UINT32 quaterPad = quaterWidth % 16;
	const __m128i zero = _mm_setzero_si128();
	const __m128i mask = _mm_set_epi8((char)0x80, 0, (char)0x80, 0, (char)0x80, 0, (char)0x80, 0,
	                                  (char)0x80, 0, (char)0x80, 0, (char)0x80, 0, (char)0x80, 0);
	const __m128i mask2 = _mm_set_epi8(0, (char)0x80, 0, (char)0x80, 0, (char)0x80, 0, (char)0x80,
	                                   0, (char)0x80, 0, (char)0x80, 0, (char)0x80, 0, (char)0x80);
	const __m128i shuffle1 =
	    _mm_set_epi8((char)0x80, 15, (char)0x80, 14, (char)0x80, 13, (char)0x80, 12, (char)0x80, 11,
	                 (char)0x80, 10, (char)0x80, 9, (char)0x80, 8);
	const __m128i shuffle2 =
	    _mm_set_epi8((char)0x80, 7, (char)0x80, 6, (char)0x80, 5, (char)0x80, 4, (char)0x80, 3,
	                 (char)0x80, 2, (char)0x80, 1, (char)0x80, 0);

	/* B4 and B5: odd UV values for width/2, height */
	for (size_t y = 0; y < nHeight; y++)
	{
		const size_t yTop = y + roi->top;
		const BYTE* pYaU = pSrc[0] + srcStep[0] * yTop + roi->left / 2;
		const BYTE* pYaV = pYaU + nTotalWidth / 2;
		BYTE* pU = pDst[1] + 1ULL * dstStep[1] * yTop + roi->left;
		BYTE* pV = pDst[2] + 1ULL * dstStep[2] * yTop + roi->left;

		size_t x = 0;
		for (; x < halfWidth - halfPad; x += 16)
		{
			{
				const __m128i u = LOAD_SI128(&pYaU[x]);
				const __m128i u2 = _mm_unpackhi_epi8(zero, u);
				const __m128i u1 = _mm_unpacklo_epi8(zero, u);
				_mm_maskmoveu_si128(u1, mask, (char*)&pU[2 * x]);
				_mm_maskmoveu_si128(u2, mask, (char*)&pU[2 * x + 16]);
			}
			{
				const __m128i v = LOAD_SI128(&pYaV[x]);
				const __m128i v2 = _mm_unpackhi_epi8(zero, v);
				const __m128i v1 = _mm_unpacklo_epi8(zero, v);
				_mm_maskmoveu_si128(v1, mask, (char*)&pV[2 * x]);
				_mm_maskmoveu_si128(v2, mask, (char*)&pV[2 * x + 16]);
			}
		}

		for (; x < halfWidth; x++)
		{
			const size_t odd = 2ULL * x + 1;
			pU[odd] = pYaU[x];
			pV[odd] = pYaV[x];
		}
	}

	/* B6 - B9 */
	for (size_t y = 0; y < halfHeight; y++)
	{
		const BYTE* pUaU = pSrc[1] + srcStep[1] * (y + roi->top / 2) + roi->left / 4;
		const BYTE* pUaV = pUaU + nTotalWidth / 4;
		const BYTE* pVaU = pSrc[2] + srcStep[2] * (y + roi->top / 2) + roi->left / 4;
		const BYTE* pVaV = pVaU + nTotalWidth / 4;
		BYTE* pU = pDst[1] + dstStep[1] * (2 * y + 1 + roi->top) + roi->left;
		BYTE* pV = pDst[2] + dstStep[2] * (2 * y + 1 + roi->top) + roi->left;

		UINT32 x = 0;
		for (; x < quaterWidth - quaterPad; x += 16)
		{
			{
				const __m128i uU = LOAD_SI128(&pUaU[x]);
				const __m128i uV = LOAD_SI128(&pVaU[x]);
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
				const __m128i vU = LOAD_SI128(&pUaV[x]);
				const __m128i vV = LOAD_SI128(&pVaV[x]);
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

	return PRIMITIVES_SUCCESS;
}

static pstatus_t sse41_YUV420CombineToYUV444(avc444_frame_type type,
                                             const BYTE* WINPR_RESTRICT pSrc[3],
                                             const UINT32 srcStep[3], UINT32 nWidth, UINT32 nHeight,
                                             BYTE* WINPR_RESTRICT pDst[3], const UINT32 dstStep[3],
                                             const RECTANGLE_16* WINPR_RESTRICT roi)
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
			return sse41_LumaToYUV444(pSrc, srcStep, pDst, dstStep, roi);

		case AVC444_CHROMAv1:
			return sse41_ChromaV1ToYUV444(pSrc, srcStep, pDst, dstStep, roi);

		case AVC444_CHROMAv2:
			return sse41_ChromaV2ToYUV444(pSrc, srcStep, nWidth, nHeight, pDst, dstStep, roi);

		default:
			return -1;
	}
}
#endif

void primitives_init_YUV_sse41_int(primitives_t* WINPR_RESTRICT prims)
{
#if defined(SSE_AVX_INTRINSICS_ENABLED)
	generic = primitives_get_generic();

	WLog_VRB(PRIM_TAG, "SSE3/sse41 optimizations");
	prims->RGBToYUV420_8u_P3AC4R = sse41_RGBToYUV420;
	prims->RGBToAVC444YUV = sse41_RGBToAVC444YUV;
	prims->RGBToAVC444YUVv2 = sse41_RGBToAVC444YUVv2;
	prims->YUV420ToRGB_8u_P3AC4R = sse41_YUV420ToRGB;
	prims->YUV444ToRGB_8u_P3AC4R = sse41_YUV444ToRGB_8u_P3AC4R;
	prims->YUV420CombineToYUV444 = sse41_YUV420CombineToYUV444;
#else
	WLog_VRB(PRIM_TAG, "undefined WITH_SIMD or sse41 intrinsics not available");
	WINPR_UNUSED(prims);
#endif
}
