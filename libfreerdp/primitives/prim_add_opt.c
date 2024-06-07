/* FreeRDP: A Remote Desktop Protocol Client
 * Optimized add operations.
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
 */

#include <freerdp/config.h>

#include <freerdp/types.h>
#include <freerdp/primitives.h>
#include <winpr/sysinfo.h>

#ifdef WITH_SSE2
#include <emmintrin.h>
#include <pmmintrin.h>
#endif /* WITH_SSE2 */

#include "prim_internal.h"
#include "prim_templates.h"

static primitives_t* generic = NULL;

#ifdef WITH_SSE2
/* ------------------------------------------------------------------------- */
SSE3_SSD_ROUTINE(sse3_add_16s, INT16, generic->add_16s, _mm_adds_epi16,
                 generic->add_16s(sptr1++, sptr2++, dptr++, 1))

static pstatus_t sse3_add_16s_inplace(INT16* WINPR_RESTRICT pSrcDst,
                                      const INT16* WINPR_RESTRICT pSrc, UINT32 len)
{
	const int shifts = 2;
	UINT32 offBeatMask;
	INT16* dptr = pSrcDst;
	const INT16* sptr = pSrc;

	size_t count;
	if (len < 16) /* pointless if too small */
		return generic->add_16s_inplace(pSrcDst, pSrc, len);

	offBeatMask = (1 << (shifts - 1)) - 1;
	if ((ULONG_PTR)pSrcDst & offBeatMask)
	{
		/* Incrementing the pointer skips over 16-byte boundary. */
		return generic->add_16s_inplace(pSrcDst, pSrc, len);
	}
	/* Get to the 16-byte boundary now. */
	const size_t rem = (ULONG_PTR)dptr & 0x0f;
	if (rem != 0)
	{
		pstatus_t status = generic->add_16s_inplace(dptr, sptr, rem);
		if (status != PRIMITIVES_SUCCESS)
			return status;
		dptr += rem;
		sptr += rem;
	}
	/* Use 4 128-bit SSE registers. */
	count = len >> (7 - shifts);
	len -= count << (7 - shifts);
	if (((const ULONG_PTR)dptr & 0x0f) || ((const ULONG_PTR)sptr & 0x0f))
	{
		/* Unaligned loads */
		while (count--)
		{
			const __m128i* sptr1 = dptr;
			const __m128i* sptr2 = sptr;
			__m128i* dptr1 = dptr;
			sptr += 4 * sizeof(__m128i);
			dptr += 4 * sizeof(__m128i);

			__m128i xmm0 = _mm_lddqu_si128(sptr1++);
			__m128i xmm1 = _mm_lddqu_si128(sptr1++);
			__m128i xmm2 = _mm_lddqu_si128(sptr1++);
			__m128i xmm3 = _mm_lddqu_si128(sptr1++);
			__m128i xmm4 = _mm_lddqu_si128(sptr2++);
			__m128i xmm5 = _mm_lddqu_si128(sptr2++);
			__m128i xmm6 = _mm_lddqu_si128(sptr2++);
			__m128i xmm7 = _mm_lddqu_si128(sptr2++);

			xmm0 = _mm_adds_epi16(xmm0, xmm4);
			xmm1 = _mm_adds_epi16(xmm1, xmm5);
			xmm2 = _mm_adds_epi16(xmm2, xmm6);
			xmm3 = _mm_adds_epi16(xmm3, xmm7);

			_mm_store_si128(dptr1++, xmm0);
			_mm_store_si128(dptr1++, xmm1);
			_mm_store_si128(dptr1++, xmm2);
			_mm_store_si128(dptr1++, xmm3);
		}
	}
	else
	{
		/* Aligned loads */
		while (count--)
		{
			const __m128i* sptr1 = dptr;
			const __m128i* sptr2 = sptr;
			__m128i* dptr1 = dptr;
			sptr += 4 * sizeof(__m128i);
			dptr += 4 * sizeof(__m128i);

			__m128i xmm0 = _mm_load_si128(sptr1++);
			__m128i xmm1 = _mm_load_si128(sptr1++);
			__m128i xmm2 = _mm_load_si128(sptr1++);
			__m128i xmm3 = _mm_load_si128(sptr1++);
			__m128i xmm4 = _mm_load_si128(sptr2++);
			__m128i xmm5 = _mm_load_si128(sptr2++);
			__m128i xmm6 = _mm_load_si128(sptr2++);
			__m128i xmm7 = _mm_load_si128(sptr2++);

			xmm0 = _mm_adds_epi16(xmm0, xmm4);
			xmm1 = _mm_adds_epi16(xmm1, xmm5);
			xmm2 = _mm_adds_epi16(xmm2, xmm6);
			xmm3 = _mm_adds_epi16(xmm3, xmm7);

			_mm_store_si128(dptr1, xmm0);
			_mm_store_si128(dptr1, xmm1);
			_mm_store_si128(dptr1, xmm2);
			_mm_store_si128(dptr1, xmm3);
		}
	}
	/* Use a single 128-bit SSE register. */
	count = len >> (5 - shifts);
	len -= count << (5 - shifts);
	while (count--)
	{
		const __m128i* sptr1 = sptr;
		__m128i* dptr1 = dptr;
		sptr += sizeof(__m128i);
		dptr += sizeof(__m128i);

		__m128i xmm0 = LOAD_SI128(sptr1);
		__m128i xmm1 = LOAD_SI128(dptr1);
		xmm0 = _mm_adds_epi16(xmm0, xmm1);
		_mm_store_si128(dptr, xmm0);
	}
	/* Finish off the remainder. */
	if (len > 0)
		return generic->add_16s_inplace(dptr, sptr, len);

	return PRIMITIVES_SUCCESS;
}

#endif

/* ------------------------------------------------------------------------- */
void primitives_init_add_opt(primitives_t* WINPR_RESTRICT prims)
{
	generic = primitives_get_generic();
	primitives_init_add(prims);

#if defined(WITH_SSE2)
	if (IsProcessorFeaturePresent(PF_SSE2_INSTRUCTIONS_AVAILABLE) &&
	    IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE)) /* for LDDQU */
	{
		prims->add_16s = sse3_add_16s;
		prims->add_16s_inplace = sse3_add_16s_inplace;
	}

#endif
}
