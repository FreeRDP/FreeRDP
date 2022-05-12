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
	pstatus_t status;

	INT16 ALIGN(src1[FUNC_TEST_SIZE + 3]), ALIGN(src2[FUNC_TEST_SIZE + 3]),
	    ALIGN(d1[FUNC_TEST_SIZE + 3]), ALIGN(d2[FUNC_TEST_SIZE + 3]);

	char testStr[256];
	testStr[0] = '\0';
	winpr_RAND((BYTE*)src1, sizeof(src1));
	winpr_RAND((BYTE*)src2, sizeof(src2));
	memset(d1, 0, sizeof(d1));
	memset(d2, 0, sizeof(d2));
	status = generic->add_16s(src1 + 1, src2 + 1, d1 + 1, FUNC_TEST_SIZE);
	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	/* Unaligned */
	status = optimized->add_16s(src1 + 1, src2 + 1, d2 + 2, FUNC_TEST_SIZE);
	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	return TRUE;
}

/* ------------------------------------------------------------------------- */
static BOOL test_add16s_speed(void)
{
	BYTE ALIGN(src1[MAX_TEST_SIZE + 3]), ALIGN(src2[MAX_TEST_SIZE + 3]),
	    ALIGN(dst[MAX_TEST_SIZE + 3]);

	if (!g_TestPrimitivesPerformance)
		return TRUE;

	winpr_RAND(src1, sizeof(src1));
	winpr_RAND(src2, sizeof(src2));

	if (!speed_test("add16s", "aligned", g_Iterations, (speed_test_fkt)generic->add_16s,
	                (speed_test_fkt)optimized->add_16s, src1, src2, dst, FUNC_TEST_SIZE))
		return FALSE;

	return TRUE;
}

int TestPrimitivesAdd(int argc, char* argv[])
{

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	prim_test_setup(FALSE);
	if (!test_add16s_func())
		return -1;

	if (g_TestPrimitivesPerformance)
	{
		if (!test_add16s_speed())
			return -1;
	}

	return 0;
}
