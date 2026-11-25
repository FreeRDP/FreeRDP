/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Optimized add operations using NEON
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
#include "prim_add.h"

#if defined(NEON_INTRINSICS_ENABLED)
#include <arm_neon.h>

static primitives_t* generic = nullptr;

static pstatus_t neon_add_16s_inplace(INT16* WINPR_RESTRICT pSrcDst1,
                                      INT16* WINPR_RESTRICT pSrcDst2, UINT32 ulen)
{
	INT16* dptr1 = pSrcDst1;
	INT16* dptr2 = pSrcDst2;
	size_t len = ulen;

	/* Use 2 128-bit NEON registers. */
	size_t count = len >> 4;
	len -= count << 4;
	while (count--)
	{
		const int16x8_t a0 = vld1q_s16(dptr1);
		const int16x8_t a1 = vld1q_s16(dptr1 + 8);
		const int16x8_t b0 = vld1q_s16(dptr2);
		const int16x8_t b1 = vld1q_s16(dptr2 + 8);
		const int16x8_t sum0 = vqaddq_s16(a0, b0);
		const int16x8_t sum1 = vqaddq_s16(a1, b1);

		vst1q_s16(dptr1, sum0);
		vst1q_s16(dptr1 + 8, sum1);
		vst1q_s16(dptr2, sum0);
		vst1q_s16(dptr2 + 8, sum1);

		dptr1 += 16;
		dptr2 += 16;
	}
	/* Finish off the remainder. */
	if (len > 0)
		return generic->add_16s_inplace(dptr1, dptr2, WINPR_ASSERTING_INT_CAST(uint32_t, len));

	return PRIMITIVES_SUCCESS;
}
#endif

/* ------------------------------------------------------------------------- */
void primitives_init_add_neon_int(primitives_t* WINPR_RESTRICT prims)
{
#if defined(NEON_INTRINSICS_ENABLED)
	generic = primitives_get_generic();

	WLog_VRB(PRIM_TAG, "NEON optimizations");
	prims->add_16s_inplace = neon_add_16s_inplace;
#else
	WLog_VRB(PRIM_TAG, "undefined WITH_SIMD or neon intrinsics not available");
	WINPR_UNUSED(prims);
#endif
}
