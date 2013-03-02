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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/sysinfo.h>
#include "prim_test.h"

#define FUNC_TEST_SIZE	65536
static const int ADD16S_PRETEST_ITERATIONS = 300000*64;
static const int TEST_TIME = 2.0;  // seconds

extern pstatus_t general_add_16s(
	const INT16 *pSrc1, const INT16 *pSrc2, INT16 *pDst, int len);
extern pstatus_t sse3_add_16s(
	const INT16 *pSrc1, const INT16 *pSrc2, INT16 *pDst, int len);

/* ========================================================================= */
int test_add16s_func(void)
{
	INT16 ALIGN(src1[FUNC_TEST_SIZE+3]), ALIGN(src2[FUNC_TEST_SIZE+3]), 
		ALIGN(d1[FUNC_TEST_SIZE+3]), ALIGN(d2[FUNC_TEST_SIZE+3]);
	int failed = 0;
#if defined(WITH_SSE2) || defined(WITH_IPP)
	int i;
#endif
	char testStr[256];

	testStr[0] = '\0';
	get_random_data(src1, sizeof(src1));
	get_random_data(src2, sizeof(src2));
	memset(d1, 0, sizeof(d1));
	memset(d2, 0, sizeof(d2));
	general_add_16s(src1+1, src2+1, d1+1, FUNC_TEST_SIZE);
#ifdef WITH_SSE2
	if(IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE))
	{
		strcat(testStr, " SSE3");
		/* Aligned */
		sse3_add_16s(src1+1, src2+1, d2+1, FUNC_TEST_SIZE);
		for (i=1; i<FUNC_TEST_SIZE; ++i)
		{
			if (d1[i] != d2[i])
			{ 
				printf("ADD16S-SSE-aligned FAIL[%d] %d+%d=%d, got %d\n", 
					i, src1[i], src2[i], d1[i], d2[i]); 
				++failed;
			}
		}
		/* Unaligned */
		sse3_add_16s(src1+1, src2+1, d2+2, FUNC_TEST_SIZE);
		for (i=1; i<FUNC_TEST_SIZE; ++i)
		{
			if (d1[i] != d2[i+1])
			{ 
				printf("ADD16S-SSE-unaligned FAIL[%d] %d+%d=%d, got %d\n", 
					i, src1[i], src2[i], d1[i], d2[i+1]); 
				++failed;
			}
		}
	}
#endif
#ifdef WITH_IPP
	strcat(testStr, " IPP");
	ippsAdd_16s(src1+1, src2+1, d2+1, FUNC_TEST_SIZE);
	for (i=1; i<FUNC_TEST_SIZE; ++i)
	{
		if (d1[i] != d2[i])
		{ 
			printf("ADD16S-IPP FAIL[%d] %d+%d=%d, got %d\n", 
				i, src1[i], src2[i], d1[i], d2[i]); 
			++failed;
		}
	}
#endif /* WITH_IPP */
	if (!failed) printf("All add16s tests passed (%s).\n", testStr);
	return (failed > 0) ? FAILURE : SUCCESS;
}
 
/* ------------------------------------------------------------------------- */
STD_SPEED_TEST(add16s_speed_test, INT16, INT16, dst=dst,
	TRUE, general_add_16s(src1, src2, dst, size),
#ifdef WITH_SSE2
	TRUE, sse3_add_16s(src1, src2, dst, size), PF_SSE3_INSTRUCTIONS_AVAILABLE, FALSE,
#else
	FALSE, PRIM_NOP, 0, FALSE,
#endif
	TRUE, ippsAdd_16s(src1, src2, dst, size));

int test_add16s_speed(void)
{
	INT16 ALIGN(src1[MAX_TEST_SIZE+3]), ALIGN(src2[MAX_TEST_SIZE+3]),
		ALIGN(dst[MAX_TEST_SIZE+3]);
	get_random_data(src1, sizeof(src1));
	get_random_data(src2, sizeof(src2));
	add16s_speed_test("add16s", "aligned", src1, src2, 0, dst,
		test_sizes, NUM_TEST_SIZES, ADD16S_PRETEST_ITERATIONS, TEST_TIME);
	add16s_speed_test("add16s", "unaligned", src1+1, src2+2, 0, dst,
		test_sizes, NUM_TEST_SIZES, ADD16S_PRETEST_ITERATIONS, TEST_TIME);
	return SUCCESS;
}
