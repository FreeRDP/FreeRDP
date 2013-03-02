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

extern pstatus_t general_alphaComp_argb(
	const BYTE *pSrc1,  int src1Step,
	const BYTE *pSrc2,  int src2Step,
	BYTE *pDst,  int dstStep,
	int width,  int height);
extern pstatus_t sse2_alphaComp_argb(
	const BYTE *pSrc1,  int src1Step,
	const BYTE *pSrc2,  int src2Step,
	BYTE *pDst,  int dstStep,
	int width,  int height);
extern pstatus_t ipp_alphaComp_argb(
	const BYTE *pSrc1,  int src1Step,
	const BYTE *pSrc2,  int src2Step,
	BYTE *pDst,  int dstStep,
	int width,  int height);

/* ========================================================================= */
#define ALF(_c_) (((_c_) & 0xFF000000U) >> 24)
#define RED(_c_) (((_c_) & 0x00FF0000U) >> 16)
#define GRN(_c_) (((_c_) & 0x0000FF00U) >> 8)
#define BLU(_c_) ((_c_) & 0x000000FFU)
#define TOLERANCE 1
#define PIXEL(_addr_, _bytes_, _x_, _y_) \
    ((UINT32 *) (((BYTE *) (_addr_)) + (_x_)*4 + (_y_)*(_bytes_)))
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

	UINT32 a3 = ((a1 * a1 + (255-a1) * a2) / 255) & 0xff;
	UINT32 r3 = ((a1 * r1 + (255-a1) * r2) / 255) & 0xff;
	UINT32 g3 = ((a1 * g1 + (255-a1) * g2) / 255) & 0xff;
	UINT32 b3 = ((a1 * b1 + (255-a1) * b2) / 255) & 0xff;

	return (a3 << 24) | (r3 << 16) | (g3 << 8) | b3;
}

/* ------------------------------------------------------------------------- */
static UINT32 colordist(
	UINT32 c1,
	UINT32 c2)
{
	int d, maxd = 0;

	d = ABS(ALF(c1) - ALF(c2));
	if (d > maxd) maxd = d;
	d = ABS(RED(c1) - RED(c2));
	if (d > maxd) maxd = d;
	d = ABS(GRN(c1) - GRN(c2));
	if (d > maxd) maxd = d;
	d = ABS(BLU(c1) - BLU(c2));
	if (d > maxd) maxd = d;
	return maxd;
}

