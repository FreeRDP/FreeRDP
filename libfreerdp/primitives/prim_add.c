/* FreeRDP: A Remote Desktop Protocol Client
 * Add operations.
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

#include <string.h>
#include <freerdp/types.h>
#include <freerdp/primitives.h>
#ifdef WITH_SSE2
# include <emmintrin.h>
# include <pmmintrin.h>
#endif /* WITH_SSE2 */
#ifdef WITH_IPP
# include <ipps.h>
#endif /* WITH_IPP */
#include "prim_internal.h"
#include "prim_templates.h"

/* ----------------------------------------------------------------------------
 * 16-bit signed add with saturation (under and over).
 */
PRIM_STATIC pstatus_t general_add_16s(
	const INT16 *pSrc1,
	const INT16 *pSrc2,
	INT16 *pDst,
	INT32 len)
{
	while (len--)
	{
		INT32 k = (INT32) (*pSrc1++) + (INT32) (*pSrc2++);
		if (k > 32767) *pDst++ = ((INT16) 32767);
		else if (k < -32768) *pDst++ = ((INT16) -32768);
		else *pDst++ = (INT16) k;
	}

	return PRIMITIVES_SUCCESS;
}

#ifdef WITH_SSE2
# if !defined(WITH_IPP) || defined(ALL_PRIMITIVES_VERSIONS)
/* ------------------------------------------------------------------------- */
SSE3_SSD_ROUTINE(sse3_add_16s, INT16, general_add_16s,
	_mm_adds_epi16, general_add_16s(sptr1++, sptr2++, dptr++, 1))
# endif /* !defined(WITH_IPP) || defined(ALL_PRIMITIVES_VERSIONS) */
#endif

/* ------------------------------------------------------------------------- */
void primitives_init_add(
	const primitives_hints_t *hints,
	primitives_t *prims)
{
	prims->add_16s = general_add_16s;
#ifdef WITH_IPP
	prims->add_16s = (__add_16s_t) ippsAdd_16s;
#elif defined(WITH_SSE2)
	if ((hints->x86_flags & PRIM_X86_SSE2_AVAILABLE)
			&& (hints->x86_flags & PRIM_X86_SSE3_AVAILABLE))	/* for LDDQU */
	{
		prims->add_16s = sse3_add_16s;
	}
#endif
}

/* ------------------------------------------------------------------------- */
void primitives_deinit_add(
	primitives_t *prims)
{
	/* Nothing to do. */
}
