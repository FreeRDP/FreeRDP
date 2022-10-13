/* test_sign.c
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

#define TEST_BUFFER_SIZE 65535

/* ------------------------------------------------------------------------- */
static BOOL test_sign16s_func(void)
{
	pstatus_t status;
	INT16 ALIGN(src[TEST_BUFFER_SIZE + 16]) = { 0 };
	INT16 ALIGN(d1[TEST_BUFFER_SIZE + 16]) = { 0 };
	INT16 ALIGN(d2[TEST_BUFFER_SIZE + 16]) = { 0 };
	winpr_RAND((BYTE*)src, sizeof(src));
	status = generic->sign_16s(src + 1, d1 + 1, TEST_BUFFER_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->sign_16s(src + 1, d2 + 1, TEST_BUFFER_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	if (memcmp(d1, d2, sizeof(d1)) != 0)
		return FALSE;

	status = generic->sign_16s(src + 1, d1 + 2, TEST_BUFFER_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->sign_16s(src + 1, d2 + 2, TEST_BUFFER_SIZE);

	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	if (memcmp(d1, d2, sizeof(d1)) != 0)
		return FALSE;

	return TRUE;
}

static int test_sign16s_speed(void)
{
	INT16 ALIGN(src[MAX_TEST_SIZE + 3]) = { 0 };
	INT16 ALIGN(dst[MAX_TEST_SIZE + 3]) = { 0 };
	winpr_RAND((BYTE*)src, sizeof(src));

	if (!speed_test("sign16s", "aligned", g_Iterations, (speed_test_fkt)generic->sign_16s,
	                (speed_test_fkt)optimized->sign_16s, src + 1, dst + 1, MAX_TEST_SIZE))
		return FALSE;

	if (!speed_test("sign16s", "unaligned", g_Iterations, (speed_test_fkt)generic->sign_16s,
	                (speed_test_fkt)optimized->sign_16s, src + 1, dst + 2, MAX_TEST_SIZE))
		return FALSE;

	return TRUE;
}

int TestPrimitivesSign(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	prim_test_setup(FALSE);

	if (!test_sign16s_func())
		return 1;

	if (g_TestPrimitivesPerformance)
	{
		if (!test_sign16s_speed())
			return 1;
	}

	return 0;
}