/* ------------------------------------------------------------------------- */
int test_alphaComp_func(void)
{
	UINT32 ALIGN(src1[SRC1_WIDTH*SRC1_HEIGHT]);
	UINT32 ALIGN(src2[SRC2_WIDTH*SRC2_HEIGHT]);
	UINT32 ALIGN(dst1[DST_WIDTH*DST_HEIGHT]);
	UINT32 ALIGN(dst2a[DST_WIDTH*DST_HEIGHT]);
	UINT32 ALIGN(dst2u[DST_WIDTH*DST_HEIGHT+1]);
	UINT32 ALIGN(dst3[DST_WIDTH*DST_HEIGHT]);
	int error = 0;
	char testStr[256];
	UINT32 *ptr;
	int i, x, y;

	testStr[0] = '\0';
	get_random_data(src1, sizeof(src1));
	/* Special-case the first two values */
	src1[0] &= 0x00FFFFFFU;
	src1[1] |= 0xFF000000U;
	get_random_data(src2, sizeof(src2));
	/* Set the second operand to fully-opaque. */
	ptr = src2;
	for (i=0; i<sizeof(src2)/4; ++i) *ptr++ |= 0xFF000000U;
	memset(dst1, 0, sizeof(dst1));
	memset(dst2a, 0, sizeof(dst2a));
	memset(dst2u, 0, sizeof(dst2u));
	memset(dst3, 0, sizeof(dst3));

	general_alphaComp_argb((const BYTE *) src1, 4*SRC1_WIDTH, 
		(const BYTE *) src2, 4*SRC2_WIDTH,
		(BYTE *) dst1, 4*DST_WIDTH, TEST_WIDTH, TEST_HEIGHT);
#ifdef WITH_SSE2
	if (IsProcessorFeaturePresent(PF_SSE2_INSTRUCTIONS_AVAILABLE))
	{
		strcat(testStr, " SSE2");
		sse2_alphaComp_argb((const BYTE *) src1, 4*SRC1_WIDTH, 
			(const BYTE *) src2, 4*SRC2_WIDTH,
			(BYTE *) dst2a, 4*DST_WIDTH, TEST_WIDTH, TEST_HEIGHT);
		sse2_alphaComp_argb((const BYTE *) src1, 4*SRC1_WIDTH, 
			(const BYTE *) src2, 4*SRC2_WIDTH,
			(BYTE *) (dst2u+1), 4*DST_WIDTH, TEST_WIDTH, TEST_HEIGHT);
	}
#endif /* i386 */
#ifdef WITH_IPP
	strcat(testStr, " IPP");
	ipp_alphaComp_argb((const BYTE *) src1, 4*SRC1_WIDTH, 
		(const BYTE *) src2, 4*SRC2_WIDTH,
		(BYTE *) dst3, 4*DST_WIDTH, TEST_WIDTH, TEST_HEIGHT);
#endif

	for (y=0; y<TEST_HEIGHT; ++y)
	{
		for (x=0; x<TEST_WIDTH; ++x)
		{
			UINT32 s1 = *PIXEL(src1, 4*SRC1_WIDTH, x, y);
			UINT32 s2 = *PIXEL(src2, 4*SRC2_WIDTH, x, y);
			UINT32 c0 = alpha_add(s1, s2);
			UINT32 c1 = *PIXEL(dst1, 4*DST_WIDTH, x, y);
			if (colordist(c0, c1) > TOLERANCE)
			{
				printf("alphaComp-general: [%d,%d] 0x%08x+0x%08x=0x%08x, got 0x%08x\n", 
					x, y, s1, s2, c0, c1);
				error = 1;
			}
#ifdef WITH_SSE2
			if (IsProcessorFeaturePresent(PF_SSE2_INSTRUCTIONS_AVAILABLE))
			{
				UINT32 c2 = *PIXEL(dst2a, 4*DST_WIDTH, x, y);
				if (colordist(c0, c2) > TOLERANCE)
				{
					printf("alphaComp-SSE-aligned: [%d,%d] 0x%08x+0x%08x=0x%08x, got 0x%08x\n", 
						x, y, s1, s2, c0, c2);
					error = 1;
				}
				c2 = *PIXEL(dst2u+1, 4*DST_WIDTH, x, y);
				if (colordist(c0, c2) > TOLERANCE)
				{
					printf("alphaComp-SSE-unaligned: [%d,%d] 0x%08x+0x%08x=0x%08x, got 0x%08x\n", 
						x, y, s1, s2, c0, c2);
					error = 1;
				}
			}
#endif /* i386 */
#ifdef WITH_IPP
			{
				UINT32 c3 = *PIXEL(dst3, 4*DST_WIDTH, x, y);
				if (colordist(c0, c3) > TOLERANCE)
				{
					printf("alphaComp-IPP: [%d,%d] 0x%08x+0x%08x=0x%08x, got 0x%08x\n",
						x, y, s1, s2, c0, c3);
					error = 1;
				}
			}
#endif
		}
	}
	if (!error) printf("All alphaComp tests passed (%s).\n", testStr);
	return (error > 0) ? FAILURE : SUCCESS;
}


/* ------------------------------------------------------------------------- */
STD_SPEED_TEST(alphaComp_speed, BYTE, BYTE, int bytes __attribute__((unused)) = size*4,
	TRUE, general_alphaComp_argb(src1, bytes, src2, bytes, dst, bytes,
		size, size),
#ifdef WITH_SSE2
	TRUE, sse2_alphaComp_argb(src1, bytes, src2, bytes, dst, bytes,
		size, size), PF_SSE2_INSTRUCTIONS_AVAILABLE, FALSE,
#else
	FALSE, PRIM_NOP, 0, FALSE,
#endif
	TRUE, ipp_alphaComp_argb(src1, bytes, src2, bytes, dst, bytes,
		size, size));

int test_alphaComp_speed(void)
{
	INT32 ALIGN(src1[MAX_BLOCK_SIZE*(MAX_BLOCK_SIZE+1)]), 
	ALIGN(src2[SIZE_SQUARED]),
	ALIGN(dst[SIZE_SQUARED]);

	get_random_data(src1, sizeof(src1));
	get_random_data(src2, sizeof(src2));

	alphaComp_speed("alphaComp", "aligned",
		(BYTE *) src1, (BYTE *) src2, 0, (BYTE *) dst,
		block_size, NUM_BLOCK_SIZES, ALPHA_PRETEST_ITERATIONS, TEST_TIME);
	alphaComp_speed("alphaComp", "unaligned",
		(BYTE *) src1+1, (BYTE *) src2, 0, (BYTE *) dst,
		block_size, NUM_BLOCK_SIZES, ALPHA_PRETEST_ITERATIONS, TEST_TIME);

	return SUCCESS;
}
