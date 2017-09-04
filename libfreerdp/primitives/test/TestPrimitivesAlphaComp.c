/* test_alphaComp.c
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

static const int ALPHA_PRETEST_ITERATIONS = 5000000;
static const float TEST_TIME = 5.0;

static const int block_size[] = { 4, 64, 256 };
#define NUM_BLOCK_SIZES (sizeof(block_size)/sizeof(int))
#define MAX_BLOCK_SIZE 256
#define SIZE_SQUARED (MAX_BLOCK_SIZE*MAX_BLOCK_SIZE)

/* ========================================================================= */
#define ALF(_c_) (((_c_) & 0xFF000000U) >> 24)
#define RED(_c_) (((_c_) & 0x00FF0000U) >> 16)
#define GRN(_c_) (((_c_) & 0x0000FF00U) >> 8)
#define BLU(_c_) ((_c_) & 0x000000FFU)
#define TOLERANCE 1
static inline const UINT32* PIXEL(const BYTE* _addr_, UINT32 _bytes_, UINT32 _x_, UINT32 _y_)
{
	const BYTE* addr = _addr_ + _x_ * sizeof(UINT32) + _y_ * _bytes_;

	return (const UINT32*)addr;
}

#define SRC1_WIDTH 6
#define SRC1_HEIGHT 6
#define SRC2_WIDTH 7
#define SRC2_HEIGHT 7
#define DST_WIDTH 9
#define DST_HEIGHT 9
#define TEST_WIDTH 4
#define TEST_HEIGHT 5

/* ------------------------------------------------------------------------- */
static UINT32 alpha_add(
		UINT32 c1,
		UINT32 c2)
{
	UINT32 a1 = ALF(c1);
	UINT32 r1 = RED(c1);
	UINT32 g1 = GRN(c1);
	UINT32 b1 = BLU(c1);
	UINT32 a2 = ALF(c2);
	UINT32 r2 = RED(c2);
	UINT32 g2 = GRN(c2);
	UINT32 b2 = BLU(c2);
	UINT32 a3 = ((a1 * a1 + (255 - a1) * a2) / 255) & 0xff;
	UINT32 r3 = ((a1 * r1 + (255 - a1) * r2) / 255) & 0xff;
	UINT32 g3 = ((a1 * g1 + (255 - a1) * g2) / 255) & 0xff;
	UINT32 b3 = ((a1 * b1 + (255 - a1) * b2) / 255) & 0xff;
	return (a3 << 24) | (r3 << 16) | (g3 << 8) | b3;
}

/* ------------------------------------------------------------------------- */
static UINT32 colordist(
		UINT32 c1,
		UINT32 c2)
{
	int d, maxd = 0;
	d = ABS((INT32)(ALF(c1) - ALF(c2)));

	if (d > maxd) maxd = d;

	d = ABS((INT32)(RED(c1) - RED(c2)));

	if (d > maxd) maxd = d;

	d = ABS((INT32)(GRN(c1) - GRN(c2)));

	if (d > maxd) maxd = d;

	d = ABS((INT32)(BLU(c1) - BLU(c2)));

	if (d > maxd) maxd = d;

	return maxd;
}

/* ------------------------------------------------------------------------- */
static BOOL check(const BYTE* pSrc1,  UINT32 src1Step,
		  const BYTE* pSrc2,  UINT32 src2Step,
		  BYTE* pDst,  UINT32 dstStep,
		  UINT32 width,  UINT32 height)
{
	UINT32 x, y;
	for (y = 0; y < height; ++y)
	{
		for (x = 0; x < width; ++x)
		{
			UINT32 s1 = *PIXEL(pSrc1, src1Step, x, y);
			UINT32 s2 = *PIXEL(pSrc2, src2Step, x, y);
			UINT32 c0 = alpha_add(s1, s2);
			UINT32 c1 = *PIXEL(pDst, dstStep, x, y);

			if (colordist(c0, c1) > TOLERANCE)
			{
				printf("alphaComp-general: [%"PRIu32",%"PRIu32"] 0x%08"PRIx32"+0x%08"PRIx32"=0x%08"PRIx32", got 0x%08"PRIx32"\n",
				       x, y, s1, s2, c0, c1);
				return FALSE;
			}
		}
	}

	return TRUE;
}

