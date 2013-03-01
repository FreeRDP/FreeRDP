/* test_set.c
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

static const int MEMSET8_PRETEST_ITERATIONS = 100000000;
static const int MEMSET32_PRETEST_ITERATIONS = 40000000;
static const float TEST_TIME = 1.0;

extern pstatus_t general_set_8u(BYTE val, BYTE *pDst, int len);
extern pstatus_t sse2_set_8u(BYTE val, BYTE *pDst, int len);
extern pstatus_t general_set_32s(INT32 val, INT32 *pDst, int len);
extern pstatus_t sse2_set_32s(INT32 val, INT32 *pDst, int len);
extern pstatus_t general_set_32u(UINT32 val, UINT32 *pDst, int len);
extern pstatus_t sse2_set_32u(UINT32 val, UINT32 *pDst, int len);
extern pstatus_t ipp_wrapper_set_32u(UINT32 val, UINT32 *pDst, int len);

static const int set_sizes[] = { 1, 4, 16, 32, 64, 256, 1024, 4096 };
#define NUM_SET_SIZES (sizeof(set_sizes)/sizeof(int))

/* ------------------------------------------------------------------------- */
int test_set8u_func(void)
{
#if defined(WITH_SSE2) || defined(WITH_IPP)
	BYTE ALIGN(dest[48]);
	int off;
#endif
	int failed = 0;
	char testStr[256];
	testStr[0] = '\0';

#ifdef WITH_SSE2
	/* Test SSE under various alignments */
	if (IsProcessorFeaturePresent(PF_SSE2_INSTRUCTIONS_AVAILABLE))
	{
		strcat(testStr, " SSE2");
		for (off=0; off<16; ++off)
		{
			int len;
			for (len=1; len<48-off; ++len)
			{
				int i;
				memset(dest, 0, sizeof(dest));
				sse2_set_8u(0xa5, dest+off, len);
				for (i=0; i<len; ++i)
				{
					if (dest[off+i] != 0xa5)
					{
						printf("SET8U-SSE FAILED: off=%d len=%d dest[%d]=0x%02x\n",
							off, len, i+off, dest[i+off]);
						failed=1;
					}
				}
			}
		}
	}
#endif /* i386 */

#ifdef WITH_IPP
	/* Test IPP under various alignments */
	strcat(testStr, " IPP");
	for (off=0; off<16; ++off)
	{
		int len;
		for (len=1; len<48-off; ++len)
		{
			int i;
			memset(dest, 0, sizeof(dest));
			ippsSet_8u(0xa5, dest+off, len);
			for (i=0; i<len; ++i)
			{
				if (dest[off+i] != 0xa5)
				{
					printf("SET8U-IPP FAILED: off=%d len=%d dest[%d]=0x%02x\n",
						off, len, i+off, dest[i+off]);
					failed=1;
				}
			}
		}
	}
#endif /* WITH_IPP */

	if (!failed) printf("All set8u tests passed (%s).\n", testStr);
	return (failed > 0) ? FAILURE : SUCCESS;
}

/* ------------------------------------------------------------------------- */
STD_SPEED_TEST(set8u_speed_test, BYTE, BYTE, dst=dst,
	TRUE, memset(dst, constant, size),
	FALSE, PRIM_NOP, 0, FALSE,
	TRUE, ippsSet_8u(constant, dst, size));

int test_set8u_speed(void)
{
	BYTE ALIGN(dst[MAX_TEST_SIZE]);
	set8u_speed_test("set8u", "aligned", NULL, NULL, 0xA5, dst,
		set_sizes, NUM_SET_SIZES, MEMSET8_PRETEST_ITERATIONS, TEST_TIME);
	return SUCCESS;
}

/* ------------------------------------------------------------------------- */
int test_set32s_func(void)
{
#if defined(WITH_SSE2) || defined(WITH_IPP)
	INT32 ALIGN(dest[512]);
	int off;
#endif
	int failed = 0;
	char testStr[256];
	testStr[0] = '\0';

#ifdef WITH_SSE2
	/* Test SSE under various alignments */
	if (IsProcessorFeaturePresent(PF_SSE2_INSTRUCTIONS_AVAILABLE))
	{
		strcat(testStr, " SSE2");
		for (off=0; off<16; ++off) {
			int len;
			for (len=1; len<512-off; ++len)
			{
				int i;
				memset(dest, 0, sizeof(dest));
				sse2_set_32s(0xdeadbeef, dest+off, len);
				for (i=0; i<len; ++i)
				{
					if (dest[off+i] != 0xdeadbeef)
					{
						printf("set32s-SSE FAIL: off=%d len=%d dest[%d]=0x%08x\n",
							off, len, i+off, dest[i+off]);
						failed=1;
					}
				}
			}
		}
	}
#endif /* i386 */

#ifdef WITH_IPP
	strcat(testStr, " IPP");
	for (off=0; off<16; ++off) {
		int len;
		for (len=1; len<512-off; ++len)
		{
			int i;
			memset(dest, 0, sizeof(dest));
			ippsSet_32s(0xdeadbeef, dest+off, len);
			for (i=0; i<len; ++i)
			{
				if (dest[off+i] != 0xdeadbeef)
				{
					printf("set32s-IPP FAIL: off=%d len=%d dest[%d]=0x%08x\n",
						off, len, i+off, dest[i+off]);
					failed=1;
				}
			}
		}
	}
#endif /* WITH_IPP */

	if (!failed) printf("All set32s tests passed (%s).\n", testStr);
	return (failed > 0) ? FAILURE : SUCCESS;
}

