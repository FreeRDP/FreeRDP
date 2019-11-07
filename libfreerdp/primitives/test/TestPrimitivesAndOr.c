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

#define FUNC_TEST_SIZE 65536

#define VALUE (0xA5A5A5A5U)

/* ========================================================================= */
static BOOL test_and_32u_impl(const char* name, __andC_32u_t fkt, const UINT32* src,
                              const UINT32 val, UINT32* dst, size_t size)
{
	size_t i;
	pstatus_t status = fkt(src, val, dst, size);
	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	for (i = 0; i < size; ++i)
	{
		if (dst[i] != (src[i] & val))
		{

			printf("AND %s FAIL[%" PRIuz "] 0x%08" PRIx32 "&0x%08" PRIx32 "=0x%08" PRIx32
			       ", got 0x%08" PRIx32 "\n",
			       name, i, src[i], val, (src[i] & val), dst[i]);

			return FALSE;
		}
	}

	return TRUE;
}

static BOOL test_and_32u_func(void)
{
	UINT32 ALIGN(src[FUNC_TEST_SIZE + 3]), ALIGN(dst[FUNC_TEST_SIZE + 3]);

	winpr_RAND((BYTE*)src, sizeof(src));

	if (!test_and_32u_impl("generic->andC_32u aligned", generic->andC_32u, src + 1, VALUE, dst + 1,
	                       FUNC_TEST_SIZE))
		return FALSE;
	if (!test_and_32u_impl("generic->andC_32u unaligned", generic->andC_32u, src + 1, VALUE,
	                       dst + 2, FUNC_TEST_SIZE))
		return FALSE;
	if (!test_and_32u_impl("optimized->andC_32u aligned", optimized->andC_32u, src + 1, VALUE,
	                       dst + 1, FUNC_TEST_SIZE))
		return FALSE;
	if (!test_and_32u_impl("optimized->andC_32u unaligned", optimized->andC_32u, src + 1, VALUE,
	                       dst + 2, FUNC_TEST_SIZE))
		return FALSE;

	return TRUE;
}

/* ------------------------------------------------------------------------- */
static BOOL test_and_32u_speed(void)
{
	UINT32 ALIGN(src[MAX_TEST_SIZE + 3]), ALIGN(dst[MAX_TEST_SIZE + 3]);

	winpr_RAND((BYTE*)src, sizeof(src));

	if (!speed_test("andC_32u", "aligned", g_Iterations, (speed_test_fkt)generic->andC_32u,
	                (speed_test_fkt)optimized->andC_32u, src + 1, VALUE, dst + 1, MAX_TEST_SIZE))
		return FALSE;
	if (!speed_test("andC_32u", "unaligned", g_Iterations, (speed_test_fkt)generic->andC_32u,
	                (speed_test_fkt)optimized->andC_32u, src + 1, VALUE, dst + 2, MAX_TEST_SIZE))
		return FALSE;

	return TRUE;
}

/* ========================================================================= */
static BOOL check(const UINT32* src, const UINT32* dst, UINT32 size, UINT32 value)
{
	UINT32 i;

	for (i = 0; i < size; ++i)
	{
		if (dst[i] != (src[i] | value))
		{
			printf("OR-general general FAIL[%" PRIu32 "] 0x%08" PRIx32 "&0x%08" PRIx32
			       "=0x%08" PRIx32 ", got 0x%08" PRIx32 "\n",
			       i, src[i], value, src[i] | value, dst[i]);
			return FALSE;
		}
	}

	return TRUE;
}

static BOOL test_or_32u_func(void)
{
	pstatus_t status;
	UINT32 ALIGN(src[FUNC_TEST_SIZE + 3]), ALIGN(dst[FUNC_TEST_SIZE + 3]);

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

	if (!speed_test("add16s", "aligned", g_Iterations, (speed_test_fkt)generic->orC_32u,
	                (speed_test_fkt)optimized->orC_32u, src + 1, VALUE, dst + 1, FUNC_TEST_SIZE))
		return FALSE;

	return TRUE;
}

int TestPrimitivesAndOr(int argc, char* argv[])
{
	prim_test_setup(FALSE);

	if (!test_and_32u_func())
		return -1;

	if (!test_or_32u_func())
		return -1;

	if (g_TestPrimitivesPerformance)
	{
		if (!test_and_32u_speed())
			return -1;
		if (!test_or_32u_speed())
			return -1;
	}

	return 0;
}
