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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/sysinfo.h>
#include "prim_test.h"

static const int SIGN_PRETEST_ITERATIONS = 100000;
static const float TEST_TIME = 1.0;

extern pstatus_t general_sign_16s(const INT16 *pSrc, INT16 *pDst, int len);
#ifdef WITH_SSE2
extern pstatus_t ssse3_sign_16s(const INT16 *pSrc, INT16 *pDst, int len);
#endif

/* ------------------------------------------------------------------------- */
int test_sign16s_func(void)
{
	INT16 ALIGN(src[65535]), ALIGN(d1[65535]);
#ifdef WITH_SSE2
	INT16 ALIGN(d2[65535]);
	int i;
#endif
	int failed = 0;
	char testStr[256];

	/* Test when we can reach 16-byte alignment */
	testStr[0] = '\0';
	get_random_data(src, sizeof(src));
	general_sign_16s(src+1, d1+1, 65535);
#ifdef WITH_SSE2
	if (IsProcessorFeaturePresentEx(PF_EX_SSSE3))
	{
		strcat(testStr, " SSSE3");
		ssse3_sign_16s(src+1, d2+1, 65535);
		for (i=1; i<65535; ++i)
		{
			if (d1[i] != d2[i])
			{ 
				printf("SIGN16s-SSE-aligned FAIL[%d] of %d: want %d, got %d\n", 
					i, src[i], d1[i], d2[i]); 
				++failed;
			}
		}
	}
#endif /* i386 */

	/* Test when we cannot reach 16-byte alignment */
	get_random_data(src, sizeof(src));
	general_sign_16s(src+1, d1+2, 65535);
#ifdef WITH_SSE2
	if (IsProcessorFeaturePresentEx(PF_EX_SSSE3))
	{
		ssse3_sign_16s(src+1, d2+2, 65535);
		for (i=2; i<65535; ++i)
		{
			if (d1[i] != d2[i])
			{ 
				printf("SIGN16s-SSE-unaligned FAIL[%d] of %d: want %d, got %d\n", 
					i, src[i-1], d1[i], d2[i]); 
				++failed;
			}
		}
	}
#endif /* i386 */
	if (!failed) printf("All sign16s tests passed (%s).\n", testStr);
	return (failed > 0) ? FAILURE : SUCCESS;
}

/* ------------------------------------------------------------------------- */
STD_SPEED_TEST(sign16s_speed_test, INT16, INT16, dst=dst,
	TRUE, general_sign_16s(src1, dst, size),
#ifdef WITH_SSE2
	TRUE, ssse3_sign_16s(src1, dst, size), PF_EX_SSSE3, TRUE,
#else
	FALSE, PRIM_NOP, 0, FALSE,
#endif
	FALSE, dst=dst);

int test_sign16s_speed(void)
{
	INT16 ALIGN(src[MAX_TEST_SIZE+3]), ALIGN(dst[MAX_TEST_SIZE+3]);
	get_random_data(src, sizeof(src));
	sign16s_speed_test("sign16s", "aligned", src, NULL, 0, dst,
		test_sizes, NUM_TEST_SIZES, SIGN_PRETEST_ITERATIONS, TEST_TIME);
	sign16s_speed_test("sign16s", "unaligned", src+1, NULL, 0, dst,
		test_sizes, NUM_TEST_SIZES, SIGN_PRETEST_ITERATIONS, TEST_TIME);
	return SUCCESS;
}
