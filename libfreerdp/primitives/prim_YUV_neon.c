/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Optimized YUV/RGB conversion operations
 *
 * Copyright 2014 Thomas Erbesdobler
 * Copyright 2016-2017 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016-2017 Norbert Federa <norbert.federa@thincast.com>
 * Copyright 2016-2017 Thincast Technologies GmbH
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
#include <winpr/crt.h>
#include <freerdp/types.h>
#include <freerdp/primitives.h>

#include "prim_internal.h"

#if !defined(WITH_NEON)
#error "This file must only be included if WITH_NEON is active!"
#endif

#include <arm_neon.h>

static primitives_t* generic = NULL;

static INLINE uint8x8_t neon_YUV2R(int32x4_t Ch, int32x4_t Cl,
                                   int16x4_t Dh, int16x4_t Dl,
                                   int16x4_t Eh, int16x4_t El)
{
	/* R = (256 * Y + 403 * (V - 128)) >> 8 */
	const int16x4_t c403 = vdup_n_s16(403);
	const int32x4_t CEh = vmlal_s16(Ch, Eh, c403);
	const int32x4_t CEl = vmlal_s16(Cl, El, c403);
	const int32x4_t Rh = vrshrq_n_s32(CEh, 8);
	const int32x4_t Rl = vrshrq_n_s32(CEl, 8);
	const int16x8_t R = vcombine_s16(vqmovn_s32(Rl), vqmovn_s32(Rh));
	return vqmovun_s16(R);
}

static INLINE uint8x8_t neon_YUV2G(int32x4_t Ch, int32x4_t Cl,
                                   int16x4_t Dh, int16x4_t Dl,
                                   int16x4_t Eh, int16x4_t El)
{
	/* G = (256L * Y -  48 * (U - 128) - 120 * (V - 128)) >> 8 */
	const int16x4_t c48 = vdup_n_s16(48);
	const int16x4_t c120 = vdup_n_s16(120);
	const int32x4_t CDh = vmlsl_s16(Ch, Dh, c48);
	const int32x4_t CDl = vmlsl_s16(Cl, Dl, c48);
	const int32x4_t CDEh = vmlsl_s16(CDh, Eh, c120);
	const int32x4_t CDEl = vmlsl_s16(CDl, El, c120);
	const int32x4_t Gh = vrshrq_n_s32(CDEh, 8);
	const int32x4_t Gl = vrshrq_n_s32(CDEl, 8);
	const int16x8_t G = vcombine_s16(vqmovn_s32(Gl), vqmovn_s32(Gh));
	return vqmovun_s16(G);
}

static INLINE uint8x8_t neon_YUV2B(int32x4_t Ch, int32x4_t Cl,
                                   int16x4_t Dh, int16x4_t Dl,
                                   int16x4_t Eh, int16x4_t El)
{
	/* B = (256L * Y + 475 * (U - 128)) >> 8*/
	const int16x4_t c475 = vdup_n_s16(475);
	const int32x4_t CDh = vmlal_s16(Ch, Dh, c475);
	const int32x4_t CDl = vmlal_s16(Ch, Dl, c475);
	const int32x4_t Bh = vrshrq_n_s32(CDh, 8);
	const int32x4_t Bl = vrshrq_n_s32(CDl, 8);
	const int16x8_t B = vcombine_s16(vqmovn_s32(Bl), vqmovn_s32(Bh));
	return vqmovun_s16(B);
}

