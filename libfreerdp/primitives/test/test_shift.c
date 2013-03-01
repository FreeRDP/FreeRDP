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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/sysinfo.h>
#include "prim_test.h"

#define FUNC_TEST_SIZE 65536
static const int SHIFT_PRETEST_ITERATIONS = 50000;
static const float TEST_TIME = 1.0;

extern pstatus_t general_lShiftC_16s(
	const INT16 *pSrc, int val, INT16 *pDst, int len);
extern pstatus_t general_rShiftC_16s(
	const INT16 *pSrc, int val, INT16 *pDst, int len);
extern pstatus_t general_shiftC_16s(
	const INT16 *pSrc, int val, INT16 *pDst, int len);
extern pstatus_t general_lShiftC_16u(
	const UINT16 *pSrc, int val, UINT16 *pDst, int len);
extern pstatus_t general_rShiftC_16u(
	const UINT16 *pSrc, int val, UINT16 *pDst, int len);
extern pstatus_t general_shiftC_16u(
	const UINT16 *pSrc, int val, UINT16 *pDst, int len);
extern pstatus_t sse2_lShiftC_16s(
	const INT16 *pSrc, int val, INT16 *pDst, int len);
extern pstatus_t sse2_rShiftC_16s(
	const INT16 *pSrc, int val, INT16 *pDst, int len);
extern pstatus_t sse2_shiftC_16s(
	const INT16 *pSrc, int val, INT16 *pDst, int len);
extern pstatus_t sse2_lShiftC_16u(
	const UINT16 *pSrc, int val, UINT16 *pDst, int len);
extern pstatus_t sse2_rShiftC_16u(
	const UINT16 *pSrc, int val, UINT16 *pDst, int len);
extern pstatus_t sse2_shiftC_16u(
	const UINT16 *pSrc, int val, UINT16 *pDst, int len);

#ifdef WITH_SSE2
#define SHIFT_TEST_FUNC(_name_, _type_, _str_, _f1_, _f2_) \
int _name_(void) \
{ \
	_type_ ALIGN(src[FUNC_TEST_SIZE+3]), \
		ALIGN(d1[FUNC_TEST_SIZE+3]), ALIGN(d2[FUNC_TEST_SIZE+3]); \
	int failed = 0; \
	int i; \
	char testStr[256]; \
	testStr[0] = '\0'; \
	get_random_data(src, sizeof(src)); \
	_f1_(src+1, 3, d1+1, FUNC_TEST_SIZE); \
	if (IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE)) \
	{ \
		strcat(testStr, " SSE3"); \
		/* Aligned */ \
		_f2_(src+1, 3, d2+1, FUNC_TEST_SIZE); \
		for (i=1; i<=FUNC_TEST_SIZE; ++i) \
		{ \
			if (d1[i] != d2[i]) \
			{  \
				printf("%s-SSE-aligned FAIL[%d]: 0x%x>>3=0x%x, got 0x%x\n", \
					_str_, i, src[i], d1[i], d2[i]);  \
				++failed; \
			} \
		} \
		/* Unaligned */ \
		_f2_(src+1, 3, d2+2, FUNC_TEST_SIZE); \
		for (i=1; i<=FUNC_TEST_SIZE; ++i) \
		{ \
			if (d1[i] != d2[i+1]) \
			{  \
				printf("%s-SSE-unaligned FAIL[%d]: 0x%x>>3=0x%x, got 0x%x\n", \
					_str_, i, src[i], d1[i], d2[i+1]);  \
				++failed; \
			} \
		} \
	} \
	if (!failed) printf("All %s tests passed (%s).\n", _str_, testStr); \
	return (failed > 0) ? FAILURE : SUCCESS; \
}
#else
#define SHIFT_TEST_FUNC(_name_, _type_, _str_, _f1_, _f2_) \
int _name_(void) \
{ \
	return SUCCESS; \
}
#endif /* i386 */

SHIFT_TEST_FUNC(test_lShift_16s_func, INT16, "lshift_16s", general_lShiftC_16s,
    sse2_lShiftC_16s)
SHIFT_TEST_FUNC(test_lShift_16u_func, UINT16, "lshift_16u", general_lShiftC_16u,
    sse2_lShiftC_16u)
SHIFT_TEST_FUNC(test_rShift_16s_func, INT16, "rshift_16s", general_rShiftC_16s,
    sse2_rShiftC_16s)
SHIFT_TEST_FUNC(test_rShift_16u_func, UINT16, "rshift_16u", general_rShiftC_16u,
    sse2_rShiftC_16u)

