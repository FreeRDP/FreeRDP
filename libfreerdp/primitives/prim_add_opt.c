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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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
SSE3_SSD_ROUTINE(sse3_add_16s, INT16, generic->add_16s, _mm_adds_epi16,
                 generic->add_16s(sptr1++, sptr2++, dptr++, 1))
#endif /* !defined(WITH_IPP) || defined(ALL_PRIMITIVES_VERSIONS) */
#endif

/* ------------------------------------------------------------------------- */
void primitives_init_add_opt(primitives_t* prims)
{
	generic = primitives_get_generic();
	primitives_init_add(prims);
#ifdef WITH_IPP
	prims->add_16s = (__add_16s_t)ippsAdd_16s;
#elif defined(WITH_SSE2)

	if (IsProcessorFeaturePresent(PF_SSE2_INSTRUCTIONS_AVAILABLE) &&
	    IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE)) /* for LDDQU */
	{
		prims->add_16s = sse3_add_16s;
	}

#endif
}
