/* FreeRDP: A Remote Desktop Protocol Client
 * Optimized Logical operations.
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

#include "prim_andor.h"

#include "prim_internal.h"
#include "prim_templates.h"

#if defined(SSE_AVX_INTRINSICS_ENABLED)
#include <emmintrin.h>
#include <pmmintrin.h>

static primitives_t* generic = NULL;

/* ------------------------------------------------------------------------- */
SSE3_SCD_PRE_ROUTINE(sse3_andC_32u, UINT32, generic->andC_32u, _mm_and_si128,
                     *dptr++ = *sptr++ & val)
SSE3_SCD_PRE_ROUTINE(sse3_orC_32u, UINT32, generic->orC_32u, _mm_or_si128, *dptr++ = *sptr++ | val)

#endif

/* ------------------------------------------------------------------------- */
void primitives_init_andor_sse3(primitives_t* WINPR_RESTRICT prims)
{
#if defined(SSE_AVX_INTRINSICS_ENABLED)
	generic = primitives_get_generic();
	primitives_init_andor(prims);

	if (IsProcessorFeaturePresent(PF_SSE2_INSTRUCTIONS_AVAILABLE) &&
	    IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE))
	{
		WLog_VRB(PRIM_TAG, "SSE2/SSE3 optimizations");
		prims->andC_32u = sse3_andC_32u;
		prims->orC_32u = sse3_orC_32u;
	}

#else
	WLog_VRB(PRIM_TAG, "undefined WITH_SIMD or SSE3 intrinsics not available");
	WINPR_UNUSED(prims);
#endif
}
