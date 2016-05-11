/* test_YCoCg.c
 * vi:ts=4 sw=4
 *
 * (c) Copyright 2014 Hewlett-Packard Development Company, L.P.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/sysinfo.h>
#include "prim_test.h"

static const int YCOCG_TRIAL_ITERATIONS = 20000;
static const float TEST_TIME = 4.0;

extern BOOL g_TestPrimitivesPerformance;

extern pstatus_t general_YCoCgToRGB_8u_AC4R(const BYTE *pSrc, INT32 srcStep,
	BYTE *pDst, INT32 dstStep, UINT32 width, UINT32 height,
	UINT8 shift, BOOL withAlpha, BOOL invert);
extern pstatus_t ssse3_YCoCgRToRGB_8u_AC4R(const BYTE *pSrc, INT32 srcStep,
	BYTE *pDst, INT32 dstStep, UINT32 width, UINT32 height,
	UINT8 shift, BOOL withAlpha, BOOL invert);

/* ------------------------------------------------------------------------- */
int test_YCoCgRToRGB_8u_AC4R_func(void)
{
#ifdef WITH_SSE2
	int i;
	INT32 ALIGN(out_sse[4098]), ALIGN(out_sse_inv[4098]);
#endif
	INT32 ALIGN(in[4098]);
	INT32 ALIGN(out_c[4098]), ALIGN(out_c_inv[4098]);
	char testStr[256];
	BOOL failed = FALSE;

	testStr[0] = '\0';
	get_random_data(in, sizeof(in));

	general_YCoCgToRGB_8u_AC4R((const BYTE *) (in+1), 63*4,
		(BYTE *) out_c, 63*4, 63, 61, 2, TRUE, FALSE);
	general_YCoCgToRGB_8u_AC4R((const BYTE *) (in+1), 63*4,
		(BYTE *) out_c_inv, 63*4, 63, 61, 2, TRUE, TRUE);
#ifdef WITH_SSE2
	if (IsProcessorFeaturePresentEx(PF_EX_SSSE3))
	{
		strcat(testStr, " SSSE3");
		ssse3_YCoCgRToRGB_8u_AC4R((const BYTE *) (in+1), 63*4,
			(BYTE *) out_sse, 63*4, 63, 61, 2, TRUE, FALSE);

		for (i=0; i<63*61; ++i)
		{
			if (out_c[i] != out_sse[i]) {
				printf("YCoCgRToRGB-SSE FAIL[%d]: 0x%08x -> C 0x%08x vs SSE 0x%08x\n", i,
					in[i+1], out_c[i], out_sse[i]);
				failed = TRUE;
			}
		}
		ssse3_YCoCgRToRGB_8u_AC4R((const BYTE *) (in+1), 63*4,
			(BYTE *) out_sse_inv, 63*4, 63, 61, 2, TRUE, TRUE);
		for (i=0; i<63*61; ++i)
		{
			if (out_c_inv[i] != out_sse_inv[i]) {
				printf("YCoCgRToRGB-SSE inverted FAIL[%d]: 0x%08x -> C 0x%08x vs SSE 0x%08x\n", i,
					in[i+1], out_c_inv[i], out_sse_inv[i]);
				failed = TRUE;
			}
		}
	}
#endif /* i386 */
	if (!failed) printf("All YCoCgRToRGB_8u_AC4R tests passed (%s).\n", testStr);
	return (failed > 0) ? FAILURE : SUCCESS;
}

/* ------------------------------------------------------------------------- */
STD_SPEED_TEST(
	ycocg_to_rgb_speed, BYTE, BYTE, PRIM_NOP,
	TRUE, general_YCoCgToRGB_8u_AC4R(src1, 64*4, dst, 64*4, 64, 64, 2, FALSE, FALSE),
#ifdef WITH_SSE2
	TRUE, ssse3_YCoCgRToRGB_8u_AC4R(src1, 64*4, dst, 64*4, 64, 64, 2, FALSE, FALSE),
		PF_EX_SSSE3, TRUE,
#else
	FALSE, PRIM_NOP, 0, FALSE,
#endif
	FALSE, PRIM_NOP);

int test_YCoCgRToRGB_8u_AC4R_speed(void)
{
	INT32 ALIGN(in[4096]);
	INT32 ALIGN(out[4096]);
	int size_array[] = { 64 };

	get_random_data(in, sizeof(in));

	ycocg_to_rgb_speed("YCoCgToRGB", "aligned", (const BYTE *) in,
		0, 0, (BYTE *) out,
		size_array, 1, YCOCG_TRIAL_ITERATIONS, TEST_TIME);
	return SUCCESS;
}

int TestPrimitivesYCoCg(int argc, char* argv[])
{
	int status;

	status = test_YCoCgRToRGB_8u_AC4R_func();

	if (status != SUCCESS)
		return 1;

	if (g_TestPrimitivesPerformance)
	{
		status = test_YCoCgRToRGB_8u_AC4R_speed();

		if (status != SUCCESS)
			return 1;
	}

	return 0;
}