static BOOL test_alphaComp_func(void)
{
	pstatus_t status;
	BYTE ALIGN(src1[SRC1_WIDTH * SRC1_HEIGHT * 4]);
	BYTE ALIGN(src2[SRC2_WIDTH * SRC2_HEIGHT * 4]);
	BYTE ALIGN(dst1[DST_WIDTH * DST_HEIGHT * 4]);
	UINT32* ptr;
	UINT32 i;

	winpr_RAND((BYTE*)src1, sizeof(src1));

	/* Special-case the first two values */
	src1[0] &= 0x00FFFFFFU;
	src1[1] |= 0xFF000000U;
	winpr_RAND((BYTE*)src2, sizeof(src2));

	/* Set the second operand to fully-opaque. */
	ptr = (UINT32*)src2;

	for (i = 0; i < sizeof(src2) / 4; ++i) *ptr++ |= 0xFF000000U;

	memset(dst1, 0, sizeof(dst1));

	status = generic->alphaComp_argb(src1, 4 * SRC1_WIDTH,
					 src2, 4 * SRC2_WIDTH,
					 dst1, 4 * DST_WIDTH, TEST_WIDTH, TEST_HEIGHT);
	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	if (!check(src1, 4 * SRC1_WIDTH,
		   src2, 4 * SRC2_WIDTH,
		   dst1, 4 * DST_WIDTH, TEST_WIDTH, TEST_HEIGHT))
		return FALSE;

	status = optimized->alphaComp_argb((const BYTE*) src1, 4 * SRC1_WIDTH,
					   (const BYTE*) src2, 4 * SRC2_WIDTH,
					   (BYTE*) dst1, 4 * DST_WIDTH, TEST_WIDTH, TEST_HEIGHT);
	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	if (!check(src1, 4 * SRC1_WIDTH,
		   src2, 4 * SRC2_WIDTH,
		   dst1, 4 * DST_WIDTH, TEST_WIDTH, TEST_HEIGHT))
		return FALSE;

	return TRUE;
}

static int test_alphaComp_speed(void)
{
	BYTE ALIGN(src1[SRC1_WIDTH * SRC1_HEIGHT]);
	BYTE ALIGN(src2[SRC2_WIDTH * SRC2_HEIGHT]);
	BYTE ALIGN(dst1[DST_WIDTH * DST_HEIGHT]);
	char testStr[256];
	UINT32* ptr;
	UINT32 i;
	testStr[0] = '\0';
	winpr_RAND((BYTE*)src1, sizeof(src1));
	/* Special-case the first two values */
	src1[0] &= 0x00FFFFFFU;
	src1[1] |= 0xFF000000U;
	winpr_RAND((BYTE*)src2, sizeof(src2));

	/* Set the second operand to fully-opaque. */
	ptr = (UINT32*)src2;

	for (i = 0; i < sizeof(src2) / 4; ++i) *ptr++ |= 0xFF000000U;

	memset(dst1, 0, sizeof(dst1));

	if (!speed_test("add16s", "aligned", g_Iterations,
			(speed_test_fkt)generic->alphaComp_argb,
			(speed_test_fkt)optimized->alphaComp_argb,
			src1, 4 * SRC1_WIDTH,
			src2, 4 * SRC2_WIDTH,
			dst1, 4 * DST_WIDTH, TEST_WIDTH, TEST_HEIGHT))
		return FALSE;

	return TRUE;
}

int TestPrimitivesAlphaComp(int argc, char* argv[])
{
	prim_test_setup(FALSE);
	if (!test_alphaComp_func())
		return -1;

	if (g_TestPrimitivesPerformance)
	{
		if (!test_alphaComp_speed())
			return -1;
	}

	return 0;
}