/* ------------------------------------------------------------------------- */
int test_set32u_func(void)
{
#if defined(WITH_SSE2) || defined(WITH_IPP)
	UINT32 ALIGN(dest[512]);
	int off;
#endif
	int failed = 0;
	char testStr[256];
	testStr[0] = '\0';

#ifdef WITH_SSE2
	/* Test SSE under various alignments */
	if (IsProcessorFeaturePresent(PF_SSE2_INSTRUCTIONS_AVAILABLE))
	{
		strcat(testStr, " SSE2");
		for (off=0; off<16; ++off) {
			int len;
			for (len=1; len<512-off; ++len)
			{
				int i;
				memset(dest, 0, sizeof(dest));
				sse2_set_32u(0xdeadbeefU, dest+off, len);
				for (i=0; i<len; ++i)
				{
					if (dest[off+i] != 0xdeadbeefU)
					{
						printf("set32u-SSE FAIL: off=%d len=%d dest[%d]=0x%08x\n",
							off, len, i+off, dest[i+off]);
						failed=1;
					}
				}
			}
		}
	}
#endif /* i386 */

#ifdef WITH_IPP
	strcat(testStr, " IPP");
	for (off=0; off<16; ++off) {
		int len;
		for (len=1; len<512-off; ++len)
		{
			int i;
			memset(dest, 0, sizeof(dest));
			ipp_wrapper_set_32u(0xdeadbeefU, dest+off, len);
			for (i=0; i<len; ++i)
			{
				if (dest[off+i] != 0xdeadbeefU)
				{
					printf("set32u-IPP FAIL: off=%d len=%d dest[%d]=0x%08x\n",
						off, len, i+off, dest[i+off]);
					failed=1;
				}
			}
		}
	}
#endif /* WITH_IPP */

	if (!failed) printf("All set32u tests passed (%s).\n", testStr);
	return (failed > 0) ? FAILURE : SUCCESS;
}

/* ------------------------------------------------------------------------- */
static inline void memset32u_naive(
	UINT32 val,
	UINT32 *dst,
	size_t count)
{
	while (count--) *dst++ = val;
}

/* ------------------------------------------------------------------------- */
STD_SPEED_TEST(set32u_speed_test, UINT32, UINT32, dst=dst,
	TRUE, memset32u_naive(constant, dst, size),
#ifdef WITH_SSE2
	TRUE, sse2_set_32u(constant, dst, size), PF_SSE2_INSTRUCTIONS_AVAILABLE, FALSE,
#else
	FALSE, PRIM_NOP, 0, FALSE,
#endif
	TRUE, ipp_wrapper_set_32u(constant, dst, size));

int test_set32u_speed(void)
{
	UINT32 ALIGN(dst[MAX_TEST_SIZE+1]);
	set32u_speed_test("set32u", "aligned", NULL, NULL, 0xdeadbeef, dst,
		set_sizes, NUM_SET_SIZES, MEMSET32_PRETEST_ITERATIONS, TEST_TIME);
#if 0
	/* Not really necessary; should be almost as fast. */
	set32u_speed_test("set32u", "unaligned", NULL, NULL, dst+1,
		set_sizes, NUM_SET_SIZES, MEMSET32_PRETEST_ITERATIONS, TEST_TIME);
#endif
	return SUCCESS;
}

/* ------------------------------------------------------------------------- */
static inline void memset32s_naive(
	INT32 val,
	INT32 *dst,
	size_t count)
{
	while (count--) *dst++ = val;
}

/* ------------------------------------------------------------------------- */
STD_SPEED_TEST(set32s_speed_test, INT32, INT32, dst=dst,
	TRUE, memset32s_naive(constant, dst, size),
#ifdef WITH_SSE2
	TRUE, sse2_set_32s(constant, dst, size), PF_SSE2_INSTRUCTIONS_AVAILABLE, FALSE,
#else
	FALSE, PRIM_NOP, 0, FALSE,
#endif
	TRUE, ippsSet_32s(constant, dst, size));

int test_set32s_speed(void)
{
	INT32 ALIGN(dst[MAX_TEST_SIZE+1]);
	set32s_speed_test("set32s", "aligned", NULL, NULL, 0xdeadbeef, dst,
		set_sizes, NUM_SET_SIZES, MEMSET32_PRETEST_ITERATIONS, TEST_TIME);
#if 0
	/* Not really necessary; should be almost as fast. */
	set32s_speed_test("set32s", "unaligned", NULL, NULL, dst+1,
		set_sizes, NUM_SET_SIZES, MEMSET32_PRETEST_ITERATIONS, TEST_TIME);
#endif
	return SUCCESS;
}
