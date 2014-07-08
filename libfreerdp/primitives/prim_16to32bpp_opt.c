/* prim_16to32bpp_opt.c
 * 16-bit to 32-bit color conversion via SSE/Neon
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
#include <winpr/sysinfo.h>

#ifdef WITH_SSE2
#include <emmintrin.h>
#include <tmmintrin.h>
/* #elif defined(WITH_NEON) */
/* #include <arm_neon.h> */
#endif /* WITH_SSE2 */

#include "prim_internal.h"
#include "prim_16to32bpp.h"

#ifdef WITH_SSE2
/* ------------------------------------------------------------------------- */
/* Note:  _no_invert and _invert could be coded with variables as shift
 * amounts and a single routine, but tests showed that was much slower.
 */
static pstatus_t sse3_RGB565ToARGB_16u32u_C3C4_no_invert(
	const UINT16* pSrc, INT32 srcStep,
	UINT32* pDst, INT32 dstStep,
	UINT32 width, UINT32 height,
	BOOL alpha)
{
	const BYTE *src = (const BYTE *) pSrc;
	BYTE *dst = (BYTE *) pDst;
	int h;
	int srcRowBump = srcStep - (width * sizeof(UINT16));
	int dstRowBump = dstStep - (width * sizeof(UINT32));
	__m128i R0, R1, R2, R_FC00, R_0300, R_00F8, R_0007, R_alpha;

	R_FC00 = _mm_set1_epi16(0xFC00);
	R_0300 = _mm_set1_epi16(0x0300);
	R_00F8 = _mm_set1_epi16(0x00F8);
	R_0007 = _mm_set1_epi16(0x0007);
	if (alpha) R_alpha = _mm_set1_epi32(0xFF00FF00U);
	else R_alpha = _mm_set1_epi32(0x00000000U);

	for (h=0; h<height; h++)
	{
		int w = width;

		/* Get to 16-byte destination boundary for the dest. */
		if ((ULONG_PTR) dst & 0x0f)
		{
			int startup = (16 - ((ULONG_PTR) dst & 0x0f)) / 4;
			if (startup > width) startup = width;
			general_RGB565ToARGB_16u32u_C3C4((const UINT16*) src, srcStep,
				(UINT32*) dst, dstStep, startup, 1, alpha, FALSE);
			src += startup * sizeof(UINT16);
			dst += startup * sizeof(UINT32);
			w -= startup;
		}

		/* The main loop handles eight pixels at a time. */
		while (w >= 8)
		{
			/* If off-stride, use the slower load. */
			if ((ULONG_PTR) src & 0x0f)
				R0 = _mm_lddqu_si128((__m128i *) src);
			else
				R0 = _mm_load_si128((__m128i *) src);
			src += (128/8);

			/* Do the lower two colors, which end up in the lower two bytes. */
			/* G = ((P<<5) & 0xFC00) | ((P>>1) & 0x0300) */
			R2 = _mm_slli_epi16(R0, 5);
			R2 = _mm_and_si128(R_FC00, R2);

			R1 = _mm_srli_epi16(R0, 1);
			R1 = _mm_and_si128(R_0300, R1);
			R2 = _mm_or_si128(R1, R2);

			/* R = ((P<<3) & 0x00F8) | ((P>>2) & 0x0007) */
			R1 = _mm_slli_epi16(R0, 3);
			R1 = _mm_and_si128(R_00F8, R1);
			R2 = _mm_or_si128(R1, R2);

			R1 = _mm_srli_epi16(R0, 2);
			R1 = _mm_and_si128(R_0007, R1);
			R2 = _mm_or_si128(R1, R2);		/* R2 = lowers */

			/* Handle the upper color. */
			/* B = ((P<<8) & 0x00F8) | ((P<<13) & 0x0007) */
			R1 = _mm_srli_epi16(R0, 8);
			R1 = _mm_and_si128(R_00F8, R1);

			R0 = _mm_srli_epi16(R0, 13);
			R0 = _mm_and_si128(R_0007, R0);
			R1 = _mm_or_si128(R0, R1);		/* R1 = uppers */

			/* Add alpha (or zero) . */
			R1 = _mm_or_si128(R_alpha, R1);	/* + alpha */

			/* Unpack to intermix the AB and GR pieces. */
			R0 = _mm_unpackhi_epi16(R2, R1);
			R2 = _mm_unpacklo_epi16(R2, R1);

			/* Store the results. */
			_mm_store_si128((__m128i *) dst, R2);  dst += (128/8);
			_mm_store_si128((__m128i *) dst, R0);  dst += (128/8);
			w -= 8;
		}

		/* Handle any remainder. */
		if (w > 0)
		{
			general_RGB565ToARGB_16u32u_C3C4((const UINT16*) src, srcStep,
				(UINT32*) dst, dstStep, w, 1, alpha, FALSE);
			src += w * sizeof(UINT16);
			dst += w * sizeof(UINT32);
		}

		/* Bump to the start of the next row. */
		src += srcRowBump;
		dst += dstRowBump;
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
static pstatus_t sse3_RGB565ToARGB_16u32u_C3C4_invert(
	const UINT16* pSrc, INT32 srcStep,
	UINT32* pDst, INT32 dstStep,
	UINT32 width, UINT32 height,
	BOOL alpha)
{
	const BYTE *src = (const BYTE *) pSrc;
	BYTE *dst = (BYTE *) pDst;
	int h;
	int srcRowBump = srcStep - (width * sizeof(UINT16));
	int dstRowBump = dstStep - (width * sizeof(UINT32));
	__m128i R0, R1, R2, R_FC00, R_0300, R_00F8, R_0007, R_alpha;

	R_FC00 = _mm_set1_epi16(0xFC00);
	R_0300 = _mm_set1_epi16(0x0300);
	R_00F8 = _mm_set1_epi16(0x00F8);
	R_0007 = _mm_set1_epi16(0x0007);
	if (alpha) R_alpha = _mm_set1_epi32(0xFF00FF00U);
	else R_alpha = _mm_set1_epi32(0x00000000U);

	for (h=0; h<height; h++)
	{
		int w = width;

		/* Get to 16-byte destination boundary for the dest. */
		if ((ULONG_PTR) dst & 0x0f)
		{
			int startup = (16 - ((ULONG_PTR) dst & 0x0f)) / 4;
			if (startup > width) startup = width;
			general_RGB565ToARGB_16u32u_C3C4((const UINT16*) src, srcStep,
				(UINT32*) dst, dstStep, startup, 1, alpha, TRUE);
			src += startup * sizeof(UINT16);
			dst += startup * sizeof(UINT32);
			w -= startup;
		}

		/* The main loop handles eight pixels at a time. */
		while (w >= 8)
		{
			/* Off-stride, slower load. */
			if ((ULONG_PTR) src & 0x0f) 
				R0 = _mm_lddqu_si128((__m128i *) src);
			else
				R0 = _mm_load_si128((__m128i *) src);
			src += (128/8);

			/* Do the lower two colors, which end up in the lower two bytes. */
			/* G = ((P<<5) & 0xFC00) | ((P>>1) & 0x0300) */
			R2 = _mm_slli_epi16(R0, 5);
			R2 = _mm_and_si128(R_FC00, R2);

			R1 = _mm_srli_epi16(R0, 1);
			R1 = _mm_and_si128(R_0300, R1);
			R2 = _mm_or_si128(R1, R2);

			/* B = ((P>>8) & 0x00F8) | ((P>>13) & 0x0007) */
			R1 = _mm_srli_epi16(R0, 8);
			R1 = _mm_and_si128(R_00F8, R1);
			R2 = _mm_or_si128(R1, R2);

			R1 = _mm_srli_epi16(R0, 13);
			R1 = _mm_and_si128(R_0007, R1);
			R2 = _mm_or_si128(R1, R2);		/* R2 = lowers */

			/* Handle the upper color. */
			/* R = ((P<<3) & 0x00F8) | ((P>>13) & 0x0007) */
			R1 = _mm_slli_epi16(R0, 3);
			R1 = _mm_and_si128(R_00F8, R1);

			R0 = _mm_srli_epi16(R0, 2);
			R0 = _mm_and_si128(R_0007, R0);
			R1 = _mm_or_si128(R0, R1);		/* R1 = uppers */

			/* Add alpha (or zero) . */
			R1 = _mm_or_si128(R_alpha, R1);	/* + alpha */

			/* Unpack to intermix the AR and GB pieces. */
			R0 = _mm_unpackhi_epi16(R2, R1);
			R2 = _mm_unpacklo_epi16(R2, R1);

			/* Store the results. */
			_mm_store_si128((__m128i *) dst, R2);  dst += (128/8);
			_mm_store_si128((__m128i *) dst, R0);  dst += (128/8);
			w -= 8;
		}

		/* Handle any remainder. */
		if (w > 0)
		{
			general_RGB565ToARGB_16u32u_C3C4((const UINT16*) src, srcStep,
				(UINT32*) dst, dstStep, w, 1, alpha, TRUE);
			src += w * sizeof(UINT16);
			dst += w * sizeof(UINT32);
		}

		/* Bump to the start of the next row. */
		src += srcRowBump;
		dst += dstRowBump;
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
pstatus_t sse3_RGB565ToARGB_16u32u_C3C4(
	const UINT16* pSrc, INT32 srcStep,
	UINT32* pDst, INT32 dstStep,
	UINT32 width, UINT32 height,
	BOOL alpha, BOOL invert)
{
	if (invert)
	{
		return sse3_RGB565ToARGB_16u32u_C3C4_invert(pSrc, srcStep,
			pDst, dstStep, width, height, alpha);
	}
	else
	{
		return sse3_RGB565ToARGB_16u32u_C3C4_no_invert(pSrc, srcStep,
			pDst, dstStep, width, height, alpha);
	}
}
#endif /* WITH_SSE2 */

/* ------------------------------------------------------------------------- */
void primitives_init_16to32bpp_opt(
	primitives_t *prims)
{
#ifdef WITH_SSE2
    if (IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE))
	{
		prims->RGB565ToARGB_16u32u_C3C4 = sse3_RGB565ToARGB_16u32u_C3C4;
	}
#endif
}
