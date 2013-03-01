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
#include "prim_shift.h"


#ifdef WITH_SSE2
# if !defined(WITH_IPP) || defined(ALL_PRIMITIVES_VERSIONS)
/* ------------------------------------------------------------------------- */
SSE3_SCD_ROUTINE(sse2_lShiftC_16s, INT16, general_lShiftC_16s,
	_mm_slli_epi16, *dptr++ = *sptr++ << val)
/* ------------------------------------------------------------------------- */
SSE3_SCD_ROUTINE(sse2_rShiftC_16s, INT16, general_rShiftC_16s,
	_mm_srai_epi16, *dptr++ = *sptr++ >> val)
/* ------------------------------------------------------------------------- */
SSE3_SCD_ROUTINE(sse2_lShiftC_16u, UINT16, general_lShiftC_16u,
	_mm_slli_epi16, *dptr++ = *sptr++ << val)
/* ------------------------------------------------------------------------- */
SSE3_SCD_ROUTINE(sse2_rShiftC_16u, UINT16, general_rShiftC_16u,
	_mm_srli_epi16, *dptr++ = *sptr++ >> val)
# endif /* !defined(WITH_IPP) || defined(ALL_PRIMITIVES_VERSIONS) */
#endif


/* Note: the IPP version will have to call ippLShiftC_16s or ippRShiftC_16s
 * depending on the sign of val.  To avoid using the deprecated inplace
 * routines, a wrapper can use the src for the dest.
 */

/* ------------------------------------------------------------------------- */
void primitives_init_shift_opt(primitives_t *prims)
{
#if defined(WITH_IPP)
	prims->lShiftC_16s = (__lShiftC_16s_t) ippsLShiftC_16s;
	prims->rShiftC_16s = (__rShiftC_16s_t) ippsRShiftC_16s;
	prims->lShiftC_16u = (__lShiftC_16u_t) ippsLShiftC_16u;
	prims->rShiftC_16u = (__rShiftC_16u_t) ippsRShiftC_16u;
#elif defined(WITH_SSE2)
	if (IsProcessorFeaturePresent(PF_SSE2_INSTRUCTIONS_AVAILABLE)
			&& IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE))
	{
		prims->lShiftC_16s = sse2_lShiftC_16s;
		prims->rShiftC_16s = sse2_rShiftC_16s;
		prims->lShiftC_16u = sse2_lShiftC_16u;
		prims->rShiftC_16u = sse2_rShiftC_16u;
	}
#endif
}

