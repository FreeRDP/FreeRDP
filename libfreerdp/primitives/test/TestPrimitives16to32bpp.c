/* test_colors.c
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

#include <assert.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/sysinfo.h>
#include "prim_test.h"

static const int RGB_TRIAL_ITERATIONS = 1000;
static const float TEST_TIME = 4.0;

extern BOOL g_TestPrimitivesPerformance;

extern pstatus_t general_RGB565ToARGB_16u32u_C3C4(
	const UINT16* pSrc, INT32 srcStep,
	UINT32* pDst, INT32 dstStep,
	UINT32 width, UINT32 height,
	BOOL alpha, BOOL invert);
extern pstatus_t sse3_RGB565ToARGB_16u32u_C3C4(
	const UINT16* pSrc, INT32 srcStep,
	UINT32* pDst, INT32 dstStep,
	UINT32 width, UINT32 height,
	BOOL alpha, BOOL invert);

/* ------------------------------------------------------------------------- */
static BOOL try_16To32(
	const UINT16 *data16,
	int sOffset,
	int dOffset,
	int width, int height)
{
	BOOL success;
	const char *sAligned = "sAlign";
	const char *sUnaligned = "s!Align";
	const char *dAligned = "dAlign";
	const char *dUnaligned = "d!Align";
	const char *sAlignStr, *dAlignStr;
	const UINT16 *src;
	UINT32 ALIGN(outNN1[4096+3]), ALIGN(outAN1[4096+3]),
		   ALIGN(outNI1[4096+3]), ALIGN(outAI1[4096+3]);
#ifdef WITH_SSE2
	UINT32 ALIGN(outNN2[4096+3]), ALIGN(outAN2[4096+3]),
		   ALIGN(outNI2[4096+3]), ALIGN(outAI2[4096+3]);
#endif

	assert(sOffset < 4);
	assert(dOffset < 4);
	assert(width*height <= 4096);

	success = TRUE;
	src = data16 + sOffset;
	sAlignStr = (sOffset == 0) ? sAligned : sUnaligned;
	dAlignStr = (dOffset == 0) ? dAligned : dUnaligned;

	general_RGB565ToARGB_16u32u_C3C4(src, width*sizeof(UINT16),
		outNN1+dOffset, width*sizeof(UINT32), width, height, FALSE, FALSE);
	general_RGB565ToARGB_16u32u_C3C4(src, width*sizeof(UINT16),
		outAN1+dOffset, width*sizeof(UINT32), width, height, TRUE, FALSE);
	general_RGB565ToARGB_16u32u_C3C4(src, width*sizeof(UINT16),
		outNI1+dOffset, width*sizeof(UINT32), width, height, FALSE, TRUE);
	general_RGB565ToARGB_16u32u_C3C4(src, width*sizeof(UINT16),
		outAI1+dOffset, width*sizeof(UINT32), width, height, TRUE, TRUE);

#ifdef WITH_SSE2
	printf("  Testing 16-to-32bpp SSE3 version (%s, %s, %dx%d)\n",
		sAlignStr, dAlignStr, width, height);
	if (IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE))
	{
		int i;

		sse3_RGB565ToARGB_16u32u_C3C4(src, width*sizeof(UINT16),
			outNN2+dOffset, width*sizeof(UINT32), width, height, FALSE, FALSE);
		sse3_RGB565ToARGB_16u32u_C3C4(src, width*sizeof(UINT16),
			outAN2+dOffset, width*sizeof(UINT32), width, height, TRUE, FALSE);
		sse3_RGB565ToARGB_16u32u_C3C4(src, width*sizeof(UINT16),
			outNI2+dOffset, width*sizeof(UINT32), width, height, FALSE, TRUE);
		sse3_RGB565ToARGB_16u32u_C3C4(src, width*sizeof(UINT16),
			outAI2+dOffset, width*sizeof(UINT32), width, height, TRUE, TRUE);
		for (i=0; i<width * height; i++)
		{
			int s = i + sOffset;
			int d = i + dOffset;
			if (outNN1[d] != outNN2[d])
			{
				printf("16To32bpp-SSE FAIL (%s, %s, !alpha, !invert)"
					" 0x%04x -> 0x%08x rather than 0x%08x\n",
					sAlignStr, dAlignStr, data16[s], outNN2[d], outNN1[d]);
				success = FALSE;
			}
			if (outAN1[d] != outAN2[d])
			{
				printf("16To32bpp-SSE FAIL (%s, %s, alpha, !invert)"
					" 0x%04x -> 0x%08x rather than 0x%08x\n",
					sAlignStr, dAlignStr, data16[s], outAN2[d], outAN1[d]);
				success = FALSE;
			}
			if (outNI1[d] != outNI2[d])
			{
				printf("16To32bpp-SSE FAIL (%s, %s, !alpha, invert)"
					" 0x%04x -> 0x%08x rather than 0x%08x\n",
					sAlignStr, dAlignStr, data16[s], outNI2[d], outNI1[d]);
				success = FALSE;
			}
			if (outAI1[d] != outAI2[d])
			{
				printf("16To32bpp-SSE FAIL (%s, %s, alpha, invert)"
					" 0x%04x -> 0x%08x rather than 0x%08x\n",
					sAlignStr, dAlignStr, data16[s], outAI2[d], outNI1[d]);
				success = FALSE;
			}
		}
	}
