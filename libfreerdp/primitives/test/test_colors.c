/* test_colors.c
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

static const int RGB_TRIAL_ITERATIONS = 1000;
static const int YCBCR_TRIAL_ITERATIONS = 1000;
static const float TEST_TIME = 4.0;

extern pstatus_t general_RGBToRGB_16s8u_P3AC4R(const INT16 *pSrc[3], 
	int srcStep, BYTE *pDst, int dstStep, const prim_size_t *roi);
extern pstatus_t sse2_RGBToRGB_16s8u_P3AC4R(const INT16 *pSrc[3], 
	int srcStep, BYTE *pDst, int dstStep, const prim_size_t *roi);
extern pstatus_t general_yCbCrToRGB_16s16s_P3P3(const INT16 *pSrc[3],
	int srcStep, INT16 *pDst[3], int dstStep, const prim_size_t *roi);
extern pstatus_t sse2_yCbCrToRGB_16s16s_P3P3(const INT16 *pSrc[3],
	int srcStep, INT16 *pDst[3], int dstStep, const prim_size_t *roi);
extern pstatus_t neon_yCbCrToRGB_16s16s_P3P3(const INT16 *pSrc[3],
	int srcStep, INT16 *pDst[3], int dstStep, const prim_size_t *roi);

/* ------------------------------------------------------------------------- */
int test_RGBToRGB_16s8u_P3AC4R_func(void)
{
	INT16 ALIGN(r[4096]), ALIGN(g[4096]), ALIGN(b[4096]);
	UINT32 ALIGN(out1[4096]);
#ifdef WITH_SSE2
	UINT32 ALIGN(out2[4096]);
#endif
	int i;
	int failed = 0;
	char testStr[256];
	INT16 *ptrs[3];
	prim_size_t roi = { 64, 64 };

	testStr[0] = '\0';
	get_random_data(r, sizeof(r));
	get_random_data(g, sizeof(g));
	get_random_data(b, sizeof(b));
	/* clear upper bytes */
	for (i=0; i<4096; ++i)
	{
		r[i] &= 0x00FFU;
		g[i] &= 0x00FFU;
		b[i] &= 0x00FFU;
	}

	ptrs[0] = r;
	ptrs[1] = g;
	ptrs[2] = b;

	general_RGBToRGB_16s8u_P3AC4R((const INT16 **) ptrs, 64*2,
		(BYTE *) out1, 64*4, &roi);
#ifdef WITH_SSE2
	if (IsProcessorFeaturePresent(PF_SSE2_INSTRUCTIONS_AVAILABLE))
	{
		strcat(testStr, " SSE2");
		sse2_RGBToRGB_16s8u_P3AC4R((const INT16 **) ptrs, 64*2,
			(BYTE *) out2, 64*4, &roi);
		for (i=0; i<4096; ++i)
		{
			if (out1[i] != out2[i])
			{
				printf("RGBToRGB-SSE FAIL: out1[%d]=0x%08x out2[%d]=0x%08x\n",
					i, out1[i], i, out2[i]);
				failed = 1;
			}
		}
	}
#endif /* i386 */
	if (!failed) printf("All RGBToRGB_16s8u_P3AC4R tests passed (%s).\n", testStr);
	return (failed > 0) ? FAILURE : SUCCESS;
}

/* ------------------------------------------------------------------------- */
static const prim_size_t roi64x64 = { 64, 64 };
STD_SPEED_TEST(
	rgb_to_argb_speed, INT16*, UINT32, dst=dst,
	TRUE, general_RGBToRGB_16s8u_P3AC4R(
		(const INT16 **) src1, 64*2, (BYTE *) dst, 64*4, &roi64x64),
#ifdef WITH_SSE2
	TRUE, sse2_RGBToRGB_16s8u_P3AC4R(
		(const INT16 **) src1, 64*2, (BYTE *) dst, 64*4, &roi64x64),
		PF_SSE2_INSTRUCTIONS_AVAILABLE, FALSE,
#else
	FALSE, PRIM_NOP, 0, FALSE,
#endif
	FALSE, dst=dst);

int test_RGBToRGB_16s8u_P3AC4R_speed(void)
{
	INT16 ALIGN(r[4096]), ALIGN(g[4096]), ALIGN(b[4096]);
	UINT32 ALIGN(dst[4096]);
	int i;
	INT16 *ptrs[3];
	int size_array[] = { 64 };

	get_random_data(r, sizeof(r));
	get_random_data(g, sizeof(g));
	get_random_data(b, sizeof(b));
	/* clear upper bytes */
	for (i=0; i<4096; ++i)
	{
		r[i] &= 0x00FFU;
		g[i] &= 0x00FFU;
		b[i] &= 0x00FFU;
	}

	ptrs[0] = r;
	ptrs[1] = g;
	ptrs[2] = b;

	rgb_to_argb_speed("RGBToARGB", "aligned", 
		(const INT16 **) ptrs, NULL, 0, dst,
		size_array, 1, RGB_TRIAL_ITERATIONS, TEST_TIME);
	return SUCCESS;
}

