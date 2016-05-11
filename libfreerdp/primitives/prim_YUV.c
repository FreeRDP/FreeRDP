/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "prim_YUV.h"

static INLINE BYTE CLIP(INT32 X)
{
	if (X > 255L)
		return 255L;
	if (X < 0L)
		return 0L;
	return X;
}

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
	halfWidth = (nWidth ) / 2;
	halfHeight = (nHeight) / 2;

	if (pMainSrc)
	{
		/* Y data is already here... */
		/* B1 */
		for (y=0; y<nHeight; y++)
		{
			const BYTE* Ym = pMainSrc[0] + srcMainStep[0] * y;
			BYTE* pY = pDst[0] + dstStep[0] * y;

			memcpy(pY, Ym, nWidth);
		}

		/* The first half of U, V are already here part of this frame. */
		/* B2 and B3 */
		for (y=0; y<halfHeight; y++)
		{
			const UINT32 val2y = (2 * y + evenY);
			const UINT32 val2y1 = val2y + oddY;
			const BYTE* Um = pMainSrc[1] + srcMainStep[1] * y;
			const BYTE* Vm = pMainSrc[2] + srcMainStep[2] * y;
			BYTE* pU = pDst[1] + dstStep[1] * val2y;
			BYTE* pV = pDst[2] + dstStep[2] * val2y;
			BYTE* pU1 = pDst[1] + dstStep[1] * val2y1;
			BYTE* pV1 = pDst[2] + dstStep[2] * val2y1;

			for (x=0; x<halfWidth; x++)
			{
				const UINT32 val2x = 2*x + evenX;
				const UINT32 val2x1 = val2x+oddX;

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

	if (!pAuxSrc)
		return PRIMITIVES_SUCCESS;

	/* The second half of U and V is a bit more tricky... */
	/* B4 and B5 */
	for (y=0; y<padHeigth; y++)
	{
		const BYTE* Ya = pAuxSrc[0] + srcAuxStep[0] * y;
		BYTE* pX;

		if ((y) % mod < (mod + 1)/2)
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
	for (y=0; y<halfHeight; y++)
	{
		const UINT32 val2y = (y * 2 + evenY);
		const BYTE* Ua = pAuxSrc[1] + srcAuxStep[1] * y;
		const BYTE* Va = pAuxSrc[2] + srcAuxStep[2] * y;
		BYTE* pU = pDst[1] + dstStep[1] * val2y;
		BYTE* pV = pDst[2] + dstStep[2] * val2y;

		for (x=0; x<halfWidth; x++)
		{
			const UINT32 val2x1 = (x * 2 + oddX);
			pU[val2x1] = Ua[x];
			pV[val2x1] = Va[x];
		}
	}

	/* Filter */
	for (y=0; y<halfHeight; y++)
	{
		const UINT32 val2y = (y * 2 + evenY);
		const UINT32 val2y1 = val2y + oddY;

		BYTE* pU1 = pDst[1] + dstStep[1] * val2y1;
		BYTE* pV1 = pDst[2] + dstStep[2] * val2y1;
		BYTE* pU = pDst[1] + dstStep[1] * val2y;
		BYTE* pV = pDst[2] + dstStep[2] * val2y;

		if (val2y1 > nHeight)
			continue;

		for (x=0; x<halfWidth; x++)
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
	for (y=0; y<roi->height; y++)
	{
		const BYTE* pSrcY = pSrc[0] + y * srcStep[0];
		BYTE* pY = pMainDst[0] + y * dstMainStep[0];
		memcpy(pY, pSrcY, roi->width);
	}

	/* B2 and B3 */
	for (y=0; y<halfHeight; y++)
	{
		const BYTE* pSrcU = pSrc[1] + 2 * y * srcStep[1];
		const BYTE* pSrcV = pSrc[2] + 2 * y * srcStep[2];
		const BYTE* pSrcU1 = pSrc[1] + (2 * y + 1) * srcStep[1];
		const BYTE* pSrcV1 = pSrc[2] + (2 * y + 1) * srcStep[2];
		BYTE* pU = pMainDst[1] + y * dstMainStep[1];
		BYTE* pV = pMainDst[2] + y * dstMainStep[2];

		for (x=0; x<halfWidth; x++)
		{
			/* Filter */
			const INT32 u = pSrcU[2*x] + pSrcU[2*x+1] + pSrcU1[2*x]
					+ pSrcU1[2*x+1];
			const INT32 v = pSrcV[2*x] + pSrcV[2*x+1] + pSrcV1[2*x]
					+ pSrcV1[2*x+1];
			pU[x] = CLIP(u / 4L);
			pV[x] = CLIP(v / 4L);
		}
	}

	/* B4 and B5 */
	for (y=0; y<padHeigth; y++)
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
	for (y=0; y<halfHeight; y++)
	{
		const BYTE* pSrcU = pSrc[1] + 2 * y * srcStep[1];
		const BYTE* pSrcV = pSrc[2] + 2 * y * srcStep[2];
		BYTE* pU = pAuxDst[1] + y * dstAuxStep[1];
		BYTE* pV = pAuxDst[2] + y * dstAuxStep[2];

		for (x=0; x<halfWidth; x++)
		{
			pU[x] = pSrcU[2*x+1];
			pV[x] = pSrcV[2*x+1];
		}
	}

	return PRIMITIVES_SUCCESS;
}

/**
 * | R |   ( | 256     0    403 | |    Y    | )
 * | G | = ( | 256   -48   -120 | | U - 128 | ) >> 8
 * | B |   ( | 256   475      0 | | V - 128 | )
 */
static INLINE INT32 C(INT32 Y)
{
	return (Y) -   0L;
}

static INLINE INT32 D(INT32 U)
{
	return (U) - 128L;
}

static INLINE INT32 E(INT32 V)
{
	return (V) - 128L;
}

static INLINE BYTE YUV2R(INT32 Y, INT32 U, INT32 V)
{
	const INT32 r = ( 256L * C(Y) +   0L * D(U) + 403L * E(V));
	const INT32 r8 = r >> 8L;
	return CLIP(r8);
}

static INLINE BYTE YUV2G(INT32 Y, INT32 U, INT32 V)
{
	const INT32 g = ( 256L * C(Y) -  48L * D(U) - 120L * E(V));
	const INT32 g8 = g >> 8L;
	return CLIP(g8);
}

static INLINE BYTE YUV2B(INT32 Y, INT32 U, INT32 V)
{
	const INT32 b = ( 256L * C(Y) + 475L * D(U) +   0L * E(V));
	const INT32 b8 = b >> 8L;
	return CLIP(b8);
}

static pstatus_t general_YUV444ToRGB_8u_P3AC4R(
		const BYTE* pSrc[3], const UINT32 srcStep[3],
		BYTE* pDst, UINT32 dstStep, const prim_size_t* roi)
{
	UINT32 x, y;
	UINT32 nWidth, nHeight;

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
			const INT32 U = pU[x];
			const INT32 V = pV[x];

			pRGB[4*x+0] = YUV2B(Y, U, V);
			pRGB[4*x+1] = YUV2G(Y, U, V);
			pRGB[4*x+2] = YUV2R(Y, U, V);
			pRGB[4*x+3] = 0xFF;
		}
	}

	return PRIMITIVES_SUCCESS;
}

/**
 * | R |    ( | 256     0    403 | |    Y    | )
 * | G | = (  | 256   -48   -120 | | U - 128 |  ) >> 8
 * | B |    ( | 256   475      0 | | V - 128 | )
 */

static pstatus_t general_YUV420ToRGB_8u_P3AC4R(
		const BYTE* pSrc[3], const UINT32 srcStep[3],
		BYTE* pDst, UINT32 dstStep, const prim_size_t* roi)
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

	for (y = 0; y < halfHeight; )
	{
		if (++y == halfHeight)
			lastRow <<= 1;

		for (x = 0; x < halfWidth; )
		{
			if (++x == halfWidth)
				lastCol <<= 1;

			U = *pU++;
			V = *pV++;

			/* 1st pixel */
			Y = *pY++;

			*pRGB++ = YUV2B(Y, U, V);
			*pRGB++ = YUV2G(Y, U, V);
			*pRGB++ = YUV2R(Y, U, V);
			*pRGB++ = 0xFF;

			/* 2nd pixel */
			if (!(lastCol & 0x02))
			{
				Y = *pY++;

				*pRGB++ = YUV2B(Y, U, V);
				*pRGB++ = YUV2G(Y, U, V);
				*pRGB++ = YUV2R(Y, U, V);
				*pRGB++ = 0xFF;
			}
			else
			{
				pY++;
				pRGB += 4;
				lastCol >>= 1;
			}
		}

		pY += srcPad[0];
		pU -= halfWidth;
		pV -= halfWidth;
		pRGB += dstPad;

		if (lastRow & 0x02)
			break;

		for (x = 0; x < halfWidth; )
		{
			if (++x == halfWidth)
				lastCol <<= 1;

			U = *pU++;
			V = *pV++;

			/* 3rd pixel */
			Y = *pY++;

			*pRGB++ = YUV2B(Y, U, V);
			*pRGB++ = YUV2G(Y, U, V);
			*pRGB++ = YUV2R(Y, U, V);
			*pRGB++ = 0xFF;

			/* 4th pixel */
			if (!(lastCol & 0x02))
			{
				Y = *pY++;

				*pRGB++ = YUV2B(Y, U, V);
				*pRGB++ = YUV2G(Y, U, V);
				*pRGB++ = YUV2R(Y, U, V);
				*pRGB++ = 0xFF;
			}
			else
			{
				pY++;
				pRGB += 4;
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
static INLINE BYTE RGB2Y(INT32 R, INT32 G, INT32 B)
{
	const INT32 y = (  54L * (R) + 183L * (G) +  18L * (B));
	const INT32 y8 = (y >> 8L);

	return CLIP(y8);
}

static INLINE BYTE RGB2U(INT32 R, INT32 G, INT32 B)
{
	const INT32 u = ( -29L * (R) -  99L * (G) + 128L * (B));
	const INT32 u8 = (u >> 8L) + 128L;

	return CLIP(u8);
}

static INLINE BYTE RGB2V(INT32 R, INT32 G, INT32 B)
{
	const INT32 v = ( 128L * (R) - 116L * (G) -  12L * (B));
	const INT32 v8 = (v >> 8L) + 128L;

	return CLIP(v8);
}

static pstatus_t general_RGBToYUV444_8u_P3AC4R(
		const BYTE* pSrc, const UINT32 srcStep,
		BYTE* pDst[3], UINT32 dstStep[3], const prim_size_t* roi)
{
	UINT32 x, y;
	UINT32 nWidth, nHeight;

	nWidth = roi->width;
	nHeight = roi->height;

	for (y=0; y<nHeight; y++)
	{
		const BYTE* pRGB = pSrc + y * srcStep;

		BYTE* pY = pDst[0] + y * dstStep[0];
		BYTE* pU = pDst[1] + y * dstStep[1];
		BYTE* pV = pDst[2] + y * dstStep[2];

		for (x=0; x<nWidth; x++)
		{
			const BYTE B = pRGB[4*x+0];
			const BYTE G = pRGB[4*x+1];
			const BYTE R = pRGB[4*x+2];

			pY[x] = RGB2Y(R, G, B);
			pU[x] = RGB2U(R, G, B);
			pV[x] = RGB2V(R, G, B);
		}
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t general_RGBToYUV420_8u_P3AC4R(
		const BYTE* pSrc, UINT32 srcStep,
		BYTE* pDst[3], UINT32 dstStep[3], const prim_size_t* roi)
{
	UINT32 x, y;
	UINT32 halfWidth;
	UINT32 halfHeight;
	UINT32 nWidth, nHeight;

	nWidth = roi->width + roi->width % 2;
	nHeight = roi->height + roi->height % 2;

	halfWidth = (nWidth + nWidth % 2) / 2;
	halfHeight = (nHeight + nHeight % 2) / 2;

	for (y = 0; y < halfHeight; y++)
	{
		const UINT32 val2y = (y * 2);
		const UINT32 val2y1 = val2y + 1;
		const BYTE*  pRGB = pSrc + val2y * srcStep;
		const BYTE*  pRGB1 = pSrc + val2y1 * srcStep;

		BYTE* pY = pDst[0] + val2y * dstStep[0];
		BYTE* pY1 = pDst[0] + val2y1 * dstStep[0];
		BYTE* pU = pDst[1] + y * dstStep[1];
		BYTE* pV = pDst[2] + y * dstStep[2];

		for (x = 0; x < halfWidth; x++)
		{
			INT32 R, G, B;
			INT32 Ra, Ga, Ba;
			const UINT32 val2x = (x * 2);
			const UINT32 val2x1 = val2x + 1;

			/* 1st pixel */
			Ba = B = pRGB[val2x * 4 + 0];
			Ga = G = pRGB[val2x * 4 + 1];
			Ra = R = pRGB[val2x * 4 + 2];
			pY[val2x] = RGB2Y(R, G, B);

			if (val2x1 < nWidth)
			{
				/* 2nd pixel */
				Ba += B = pRGB[val2x * 4 + 4];
				Ga += G = pRGB[val2x * 4 + 5];
				Ra += R = pRGB[val2x * 4 + 6];
				pY[val2x1] = RGB2Y(R, G, B);
			}

			if (val2y1 < nHeight)
			{
				/* 3rd pixel */
				Ba += B = pRGB1[val2x * 4 + 0];
				Ga += G = pRGB1[val2x * 4 + 1];
				Ra += R = pRGB1[val2x * 4 + 2];
				pY1[val2x] = RGB2Y(R, G, B);

				if (val2x1 < nWidth)
				{
					/* 4th pixel */
					Ba += B = pRGB1[val2x * 4 + 4];
					Ga += G = pRGB1[val2x * 4 + 5];
					Ra += R = pRGB1[val2x * 4 + 6];
					pY1[val2x1] = RGB2Y(R, G, B);
				}
			}

			Ba >>= 2;
			Ga >>= 2;
			Ra >>= 2;

			pU[x] = RGB2U(Ra, Ga, Ba);
			pV[x] = RGB2V(Ra, Ga, Ba);
		}
	}

	return PRIMITIVES_SUCCESS;
}

void primitives_init_YUV(primitives_t* prims)
{
	prims->YUV420ToRGB_8u_P3AC4R = general_YUV420ToRGB_8u_P3AC4R;
	prims->YUV444ToRGB_8u_P3AC4R = general_YUV444ToRGB_8u_P3AC4R;
	prims->RGBToYUV420_8u_P3AC4R = general_RGBToYUV420_8u_P3AC4R;
	prims->RGBToYUV444_8u_P3AC4R = general_RGBToYUV444_8u_P3AC4R;
	prims->YUV420CombineToYUV444 = general_YUV420CombineToYUV444;
	prims->YUV444SplitToYUV420 = general_YUV444SplitToYUV420;

	primitives_init_YUV_opt(prims);
}

void primitives_deinit_YUV(primitives_t* prims)
{

}
