/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Generic YUV/RGB conversion operations
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015-2017 Armin Novak <armin.novak@thincast.com>
 * Copyright 2015-2017 Norbert Federa <norbert.federa@thincast.com>
 * Copyright 2015-2017 Vic Lee
 * Copyright 2015-2017 Thincast Technologies GmbH
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

#include <winpr/wtypes.h>
#include <winpr/assert.h>
#include <winpr/cast.h>

#include <freerdp/config.h>

#include <freerdp/types.h>
#include <freerdp/primitives.h>
#include <freerdp/codec/color.h>
#include "prim_internal.h"
#include "prim_YUV.h"

static inline pstatus_t general_LumaToYUV444(const BYTE* WINPR_RESTRICT pSrcRaw[3],
                                             const UINT32 srcStep[3],
                                             BYTE* WINPR_RESTRICT pDstRaw[3],
                                             const UINT32 dstStep[3],
                                             const RECTANGLE_16* WINPR_RESTRICT roi)
{
	const UINT32 nWidth = roi->right - roi->left;
	const UINT32 nHeight = roi->bottom - roi->top;
	const UINT32 halfWidth = (nWidth + 1) / 2;
	const UINT32 halfHeight = (nHeight + 1) / 2;
	const UINT32 oddY = 1;
	const UINT32 evenY = 0;
	const UINT32 oddX = 1;
	const UINT32 evenX = 0;
	const BYTE* pSrc[3] = { pSrcRaw[0] + 1ULL * roi->top * srcStep[0] + roi->left,
		                    pSrcRaw[1] + 1ULL * roi->top / 2 * srcStep[1] + roi->left / 2,
		                    pSrcRaw[2] + 1ULL * roi->top / 2 * srcStep[2] + roi->left / 2 };
	BYTE* pDst[3] = { pDstRaw[0] + 1ULL * roi->top * dstStep[0] + roi->left,
		              pDstRaw[1] + 1ULL * roi->top * dstStep[1] + roi->left,
		              pDstRaw[2] + 1ULL * roi->top * dstStep[2] + roi->left };

	/* Y data is already here... */
	/* B1 */
	for (size_t y = 0; y < nHeight; y++)
	{
		const BYTE* Ym = pSrc[0] + y * srcStep[0];
		BYTE* pY = pDst[0] + dstStep[0] * y;
		memcpy(pY, Ym, nWidth);
	}

	/* The first half of U, V are already here part of this frame. */
	/* B2 and B3 */
	for (UINT32 y = 0; y < halfHeight; y++)
	{
		const UINT32 val2y = (2UL * y + evenY);
		const UINT32 val2y1 = val2y + oddY;
		const BYTE* Um = pSrc[1] + 1ULL * y * srcStep[1];
		const BYTE* Vm = pSrc[2] + 1ULL * y * srcStep[2];
		BYTE* pU = pDst[1] + 1ULL * dstStep[1] * val2y;
		BYTE* pV = pDst[2] + 1ULL * dstStep[2] * val2y;
		BYTE* pU1 = pDst[1] + 1ULL * dstStep[1] * val2y1;
		BYTE* pV1 = pDst[2] + 1ULL * dstStep[2] * val2y1;

		for (UINT32 x = 0; x < halfWidth; x++)
		{
			const UINT32 val2x = 2UL * x + evenX;
			const UINT32 val2x1 = val2x + oddX;
			pU[val2x] = Um[x];
			pV[val2x] = Vm[x];
			pU[val2x1] = Um[x];
			pV[val2x1] = Vm[x];
			pU1[val2x] = Um[x];
			pV1[val2x] = Vm[x];
			pU1[val2x1] = Um[x];
			pV1[val2x1] = Vm[x];
		}
	}

	return PRIMITIVES_SUCCESS;
}

static inline pstatus_t general_ChromaV1ToYUV444(const BYTE* WINPR_RESTRICT pSrcRaw[3],
                                                 const UINT32 srcStep[3],
                                                 BYTE* WINPR_RESTRICT pDstRaw[3],
                                                 const UINT32 dstStep[3],
                                                 const RECTANGLE_16* WINPR_RESTRICT roi)
{
	const UINT32 mod = 16;
	UINT32 uY = 0;
	UINT32 vY = 0;
	const UINT32 nWidth = roi->right - roi->left;
	const UINT32 nHeight = roi->bottom - roi->top;
	const UINT32 halfWidth = (nWidth) / 2;
	const UINT32 halfHeight = (nHeight) / 2;
	const UINT32 oddY = 1;
	const UINT32 evenY = 0;
	const UINT32 oddX = 1;
	/* The auxiliary frame is aligned to multiples of 16x16.
	 * We need the padded height for B4 and B5 conversion. */
	const UINT32 padHeigth = nHeight + 16 - nHeight % 16;
	const BYTE* pSrc[3] = { pSrcRaw[0] + 1ULL * roi->top * srcStep[0] + roi->left,
		                    pSrcRaw[1] + 1ULL * roi->top / 2 * srcStep[1] + roi->left / 2,
		                    pSrcRaw[2] + 1ULL * roi->top / 2 * srcStep[2] + roi->left / 2 };
	BYTE* pDst[3] = { pDstRaw[0] + 1ULL * roi->top * dstStep[0] + roi->left,
		              pDstRaw[1] + 1ULL * roi->top * dstStep[1] + roi->left,
		              pDstRaw[2] + 1ULL * roi->top * dstStep[2] + roi->left };

	/* The second half of U and V is a bit more tricky... */
	/* B4 and B5 */
	for (size_t y = 0; y < padHeigth; y++)
	{
		const BYTE* Ya = pSrc[0] + y * srcStep[0];
		BYTE* pX = NULL;

		if ((y) % mod < (mod + 1) / 2)
		{
			const size_t pos = (2 * uY++ + oddY);

			if (pos >= nHeight)
				continue;

			pX = pDst[1] + dstStep[1] * pos;
		}
		else
		{
			const size_t pos = (2 * vY++ + oddY);

			if (pos >= nHeight)
				continue;

			pX = pDst[2] + dstStep[2] * pos;
		}

		memcpy(pX, Ya, nWidth);
	}

	/* B6 and B7 */
	for (UINT32 y = 0; y < halfHeight; y++)
	{
		const UINT32 val2y = (y * 2UL + evenY);
		const BYTE* Ua = pSrc[1] + 1ULL * y * srcStep[1];
		const BYTE* Va = pSrc[2] + 1ULL * y * srcStep[2];
		BYTE* pU = pDst[1] + 1ULL * dstStep[1] * val2y;
		BYTE* pV = pDst[2] + 1ULL * dstStep[2] * val2y;

		for (UINT32 x = 0; x < halfWidth; x++)
		{
			const UINT32 val2x1 = (x * 2 + oddX);
			pU[val2x1] = Ua[x];
			pV[val2x1] = Va[x];
		}
	}

	return PRIMITIVES_SUCCESS;
}

