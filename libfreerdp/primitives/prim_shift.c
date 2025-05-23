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

#include <freerdp/config.h>
#include <winpr/assert.h>
#include <winpr/cast.h>

#include <freerdp/types.h>
#include <freerdp/primitives.h>

#include "prim_internal.h"
#include "prim_shift.h"

/* ------------------------------------------------------------------------- */
static INLINE INT16 shift(INT16 val, UINT32 sh)
{
	const INT16 rc = (int16_t)(((UINT32)val << sh) & 0xFFFF);
	return WINPR_ASSERTING_INT_CAST(INT16, rc);
}

static INLINE pstatus_t general_lShiftC_16s_inplace(INT16* WINPR_RESTRICT pSrcDst, UINT32 val,
                                                    UINT32 len)
{
	if (val == 0)
		return PRIMITIVES_SUCCESS;
	if (val >= 16)
		return -1;

	for (UINT32 x = 0; x < len; x++)
		pSrcDst[x] = shift(pSrcDst[x], val);

	return PRIMITIVES_SUCCESS;
}

static INLINE pstatus_t general_lShiftC_16s(const INT16* WINPR_RESTRICT pSrc, UINT32 val,
                                            INT16* WINPR_RESTRICT pDst, UINT32 len)
{
	if (val == 0)
		return PRIMITIVES_SUCCESS;
	if (val >= 16)
		return -1;

	for (UINT32 x = 0; x < len; x++)
		pDst[x] = shift(pSrc[x], val);

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
static INLINE pstatus_t general_rShiftC_16s(const INT16* WINPR_RESTRICT pSrc, UINT32 val,
                                            INT16* WINPR_RESTRICT pDst, UINT32 len)
{
	if (val == 0)
		return PRIMITIVES_SUCCESS;
	if (val >= 16)
		return -1;

	for (UINT32 x = 0; x < len; x++)
		pDst[x] = WINPR_ASSERTING_INT_CAST(int16_t, pSrc[x] >> val);

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
static INLINE pstatus_t general_lShiftC_16u(const UINT16* WINPR_RESTRICT pSrc, UINT32 val,
                                            UINT16* WINPR_RESTRICT pDst, UINT32 len)
{
	if (val == 0)
		return PRIMITIVES_SUCCESS;
	if (val >= 16)
		return -1;

	for (UINT32 x = 0; x < len; x++)
		pDst[x] = WINPR_ASSERTING_INT_CAST(UINT16, ((pSrc[x] << val) & 0xFFFF));

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
static INLINE pstatus_t general_rShiftC_16u(const UINT16* WINPR_RESTRICT pSrc, UINT32 val,
                                            UINT16* WINPR_RESTRICT pDst, UINT32 len)
{
	if (val == 0)
		return PRIMITIVES_SUCCESS;
	if (val >= 16)
		return -1;

	for (UINT32 x = 0; x < len; x++)
		pDst[x] = pSrc[x] >> val;

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
static INLINE pstatus_t general_shiftC_16s(const INT16* WINPR_RESTRICT pSrc, INT32 val,
                                           INT16* WINPR_RESTRICT pDst, UINT32 len)
{
	if (val == 0)
		return PRIMITIVES_SUCCESS;

	if (val < 0)
		return general_rShiftC_16s(pSrc, WINPR_ASSERTING_INT_CAST(UINT32, -val), pDst, len);
	else
		return general_lShiftC_16s(pSrc, WINPR_ASSERTING_INT_CAST(UINT32, val), pDst, len);
}

/* ------------------------------------------------------------------------- */
static INLINE pstatus_t general_shiftC_16u(const UINT16* WINPR_RESTRICT pSrc, INT32 val,
                                           UINT16* WINPR_RESTRICT pDst, UINT32 len)
{
	if (val == 0)
		return PRIMITIVES_SUCCESS;

	if (val < 0)
		return general_rShiftC_16u(pSrc, WINPR_ASSERTING_INT_CAST(UINT32, -val), pDst, len);
	else
		return general_lShiftC_16u(pSrc, WINPR_ASSERTING_INT_CAST(UINT32, val), pDst, len);
}

/* ------------------------------------------------------------------------- */
void primitives_init_shift(primitives_t* WINPR_RESTRICT prims)
{
	/* Start with the default. */
	prims->lShiftC_16s_inplace = general_lShiftC_16s_inplace;
	prims->lShiftC_16s = general_lShiftC_16s;
	prims->rShiftC_16s = general_rShiftC_16s;
	prims->lShiftC_16u = general_lShiftC_16u;
	prims->rShiftC_16u = general_rShiftC_16u;
	/* Wrappers */
	prims->shiftC_16s = general_shiftC_16s;
	prims->shiftC_16u = general_shiftC_16u;
}

void primitives_init_shift_opt(primitives_t* WINPR_RESTRICT prims)
{
	primitives_init_shift(prims);
	primitives_init_shift_sse3(prims);
}