static INLINE BYTE* neon_YuvToRgbPixel(BYTE* pRGB, int16x8_t Y, int16x8_t D, int16x8_t E,
                                       const uint8_t rPos,  const uint8_t gPos, const uint8_t bPos, const uint8_t aPos)
{
	uint8x8x4_t bgrx;
	const int32x4_t Ch = vmulq_n_s32(vmovl_s16(vget_high_s16(Y)), 256);  /* Y * 256 */
	const int32x4_t Cl = vmulq_n_s32(vmovl_s16(vget_low_s16(Y)), 256);  /* Y * 256 */
	const int16x4_t Dh = vget_high_s16(D);
	const int16x4_t Dl = vget_low_s16(D);
	const int16x4_t Eh = vget_high_s16(E);
	const int16x4_t El = vget_low_s16(E);
	{
		/* B = (256L * Y + 475 * (U - 128)) >> 8*/
		const int16x4_t c475 = vdup_n_s16(475);
		const int32x4_t CDh = vmlal_s16(Ch, Dh, c475);
		const int32x4_t CDl = vmlal_s16(Cl, Dl, c475);
		const int32x4_t Bh = vrshrq_n_s32(CDh, 8);
		const int32x4_t Bl = vrshrq_n_s32(CDl, 8);
		const int16x8_t B = vcombine_s16(vqmovn_s32(Bl), vqmovn_s32(Bh));
		bgrx.val[bPos] = vqmovun_s16(B);
	}
	{
		/* G = (256L * Y -  48 * (U - 128) - 120 * (V - 128)) >> 8 */
		const int16x4_t c48 = vdup_n_s16(48);
		const int16x4_t c120 = vdup_n_s16(120);
		const int32x4_t CDh = vmlsl_s16(Ch, Dh, c48);
		const int32x4_t CDl = vmlsl_s16(Cl, Dl, c48);
		const int32x4_t CDEh = vmlsl_s16(CDh, Eh, c120);
		const int32x4_t CDEl = vmlsl_s16(CDl, El, c120);
		const int32x4_t Gh = vrshrq_n_s32(CDEh, 8);
		const int32x4_t Gl = vrshrq_n_s32(CDEl, 8);
		const int16x8_t G = vcombine_s16(vqmovn_s32(Gl), vqmovn_s32(Gh));
		bgrx.val[gPos] = vqmovun_s16(G);
	}
	{
		/* R = (256 * Y + 403 * (V - 128)) >> 8 */
		const int16x4_t c403 = vdup_n_s16(403);
		const int32x4_t CEh = vmlal_s16(Ch, Eh, c403);
		const int32x4_t CEl = vmlal_s16(Cl, El, c403);
		const int32x4_t Rh = vrshrq_n_s32(CEh, 8);
		const int32x4_t Rl = vrshrq_n_s32(CEl, 8);
		const int16x8_t R = vcombine_s16(vqmovn_s32(Rl), vqmovn_s32(Rh));
		bgrx.val[rPos] = vqmovun_s16(R);
	}
	{
		/* A */
		bgrx.val[aPos] = vdup_n_u8(0xFF);
	}
	vst4_u8(pRGB, bgrx);
	pRGB += 32;
	return pRGB;
}

