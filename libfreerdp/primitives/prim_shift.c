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

#include "prim_internal.h"
/* ------------------------------------------------------------------------- */
static INLINE pstatus_t general_lShiftC_16s(const INT16* pSrc, UINT32 val, INT16* pDst, UINT32 len)
{
	if (val == 0)
		return PRIMITIVES_SUCCESS;

	while (len--)
		*pDst++ = *pSrc++ << val;

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
static INLINE pstatus_t general_rShiftC_16s(const INT16* pSrc, UINT32 val, INT16* pDst, UINT32 len)
{
	if (val == 0)
		return PRIMITIVES_SUCCESS;

	while (len--)
		*pDst++ = *pSrc++ >> val;

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
static INLINE pstatus_t general_lShiftC_16u(const UINT16* pSrc, UINT32 val, UINT16* pDst,
                                            UINT32 len)
{
	if (val == 0)
		return PRIMITIVES_SUCCESS;

	while (len--)
		*pDst++ = *pSrc++ << val;

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
static INLINE pstatus_t general_rShiftC_16u(const UINT16* pSrc, UINT32 val, UINT16* pDst,
                                            UINT32 len)
{
	if (val == 0)
		return PRIMITIVES_SUCCESS;

	while (len--)
		*pDst++ = *pSrc++ >> val;

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
static INLINE pstatus_t general_shiftC_16s(const INT16* pSrc, INT32 val, INT16* pDst, UINT32 len)
{
	if (val == 0)
		return PRIMITIVES_SUCCESS;

	if (val < 0)
		return general_rShiftC_16s(pSrc, -val, pDst, len);
	else
		return general_lShiftC_16s(pSrc, val, pDst, len);
}

/* ------------------------------------------------------------------------- */
static INLINE pstatus_t general_shiftC_16u(const UINT16* pSrc, INT32 val, UINT16* pDst, UINT32 len)
{
	if (val == 0)
		return PRIMITIVES_SUCCESS;

	if (val < 0)
		return general_rShiftC_16u(pSrc, -val, pDst, len);
	else
		return general_lShiftC_16u(pSrc, val, pDst, len);
}

/* ------------------------------------------------------------------------- */
void primitives_init_shift(primitives_t* prims)
{
	/* Start with the default. */
	prims->lShiftC_16s = general_lShiftC_16s;
	prims->rShiftC_16s = general_rShiftC_16s;
	prims->lShiftC_16u = general_lShiftC_16u;
	prims->rShiftC_16u = general_rShiftC_16u;
	/* Wrappers */
	prims->shiftC_16s = general_shiftC_16s;
	prims->shiftC_16u = general_shiftC_16u;
}
