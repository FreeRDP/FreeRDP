/* test_andor.c
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

#define FUNC_TEST_SIZE	65536
static const int ANDOR_PRETEST_ITERATIONS = 100000;
static const int TEST_TIME = 2.0;  // seconds

#define VALUE (0xA5A5A5A5U)

/* ========================================================================= */
static BOOL test_and_32u_func(void)
{
	UINT32 ALIGN(src[FUNC_TEST_SIZE + 3]), ALIGN(dst[FUNC_TEST_SIZE + 3]);
	int failed = 0;
	int i;
	char testStr[256];
	testStr[0] = '\0';
	winpr_RAND(src, sizeof(src));
	generic->andC_32u(src + 1, VALUE, dst + 1, FUNC_TEST_SIZE);
	strcat(testStr, " general");

	for (i = 1; i <= FUNC_TEST_SIZE; ++i)
	{
		if (dst[i] != (src[i] & VALUE))
		{
			printf("AND-general FAIL[%d] 0x%08x&0x%08x=0x%08x, got 0x%08x\n",
			       i, src[i], VALUE, src[i] & VALUE, dst[i]);
			++failed;
		}
	}

#ifdef WITH_SSE2

	if (IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE))
	{
		strcat(testStr, " SSE3");
		/* Aligned */
		memset(dst, 0, sizeof(dst));
		sse3_andC_32u(src + 1, VALUE, dst + 1, FUNC_TEST_SIZE);

		for (i = 1; i <= FUNC_TEST_SIZE; ++i)
		{
			if (dst[i] != (src[i] & VALUE))
			{
				printf("AND-SSE-aligned FAIL[%d] 0x%08x&0x%08x=0x%08x, got 0x%08x\n",
				       i, src[i], VALUE, src[i] & VALUE, dst[i]);
				++failed;
			}
		}

		/* Unaligned */
		memset(dst, 0, sizeof(dst));
		sse3_andC_32u(src + 1, VALUE, dst + 2, FUNC_TEST_SIZE);

		for (i = 1; i <= FUNC_TEST_SIZE; ++i)
		{
			if (dst[i + 1] != (src[i] & VALUE))
			{
				printf("AND-SSE-unaligned FAIL[%d] 0x%08x&0x%08x=0x%08x, got 0x%08x\n",
				       i, src[i], VALUE, src[i] & VALUE, dst[i + 1]);
				++failed;
			}
		}
	}

#endif /* i386 */

	if (!failed) printf("All and_32u tests passed (%s).\n", testStr);

	return (failed > 0) ? FAILURE : SUCCESS;
}

/* ------------------------------------------------------------------------- */
static BOOL test_and_32u_speed(void)
{
	UINT32 ALIGN(src[MAX_TEST_SIZE + 3]), ALIGN(dst[MAX_TEST_SIZE + 3]);
	winpr_RAND(src, sizeof(src));
	andC_32u_speed_test("and32u", "aligned", src, NULL, VALUE, dst,
			    test_sizes, NUM_TEST_SIZES, ANDOR_PRETEST_ITERATIONS, TEST_TIME);
	andC_32u_speed_test("and32u", "unaligned", src + 1, NULL, VALUE, dst,
			    test_sizes, NUM_TEST_SIZES, ANDOR_PRETEST_ITERATIONS, TEST_TIME);
	return SUCCESS;
}

/* ========================================================================= */
static BOOL check(const UINT32* src, const UINT32* dst, UINT32 size, UINT32 value)
{
	UINT32 i;
	UINT32 failed = 0;

	for (i = 1; i <= size; ++i)
	{
		if (dst[i] != (src[i] | value))
		{
			printf("OR-general general FAIL[%d] 0x%08x&0x%08x=0x%08x, got 0x%08x\n",
			       i, src[i], value, src[i] | value, dst[i]);
			++failed;
		}
	}

	return TRUE;
}

static BOOL test_or_32u_func(void)
{
	pstatus_t status;
	UINT32 ALIGN(src[FUNC_TEST_SIZE + 3]), ALIGN(dst[FUNC_TEST_SIZE + 3]);
	char testStr[256];
	testStr[0] = '\0';
	winpr_RAND((BYTE*)src, sizeof(src));

	status = generic->orC_32u(src + 1, VALUE, dst + 1, FUNC_TEST_SIZE);
	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	if (!check(src + 1, dst + 1, FUNC_TEST_SIZE, VALUE))
		return FALSE;

	status = optimized->orC_32u(src + 1, VALUE, dst + 1, FUNC_TEST_SIZE);
	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	if (!check(src + 1, dst + 1, FUNC_TEST_SIZE, VALUE))
		return FALSE;

	return TRUE;
}

/* ------------------------------------------------------------------------- */
static BOOL test_or_32u_speed(void)
{
	UINT32 ALIGN(src[FUNC_TEST_SIZE + 3]), ALIGN(dst[FUNC_TEST_SIZE + 3]);
	char testStr[256];
	testStr[0] = '\0';
	winpr_RAND((BYTE*)src, sizeof(src));

	if (!speed_test("add16s", "aligned", g_Iterations,
			generic->orC_32u, optimized->orC_32u,
			src + 1, VALUE, dst + 1, FUNC_TEST_SIZE))
		return FALSE;

	return TRUE;
}

int TestPrimitivesAndOr(int argc, char* argv[])
{
	prim_test_setup(FALSE);

	if (!test_and_32u_func())
		return -1;

	if (!test_and_32u_speed())
		return -1;

	if (!test_or_32u_func())
		return -1;

	if (!test_or_32u_speed())
		return -1;

	return 0;
}
