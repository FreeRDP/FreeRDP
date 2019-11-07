/* test_shift.c
 * vi:ts=4 sw=4
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

#include <winpr/sysinfo.h>
#include "prim_test.h"

#define FUNC_TEST_SIZE 65536

static BOOL test_lShift_16s_func(void)
{
	pstatus_t status;
	INT16 ALIGN(src[FUNC_TEST_SIZE + 3]);
	INT16 ALIGN(d1[FUNC_TEST_SIZE + 3]);
	UINT32 val;
	winpr_RAND((BYTE*)&val, sizeof(val));
	winpr_RAND((BYTE*)src, sizeof(src));
	val = (val % (FUNC_TEST_SIZE - 1)) + 1;
	/* Aligned */
	status = generic->lShiftC_16s(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->lShiftC_16s(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	/* Unaligned */
	status = generic->lShiftC_16s(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->lShiftC_16s(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	return TRUE;
}

static BOOL test_lShift_16u_func(void)
{
	pstatus_t status;
	UINT16 ALIGN(src[FUNC_TEST_SIZE + 3]);
	UINT16 ALIGN(d1[FUNC_TEST_SIZE + 3]);
	UINT32 val;
	winpr_RAND((BYTE*)&val, sizeof(val));
	winpr_RAND((BYTE*)src, sizeof(src));
	val = (val % (FUNC_TEST_SIZE - 1)) + 1;
	/* Aligned */
	status = generic->lShiftC_16u(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->lShiftC_16u(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	/* Unaligned */
	status = generic->lShiftC_16u(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->lShiftC_16u(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	return TRUE;
}

static BOOL test_rShift_16s_func(void)
{
	pstatus_t status;
	INT16 ALIGN(src[FUNC_TEST_SIZE + 3]);
	INT16 ALIGN(d1[FUNC_TEST_SIZE + 3]);
	UINT32 val;
	winpr_RAND((BYTE*)&val, sizeof(val));
	winpr_RAND((BYTE*)src, sizeof(src));
	val = (val % (FUNC_TEST_SIZE - 1)) + 1;
	/* Aligned */
	status = generic->rShiftC_16s(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->rShiftC_16s(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	/* Unaligned */
	status = generic->rShiftC_16s(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->rShiftC_16s(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	return TRUE;
}

static BOOL test_rShift_16u_func(void)
{
	pstatus_t status;
	UINT16 ALIGN(src[FUNC_TEST_SIZE + 3]);
	UINT16 ALIGN(d1[FUNC_TEST_SIZE + 3]);
	UINT32 val;
	winpr_RAND((BYTE*)&val, sizeof(val));
	winpr_RAND((BYTE*)src, sizeof(src));
	val = (val % (FUNC_TEST_SIZE - 1)) + 1;
	/* Aligned */
	status = generic->rShiftC_16u(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->rShiftC_16u(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	/* Unaligned */
	status = generic->rShiftC_16u(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->rShiftC_16u(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	return TRUE;
}

static BOOL test_ShiftWrapper_16s_func(void)
{
	pstatus_t status;
	INT16 ALIGN(src[FUNC_TEST_SIZE + 3]);
	INT16 ALIGN(d1[FUNC_TEST_SIZE + 3]);
	UINT32 tmp;
	INT32 val;
	winpr_RAND((BYTE*)&tmp, sizeof(tmp));
	winpr_RAND((BYTE*)src, sizeof(src));
	val = (tmp % (FUNC_TEST_SIZE - 1)) + 1;
	/* Aligned */
	status = generic->shiftC_16s(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->shiftC_16s(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = generic->shiftC_16s(src + 1, -val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->shiftC_16s(src + 1, -val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	/* Unaligned */
	status = generic->shiftC_16s(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->shiftC_16s(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = generic->shiftC_16s(src + 1, -val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->shiftC_16s(src + 1, -val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	return TRUE;
}

static BOOL test_ShiftWrapper_16u_func(void)
{
	pstatus_t status;
	UINT16 ALIGN(src[FUNC_TEST_SIZE + 3]);
	UINT16 ALIGN(d1[FUNC_TEST_SIZE + 3]);
	UINT32 tmp;
	INT32 val;
	winpr_RAND((BYTE*)&tmp, sizeof(tmp));
	winpr_RAND((BYTE*)src, sizeof(src));
	val = (tmp % (FUNC_TEST_SIZE - 1)) + 1;
	/* Aligned */
	status = generic->shiftC_16u(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->shiftC_16u(src + 1, val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = generic->shiftC_16u(src + 1, -val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->shiftC_16u(src + 1, -val, d1 + 1, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	/* Unaligned */
	status = generic->shiftC_16u(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->shiftC_16u(src + 1, val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = generic->shiftC_16u(src + 1, -val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->shiftC_16u(src + 1, -val, d1 + 2, FUNC_TEST_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	return TRUE;
}

/* ------------------------------------------------------------------------- */
static BOOL test_lShift_16s_speed(void)
{
	UINT32 val;
	INT16 ALIGN(src[MAX_TEST_SIZE + 1]), ALIGN(dst[MAX_TEST_SIZE + 1]);
	winpr_RAND((BYTE*)src, sizeof(src));
	winpr_RAND((BYTE*)&val, sizeof(val));

	if (!speed_test("lShift_16s", "aligned", g_Iterations, (speed_test_fkt)generic->lShiftC_16s,
	                (speed_test_fkt)optimized->lShiftC_16s, src, val, dst, MAX_TEST_SIZE))
		return FALSE;

	if (!speed_test("lShift_16s", "unaligned", g_Iterations, (speed_test_fkt)generic->lShiftC_16s,
	                (speed_test_fkt)optimized->lShiftC_16s, src + 1, val, dst, MAX_TEST_SIZE))
		return FALSE;

	return TRUE;
}

/* ------------------------------------------------------------------------- */
static BOOL test_lShift_16u_speed(void)
{
	UINT32 val;
	UINT16 ALIGN(src[MAX_TEST_SIZE + 1]), ALIGN(dst[MAX_TEST_SIZE + 1]);
	winpr_RAND((BYTE*)&val, sizeof(val));
	winpr_RAND((BYTE*)src, sizeof(src));

	if (!speed_test("lShift_16u", "aligned", g_Iterations, (speed_test_fkt)generic->lShiftC_16u,
	                (speed_test_fkt)optimized->lShiftC_16u, src, val, dst, MAX_TEST_SIZE))
		return FALSE;

	if (!speed_test("lShift_16u", "unaligned", g_Iterations, (speed_test_fkt)generic->lShiftC_16u,
	                (speed_test_fkt)optimized->lShiftC_16u, src + 1, val, dst, MAX_TEST_SIZE))
		return FALSE;

	return TRUE;
}

/* ------------------------------------------------------------------------- */
static BOOL test_rShift_16s_speed(void)
{
	UINT32 val;
	INT16 ALIGN(src[MAX_TEST_SIZE + 1]), ALIGN(dst[MAX_TEST_SIZE + 1]);
	winpr_RAND((BYTE*)src, sizeof(src));
	winpr_RAND((BYTE*)&val, sizeof(val));

	if (!speed_test("rShift_16s", "aligned", g_Iterations, (speed_test_fkt)generic->rShiftC_16s,
	                (speed_test_fkt)optimized->rShiftC_16s, src, val, dst, MAX_TEST_SIZE))
		return FALSE;

	if (!speed_test("rShift_16s", "unaligned", g_Iterations, (speed_test_fkt)generic->rShiftC_16s,
	                (speed_test_fkt)optimized->rShiftC_16s, src + 1, val, dst, MAX_TEST_SIZE))
		return FALSE;

	return TRUE;
}

/* ------------------------------------------------------------------------- */
static BOOL test_rShift_16u_speed(void)
{
	UINT32 val;
	UINT16 ALIGN(src[MAX_TEST_SIZE + 1]), ALIGN(dst[MAX_TEST_SIZE + 1]);
	winpr_RAND((BYTE*)&val, sizeof(val));
	winpr_RAND((BYTE*)src, sizeof(src));

	if (!speed_test("rShift_16u", "aligned", g_Iterations, (speed_test_fkt)generic->rShiftC_16u,
	                (speed_test_fkt)optimized->rShiftC_16u, src, val, dst, MAX_TEST_SIZE))
		return FALSE;

	if (!speed_test("rShift_16u", "unaligned", g_Iterations, (speed_test_fkt)generic->rShiftC_16u,
	                (speed_test_fkt)optimized->rShiftC_16u, src + 1, val, dst, MAX_TEST_SIZE))
		return FALSE;

	return TRUE;
}

int TestPrimitivesShift(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	prim_test_setup(FALSE);

	if (!test_lShift_16s_func())
		return 1;

	if (g_TestPrimitivesPerformance)
	{
		if (!test_lShift_16s_speed())
			return 1;
	}

	if (!test_lShift_16u_func())
		return 1;

	if (g_TestPrimitivesPerformance)
	{
		if (!test_lShift_16u_speed())
			return 1;
	}

	if (!test_rShift_16s_func())
		return 1;

	if (g_TestPrimitivesPerformance)
	{
		if (!test_rShift_16s_speed())
			return 1;
	}

	if (!test_rShift_16u_func())
		return 1;

	if (g_TestPrimitivesPerformance)
	{
		if (!test_rShift_16u_speed())
			return 1;
	}

	if (!test_ShiftWrapper_16s_func())
		return 1;

	if (!test_ShiftWrapper_16u_func())
		return 1;

	return 0;
}
