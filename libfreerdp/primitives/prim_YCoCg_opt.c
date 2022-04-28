/* FreeRDP: A Remote Desktop Protocol Client
 * Optimized YCoCg<->RGB conversion operations.
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

#include <freerdp/config.h>

#include <freerdp/types.h>
#include <freerdp/primitives.h>
#include <winpr/sysinfo.h>

#ifdef WITH_SSE2
#include <emmintrin.h>
#include <tmmintrin.h>
#elif defined(WITH_NEON)
#include <arm_neon.h>
#endif /* WITH_SSE2 else WITH_NEON */

#include "prim_internal.h"
#include "prim_templates.h"

static primitives_t* generic = NULL;

#ifdef WITH_SSE2
/* ------------------------------------------------------------------------- */
static pstatus_t ssse3_YCoCgRToRGB_8u_AC4R_invert(const BYTE* pSrc, UINT32 srcStep, BYTE* pDst,
                                                  UINT32 DstFormat, UINT32 dstStep, UINT32 width,
                                                  UINT32 height, UINT8 shift, BOOL withAlpha)
{
	const BYTE* sptr = pSrc;
	BYTE* dptr = (BYTE*)pDst;
	int sRowBump = srcStep - width * sizeof(UINT32);
	int dRowBump = dstStep - width * sizeof(UINT32);
	UINT32 h;
	/* Shift left by "shift" and divide by two is the same as shift
	 * left by "shift-1".
	 */
	int dataShift = shift - 1;
	BYTE mask = (BYTE)(0xFFU << dataShift);

	/* Let's say the data is of the form:
	 * y0y0o0g0 a1y1o1g1 a2y2o2g2...
	 * Apply:
	 * |R|   | 1  1/2 -1/2 |   |y|
	 * |G| = | 1  0    1/2 | * |o|
	 * |B|   | 1 -1/2 -1/2 |   |g|
	 * where Y is 8-bit unsigned and o & g are 8-bit signed.
	 */

	if ((width < 8) || (ULONG_PTR)dptr & 0x03)
	{
		/* Too small, or we'll never hit a 16-byte boundary.  Punt. */
		return generic->YCoCgToRGB_8u_AC4R(pSrc, srcStep, pDst, DstFormat, dstStep, width, height,
		                                   shift, withAlpha);
	}

	for (h = 0; h < height; h++)
	{
		UINT32 w = width;
		BOOL onStride;

		/* Get to a 16-byte destination boundary. */
		if ((ULONG_PTR)dptr & 0x0f)
		{
			pstatus_t status;
			UINT32 startup = (16 - ((ULONG_PTR)dptr & 0x0f)) / 4;

			if (startup > width)
				startup = width;

			status = generic->YCoCgToRGB_8u_AC4R(sptr, srcStep, dptr, DstFormat, dstStep, startup,
			                                     1, shift, withAlpha);

			if (status != PRIMITIVES_SUCCESS)
				return status;

			sptr += startup * sizeof(UINT32);
			dptr += startup * sizeof(UINT32);
			w -= startup;
		}

		/* Each loop handles eight pixels at a time. */
		onStride = (((ULONG_PTR)sptr & 0x0f) == 0) ? TRUE : FALSE;

		while (w >= 8)
		{
			__m128i R0, R1, R2, R3, R4, R5, R6, R7;

			if (onStride)
			{
				/* The faster path, 16-byte aligned load. */
				R0 = _mm_load_si128((const __m128i*)sptr);
				sptr += (128 / 8);
				R1 = _mm_load_si128((const __m128i*)sptr);
				sptr += (128 / 8);
			}
			else
			{
				/* Off-stride, slower LDDQU load. */
				R0 = _mm_lddqu_si128((const __m128i*)sptr);
				sptr += (128 / 8);
				R1 = _mm_lddqu_si128((const __m128i*)sptr);
				sptr += (128 / 8);
			}

			/* R0 = a3y3o3g3 a2y2o2g2 a1y1o1g1 a0y0o0g0 */
			/* R1 = a7y7o7g7 a6y6o6g6 a5y5o5g5 a4y4o4g4 */
			/* Shuffle to pack all the like types together. */
			R2 = _mm_set_epi32(0x0f0b0703, 0x0e0a0602, 0x0d090501, 0x0c080400);
			R3 = _mm_shuffle_epi8(R0, R2);
			R4 = _mm_shuffle_epi8(R1, R2);
			/* R3 = a3a2a1a0 y3y2y1y0 o3o2o1o0 g3g2g1g0 */
			/* R4 = a7a6a5a4 y7y6y5y4 o7o6o5o4 g7g6g5g4 */
			R5 = _mm_unpackhi_epi32(R3, R4);
			R6 = _mm_unpacklo_epi32(R3, R4);

			/* R5 = a7a6a5a4 a3a2a1a0 y7y6y5y4 y3y2y1y0 */
			/* R6 = o7o6o5o4 o3o2o1o0 g7g6g5g4 g3g2g1g0 */
			/* Save alphas aside */
			if (withAlpha)
				R7 = _mm_unpackhi_epi64(R5, R5);
			else
				R7 = _mm_set1_epi32(0xFFFFFFFFU);

			/* R7 = a7a6a5a4 a3a2a1a0 a7a6a5a4 a3a2a1a0 */
			/* Expand Y's from 8-bit unsigned to 16-bit signed. */
			R1 = _mm_set1_epi32(0);
			R0 = _mm_unpacklo_epi8(R5, R1);
			/* R0 = 00y700y6 00y500y4 00y300y2 00y100y0 */
			/* Shift Co's and Cg's by (shift-1).  -1 covers division by two.
			 * Note: this must be done before sign-conversion.
			 * Note also there is no slli_epi8, so we have to use a 16-bit
			 * version and then mask.
			 */
			R6 = _mm_slli_epi16(R6, dataShift);
			R1 = _mm_set1_epi8(mask);
			R6 = _mm_and_si128(R6, R1);
			/* R6 = shifted o7o6o5o4 o3o2o1o0 g7g6g5g4 g3g2g1g0 */
			/* Expand Co's from 8-bit signed to 16-bit signed */
			R1 = _mm_unpackhi_epi8(R6, R6);
			R1 = _mm_srai_epi16(R1, 8);
			/* R1 = xxo7xxo6 xxo5xxo4 xxo3xxo2 xxo1xxo0 */
			/* Expand Cg's form 8-bit signed to 16-bit signed */
			R2 = _mm_unpacklo_epi8(R6, R6);
			R2 = _mm_srai_epi16(R2, 8);
			/* R2 = xxg7xxg6 xxg5xxg4 xxg3xxg2 xxg1xxg0 */
			/* Get Y - halfCg and save */
			R6 = _mm_subs_epi16(R0, R2);
			/* R = (Y-halfCg) + halfCo */
			R3 = _mm_adds_epi16(R6, R1);
			/* R3 = xxR7xxR6 xxR5xxR4 xxR3xxR2 xxR1xxR0 */
			/* G = Y + Cg(/2) */
			R4 = _mm_adds_epi16(R0, R2);
			/* R4 = xxG7xxG6 xxG5xxG4 xxG3xxG2 xxG1xxG0 */
			/* B = (Y-halfCg) - Co(/2) */
			R5 = _mm_subs_epi16(R6, R1);
			/* R5 = xxB7xxB6 xxB5xxB4 xxB3xxB2 xxB1xxB0 */
			/* Repack R's & B's.  */
			R0 = _mm_packus_epi16(R3, R5);
			/* R0 = R7R6R5R4 R3R2R1R0 B7B6B5B4 B3B2B1B0 */
			/* Repack G's. */
			R1 = _mm_packus_epi16(R4, R4);
			/* R1 = G7G6G6G4 G3G2G1G0 G7G6G6G4 G3G2G1G0 */
			/* And add the A's. */
			R1 = _mm_unpackhi_epi64(R1, R7);
			/* R1 = A7A6A6A4 A3A2A1A0 G7G6G6G4 G3G2G1G0 */
			/* Now do interleaving again. */
			R2 = _mm_unpacklo_epi8(R0, R1);
			/* R2 = G7B7G6B6 G5B5G4B4 G3B3G2B2 G1B1G0B0 */
			R3 = _mm_unpackhi_epi8(R0, R1);
			/* R3 = A7R7A6R6 A5R5A4R4 A3R3A2R2 A1R1A0R0 */
			R4 = _mm_unpacklo_epi16(R2, R3);
			/* R4 = A3R3G3B3 A2R2G2B2 A1R1G1B1 A0R0G0B0 */
			R5 = _mm_unpackhi_epi16(R2, R3);
			/* R5 = A7R7G7B7 A6R6G6B6 A5R6G5B5 A4R4G4B4 */
			_mm_store_si128((__m128i*)dptr, R4);
			dptr += (128 / 8);
			_mm_store_si128((__m128i*)dptr, R5);
			dptr += (128 / 8);
			w -= 8;
		}

		/* Handle any remainder pixels. */
		if (w > 0)
		{
			pstatus_t status;
			status = generic->YCoCgToRGB_8u_AC4R(sptr, srcStep, dptr, DstFormat, dstStep, w, 1,
			                                     shift, withAlpha);

			if (status != PRIMITIVES_SUCCESS)
				return status;

			sptr += w * sizeof(UINT32);
			dptr += w * sizeof(UINT32);
		}

		sptr += sRowBump;
		dptr += dRowBump;
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
static pstatus_t ssse3_YCoCgRToRGB_8u_AC4R_no_invert(const BYTE* pSrc, UINT32 srcStep, BYTE* pDst,
                                                     UINT32 DstFormat, UINT32 dstStep, UINT32 width,
                                                     UINT32 height, UINT8 shift, BOOL withAlpha)
{
	const BYTE* sptr = pSrc;
	BYTE* dptr = (BYTE*)pDst;
	int sRowBump = srcStep - width * sizeof(UINT32);
	int dRowBump = dstStep - width * sizeof(UINT32);
	UINT32 h;
	/* Shift left by "shift" and divide by two is the same as shift
	 * left by "shift-1".
	 */
	int dataShift = shift - 1;
	BYTE mask = (BYTE)(0xFFU << dataShift);

	/* Let's say the data is of the form:
	 * y0y0o0g0 a1y1o1g1 a2y2o2g2...
	 * Apply:
	 * |R|   | 1  1/2 -1/2 |   |y|
	 * |G| = | 1  0    1/2 | * |o|
	 * |B|   | 1 -1/2 -1/2 |   |g|
	 * where Y is 8-bit unsigned and o & g are 8-bit signed.
	 */

	if ((width < 8) || (ULONG_PTR)dptr & 0x03)
	{
		/* Too small, or we'll never hit a 16-byte boundary.  Punt. */
		return generic->YCoCgToRGB_8u_AC4R(pSrc, srcStep, pDst, DstFormat, dstStep, width, height,
		                                   shift, withAlpha);
	}

	for (h = 0; h < height; h++)
	{
		int w = width;
		BOOL onStride;

		/* Get to a 16-byte destination boundary. */
		if ((ULONG_PTR)dptr & 0x0f)
		{
			pstatus_t status;
			UINT32 startup = (16 - ((ULONG_PTR)dptr & 0x0f)) / 4;

			if (startup > width)
				startup = width;

			status = generic->YCoCgToRGB_8u_AC4R(sptr, srcStep, dptr, DstFormat, dstStep, startup,
			                                     1, shift, withAlpha);

			if (status != PRIMITIVES_SUCCESS)
				return status;

			sptr += startup * sizeof(UINT32);
			dptr += startup * sizeof(UINT32);
			w -= startup;
		}

		/* Each loop handles eight pixels at a time. */
		onStride = (((const ULONG_PTR)sptr & 0x0f) == 0) ? TRUE : FALSE;

		while (w >= 8)
		{
			__m128i R0, R1, R2, R3, R4, R5, R6, R7;

			if (onStride)
			{
				/* The faster path, 16-byte aligned load. */
				R0 = _mm_load_si128((const __m128i*)sptr);
				sptr += (128 / 8);
				R1 = _mm_load_si128((const __m128i*)sptr);
				sptr += (128 / 8);
			}
			else
			{
				/* Off-stride, slower LDDQU load. */
				R0 = _mm_lddqu_si128((const __m128i*)sptr);
				sptr += (128 / 8);
				R1 = _mm_lddqu_si128((const __m128i*)sptr);
				sptr += (128 / 8);
			}

			/* R0 = a3y3o3g3 a2y2o2g2 a1y1o1g1 a0y0o0g0 */
			/* R1 = a7y7o7g7 a6y6o6g6 a5y5o5g5 a4y4o4g4 */
			/* Shuffle to pack all the like types together. */
			R2 = _mm_set_epi32(0x0f0b0703, 0x0e0a0602, 0x0d090501, 0x0c080400);
			R3 = _mm_shuffle_epi8(R0, R2);
			R4 = _mm_shuffle_epi8(R1, R2);
			/* R3 = a3a2a1a0 y3y2y1y0 o3o2o1o0 g3g2g1g0 */
			/* R4 = a7a6a5a4 y7y6y5y4 o7o6o5o4 g7g6g5g4 */
			R5 = _mm_unpackhi_epi32(R3, R4);
			R6 = _mm_unpacklo_epi32(R3, R4);

			/* R5 = a7a6a5a4 a3a2a1a0 y7y6y5y4 y3y2y1y0 */
			/* R6 = o7o6o5o4 o3o2o1o0 g7g6g5g4 g3g2g1g0 */
			/* Save alphas aside */
			if (withAlpha)
				R7 = _mm_unpackhi_epi64(R5, R5);
			else
				R7 = _mm_set1_epi32(0xFFFFFFFFU);

			/* R7 = a7a6a5a4 a3a2a1a0 a7a6a5a4 a3a2a1a0 */
			/* Expand Y's from 8-bit unsigned to 16-bit signed. */
			R1 = _mm_set1_epi32(0);
			R0 = _mm_unpacklo_epi8(R5, R1);
			/* R0 = 00y700y6 00y500y4 00y300y2 00y100y0 */
			/* Shift Co's and Cg's by (shift-1).  -1 covers division by two.
			 * Note: this must be done before sign-conversion.
			 * Note also there is no slli_epi8, so we have to use a 16-bit
			 * version and then mask.
			 */
			R6 = _mm_slli_epi16(R6, dataShift);
			R1 = _mm_set1_epi8(mask);
			R6 = _mm_and_si128(R6, R1);
			/* R6 = shifted o7o6o5o4 o3o2o1o0 g7g6g5g4 g3g2g1g0 */
			/* Expand Co's from 8-bit signed to 16-bit signed */
			R1 = _mm_unpackhi_epi8(R6, R6);
			R1 = _mm_srai_epi16(R1, 8);
			/* R1 = xxo7xxo6 xxo5xxo4 xxo3xxo2 xxo1xxo0 */
			/* Expand Cg's form 8-bit signed to 16-bit signed */
			R2 = _mm_unpacklo_epi8(R6, R6);
			R2 = _mm_srai_epi16(R2, 8);
			/* R2 = xxg7xxg6 xxg5xxg4 xxg3xxg2 xxg1xxg0 */
			/* Get Y - halfCg and save */
			R6 = _mm_subs_epi16(R0, R2);
			/* R = (Y-halfCg) + halfCo */
			R3 = _mm_adds_epi16(R6, R1);
			/* R3 = xxR7xxR6 xxR5xxR4 xxR3xxR2 xxR1xxR0 */
			/* G = Y + Cg(/2) */
			R4 = _mm_adds_epi16(R0, R2);
			/* R4 = xxG7xxG6 xxG5xxG4 xxG3xxG2 xxG1xxG0 */
			/* B = (Y-halfCg) - Co(/2) */
			R5 = _mm_subs_epi16(R6, R1);
			/* R5 = xxB7xxB6 xxB5xxB4 xxB3xxB2 xxB1xxB0 */
			/* Repack R's & B's.  */
			/* This line is the only diff between inverted and non-inverted.
			 * Unfortunately, it would be expensive to check "inverted"
			 * every time through this loop.
			 */
			R0 = _mm_packus_epi16(R5, R3);
			/* R0 = B7B6B5B4 B3B2B1B0 R7R6R5R4 R3R2R1R0 */
			/* Repack G's. */
			R1 = _mm_packus_epi16(R4, R4);
			/* R1 = G7G6G6G4 G3G2G1G0 G7G6G6G4 G3G2G1G0 */
			/* And add the A's. */
			R1 = _mm_unpackhi_epi64(R1, R7);
			/* R1 = A7A6A6A4 A3A2A1A0 G7G6G6G4 G3G2G1G0 */
			/* Now do interleaving again. */
			R2 = _mm_unpacklo_epi8(R0, R1);
			/* R2 = G7B7G6B6 G5B5G4B4 G3B3G2B2 G1B1G0B0 */
			R3 = _mm_unpackhi_epi8(R0, R1);
			/* R3 = A7R7A6R6 A5R5A4R4 A3R3A2R2 A1R1A0R0 */
			R4 = _mm_unpacklo_epi16(R2, R3);
			/* R4 = A3R3G3B3 A2R2G2B2 A1R1G1B1 A0R0G0B0 */
			R5 = _mm_unpackhi_epi16(R2, R3);
			/* R5 = A7R7G7B7 A6R6G6B6 A5R6G5B5 A4R4G4B4 */
			_mm_store_si128((__m128i*)dptr, R4);
			dptr += (128 / 8);
			_mm_store_si128((__m128i*)dptr, R5);
			dptr += (128 / 8);
			w -= 8;
		}

		/* Handle any remainder pixels. */
		if (w > 0)
		{
			pstatus_t status;
			status = generic->YCoCgToRGB_8u_AC4R(sptr, srcStep, dptr, DstFormat, dstStep, w, 1,
			                                     shift, withAlpha);

			if (status != PRIMITIVES_SUCCESS)
				return status;

			sptr += w * sizeof(UINT32);
			dptr += w * sizeof(UINT32);
		}

		sptr += sRowBump;
		dptr += dRowBump;
	}

	return PRIMITIVES_SUCCESS;
}
#endif /* WITH_SSE2 */

#ifdef WITH_SSE2
/* ------------------------------------------------------------------------- */
static pstatus_t ssse3_YCoCgRToRGB_8u_AC4R(const BYTE* pSrc, INT32 srcStep, BYTE* pDst,
                                           UINT32 DstFormat, INT32 dstStep, UINT32 width,
                                           UINT32 height, UINT8 shift, BOOL withAlpha)
{
	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRX32:
		case PIXEL_FORMAT_BGRA32:
			return ssse3_YCoCgRToRGB_8u_AC4R_invert(pSrc, srcStep, pDst, DstFormat, dstStep, width,
			                                        height, shift, withAlpha);

		case PIXEL_FORMAT_RGBX32:
		case PIXEL_FORMAT_RGBA32:
			return ssse3_YCoCgRToRGB_8u_AC4R_no_invert(pSrc, srcStep, pDst, DstFormat, dstStep,
			                                           width, height, shift, withAlpha);

		default:
			return generic->YCoCgToRGB_8u_AC4R(pSrc, srcStep, pDst, DstFormat, dstStep, width,
			                                   height, shift, withAlpha);
	}
}
#elif defined(WITH_NEON)

static pstatus_t neon_YCoCgToRGB_8u_X(const BYTE* pSrc, INT32 srcStep, BYTE* pDst, UINT32 DstFormat,
                                      INT32 dstStep, UINT32 width, UINT32 height, UINT8 shift,
                                      BYTE bPos, BYTE gPos, BYTE rPos, BYTE aPos, BOOL alpha)
{
	UINT32 y;
	BYTE* dptr = pDst;
	const BYTE* sptr = pSrc;
	const DWORD formatSize = FreeRDPGetBytesPerPixel(DstFormat);
	const int8_t cll = shift - 1; /* -1 builds in the /2's */
	const UINT32 srcPad = srcStep - (width * 4);
	const UINT32 dstPad = dstStep - (width * formatSize);
	const UINT32 pad = width % 8;
	const uint8x8_t aVal = vdup_n_u8(0xFF);
	const int8x8_t cllv = vdup_n_s8(cll);

	for (y = 0; y < height; y++)
	{
		UINT32 x;

		for (x = 0; x < width - pad; x += 8)
		{
			/* Note: shifts must be done before sign-conversion. */
			const uint8x8x4_t raw = vld4_u8(sptr);
			const int8x8_t CgRaw = vreinterpret_s8_u8(vshl_u8(raw.val[0], cllv));
			const int8x8_t CoRaw = vreinterpret_s8_u8(vshl_u8(raw.val[1], cllv));
			const int16x8_t Cg = vmovl_s8(CgRaw);
			const int16x8_t Co = vmovl_s8(CoRaw);
			const int16x8_t Y = vreinterpretq_s16_u16(vmovl_u8(raw.val[2])); /* UINT8 -> INT16 */
			const int16x8_t T = vsubq_s16(Y, Cg);
			const int16x8_t R = vaddq_s16(T, Co);
			const int16x8_t G = vaddq_s16(Y, Cg);
			const int16x8_t B = vsubq_s16(T, Co);
			uint8x8x4_t bgrx;
			bgrx.val[bPos] = vqmovun_s16(B);
			bgrx.val[gPos] = vqmovun_s16(G);
			bgrx.val[rPos] = vqmovun_s16(R);

			if (alpha)
				bgrx.val[aPos] = raw.val[3];
			else
				bgrx.val[aPos] = aVal;

			vst4_u8(dptr, bgrx);
			sptr += sizeof(raw);
			dptr += sizeof(bgrx);
		}

		for (x = 0; x < pad; x++)
		{
			/* Note: shifts must be done before sign-conversion. */
			const INT16 Cg = (INT16)((INT8)((*sptr++) << cll));
			const INT16 Co = (INT16)((INT8)((*sptr++) << cll));
			const INT16 Y = (INT16)(*sptr++); /* UINT8->INT16 */
			const INT16 T = Y - Cg;
			const INT16 R = T + Co;
			const INT16 G = Y + Cg;
			const INT16 B = T - Co;
			BYTE bgra[4];
			bgra[bPos] = CLIP(B);
			bgra[gPos] = CLIP(G);
			bgra[rPos] = CLIP(R);
			bgra[aPos] = *sptr++;

			if (!alpha)
				bgra[aPos] = 0xFF;

			*dptr++ = bgra[0];
			*dptr++ = bgra[1];
			*dptr++ = bgra[2];
			*dptr++ = bgra[3];
		}

		sptr += srcPad;
		dptr += dstPad;
	}

	return PRIMITIVES_SUCCESS;
}
static pstatus_t neon_YCoCgToRGB_8u_AC4R(const BYTE* pSrc, INT32 srcStep, BYTE* pDst,
                                         UINT32 DstFormat, INT32 dstStep, UINT32 width,
                                         UINT32 height, UINT8 shift, BOOL withAlpha)
{
	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRA32:
			return neon_YCoCgToRGB_8u_X(pSrc, srcStep, pDst, DstFormat, dstStep, width, height,
			                            shift, 2, 1, 0, 3, withAlpha);

		case PIXEL_FORMAT_BGRX32:
			return neon_YCoCgToRGB_8u_X(pSrc, srcStep, pDst, DstFormat, dstStep, width, height,
			                            shift, 2, 1, 0, 3, withAlpha);

		case PIXEL_FORMAT_RGBA32:
			return neon_YCoCgToRGB_8u_X(pSrc, srcStep, pDst, DstFormat, dstStep, width, height,
			                            shift, 0, 1, 2, 3, withAlpha);

		case PIXEL_FORMAT_RGBX32:
			return neon_YCoCgToRGB_8u_X(pSrc, srcStep, pDst, DstFormat, dstStep, width, height,
			                            shift, 0, 1, 2, 3, withAlpha);

		case PIXEL_FORMAT_ARGB32:
			return neon_YCoCgToRGB_8u_X(pSrc, srcStep, pDst, DstFormat, dstStep, width, height,
			                            shift, 1, 2, 3, 0, withAlpha);

		case PIXEL_FORMAT_XRGB32:
			return neon_YCoCgToRGB_8u_X(pSrc, srcStep, pDst, DstFormat, dstStep, width, height,
			                            shift, 1, 2, 3, 0, withAlpha);

		case PIXEL_FORMAT_ABGR32:
			return neon_YCoCgToRGB_8u_X(pSrc, srcStep, pDst, DstFormat, dstStep, width, height,
			                            shift, 3, 2, 1, 0, withAlpha);

		case PIXEL_FORMAT_XBGR32:
			return neon_YCoCgToRGB_8u_X(pSrc, srcStep, pDst, DstFormat, dstStep, width, height,
			                            shift, 3, 2, 1, 0, withAlpha);

		default:
			return generic->YCoCgToRGB_8u_AC4R(pSrc, srcStep, pDst, DstFormat, dstStep, width,
			                                   height, shift, withAlpha);
	}
}
#endif /* WITH_SSE2 */

/* ------------------------------------------------------------------------- */
void primitives_init_YCoCg_opt(primitives_t* prims)
{
	generic = primitives_get_generic();
	primitives_init_YCoCg(prims);
	/* While IPP acknowledges the existence of YCoCg-R, it doesn't currently
	 * include any routines to work with it, especially with variable shift
	 * width.
	 */
#if defined(WITH_SSE2)

	if (IsProcessorFeaturePresentEx(PF_EX_SSSE3) &&
	    IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE))
	{
		prims->YCoCgToRGB_8u_AC4R = ssse3_YCoCgRToRGB_8u_AC4R;
	}

#elif defined(WITH_NEON)

	if (IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE))
	{
		prims->YCoCgToRGB_8u_AC4R = neon_YCoCgToRGB_8u_AC4R;
	}

#endif /* WITH_SSE2 */
}
