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

/* ------------------------------------------------------------------------- */
static BOOL test_RGBToRGB_16s8u_P3AC4R_func(void)
{
	INT16 ALIGN(r[4096]), ALIGN(g[4096]), ALIGN(b[4096]);
	UINT32 ALIGN(out1[4096]);
	UINT32 ALIGN(out2[4096]);
	int i;
	BOOL failed = FALSE;
	INT16* ptrs[3];
	prim_size_t roi = { 64, 64 };

	winpr_RAND((BYTE*)r, sizeof(r));
	winpr_RAND((BYTE*)g, sizeof(g));
	winpr_RAND((BYTE*)b, sizeof(b));

	/* clear upper bytes */
	for (i = 0; i < 4096; ++i)
	{
		r[i] &= 0x00FFU;
		g[i] &= 0x00FFU;
		b[i] &= 0x00FFU;
	}

	ptrs[0] = r;
	ptrs[1] = g;
	ptrs[2] = b;
	if (generic->RGBToRGB_16s8u_P3AC4R((const INT16**) ptrs, 64 * 2,
				      (BYTE*) out1, 64 * 4, PIXEL_FORMAT_RGBA32,
				       &roi) != PRIMITIVES_SUCCESS)
		return FALSE;

	if (optimized->RGBToRGB_16s8u_P3AC4R((const INT16**) ptrs, 64 * 2,
					 (BYTE*) out2, 64 * 4, PIXEL_FORMAT_RGBA32,
					  &roi) != PRIMITIVES_SUCCESS)
		return FALSE;

	for (i = 0; i < 4096; ++i)
	{
		if (out1[i] != out2[i])
		{
			printf("RGBToRGB-SSE FAIL: out1[%d]=0x%08"PRIx32" out2[%d]=0x%08"PRIx32"\n",
			       i, out1[i], i, out2[i]);
			failed = TRUE;
		}
	}

	return !failed;
}

/* ------------------------------------------------------------------------- */
static BOOL test_RGBToRGB_16s8u_P3AC4R_speed(void)
{
	const prim_size_t roi64x64 = { 64, 64 };
	INT16 ALIGN(r[4096+1]), ALIGN(g[4096+1]), ALIGN(b[4096+1]);
	UINT32 ALIGN(dst[4096+1]);
	int i;
	INT16* ptrs[3];

	winpr_RAND((BYTE*)r, sizeof(r));
	winpr_RAND((BYTE*)g, sizeof(g));
	winpr_RAND((BYTE*)b, sizeof(b));

	/* clear upper bytes */
	for (i = 0; i < 4096; ++i)
	{
		r[i] &= 0x00FFU;
		g[i] &= 0x00FFU;
		b[i] &= 0x00FFU;
	}

	ptrs[0] = r+1;
	ptrs[1] = g+1;
	ptrs[2] = b+1;

	if (!speed_test("RGBToRGB_16s8u_P3AC4R", "aligned", g_Iterations,
			(speed_test_fkt)generic->RGBToRGB_16s8u_P3AC4R,
			(speed_test_fkt)optimized->RGBToRGB_16s8u_P3AC4R,
			(const INT16**) ptrs, 64 * 2, (BYTE*) dst, 64 * 4, &roi64x64))
		return FALSE;

	if (!speed_test("RGBToRGB_16s8u_P3AC4R", "unaligned", g_Iterations,
			(speed_test_fkt)generic->RGBToRGB_16s8u_P3AC4R,
			(speed_test_fkt)optimized->RGBToRGB_16s8u_P3AC4R,
			(const INT16**) ptrs, 64 * 2, ((BYTE*) dst)+1, 64 * 4, &roi64x64))
		return FALSE;

	return TRUE;
}

/* ========================================================================= */
static BOOL test_yCbCrToRGB_16s16s_P3P3_func(void)
{
	pstatus_t status;
	INT16 ALIGN(y[4096]), ALIGN(cb[4096]), ALIGN(cr[4096]);
	INT16 ALIGN(r1[4096]), ALIGN(g1[4096]), ALIGN(b1[4096]);
	INT16 ALIGN(r2[4096]), ALIGN(g2[4096]), ALIGN(b2[4096]);
	int i;
	const INT16* in[3];
	INT16* out1[3];
	INT16* out2[3];
	prim_size_t roi = { 64, 64 };

	winpr_RAND((BYTE*)y, sizeof(y));
	winpr_RAND((BYTE*)cb, sizeof(cb));
	winpr_RAND((BYTE*)cr, sizeof(cr));

	/* Normalize to 11.5 fixed radix */
	for (i = 0; i < 4096; ++i)
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

	status = generic->yCbCrToRGB_16s16s_P3P3(in, 64 * 2, out1, 64 * 2, &roi);
	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	status = optimized->yCbCrToRGB_16s16s_P3P3(in, 64 * 2, out2, 64 * 2, &roi);
	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	for (i = 0; i < 4096; ++i)
	{
		if ((ABS(r1[i] - r2[i]) > 1)
		    || (ABS(g1[i] - g2[i]) > 1)
		    || (ABS(b1[i] - b2[i]) > 1))
		{
			printf("YCbCrToRGB-SSE FAIL[%d]: %"PRId16",%"PRId16",%"PRId16" vs %"PRId16",%"PRId16",%"PRId16"\n", i,
			       r1[i], g1[i], b1[i], r2[i], g2[i], b2[i]);
			return FALSE;
		}
	}

	return TRUE;
}

/* ------------------------------------------------------------------------- */
static int test_yCbCrToRGB_16s16s_P3P3_speed(void)
{
	prim_size_t roi = { 64, 64 };
	INT16 ALIGN(y[4096]), ALIGN(cb[4096]), ALIGN(cr[4096]);
	INT16 ALIGN(r[4096]), ALIGN(g[4096]), ALIGN(b[4096]);
	int i;
	const INT16* input[3];
	INT16* output[3];

	winpr_RAND((BYTE*)y, sizeof(y));
	winpr_RAND((BYTE*)cb, sizeof(cb));
	winpr_RAND((BYTE*)cr, sizeof(cr));

	/* Normalize to 11.5 fixed radix */
	for (i = 0; i < 4096; ++i)
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

	if (!speed_test("yCbCrToRGB_16s16s_P3P3", "aligned", g_Iterations,
			(speed_test_fkt)generic->yCbCrToRGB_16s16s_P3P3,
			(speed_test_fkt)optimized->yCbCrToRGB_16s16s_P3P3,
			input, 64 * 2, output, 64 * 2, &roi))
		return FALSE;

	return TRUE;
}

int TestPrimitivesColors(int argc, char* argv[])
{
	prim_test_setup(FALSE);

	if (!test_RGBToRGB_16s8u_P3AC4R_func())
		return 1;

	if (g_TestPrimitivesPerformance)
	{
		if (!test_RGBToRGB_16s8u_P3AC4R_speed())
			return 1;
	}

	if (!test_yCbCrToRGB_16s16s_P3P3_func())
		return 1;

	if (g_TestPrimitivesPerformance)
	{
		if (!test_yCbCrToRGB_16s16s_P3P3_speed())
			return 1;
	}

	return 0;
}
