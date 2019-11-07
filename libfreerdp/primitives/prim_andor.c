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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/types.h>
#include <freerdp/primitives.h>

#include "prim_internal.h"

/* ----------------------------------------------------------------------------
 * 32-bit AND with a constant.
 */
static pstatus_t general_andC_32u(const UINT32* pSrc, UINT32 val, UINT32* pDst, INT32 len)
{
	if (val == 0)
		return PRIMITIVES_SUCCESS;

	while (len--)
		*pDst++ = *pSrc++ & val;

	return PRIMITIVES_SUCCESS;
}

/* ----------------------------------------------------------------------------
 * 32-bit OR with a constant.
 */
static pstatus_t general_orC_32u(const UINT32* pSrc, UINT32 val, UINT32* pDst, INT32 len)
{
	if (val == 0)
		return PRIMITIVES_SUCCESS;

	while (len--)
		*pDst++ = *pSrc++ | val;

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
void primitives_init_andor(primitives_t* prims)
{
	/* Start with the default. */
	prims->andC_32u = general_andC_32u;
	prims->orC_32u = general_orC_32u;
}
