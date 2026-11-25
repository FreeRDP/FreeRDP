/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Optimized shift operations using NEON
 *
 * Copyright 2026 Hayaki Saito <saitoha@me.com>
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

#include "prim_internal.h"
#include "prim_shift.h"

#if defined(NEON_INTRINSICS_ENABLED)
#include <arm_neon.h>

static primitives_t* generic = nullptr;

static pstatus_t neon_lShiftC_16s_inplace(INT16* WINPR_RESTRICT pSrcDst, UINT32 val, UINT32 ulen)
{
	INT16* dptr = pSrcDst;
	size_t len = ulen;
	if (val == 0)
		return PRIMITIVES_SUCCESS;
	if (val >= 16)
		return -1;

	const int16x8_t shift = vdupq_n_s16((INT16)val);

	/* Use 2 128-bit NEON registers. */
	size_t count = len >> 4;
	len -= count << 4;
	while (count--)
	{
		const int16x8_t v0 = vld1q_s16(dptr);
		const int16x8_t v1 = vld1q_s16(dptr + 8);

		vst1q_s16(dptr, vshlq_s16(v0, shift));
		vst1q_s16(dptr + 8, vshlq_s16(v1, shift));

		dptr += 16;
	}
	/* Finish off the remainder. */
	if (len > 0)
		return generic->lShiftC_16s_inplace(dptr, val, WINPR_ASSERTING_INT_CAST(uint32_t, len));

	return PRIMITIVES_SUCCESS;
}
#endif

/* ------------------------------------------------------------------------- */
void primitives_init_shift_neon_int(primitives_t* WINPR_RESTRICT prims)
{
#if defined(NEON_INTRINSICS_ENABLED)
	generic = primitives_get_generic();

	WLog_VRB(PRIM_TAG, "NEON optimizations");
	prims->lShiftC_16s_inplace = neon_lShiftC_16s_inplace;
#else
	WLog_VRB(PRIM_TAG, "undefined WITH_SIMD or neon intrinsics not available");
	WINPR_UNUSED(prims);
#endif
}
