/* FreeRDP: A Remote Desktop Protocol Client
 * Optimized alpha blending routines.
 * vi:ts=4 sw=4:
 *
 * (c) Copyright 2012 Hewlett-Packard Development Company, L.P.
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0.
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 *
 * Note: this code assumes the second operand is fully opaque,
 * e.g.
 *   newval = alpha1*val1 + (1-alpha1)*val2
 * rather than
 *   newval = alpha1*val1 + (1-alpha1)*alpha2*val2
 * The IPP gives other options.
 */

#include <freerdp/config.h>

#include <freerdp/types.h>
#include <freerdp/primitives.h>
#include <winpr/sysinfo.h>

#include "prim_alphaComp.h"

#include "prim_internal.h"
#include "prim_avxsse.h"

/* ------------------------------------------------------------------------- */
#if defined(SSE_AVX_INTRINSICS_ENABLED)
#include <emmintrin.h>
#include <pmmintrin.h>

static primitives_t* generic = NULL;

static pstatus_t sse2_alphaComp_argb(const BYTE* WINPR_RESTRICT pSrc1, UINT32 src1Step,
                                     const BYTE* WINPR_RESTRICT pSrc2, UINT32 src2Step,
                                     BYTE* WINPR_RESTRICT pDst, UINT32 dstStep, UINT32 width,
                                     UINT32 height)
{
	const UINT32* sptr1 = (const UINT32*)pSrc1;
	const UINT32* sptr2 = (const UINT32*)pSrc2;

	if ((width <= 0) || (height <= 0))
		return PRIMITIVES_SUCCESS;

	if (width < 4) /* pointless if too small */
	{
		return generic->alphaComp_argb(pSrc1, src1Step, pSrc2, src2Step, pDst, dstStep, width,
		                               height);
	}

	UINT32* dptr = (UINT32*)pDst;
	const size_t linebytes = width * sizeof(UINT32);
	const size_t src1Jump = (src1Step - linebytes) / sizeof(UINT32);
	const size_t src2Jump = (src2Step - linebytes) / sizeof(UINT32);
	const size_t dstJump = (dstStep - linebytes) / sizeof(UINT32);
	__m128i xmm0 = mm_set1_epu32(0);
	__m128i xmm1 = _mm_set1_epi16(1);

	for (UINT32 y = 0; y < height; ++y)
	{
		uint32_t pixels = width;
		uint32_t count = 0;
		/* Get to the 16-byte boundary now. */
		uint32_t leadIn = 0;

		switch ((ULONG_PTR)dptr & 0x0f)
		{
			case 0:
				leadIn = 0;
				break;

			case 4:
				leadIn = 3;
				break;

			case 8:
				leadIn = 2;
				break;

			case 12:
				leadIn = 1;
				break;

			default:
				/* We'll never hit a 16-byte boundary, so do the whole
				 * thing the slow way.
				 */
				leadIn = width;
				break;
		}

		if (leadIn)
		{
			pstatus_t status = 0;
			status = generic->alphaComp_argb((const BYTE*)sptr1, src1Step, (const BYTE*)sptr2,
			                                 src2Step, (BYTE*)dptr, dstStep, leadIn, 1);
			if (status != PRIMITIVES_SUCCESS)
				return status;

			sptr1 += leadIn;
			sptr2 += leadIn;
			dptr += leadIn;
			pixels -= leadIn;
		}

		/* Use SSE registers to do 4 pixels at a time. */
		count = pixels >> 2;
		pixels -= count << 2;

		while (count--)
		{
			__m128i xmm2;
			__m128i xmm3;
			__m128i xmm4;
			__m128i xmm5;
			__m128i xmm6;
			__m128i xmm7;
			/* BdGdRdAdBcGcRcAcBbGbRbAbBaGaRaAa */
			xmm2 = LOAD_SI128(sptr1);
			sptr1 += 4;
			/* BhGhRhAhBgGgRgAgBfGfRfAfBeGeReAe */
			xmm3 = LOAD_SI128(sptr2);
			sptr2 += 4;
			/* 00Bb00Gb00Rb00Ab00Ba00Ga00Ra00Aa */
			xmm4 = _mm_unpackhi_epi8(xmm2, xmm0);
			/* 00Bf00Gf00Bf00Af00Be00Ge00Re00Ae */
			xmm5 = _mm_unpackhi_epi8(xmm3, xmm0);
			/* subtract */
			xmm6 = _mm_subs_epi16(xmm4, xmm5);
			/* 00Bb00Gb00Rb00Ab00Aa00Aa00Aa00Aa */
			xmm4 = _mm_shufflelo_epi16(xmm4, 0xff);
			/* 00Ab00Ab00Ab00Ab00Aa00Aa00Aa00Aa */
			xmm4 = _mm_shufflehi_epi16(xmm4, 0xff);
			/* Add one to alphas */
			xmm4 = _mm_adds_epi16(xmm4, xmm1);
			/* Multiply and take low word */
			xmm4 = _mm_mullo_epi16(xmm4, xmm6);
			/* Shift 8 right */
			xmm4 = _mm_srai_epi16(xmm4, 8);
			/* Add xmm5 */
			xmm4 = _mm_adds_epi16(xmm4, xmm5);
			/* 00Bj00Gj00Rj00Aj00Bi00Gi00Ri00Ai */
			/* 00Bd00Gd00Rd00Ad00Bc00Gc00Rc00Ac */
			xmm5 = _mm_unpacklo_epi8(xmm2, xmm0);
			/* 00Bh00Gh00Rh00Ah00Bg00Gg00Rg00Ag */
			xmm6 = _mm_unpacklo_epi8(xmm3, xmm0);
			/* subtract */
			xmm7 = _mm_subs_epi16(xmm5, xmm6);
			/* 00Bd00Gd00Rd00Ad00Ac00Ac00Ac00Ac */
			xmm5 = _mm_shufflelo_epi16(xmm5, 0xff);
			/* 00Ad00Ad00Ad00Ad00Ac00Ac00Ac00Ac */
			xmm5 = _mm_shufflehi_epi16(xmm5, 0xff);
			/* Add one to alphas */
			xmm5 = _mm_adds_epi16(xmm5, xmm1);
			/* Multiply and take low word */
			xmm5 = _mm_mullo_epi16(xmm5, xmm7);
			/* Shift 8 right */
			xmm5 = _mm_srai_epi16(xmm5, 8);
			/* Add xmm6 */
			xmm5 = _mm_adds_epi16(xmm5, xmm6);
			/* 00Bl00Gl00Rl00Al00Bk00Gk00Rk0ABk */
			/* Must mask off remainders or pack gets confused */
			xmm3 = _mm_set1_epi16(0x00ffU);
			xmm4 = _mm_and_si128(xmm4, xmm3);
			xmm5 = _mm_and_si128(xmm5, xmm3);
			/* BlGlRlAlBkGkRkAkBjGjRjAjBiGiRiAi */
			xmm5 = _mm_packus_epi16(xmm5, xmm4);
			STORE_SI128(dptr, xmm5);
			dptr += 4;
		}

		/* Finish off the remainder. */
		if (pixels)
		{
			pstatus_t status = 0;
			status = generic->alphaComp_argb((const BYTE*)sptr1, src1Step, (const BYTE*)sptr2,
			                                 src2Step, (BYTE*)dptr, dstStep, pixels, 1);
			if (status != PRIMITIVES_SUCCESS)
				return status;

			sptr1 += pixels;
			sptr2 += pixels;
			dptr += pixels;
		}

		/* Jump to next row. */
		sptr1 += src1Jump;
		sptr2 += src2Jump;
		dptr += dstJump;
	}

	return PRIMITIVES_SUCCESS;
}
#endif

/* ------------------------------------------------------------------------- */
void primitives_init_alphaComp_sse3_int(primitives_t* WINPR_RESTRICT prims)
{
#if defined(SSE_AVX_INTRINSICS_ENABLED)
	generic = primitives_get_generic();
	WLog_VRB(PRIM_TAG, "SSE2/SSE3 optimizations");
	prims->alphaComp_argb = sse2_alphaComp_argb;

#else
	WLog_VRB(PRIM_TAG, "undefined WITH_SIMD or SSE3 intrinsics not available");
	WINPR_UNUSED(prims);
#endif
}
