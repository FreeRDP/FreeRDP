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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/types.h>
#include <freerdp/primitives.h>
#include <freerdp/codec/color.h>
#include "prim_internal.h"

static pstatus_t general_LumaToYUV444(const BYTE* pSrcRaw[3], const UINT32 srcStep[3],
                                      BYTE* pDstRaw[3], const UINT32 dstStep[3],
                                      const RECTANGLE_16* roi)
{
	UINT32 x, y;
	const UINT32 nWidth = roi->right - roi->left;
	const UINT32 nHeight = roi->bottom - roi->top;
	const UINT32 halfWidth = (nWidth + 1) / 2;
	const UINT32 halfHeight = (nHeight + 1) / 2;
	const UINT32 oddY = 1;
	const UINT32 evenY = 0;
	const UINT32 oddX = 1;
	const UINT32 evenX = 0;
	const BYTE* pSrc[3] =
	{
		pSrcRaw[0] + roi->top* srcStep[0] + roi->left,
		pSrcRaw[1] + roi->top / 2 * srcStep[1] + roi->left / 2,
		pSrcRaw[2] + roi->top / 2 * srcStep[2] + roi->left / 2
	};
	BYTE* pDst[3] =
	{
		pDstRaw[0] + roi->top* dstStep[0] + roi->left,
		pDstRaw[1] + roi->top* dstStep[1] + roi->left,
		pDstRaw[2] + roi->top* dstStep[2] + roi->left
	};

	/* Y data is already here... */
	/* B1 */
	for (y = 0; y < nHeight; y++)
	{
		const BYTE* Ym = pSrc[0] + srcStep[0] * y;
		BYTE* pY = pDst[0] + dstStep[0] * y;
		memcpy(pY, Ym, nWidth);
	}

	/* The first half of U, V are already here part of this frame. */
	/* B2 and B3 */
	for (y = 0; y < halfHeight; y++)
	{
		const UINT32 val2y = (2 * y + evenY);
		const UINT32 val2y1 = val2y + oddY;
		const BYTE* Um = pSrc[1] + srcStep[1] * y;
		const BYTE* Vm = pSrc[2] + srcStep[2] * y;
		BYTE* pU = pDst[1] + dstStep[1] * val2y;
		BYTE* pV = pDst[2] + dstStep[2] * val2y;
		BYTE* pU1 = pDst[1] + dstStep[1] * val2y1;
		BYTE* pV1 = pDst[2] + dstStep[2] * val2y1;

		for (x = 0; x < halfWidth; x++)
		{
			const UINT32 val2x = 2 * x + evenX;
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

static pstatus_t general_ChromaFilter(BYTE* pDst[3], const UINT32 dstStep[3],
                                      const RECTANGLE_16* roi)
{
	const UINT32 oddY = 1;
	const UINT32 evenY = 0;
	const UINT32 nWidth = roi->right - roi->left;
	const UINT32 nHeight = roi->bottom - roi->top;
	const UINT32 halfHeight = (nHeight + 1) / 2;
	const UINT32 halfWidth = (nWidth + 1) / 2;
	UINT32 x, y;

	/* Filter */
	for (y = roi->top; y < halfHeight + roi->top; y++)
	{
		const UINT32 val2y = (y * 2 + evenY);
		const UINT32 val2y1 = val2y + oddY;
		BYTE* pU1 = pDst[1] + dstStep[1] * val2y1;
		BYTE* pV1 = pDst[2] + dstStep[2] * val2y1;
		BYTE* pU = pDst[1] + dstStep[1] * val2y;
		BYTE* pV = pDst[2] + dstStep[2] * val2y;

		if (val2y1 > nHeight)
			continue;

		for (x = roi->left; x < halfWidth + roi->left; x++)
		{
			const UINT32 val2x = (x * 2);
			const UINT32 val2x1 = val2x + 1;
			const INT32 up = pU[val2x] * 4;
			const INT32 vp = pV[val2x] * 4;
			INT32 u2020;
			INT32 v2020;

			if (val2x1 > nWidth)
				continue;

			u2020 = up - pU[val2x1] - pU1[val2x] - pU1[val2x1];
			v2020 = vp - pV[val2x1] - pV1[val2x] - pV1[val2x1];
			pU[val2x] = CLIP(u2020);
			pV[val2x] = CLIP(v2020);
		}
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t general_ChromaV1ToYUV444(const BYTE* pSrcRaw[3], const UINT32 srcStep[3],
        BYTE* pDstRaw[3], const UINT32 dstStep[3],
        const RECTANGLE_16* roi)
{
	const UINT32 mod = 16;
	UINT32 uY = 0;
	UINT32 vY = 0;
	UINT32 x, y;
	const UINT32 nWidth = roi->right - roi->left;
	const UINT32 nHeight = roi->bottom - roi->top;
	const UINT32 halfWidth = (nWidth) / 2;
	const UINT32 halfHeight = (nHeight) / 2;
	const UINT32 oddY = 1;
	const UINT32 evenY = 0;
	const UINT32 oddX = 1;
	/* The auxilary frame is aligned to multiples of 16x16.
	 * We need the padded height for B4 and B5 conversion. */
	const UINT32 padHeigth = nHeight + 16 - nHeight % 16;
	const BYTE* pSrc[3] =
	{
		pSrcRaw[0] + roi->top* srcStep[0] + roi->left,
		pSrcRaw[1] + roi->top / 2 * srcStep[1] + roi->left / 2,
		pSrcRaw[2] + roi->top / 2 * srcStep[2] + roi->left / 2
	};
	BYTE* pDst[3] =
	{
		pDstRaw[0] + roi->top* dstStep[0] + roi->left,
		pDstRaw[1] + roi->top* dstStep[1] + roi->left,
		pDstRaw[2] + roi->top* dstStep[2] + roi->left
	};

	/* The second half of U and V is a bit more tricky... */
	/* B4 and B5 */
	for (y = 0; y < padHeigth; y++)
	{
		const BYTE* Ya = pSrc[0] + srcStep[0] * y;
		BYTE* pX;

		if ((y) % mod < (mod + 1) / 2)
		{
			const UINT32 pos = (2 * uY++ + oddY);

			if (pos >= nHeight)
				continue;

			pX = pDst[1] + dstStep[1] * pos;
		}
		else
		{
			const UINT32 pos = (2 * vY++ + oddY);

			if (pos >= nHeight)
				continue;

			pX = pDst[2] + dstStep[2] * pos;
		}

		memcpy(pX, Ya, nWidth);
	}

	/* B6 and B7 */
	for (y = 0; y < halfHeight; y++)
	{
		const UINT32 val2y = (y * 2 + evenY);
		const BYTE* Ua = pSrc[1] + srcStep[1] * y;
		const BYTE* Va = pSrc[2] + srcStep[2] * y;
		BYTE* pU = pDst[1] + dstStep[1] * val2y;
		BYTE* pV = pDst[2] + dstStep[2] * val2y;

		for (x = 0; x < halfWidth; x++)
		{
			const UINT32 val2x1 = (x * 2 + oddX);
			pU[val2x1] = Ua[x];
			pV[val2x1] = Va[x];
		}
	}

	/* Filter */
	return general_ChromaFilter(pDst, dstStep, roi);
}

static pstatus_t general_ChromaV2ToYUV444(const BYTE* pSrc[3], const UINT32 srcStep[3],
        UINT32 nTotalWidth, UINT32 nTotalHeight,
        BYTE* pDst[3], const UINT32 dstStep[3],
        const RECTANGLE_16* roi)
{
	UINT32 x, y;
	const UINT32 nWidth = roi->right - roi->left;
	const UINT32 nHeight = roi->bottom - roi->top;
	const UINT32 halfWidth = (nWidth + 1) / 2;
	const UINT32 halfHeight = (nHeight + 1) / 2;
	const UINT32 quaterWidth = (nWidth + 3) / 4;

	/* B4 and B5: odd UV values for width/2, height */
	for (y = 0; y < nHeight; y++)
	{
		const UINT32 yTop = y + roi->top;
		const BYTE* pYaU = pSrc[0] + srcStep[0] * yTop + roi->left / 2;
		const BYTE* pYaV = pYaU + nTotalWidth / 2;
		BYTE* pU = pDst[1] + dstStep[1] * yTop + roi->left;
		BYTE* pV = pDst[2] + dstStep[2] * yTop + roi->left;

		for (x = 0; x < halfWidth; x++)
		{
			const UINT32 odd = 2 * x + 1;
			pU[odd] = *pYaU++;
			pV[odd] = *pYaV++;
		}
	}

	/* B6 - B9 */
	for (y = 0; y < halfHeight; y++)
	{
		const BYTE* pUaU = pSrc[1] + srcStep[1] * (y + roi->top / 2) + roi->left / 4;
		const BYTE* pUaV = pUaU + nTotalWidth / 4;
		const BYTE* pVaU = pSrc[2] + srcStep[2] * (y + roi->top / 2) + roi->left / 4;
		const BYTE* pVaV = pVaU + nTotalWidth / 4;
		BYTE* pU = pDst[1] + dstStep[1] * (2 * y + 1 + roi->top) + roi->left;
		BYTE* pV = pDst[2] + dstStep[2] * (2 * y + 1 + roi->top) + roi->left;

		for (x = 0; x < quaterWidth; x++)
		{
			pU[4 * x + 0] = *pUaU++;
			pV[4 * x + 0] = *pUaV++;
			pU[4 * x + 2] = *pVaU++;
			pV[4 * x + 2] = *pVaV++;
		}
	}

	return general_ChromaFilter(pDst, dstStep, roi);
}

static pstatus_t general_YUV420CombineToYUV444(
    avc444_frame_type type,
    const BYTE* pSrc[3], const UINT32 srcStep[3],
    UINT32 nWidth, UINT32 nHeight,
    BYTE* pDst[3], const UINT32 dstStep[3],
    const RECTANGLE_16* roi)
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

static pstatus_t general_YUV444SplitToYUV420(
    const BYTE* pSrc[3], const UINT32 srcStep[3],
    BYTE* pMainDst[3], const UINT32 dstMainStep[3],
    BYTE* pAuxDst[3], const UINT32 dstAuxStep[3],
    const prim_size_t* roi)
{
	UINT32 x, y, uY = 0, vY = 0;
	UINT32 halfWidth, halfHeight;
	/* The auxilary frame is aligned to multiples of 16x16.
	 * We need the padded height for B4 and B5 conversion. */
	const UINT32 padHeigth = roi->height + 16 - roi->height % 16;
	halfWidth = (roi->width + 1) / 2;
	halfHeight = (roi->height + 1) / 2;

	/* B1 */
	for (y = 0; y < roi->height; y++)
	{
		const BYTE* pSrcY = pSrc[0] + y * srcStep[0];
		BYTE* pY = pMainDst[0] + y * dstMainStep[0];
		memcpy(pY, pSrcY, roi->width);
	}

	/* B2 and B3 */
	for (y = 0; y < halfHeight; y++)
	{
		const BYTE* pSrcU = pSrc[1] + 2 * y * srcStep[1];
		const BYTE* pSrcV = pSrc[2] + 2 * y * srcStep[2];
		const BYTE* pSrcU1 = pSrc[1] + (2 * y + 1) * srcStep[1];
		const BYTE* pSrcV1 = pSrc[2] + (2 * y + 1) * srcStep[2];
		BYTE* pU = pMainDst[1] + y * dstMainStep[1];
		BYTE* pV = pMainDst[2] + y * dstMainStep[2];

		for (x = 0; x < halfWidth; x++)
		{
			/* Filter */
			const INT32 u = pSrcU[2 * x] + pSrcU[2 * x + 1] + pSrcU1[2 * x]
			                + pSrcU1[2 * x + 1];
			const INT32 v = pSrcV[2 * x] + pSrcV[2 * x + 1] + pSrcV1[2 * x]
			                + pSrcV1[2 * x + 1];
			pU[x] = CLIP(u / 4L);
			pV[x] = CLIP(v / 4L);
		}
	}

	/* B4 and B5 */
	for (y = 0; y < padHeigth; y++)
	{
		BYTE* pY = pAuxDst[0] + y * dstAuxStep[0];

		if (y % 16 < 8)
		{
			const UINT32 pos = (2 * uY++ + 1);
			const BYTE* pSrcU = pSrc[1] + pos * srcStep[1];

			if (pos >= roi->height)
				continue;

			memcpy(pY, pSrcU, roi->width);
		}
		else
		{
			const UINT32 pos = (2 * vY++ + 1);
			const BYTE* pSrcV = pSrc[2] + pos * srcStep[2];

			if (pos >= roi->height)
				continue;

			memcpy(pY, pSrcV, roi->width);
		}
	}

	/* B6 and B7 */
	for (y = 0; y < halfHeight; y++)
	{
		const BYTE* pSrcU = pSrc[1] + 2 * y * srcStep[1];
		const BYTE* pSrcV = pSrc[2] + 2 * y * srcStep[2];
		BYTE* pU = pAuxDst[1] + y * dstAuxStep[1];
		BYTE* pV = pAuxDst[2] + y * dstAuxStep[2];

		for (x = 0; x < halfWidth; x++)
		{
			pU[x] = pSrcU[2 * x + 1];
			pV[x] = pSrcV[2 * x + 1];
		}
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t general_YUV444ToRGB_8u_P3AC4R_general(
    const BYTE* pSrc[3], const UINT32 srcStep[3],
    BYTE* pDst, UINT32 dstStep, UINT32 DstFormat,
    const prim_size_t* roi)
{
	UINT32 x, y;
	UINT32 nWidth, nHeight;
	const DWORD formatSize = GetBytesPerPixel(DstFormat);
	fkt_writePixel writePixel = getPixelWriteFunction(DstFormat);
	nWidth = roi->width;
	nHeight = roi->height;

	for (y = 0; y < nHeight; y++)
	{
		const BYTE* pY = pSrc[0] + y * srcStep[0];
		const BYTE* pU = pSrc[1] + y * srcStep[1];
		const BYTE* pV = pSrc[2] + y * srcStep[2];
		BYTE* pRGB = pDst + y * dstStep;

		for (x = 0; x < nWidth; x++)
		{
			const BYTE Y = pY[x];
			const BYTE U = pU[x];
			const BYTE V = pV[x];
			const BYTE r = YUV2R(Y, U, V);
			const BYTE g = YUV2G(Y, U, V);
			const BYTE b = YUV2B(Y, U, V);
			pRGB = (*writePixel)(pRGB, formatSize, DstFormat, r, g, b, 0xFF);
		}
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t general_YUV444ToRGB_8u_P3AC4R_BGRX(
    const BYTE* pSrc[3], const UINT32 srcStep[3],
    BYTE* pDst, UINT32 dstStep, UINT32 DstFormat,
    const prim_size_t* roi)
{
	UINT32 x, y;
	UINT32 nWidth, nHeight;
	const DWORD formatSize = GetBytesPerPixel(DstFormat);
	nWidth = roi->width;
	nHeight = roi->height;

	for (y = 0; y < nHeight; y++)
	{
		const BYTE* pY = pSrc[0] + y * srcStep[0];
		const BYTE* pU = pSrc[1] + y * srcStep[1];
		const BYTE* pV = pSrc[2] + y * srcStep[2];
		BYTE* pRGB = pDst + y * dstStep;

		for (x = 0; x < nWidth; x++)
		{
			const BYTE Y = pY[x];
			const BYTE U = pU[x];
			const BYTE V = pV[x];
			const BYTE r = YUV2R(Y, U, V);
			const BYTE g = YUV2G(Y, U, V);
			const BYTE b = YUV2B(Y, U, V);
			pRGB = writePixelBGRX(pRGB, formatSize, DstFormat, r, g, b, 0xFF);
		}
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t general_YUV444ToRGB_8u_P3AC4R(
    const BYTE* pSrc[3], const UINT32 srcStep[3],
    BYTE* pDst, UINT32 dstStep, UINT32 DstFormat,
    const prim_size_t* roi)
{
	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return general_YUV444ToRGB_8u_P3AC4R_BGRX(pSrc, srcStep, pDst, dstStep, DstFormat, roi);

		default:
			return general_YUV444ToRGB_8u_P3AC4R_general(pSrc, srcStep, pDst, dstStep, DstFormat, roi);
	}
}
/**
 * | R |    ( | 256     0    403 | |    Y    | )
 * | G | = (  | 256   -48   -120 | | U - 128 |  ) >> 8
 * | B |    ( | 256   475      0 | | V - 128 | )
 */
static pstatus_t general_YUV420ToRGB_8u_P3AC4R(
    const BYTE* pSrc[3], const UINT32 srcStep[3],
    BYTE* pDst, UINT32 dstStep, UINT32 DstFormat,
    const prim_size_t* roi)
{
	UINT32 x, y;
	UINT32 dstPad;
	UINT32 srcPad[3];
	BYTE Y, U, V;
	UINT32 halfWidth;
	UINT32 halfHeight;
	const BYTE* pY;
	const BYTE* pU;
	const BYTE* pV;
	BYTE* pRGB = pDst;
	UINT32 nWidth, nHeight;
	UINT32 lastRow, lastCol;
	const DWORD formatSize = GetBytesPerPixel(DstFormat);
	fkt_writePixel writePixel = getPixelWriteFunction(DstFormat);
	pY = pSrc[0];
	pU = pSrc[1];
	pV = pSrc[2];
	lastCol = roi->width & 0x01;
	lastRow = roi->height & 0x01;
	nWidth = (roi->width + 1) & ~0x0001;
	nHeight = (roi->height + 1) & ~0x0001;
	halfWidth = nWidth / 2;
	halfHeight = nHeight / 2;
	srcPad[0] = (srcStep[0] - nWidth);
	srcPad[1] = (srcStep[1] - halfWidth);
	srcPad[2] = (srcStep[2] - halfWidth);
	dstPad = (dstStep - (nWidth * 4));

	for (y = 0; y < halfHeight;)
	{
		if (++y == halfHeight)
			lastRow <<= 1;

		for (x = 0; x < halfWidth;)
		{
			BYTE r;
			BYTE g;
			BYTE b;

			if (++x == halfWidth)
				lastCol <<= 1;

			U = *pU++;
			V = *pV++;
			/* 1st pixel */
			Y = *pY++;
			r = YUV2R(Y, U, V);
			g = YUV2G(Y, U, V);
			b = YUV2B(Y, U, V);
			pRGB = (*writePixel)(pRGB, formatSize, DstFormat, r, g, b, 0xFF);

			/* 2nd pixel */
			if (!(lastCol & 0x02))
			{
				Y = *pY++;
				r = YUV2R(Y, U, V);
				g = YUV2G(Y, U, V);
				b = YUV2B(Y, U, V);
				pRGB = (*writePixel)(pRGB, formatSize, DstFormat, r, g, b, 0xFF);
			}
			else
			{
				pY++;
				pRGB += formatSize;
				lastCol >>= 1;
			}
		}

		pY += srcPad[0];
		pU -= halfWidth;
		pV -= halfWidth;
		pRGB += dstPad;

		if (lastRow & 0x02)
			break;

		for (x = 0; x < halfWidth;)
		{
			BYTE r;
			BYTE g;
			BYTE b;

			if (++x == halfWidth)
				lastCol <<= 1;

			U = *pU++;
			V = *pV++;
			/* 3rd pixel */
			Y = *pY++;
			r = YUV2R(Y, U, V);
			g = YUV2G(Y, U, V);
			b = YUV2B(Y, U, V);
			pRGB = (*writePixel)(pRGB, formatSize, DstFormat, r, g, b, 0xFF);

			/* 4th pixel */
			if (!(lastCol & 0x02))
			{
				Y = *pY++;
				r = YUV2R(Y, U, V);
				g = YUV2G(Y, U, V);
				b = YUV2B(Y, U, V);
				pRGB = (*writePixel)(pRGB, formatSize, DstFormat, r, g, b, 0xFF);
			}
			else
			{
				pY++;
				pRGB += formatSize;
				lastCol >>= 1;
			}
		}

		pY += srcPad[0];
		pU += srcPad[1];
		pV += srcPad[2];
		pRGB += dstPad;
	}

	return PRIMITIVES_SUCCESS;
}

/**
 * | Y |    ( |  54   183     18 | | R | )        |  0  |
 * | U | =  ( | -29   -99    128 | | G | ) >> 8 + | 128 |
 * | V |    ( | 128  -116    -12 | | B | )        | 128 |
 */
static INLINE BYTE RGB2Y(BYTE R, BYTE G, BYTE B)
{
	return (54 * R + 183 * G + 18 * B) >> 8;
}

static INLINE BYTE RGB2U(BYTE R, BYTE G, BYTE B)
{
	return ((-29 * R - 99 * G + 128 * B) >> 8) + 128;
}

static INLINE BYTE RGB2V(INT32 R, INT32 G, INT32 B)
{
	return ((128L * R - 116 * G - 12 * B) >> 8) + 128;
}

static pstatus_t general_RGBToYUV444_8u_P3AC4R(
    const BYTE* pSrc, UINT32 SrcFormat, const UINT32 srcStep,
    BYTE* pDst[3], UINT32 dstStep[3], const prim_size_t* roi)
{
	const UINT32 bpp = GetBytesPerPixel(SrcFormat);
	UINT32 x, y;
	UINT32 nWidth, nHeight;
	nWidth = roi->width;
	nHeight = roi->height;

	for (y = 0; y < nHeight; y++)
	{
		const BYTE* pRGB = pSrc + y * srcStep;
		BYTE* pY = pDst[0] + y * dstStep[0];
		BYTE* pU = pDst[1] + y * dstStep[1];
		BYTE* pV = pDst[2] + y * dstStep[2];

		for (x = 0; x < nWidth; x++)
		{
			BYTE B, G, R;
			const UINT32 color = ReadColor(&pRGB[x * bpp], SrcFormat);
			SplitColor(color, SrcFormat, &R, &G, &B, NULL, NULL);
			pY[x] = RGB2Y(R, G, B);
			pU[x] = RGB2U(R, G, B);
			pV[x] = RGB2V(R, G, B);
		}
	}

	return PRIMITIVES_SUCCESS;
}


static INLINE pstatus_t general_RGBToYUV420_BGRX(
    const BYTE* pSrc, UINT32 srcStep,
    BYTE* pDst[3], UINT32 dstStep[3], const prim_size_t* roi)
{
	UINT32 x, y, i;
	size_t x1 = 0, x2 = 4, x3 = srcStep, x4 = srcStep + 4;
	size_t y1 = 0, y2 = 1, y3 = dstStep[0], y4 = dstStep[0] + 1;
	UINT32 max_x = roi->width - 1;
	UINT32 max_y = roi->height - 1;

	for (y = i = 0; y < roi->height; y += 2, i++)
	{
		const BYTE* src = pSrc + y * srcStep;
		BYTE* ydst = pDst[0] + y * dstStep[0];
		BYTE* udst = pDst[1] + i * dstStep[1];
		BYTE* vdst = pDst[2] + i * dstStep[2];

		for (x = 0; x < roi->width; x += 2)
		{
			BYTE R, G, B;
			INT32 Ra, Ga, Ba;
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

			if (y < max_y)
			{
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

static INLINE pstatus_t general_RGBToYUV420_RGBX(
    const BYTE* pSrc, UINT32 srcStep,
    BYTE* pDst[3], UINT32 dstStep[3], const prim_size_t* roi)
{
	UINT32 x, y, i;
	size_t x1 = 0, x2 = 4, x3 = srcStep, x4 = srcStep + 4;
	size_t y1 = 0, y2 = 1, y3 = dstStep[0], y4 = dstStep[0] + 1;
	UINT32 max_x = roi->width - 1;
	UINT32 max_y = roi->height - 1;

	for (y = i = 0; y < roi->height; y += 2, i++)
	{
		const BYTE* src = pSrc + y * srcStep;
		BYTE* ydst = pDst[0] + y * dstStep[0];
		BYTE* udst = pDst[1] + i * dstStep[1];
		BYTE* vdst = pDst[2] + i * dstStep[2];

		for (x = 0; x < roi->width; x += 2)
		{
			BYTE R, G, B;
			INT32 Ra, Ga, Ba;
			/* row 1, pixel 1 */
			Ra = R = *(src + x1 + 0);
			Ga = G = *(src + x1 + 1);
			Ba = B = *(src + x1 + 2);
			ydst[y1] = RGB2Y(R, G, B);

			if (x < max_x)
			{
				/* row 1, pixel 2 */
				Ra += R = *(src + x2 + 0);
				Ga += G = *(src + x2 + 1);
				Ba += B = *(src + x2 + 2);
				ydst[y2] = RGB2Y(R, G, B);
			}

			if (y < max_y)
			{
				/* row 2, pixel 1 */
				Ra += R = *(src + x3 + 0);
				Ga += G = *(src + x3 + 1);
				Ba += B = *(src + x3 + 2);
				ydst[y3] = RGB2Y(R, G, B);

				if (x < max_x)
				{
					/* row 2, pixel 2 */
					Ra += R = *(src + x4 + 0);
					Ga += G = *(src + x4 + 1);
					Ba += B = *(src + x4 + 2);
					ydst[y4] = RGB2Y(R, G, B);
				}
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

static INLINE pstatus_t general_RGBToYUV420_ANY(
    const BYTE* pSrc, UINT32 srcFormat, UINT32 srcStep,
    BYTE* pDst[3], UINT32 dstStep[3], const prim_size_t* roi)
{
	const UINT32 bpp = GetBytesPerPixel(srcFormat);
	UINT32 x, y, i;
	size_t x1 = 0, x2 = bpp, x3 = srcStep, x4 = srcStep + bpp;
	size_t y1 = 0, y2 = 1, y3 = dstStep[0], y4 = dstStep[0] + 1;
	UINT32 max_x = roi->width - 1;
	UINT32 max_y = roi->height - 1;

	for (y = i = 0; y < roi->height; y += 2, i++)
	{
		const BYTE* src = pSrc + y * srcStep;
		BYTE* ydst = pDst[0] + y * dstStep[0];
		BYTE* udst = pDst[1] + i * dstStep[1];
		BYTE* vdst = pDst[2] + i * dstStep[2];

		for (x = 0; x < roi->width; x += 2)
		{
			BYTE R, G, B;
			INT32 Ra, Ga, Ba;
			UINT32 color;
			/* row 1, pixel 1 */
			color = ReadColor(src + x1, srcFormat);
			SplitColor(color, srcFormat, &R, &G, &B, NULL, NULL);
			Ra = R;
			Ga = G;
			Ba = B;
			ydst[y1] = RGB2Y(R, G, B);

			if (x < max_x)
			{
				/* row 1, pixel 2 */
				color = ReadColor(src + x2, srcFormat);
				SplitColor(color, srcFormat, &R, &G, &B, NULL, NULL);
				Ra += R;
				Ga += G;
				Ba += B;
				ydst[y2] = RGB2Y(R, G, B);
			}

			if (y < max_y)
			{
				/* row 2, pixel 1 */
				color = ReadColor(src + x3, srcFormat);
				SplitColor(color, srcFormat, &R, &G, &B, NULL, NULL);
				Ra += R;
				Ga += G;
				Ba += B;
				ydst[y3] = RGB2Y(R, G, B);

				if (x < max_x)
				{
					/* row 2, pixel 2 */
					color = ReadColor(src + x4, srcFormat);
					SplitColor(color, srcFormat, &R, &G, &B, NULL, NULL);
					Ra += R;
					Ga += G;
					Ba += B;
					ydst[y4] = RGB2Y(R, G, B);
				}
			}

			Ra >>= 2;
			Ga >>= 2;
			Ba >>= 2;
			*udst++ = RGB2U(Ra, Ga, Ba);
			*vdst++ = RGB2V(Ra, Ga, Ba);
			ydst += 2;
			src += 2 * bpp;
		}
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t general_RGBToYUV420_8u_P3AC4R(
    const BYTE* pSrc, UINT32 srcFormat, UINT32 srcStep,
    BYTE* pDst[3], UINT32 dstStep[3], const prim_size_t* roi)
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

static INLINE void general_RGBToAVC444YUV_BGRX_DOUBLE_ROW(
    const BYTE* srcEven, const BYTE* srcOdd, BYTE* b1Even, BYTE* b1Odd, BYTE* b2,
    BYTE* b3, BYTE* b4, BYTE* b5, BYTE* b6, BYTE* b7, UINT32 width)
{
	UINT32 x;

	for (x = 0; x < width; x += 2)
	{
		const BOOL lastX = (x + 1) >= width;
		BYTE Y1e, Y2e, U1e, V1e, U2e, V2e;
		BYTE Y1o, Y2o, U1o, V1o, U2o, V2o;
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
			const BYTE Uavg = ((UINT16)U1e + (UINT16)U2e + (UINT16)U1o + (UINT16)U2o) / 4;
			const BYTE Vavg = ((UINT16)V1e + (UINT16)V2e + (UINT16)V1o + (UINT16)V2o) / 4;
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

static INLINE pstatus_t general_RGBToAVC444YUV_BGRX(
    const BYTE* pSrc, UINT32 srcStep,
    BYTE* pDst1[3], const UINT32 dst1Step[3],
    BYTE* pDst2[3], const UINT32 dst2Step[3],
    const prim_size_t* roi)
{
	/**
	 * Note:
	 * Read information in function general_RGBToAVC444YUV_ANY below !
	 */
	UINT32 y;
	const BYTE* pMaxSrc = pSrc + (roi->height - 1) * srcStep;

	for (y = 0; y < roi->height; y += 2)
	{
		const BOOL last = (y >= (roi->height - 1));
		const BYTE* srcEven = y < roi->height ? pSrc + y * srcStep : pMaxSrc;
		const BYTE* srcOdd = !last ? pSrc + (y + 1) * srcStep : pMaxSrc;
		const UINT32 i = y >> 1;
		const UINT32 n = (i & ~7) + i;
		BYTE* b1Even = pDst1[0] + y * dst1Step[0];
		BYTE* b1Odd = !last ? (b1Even + dst1Step[0]) : NULL;
		BYTE* b2 = pDst1[1] + (y / 2) * dst1Step[1];
		BYTE* b3 = pDst1[2] + (y / 2) * dst1Step[2];
		BYTE* b4 = pDst2[0] + dst2Step[0] * n;
		BYTE* b5 = b4 + 8 * dst2Step[0];
		BYTE* b6 = pDst2[1] + (y / 2) * dst2Step[1];
		BYTE* b7 = pDst2[2] + (y / 2) * dst2Step[2];
		general_RGBToAVC444YUV_BGRX_DOUBLE_ROW(srcEven, srcOdd, b1Even, b1Odd, b2, b3, b4, b5, b6, b7,
		                                       roi->width);
	}

	return PRIMITIVES_SUCCESS;
}

static INLINE void general_RGBToAVC444YUV_RGBX_DOUBLE_ROW(
    const BYTE* srcEven, const BYTE* srcOdd, BYTE* b1Even, BYTE* b1Odd, BYTE* b2,
    BYTE* b3, BYTE* b4, BYTE* b5, BYTE* b6, BYTE* b7, UINT32 width)
{
	UINT32 x;

	for (x = 0; x < width; x += 2)
	{
		const BOOL lastX = (x + 1) >= width;
		BYTE Y1e, Y2e, U1e, V1e, U2e, V2e;
		BYTE Y1o, Y2o, U1o, V1o, U2o, V2o;
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
			const BYTE Uavg = ((UINT16)U1e + (UINT16)U2e + (UINT16)U1o + (UINT16)U2o) / 4;
			const BYTE Vavg = ((UINT16)V1e + (UINT16)V2e + (UINT16)V1o + (UINT16)V2o) / 4;
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

static INLINE pstatus_t general_RGBToAVC444YUV_RGBX(
    const BYTE* pSrc, UINT32 srcStep,
    BYTE* pDst1[3], const UINT32 dst1Step[3],
    BYTE* pDst2[3], const UINT32 dst2Step[3],
    const prim_size_t* roi)
{
	/**
	 * Note:
	 * Read information in function general_RGBToAVC444YUV_ANY below !
	 */
	UINT32 y;
	const BYTE* pMaxSrc = pSrc + (roi->height - 1) * srcStep;

	for (y = 0; y < roi->height; y += 2)
	{
		const BOOL last = (y >= (roi->height - 1));
		const BYTE* srcEven = y < roi->height ? pSrc + y * srcStep : pMaxSrc;
		const BYTE* srcOdd = !last ? pSrc + (y + 1) * srcStep : pMaxSrc;
		const UINT32 i = y >> 1;
		const UINT32 n = (i & ~7) + i;
		BYTE* b1Even = pDst1[0] + y * dst1Step[0];
		BYTE* b1Odd = !last ? (b1Even + dst1Step[0]) : NULL;
		BYTE* b2 = pDst1[1] + (y / 2) * dst1Step[1];
		BYTE* b3 = pDst1[2] + (y / 2) * dst1Step[2];
		BYTE* b4 = pDst2[0] + dst2Step[0] * n;
		BYTE* b5 = b4 + 8 * dst2Step[0];
		BYTE* b6 = pDst2[1] + (y / 2) * dst2Step[1];
		BYTE* b7 = pDst2[2] + (y / 2) * dst2Step[2];
		general_RGBToAVC444YUV_RGBX_DOUBLE_ROW(srcEven, srcOdd, b1Even, b1Odd, b2, b3, b4, b5, b6, b7,
		                                       roi->width);
	}

	return PRIMITIVES_SUCCESS;
}

static INLINE void general_RGBToAVC444YUV_ANY_DOUBLE_ROW(
    const BYTE* srcEven, const BYTE* srcOdd, UINT32 srcFormat,
    BYTE* b1Even, BYTE* b1Odd, BYTE* b2,
    BYTE* b3, BYTE* b4, BYTE* b5, BYTE* b6, BYTE* b7, UINT32 width)
{
	const UINT32 bpp = GetBytesPerPixel(srcFormat);
	UINT32 x;

	for (x = 0; x < width; x += 2)
	{
		const BOOL lastX = (x + 1) >= width;
		BYTE Y1e, Y2e, U1e, V1e, U2e, V2e;
		BYTE Y1o, Y2o, U1o, V1o, U2o, V2o;
		/* Read 4 pixels, 2 from even, 2 from odd lines */
		{
			BYTE r, g, b;
			const UINT32 color = ReadColor(srcEven, srcFormat);
			srcEven += bpp;
			SplitColor(color, srcFormat, &r, &g, &b, NULL, NULL);
			Y1e = Y2e = Y1o = Y2o = RGB2Y(r, g, b);
			U1e = U2e = U1o = U2o = RGB2U(r, g, b);
			V1e = V2e = V1o = V2o = RGB2V(r, g, b);
		}

		if (!lastX)
		{
			BYTE r, g, b;
			const UINT32 color = ReadColor(srcEven, srcFormat);
			srcEven += bpp;
			SplitColor(color, srcFormat, &r, &g, &b, NULL, NULL);
			Y2e = RGB2Y(r, g, b);
			U2e = RGB2U(r, g, b);
			V2e = RGB2V(r, g, b);
		}

		if (b1Odd)
		{
			BYTE r, g, b;
			const UINT32 color = ReadColor(srcOdd, srcFormat);
			srcOdd += bpp;
			SplitColor(color, srcFormat, &r, &g, &b, NULL, NULL);
			Y1o = Y2o = RGB2Y(r, g, b);
			U1o = U2o = RGB2U(r, g, b);
			V1o = V2o = RGB2V(r, g, b);
		}

		if (b1Odd && !lastX)
		{
			BYTE r, g, b;
			const UINT32 color = ReadColor(srcOdd, srcFormat);
			srcOdd += bpp;
			SplitColor(color, srcFormat, &r, &g, &b, NULL, NULL);
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
			const BYTE Uavg = ((UINT16)U1e + (UINT16)U2e + (UINT16)U1o + (UINT16)U2o) / 4;
			const BYTE Vavg = ((UINT16)V1e + (UINT16)V2e + (UINT16)V1o + (UINT16)V2o) / 4;
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

static INLINE pstatus_t general_RGBToAVC444YUV_ANY(
    const BYTE* pSrc, UINT32 srcFormat, UINT32 srcStep,
    BYTE* pDst1[3], const UINT32 dst1Step[3],
    BYTE* pDst2[3], const UINT32 dst2Step[3],
    const prim_size_t* roi)
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
	 * Chroma420 frame (auxillary view):
	 * B45: From U444 and V444 all pixels from all odd rows
	 *      (The odd U444 and V444 rows must be interleaved in 8-line blocks in B45 !!!)
	 * B6:  From U444 all pixels in even rows with odd columns
	 * B7:  From V444 all pixels in even rows with odd columns
	 *
	 * Microsoft's horrible unclear description in MS-RDPEGFX translated to pseudo code looks like this:
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
	UINT32 y;
	const BYTE* pMaxSrc = pSrc + (roi->height - 1) * srcStep;

	for (y = 0; y < roi->height; y += 2)
	{
		const BOOL last = (y >= (roi->height - 1));
		const BYTE* srcEven = y < roi->height ? pSrc + y * srcStep : pMaxSrc;
		const BYTE* srcOdd = !last ? pSrc + (y + 1) * srcStep : pMaxSrc;
		const UINT32 i = y >> 1;
		const UINT32 n = (i & ~7) + i;
		BYTE* b1Even = pDst1[0] + y * dst1Step[0];
		BYTE* b1Odd = !last ? (b1Even + dst1Step[0]) : NULL;
		BYTE* b2 = pDst1[1] + (y / 2) * dst1Step[1];
		BYTE* b3 = pDst1[2] + (y / 2) * dst1Step[2];
		BYTE* b4 = pDst2[0] + dst2Step[0] * n;
		BYTE* b5 = b4 + 8 * dst2Step[0];
		BYTE* b6 = pDst2[1] + (y / 2) * dst2Step[1];
		BYTE* b7 = pDst2[2] + (y / 2) * dst2Step[2];
		general_RGBToAVC444YUV_ANY_DOUBLE_ROW(srcEven, srcOdd, srcFormat,
		                                      b1Even, b1Odd, b2, b3, b4, b5, b6, b7,
		                                      roi->width);
	}

	return PRIMITIVES_SUCCESS;
}

static INLINE pstatus_t general_RGBToAVC444YUV(
    const BYTE* pSrc, UINT32 srcFormat, UINT32 srcStep,
    BYTE* pDst1[3], const UINT32 dst1Step[3],
    BYTE* pDst2[3], const UINT32 dst2Step[3],
    const prim_size_t* roi)
{
	switch (srcFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return general_RGBToAVC444YUV_BGRX(pSrc, srcStep, pDst1, dst1Step, pDst2, dst2Step, roi);

		case PIXEL_FORMAT_RGBA32:
		case PIXEL_FORMAT_RGBX32:
			return general_RGBToAVC444YUV_RGBX(pSrc, srcStep, pDst1, dst1Step, pDst2, dst2Step, roi);

		default:
			return general_RGBToAVC444YUV_ANY(pSrc, srcFormat, srcStep, pDst1, dst1Step, pDst2, dst2Step, roi);
	}

	return !PRIMITIVES_SUCCESS;
}

static INLINE void general_RGBToAVC444YUVv2_ANY_DOUBLE_ROW(
    const BYTE* srcEven, const BYTE* srcOdd, UINT32 srcFormat,
    BYTE* yLumaDstEven, BYTE* yLumaDstOdd,
    BYTE* uLumaDst, BYTE* vLumaDst,
    BYTE* yEvenChromaDst1, BYTE* yEvenChromaDst2,
    BYTE* yOddChromaDst1, BYTE* yOddChromaDst2,
    BYTE* uChromaDst1, BYTE* uChromaDst2,
    BYTE* vChromaDst1, BYTE* vChromaDst2,
    UINT32 width)
{
	UINT32 x;
	const UINT32 bpp = GetBytesPerPixel(srcFormat);

	for (x = 0; x < width; x += 2)
	{
		BYTE Ya, Ua, Va;
		BYTE Yb, Ub, Vb;
		BYTE Yc, Uc, Vc;
		BYTE Yd, Ud, Vd;
		{
			BYTE b, g, r;
			const UINT32 color = ReadColor(srcEven, srcFormat);
			srcEven += bpp;
			SplitColor(color, srcFormat, &r, &g, &b, NULL, NULL);
			Ya = RGB2Y(r, g, b);
			Ua = RGB2U(r, g, b);
			Va = RGB2V(r, g, b);
		}

		if (x < width - 1)
		{
			BYTE b, g, r;
			const UINT32 color = ReadColor(srcEven, srcFormat);
			srcEven += bpp;
			SplitColor(color, srcFormat, &r, &g, &b, NULL, NULL);
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
			BYTE b, g, r;
			const UINT32 color = ReadColor(srcOdd, srcFormat);
			srcOdd += bpp;
			SplitColor(color, srcFormat, &r, &g, &b, NULL, NULL);
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
			BYTE b, g, r;
			const UINT32 color = ReadColor(srcOdd, srcFormat);
			srcOdd += bpp;
			SplitColor(color, srcFormat, &r, &g, &b, NULL, NULL);
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

static INLINE pstatus_t general_RGBToAVC444YUVv2_ANY(
    const BYTE* pSrc, UINT32 srcFormat, UINT32 srcStep,
    BYTE* pDst1[3], const UINT32 dst1Step[3],
    BYTE* pDst2[3], const UINT32 dst2Step[3],
    const prim_size_t* roi)
{
	/**
	 * Note: According to [MS-RDPEGFX 2.2.4.4 RFX_AVC420_BITMAP_STREAM] the
	 * width and height of the MPEG-4 AVC/H.264 codec bitstream MUST be aligned
	 * to a multiple of 16.
	 * Hence the passed destination YUV420/CHROMA420 buffers must have been
	 * allocated accordingly !!
	 */
	/**
	 * [MS-RDPEGFX 3.3.8.3.3 YUV420p Stream Combination for YUV444v2 mode] defines the following "Bx areas":
	 *
	 * YUV420 frame (main view):
	 * B1:  From Y444 all pixels
	 * B2:  From U444 all pixels in even rows with even rows and columns
	 * B3:  From V444 all pixels in even rows with even rows and columns
	 *
	 * Chroma420 frame (auxillary view):
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
	UINT32 y;

	if (roi->height < 1 || roi->width < 1)
		return !PRIMITIVES_SUCCESS;

	for (y = 0; y < roi->height; y += 2)
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
		general_RGBToAVC444YUVv2_ANY_DOUBLE_ROW(srcEven, srcOdd, srcFormat, dstLumaYEven,
		                                        dstLumaYOdd, dstLumaU, dstLumaV,
		                                        dstEvenChromaY1, dstEvenChromaY2,
		                                        dstOddChromaY1, dstOddChromaY2,
		                                        dstChromaU1, dstChromaU2,
		                                        dstChromaV1, dstChromaV2,
		                                        roi->width);
	}

	return PRIMITIVES_SUCCESS;
}

static INLINE void general_RGBToAVC444YUVv2_BGRX_DOUBLE_ROW(
    const BYTE* srcEven, const BYTE* srcOdd,
    BYTE* yLumaDstEven, BYTE* yLumaDstOdd,
    BYTE* uLumaDst, BYTE* vLumaDst,
    BYTE* yEvenChromaDst1, BYTE* yEvenChromaDst2,
    BYTE* yOddChromaDst1, BYTE* yOddChromaDst2,
    BYTE* uChromaDst1, BYTE* uChromaDst2,
    BYTE* vChromaDst1, BYTE* vChromaDst2,
    UINT32 width)
{
	UINT32 x;

	for (x = 0; x < width; x += 2)
	{
		BYTE Ya, Ua, Va;
		BYTE Yb, Ub, Vb;
		BYTE Yc, Uc, Vc;
		BYTE Yd, Ud, Vd;
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

static INLINE pstatus_t general_RGBToAVC444YUVv2_BGRX(
    const BYTE* pSrc, UINT32 srcStep,
    BYTE* pDst1[3], const UINT32 dst1Step[3],
    BYTE* pDst2[3], const UINT32 dst2Step[3],
    const prim_size_t* roi)
{
	UINT32 y;

	if (roi->height < 1 || roi->width < 1)
		return !PRIMITIVES_SUCCESS;

	for (y = 0; y < roi->height; y += 2)
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
		general_RGBToAVC444YUVv2_BGRX_DOUBLE_ROW(srcEven, srcOdd, dstLumaYEven,
		        dstLumaYOdd, dstLumaU, dstLumaV,
		        dstEvenChromaY1, dstEvenChromaY2,
		        dstOddChromaY1, dstOddChromaY2,
		        dstChromaU1, dstChromaU2,
		        dstChromaV1, dstChromaV2,
		        roi->width);
	}

	return PRIMITIVES_SUCCESS;
}

static INLINE pstatus_t general_RGBToAVC444YUVv2(
    const BYTE* pSrc, UINT32 srcFormat, UINT32 srcStep,
    BYTE* pDst1[3], const UINT32 dst1Step[3],
    BYTE* pDst2[3], const UINT32 dst2Step[3],
    const prim_size_t* roi)
{
	switch (srcFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return general_RGBToAVC444YUVv2_BGRX(pSrc, srcStep, pDst1, dst1Step, pDst2, dst2Step, roi);

		default:
			return general_RGBToAVC444YUVv2_ANY(pSrc, srcFormat, srcStep, pDst1, dst1Step, pDst2, dst2Step,
			                                    roi);
	}

	return !PRIMITIVES_SUCCESS;
}

void primitives_init_YUV(primitives_t* prims)
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

