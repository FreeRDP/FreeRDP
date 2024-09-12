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

#include <freerdp/config.h>

#include <stdint.h>

#include <freerdp/types.h>
#include <freerdp/primitives.h>

#include "prim_internal.h"
#include "prim_add.h"

/* ----------------------------------------------------------------------------
 * 16-bit signed add with saturation (under and over).
 */
static INLINE INT16 add(INT16 a, INT16 b)
{
	INT32 k = (INT32)a + (INT32)b;

	if (k > INT16_MAX)
		return INT16_MAX;

	if (k < INT16_MIN)
		return INT16_MIN;

	return (INT16)k;
}

static pstatus_t general_add_16s(const INT16* WINPR_RESTRICT pSrc1,
                                 const INT16* WINPR_RESTRICT pSrc2, INT16* WINPR_RESTRICT pDst,
                                 UINT32 len)
{
	const UINT32 rem = len % 16;
	const UINT32 align = len - rem;

	for (UINT32 x = 0; x < align; x++)
		*pDst++ = add(*pSrc1++, *pSrc2++);

	for (UINT32 x = 0; x < rem; x++)
		*pDst++ = add(*pSrc1++, *pSrc2++);

	return PRIMITIVES_SUCCESS;
}

static pstatus_t general_add_16s_inplace(INT16* WINPR_RESTRICT pSrcDst1,
                                         INT16* WINPR_RESTRICT pSrcDst2, UINT32 len)
{
	for (UINT32 x = 0; x < len; x++)
	{
		INT16 v = add(pSrcDst1[x], pSrcDst2[x]);
		pSrcDst1[x] = v;
		pSrcDst2[x] = v;
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
void primitives_init_add(primitives_t* WINPR_RESTRICT prims)
{
	prims->add_16s = general_add_16s;
	prims->add_16s_inplace = general_add_16s_inplace;
}

void primitives_init_add_opt(primitives_t* WINPR_RESTRICT prims)
{
	primitives_init_add_sse3(prims);
}
