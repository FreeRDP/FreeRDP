/* FreeRDP: A Remote Desktop Protocol Client
 * Alpha blending routines.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <freerdp/types.h>
#include <freerdp/primitives.h>

#include "prim_internal.h"

#ifdef WITH_SSE2
#include <emmintrin.h>
#include <pmmintrin.h>
#endif /* WITH_SSE2 */

#ifdef WITH_IPP
#include <ippi.h>
#endif /* WITH_IPP */

#define ALPHA(_k_)	(((_k_) & 0xFF000000U) >> 24)
#define RED(_k_)	(((_k_) & 0x00FF0000U) >> 16)
#define GRN(_k_)	(((_k_) & 0x0000FF00U) >> 8)
#define BLU(_k_)	(((_k_) & 0x000000FFU))

/* ------------------------------------------------------------------------- */
PRIM_STATIC pstatus_t general_alphaComp_argb(
	const BYTE *pSrc1,  INT32 src1Step,
	const BYTE *pSrc2,  INT32 src2Step,
	BYTE *pDst,  INT32 dstStep,
	INT32 width,  INT32 height)
{
	const UINT32 *sptr1 = (const UINT32 *) pSrc1;
	const UINT32 *sptr2 = (const UINT32 *) pSrc2;
	UINT32 *dptr = (UINT32 *) pDst;
	int linebytes = width * sizeof(UINT32);
	int src1Jump = (src1Step - linebytes) / sizeof(UINT32);
	int src2Jump = (src2Step - linebytes) / sizeof(UINT32);
	int dstJump  = (dstStep  - linebytes) / sizeof(UINT32);

	int y;
	for (y=0; y<height; y++)
	{
		int x;
		for (x=0; x<width; x++)
		{
			const UINT32 src1 = *sptr1++;
			const UINT32 src2 = *sptr2++;
			UINT32 alpha = ALPHA(src1) + 1;
			if (alpha == 256)
			{
				/* If alpha is 255+1, just copy src1. */
				*dptr++ = src1;
			}
			else if (alpha <= 1)
			{
				/* If alpha is 0+1, just copy src2. */
				*dptr++ = src2;
			}
			else
			{
				/* A perfectly accurate blend would do (a*src + (255-a)*dst)/255
				 * rather than adding one to alpha and dividing by 256, but this
				 * is much faster and only differs by one 16% of the time.
				 * I'm not sure who first designed the double-ops trick
				 * (Red Blue and Alpha Green).
				 */
				UINT32 rb, ag;
				UINT32 s2rb = src2 & 0x00FF00FFU;
				UINT32 s2ag = (src2 >> 8) & 0x00FF00FFU;
				UINT32 s1rb = src1 & 0x00FF00FFU;
				UINT32 s1ag = (src1 >> 8) & 0x00FF00FFU;

				UINT32 drb = s1rb - s2rb;
				UINT32 dag = s1ag - s2ag;
				drb *= alpha;
				dag *= alpha;

				rb  =  ((drb >> 8) + s2rb)       & 0x00FF00FFU;
				ag  = (((dag >> 8) + s2ag) << 8) & 0xFF00FF00U;
				*dptr++ = rb | ag;
			}
		}
		sptr1 += src1Jump;
		sptr2 += src2Jump;
		dptr  += dstJump;
    }

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
#ifdef WITH_SSE2
#if !defined(WITH_IPP) || defined(ALL_PRIMITIVES_VERSIONS)

PRIM_STATIC pstatus_t sse2_alphaComp_argb(
	const BYTE *pSrc1,  INT32 src1Step,
	const BYTE *pSrc2,  INT32 src2Step,
	BYTE *pDst,  INT32 dstStep,
	INT32 width,  INT32 height)
{
	const UINT32 *sptr1 = (const UINT32 *) pSrc1;
	const UINT32 *sptr2 = (const UINT32 *) pSrc2;
	UINT32 *dptr;
	int linebytes, src1Jump, src2Jump, dstJump, y;
	__m128i xmm0, xmm1;

	if ((width <= 0) || (height <= 0)) return PRIMITIVES_SUCCESS;

	if (width < 4)     /* pointless if too small */
	{
		return general_alphaComp_argb(pSrc1, src1Step, pSrc2, src2Step,
			pDst, dstStep, width, height);
	}
	dptr = (UINT32 *) pDst;
	linebytes = width * sizeof(UINT32);
	src1Jump = (src1Step - linebytes) / sizeof(UINT32);
	src2Jump = (src2Step - linebytes) / sizeof(UINT32);
	dstJump  = (dstStep  - linebytes) / sizeof(UINT32);

	xmm0 = _mm_set1_epi32(0);
	xmm1 = _mm_set1_epi16(1);

	for (y=0; y<height; ++y)
	{
		int pixels = width;
		int count;

		/* Get to the 16-byte boundary now. */
		int leadIn = 0;
		switch ((ULONG_PTR) dptr & 0x0f)
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
			general_alphaComp_argb((const BYTE *) sptr1,
				src1Step, (const BYTE *) sptr2, src2Step,
				(BYTE *) dptr, dstStep, leadIn, 1);
			sptr1 += leadIn;
			sptr2 += leadIn;
			dptr  += leadIn;
			pixels -= leadIn;
		}

		/* Use SSE registers to do 4 pixels at a time. */
		count = pixels >> 2;
		pixels -= count << 2;
		while (count--)
		{
			__m128i xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
			/* BdGdRdAdBcGcRcAcBbGbRbAbBaGaRaAa */
			xmm2 = LOAD_SI128(sptr1); sptr1 += 4;
			/* BhGhRhAhBgGgRgAgBfGfRfAfBeGeReAe */
			xmm3 = LOAD_SI128(sptr2); sptr2 += 4;
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
			_mm_store_si128((__m128i *) dptr, xmm5);  dptr += 4;
		}

		/* Finish off the remainder. */
		if (pixels)
		{
			general_alphaComp_argb((const BYTE *) sptr1, src1Step,
				(const BYTE *) sptr2, src2Step,
				(BYTE *) dptr, dstStep, pixels, 1);
			sptr1 += pixels;
			sptr2 += pixels;
			dptr  += pixels;
		}

		/* Jump to next row. */
		sptr1 += src1Jump;
		sptr2 += src2Jump;
		dptr  += dstJump;
	}

	return PRIMITIVES_SUCCESS;
}
#endif /* !defined(WITH_IPP) || defined(ALL_PRIMITIVES_VERSIONS) */
#endif