#endif /* WITH_SSE2 */

	return success;
}


/* ------------------------------------------------------------------------- */
int test_RGB565ToARGB_16u32u_C3C4_func(void)
{
	UINT16 ALIGN(data16[4096+3]);
	BOOL success;

	success = TRUE;
	get_random_data(data16, sizeof(data16));

	/* Source aligned, dest aligned, 64x64 */
	success &= try_16To32(data16, 0, 0, 64, 64);
	/* Source !aligned, dest aligned, 64x64 */
	success &= try_16To32(data16, 1, 0, 64, 64);
	/* Source aligned, dest !aligned, 64x64 */
	success &= try_16To32(data16, 0, 1, 64, 64);
	/* Source !aligned, dest !aligned, 64x64 */
	success &= try_16To32(data16, 1, 1, 64, 64);
	/* Odd size */
	success &= try_16To32(data16, 0, 0, 17, 53);

	if (success) printf("All RGB565ToARGB_16u32u_C3C4 tests passed.\n");
	return success ? SUCCESS : FAILURE;
}

/* ------------------------------------------------------------------------- */
STD_SPEED_TEST(
	test16to32_speed, UINT16, UINT32, PRIM_NOP,
	TRUE, general_RGB565ToARGB_16u32u_C3C4(
		(const UINT16 *) src1, 64*2, (UINT32 *) dst, 64*4,
		64,64, TRUE, TRUE),
#ifdef WITH_SSE2
	TRUE, sse3_RGB565ToARGB_16u32u_C3C4(
		(const UINT16 *) src1, 64*2, (UINT32 *) dst, 64*4,
		64,64, TRUE, TRUE),
		PF_SSE3_INSTRUCTIONS_AVAILABLE, FALSE,
#else
	FALSE, PRIM_NOP, 0, FALSE,
#endif
	FALSE, PRIM_NOP);

/* ------------------------------------------------------------------------- */
int test_RGB565ToARGB_16u32u_C3C4_speed(void)
{
	UINT16 ALIGN(src[4096]);
	UINT32 ALIGN(dst[4096]);
	int size_array[] = { 64 };

	get_random_data(src, sizeof(src));

	test16to32_speed("16-to-32bpp", "aligned",
		(const UINT16 *) src, 0, 0, (UINT32 *) dst,
		size_array, 1, RGB_TRIAL_ITERATIONS, TEST_TIME);
	return SUCCESS;
}

int TestPrimitives16to32bpp(int argc, char* argv[])
{
	int status;

	status = test_RGB565ToARGB_16u32u_C3C4_func();

	if (status != SUCCESS)
		return 1;

	if (g_TestPrimitivesPerformance)
	{
		status = test_RGB565ToARGB_16u32u_C3C4_speed();

		if (status != SUCCESS)
			return 1;
	}

	return 0;
}