/* ========================================================================= */
int test_yCbCrToRGB_16s16s_P3P3_func(void)
{
	INT16 ALIGN(y[4096]), ALIGN(cb[4096]), ALIGN(cr[4096]);
	INT16 ALIGN(r1[4096]), ALIGN(g1[4096]), ALIGN(b1[4096]);
	INT16 ALIGN(r2[4096]), ALIGN(g2[4096]), ALIGN(b2[4096]);
	int i;
	int failed = 0;
	char testStr[256];
	const INT16 *in[3];
	INT16 *out1[3];
	INT16 *out2[3];
	prim_size_t roi = { 64, 64 };

	testStr[0] = '\0';
	get_random_data(y, sizeof(y));
	get_random_data(cb, sizeof(cb));
	get_random_data(cr, sizeof(cr));
	/* Normalize to 11.5 fixed radix */
	for (i=0; i<4096; ++i)
	{
		y[i]  &= 0x1FE0U;
		cb[i] &= 0x1FE0U;
		cr[i] &= 0x1FE0U;
	}
	memset(r1, 0, sizeof(r1));
	memset(g1, 0, sizeof(g1));
	memset(b1, 0, sizeof(b1));
	memset(r2, 0, sizeof(r2));
	memset(g2, 0, sizeof(g2));
	memset(b2, 0, sizeof(b2));

	in[0] = y;
	in[1] = cb;
	in[2] = cr;
	out1[0] = r1;
	out1[1] = g1;
	out1[2] = b1;
	out2[0] = r2;
	out2[1] = g2;
	out2[2] = b2;

	general_yCbCrToRGB_16s16s_P3P3(in, 64*2, out1, 64*2, &roi);
#ifdef WITH_SSE2
	if (IsProcessorFeaturePresent(PF_SSE2_INSTRUCTIONS_AVAILABLE))
	{
		strcat(testStr, " SSE2");
		sse2_yCbCrToRGB_16s16s_P3P3(in, 64*2, out2, 64*2, &roi);
		for (i=0; i<4096; ++i)
		{
			if ((ABS(r1[i]-r2[i]) > 1)
					|| (ABS(g1[i]-g2[i]) > 1)
					|| (ABS(b1[i]-b2[i]) > 1)) {
				printf("YCbCrToRGB-SSE FAIL[%d]: %d,%d,%d vs %d,%d,%d\n", i,
					r1[i],g1[i],b1[i], r2[i],g2[i],b2[i]);
				failed = 1;
			}
		}
	}
#endif /* i386 */
	if (!failed) printf("All yCbCrToRGB_16s16s_P3P3 tests passed (%s).\n", testStr);
	return (failed > 0) ? FAILURE : SUCCESS;
}

/* ------------------------------------------------------------------------- */
STD_SPEED_TEST(
	ycbcr_to_rgb_speed, INT16*, INT16*, dst=dst,
	TRUE, general_yCbCrToRGB_16s16s_P3P3(src1, 64*2, dst, 64*2, &roi64x64),
#ifdef WITH_SSE2
	TRUE, sse2_yCbCrToRGB_16s16s_P3P3(src1, 64*2, dst, 64*2, &roi64x64),
		PF_SSE2_INSTRUCTIONS_AVAILABLE, FALSE,
#elif defined(WITH_NEON)
	TRUE, neon_yCbCrToRGB_16s16s_P3P3(src1, 64*2, dst, 64*2, &roi64x64),
		PF_ARM_NEON_INSTRUCTIONS_AVAILABLE, FALSE,
#else
	FALSE, PRIM_NOP, 0, FALSE,
#endif
	FALSE, dst=dst);

int test_yCbCrToRGB_16s16s_P3P3_speed(void)
{
	INT16 ALIGN(y[4096]), ALIGN(cb[4096]), ALIGN(cr[4096]);
	INT16 ALIGN(r[4096]), ALIGN(g[4096]), ALIGN(b[4096]);
	int i;
	const INT16 *input[3];
	INT16 *output[3];
	int size_array[] = { 64 };

	get_random_data(y, sizeof(y));
	get_random_data(cb, sizeof(cb));
	get_random_data(cr, sizeof(cr));
	/* Normalize to 11.5 fixed radix */
	for (i=0; i<4096; ++i)
	{
		y[i]  &= 0x1FE0U;
		cb[i] &= 0x1FE0U;
		cr[i] &= 0x1FE0U;
	}

	input[0] = y;
	input[1] = cb;
	input[2] = cr;
	output[0] = r;
	output[1] = g;
	output[2] = b;

	ycbcr_to_rgb_speed("yCbCrToRGB", "aligned", input, NULL, NULL, output,
		size_array, 1, YCBCR_TRIAL_ITERATIONS, TEST_TIME);
	return SUCCESS;
}
