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

#ifdef WITH_SSE2
#include <emmintrin.h>
#include <pmmintrin.h>
#endif /* WITH_SSE2 */

#ifdef WITH_IPP
#include <ipps.h>
#endif /* WITH_IPP */

#include "prim_internal.h"
#include "prim_templates.h"

static primitives_t* generic = NULL;

#ifdef WITH_SSE2
#if !defined(WITH_IPP) || defined(ALL_PRIMITIVES_VERSIONS)
/* ------------------------------------------------------------------------- */
SSE3_SCD_PRE_ROUTINE(sse3_andC_32u, UINT32, generic->andC_32u, _mm_and_si128,
                     *dptr++ = *sptr++ & val)
SSE3_SCD_PRE_ROUTINE(sse3_orC_32u, UINT32, generic->orC_32u, _mm_or_si128, *dptr++ = *sptr++ | val)
#endif /* !defined(WITH_IPP) || defined(ALL_PRIMITIVES_VERSIONS) */
#endif

/* ------------------------------------------------------------------------- */
void primitives_init_andor_opt(primitives_t* prims)
{
	generic = primitives_get_generic();
	primitives_init_andor(prims);
#if defined(WITH_IPP)
	prims->andC_32u = (__andC_32u_t)ippsAndC_32u;
	prims->orC_32u = (__orC_32u_t)ippsOrC_32u;
#elif defined(WITH_SSE2)

	if (IsProcessorFeaturePresent(PF_SSE2_INSTRUCTIONS_AVAILABLE) &&
	    IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE))
	{
		prims->andC_32u = sse3_andC_32u;
		prims->orC_32u = sse3_orC_32u;
	}

#endif
}
