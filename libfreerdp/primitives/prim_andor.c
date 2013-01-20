/* FreeRDP: A Remote Desktop Protocol Client
 * Logical operations.
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

#include <config.h>
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
 * 32-bit AND with a constant.
 */
PRIM_STATIC pstatus_t general_andC_32u(
	const UINT32 *pSrc,
	UINT32 val,
	UINT32 *pDst,
	INT32 len)
{
	if (val == 0) return PRIMITIVES_SUCCESS;
	while (len--) *pDst++ = *pSrc++ & val;
	return PRIMITIVES_SUCCESS;
}

/* ----------------------------------------------------------------------------
 * 32-bit OR with a constant.
 */
PRIM_STATIC pstatus_t general_orC_32u(
	const UINT32 *pSrc,
	UINT32 val,
	UINT32 *pDst,
	INT32 len)
{
	if (val == 0) return PRIMITIVES_SUCCESS;
	while (len--) *pDst++ = *pSrc++ | val;
	return PRIMITIVES_SUCCESS;
}

#ifdef WITH_SSE2
# if !defined(WITH_IPP) || defined(ALL_PRIMITIVES_VERSIONS)
/* ------------------------------------------------------------------------- */
SSE3_SCD_PRE_ROUTINE(sse3_andC_32u, UINT32, general_andC_32u,
	_mm_and_si128, *dptr++ = *sptr++ & val)
SSE3_SCD_PRE_ROUTINE(sse3_orC_32u, UINT32, general_orC_32u,
	_mm_or_si128, *dptr++ = *sptr++ | val)
# endif /* !defined(WITH_IPP) || defined(ALL_PRIMITIVES_VERSIONS) */
#endif

/* ------------------------------------------------------------------------- */
void primitives_init_andor(
	const primitives_hints_t *hints,
	primitives_t *prims)
{
	/* Start with the default. */
	prims->andC_32u = general_andC_32u;
	prims->orC_32u  = general_orC_32u;
#if defined(WITH_IPP)
	prims->andC_32u = (__andC_32u_t) ippsAndC_32u;
	prims->orC_32u  = (__orC_32u_t) ippsOrC_32u;
#elif defined(WITH_SSE2)
	if ((hints->x86_flags & PRIM_X86_SSE2_AVAILABLE)
			&& (hints->x86_flags & PRIM_X86_SSE3_AVAILABLE))
	{
		prims->andC_32u = sse3_andC_32u;
		prims->orC_32u  = sse3_orC_32u;
	}
#endif
}

/* ------------------------------------------------------------------------- */
void primitives_deinit_andor(
	primitives_t *prims)
{
	/* Nothing to do. */
}
