/* FreeRDP: A Remote Desktop Protocol Client
 * Shift operations.
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
 */

#include <freerdp/config.h>

#include <freerdp/types.h>
#include <freerdp/primitives.h>
#include <winpr/sysinfo.h>

#include "prim_shift.h"

#include "prim_internal.h"
#include "prim_templates.h"

#if defined(SSE_AVX_INTRINSICS_ENABLED)
#include <emmintrin.h>
#include <pmmintrin.h>

static primitives_t* generic = NULL;

/* ------------------------------------------------------------------------- */
SSE3_SCD_ROUTINE(sse2_lShiftC_16s, INT16, generic->lShiftC_16s, _mm_slli_epi16,
                 *dptr++ = (INT16)((UINT16)*sptr++ << val))
/* ------------------------------------------------------------------------- */
SSE3_SCD_ROUTINE(sse2_rShiftC_16s, INT16, generic->rShiftC_16s, _mm_srai_epi16,
                 *dptr++ = *sptr++ >> val)
/* ------------------------------------------------------------------------- */
SSE3_SCD_ROUTINE(sse2_lShiftC_16u, UINT16, generic->lShiftC_16u, _mm_slli_epi16,
                 *dptr++ = (INT16)((UINT16)*sptr++ << val))
/* ------------------------------------------------------------------------- */
SSE3_SCD_ROUTINE(sse2_rShiftC_16u, UINT16, generic->rShiftC_16u, _mm_srli_epi16,
                 *dptr++ = *sptr++ >> val)

static pstatus_t sse2_lShiftC_16s_inplace(INT16* WINPR_RESTRICT pSrcDst, UINT32 val, UINT32 len)
{
	const INT32 shifts = 2;
	if (val == 0)
		return PRIMITIVES_SUCCESS;
	if (val >= 16)
		return -1;
	if (len < 16) /* pointless if too small */
		return generic->lShiftC_16s_inplace(pSrcDst, val, len);

	UINT32 offBeatMask = (1 << (shifts - 1)) - 1;
	if ((ULONG_PTR)pSrcDst & offBeatMask)
	{
		/* Incrementing the pointer skips over 16-byte boundary. */
		return generic->lShiftC_16s_inplace(pSrcDst, val, len);
	}
	/* Get to the 16-byte boundary now. */
	const UINT32 rem = ((UINT_PTR)pSrcDst & 0x0f) / sizeof(INT16);
	if (rem > 0)
	{
		const UINT32 add = 16 - rem;
		pstatus_t status = generic->lShiftC_16s_inplace(pSrcDst, val, add);
		if (status != PRIMITIVES_SUCCESS)
			return status;
		pSrcDst += add;
		len -= add;
	}

	/* Use 8 128-bit SSE registers. */
	int count = len >> (8 - shifts);
	len -= count << (8 - shifts);

	while (count--)
	{
		const __m128i* src = (const __m128i*)pSrcDst;

		__m128i xmm0 = _mm_load_si128(src++);
		__m128i xmm1 = _mm_load_si128(src++);
		__m128i xmm2 = _mm_load_si128(src++);
		__m128i xmm3 = _mm_load_si128(src++);
		__m128i xmm4 = _mm_load_si128(src++);
		__m128i xmm5 = _mm_load_si128(src++);
		__m128i xmm6 = _mm_load_si128(src++);
		__m128i xmm7 = _mm_load_si128(src);

		xmm0 = _mm_slli_epi16(xmm0, val);
		xmm1 = _mm_slli_epi16(xmm1, val);
		xmm2 = _mm_slli_epi16(xmm2, val);
		xmm3 = _mm_slli_epi16(xmm3, val);
		xmm4 = _mm_slli_epi16(xmm4, val);
		xmm5 = _mm_slli_epi16(xmm5, val);
		xmm6 = _mm_slli_epi16(xmm6, val);
		xmm7 = _mm_slli_epi16(xmm7, val);

		__m128i* dst = (__m128i*)pSrcDst;

		_mm_store_si128(dst++, xmm0);
		_mm_store_si128(dst++, xmm1);
		_mm_store_si128(dst++, xmm2);
		_mm_store_si128(dst++, xmm3);
		_mm_store_si128(dst++, xmm4);
		_mm_store_si128(dst++, xmm5);
		_mm_store_si128(dst++, xmm6);
		_mm_store_si128(dst++, xmm7);

		pSrcDst = (INT16*)dst;
	}

	/* Use a single 128-bit SSE register. */
	count = len >> (5 - shifts);
	len -= count << (5 - shifts);
	while (count--)
	{
		const __m128i* src = (const __m128i*)pSrcDst;
		__m128i xmm0 = LOAD_SI128(src);

		xmm0 = _mm_slli_epi16(xmm0, val);

		__m128i* dst = (__m128i*)pSrcDst;
		_mm_store_si128(dst++, xmm0);
		pSrcDst = (INT16*)dst;
	}

	/* Finish off the remainder. */
	if (len > 0)
		return generic->lShiftC_16s_inplace(pSrcDst, val, len);

	return PRIMITIVES_SUCCESS;
}
#endif

/* Note: the IPP version will have to call ippLShiftC_16s or ippRShiftC_16s
 * depending on the sign of val.  To avoid using the deprecated inplace
 * routines, a wrapper can use the src for the dest.
 */

/* ------------------------------------------------------------------------- */
void primitives_init_shift_sse3(primitives_t* WINPR_RESTRICT prims)
{
#if defined(SSE_AVX_INTRINSICS_ENABLED)
	generic = primitives_get_generic();
	primitives_init_shift(prims);

	if (IsProcessorFeaturePresent(PF_SSE2_INSTRUCTIONS_AVAILABLE) &&
	    IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE))
	{
		WLog_VRB(PRIM_TAG, "SSE2/SSE3 optimizations");
		prims->lShiftC_16s_inplace = sse2_lShiftC_16s_inplace;
		prims->lShiftC_16s = sse2_lShiftC_16s;
		prims->rShiftC_16s = sse2_rShiftC_16s;
		prims->lShiftC_16u = sse2_lShiftC_16u;
		prims->rShiftC_16u = sse2_rShiftC_16u;
	}

#else
	WLog_VRB(PRIM_TAG, "undefined WITH_SIMD or SSE3 intrinsics not available");
	WINPR_UNUSED(prims);
#endif
}
