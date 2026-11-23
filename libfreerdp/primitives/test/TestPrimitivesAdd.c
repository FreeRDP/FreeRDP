/* test_add.c
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
/* ========================================================================= */
static BOOL test_add16s_func(void)
{
	pstatus_t status = 0;

	INT16 src1[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	INT16 src2[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	INT16 d1[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	INT16 d2[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;

	if (winpr_RAND(src1, sizeof(src1)) < 0)
		return FALSE;
	if (winpr_RAND(src2, sizeof(src2)) < 0)
		return FALSE;
	status = generic->add_16s(src1 + 1, src2 + 1, d1 + 1, FUNC_TEST_SIZE);
	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	/* Unaligned */
	status = optimized->add_16s(src1 + 1, src2 + 1, d2 + 2, FUNC_TEST_SIZE);
	return (status == PRIMITIVES_SUCCESS);
}

/* ------------------------------------------------------------------------- */
static BOOL test_add16s_inplace_func(void)
{
	const UINT32 lengths[] = { 1, 7, 15, 16, 17, 33, FUNC_TEST_SIZE };
	INT16 src1[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	INT16 src2[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	INT16 d1[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	INT16 d2[FUNC_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;

	for (size_t i = 0; i < ARRAYSIZE(lengths); i++)
	{
		/* Aligned and unaligned */
		for (size_t offset = 0; offset < 2; offset++)
		{
			if (winpr_RAND(src1, sizeof(src1)) < 0)
				return FALSE;
			if (winpr_RAND(src2, sizeof(src2)) < 0)
				return FALSE;
			memcpy(d1, src1, sizeof(src1));
			memcpy(d2, src2, sizeof(src2));

			if (generic->add_16s_inplace(src1 + offset, src2 + offset, lengths[i]) !=
			    PRIMITIVES_SUCCESS)
				return FALSE;
			if (optimized->add_16s_inplace(d1 + offset, d2 + offset, lengths[i]) !=
			    PRIMITIVES_SUCCESS)
				return FALSE;
			if ((memcmp(src1, d1, sizeof(d1)) != 0) || (memcmp(src2, d2, sizeof(d2)) != 0))
			{
				(void)fprintf(stderr, "add_16s_inplace mismatch, len=%" PRIu32 "\n", lengths[i]);
				return FALSE;
			}
		}
	}

	return TRUE;
}

/* ------------------------------------------------------------------------- */
static BOOL test_add16s_speed(void)
{
	BYTE src1[MAX_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	BYTE src2[MAX_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;
	BYTE dst[MAX_TEST_SIZE + 3] = WINPR_C_ARRAY_INIT;

	if (!g_TestPrimitivesPerformance)
		return TRUE;

	if (winpr_RAND(src1, sizeof(src1)) < 0)
		return FALSE;
	if (winpr_RAND(src2, sizeof(src2)) < 0)
		return FALSE;

	return (speed_test("add16s", "aligned", g_Iterations, (speed_test_fkt)generic->add_16s,
	                   (speed_test_fkt)optimized->add_16s, src1, src2, dst, FUNC_TEST_SIZE));
}

int TestPrimitivesAdd(int argc, char* argv[])
{

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	prim_test_setup(FALSE);
	if (!test_add16s_func())
		return -1;

	if (!test_add16s_inplace_func())
		return -1;

	if (g_TestPrimitivesPerformance)
	{
		if (!test_add16s_speed())
			return -1;
	}

	return 0;
}