static inline pstatus_t general_ChromaV2ToYUV444(const BYTE* WINPR_RESTRICT pSrc[3],
                                                 const UINT32 srcStep[3], UINT32 nTotalWidth,
                                                 WINPR_ATTR_UNUSED UINT32 nTotalHeight,
                                                 BYTE* WINPR_RESTRICT pDst[3],
                                                 const UINT32 dstStep[3],
                                                 const RECTANGLE_16* WINPR_RESTRICT roi)
{
	const UINT32 nWidth = roi->right - roi->left;
	const UINT32 nHeight = roi->bottom - roi->top;
	const UINT32 halfWidth = (nWidth + 1) / 2;
	const UINT32 halfHeight = (nHeight + 1) / 2;
	const UINT32 quaterWidth = (nWidth + 3) / 4;

	/* B4 and B5: odd UV values for width/2, height */
	for (UINT32 y = 0; y < nHeight; y++)
	{
		const UINT32 yTop = y + roi->top;
		const BYTE* pYaU = pSrc[0] + 1ULL * srcStep[0] * yTop + roi->left / 2;
		const BYTE* pYaV = pYaU + nTotalWidth / 2;
		BYTE* pU = pDst[1] + 1ULL * dstStep[1] * yTop + roi->left;
		BYTE* pV = pDst[2] + 1ULL * dstStep[2] * yTop + roi->left;

		for (UINT32 x = 0; x < halfWidth; x++)
		{
			const UINT32 odd = 2UL * x + 1UL;
			pU[odd] = *pYaU++;
			pV[odd] = *pYaV++;
		}
	}

	/* B6 - B9 */
	for (size_t y = 0; y < halfHeight; y++)
	{
		const BYTE* pUaU = pSrc[1] + srcStep[1] * (y + roi->top / 2) + roi->left / 4;
		const BYTE* pUaV = pUaU + nTotalWidth / 4;
		const BYTE* pVaU = pSrc[2] + srcStep[2] * (y + roi->top / 2) + roi->left / 4;
		const BYTE* pVaV = pVaU + nTotalWidth / 4;
		BYTE* pU = pDst[1] + 1ULL * dstStep[1] * (2ULL * y + 1 + roi->top) + roi->left;
		BYTE* pV = pDst[2] + 1ULL * dstStep[2] * (2ULL * y + 1 + roi->top) + roi->left;

		for (size_t x = 0; x < quaterWidth; x++)
		{
			pU[4 * x + 0] = *pUaU++;
			pV[4 * x + 0] = *pUaV++;
			pU[4 * x + 2] = *pVaU++;
			pV[4 * x + 2] = *pVaV++;
		}
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t general_YUV420CombineToYUV444(avc444_frame_type type,
                                               const BYTE* WINPR_RESTRICT pSrc[3],
                                               const UINT32 srcStep[3], UINT32 nWidth,
                                               UINT32 nHeight, BYTE* WINPR_RESTRICT pDst[3],
                                               const UINT32 dstStep[3],
                                               const RECTANGLE_16* WINPR_RESTRICT roi)
{
	if (!pSrc || !pSrc[0] || !pSrc[1] || !pSrc[2])
		return -1;

	if (!pDst || !pDst[0] || !pDst[1] || !pDst[2])
		return -1;

	if (!roi)
		return -1;

	switch (type)
	{
		case AVC444_LUMA:
			return general_LumaToYUV444(pSrc, srcStep, pDst, dstStep, roi);

		case AVC444_CHROMAv1:
			return general_ChromaV1ToYUV444(pSrc, srcStep, pDst, dstStep, roi);

		case AVC444_CHROMAv2:
			return general_ChromaV2ToYUV444(pSrc, srcStep, nWidth, nHeight, pDst, dstStep, roi);

		default:
			return -1;
	}
}

static pstatus_t
general_YUV444SplitToYUV420(const BYTE* WINPR_RESTRICT pSrc[3], const UINT32 srcStep[3],
                            BYTE* WINPR_RESTRICT pMainDst[3], const UINT32 dstMainStep[3],
                            BYTE* WINPR_RESTRICT pAuxDst[3], const UINT32 dstAuxStep[3],
                            const prim_size_t* WINPR_RESTRICT roi)
{
	UINT32 uY = 0;
	UINT32 vY = 0;

	/* The auxiliary frame is aligned to multiples of 16x16.
	 * We need the padded height for B4 and B5 conversion. */
	const UINT32 padHeigth = roi->height + 16 - roi->height % 16;
	const UINT32 halfWidth = (roi->width + 1) / 2;
	const UINT32 halfHeight = (roi->height + 1) / 2;

	/* B1 */
	for (size_t y = 0; y < roi->height; y++)
	{
		const BYTE* pSrcY = pSrc[0] + y * srcStep[0];
		BYTE* pY = pMainDst[0] + y * dstMainStep[0];
		memcpy(pY, pSrcY, roi->width);
	}

	/* B2 and B3 */
	for (size_t y = 0; y < halfHeight; y++)
	{
		const BYTE* pSrcU = pSrc[1] + 2ULL * y * srcStep[1];
		const BYTE* pSrcV = pSrc[2] + 2ULL * y * srcStep[2];
		BYTE* pU = pMainDst[1] + y * dstMainStep[1];
		BYTE* pV = pMainDst[2] + y * dstMainStep[2];

		for (size_t x = 0; x < halfWidth; x++)
		{
			pU[x] = pSrcV[2 * x];
			pV[x] = pSrcU[2 * x];
		}
	}

	/* B4 and B5 */
	for (size_t y = 0; y < padHeigth; y++)
	{
		BYTE* pY = pAuxDst[0] + y * dstAuxStep[0];

		if (y % 16 < 8)
		{
			const size_t pos = (2 * uY++ + 1);
			const BYTE* pSrcU = pSrc[1] + pos * srcStep[1];

			if (pos >= roi->height)
				continue;

			memcpy(pY, pSrcU, roi->width);
		}
		else
		{
			const size_t pos = (2 * vY++ + 1);
			const BYTE* pSrcV = pSrc[2] + pos * srcStep[2];

			if (pos >= roi->height)
				continue;

			memcpy(pY, pSrcV, roi->width);
		}
	}

	/* B6 and B7 */
	for (size_t y = 0; y < halfHeight; y++)
	{
		const BYTE* pSrcU = pSrc[1] + 2 * y * srcStep[1];
		const BYTE* pSrcV = pSrc[2] + 2 * y * srcStep[2];
		BYTE* pU = pAuxDst[1] + y * dstAuxStep[1];
		BYTE* pV = pAuxDst[2] + y * dstAuxStep[2];

		for (size_t x = 0; x < halfWidth; x++)
		{
			pU[x] = pSrcU[2 * x + 1];
			pV[x] = pSrcV[2 * x + 1];
		}
	}

	return PRIMITIVES_SUCCESS;
}

static inline void general_YUV444ToRGB_DOUBLE_ROW(BYTE* WINPR_RESTRICT pRGB[2], UINT32 DstFormat,
                                                  const BYTE* WINPR_RESTRICT pY[2],
                                                  const BYTE* WINPR_RESTRICT pU[2],
                                                  const BYTE* WINPR_RESTRICT pV[2], size_t nWidth)
{
	fkt_writePixel writePixel = getPixelWriteFunction(DstFormat, FALSE);

	WINPR_ASSERT(nWidth % 2 == 0);
	for (size_t x = 0; x < nWidth; x += 2)
	{
		for (size_t i = 0; i < 2; i++)
		{
			for (size_t j = 0; j < 2; j++)
			{
				const BYTE y = pY[i][x + j];
				INT32 u = pU[i][x + j];
				INT32 v = pV[i][x + j];
				if ((i == 0) && (j == 0))
				{
					const INT32 subU = (INT32)pU[0][x + 1] + pU[1][x] + pU[1][x + 1];
					const INT32 avgU = ((4 * u) - subU);
					u = CONDITIONAL_CLIP(avgU, WINPR_ASSERTING_INT_CAST(BYTE, u));

					const INT32 subV = (INT32)pV[0][x + 1] + pV[1][x] + pV[1][x + 1];
					const INT32 avgV = ((4 * v) - subV);
					v = CONDITIONAL_CLIP(avgV, WINPR_ASSERTING_INT_CAST(BYTE, v));
				}
				pRGB[i] = writeYUVPixel(pRGB[i], DstFormat, y, u, v, writePixel);
			}
		}
	}
}

static inline void general_YUV444ToRGB_SINGLE_ROW(BYTE* WINPR_RESTRICT pRGB, UINT32 DstFormat,
                                                  const BYTE* WINPR_RESTRICT pY,
                                                  const BYTE* WINPR_RESTRICT pU,
                                                  const BYTE* WINPR_RESTRICT pV, size_t nWidth)
{
	fkt_writePixel writePixel = getPixelWriteFunction(DstFormat, FALSE);

	WINPR_ASSERT(nWidth % 2 == 0);
	for (size_t x = 0; x < nWidth; x += 2)
	{
		for (size_t j = 0; j < 2; j++)
		{
			const BYTE y = pY[x + j];
			const BYTE u = pU[x + j];
			const BYTE v = pV[x + j];
			pRGB = writeYUVPixel(pRGB, DstFormat, y, u, v, writePixel);
		}
	}
}

static inline pstatus_t general_YUV444ToRGB_8u_P3AC4R_general(const BYTE* WINPR_RESTRICT pSrc[3],
                                                              const UINT32 srcStep[3],
                                                              BYTE* WINPR_RESTRICT pDst,
                                                              UINT32 dstStep, UINT32 DstFormat,
                                                              const prim_size_t* WINPR_RESTRICT roi)
{
	WINPR_ASSERT(pSrc);
	WINPR_ASSERT(pDst);
	WINPR_ASSERT(roi);

	const UINT32 nWidth = roi->width;
	const UINT32 nHeight = roi->height;

	size_t y = 0;
	for (; y < nHeight - nHeight % 2; y += 2)
	{
		const BYTE* WINPR_RESTRICT pY[2] = { pSrc[0] + y * srcStep[0],
			                                 pSrc[0] + (y + 1) * srcStep[0] };
		const BYTE* WINPR_RESTRICT pU[2] = { pSrc[1] + y * srcStep[1],
			                                 pSrc[1] + (y + 1) * srcStep[1] };
		const BYTE* WINPR_RESTRICT pV[2] = { pSrc[2] + y * srcStep[2],
			                                 pSrc[2] + (y + 1) * srcStep[2] };
		BYTE* WINPR_RESTRICT pRGB[] = { pDst + y * dstStep, pDst + (y + 1) * dstStep };

		general_YUV444ToRGB_DOUBLE_ROW(pRGB, DstFormat, pY, pU, pV, nWidth);
	}
	for (; y < nHeight; y++)
	{
		const BYTE* WINPR_RESTRICT pY = pSrc[0] + y * srcStep[0];
		const BYTE* WINPR_RESTRICT pU = pSrc[1] + y * srcStep[1];
		const BYTE* WINPR_RESTRICT pV = pSrc[2] + y * srcStep[2];
		BYTE* WINPR_RESTRICT pRGB = pDst + y * dstStep;

		general_YUV444ToRGB_SINGLE_ROW(pRGB, DstFormat, pY, pU, pV, nWidth);
	}

	return PRIMITIVES_SUCCESS;
}

static inline void general_YUV444ToBGRX_DOUBLE_ROW(BYTE* WINPR_RESTRICT pRGB[2], UINT32 DstFormat,
                                                   const BYTE* WINPR_RESTRICT pY[2],
                                                   const BYTE* WINPR_RESTRICT pU[2],
                                                   const BYTE* WINPR_RESTRICT pV[2], size_t nWidth)
{
	WINPR_ASSERT(nWidth % 2 == 0);
	for (size_t x = 0; x < nWidth; x += 2)
	{
		const INT32 subU = pU[0][x + 1] + pU[1][x] + pU[1][x + 1];
		const INT32 avgU = ((4 * pU[0][x]) - subU);
		const BYTE useU = CONDITIONAL_CLIP(avgU, pU[0][x]);
		const INT32 subV = pV[0][x + 1] + pV[1][x] + pV[1][x + 1];
		const INT32 avgV = ((4 * pV[0][x]) - subV);
		const BYTE useV = CONDITIONAL_CLIP(avgV, pV[0][x]);

		const BYTE U[2][2] = { { useU, pU[0][x + 1] }, { pU[1][x], pU[1][x + 1] } };
		const BYTE V[2][2] = { { useV, pV[0][x + 1] }, { pV[1][x], pV[1][x + 1] } };

		for (size_t i = 0; i < 2; i++)
		{
			for (size_t j = 0; j < 2; j++)
			{
				const BYTE y = pY[i][x + j];
				const BYTE u = U[i][j];
				const BYTE v = V[i][j];
				pRGB[i] = writeYUVPixel(pRGB[i], DstFormat, y, u, v, writePixelBGRX);
			}
		}
	}
}

static inline void general_YUV444ToBGRX_SINGLE_ROW(BYTE* WINPR_RESTRICT pRGB, UINT32 DstFormat,
                                                   const BYTE* WINPR_RESTRICT pY,
                                                   const BYTE* WINPR_RESTRICT pU,
                                                   const BYTE* WINPR_RESTRICT pV, size_t nWidth)
{
	WINPR_ASSERT(nWidth % 2 == 0);
	for (size_t x = 0; x < nWidth; x += 2)
	{
		for (size_t j = 0; j < 2; j++)
		{
			const BYTE Y = pY[x + j];
			const BYTE U = pU[x + j];
			const BYTE V = pV[x + j];
			pRGB = writeYUVPixel(pRGB, DstFormat, Y, U, V, writePixelBGRX);
		}
	}
}

static inline pstatus_t general_YUV444ToRGB_8u_P3AC4R_BGRX(const BYTE* WINPR_RESTRICT pSrc[3],
                                                           const UINT32 srcStep[3],
                                                           BYTE* WINPR_RESTRICT pDst,
                                                           UINT32 dstStep, UINT32 DstFormat,
                                                           const prim_size_t* WINPR_RESTRICT roi)
{
	WINPR_ASSERT(pSrc);
	WINPR_ASSERT(pDst);
	WINPR_ASSERT(roi);

	const UINT32 nWidth = roi->width;
	const UINT32 nHeight = roi->height;

	size_t y = 0;
	for (; y < nHeight - nHeight % 2; y += 2)
	{
		const BYTE* pY[2] = { pSrc[0] + y * srcStep[0], pSrc[0] + (y + 1) * srcStep[0] };
		const BYTE* pU[2] = { pSrc[1] + y * srcStep[1], pSrc[1] + (y + 1) * srcStep[1] };
		const BYTE* pV[2] = { pSrc[2] + y * srcStep[2], pSrc[2] + (y + 1) * srcStep[2] };
		BYTE* pRGB[] = { pDst + y * dstStep, pDst + (y + 1) * dstStep };

		general_YUV444ToBGRX_DOUBLE_ROW(pRGB, DstFormat, pY, pU, pV, nWidth);
	}

	for (; y < nHeight; y++)
	{
		const BYTE* WINPR_RESTRICT pY = pSrc[0] + y * srcStep[0];
		const BYTE* WINPR_RESTRICT pU = pSrc[1] + y * srcStep[1];
		const BYTE* WINPR_RESTRICT pV = pSrc[2] + y * srcStep[2];
		BYTE* WINPR_RESTRICT pRGB = pDst + y * dstStep;

		general_YUV444ToBGRX_SINGLE_ROW(pRGB, DstFormat, pY, pU, pV, nWidth);
	}
	return PRIMITIVES_SUCCESS;
}

static pstatus_t general_YUV444ToRGB_8u_P3AC4R(const BYTE* WINPR_RESTRICT pSrc[3],
                                               const UINT32 srcStep[3], BYTE* WINPR_RESTRICT pDst,
                                               UINT32 dstStep, UINT32 DstFormat,
                                               const prim_size_t* WINPR_RESTRICT roi)
{
	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return general_YUV444ToRGB_8u_P3AC4R_BGRX(pSrc, srcStep, pDst, dstStep, DstFormat, roi);

		default:
			return general_YUV444ToRGB_8u_P3AC4R_general(pSrc, srcStep, pDst, dstStep, DstFormat,
			                                             roi);
	}
}
/**
 * | R |   ( | 256     0    403 | |    Y    | )
 * | G | = ( | 256   -48   -120 | | U - 128 | ) >> 8
 * | B |   ( | 256   475      0 | | V - 128 | )
 */
static void general_YUV420ToRGB_8u_P3AC4R_double_line(BYTE* WINPR_RESTRICT pEven,
                                                      BYTE* WINPR_RESTRICT pOdd, UINT32 DstFormat,
                                                      const BYTE* WINPR_RESTRICT pYeven,
                                                      const BYTE* WINPR_RESTRICT pYodd,
                                                      const BYTE* WINPR_RESTRICT pU,
                                                      const BYTE* WINPR_RESTRICT pV, UINT32 width,
                                                      fkt_writePixel writePixel, UINT32 formatSize)
{

	UINT32 x = 0;
	for (; x < width / 2; x++)
	{
		const BYTE U = pU[x];
		const BYTE V = pV[x];
		const BYTE eY0 = pYeven[2ULL * x + 0];
		const BYTE eY1 = pYeven[2ULL * x + 1];
		writeYUVPixel(&pEven[2ULL * x * formatSize], DstFormat, eY0, U, V, writePixel);
		writeYUVPixel(&pEven[(2ULL * x + 1) * formatSize], DstFormat, eY1, U, V, writePixel);

		const BYTE oY0 = pYodd[2ULL * x + 0];
		const BYTE oY1 = pYodd[2ULL * x + 1];
		writeYUVPixel(&pOdd[2ULL * x * formatSize], DstFormat, oY0, U, V, writePixel);
		writeYUVPixel(&pOdd[(2ULL * x + 1) * formatSize], DstFormat, oY1, U, V, writePixel);
	}

	for (; x < (width + 1) / 2; x++)
	{
		const BYTE U = pU[x];
		const BYTE V = pV[x];
		const BYTE eY0 = pYeven[2ULL * x + 0];
		writeYUVPixel(&pEven[2ULL * x * formatSize], DstFormat, eY0, U, V, writePixel);

		const BYTE oY0 = pYodd[2ULL * x + 0];
		writeYUVPixel(&pOdd[2ULL * x * formatSize], DstFormat, oY0, U, V, writePixel);
	}
}

static void general_YUV420ToRGB_8u_P3AC4R_single_line(BYTE* WINPR_RESTRICT pEven, UINT32 DstFormat,
                                                      const BYTE* WINPR_RESTRICT pYeven,
                                                      const BYTE* WINPR_RESTRICT pU,
                                                      const BYTE* WINPR_RESTRICT pV, UINT32 width,
                                                      fkt_writePixel writePixel, UINT32 formatSize)
{

	UINT32 x = 0;
	for (; x < width / 2; x++)
	{
		const BYTE U = pU[x];
		const BYTE V = pV[x];
		const BYTE eY0 = pYeven[2ULL * x + 0];
		const BYTE eY1 = pYeven[2ULL * x + 1];
		writeYUVPixel(&pEven[2ULL * x * formatSize], DstFormat, eY0, U, V, writePixel);
		writeYUVPixel(&pEven[(2ULL * x + 1) * formatSize], DstFormat, eY1, U, V, writePixel);
	}

	for (; x < (width + 1) / 2; x++)
	{
		const BYTE U = pU[x];
		const BYTE V = pV[x];
		const BYTE eY0 = pYeven[2ULL * x + 0];
		writeYUVPixel(&pEven[2ULL * x * formatSize], DstFormat, eY0, U, V, writePixel);
	}
}

static pstatus_t general_YUV420ToRGB_8u_P3AC4R(const BYTE* WINPR_RESTRICT pSrc[3],
                                               const UINT32 srcStep[3], BYTE* WINPR_RESTRICT pDst,
                                               UINT32 dstStep, UINT32 DstFormat,
                                               const prim_size_t* WINPR_RESTRICT roi)
{
	WINPR_ASSERT(roi);
	const DWORD formatSize = FreeRDPGetBytesPerPixel(DstFormat);
	fkt_writePixel writePixel = getPixelWriteFunction(DstFormat, FALSE);
	const UINT32 nWidth = roi->width;
	const UINT32 nHeight = roi->height;

	UINT32 y = 0;
	for (; y < nHeight / 2; y++)
	{
		const BYTE* pYe = &pSrc[0][(2ULL * y + 0) * srcStep[0]];
		const BYTE* pYo = &pSrc[0][(2ULL * y + 1) * srcStep[0]];
		const BYTE* pU = &pSrc[1][1ULL * srcStep[1] * y];
		const BYTE* pV = &pSrc[2][1ULL * srcStep[2] * y];
		BYTE* pRGBeven = &pDst[2ULL * y * dstStep];
		BYTE* pRGBodd = &pDst[(2ULL * y + 1) * dstStep];
		general_YUV420ToRGB_8u_P3AC4R_double_line(pRGBeven, pRGBodd, DstFormat, pYe, pYo, pU, pV,
		                                          nWidth, writePixel, formatSize);
	}

	// Last row (if odd)
	for (; y < (nHeight + 1) / 2; y++)
	{
		const BYTE* pY = &pSrc[0][2ULL * srcStep[0] * y];
		const BYTE* pU = &pSrc[1][1ULL * srcStep[1] * y];
		const BYTE* pV = &pSrc[2][1ULL * srcStep[2] * y];
		BYTE* pEven = &pDst[2ULL * y * dstStep];

		general_YUV420ToRGB_8u_P3AC4R_single_line(pEven, DstFormat, pY, pU, pV, nWidth, writePixel,
		                                          formatSize);
	}

	return PRIMITIVES_SUCCESS;
}

static inline void BGRX_fillYUV(size_t offset, const BYTE* WINPR_RESTRICT pRGB[2],
                                BYTE* WINPR_RESTRICT pY[2], BYTE* WINPR_RESTRICT pU[2],
                                BYTE* WINPR_RESTRICT pV[2])
{
	WINPR_ASSERT(pRGB);
	WINPR_ASSERT(pY);
	WINPR_ASSERT(pU);
	WINPR_ASSERT(pV);

	const UINT32 SrcFormat = PIXEL_FORMAT_BGRX32;
	const UINT32 bpp = 4;

	for (size_t i = 0; i < 2; i++)
	{
		for (size_t j = 0; j < 2; j++)
		{
			BYTE B = 0;
			BYTE G = 0;
			BYTE R = 0;
			const UINT32 color = FreeRDPReadColor(&pRGB[i][(offset + j) * bpp], SrcFormat);
			FreeRDPSplitColor(color, SrcFormat, &R, &G, &B, NULL, NULL);
			pY[i][offset + j] = RGB2Y(R, G, B);
			pU[i][offset + j] = RGB2U(R, G, B);
			pV[i][offset + j] = RGB2V(R, G, B);
		}
	}

	/* Apply chroma filter */
	const INT32 avgU = (pU[0][offset] + pU[0][offset + 1] + pU[1][offset] + pU[1][offset + 1]) / 4;
	pU[0][offset] = CONDITIONAL_CLIP(avgU, pU[0][offset]);
	const INT32 avgV = (pV[0][offset] + pV[0][offset + 1] + pV[1][offset] + pV[1][offset + 1]) / 4;
	pV[0][offset] = CONDITIONAL_CLIP(avgV, pV[0][offset]);
}

static inline void BGRX_fillYUV_single(size_t offset, const BYTE* WINPR_RESTRICT pRGB,
                                       BYTE* WINPR_RESTRICT pY, BYTE* WINPR_RESTRICT pU,
                                       BYTE* WINPR_RESTRICT pV)
{
	WINPR_ASSERT(pRGB);
	WINPR_ASSERT(pY);
	WINPR_ASSERT(pU);
	WINPR_ASSERT(pV);

	const UINT32 SrcFormat = PIXEL_FORMAT_BGRX32;
	const UINT32 bpp = 4;

	for (size_t j = 0; j < 2; j++)
	{
		BYTE B = 0;
		BYTE G = 0;
		BYTE R = 0;
		const UINT32 color = FreeRDPReadColor(&pRGB[(offset + j) * bpp], SrcFormat);
		FreeRDPSplitColor(color, SrcFormat, &R, &G, &B, NULL, NULL);
		pY[offset + j] = RGB2Y(R, G, B);
		pU[offset + j] = RGB2U(R, G, B);
		pV[offset + j] = RGB2V(R, G, B);
	}
}

static inline void general_BGRXToYUV444_DOUBLE_ROW(const BYTE* WINPR_RESTRICT pRGB[2],
                                                   BYTE* WINPR_RESTRICT pY[2],
                                                   BYTE* WINPR_RESTRICT pU[2],
                                                   BYTE* WINPR_RESTRICT pV[2], UINT32 nWidth)
{

	WINPR_ASSERT((nWidth % 2) == 0);
	for (size_t x = 0; x < nWidth; x += 2)
	{
		BGRX_fillYUV(x, pRGB, pY, pU, pV);
	}
}

static inline void general_BGRXToYUV444_SINGLE_ROW(const BYTE* WINPR_RESTRICT pRGB,
                                                   BYTE* WINPR_RESTRICT pY, BYTE* WINPR_RESTRICT pU,
                                                   BYTE* WINPR_RESTRICT pV, UINT32 nWidth)
{

	WINPR_ASSERT((nWidth % 2) == 0);
	for (size_t x = 0; x < nWidth; x += 2)
	{
		BGRX_fillYUV_single(x, pRGB, pY, pU, pV);
	}
}

static inline pstatus_t general_RGBToYUV444_8u_P3AC4R_BGRX(const BYTE* WINPR_RESTRICT pSrc,
                                                           const UINT32 srcStep,
                                                           BYTE* WINPR_RESTRICT pDst[3],
                                                           const UINT32 dstStep[3],
                                                           const prim_size_t* WINPR_RESTRICT roi)
{
	const UINT32 nWidth = roi->width;
	const UINT32 nHeight = roi->height;

	size_t y = 0;
	for (; y < nHeight - nHeight % 2; y += 2)
	{
		const BYTE* pRGB[] = { pSrc + y * srcStep, pSrc + (y + 1) * srcStep };
		BYTE* pY[] = { pDst[0] + y * dstStep[0], pDst[0] + (y + 1) * dstStep[0] };
		BYTE* pU[] = { pDst[1] + y * dstStep[1], pDst[1] + (y + 1) * dstStep[1] };
		BYTE* pV[] = { pDst[2] + y * dstStep[2], pDst[2] + (y + 1) * dstStep[2] };

		general_BGRXToYUV444_DOUBLE_ROW(pRGB, pY, pU, pV, nWidth);
	}

	for (; y < nHeight; y++)
	{
		const BYTE* pRGB = pSrc + y * srcStep;
		BYTE* pY = pDst[0] + y * dstStep[0];
		BYTE* pU = pDst[1] + y * dstStep[1];
		BYTE* pV = pDst[2] + y * dstStep[2];

		general_BGRXToYUV444_SINGLE_ROW(pRGB, pY, pU, pV, nWidth);
	}

	return PRIMITIVES_SUCCESS;
}

static inline void fillYUV(size_t offset, const BYTE* WINPR_RESTRICT pRGB[2], UINT32 SrcFormat,
                           BYTE* WINPR_RESTRICT pY[2], BYTE* WINPR_RESTRICT pU[2],
                           BYTE* WINPR_RESTRICT pV[2])
{
	WINPR_ASSERT(pRGB);
	WINPR_ASSERT(pY);
	WINPR_ASSERT(pU);
	WINPR_ASSERT(pV);
	const UINT32 bpp = FreeRDPGetBytesPerPixel(SrcFormat);

	INT32 avgU = 0;
	INT32 avgV = 0;
	for (size_t i = 0; i < 2; i++)
	{
		for (size_t j = 0; j < 2; j++)
		{
			BYTE B = 0;
			BYTE G = 0;
			BYTE R = 0;
			const UINT32 color = FreeRDPReadColor(&pRGB[i][(offset + j) * bpp], SrcFormat);
			FreeRDPSplitColor(color, SrcFormat, &R, &G, &B, NULL, NULL);
			const BYTE y = RGB2Y(R, G, B);
			const BYTE u = RGB2U(R, G, B);
			const BYTE v = RGB2V(R, G, B);
			avgU += u;
			avgV += v;
			pY[i][offset + j] = y;
			pU[i][offset + j] = u;
			pV[i][offset + j] = v;
		}
	}

	/* Apply chroma filter */
	avgU /= 4;
	pU[0][offset] = CLIP(avgU);

	avgV /= 4;
	pV[0][offset] = CLIP(avgV);
}

static inline void fillYUV_single(size_t offset, const BYTE* WINPR_RESTRICT pRGB, UINT32 SrcFormat,
                                  BYTE* WINPR_RESTRICT pY, BYTE* WINPR_RESTRICT pU,
                                  BYTE* WINPR_RESTRICT pV)
{
	WINPR_ASSERT(pRGB);
	WINPR_ASSERT(pY);
	WINPR_ASSERT(pU);
	WINPR_ASSERT(pV);
	const UINT32 bpp = FreeRDPGetBytesPerPixel(SrcFormat);

	for (size_t j = 0; j < 2; j++)
	{
		BYTE B = 0;
		BYTE G = 0;
		BYTE R = 0;
		const UINT32 color = FreeRDPReadColor(&pRGB[(offset + j) * bpp], SrcFormat);
		FreeRDPSplitColor(color, SrcFormat, &R, &G, &B, NULL, NULL);
		const BYTE y = RGB2Y(R, G, B);
		const BYTE u = RGB2U(R, G, B);
		const BYTE v = RGB2V(R, G, B);
		pY[offset + j] = y;
		pU[offset + j] = u;
		pV[offset + j] = v;
	}
}

static inline void general_RGBToYUV444_DOUBLE_ROW(const BYTE* WINPR_RESTRICT pRGB[2],
                                                  UINT32 SrcFormat, BYTE* WINPR_RESTRICT pY[2],
                                                  BYTE* WINPR_RESTRICT pU[2],
                                                  BYTE* WINPR_RESTRICT pV[2], UINT32 nWidth)
{

	WINPR_ASSERT((nWidth % 2) == 0);
	for (size_t x = 0; x < nWidth; x += 2)
	{
		fillYUV(x, pRGB, SrcFormat, pY, pU, pV);
	}
}

static inline void general_RGBToYUV444_SINGLE_ROW(const BYTE* WINPR_RESTRICT pRGB, UINT32 SrcFormat,
                                                  BYTE* WINPR_RESTRICT pY, BYTE* WINPR_RESTRICT pU,
                                                  BYTE* WINPR_RESTRICT pV, UINT32 nWidth)
{

	WINPR_ASSERT((nWidth % 2) == 0);
	for (size_t x = 0; x < nWidth; x += 2)
	{
		fillYUV_single(x, pRGB, SrcFormat, pY, pU, pV);
	}
}

static inline pstatus_t general_RGBToYUV444_8u_P3AC4R_RGB(const BYTE* WINPR_RESTRICT pSrc,
                                                          UINT32 SrcFormat, const UINT32 srcStep,
                                                          BYTE* WINPR_RESTRICT pDst[3],
                                                          const UINT32 dstStep[3],
                                                          const prim_size_t* WINPR_RESTRICT roi)
{
	const UINT32 nWidth = roi->width;
	const UINT32 nHeight = roi->height;

	size_t y = 0;
	for (; y < nHeight - nHeight % 2; y += 2)
	{
		const BYTE* pRGB[] = { pSrc + y * srcStep, pSrc + (y + 1) * srcStep };
		BYTE* pY[] = { &pDst[0][y * dstStep[0]], &pDst[0][(y + 1) * dstStep[0]] };
		BYTE* pU[] = { &pDst[1][y * dstStep[1]], &pDst[1][(y + 1) * dstStep[1]] };
		BYTE* pV[] = { &pDst[2][y * dstStep[2]], &pDst[2][(y + 1) * dstStep[2]] };

		general_RGBToYUV444_DOUBLE_ROW(pRGB, SrcFormat, pY, pU, pV, nWidth);
	}
	for (; y < nHeight; y++)
	{
		const BYTE* pRGB = pSrc + y * srcStep;
		BYTE* pY = &pDst[0][y * dstStep[0]];
		BYTE* pU = &pDst[1][y * dstStep[1]];
		BYTE* pV = &pDst[2][y * dstStep[2]];

		general_RGBToYUV444_SINGLE_ROW(pRGB, SrcFormat, pY, pU, pV, nWidth);
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t general_RGBToYUV444_8u_P3AC4R(const BYTE* WINPR_RESTRICT pSrc, UINT32 SrcFormat,
                                               const UINT32 srcStep, BYTE* WINPR_RESTRICT pDst[3],
                                               const UINT32 dstStep[3],
                                               const prim_size_t* WINPR_RESTRICT roi)
{
	switch (SrcFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return general_RGBToYUV444_8u_P3AC4R_BGRX(pSrc, srcStep, pDst, dstStep, roi);
		default:
			return general_RGBToYUV444_8u_P3AC4R_RGB(pSrc, SrcFormat, srcStep, pDst, dstStep, roi);
	}
}

static inline pstatus_t general_RGBToYUV420_BGRX(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcStep,
                                                 BYTE* WINPR_RESTRICT pDst[3],
                                                 const UINT32 dstStep[3],
                                                 const prim_size_t* WINPR_RESTRICT roi)
{
	size_t x1 = 0;
	size_t x2 = 4;
	size_t x3 = srcStep;
	size_t x4 = srcStep + 4;
	size_t y1 = 0;
	size_t y2 = 1;
	size_t y3 = dstStep[0];
	size_t y4 = dstStep[0] + 1;
	UINT32 max_x = roi->width - 1;

	size_t y = 0;
	for (size_t i = 0; y < roi->height - roi->height % 2; y += 2, i++)
	{
		const BYTE* src = pSrc + y * srcStep;
		BYTE* ydst = pDst[0] + y * dstStep[0];
		BYTE* udst = pDst[1] + i * dstStep[1];
		BYTE* vdst = pDst[2] + i * dstStep[2];

		for (size_t x = 0; x < roi->width; x += 2)
		{
			BYTE R = 0;
			BYTE G = 0;
			BYTE B = 0;
			INT32 Ra = 0;
			INT32 Ga = 0;
			INT32 Ba = 0;
			/* row 1, pixel 1 */
			Ba = B = *(src + x1 + 0);
			Ga = G = *(src + x1 + 1);
			Ra = R = *(src + x1 + 2);
			ydst[y1] = RGB2Y(R, G, B);

			if (x < max_x)
			{
				/* row 1, pixel 2 */
				Ba += B = *(src + x2 + 0);
				Ga += G = *(src + x2 + 1);
				Ra += R = *(src + x2 + 2);
				ydst[y2] = RGB2Y(R, G, B);
			}

			/* row 2, pixel 1 */
			Ba += B = *(src + x3 + 0);
			Ga += G = *(src + x3 + 1);
			Ra += R = *(src + x3 + 2);
			ydst[y3] = RGB2Y(R, G, B);

			if (x < max_x)
			{
				/* row 2, pixel 2 */
				Ba += B = *(src + x4 + 0);
				Ga += G = *(src + x4 + 1);
				Ra += R = *(src + x4 + 2);
				ydst[y4] = RGB2Y(R, G, B);
			}

			Ba >>= 2;
			Ga >>= 2;
			Ra >>= 2;
			*udst++ = RGB2U(Ra, Ga, Ba);
			*vdst++ = RGB2V(Ra, Ga, Ba);
			ydst += 2;
			src += 8;
		}
	}

	for (; y < roi->height; y++)
	{
		const BYTE* src = pSrc + y * srcStep;
		BYTE* ydst = pDst[0] + y * dstStep[0];
		BYTE* udst = pDst[1] + (y / 2) * dstStep[1];
		BYTE* vdst = pDst[2] + (y / 2) * dstStep[2];

		for (size_t x = 0; x < roi->width; x += 2)
		{
			BYTE R = 0;
			BYTE G = 0;
			BYTE B = 0;
			INT32 Ra = 0;
			INT32 Ga = 0;
			INT32 Ba = 0;
			/* row 1, pixel 1 */
			Ba = B = *(src + x1 + 0);
			Ga = G = *(src + x1 + 1);
			Ra = R = *(src + x1 + 2);
			ydst[y1] = RGB2Y(R, G, B);

			if (x < max_x)
			{
				/* row 1, pixel 2 */
				Ba += B = *(src + x2 + 0);
				Ga += G = *(src + x2 + 1);
				Ra += R = *(src + x2 + 2);
				ydst[y2] = RGB2Y(R, G, B);
			}

			Ba >>= 2;
			Ga >>= 2;
			Ra >>= 2;
			*udst++ = RGB2U(Ra, Ga, Ba);
			*vdst++ = RGB2V(Ra, Ga, Ba);
			ydst += 2;
			src += 8;
		}
	}

	return PRIMITIVES_SUCCESS;
}

static inline pstatus_t general_RGBToYUV420_RGBX(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcStep,
                                                 BYTE* WINPR_RESTRICT pDst[3],
                                                 const UINT32 dstStep[3],
                                                 const prim_size_t* WINPR_RESTRICT roi)
{
	size_t x1 = 0;
	size_t x2 = 4;
	size_t x3 = srcStep;
	size_t x4 = srcStep + 4;
	size_t y1 = 0;
	size_t y2 = 1;
	size_t y3 = dstStep[0];
	size_t y4 = dstStep[0] + 1;
	UINT32 max_x = roi->width - 1;

	size_t y = 0;
	for (size_t i = 0; y < roi->height - roi->height % 2; y += 2, i++)
	{
		const BYTE* src = pSrc + y * srcStep;
		BYTE* ydst = pDst[0] + y * dstStep[0];
		BYTE* udst = pDst[1] + i * dstStep[1];
		BYTE* vdst = pDst[2] + i * dstStep[2];

		for (UINT32 x = 0; x < roi->width; x += 2)
		{
			BYTE R = *(src + x1 + 0);
			BYTE G = *(src + x1 + 1);
			BYTE B = *(src + x1 + 2);
			/* row 1, pixel 1 */
			INT32 Ra = R;
			INT32 Ga = G;
			INT32 Ba = B;
			ydst[y1] = RGB2Y(R, G, B);

			if (x < max_x)
			{
				/* row 1, pixel 2 */
				R = *(src + x2 + 0);
				G = *(src + x2 + 1);
				B = *(src + x2 + 2);
				Ra += R;
				Ga += G;
				Ba += B;
				ydst[y2] = RGB2Y(R, G, B);
			}

			/* row 2, pixel 1 */
			R = *(src + x3 + 0);
			G = *(src + x3 + 1);
			B = *(src + x3 + 2);

			Ra += R;
			Ga += G;
			Ba += B;
			ydst[y3] = RGB2Y(R, G, B);

			if (x < max_x)
			{
				/* row 2, pixel 2 */
				R = *(src + x4 + 0);
				G = *(src + x4 + 1);
				B = *(src + x4 + 2);

				Ra += R;
				Ga += G;
				Ba += B;
				ydst[y4] = RGB2Y(R, G, B);
			}

			Ba >>= 2;
			Ga >>= 2;
			Ra >>= 2;
			*udst++ = RGB2U(Ra, Ga, Ba);
			*vdst++ = RGB2V(Ra, Ga, Ba);
			ydst += 2;
			src += 8;
		}
	}

	for (; y < roi->height; y++)
	{
		const BYTE* src = pSrc + y * srcStep;
		BYTE* ydst = pDst[0] + y * dstStep[0];
		BYTE* udst = pDst[1] + (y / 2) * dstStep[1];
		BYTE* vdst = pDst[2] + (y / 2) * dstStep[2];

		for (UINT32 x = 0; x < roi->width; x += 2)
		{
			BYTE R = *(src + x1 + 0);
			BYTE G = *(src + x1 + 1);
			BYTE B = *(src + x1 + 2);
			/* row 1, pixel 1 */
			INT32 Ra = R;
			INT32 Ga = G;
			INT32 Ba = B;
			ydst[y1] = RGB2Y(R, G, B);

			if (x < max_x)
			{
				/* row 1, pixel 2 */
				R = *(src + x2 + 0);
				G = *(src + x2 + 1);
				B = *(src + x2 + 2);
				Ra += R;
				Ga += G;
				Ba += B;
				ydst[y2] = RGB2Y(R, G, B);
			}

			Ba >>= 2;
			Ga >>= 2;
			Ra >>= 2;
			*udst++ = RGB2U(Ra, Ga, Ba);
			*vdst++ = RGB2V(Ra, Ga, Ba);
			ydst += 2;
			src += 8;
		}
	}

	return PRIMITIVES_SUCCESS;
}

static inline pstatus_t general_RGBToYUV420_ANY(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcFormat,
                                                UINT32 srcStep, BYTE* WINPR_RESTRICT pDst[3],
                                                const UINT32 dstStep[3],
                                                const prim_size_t* WINPR_RESTRICT roi)
{
	const UINT32 bpp = FreeRDPGetBytesPerPixel(srcFormat);
	size_t x1 = 0;
	size_t x2 = bpp;
	size_t x3 = srcStep;
	size_t x4 = srcStep + bpp;
	size_t y1 = 0;
	size_t y2 = 1;
	size_t y3 = dstStep[0];
	size_t y4 = dstStep[0] + 1;
	UINT32 max_x = roi->width - 1;

	size_t y = 0;
	for (size_t i = 0; y < roi->height - roi->height % 2; y += 2, i++)
	{
		const BYTE* src = pSrc + y * srcStep;
		BYTE* ydst = pDst[0] + y * dstStep[0];
		BYTE* udst = pDst[1] + i * dstStep[1];
		BYTE* vdst = pDst[2] + i * dstStep[2];

		for (size_t x = 0; x < roi->width; x += 2)
		{
			BYTE R = 0;
			BYTE G = 0;
			BYTE B = 0;
			INT32 Ra = 0;
			INT32 Ga = 0;
			INT32 Ba = 0;
			UINT32 color = 0;
			/* row 1, pixel 1 */
			color = FreeRDPReadColor(src + x1, srcFormat);
			FreeRDPSplitColor(color, srcFormat, &R, &G, &B, NULL, NULL);
			Ra = R;
			Ga = G;
			Ba = B;
			ydst[y1] = RGB2Y(R, G, B);

			if (x < max_x)
			{
				/* row 1, pixel 2 */
				color = FreeRDPReadColor(src + x2, srcFormat);
				FreeRDPSplitColor(color, srcFormat, &R, &G, &B, NULL, NULL);
				Ra += R;
				Ga += G;
				Ba += B;
				ydst[y2] = RGB2Y(R, G, B);
			}

			/* row 2, pixel 1 */
			color = FreeRDPReadColor(src + x3, srcFormat);
			FreeRDPSplitColor(color, srcFormat, &R, &G, &B, NULL, NULL);
			Ra += R;
			Ga += G;
			Ba += B;
			ydst[y3] = RGB2Y(R, G, B);

			if (x < max_x)
			{
				/* row 2, pixel 2 */
				color = FreeRDPReadColor(src + x4, srcFormat);
				FreeRDPSplitColor(color, srcFormat, &R, &G, &B, NULL, NULL);
				Ra += R;
				Ga += G;
				Ba += B;
				ydst[y4] = RGB2Y(R, G, B);
			}

			Ra >>= 2;
			Ga >>= 2;
			Ba >>= 2;
			*udst++ = RGB2U(Ra, Ga, Ba);
			*vdst++ = RGB2V(Ra, Ga, Ba);
			ydst += 2;
			src += 2ULL * bpp;
		}
	}

	for (; y < roi->height; y++)
	{
		const BYTE* src = pSrc + y * srcStep;
		BYTE* ydst = pDst[0] + y * dstStep[0];
		BYTE* udst = pDst[1] + (y / 2) * dstStep[1];
		BYTE* vdst = pDst[2] + (y / 2) * dstStep[2];

		for (size_t x = 0; x < roi->width; x += 2)
		{
			BYTE R = 0;
			BYTE G = 0;
			BYTE B = 0;
			/* row 1, pixel 1 */
			UINT32 color = FreeRDPReadColor(src + x1, srcFormat);
			FreeRDPSplitColor(color, srcFormat, &R, &G, &B, NULL, NULL);
			INT32 Ra = R;
			INT32 Ga = G;
			INT32 Ba = B;
			ydst[y1] = RGB2Y(R, G, B);

			if (x < max_x)
			{
				/* row 1, pixel 2 */
				color = FreeRDPReadColor(src + x2, srcFormat);
				FreeRDPSplitColor(color, srcFormat, &R, &G, &B, NULL, NULL);
				Ra += R;
				Ga += G;
				Ba += B;
				ydst[y2] = RGB2Y(R, G, B);
			}

			Ra >>= 2;
			Ga >>= 2;
			Ba >>= 2;
			*udst++ = RGB2U(Ra, Ga, Ba);
			*vdst++ = RGB2V(Ra, Ga, Ba);
			ydst += 2;
			src += 2ULL * bpp;
		}
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t general_RGBToYUV420_8u_P3AC4R(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcFormat,
                                               UINT32 srcStep, BYTE* WINPR_RESTRICT pDst[3],
                                               const UINT32 dstStep[3],
                                               const prim_size_t* WINPR_RESTRICT roi)
{
	switch (srcFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return general_RGBToYUV420_BGRX(pSrc, srcStep, pDst, dstStep, roi);

		case PIXEL_FORMAT_RGBA32:
		case PIXEL_FORMAT_RGBX32:
			return general_RGBToYUV420_RGBX(pSrc, srcStep, pDst, dstStep, roi);

		default:
			return general_RGBToYUV420_ANY(pSrc, srcFormat, srcStep, pDst, dstStep, roi);
	}
}

static inline void int_general_RGBToAVC444YUV_BGRX_DOUBLE_ROW(
    size_t offset, const BYTE* WINPR_RESTRICT pSrcEven, const BYTE* WINPR_RESTRICT pSrcOdd,
    BYTE* WINPR_RESTRICT b1Even, BYTE* WINPR_RESTRICT b1Odd, BYTE* WINPR_RESTRICT b2,
    BYTE* WINPR_RESTRICT b3, BYTE* WINPR_RESTRICT b4, BYTE* WINPR_RESTRICT b5,
    BYTE* WINPR_RESTRICT b6, BYTE* WINPR_RESTRICT b7, UINT32 width)
{
	WINPR_ASSERT((width % 2) == 0);
	for (size_t x = offset; x < width; x += 2)
	{
		const BYTE* srcEven = &pSrcEven[4ULL * x];
		const BYTE* srcOdd = &pSrcOdd[4ULL * x];
		const BOOL lastX = (x + 1) >= width;
		BYTE Y1e = 0;
		BYTE Y2e = 0;
		BYTE U1e = 0;
		BYTE V1e = 0;
		BYTE U2e = 0;
		BYTE V2e = 0;
		BYTE Y1o = 0;
		BYTE Y2o = 0;
		BYTE U1o = 0;
		BYTE V1o = 0;
		BYTE U2o = 0;
		BYTE V2o = 0;
		/* Read 4 pixels, 2 from even, 2 from odd lines */
		{
			const BYTE b = *srcEven++;
			const BYTE g = *srcEven++;
			const BYTE r = *srcEven++;
			srcEven++;
			Y1e = Y2e = Y1o = Y2o = RGB2Y(r, g, b);
			U1e = U2e = U1o = U2o = RGB2U(r, g, b);
			V1e = V2e = V1o = V2o = RGB2V(r, g, b);
		}

		if (!lastX)
		{
			const BYTE b = *srcEven++;
			const BYTE g = *srcEven++;
			const BYTE r = *srcEven++;
			srcEven++;
			Y2e = RGB2Y(r, g, b);
			U2e = RGB2U(r, g, b);
			V2e = RGB2V(r, g, b);
		}

		if (b1Odd)
		{
			const BYTE b = *srcOdd++;
			const BYTE g = *srcOdd++;
			const BYTE r = *srcOdd++;
			srcOdd++;
			Y1o = Y2o = RGB2Y(r, g, b);
			U1o = U2o = RGB2U(r, g, b);
			V1o = V2o = RGB2V(r, g, b);
		}

		if (b1Odd && !lastX)
		{
			const BYTE b = *srcOdd++;
			const BYTE g = *srcOdd++;
			const BYTE r = *srcOdd++;
			srcOdd++;
			Y2o = RGB2Y(r, g, b);
			U2o = RGB2U(r, g, b);
			V2o = RGB2V(r, g, b);
		}

		/* We have 4 Y pixels, so store them. */
		*b1Even++ = Y1e;
		*b1Even++ = Y2e;

		if (b1Odd)
		{
			*b1Odd++ = Y1o;
			*b1Odd++ = Y2o;
		}

		/* 2x 2y pixel in luma UV plane use averaging
		 */
		{
			const BYTE Uavg = WINPR_ASSERTING_INT_CAST(BYTE, ((UINT16)U1e + U2e + U1o + U2o) / 4);
			const BYTE Vavg = WINPR_ASSERTING_INT_CAST(BYTE, ((UINT16)V1e + V2e + V1o + V2o) / 4);
			*b2++ = Uavg;
			*b3++ = Vavg;
		}

		/* UV from 2x, 2y+1 */
		if (b1Odd)
		{
			*b4++ = U1o;
			*b5++ = V1o;

			if (!lastX)
			{
				*b4++ = U2o;
				*b5++ = V2o;
			}
		}

		/* UV from 2x+1, 2y */
		if (!lastX)
		{
			*b6++ = U2e;
			*b7++ = V2e;
		}
	}
}

void general_RGBToAVC444YUV_BGRX_DOUBLE_ROW(size_t offset, const BYTE* WINPR_RESTRICT pSrcEven,
                                            const BYTE* WINPR_RESTRICT pSrcOdd,
                                            BYTE* WINPR_RESTRICT b1Even, BYTE* WINPR_RESTRICT b1Odd,
                                            BYTE* WINPR_RESTRICT b2, BYTE* WINPR_RESTRICT b3,
                                            BYTE* WINPR_RESTRICT b4, BYTE* WINPR_RESTRICT b5,
                                            BYTE* WINPR_RESTRICT b6, BYTE* WINPR_RESTRICT b7,
                                            UINT32 width)
{
	int_general_RGBToAVC444YUV_BGRX_DOUBLE_ROW(offset, pSrcEven, pSrcOdd, b1Even, b1Odd, b2, b3, b4,
	                                           b5, b6, b7, width);
}

static inline pstatus_t general_RGBToAVC444YUV_BGRX(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcStep,
                                                    BYTE* WINPR_RESTRICT pDst1[3],
                                                    const UINT32 dst1Step[3],
                                                    BYTE* WINPR_RESTRICT pDst2[3],
                                                    const UINT32 dst2Step[3],
                                                    const prim_size_t* WINPR_RESTRICT roi)
{
	/**
	 * Note:
	 * Read information in function general_RGBToAVC444YUV_ANY below !
	 */
	size_t y = 0;
	for (; y < roi->height - roi->height % 2; y += 2)
	{
		const BYTE* srcEven = pSrc + 1ULL * y * srcStep;
		const BYTE* srcOdd = pSrc + 1ULL * (y + 1) * srcStep;
		const size_t i = y >> 1;
		const size_t n = (i & (uint32_t)~7) + i;
		BYTE* b1Even = pDst1[0] + 1ULL * y * dst1Step[0];
		BYTE* b1Odd = (b1Even + dst1Step[0]);
		BYTE* b2 = pDst1[1] + 1ULL * (y / 2) * dst1Step[1];
		BYTE* b3 = pDst1[2] + 1ULL * (y / 2) * dst1Step[2];
		BYTE* b4 = pDst2[0] + 1ULL * dst2Step[0] * n;
		BYTE* b5 = b4 + 8ULL * dst2Step[0];
		BYTE* b6 = pDst2[1] + 1ULL * (y / 2) * dst2Step[1];
		BYTE* b7 = pDst2[2] + 1ULL * (y / 2) * dst2Step[2];
		int_general_RGBToAVC444YUV_BGRX_DOUBLE_ROW(0, srcEven, srcOdd, b1Even, b1Odd, b2, b3, b4,
		                                           b5, b6, b7, roi->width);
	}
	for (; y < roi->height; y++)
	{
		const BYTE* srcEven = pSrc + 1ULL * y * srcStep;
		BYTE* b1Even = pDst1[0] + 1ULL * y * dst1Step[0];
		BYTE* b2 = pDst1[1] + 1ULL * (y / 2) * dst1Step[1];
		BYTE* b3 = pDst1[2] + 1ULL * (y / 2) * dst1Step[2];
		BYTE* b6 = pDst2[1] + 1ULL * (y / 2) * dst2Step[1];
		BYTE* b7 = pDst2[2] + 1ULL * (y / 2) * dst2Step[2];
		int_general_RGBToAVC444YUV_BGRX_DOUBLE_ROW(0, srcEven, NULL, b1Even, NULL, b2, b3, NULL,
		                                           NULL, b6, b7, roi->width);
	}

	return PRIMITIVES_SUCCESS;
}

static inline void general_RGBToAVC444YUV_RGBX_DOUBLE_ROW(
    const BYTE* WINPR_RESTRICT srcEven, const BYTE* WINPR_RESTRICT srcOdd,
    BYTE* WINPR_RESTRICT b1Even, BYTE* WINPR_RESTRICT b1Odd, BYTE* WINPR_RESTRICT b2,
    BYTE* WINPR_RESTRICT b3, BYTE* WINPR_RESTRICT b4, BYTE* WINPR_RESTRICT b5,
    BYTE* WINPR_RESTRICT b6, BYTE* WINPR_RESTRICT b7, UINT32 width)
{
	WINPR_ASSERT((width % 2) == 0);
	for (UINT32 x = 0; x < width; x += 2)
	{
		const BOOL lastX = (x + 1) >= width;
		BYTE Y1e = 0;
		BYTE Y2e = 0;
		BYTE U1e = 0;
		BYTE V1e = 0;
		BYTE U2e = 0;
		BYTE V2e = 0;
		BYTE Y1o = 0;
		BYTE Y2o = 0;
		BYTE U1o = 0;
		BYTE V1o = 0;
		BYTE U2o = 0;
		BYTE V2o = 0;
		/* Read 4 pixels, 2 from even, 2 from odd lines */
		{
			const BYTE r = *srcEven++;
			const BYTE g = *srcEven++;
			const BYTE b = *srcEven++;
			srcEven++;
			Y1e = Y2e = Y1o = Y2o = RGB2Y(r, g, b);
			U1e = U2e = U1o = U2o = RGB2U(r, g, b);
			V1e = V2e = V1o = V2o = RGB2V(r, g, b);
		}

		if (!lastX)
		{
			const BYTE r = *srcEven++;
			const BYTE g = *srcEven++;
			const BYTE b = *srcEven++;
			srcEven++;
			Y2e = RGB2Y(r, g, b);
			U2e = RGB2U(r, g, b);
			V2e = RGB2V(r, g, b);
		}

		if (b1Odd)
		{
			const BYTE r = *srcOdd++;
			const BYTE g = *srcOdd++;
			const BYTE b = *srcOdd++;
			srcOdd++;
			Y1o = Y2o = RGB2Y(r, g, b);
			U1o = U2o = RGB2U(r, g, b);
			V1o = V2o = RGB2V(r, g, b);
		}

		if (b1Odd && !lastX)
		{
			const BYTE r = *srcOdd++;
			const BYTE g = *srcOdd++;
			const BYTE b = *srcOdd++;
			srcOdd++;
			Y2o = RGB2Y(r, g, b);
			U2o = RGB2U(r, g, b);
			V2o = RGB2V(r, g, b);
		}

		/* We have 4 Y pixels, so store them. */
		*b1Even++ = Y1e;
		*b1Even++ = Y2e;

		if (b1Odd)
		{
			*b1Odd++ = Y1o;
			*b1Odd++ = Y2o;
		}

		/* 2x 2y pixel in luma UV plane use averaging
		 */
		{
			const BYTE Uavg = WINPR_ASSERTING_INT_CAST(BYTE, ((UINT16)U1e + U2e + U1o + U2o) / 4);
			const BYTE Vavg = WINPR_ASSERTING_INT_CAST(BYTE, ((UINT16)V1e + V2e + V1o + V2o) / 4);
			*b2++ = Uavg;
			*b3++ = Vavg;
		}

		/* UV from 2x, 2y+1 */
		if (b1Odd)
		{
			*b4++ = U1o;
			*b5++ = V1o;

			if (!lastX)
			{
				*b4++ = U2o;
				*b5++ = V2o;
			}
		}

		/* UV from 2x+1, 2y */
		if (!lastX)
		{
			*b6++ = U2e;
			*b7++ = V2e;
		}
	}
}

static inline pstatus_t general_RGBToAVC444YUV_RGBX(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcStep,
                                                    BYTE* WINPR_RESTRICT pDst1[3],
                                                    const UINT32 dst1Step[3],
                                                    BYTE* WINPR_RESTRICT pDst2[3],
                                                    const UINT32 dst2Step[3],
                                                    const prim_size_t* WINPR_RESTRICT roi)
{
	/**
	 * Note:
	 * Read information in function general_RGBToAVC444YUV_ANY below !
	 */

	size_t y = 0;
	for (; y < roi->height - roi->height % 2; y += 2)
	{
		const BOOL last = (y >= (roi->height - 1));
		const BYTE* srcEven = pSrc + 1ULL * y * srcStep;
		const BYTE* srcOdd = pSrc + 1ULL * (y + 1) * srcStep;
		const size_t i = y >> 1;
		const size_t n = (i & (size_t)~7) + i;
		BYTE* b1Even = pDst1[0] + 1ULL * y * dst1Step[0];
		BYTE* b1Odd = !last ? (b1Even + dst1Step[0]) : NULL;
		BYTE* b2 = pDst1[1] + 1ULL * (y / 2) * dst1Step[1];
		BYTE* b3 = pDst1[2] + 1ULL * (y / 2) * dst1Step[2];
		BYTE* b4 = pDst2[0] + 1ULL * dst2Step[0] * n;
		BYTE* b5 = b4 + 8ULL * dst2Step[0];
		BYTE* b6 = pDst2[1] + 1ULL * (y / 2) * dst2Step[1];
		BYTE* b7 = pDst2[2] + 1ULL * (y / 2) * dst2Step[2];
		general_RGBToAVC444YUV_RGBX_DOUBLE_ROW(srcEven, srcOdd, b1Even, b1Odd, b2, b3, b4, b5, b6,
		                                       b7, roi->width);
	}
	for (; y < roi->height; y++)
	{
		const BYTE* srcEven = pSrc + 1ULL * y * srcStep;
		BYTE* b1Even = pDst1[0] + 1ULL * y * dst1Step[0];
		BYTE* b2 = pDst1[1] + 1ULL * (y / 2) * dst1Step[1];
		BYTE* b3 = pDst1[2] + 1ULL * (y / 2) * dst1Step[2];
		BYTE* b6 = pDst2[1] + 1ULL * (y / 2) * dst2Step[1];
		BYTE* b7 = pDst2[2] + 1ULL * (y / 2) * dst2Step[2];
		general_RGBToAVC444YUV_RGBX_DOUBLE_ROW(srcEven, NULL, b1Even, NULL, b2, b3, NULL, NULL, b6,
		                                       b7, roi->width);
	}
	return PRIMITIVES_SUCCESS;
}

static inline void general_RGBToAVC444YUV_ANY_DOUBLE_ROW(
    const BYTE* WINPR_RESTRICT srcEven, const BYTE* WINPR_RESTRICT srcOdd, UINT32 srcFormat,
    BYTE* WINPR_RESTRICT b1Even, BYTE* WINPR_RESTRICT b1Odd, BYTE* WINPR_RESTRICT b2,
    BYTE* WINPR_RESTRICT b3, BYTE* WINPR_RESTRICT b4, BYTE* WINPR_RESTRICT b5,
    BYTE* WINPR_RESTRICT b6, BYTE* WINPR_RESTRICT b7, UINT32 width)
{
	const UINT32 bpp = FreeRDPGetBytesPerPixel(srcFormat);
	for (UINT32 x = 0; x < width; x += 2)
	{
		const BOOL lastX = (x + 1) >= width;
		BYTE Y1e = 0;
		BYTE Y2e = 0;
		BYTE U1e = 0;
		BYTE V1e = 0;
		BYTE U2e = 0;
		BYTE V2e = 0;
		BYTE Y1o = 0;
		BYTE Y2o = 0;
		BYTE U1o = 0;
		BYTE V1o = 0;
		BYTE U2o = 0;
		BYTE V2o = 0;
		/* Read 4 pixels, 2 from even, 2 from odd lines */
		{
			BYTE r = 0;
			BYTE g = 0;
			BYTE b = 0;
			const UINT32 color = FreeRDPReadColor(srcEven, srcFormat);
			srcEven += bpp;
			FreeRDPSplitColor(color, srcFormat, &r, &g, &b, NULL, NULL);
			Y1e = Y2e = Y1o = Y2o = RGB2Y(r, g, b);
			U1e = U2e = U1o = U2o = RGB2U(r, g, b);
			V1e = V2e = V1o = V2o = RGB2V(r, g, b);
		}

		if (!lastX)
		{
			BYTE r = 0;
			BYTE g = 0;
			BYTE b = 0;
			const UINT32 color = FreeRDPReadColor(srcEven, srcFormat);
			srcEven += bpp;
			FreeRDPSplitColor(color, srcFormat, &r, &g, &b, NULL, NULL);
			Y2e = RGB2Y(r, g, b);
			U2e = RGB2U(r, g, b);
			V2e = RGB2V(r, g, b);
		}

		if (b1Odd)
		{
			BYTE r = 0;
			BYTE g = 0;
			BYTE b = 0;
			const UINT32 color = FreeRDPReadColor(srcOdd, srcFormat);
			srcOdd += bpp;
			FreeRDPSplitColor(color, srcFormat, &r, &g, &b, NULL, NULL);
			Y1o = Y2o = RGB2Y(r, g, b);
			U1o = U2o = RGB2U(r, g, b);
			V1o = V2o = RGB2V(r, g, b);
		}

		if (b1Odd && !lastX)
		{
			BYTE r = 0;
			BYTE g = 0;
			BYTE b = 0;
			const UINT32 color = FreeRDPReadColor(srcOdd, srcFormat);
			srcOdd += bpp;
			FreeRDPSplitColor(color, srcFormat, &r, &g, &b, NULL, NULL);
			Y2o = RGB2Y(r, g, b);
			U2o = RGB2U(r, g, b);
			V2o = RGB2V(r, g, b);
		}

		/* We have 4 Y pixels, so store them. */
		*b1Even++ = Y1e;
		*b1Even++ = Y2e;

		if (b1Odd)
		{
			*b1Odd++ = Y1o;
			*b1Odd++ = Y2o;
		}

		/* 2x 2y pixel in luma UV plane use averaging
		 */
		{
			const BYTE Uavg = WINPR_ASSERTING_INT_CAST(
			    BYTE, ((UINT16)U1e + (UINT16)U2e + (UINT16)U1o + (UINT16)U2o) / 4);
			const BYTE Vavg = WINPR_ASSERTING_INT_CAST(
			    BYTE, ((UINT16)V1e + (UINT16)V2e + (UINT16)V1o + (UINT16)V2o) / 4);
			*b2++ = Uavg;
			*b3++ = Vavg;
		}

		/* UV from 2x, 2y+1 */
		if (b1Odd)
		{
			*b4++ = U1o;
			*b5++ = V1o;

			if (!lastX)
			{
				*b4++ = U2o;
				*b5++ = V2o;
			}
		}

		/* UV from 2x+1, 2y */
		if (!lastX)
		{
			*b6++ = U2e;
			*b7++ = V2e;
		}
	}
}

static inline pstatus_t
general_RGBToAVC444YUV_ANY(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcFormat, UINT32 srcStep,
                           BYTE* WINPR_RESTRICT pDst1[3], const UINT32 dst1Step[3],
                           BYTE* WINPR_RESTRICT pDst2[3], const UINT32 dst2Step[3],
                           const prim_size_t* WINPR_RESTRICT roi)
{
	/**
	 * Note: According to [MS-RDPEGFX 2.2.4.4 RFX_AVC420_BITMAP_STREAM] the
	 * width and height of the MPEG-4 AVC/H.264 codec bitstream MUST be aligned
	 * to a multiple of 16.
	 * Hence the passed destination YUV420/CHROMA420 buffers must have been
	 * allocated accordingly !!
	 */
	/**
	 * [MS-RDPEGFX 3.3.8.3.2 YUV420p Stream Combination] defines the following "Bx areas":
	 *
	 * YUV420 frame (main view):
	 * B1:  From Y444 all pixels
	 * B2:  From U444 all pixels in even rows with even columns
	 * B3:  From V444 all pixels in even rows with even columns
	 *
	 * Chroma420 frame (auxiliary view):
	 * B45: From U444 and V444 all pixels from all odd rows
	 *      (The odd U444 and V444 rows must be interleaved in 8-line blocks in B45 !!!)
	 * B6:  From U444 all pixels in even rows with odd columns
	 * B7:  From V444 all pixels in even rows with odd columns
	 *
	 * Microsoft's horrible unclear description in MS-RDPEGFX translated to pseudo code looks like
	 * this:
	 *
	 * for (y = 0; y < fullHeight; y++)
	 * {
	 *     for (x = 0; x < fullWidth; x++)
	 *     {
	 *         B1[x,y] = Y444[x,y];
	 *     }
	 *  }
	 *
	 * for (y = 0; y < halfHeight; y++)
	 * {
	 *     for (x = 0; x < halfWidth; x++)
	 *     {
	 *         B2[x,y] = U444[2 * x,     2 * y];
	 *         B3[x,y] = V444[2 * x,     2 * y];
	 *         B6[x,y] = U444[2 * x + 1, 2 * y];
	 *     	   B7[x,y] = V444[2 * x + 1, 2 * y];
	 *     }
	 *  }
	 *
	 * for (y = 0; y < halfHeight; y++)
	 * {
	 *     yU  = (y / 8) * 16;   // identify first row of correct 8-line U block in B45
	 *     yU += (y % 8);        // add offset rows in destination block
	 *     yV  = yU + 8;         // the corresponding v line is always 8 rows ahead
	 *
	 *     for (x = 0; x < fullWidth; x++)
	 *     {
	 *         B45[x,yU] = U444[x, 2 * y + 1];
	 *         B45[x,yV] = V444[x, 2 * y + 1];
	 *     }
	 *  }
	 *
	 */
	const BYTE* pMaxSrc = pSrc + 1ULL * (roi->height - 1) * srcStep;

	for (size_t y = 0; y < roi->height; y += 2)
	{
		WINPR_ASSERT(y < UINT32_MAX);

		const BOOL last = (y >= (roi->height - 1));
		const BYTE* srcEven = y < roi->height ? pSrc + y * srcStep : pMaxSrc;
		const BYTE* srcOdd = !last ? pSrc + (y + 1) * srcStep : pMaxSrc;
		const UINT32 i = (UINT32)y >> 1;
		const UINT32 n = (i & (uint32_t)~7) + i;
		BYTE* b1Even = pDst1[0] + y * dst1Step[0];
		BYTE* b1Odd = !last ? (b1Even + dst1Step[0]) : NULL;
		BYTE* b2 = pDst1[1] + (y / 2) * dst1Step[1];
		BYTE* b3 = pDst1[2] + (y / 2) * dst1Step[2];
		BYTE* b4 = pDst2[0] + 1ULL * dst2Step[0] * n;
		BYTE* b5 = b4 + 8ULL * dst2Step[0];
		BYTE* b6 = pDst2[1] + (y / 2) * dst2Step[1];
		BYTE* b7 = pDst2[2] + (y / 2) * dst2Step[2];
		general_RGBToAVC444YUV_ANY_DOUBLE_ROW(srcEven, srcOdd, srcFormat, b1Even, b1Odd, b2, b3, b4,
		                                      b5, b6, b7, roi->width);
	}

	return PRIMITIVES_SUCCESS;
}

static inline pstatus_t general_RGBToAVC444YUV(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcFormat,
                                               UINT32 srcStep, BYTE* WINPR_RESTRICT pDst1[3],
                                               const UINT32 dst1Step[3],
                                               BYTE* WINPR_RESTRICT pDst2[3],
                                               const UINT32 dst2Step[3],
                                               const prim_size_t* WINPR_RESTRICT roi)
{
	if (!pSrc || !pDst1 || !dst1Step || !pDst2 || !dst2Step)
		return -1;

	if (!pDst1[0] || !pDst1[1] || !pDst1[2])
		return -1;

	if (!dst1Step[0] || !dst1Step[1] || !dst1Step[2])
		return -1;

	if (!pDst2[0] || !pDst2[1] || !pDst2[2])
		return -1;

	if (!dst2Step[0] || !dst2Step[1] || !dst2Step[2])
		return -1;

	switch (srcFormat)
	{

		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return general_RGBToAVC444YUV_BGRX(pSrc, srcStep, pDst1, dst1Step, pDst2, dst2Step,
			                                   roi);

		case PIXEL_FORMAT_RGBA32:
		case PIXEL_FORMAT_RGBX32:
			return general_RGBToAVC444YUV_RGBX(pSrc, srcStep, pDst1, dst1Step, pDst2, dst2Step,
			                                   roi);

		default:
			return general_RGBToAVC444YUV_ANY(pSrc, srcFormat, srcStep, pDst1, dst1Step, pDst2,
			                                  dst2Step, roi);
	}

	return !PRIMITIVES_SUCCESS;
}

static inline void general_RGBToAVC444YUVv2_ANY_DOUBLE_ROW(
    const BYTE* WINPR_RESTRICT srcEven, const BYTE* WINPR_RESTRICT srcOdd, UINT32 srcFormat,
    BYTE* WINPR_RESTRICT yLumaDstEven, BYTE* WINPR_RESTRICT yLumaDstOdd,
    BYTE* WINPR_RESTRICT uLumaDst, BYTE* WINPR_RESTRICT vLumaDst,
    BYTE* WINPR_RESTRICT yEvenChromaDst1, BYTE* WINPR_RESTRICT yEvenChromaDst2,
    BYTE* WINPR_RESTRICT yOddChromaDst1, BYTE* WINPR_RESTRICT yOddChromaDst2,
    BYTE* WINPR_RESTRICT uChromaDst1, BYTE* WINPR_RESTRICT uChromaDst2,
    BYTE* WINPR_RESTRICT vChromaDst1, BYTE* WINPR_RESTRICT vChromaDst2, UINT32 width)
{
	const UINT32 bpp = FreeRDPGetBytesPerPixel(srcFormat);

	WINPR_ASSERT((width % 2) == 0);
	for (UINT32 x = 0; x < width; x += 2)
	{
		BYTE Ya = 0;
		BYTE Ua = 0;
		BYTE Va = 0;
		BYTE Yb = 0;
		BYTE Ub = 0;
		BYTE Vb = 0;
		BYTE Yc = 0;
		BYTE Uc = 0;
		BYTE Vc = 0;
		BYTE Yd = 0;
		BYTE Ud = 0;
		BYTE Vd = 0;
		{
			BYTE b = 0;
			BYTE g = 0;
			BYTE r = 0;
			const UINT32 color = FreeRDPReadColor(srcEven, srcFormat);
			srcEven += bpp;
			FreeRDPSplitColor(color, srcFormat, &r, &g, &b, NULL, NULL);
			Ya = RGB2Y(r, g, b);
			Ua = RGB2U(r, g, b);
			Va = RGB2V(r, g, b);
		}

		if (x < width - 1)
		{
			BYTE b = 0;
			BYTE g = 0;
			BYTE r = 0;
			const UINT32 color = FreeRDPReadColor(srcEven, srcFormat);
			srcEven += bpp;
			FreeRDPSplitColor(color, srcFormat, &r, &g, &b, NULL, NULL);
			Yb = RGB2Y(r, g, b);
			Ub = RGB2U(r, g, b);
			Vb = RGB2V(r, g, b);
		}
		else
		{
			Yb = Ya;
			Ub = Ua;
			Vb = Va;
		}

		if (srcOdd)
		{
			BYTE b = 0;
			BYTE g = 0;
			BYTE r = 0;
			const UINT32 color = FreeRDPReadColor(srcOdd, srcFormat);
			srcOdd += bpp;
			FreeRDPSplitColor(color, srcFormat, &r, &g, &b, NULL, NULL);
			Yc = RGB2Y(r, g, b);
			Uc = RGB2U(r, g, b);
			Vc = RGB2V(r, g, b);
		}
		else
		{
			Yc = Ya;
			Uc = Ua;
			Vc = Va;
		}

		if (srcOdd && (x < width - 1))
		{
			BYTE b = 0;
			BYTE g = 0;
			BYTE r = 0;
			const UINT32 color = FreeRDPReadColor(srcOdd, srcFormat);
			srcOdd += bpp;
			FreeRDPSplitColor(color, srcFormat, &r, &g, &b, NULL, NULL);
			Yd = RGB2Y(r, g, b);
			Ud = RGB2U(r, g, b);
			Vd = RGB2V(r, g, b);
		}
		else
		{
			Yd = Ya;
			Ud = Ua;
			Vd = Va;
		}

		/* Y [b1] */
		*yLumaDstEven++ = Ya;

		if (x < width - 1)
			*yLumaDstEven++ = Yb;

		if (srcOdd)
			*yLumaDstOdd++ = Yc;

		if (srcOdd && (x < width - 1))
			*yLumaDstOdd++ = Yd;

		/* 2x 2y [b2,b3] */
		*uLumaDst++ = (Ua + Ub + Uc + Ud) / 4;
		*vLumaDst++ = (Va + Vb + Vc + Vd) / 4;

		/* 2x+1, y [b4,b5] even */
		if (x < width - 1)
		{
			*yEvenChromaDst1++ = Ub;
			*yEvenChromaDst2++ = Vb;
		}

		if (srcOdd)
		{
			/* 2x+1, y [b4,b5] odd */
			if (x < width - 1)
			{
				*yOddChromaDst1++ = Ud;
				*yOddChromaDst2++ = Vd;
			}

			/* 4x 2y+1 [b6, b7] */
			if (x % 4 == 0)
			{
				*uChromaDst1++ = Uc;
				*uChromaDst2++ = Vc;
			}
			/* 4x+2 2y+1 [b8, b9] */
			else
			{
				*vChromaDst1++ = Uc;
				*vChromaDst2++ = Vc;
			}
		}
	}
}

static inline pstatus_t
general_RGBToAVC444YUVv2_ANY(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcFormat, UINT32 srcStep,
                             BYTE* WINPR_RESTRICT pDst1[3], const UINT32 dst1Step[3],
                             BYTE* WINPR_RESTRICT pDst2[3], const UINT32 dst2Step[3],
                             const prim_size_t* WINPR_RESTRICT roi)
{
	/**
	 * Note: According to [MS-RDPEGFX 2.2.4.4 RFX_AVC420_BITMAP_STREAM] the
	 * width and height of the MPEG-4 AVC/H.264 codec bitstream MUST be aligned
	 * to a multiple of 16.
	 * Hence the passed destination YUV420/CHROMA420 buffers must have been
	 * allocated accordingly !!
	 */
	/**
	 * [MS-RDPEGFX 3.3.8.3.3 YUV420p Stream Combination for YUV444v2 mode] defines the following "Bx
	 * areas":
	 *
	 * YUV420 frame (main view):
	 * B1:  From Y444 all pixels
	 * B2:  From U444 all pixels in even rows with even rows and columns
	 * B3:  From V444 all pixels in even rows with even rows and columns
	 *
	 * Chroma420 frame (auxiliary view):
	 * B45: From U444 and V444 all pixels from all odd columns
	 * B67: From U444 and V444 every 4th pixel in odd rows
	 * B89:  From U444 and V444 every 4th pixel (initial offset of 2) in odd rows
	 *
	 * Chroma Bxy areas correspond to the left and right half of the YUV420 plane.
	 * for (y = 0; y < fullHeight; y++)
	 * {
	 *     for (x = 0; x < fullWidth; x++)
	 *     {
	 *         B1[x,y] = Y444[x,y];
	 *     }
	 *
	 *     for (x = 0; x < halfWidth; x++)
	 *     {
	 *         B4[x,y] = U444[2 * x, 2 * y];
	 *         B5[x,y] = V444[2 * x, 2 * y];
	 *     }
	 *  }
	 *
	 * for (y = 0; y < halfHeight; y++)
	 * {
	 *     for (x = 0; x < halfWidth; x++)
	 *     {
	 *         B2[x,y] = U444[2 * x,     2 * y];
	 *         B3[x,y] = V444[2 * x,     2 * y];
	 *         B6[x,y] = U444[4 * x,     2 * y + 1];
	 *         B7[x,y] = V444[4 * x,     2 * y + 1];
	 *         B8[x,y] = V444[4 * x + 2, 2 * y + 1];
	 *         B9[x,y] = V444[4 * x + 2, 2 * y] + 1;
	 *     }
	 *  }
	 *
	 */
	if (roi->height < 1 || roi->width < 1)
		return !PRIMITIVES_SUCCESS;

	size_t y = 0;
	for (; y < roi->height - roi->height % 2; y += 2)
	{
		const BYTE* srcEven = (pSrc + y * srcStep);
		const BYTE* srcOdd = (y < roi->height - 1) ? (srcEven + srcStep) : NULL;
		BYTE* dstLumaYEven = (pDst1[0] + y * dst1Step[0]);
		BYTE* dstLumaYOdd = (dstLumaYEven + dst1Step[0]);
		BYTE* dstLumaU = (pDst1[1] + (y / 2) * dst1Step[1]);
		BYTE* dstLumaV = (pDst1[2] + (y / 2) * dst1Step[2]);
		BYTE* dstEvenChromaY1 = (pDst2[0] + y * dst2Step[0]);
		BYTE* dstEvenChromaY2 = dstEvenChromaY1 + roi->width / 2;
		BYTE* dstOddChromaY1 = dstEvenChromaY1 + dst2Step[0];
		BYTE* dstOddChromaY2 = dstEvenChromaY2 + dst2Step[0];
		BYTE* dstChromaU1 = (pDst2[1] + (y / 2) * dst2Step[1]);
		BYTE* dstChromaV1 = (pDst2[2] + (y / 2) * dst2Step[2]);
		BYTE* dstChromaU2 = dstChromaU1 + roi->width / 4;
		BYTE* dstChromaV2 = dstChromaV1 + roi->width / 4;
		general_RGBToAVC444YUVv2_ANY_DOUBLE_ROW(
		    srcEven, srcOdd, srcFormat, dstLumaYEven, dstLumaYOdd, dstLumaU, dstLumaV,
		    dstEvenChromaY1, dstEvenChromaY2, dstOddChromaY1, dstOddChromaY2, dstChromaU1,
		    dstChromaU2, dstChromaV1, dstChromaV2, roi->width);
	}
	for (; y < roi->height; y++)
	{
		const BYTE* srcEven = (pSrc + y * srcStep);
		BYTE* dstLumaYEven = (pDst1[0] + y * dst1Step[0]);
		BYTE* dstLumaU = (pDst1[1] + (y / 2) * dst1Step[1]);
		BYTE* dstLumaV = (pDst1[2] + (y / 2) * dst1Step[2]);
		BYTE* dstEvenChromaY1 = (pDst2[0] + y * dst2Step[0]);
		BYTE* dstEvenChromaY2 = dstEvenChromaY1 + roi->width / 2;
		general_RGBToAVC444YUVv2_ANY_DOUBLE_ROW(
		    srcEven, NULL, srcFormat, dstLumaYEven, NULL, dstLumaU, dstLumaV, dstEvenChromaY1,
		    dstEvenChromaY2, NULL, NULL, NULL, NULL, NULL, NULL, roi->width);
	}

	return PRIMITIVES_SUCCESS;
}

static inline void int_general_RGBToAVC444YUVv2_BGRX_DOUBLE_ROW(
    size_t offset, const BYTE* WINPR_RESTRICT pSrcEven, const BYTE* WINPR_RESTRICT pSrcOdd,
    BYTE* WINPR_RESTRICT yLumaDstEven, BYTE* WINPR_RESTRICT yLumaDstOdd,
    BYTE* WINPR_RESTRICT uLumaDst, BYTE* WINPR_RESTRICT vLumaDst,
    BYTE* WINPR_RESTRICT yEvenChromaDst1, BYTE* WINPR_RESTRICT yEvenChromaDst2,
    BYTE* WINPR_RESTRICT yOddChromaDst1, BYTE* WINPR_RESTRICT yOddChromaDst2,
    BYTE* WINPR_RESTRICT uChromaDst1, BYTE* WINPR_RESTRICT uChromaDst2,
    BYTE* WINPR_RESTRICT vChromaDst1, BYTE* WINPR_RESTRICT vChromaDst2, UINT32 width)
{
	WINPR_ASSERT((width % 2) == 0);
	WINPR_ASSERT(pSrcEven);
	WINPR_ASSERT(yLumaDstEven);
	WINPR_ASSERT(uLumaDst);
	WINPR_ASSERT(vLumaDst);

	for (size_t x = offset; x < width; x += 2)
	{
		const BYTE* srcEven = &pSrcEven[4ULL * x];
		const BYTE* srcOdd = pSrcOdd ? &pSrcOdd[4ULL * x] : NULL;
		BYTE Ya = 0;
		BYTE Ua = 0;
		BYTE Va = 0;
		BYTE Yb = 0;
		BYTE Ub = 0;
		BYTE Vb = 0;
		BYTE Yc = 0;
		BYTE Uc = 0;
		BYTE Vc = 0;
		BYTE Yd = 0;
		BYTE Ud = 0;
		BYTE Vd = 0;
		{
			const BYTE b = *srcEven++;
			const BYTE g = *srcEven++;
			const BYTE r = *srcEven++;
			srcEven++;
			Ya = RGB2Y(r, g, b);
			Ua = RGB2U(r, g, b);
			Va = RGB2V(r, g, b);
		}

		if (x < width - 1)
		{
			const BYTE b = *srcEven++;
			const BYTE g = *srcEven++;
			const BYTE r = *srcEven++;
			srcEven++;
			Yb = RGB2Y(r, g, b);
			Ub = RGB2U(r, g, b);
			Vb = RGB2V(r, g, b);
		}
		else
		{
			Yb = Ya;
			Ub = Ua;
			Vb = Va;
		}

		if (srcOdd)
		{
			const BYTE b = *srcOdd++;
			const BYTE g = *srcOdd++;
			const BYTE r = *srcOdd++;
			srcOdd++;
			Yc = RGB2Y(r, g, b);
			Uc = RGB2U(r, g, b);
			Vc = RGB2V(r, g, b);
		}
		else
		{
			Yc = Ya;
			Uc = Ua;
			Vc = Va;
		}

		if (srcOdd && (x < width - 1))
		{
			const BYTE b = *srcOdd++;
			const BYTE g = *srcOdd++;
			const BYTE r = *srcOdd++;
			srcOdd++;
			Yd = RGB2Y(r, g, b);
			Ud = RGB2U(r, g, b);
			Vd = RGB2V(r, g, b);
		}
		else
		{
			Yd = Ya;
			Ud = Ua;
			Vd = Va;
		}

		/* Y [b1] */
		*yLumaDstEven++ = Ya;

		if (x < width - 1)
			*yLumaDstEven++ = Yb;

		if (srcOdd && yLumaDstOdd)
			*yLumaDstOdd++ = Yc;

		if (srcOdd && (x < width - 1) && yLumaDstOdd)
			*yLumaDstOdd++ = Yd;

		/* 2x 2y [b2,b3] */
		*uLumaDst++ = (Ua + Ub + Uc + Ud) / 4;
		*vLumaDst++ = (Va + Vb + Vc + Vd) / 4;

		/* 2x+1, y [b4,b5] even */
		if (x < width - 1)
		{
			*yEvenChromaDst1++ = Ub;
			*yEvenChromaDst2++ = Vb;
		}

		if (srcOdd)
		{
			/* 2x+1, y [b4,b5] odd */
			if (x < width - 1)
			{
				*yOddChromaDst1++ = Ud;
				*yOddChromaDst2++ = Vd;
			}

			/* 4x 2y+1 [b6, b7] */
			if (x % 4 == 0)
			{
				*uChromaDst1++ = Uc;
				*uChromaDst2++ = Vc;
			}
			/* 4x+2 2y+1 [b8, b9] */
			else
			{
				*vChromaDst1++ = Uc;
				*vChromaDst2++ = Vc;
			}
		}
	}
}

void general_RGBToAVC444YUVv2_BGRX_DOUBLE_ROW(
    size_t offset, const BYTE* WINPR_RESTRICT pSrcEven, const BYTE* WINPR_RESTRICT pSrcOdd,
    BYTE* WINPR_RESTRICT yLumaDstEven, BYTE* WINPR_RESTRICT yLumaDstOdd,
    BYTE* WINPR_RESTRICT uLumaDst, BYTE* WINPR_RESTRICT vLumaDst,
    BYTE* WINPR_RESTRICT yEvenChromaDst1, BYTE* WINPR_RESTRICT yEvenChromaDst2,
    BYTE* WINPR_RESTRICT yOddChromaDst1, BYTE* WINPR_RESTRICT yOddChromaDst2,
    BYTE* WINPR_RESTRICT uChromaDst1, BYTE* WINPR_RESTRICT uChromaDst2,
    BYTE* WINPR_RESTRICT vChromaDst1, BYTE* WINPR_RESTRICT vChromaDst2, UINT32 width)
{
	int_general_RGBToAVC444YUVv2_BGRX_DOUBLE_ROW(
	    offset, pSrcEven, pSrcOdd, yLumaDstEven, yLumaDstOdd, uLumaDst, vLumaDst, yEvenChromaDst1,
	    yEvenChromaDst2, yOddChromaDst1, yOddChromaDst2, uChromaDst1, uChromaDst2, vChromaDst1,
	    vChromaDst2, width);
}

static inline pstatus_t general_RGBToAVC444YUVv2_BGRX(const BYTE* WINPR_RESTRICT pSrc,
                                                      UINT32 srcStep, BYTE* WINPR_RESTRICT pDst1[3],
                                                      const UINT32 dst1Step[3],
                                                      BYTE* WINPR_RESTRICT pDst2[3],
                                                      const UINT32 dst2Step[3],
                                                      const prim_size_t* WINPR_RESTRICT roi)
{
	if (roi->height < 1 || roi->width < 1)
		return !PRIMITIVES_SUCCESS;

	size_t y = 0;
	for (; y < roi->height - roi->height % 2; y += 2)
	{
		const BYTE* srcEven = (pSrc + y * srcStep);
		const BYTE* srcOdd = (srcEven + srcStep);
		BYTE* dstLumaYEven = (pDst1[0] + y * dst1Step[0]);
		BYTE* dstLumaYOdd = (dstLumaYEven + dst1Step[0]);
		BYTE* dstLumaU = (pDst1[1] + (y / 2) * dst1Step[1]);
		BYTE* dstLumaV = (pDst1[2] + (y / 2) * dst1Step[2]);
		BYTE* dstEvenChromaY1 = (pDst2[0] + y * dst2Step[0]);
		BYTE* dstEvenChromaY2 = dstEvenChromaY1 + roi->width / 2;
		BYTE* dstOddChromaY1 = dstEvenChromaY1 + dst2Step[0];
		BYTE* dstOddChromaY2 = dstEvenChromaY2 + dst2Step[0];
		BYTE* dstChromaU1 = (pDst2[1] + (y / 2) * dst2Step[1]);
		BYTE* dstChromaV1 = (pDst2[2] + (y / 2) * dst2Step[2]);
		BYTE* dstChromaU2 = dstChromaU1 + roi->width / 4;
		BYTE* dstChromaV2 = dstChromaV1 + roi->width / 4;
		int_general_RGBToAVC444YUVv2_BGRX_DOUBLE_ROW(
		    0, srcEven, srcOdd, dstLumaYEven, dstLumaYOdd, dstLumaU, dstLumaV, dstEvenChromaY1,
		    dstEvenChromaY2, dstOddChromaY1, dstOddChromaY2, dstChromaU1, dstChromaU2, dstChromaV1,
		    dstChromaV2, roi->width);
	}
	for (; y < roi->height; y++)
	{
		const BYTE* srcEven = (pSrc + y * srcStep);
		BYTE* dstLumaYEven = (pDst1[0] + y * dst1Step[0]);
		BYTE* dstLumaU = (pDst1[1] + (y / 2) * dst1Step[1]);
		BYTE* dstLumaV = (pDst1[2] + (y / 2) * dst1Step[2]);
		BYTE* dstEvenChromaY1 = (pDst2[0] + y * dst2Step[0]);
		BYTE* dstEvenChromaY2 = dstEvenChromaY1 + roi->width / 2;
		int_general_RGBToAVC444YUVv2_BGRX_DOUBLE_ROW(
		    0, srcEven, NULL, dstLumaYEven, NULL, dstLumaU, dstLumaV, dstEvenChromaY1,
		    dstEvenChromaY2, NULL, NULL, NULL, NULL, NULL, NULL, roi->width);
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t general_RGBToAVC444YUVv2(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcFormat,
                                          UINT32 srcStep, BYTE* WINPR_RESTRICT pDst1[3],
                                          const UINT32 dst1Step[3], BYTE* WINPR_RESTRICT pDst2[3],
                                          const UINT32 dst2Step[3],
                                          const prim_size_t* WINPR_RESTRICT roi)
{
	switch (srcFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return general_RGBToAVC444YUVv2_BGRX(pSrc, srcStep, pDst1, dst1Step, pDst2, dst2Step,
			                                     roi);

		default:
			return general_RGBToAVC444YUVv2_ANY(pSrc, srcFormat, srcStep, pDst1, dst1Step, pDst2,
			                                    dst2Step, roi);
	}

	return !PRIMITIVES_SUCCESS;
}

void primitives_init_YUV(primitives_t* WINPR_RESTRICT prims)
{
	prims->YUV420ToRGB_8u_P3AC4R = general_YUV420ToRGB_8u_P3AC4R;
	prims->YUV444ToRGB_8u_P3AC4R = general_YUV444ToRGB_8u_P3AC4R;
	prims->RGBToYUV420_8u_P3AC4R = general_RGBToYUV420_8u_P3AC4R;
	prims->RGBToYUV444_8u_P3AC4R = general_RGBToYUV444_8u_P3AC4R;
	prims->YUV420CombineToYUV444 = general_YUV420CombineToYUV444;
	prims->YUV444SplitToYUV420 = general_YUV444SplitToYUV420;
	prims->RGBToAVC444YUV = general_RGBToAVC444YUV;
	prims->RGBToAVC444YUVv2 = general_RGBToAVC444YUVv2;
}

void primitives_init_YUV_opt(primitives_t* WINPR_RESTRICT prims)
{
	primitives_init_YUV(prims);
	primitives_init_YUV_sse41(prims);
	primitives_init_YUV_neon(prims);
}
