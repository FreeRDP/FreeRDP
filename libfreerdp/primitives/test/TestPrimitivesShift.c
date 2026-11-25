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

#include <freerdp/config.h>

#include <winpr/sysinfo.h>
#include "prim_test.h"

#define FUNC_TEST_SIZE 65536

static BOOL test_lShift_16s_func(void)
{
	pstatus_t status = 0;
	INT16 src[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	INT16 d1[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	UINT32 val = 0;
	if (winpr_RAND(&val, sizeof(val)) < 0)
		return FALSE;
	if (winpr_RAND(src, sizeof(src)) < 0)
		return FALSE;
	val = val % 16;
	/* Negative tests */
	status = generic->lShiftC_16s(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->lShiftC_16s(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

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

	return (status == PRIMITIVES_SUCCESS);
}

static BOOL test_lShift_16s_inplace_func(void)
{
	const UINT32 lengths[] = { 1, 7, 15, 16, 17, 33, FUNC_TEST_SIZE };
	INT16 d1[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	INT16 d2[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;

	if (winpr_RAND(d1, sizeof(d1)) < 0)
		return FALSE;
	memcpy(d2, d1, sizeof(d1));

	/* Negative tests */
	if (generic->lShiftC_16s_inplace(d1 + 1, 16, FUNC_TEST_SIZE) == PRIMITIVES_SUCCESS)
		return FALSE;
	if (optimized->lShiftC_16s_inplace(d2 + 1, 16, FUNC_TEST_SIZE) == PRIMITIVES_SUCCESS)
		return FALSE;
	if (memcmp(d1, d2, sizeof(d1)) != 0)
		return FALSE;

	for (UINT32 val = 0; val < 16; val++)
	{
		for (size_t i = 0; i < ARRAYSIZE(lengths); i++)
		{
			/* Aligned and unaligned */
			for (size_t offset = 0; offset < 2; offset++)
			{
				if (winpr_RAND(d1, sizeof(d1)) < 0)
					return FALSE;
				memcpy(d2, d1, sizeof(d1));

				if (generic->lShiftC_16s_inplace(d1 + offset, val, lengths[i]) !=
				    PRIMITIVES_SUCCESS)
					return FALSE;
				if (optimized->lShiftC_16s_inplace(d2 + offset, val, lengths[i]) !=
				    PRIMITIVES_SUCCESS)
					return FALSE;
				if (memcmp(d1, d2, sizeof(d1)) != 0)
				{
					(void)fprintf(stderr,
					              "lShiftC_16s_inplace mismatch, val=%" PRIu32 " len=%" PRIu32 "\n",
					              val, lengths[i]);
					return FALSE;
				}
			}
		}
	}

	return TRUE;
}

static BOOL test_lShift_16u_func(void)
{
	pstatus_t status = 0;
	UINT16 src[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	UINT16 d1[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	UINT32 val = 0;
	if (winpr_RAND(&val, sizeof(val)) < 0)
		return FALSE;
	if (winpr_RAND(src, sizeof(src)) < 0)
		return FALSE;
	val = val % 16;

	/* Negative tests */
	status = generic->lShiftC_16u(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->lShiftC_16u(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

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

	return (status == PRIMITIVES_SUCCESS);
}

static BOOL test_rShift_16s_func(void)
{
	pstatus_t status = 0;
	INT16 src[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	INT16 d1[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	UINT32 val = 0;
	if (winpr_RAND(&val, sizeof(val)) < 0)
		return FALSE;
	if (winpr_RAND(src, sizeof(src)) < 0)
		return FALSE;
	val = val % 16;

	/* Negative Tests */
	status = generic->rShiftC_16s(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->rShiftC_16s(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

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

	return (status == PRIMITIVES_SUCCESS);
}

static BOOL test_rShift_16u_func(void)
{
	pstatus_t status = 0;
	UINT16 src[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	UINT16 d1[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	UINT32 val = 0;
	if (winpr_RAND(&val, sizeof(val)) < 0)
		return FALSE;
	if (winpr_RAND(src, sizeof(src)) < 0)
		return FALSE;
	val = val % 16;
	/* Negative tests */
	status = generic->rShiftC_16u(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->rShiftC_16u(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

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

	return (status == PRIMITIVES_SUCCESS);
}

static BOOL test_ShiftWrapper_16s_func(void)
{
	pstatus_t status = 0;
	INT16 src[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	INT16 d1[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	UINT32 tmp = 0;
	if (winpr_RAND(&tmp, sizeof(tmp)) < 0)
		return FALSE;
	if (winpr_RAND(src, sizeof(src)) < 0)
		return FALSE;
	INT32 val = WINPR_ASSERTING_INT_CAST(int32_t, tmp % 16);

	/* Negative tests */
	status = generic->shiftC_16s(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->shiftC_16s(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

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

	return (status == PRIMITIVES_SUCCESS);
}

static BOOL test_ShiftWrapper_16u_func(void)
{
	pstatus_t status = 0;
	UINT16 src[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	UINT16 d1[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	UINT32 tmp = 0;
	if (winpr_RAND(&tmp, sizeof(tmp)) < 0)
		return FALSE;
	if (winpr_RAND(src, sizeof(src)) < 0)
		return FALSE;
	INT32 val = WINPR_ASSERTING_INT_CAST(int32_t, tmp % 16);

	/* Negative */
	status = generic->shiftC_16u(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);
	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->shiftC_16u(src + 1, 16, d1 + 1, FUNC_TEST_SIZE);

	if (status == PRIMITIVES_SUCCESS)
		return FALSE;

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

	return (status == PRIMITIVES_SUCCESS);
}

/* ------------------------------------------------------------------------- */
static BOOL test_lShift_16s_speed(void)
{
	UINT32 val = 0;
	INT16 src[MAX_TEST_SIZE + 1] = WINPR_C_ARRAY_INIT;
	INT16 dst[MAX_TEST_SIZE + 1] = WINPR_C_ARRAY_INIT;
	if (winpr_RAND(src, sizeof(src)) < 0)
		return FALSE;
	if (winpr_RAND(&val, sizeof(val)))
		return FALSE;

	val = val % 16;
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
	UINT32 val = 0;
	UINT16 src[MAX_TEST_SIZE + 1] = WINPR_C_ARRAY_INIT;
	UINT16 dst[MAX_TEST_SIZE + 1] = WINPR_C_ARRAY_INIT;
	if (winpr_RAND(&val, sizeof(val)) < 0)
		return FALSE;
	if (winpr_RAND(src, sizeof(src)) < 0)
		return FALSE;

	val = val % 16;
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
	UINT32 val = 0;
	INT16 src[MAX_TEST_SIZE + 1] = WINPR_C_ARRAY_INIT;
	INT16 dst[MAX_TEST_SIZE + 1] = WINPR_C_ARRAY_INIT;
	if (winpr_RAND(src, sizeof(src)) < 0)
		return FALSE;
	if (winpr_RAND(&val, sizeof(val)) < 0)
		return FALSE;

	val = val % 16;
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
	UINT32 val = 0;
	UINT16 src[MAX_TEST_SIZE + 1] = WINPR_C_ARRAY_INIT;
	UINT16 dst[MAX_TEST_SIZE + 1] = WINPR_C_ARRAY_INIT;
	if (winpr_RAND(&val, sizeof(val)) < 0)
		return FALSE;
	if (winpr_RAND(src, sizeof(src)) < 0)
		return FALSE;

	val = val % 16;
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

	if (!test_lShift_16s_inplace_func())
		return 1;

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