static INLINE pstatus_t neon_YUV420ToX(
    const BYTE* pSrc[3], const UINT32 srcStep[3],
    BYTE* pDst, UINT32 dstStep,
    const prim_size_t* roi, const uint8_t rPos,  const uint8_t gPos,
    const uint8_t bPos, const uint8_t aPos)
{
	UINT32 y;
	const UINT32 nWidth = roi->width;
	const UINT32 nHeight = roi->height;
	const DWORD pad = nWidth % 16;
	const UINT32 yPad = srcStep[0] - roi->width;
	const UINT32 uPad = srcStep[1] - roi->width / 2;
	const UINT32 vPad = srcStep[2] - roi->width / 2;
	const UINT32 dPad = dstStep - roi->width * 4;
	const int16x8_t c128 = vdupq_n_s16(128);

	for (y = 0; y < nHeight; y += 2)
	{
		const uint8_t* pY1 = pSrc[0] + y * srcStep[0];
		const uint8_t* pY2 = pY1 + srcStep[0];
		const uint8_t* pU = pSrc[1] + (y / 2) * srcStep[1];
		const uint8_t* pV = pSrc[2] + (y / 2) * srcStep[2];
		uint8_t* pRGB1 = pDst + y * dstStep;
		uint8_t* pRGB2 = pRGB1 + dstStep;
		UINT32 x;
		const BOOL lastY = y >= nHeight - 1;

		for (x = 0; x < nWidth - pad;)
		{
			const uint8x8_t Uraw = vld1_u8(pU);
			const uint8x8x2_t Uu = vzip_u8(Uraw, Uraw);
			const int16x8_t U1 = vreinterpretq_s16_u16(vmovl_u8(Uu.val[0]));
			const int16x8_t U2 = vreinterpretq_s16_u16(vmovl_u8(Uu.val[1]));
			const uint8x8_t Vraw = vld1_u8(pV);
			const uint8x8x2_t Vu = vzip_u8(Vraw, Vraw);
			const int16x8_t V1 = vreinterpretq_s16_u16(vmovl_u8(Vu.val[0]));
			const int16x8_t V2 = vreinterpretq_s16_u16(vmovl_u8(Vu.val[1]));
			const int16x8_t D1 = vsubq_s16(U1, c128);
			const int16x8_t E1 = vsubq_s16(V1, c128);
			const int16x8_t D2 = vsubq_s16(U2, c128);
			const int16x8_t E2 = vsubq_s16(V2, c128);
			{
				const uint8x8_t Y1u = vld1_u8(pY1);
				const int16x8_t Y1 = vreinterpretq_s16_u16(vmovl_u8(Y1u));
				pRGB1 = neon_YuvToRgbPixel(pRGB1, Y1, D1, E1, rPos, gPos, bPos, aPos);
				pY1 += 8;
				x += 8;
			}
			{
				const uint8x8_t Y1u = vld1_u8(pY1);
				const int16x8_t Y1 = vreinterpretq_s16_u16(vmovl_u8(Y1u));
				pRGB1 = neon_YuvToRgbPixel(pRGB1, Y1, D2, E2, rPos, gPos, bPos, aPos);
				pY1 += 8;
				x += 8;
			}

			if (!lastY)
			{
				{
					const uint8x8_t Y2u = vld1_u8(pY2);
					const int16x8_t Y2 = vreinterpretq_s16_u16(vmovl_u8(Y2u));
					pRGB2 = neon_YuvToRgbPixel(pRGB2, Y2, D1, E1, rPos, gPos, bPos, aPos);
					pY2 += 8;
				}
				{
					const uint8x8_t Y2u = vld1_u8(pY2);
					const int16x8_t Y2 = vreinterpretq_s16_u16(vmovl_u8(Y2u));
					pRGB2 = neon_YuvToRgbPixel(pRGB2, Y2, D2, E2, rPos, gPos, bPos, aPos);
					pY2 += 8;
				}
			}

			pU += 8;
			pV += 8;
		}

		for (; x < nWidth; x++)
		{
			const BYTE U = *pU;
			const BYTE V = *pV;
			{
				const BYTE Y = *pY1++;
				const BYTE r = YUV2R(Y, U, V);
				const BYTE g = YUV2G(Y, U, V);
				const BYTE b = YUV2B(Y, U, V);
				pRGB1[aPos] = 0xFF;
				pRGB1[rPos] = r;
				pRGB1[gPos] = g;
				pRGB1[bPos] = b;
				pRGB1 += 4;
			}

			if (!lastY)
			{
				const BYTE Y = *pY2++;
				const BYTE r = YUV2R(Y, U, V);
				const BYTE g = YUV2G(Y, U, V);
				const BYTE b = YUV2B(Y, U, V);
				pRGB2[aPos] = 0xFF;
				pRGB2[rPos] = r;
				pRGB2[gPos] = g;
				pRGB2[bPos] = b;
				pRGB2 += 4;
			}

			if (x % 2)
			{
				pU++;
				pV++;
			}
		}

		pRGB1 += dPad;
		pRGB2 += dPad;
		pY1 += yPad;
		pY2 += yPad;
		pU += uPad;
		pV += vPad;
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t neon_YUV420ToRGB_8u_P3AC4R(
    const BYTE* pSrc[3], const UINT32 srcStep[3],
    BYTE* pDst, UINT32 dstStep, UINT32 DstFormat,
    const prim_size_t* roi)
{
	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return neon_YUV420ToX(pSrc, srcStep, pDst, dstStep, roi, 2, 1, 0, 3);

		case PIXEL_FORMAT_RGBA32:
		case PIXEL_FORMAT_RGBX32:
			return neon_YUV420ToX(pSrc, srcStep, pDst, dstStep, roi, 0, 1, 2, 3);

		case PIXEL_FORMAT_ARGB32:
		case PIXEL_FORMAT_XRGB32:
			return neon_YUV420ToX(pSrc, srcStep, pDst, dstStep, roi, 1, 2, 3, 0);

		case PIXEL_FORMAT_ABGR32:
		case PIXEL_FORMAT_XBGR32:
			return neon_YUV420ToX(pSrc, srcStep, pDst, dstStep, roi, 3, 2, 1, 0);

		default:
			return generic->YUV420ToRGB_8u_P3AC4R(pSrc, srcStep, pDst, dstStep, DstFormat, roi);
	}
}

static INLINE pstatus_t neon_YUV444ToX(
    const BYTE* pSrc[3], const UINT32 srcStep[3],
    BYTE* pDst, UINT32 dstStep,
    const prim_size_t* roi, const uint8_t rPos,  const uint8_t gPos,
    const uint8_t bPos, const uint8_t aPos)
{
	UINT32 y;
	const UINT32 nWidth = roi->width;
	const UINT32 nHeight = roi->height;
	const UINT32 yPad = srcStep[0] - roi->width;
	const UINT32 uPad = srcStep[1] - roi->width;
	const UINT32 vPad = srcStep[2] - roi->width;
	const UINT32 dPad = dstStep - roi->width * 4;
	const uint8_t* pY = pSrc[0];
	const uint8_t* pU = pSrc[1];
	const uint8_t* pV = pSrc[2];
	uint8_t* pRGB = pDst;
	const int16x8_t c128 = vdupq_n_s16(128);
	const DWORD pad = nWidth % 8;

	for (y = 0; y < nHeight; y++)
	{
		UINT32 x;

		for (x = 0; x < nWidth - pad; x += 8)
		{
			const uint8x8_t Yu = vld1_u8(pY);
			const int16x8_t Y = vreinterpretq_s16_u16(vmovl_u8(Yu));
			const uint8x8_t Uu = vld1_u8(pU);
			const int16x8_t U = vreinterpretq_s16_u16(vmovl_u8(Uu));
			const uint8x8_t Vu = vld1_u8(pV);
			const int16x8_t V = vreinterpretq_s16_u16(vmovl_u8(Vu));
			/* Do the calculations on Y in 32bit width, the result of 255 * 256 does not fit
			 * a signed 16 bit value. */
			const int16x8_t D = vsubq_s16(U, c128);
			const int16x8_t E = vsubq_s16(V, c128);
			pRGB = neon_YuvToRgbPixel(pRGB, Y, D, E, rPos, gPos, bPos, aPos);
			pY += 8;
			pU += 8;
			pV += 8;
		}

		for (x = 0; x < pad; x++)
		{
			const BYTE Y = *pY++;
			const BYTE U = *pU++;
			const BYTE V = *pV++;
			const BYTE r = YUV2R(Y, U, V);
			const BYTE g = YUV2G(Y, U, V);
			const BYTE b = YUV2B(Y, U, V);
			pRGB[aPos] = 0xFF;
			pRGB[rPos] = r;
			pRGB[gPos] = g;
			pRGB[bPos] = b;
			pRGB += 4;
		}

		pRGB += dPad;
		pY += yPad;
		pU += uPad;
		pV += vPad;
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t neon_YUV444ToRGB_8u_P3AC4R(
    const BYTE* pSrc[3], const UINT32 srcStep[3],
    BYTE* pDst, UINT32 dstStep, UINT32 DstFormat,
    const prim_size_t* roi)
{
	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return neon_YUV444ToX(pSrc, srcStep, pDst, dstStep, roi, 2, 1, 0, 3);

		case PIXEL_FORMAT_RGBA32:
		case PIXEL_FORMAT_RGBX32:
			return neon_YUV444ToX(pSrc, srcStep, pDst, dstStep, roi, 0, 1, 2, 3);

		case PIXEL_FORMAT_ARGB32:
		case PIXEL_FORMAT_XRGB32:
			return neon_YUV444ToX(pSrc, srcStep, pDst, dstStep, roi, 1, 2, 3, 0);

		case PIXEL_FORMAT_ABGR32:
		case PIXEL_FORMAT_XBGR32:
			return neon_YUV444ToX(pSrc, srcStep, pDst, dstStep, roi, 3, 2, 1, 0);

		default:
			return generic->YUV444ToRGB_8u_P3AC4R(pSrc, srcStep, pDst, dstStep, DstFormat, roi);
	}
}

static pstatus_t neon_LumaToYUV444(const BYTE* pSrcRaw[3], const UINT32 srcStep[3],
                                   BYTE* pDstRaw[3], const UINT32 dstStep[3],
                                   const RECTANGLE_16* roi)
{
	UINT32 x, y;
	const UINT32 nWidth = roi->right - roi->left;
	const UINT32 nHeight = roi->bottom - roi->top;
	const UINT32 halfWidth = (nWidth + 1) / 2;
	const UINT32 halfHeight = (nHeight + 1) / 2;
	const UINT32 evenY = 0;
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
		const BYTE* Um = pSrc[1] + srcStep[1] * y;
		const BYTE* Vm = pSrc[2] + srcStep[2] * y;
		BYTE* pU = pDst[1] + dstStep[1] * val2y;
		BYTE* pV = pDst[2] + dstStep[2] * val2y;
		BYTE* pU1 = pU + dstStep[1];
		BYTE* pV1 = pV + dstStep[2];

		for (x = 0; x + 16 < halfWidth; x += 16)
		{
			{
				const uint8x16_t u = vld1q_u8(Um);
				uint8x16x2_t u2x;
				u2x.val[0] = u;
				u2x.val[1] = u;
				vst2q_u8(pU, u2x);
				vst2q_u8(pU1, u2x);
				Um += 16;
				pU += 32;
				pU1 += 32;
			}
			{
				const uint8x16_t v = vld1q_u8(Vm);
				uint8x16x2_t v2x;
				v2x.val[0] = v;
				v2x.val[1] = v;
				vst2q_u8(pV, v2x);
				vst2q_u8(pV1, v2x);
				Vm += 16;
				pV += 32;
				pV1 += 32;
			}
		}

		for (; x < halfWidth; x++)
		{
			const BYTE u = *Um++;
			const BYTE v = *Vm++;
			*pU++ = u;
			*pU++ = u;
			*pU1++ = u;
			*pU1++ = u;
			*pV++ = v;
			*pV++ = v;
			*pV1++ = v;
			*pV1++ = v;
		}
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t neon_ChromaFilter(BYTE* pDst[3], const UINT32 dstStep[3],
                                   const RECTANGLE_16* roi)
{
	const UINT32 oddY = 1;
	const UINT32 evenY = 0;
	const UINT32 nWidth = roi->right - roi->left;
	const UINT32 nHeight = roi->bottom - roi->top;
	const UINT32 halfHeight = (nHeight + 1) / 2;
	const UINT32 halfWidth = (nWidth + 1) / 2;
	const UINT32 halfPad = halfWidth % 16;
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

		for (x = roi->left / 2; x < halfWidth + roi->left / 2 - halfPad; x += 16)
		{
			{
				/* U = (U2x,2y << 2) - U2x1,2y - U2x,2y1 - U2x1,2y1 */
				uint8x8x2_t u = vld2_u8(&pU[2 * x]);
				const int16x8_t up = vreinterpretq_s16_u16(vshll_n_u8(u.val[0], 2)); /* Ux2,2y << 2 */
				const uint8x8x2_t u1 = vld2_u8(&pU1[2 * x]);
				const uint16x8_t usub = vaddl_u8(u1.val[1], u1.val[0]); /* U2x,2y1 + U2x1,2y1 */
				const int16x8_t us = vreinterpretq_s16_u16(vaddw_u8(usub,
				                     u.val[1])); /* U2x1,2y + U2x,2y1 + U2x1,2y1 */
				const int16x8_t un = vsubq_s16(up, us);
				const uint8x8_t u8 = vqmovun_s16(un); /* CLIP(un) */
				u.val[0] = u8;
				vst2_u8(&pU[2 * x], u);
			}
			{
				/* V = (V2x,2y << 2) - V2x1,2y - V2x,2y1 - V2x1,2y1 */
				uint8x8x2_t v = vld2_u8(&pV[2 * x]);
				const int16x8_t vp = vreinterpretq_s16_u16(vshll_n_u8(v.val[0], 2)); /* Vx2,2y << 2 */
				const uint8x8x2_t v1 = vld2_u8(&pV1[2 * x]);
				const uint16x8_t vsub = vaddl_u8(v1.val[1], v1.val[0]); /* V2x,2y1 + V2x1,2y1 */
				const int16x8_t vs = vreinterpretq_s16_u16(vaddw_u8(vsub,
				                     v.val[1])); /* V2x1,2y + V2x,2y1 + V2x1,2y1 */
				const int16x8_t vn = vsubq_s16(vp, vs);
				const uint8x8_t v8 = vqmovun_s16(vn); /* CLIP(vn) */
				v.val[0] = v8;
				vst2_u8(&pV[2 * x], v);
			}
		}

		for (; x < halfWidth + roi->left / 2; x++)
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

static pstatus_t neon_ChromaV1ToYUV444(const BYTE* pSrcRaw[3], const UINT32 srcStep[3],
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
	const UINT32 halfPad = halfWidth % 16;
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

		for (x = 0; x < halfWidth - halfPad; x += 16)
		{
			{
				uint8x16x2_t u = vld2q_u8(&pU[2 * x]);
				u.val[1] = vld1q_u8(&Ua[x]);
				vst2q_u8(&pU[2 * x], u);
			}
			{
				uint8x16x2_t v = vld2q_u8(&pV[2 * x]);
				v.val[1] = vld1q_u8(&Va[x]);
				vst2q_u8(&pV[2 * x], v);
			}
		}

		for (; x < halfWidth; x++)
		{
			const UINT32 val2x1 = (x * 2 + oddX);
			pU[val2x1] = Ua[x];
			pV[val2x1] = Va[x];
		}
	}

	/* Filter */
	return neon_ChromaFilter(pDst, dstStep, roi);
}

static pstatus_t neon_ChromaV2ToYUV444(const BYTE* pSrc[3], const UINT32 srcStep[3],
                                       UINT32 nTotalWidth, UINT32 nTotalHeight,
                                       BYTE* pDst[3], const UINT32 dstStep[3],
                                       const RECTANGLE_16* roi)
{
	UINT32 x, y;
	const UINT32 nWidth = roi->right - roi->left;
	const UINT32 nHeight = roi->bottom - roi->top;
	const UINT32 halfWidth = (nWidth + 1) / 2;
	const UINT32 halfPad = halfWidth % 16;
	const UINT32 halfHeight = (nHeight + 1) / 2;
	const UINT32 quaterWidth = (nWidth + 3) / 4;
	const UINT32 quaterPad = quaterWidth % 16;

	/* B4 and B5: odd UV values for width/2, height */
	for (y = 0; y < nHeight; y++)
	{
		const UINT32 yTop = y + roi->top;
		const BYTE* pYaU = pSrc[0] + srcStep[0] * yTop + roi->left / 2;
		const BYTE* pYaV = pYaU + nTotalWidth / 2;
		BYTE* pU = pDst[1] + dstStep[1] * yTop + roi->left;
		BYTE* pV = pDst[2] + dstStep[2] * yTop + roi->left;

		for (x = 0; x < halfWidth - halfPad; x += 16)
		{
			{
				uint8x16x2_t u = vld2q_u8(&pU[2 * x]);
				u.val[1] = vld1q_u8(&pYaU[x]);
				vst2q_u8(&pU[2 * x], u);
			}
			{
				uint8x16x2_t v = vld2q_u8(&pV[2 * x]);
				v.val[1] = vld1q_u8(&pYaV[x]);
				vst2q_u8(&pV[2 * x], v);
			}
		}

		for (; x < halfWidth; x++)
		{
			const UINT32 odd = 2 * x + 1;
			pU[odd] = pYaU[x];
			pV[odd] = pYaV[x];
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

		for (x = 0; x < quaterWidth - quaterPad; x += 16)
		{
			{
				uint8x16x4_t u = vld4q_u8(&pU[4 * x]);
				u.val[0] = vld1q_u8(&pUaU[x]);
				u.val[2] = vld1q_u8(&pVaU[x]);
				vst4q_u8(&pU[4 * x], u);
			}
			{
				uint8x16x4_t v = vld4q_u8(&pV[4 * x]);
				v.val[0] = vld1q_u8(&pUaV[x]);
				v.val[2] = vld1q_u8(&pVaV[x]);
				vst4q_u8(&pV[4 * x], v);
			}
		}

		for (; x < quaterWidth; x++)
		{
			pU[4 * x + 0] = pUaU[x];
			pV[4 * x + 0] = pUaV[x];
			pU[4 * x + 2] = pVaU[x];
			pV[4 * x + 2] = pVaV[x];
		}
	}

	return neon_ChromaFilter(pDst, dstStep, roi);
}

static pstatus_t neon_YUV420CombineToYUV444(
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
			return neon_LumaToYUV444(pSrc, srcStep, pDst, dstStep, roi);

		case AVC444_CHROMAv1:
			return neon_ChromaV1ToYUV444(pSrc, srcStep, pDst, dstStep, roi);

		case AVC444_CHROMAv2:
			return neon_ChromaV2ToYUV444(pSrc, srcStep, nWidth, nHeight, pDst, dstStep, roi);

		default:
			return -1;
	}
}

void primitives_init_YUV_opt(primitives_t* prims)
{
	generic = primitives_get_generic();
	primitives_init_YUV(prims);

	if (IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE))
	{
		prims->YUV420ToRGB_8u_P3AC4R = neon_YUV420ToRGB_8u_P3AC4R;
		prims->YUV444ToRGB_8u_P3AC4R = neon_YUV444ToRGB_8u_P3AC4R;
		prims->YUV420CombineToYUV444 = neon_YUV420CombineToYUV444;
	}
}