#ifdef WITH_IPP
/* ------------------------------------------------------------------------- */
PRIM_STATIC pstatus_t ipp_alphaComp_argb(
	const BYTE *pSrc1,  INT32 src1Step,
	const BYTE *pSrc2,  INT32 src2Step,
	BYTE *pDst,  INT32 dstStep,
	INT32 width,  INT32 height)
{
	IppiSize sz;
	sz.width  = width;
	sz.height = height;
	return ippiAlphaComp_8u_AC4R(pSrc1, src1Step, pSrc2, src2Step,
		pDst, dstStep, sz, ippAlphaOver);
}
#endif

/* ------------------------------------------------------------------------- */
void primitives_init_alphaComp(const primitives_hints_t* hints, primitives_t* prims)
{
	prims->alphaComp_argb = general_alphaComp_argb;
#ifdef WITH_IPP
	prims->alphaComp_argb = ipp_alphaComp_argb;
#elif defined(WITH_SSE2)
	if ((hints->x86_flags & PRIM_X86_SSE2_AVAILABLE)
			&& (hints->x86_flags & PRIM_X86_SSE3_AVAILABLE))	/* for LDDQU */
	{
		prims->alphaComp_argb = sse2_alphaComp_argb;
	}
#endif
}

/* ------------------------------------------------------------------------- */
void primitives_deinit_alphaComp(primitives_t *prims)
{
	/* Nothing to do. */
}
