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

#include "prim_add.h"

#include "prim_internal.h"
#include "prim_templates.h"

#if defined(SSE_AVX_INTRINSICS_ENABLED)
#include <emmintrin.h>
#include <pmmintrin.h>

static primitives_t* generic = NULL;

/* ------------------------------------------------------------------------- */
SSE3_SSD_ROUTINE(sse3_add_16s, INT16, generic->add_16s, _mm_adds_epi16,
                 generic->add_16s(sptr1++, sptr2++, dptr++, 1))

static pstatus_t sse3_add_16s_inplace(INT16* WINPR_RESTRICT pSrcDst1,
                                      INT16* WINPR_RESTRICT pSrcDst2, UINT32 len)
{
	const int shifts = 2;
	INT16* dptr1 = pSrcDst1;
	INT16* dptr2 = pSrcDst2;

	if (len < 16) /* pointless if too small */
		return generic->add_16s_inplace(pSrcDst1, pSrcDst2, len);

	UINT32 offBeatMask = (1 << (shifts - 1)) - 1;
	if ((ULONG_PTR)pSrcDst1 & offBeatMask)
	{
		/* Incrementing the pointer skips over 16-byte boundary. */
		return generic->add_16s_inplace(pSrcDst1, pSrcDst2, len);
	}
	/* Get to the 16-byte boundary now. */
	const size_t rem = ((UINT_PTR)dptr1 & 0xf) / sizeof(INT16);
	if (rem != 0)
	{
		const UINT32 add = 16 - (UINT32)rem;
		pstatus_t status = generic->add_16s_inplace(dptr1, dptr2, add);
		if (status != PRIMITIVES_SUCCESS)
			return status;
		dptr1 += add;
		dptr2 += add;
	}
	/* Use 4 128-bit SSE registers. */
	size_t count = len >> (7 - shifts);
	len -= count << (7 - shifts);
	if (((const ULONG_PTR)dptr1 & 0x0f) || ((const ULONG_PTR)dptr2 & 0x0f))
	{
		/* Unaligned loads */
		while (count--)
		{
			const __m128i* vsptr1 = (const __m128i*)dptr1;
			const __m128i* vsptr2 = (const __m128i*)dptr2;
			__m128i* vdptr1 = (__m128i*)dptr1;
			__m128i* vdptr2 = (__m128i*)dptr2;

			__m128i xmm0 = _mm_lddqu_si128(vsptr1++);
			__m128i xmm1 = _mm_lddqu_si128(vsptr1++);
			__m128i xmm2 = _mm_lddqu_si128(vsptr1++);
			__m128i xmm3 = _mm_lddqu_si128(vsptr1++);
			__m128i xmm4 = _mm_lddqu_si128(vsptr2++);
			__m128i xmm5 = _mm_lddqu_si128(vsptr2++);
			__m128i xmm6 = _mm_lddqu_si128(vsptr2++);
			__m128i xmm7 = _mm_lddqu_si128(vsptr2++);

			xmm0 = _mm_adds_epi16(xmm0, xmm4);
			xmm1 = _mm_adds_epi16(xmm1, xmm5);
			xmm2 = _mm_adds_epi16(xmm2, xmm6);
			xmm3 = _mm_adds_epi16(xmm3, xmm7);

			_mm_store_si128(vdptr1++, xmm0);
			_mm_store_si128(vdptr1++, xmm1);
			_mm_store_si128(vdptr1++, xmm2);
			_mm_store_si128(vdptr1++, xmm3);

			_mm_store_si128(vdptr2++, xmm0);
			_mm_store_si128(vdptr2++, xmm1);
			_mm_store_si128(vdptr2++, xmm2);
			_mm_store_si128(vdptr2++, xmm3);

			dptr1 = (INT16*)vdptr1;
			dptr2 = (INT16*)vdptr2;
		}
	}
	else
	{
		/* Aligned loads */
		while (count--)
		{
			const __m128i* vsptr1 = (const __m128i*)dptr1;
			const __m128i* vsptr2 = (const __m128i*)dptr2;
			__m128i* vdptr1 = (__m128i*)dptr1;
			__m128i* vdptr2 = (__m128i*)dptr2;

			__m128i xmm0 = _mm_load_si128(vsptr1++);
			__m128i xmm1 = _mm_load_si128(vsptr1++);
			__m128i xmm2 = _mm_load_si128(vsptr1++);
			__m128i xmm3 = _mm_load_si128(vsptr1++);
			__m128i xmm4 = _mm_load_si128(vsptr2++);
			__m128i xmm5 = _mm_load_si128(vsptr2++);
			__m128i xmm6 = _mm_load_si128(vsptr2++);
			__m128i xmm7 = _mm_load_si128(vsptr2++);

			xmm0 = _mm_adds_epi16(xmm0, xmm4);
			xmm1 = _mm_adds_epi16(xmm1, xmm5);
			xmm2 = _mm_adds_epi16(xmm2, xmm6);
			xmm3 = _mm_adds_epi16(xmm3, xmm7);

			_mm_store_si128(vdptr1++, xmm0);
			_mm_store_si128(vdptr1++, xmm1);
			_mm_store_si128(vdptr1++, xmm2);
			_mm_store_si128(vdptr1++, xmm3);

			_mm_store_si128(vdptr2++, xmm0);
			_mm_store_si128(vdptr2++, xmm1);
			_mm_store_si128(vdptr2++, xmm2);
			_mm_store_si128(vdptr2++, xmm3);

			dptr1 = (INT16*)vdptr1;
			dptr2 = (INT16*)vdptr2;
		}
	}
	/* Use a single 128-bit SSE register. */
	count = len >> (5 - shifts);
	len -= count << (5 - shifts);
	while (count--)
	{
		const __m128i* vsptr1 = (const __m128i*)dptr1;
		const __m128i* vsptr2 = (const __m128i*)dptr2;
		__m128i* vdptr1 = (__m128i*)dptr1;
		__m128i* vdptr2 = (__m128i*)dptr2;

		__m128i xmm0 = LOAD_SI128(vsptr1);
		__m128i xmm1 = LOAD_SI128(vsptr2);

		xmm0 = _mm_adds_epi16(xmm0, xmm1);

		_mm_store_si128(vdptr1++, xmm0);
		_mm_store_si128(vdptr2++, xmm0);

		dptr1 = (INT16*)vdptr1;
		dptr2 = (INT16*)vdptr2;
	}
	/* Finish off the remainder. */
	if (len > 0)
		return generic->add_16s_inplace(dptr1, dptr2, len);

	return PRIMITIVES_SUCCESS;
}
#endif

/* ------------------------------------------------------------------------- */
void primitives_init_add_sse3(primitives_t* WINPR_RESTRICT prims)
{
#if defined(SSE_AVX_INTRINSICS_ENABLED)
	generic = primitives_get_generic();
	primitives_init_add(prims);

	if (IsProcessorFeaturePresent(PF_SSE2_INSTRUCTIONS_AVAILABLE) &&
	    IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE)) /* for LDDQU */
	{
		WLog_VRB(PRIM_TAG, "SSE2/SSE3 optimizations");
		prims->add_16s = sse3_add_16s;
		prims->add_16s_inplace = sse3_add_16s_inplace;
	}

#else
	WLog_VRB(PRIM_TAG, "undefined WITH_SIMD or SSE3 intrinsics not available");
	WINPR_UNUSED(prims);
#endif
}