/* ========================================================================= */
STD_SPEED_TEST(speed_lShift_16s, INT16, INT16, dst=dst,
    TRUE, general_lShiftC_16s(src1, constant, dst, size),
#ifdef WITH_SSE2
	TRUE, sse2_lShiftC_16s(src1, constant, dst, size), PF_SSE2_INSTRUCTIONS_AVAILABLE, FALSE,
#else
	FALSE, PRIM_NOP, 0, FALSE,
#endif
	TRUE, ippsLShiftC_16s(src1, constant, dst, size));
STD_SPEED_TEST(speed_lShift_16u, UINT16, UINT16, dst=dst,
    TRUE, general_lShiftC_16u(src1, constant, dst, size),
#ifdef WITH_SSE2
	TRUE, sse2_lShiftC_16u(src1, constant, dst, size), PF_SSE2_INSTRUCTIONS_AVAILABLE, FALSE,
#else
	FALSE, PRIM_NOP, 0, FALSE,
#endif
	TRUE, ippsLShiftC_16u(src1, constant, dst, size));
STD_SPEED_TEST(speed_rShift_16s, INT16, INT16, dst=dst,
    TRUE, general_rShiftC_16s(src1, constant, dst, size),
#ifdef WITH_SSE2
	TRUE, sse2_rShiftC_16s(src1, constant, dst, size), PF_SSE2_INSTRUCTIONS_AVAILABLE, FALSE,
#else
	FALSE, PRIM_NOP, 0, FALSE,
#endif
	TRUE, ippsRShiftC_16s(src1, constant, dst, size));
STD_SPEED_TEST(speed_rShift_16u, UINT16, UINT16, dst=dst,
    TRUE, general_rShiftC_16u(src1, constant, dst, size),
#ifdef WITH_SSE2
	TRUE, sse2_rShiftC_16u(src1, constant, dst, size), PF_SSE2_INSTRUCTIONS_AVAILABLE, FALSE,
#else
	FALSE, PRIM_NOP, 0, FALSE,
#endif
	TRUE, ippsRShiftC_16u(src1, constant, dst, size));

/* ------------------------------------------------------------------------- */
int test_lShift_16s_speed(void)
{
	INT16 ALIGN(src[MAX_TEST_SIZE+1]), ALIGN(dst[MAX_TEST_SIZE+1]);
	get_random_data(src, sizeof(src));
	speed_lShift_16s("lShift_16s", "aligned", src, NULL, 3, dst,
		test_sizes, NUM_TEST_SIZES, SHIFT_PRETEST_ITERATIONS, TEST_TIME);
	speed_lShift_16s("lShift_16s", "unaligned", src+1, NULL, 3, dst,
		test_sizes, NUM_TEST_SIZES, SHIFT_PRETEST_ITERATIONS, TEST_TIME);
	return SUCCESS;
}

/* ------------------------------------------------------------------------- */
int test_lShift_16u_speed(void)
{
	UINT16 ALIGN(src[MAX_TEST_SIZE+1]), ALIGN(dst[MAX_TEST_SIZE+1]);
	get_random_data(src, sizeof(src));
	speed_lShift_16u("lShift_16u", "aligned", src, NULL, 3, dst,
		test_sizes, NUM_TEST_SIZES, SHIFT_PRETEST_ITERATIONS, TEST_TIME);
	speed_lShift_16u("lShift_16u", "unaligned", src+1, NULL, 3, dst,
		test_sizes, NUM_TEST_SIZES, SHIFT_PRETEST_ITERATIONS, TEST_TIME);
	return SUCCESS;
}

/* ------------------------------------------------------------------------- */
int test_rShift_16s_speed(void)
{
	INT16 ALIGN(src[MAX_TEST_SIZE+1]), ALIGN(dst[MAX_TEST_SIZE+1]);
	get_random_data(src, sizeof(src));
	speed_rShift_16s("rShift_16s", "aligned", src, NULL, 3, dst,
		test_sizes, NUM_TEST_SIZES, SHIFT_PRETEST_ITERATIONS, TEST_TIME);
	speed_rShift_16s("rShift_16s", "unaligned", src+1, NULL, 3, dst,
		test_sizes, NUM_TEST_SIZES, SHIFT_PRETEST_ITERATIONS, TEST_TIME);
	return SUCCESS;
}

/* ------------------------------------------------------------------------- */
int test_rShift_16u_speed(void)
{
	UINT16 ALIGN(src[MAX_TEST_SIZE+1]), ALIGN(dst[MAX_TEST_SIZE+1]);
	get_random_data(src, sizeof(src));
	speed_rShift_16u("rShift_16u", "aligned", src, NULL, 3, dst,
		test_sizes, NUM_TEST_SIZES, SHIFT_PRETEST_ITERATIONS, TEST_TIME);
	speed_rShift_16u("rShift_16u", "unaligned", src+1, NULL, 3, dst,
		test_sizes, NUM_TEST_SIZES, SHIFT_PRETEST_ITERATIONS, TEST_TIME);
	return SUCCESS;
}
