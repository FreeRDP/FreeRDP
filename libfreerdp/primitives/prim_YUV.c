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

/**
 * @brief general_YUV420CombineToYUV444
 *
 * @param pMainSrc    Pointer to luma YUV420 data
 * @param srcMainStep Step width in luma YUV420 data
 * @param pAuxSrc     Pointer to chroma YUV420 data
 * @param srcAuxStep  Step width in chroma YUV420 data
 * @param pDst        Pointer to YUV444 data
 * @param dstStep     Step width in YUV444 data
 * @param roi         Region of source to combine in destination.
 *
 * @return PRIMITIVES_SUCCESS on success, an error code otherwise.
 */
static pstatus_t general_YUV420CombineToYUV444(
    const BYTE* pMainSrc[3], const UINT32 srcMainStep[3],
    const BYTE* pAuxSrc[3], const UINT32 srcAuxStep[3],
    BYTE* pDst[3], const UINT32 dstStep[3],
    const prim_size_t* roi)
{
	const UINT32 mod = 16;
	UINT32 uY = 0;
	UINT32 vY = 0;
	UINT32 x, y;
	UINT32 nWidth, nHeight;
	UINT32 halfWidth, halfHeight;
	const UINT32 oddY = 1;
	const UINT32 evenY = 0;
	const UINT32 oddX = 1;
	const UINT32 evenX = 0;
	/* The auxilary frame is aligned to multiples of 16x16.
	 * We need the padded height for B4 and B5 conversion. */
	const UINT32 padHeigth = roi->height + 16 - roi->height % 16;
	nWidth = roi->width;
	nHeight = roi->height;
	halfWidth = (nWidth) / 2;
	halfHeight = (nHeight) / 2;

	if (pMainSrc && pMainSrc[0] && pMainSrc[1] && pMainSrc[2])
	{
		/* Y data is already here... */
		/* B1 */
		for (y = 0; y < nHeight; y++)
		{
			const BYTE* Ym = pMainSrc[0] + srcMainStep[0] * y;
			BYTE* pY = pDst[0] + dstStep[0] * y;
			memcpy(pY, Ym, nWidth);
		}

		/* The first half of U, V are already here part of this frame. */
		/* B2 and B3 */
		for (y = 0; y < halfHeight; y++)
		{
			const UINT32 val2y = (2 * y + evenY);
			const UINT32 val2y1 = val2y + oddY;
			const BYTE* Um = pMainSrc[1] + srcMainStep[1] * y;
			const BYTE* Vm = pMainSrc[2] + srcMainStep[2] * y;
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
	}

	if (!pAuxSrc || !pAuxSrc[0] || !pAuxSrc[1] || !pAuxSrc[2])
		return PRIMITIVES_SUCCESS;

	/* The second half of U and V is a bit more tricky... */
	/* B4 and B5 */
	for (y = 0; y < padHeigth; y++)
	{
		const BYTE* Ya = pAuxSrc[0] + srcAuxStep[0] * y;
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
		const BYTE* Ua = pAuxSrc[1] + srcAuxStep[1] * y;
		const BYTE* Va = pAuxSrc[2] + srcAuxStep[2] * y;
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
	for (y = 0; y < halfHeight; y++)
	{
		const UINT32 val2y = (y * 2 + evenY);
		const UINT32 val2y1 = val2y + oddY;
		BYTE* pU1 = pDst[1] + dstStep[1] * val2y1;
		BYTE* pV1 = pDst[2] + dstStep[2] * val2y1;
		BYTE* pU = pDst[1] + dstStep[1] * val2y;
		BYTE* pV = pDst[2] + dstStep[2] * val2y;

		if (val2y1 > nHeight)
			continue;

		for (x = 0; x < halfWidth; x++)
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

		default:
			return general_RGBToYUV420_ANY(pSrc, srcFormat, srcStep, pDst, dstStep, roi);
	}
}



void primitives_init_YUV(primitives_t* prims)
{
	prims->YUV420ToRGB_8u_P3AC4R = general_YUV420ToRGB_8u_P3AC4R;
	prims->YUV444ToRGB_8u_P3AC4R = general_YUV444ToRGB_8u_P3AC4R;
	prims->RGBToYUV420_8u_P3AC4R = general_RGBToYUV420_8u_P3AC4R;
	prims->RGBToYUV444_8u_P3AC4R = general_RGBToYUV444_8u_P3AC4R;
	prims->YUV420CombineToYUV444 = general_YUV420CombineToYUV444;
	prims->YUV444SplitToYUV420 = general_YUV444SplitToYUV420;
}

