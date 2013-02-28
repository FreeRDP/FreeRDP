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

extern pstatus_t general_andC_32u(const UINT32 *pSrc, UINT32 val,
	UINT32 *pDst, int len);
extern pstatus_t sse3_andC_32u(const UINT32 *pSrc, UINT32 val,
	UINT32 *pDst, int len);
extern pstatus_t general_orC_32u(const UINT32 *pSrc, UINT32 val,
	UINT32 *pDst, int len);
extern pstatus_t sse3_orC_32u(const UINT32 *pSrc, UINT32 val,
	UINT32 *pDst, int len);

#define VALUE (0xA5A5A5A5U)

/* ========================================================================= */
int test_and_32u_func(void)
{
	UINT32 ALIGN(src[FUNC_TEST_SIZE+3]), ALIGN(dst[FUNC_TEST_SIZE+3]);
	int failed = 0;
	int i;
	char testStr[256];

	testStr[0] = '\0';
	get_random_data(src, sizeof(src));
	general_andC_32u(src+1, VALUE, dst+1, FUNC_TEST_SIZE);
	strcat(testStr, " general");
	for (i=1; i<=FUNC_TEST_SIZE; ++i)
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
		sse3_andC_32u(src+1, VALUE, dst+1, FUNC_TEST_SIZE);
		for (i=1; i<=FUNC_TEST_SIZE; ++i)
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
		sse3_andC_32u(src+1, VALUE, dst+2, FUNC_TEST_SIZE);
		for (i=1; i<=FUNC_TEST_SIZE; ++i)
		{
			if (dst[i+1] != (src[i] & VALUE))
			{ 
				printf("AND-SSE-unaligned FAIL[%d] 0x%08x&0x%08x=0x%08x, got 0x%08x\n", 
					i, src[i], VALUE, src[i] & VALUE, dst[i+1]); 
				++failed;
			}
		}
	}
#endif /* i386 */
	if (!failed) printf("All and_32u tests passed (%s).\n", testStr);
	return (failed > 0) ? FAILURE : SUCCESS;
}

/* ------------------------------------------------------------------------- */
STD_SPEED_TEST(andC_32u_speed_test, UINT32, UINT32, dst=dst,
	TRUE, general_andC_32u(src1, constant, dst, size),
#ifdef WITH_SSE2
	TRUE, sse3_andC_32u(src1, constant, dst, size), PF_SSE3_INSTRUCTIONS_AVAILABLE, FALSE,
#else
	FALSE, PRIM_NOP, 0, FALSE,
#endif
	TRUE, ippsAndC_32u(src1, constant, dst, size))

int test_and_32u_speed(void)
{
	UINT32 ALIGN(src[MAX_TEST_SIZE+3]), ALIGN(dst[MAX_TEST_SIZE+3]);
	get_random_data(src, sizeof(src));
	andC_32u_speed_test("and32u", "aligned", src, NULL, VALUE, dst,
		test_sizes, NUM_TEST_SIZES, ANDOR_PRETEST_ITERATIONS, TEST_TIME);
	andC_32u_speed_test("and32u", "unaligned", src+1, NULL, VALUE, dst,
		test_sizes, NUM_TEST_SIZES, ANDOR_PRETEST_ITERATIONS, TEST_TIME);
	return SUCCESS;
}

/* ========================================================================= */
int test_or_32u_func(void)
{
	UINT32 ALIGN(src[FUNC_TEST_SIZE+3]), ALIGN(dst[FUNC_TEST_SIZE+3]);
	int failed = 0;
	int i;
	char testStr[256];

	testStr[0] = '\0';
	get_random_data(src, sizeof(src));
	general_orC_32u(src+1, VALUE, dst+1, FUNC_TEST_SIZE);
	strcat(testStr, " general");
	for (i=1; i<=FUNC_TEST_SIZE; ++i)
	{
		if (dst[i] != (src[i] | VALUE))
		{ 
			printf("OR-general general FAIL[%d] 0x%08x&0x%08x=0x%08x, got 0x%08x\n", 
				i, src[i], VALUE, src[i] | VALUE, dst[i]); 
		++failed;
		}
	}
#ifdef WITH_SSE2
	if(IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE))
	{
		strcat(testStr, " SSE3");
		/* Aligned */
		memset(dst, 0, sizeof(dst));
		sse3_orC_32u(src+1, VALUE, dst+1, FUNC_TEST_SIZE);
		for (i=1; i<FUNC_TEST_SIZE; ++i)
		{
			if (dst[i] != (src[i] | VALUE))
			{ 
				printf("OR-SSE-aligned FAIL[%d] 0x%08x&0x%08x=0x%08x, got 0x%08x\n", 
					i, src[i], VALUE, src[i] | VALUE, dst[i]); 
				++failed;
			}
		}
		/* Unaligned */
		memset(dst, 0, sizeof(dst));
		sse3_orC_32u(src+1, VALUE, dst+2, FUNC_TEST_SIZE);
		for (i=1; i<FUNC_TEST_SIZE; ++i)
		{
			if (dst[i+1] != (src[i] | VALUE))
			{ 
				printf("OR-SSE-unaligned FAIL[%d] 0x%08x&0x%08x=0x%08x, got 0x%08x\n", 
					i, src[i], VALUE, src[i] | VALUE, dst[i+1]); 
				++failed;
			}
		}
	}
#endif /* i386 */
	if (!failed) printf("All or_32u tests passed (%s).\n", testStr);
	return (failed > 0) ? FAILURE : SUCCESS;
}
    	
/* ------------------------------------------------------------------------- */
STD_SPEED_TEST(orC_32u_speed_test, UINT32, UINT32, dst=dst,
	TRUE, general_orC_32u(src1, constant, dst, size),
#ifdef WITH_SSE2
	TRUE, sse3_orC_32u(src1, constant, dst, size), PF_SSE3_INSTRUCTIONS_AVAILABLE, FALSE,
#else
	FALSE, PRIM_NOP, 0, FALSE,
#endif
	TRUE, ippsOrC_32u(src1, constant, dst, size))

int test_or_32u_speed(void)
{
	UINT32 ALIGN(src[MAX_TEST_SIZE+3]), ALIGN(dst[MAX_TEST_SIZE+3]);
	get_random_data(src, sizeof(src));
	orC_32u_speed_test("or32u", "aligned", src, NULL, VALUE, dst,
		test_sizes, NUM_TEST_SIZES, ANDOR_PRETEST_ITERATIONS, TEST_TIME);
	orC_32u_speed_test("or32u", "unaligned", src+1, NULL, VALUE, dst,
		test_sizes, NUM_TEST_SIZES, ANDOR_PRETEST_ITERATIONS, TEST_TIME);
	return SUCCESS;
}
