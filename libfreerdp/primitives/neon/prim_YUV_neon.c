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

#include <freerdp/config.h>

#include <winpr/sysinfo.h>
#include <winpr/crt.h>
#include <freerdp/types.h>
#include <freerdp/primitives.h>

#include "prim_internal.h"
#include "prim_YUV.h"

#if defined(NEON_INTRINSICS_ENABLED)
#include <arm_neon.h>

static primitives_t* generic = nullptr;

static inline uint8x8_t neon_YUV2R_single(uint16x8_t C, int16x8_t D, int16x8_t E)
{
	/* R = (256 * Y + 403 * (V - 128)) >> 8 */
	const int32x4_t Ch = vreinterpretq_s32_u32(vmovl_u16(vget_high_u16(C)));
	const int32x4_t e403h = vmull_n_s16(vget_high_s16(E), 403);
	const int32x4_t cehm = vaddq_s32(Ch, e403h);
	const int32x4_t ceh = vshrq_n_s32(cehm, 8);

	const int32x4_t Cl = vreinterpretq_s32_u32(vmovl_u16(vget_low_u16(C)));
	const int32x4_t e403l = vmull_n_s16(vget_low_s16(E), 403);
	const int32x4_t celm = vaddq_s32(Cl, e403l);
	const int32x4_t cel = vshrq_n_s32(celm, 8);
	const int16x8_t ce = vcombine_s16(vqmovn_s32(cel), vqmovn_s32(ceh));
	return vqmovun_s16(ce);
}

static inline uint8x8x2_t neon_YUV2R(uint16x8x2_t C, int16x8x2_t D, int16x8x2_t E)
{
	uint8x8x2_t res = { { neon_YUV2R_single(C.val[0], D.val[0], E.val[0]),
		                  neon_YUV2R_single(C.val[1], D.val[1], E.val[1]) } };
	return res;
}

static inline uint8x8_t neon_YUV2G_single(uint16x8_t C, int16x8_t D, int16x8_t E)
{
	/* G = (256L * Y -  48 * (U - 128) - 120 * (V - 128)) >> 8 */
	const int16x8_t d48 = vmulq_n_s16(D, 48);
	const int16x8_t e120 = vmulq_n_s16(E, 120);
	const int32x4_t deh = vaddl_s16(vget_high_s16(d48), vget_high_s16(e120));
	const int32x4_t Ch = vreinterpretq_s32_u32(vmovl_u16(vget_high_u16(C)));
	const int32x4_t cdeh32m = vsubq_s32(Ch, deh);
	const int32x4_t cdeh32 = vshrq_n_s32(cdeh32m, 8);
	const int16x4_t cdeh = vqmovn_s32(cdeh32);

	const int32x4_t del = vaddl_s16(vget_low_s16(d48), vget_low_s16(e120));
	const int32x4_t Cl = vreinterpretq_s32_u32(vmovl_u16(vget_low_u16(C)));
	const int32x4_t cdel32m = vsubq_s32(Cl, del);
	const int32x4_t cdel32 = vshrq_n_s32(cdel32m, 8);
	const int16x4_t cdel = vqmovn_s32(cdel32);
	const int16x8_t cde = vcombine_s16(cdel, cdeh);
	return vqmovun_s16(cde);
}

static inline uint8x8x2_t neon_YUV2G(uint16x8x2_t C, int16x8x2_t D, int16x8x2_t E)
{
	uint8x8x2_t res = { { neon_YUV2G_single(C.val[0], D.val[0], E.val[0]),
		                  neon_YUV2G_single(C.val[1], D.val[1], E.val[1]) } };
	return res;
}

static inline uint8x8_t neon_YUV2B_single(uint16x8_t C, int16x8_t D, int16x8_t E)
{
	/* B = (256L * Y + 475 * (U - 128)) >> 8*/
	const int32x4_t Ch = vreinterpretq_s32_u32(vmovl_u16(vget_high_u16(C)));
	const int32x4_t d475h = vmull_n_s16(vget_high_s16(D), 475);
	const int32x4_t cdhm = vaddq_s32(Ch, d475h);
	const int32x4_t cdh = vshrq_n_s32(cdhm, 8);

	const int32x4_t Cl = vreinterpretq_s32_u32(vmovl_u16(vget_low_u16(C)));
	const int32x4_t d475l = vmull_n_s16(vget_low_s16(D), 475);
	const int32x4_t cdlm = vaddq_s32(Cl, d475l);
	const int32x4_t cdl = vshrq_n_s32(cdlm, 8);
	const int16x8_t cd = vcombine_s16(vqmovn_s32(cdl), vqmovn_s32(cdh));
	return vqmovun_s16(cd);
}

static inline uint8x8x2_t neon_YUV2B(uint16x8x2_t C, int16x8x2_t D, int16x8x2_t E)
{
	uint8x8x2_t res = { { neon_YUV2B_single(C.val[0], D.val[0], E.val[0]),
		                  neon_YUV2B_single(C.val[1], D.val[1], E.val[1]) } };
	return res;
}

static inline void neon_store_bgrx(BYTE* WINPR_RESTRICT pRGB, uint8x8_t r, uint8x8_t g, uint8x8_t b,
                                   uint8_t rPos, uint8_t gPos, uint8_t bPos, uint8_t aPos)
{
	uint8x8x4_t bgrx = vld4_u8(pRGB);
	bgrx.val[rPos] = r;
	bgrx.val[gPos] = g;
	bgrx.val[bPos] = b;
	vst4_u8(pRGB, bgrx);
}

static inline void neon_YuvToRgbPixel(BYTE* pRGB, uint8x8x2_t Y, int16x8x2_t D, int16x8x2_t E,
                                      const uint8_t rPos, const uint8_t gPos, const uint8_t bPos,
                                      const uint8_t aPos)
{
	/* Y * 256 == Y << 8  */
	const uint16x8x2_t C = { { vshlq_n_u16(vmovl_u8(Y.val[0]), 8),
		                       vshlq_n_u16(vmovl_u8(Y.val[1]), 8) } };

	const uint8x8x2_t r = neon_YUV2R(C, D, E);
	const uint8x8x2_t g = neon_YUV2G(C, D, E);
	const uint8x8x2_t b = neon_YUV2B(C, D, E);

	neon_store_bgrx(pRGB, r.val[0], g.val[0], b.val[0], rPos, gPos, bPos, aPos);
	neon_store_bgrx(pRGB + sizeof(uint8x8x4_t), r.val[1], g.val[1], b.val[1], rPos, gPos, bPos,
	                aPos);
}

static inline int16x8x2_t loadUV(const BYTE* WINPR_RESTRICT pV, size_t x)
{
	const uint8x8_t Vraw = vld1_u8(&pV[x / 2]);
	const int16x8_t V = vreinterpretq_s16_u16(vmovl_u8(Vraw));
	const int16x8_t c128 = vdupq_n_s16(128);
	const int16x8_t E = vsubq_s16(V, c128);
	return vzipq_s16(E, E);
}

static inline void neon_write_pixel(BYTE* pRGB, BYTE Y, BYTE U, BYTE V, const uint8_t rPos,
                                    const uint8_t gPos, const uint8_t bPos, const uint8_t aPos)
{
	const BYTE r = YUV2R(Y, U, V);
	const BYTE g = YUV2G(Y, U, V);
	const BYTE b = YUV2B(Y, U, V);

	pRGB[rPos] = r;
	pRGB[gPos] = g;
	pRGB[bPos] = b;
}

static inline void neon_YUV420ToX_DOUBLE_ROW(const BYTE* WINPR_RESTRICT pY[2],
                                             const BYTE* WINPR_RESTRICT pU,
                                             const BYTE* WINPR_RESTRICT pV,
                                             BYTE* WINPR_RESTRICT pRGB[2], size_t width,
                                             const uint8_t rPos, const uint8_t gPos,
                                             const uint8_t bPos, const uint8_t aPos)
{
	UINT32 x = 0;

	for (; x < width - width % 16; x += 16)
	{
		const uint8x16_t Y0raw = vld1q_u8(&pY[0][x]);
		const uint8x8x2_t Y0 = { { vget_low_u8(Y0raw), vget_high_u8(Y0raw) } };
		const int16x8x2_t D = loadUV(pU, x);
		const int16x8x2_t E = loadUV(pV, x);
		neon_YuvToRgbPixel(&pRGB[0][4ULL * x], Y0, D, E, rPos, gPos, bPos, aPos);

		const uint8x16_t Y1raw = vld1q_u8(&pY[1][x]);
		const uint8x8x2_t Y1 = { { vget_low_u8(Y1raw), vget_high_u8(Y1raw) } };
		neon_YuvToRgbPixel(&pRGB[1][4ULL * x], Y1, D, E, rPos, gPos, bPos, aPos);
	}

	for (; x < width - width % 2; x += 2)
	{
		const BYTE U = pU[x / 2];
		const BYTE V = pV[x / 2];

		neon_write_pixel(&pRGB[0][4 * x], pY[0][x], U, V, rPos, gPos, bPos, aPos);
		neon_write_pixel(&pRGB[0][4 * (1ULL + x)], pY[0][1ULL + x], U, V, rPos, gPos, bPos, aPos);
		neon_write_pixel(&pRGB[1][4 * x], pY[1][x], U, V, rPos, gPos, bPos, aPos);
		neon_write_pixel(&pRGB[1][4 * (1ULL + x)], pY[1][1ULL + x], U, V, rPos, gPos, bPos, aPos);
	}

	for (; x < width; x++)
	{
		const BYTE U = pU[x / 2];
		const BYTE V = pV[x / 2];

		neon_write_pixel(&pRGB[0][4 * x], pY[0][x], U, V, rPos, gPos, bPos, aPos);
		neon_write_pixel(&pRGB[1][4 * x], pY[1][x], U, V, rPos, gPos, bPos, aPos);
	}
}

static inline void neon_YUV420ToX_SINGLE_ROW(const BYTE* WINPR_RESTRICT pY,
                                             const BYTE* WINPR_RESTRICT pU,
                                             const BYTE* WINPR_RESTRICT pV,
                                             BYTE* WINPR_RESTRICT pRGB, size_t width,
                                             const uint8_t rPos, const uint8_t gPos,
                                             const uint8_t bPos, const uint8_t aPos)
{
	UINT32 x = 0;

	for (; x < width - width % 16; x += 16)
	{
		const uint8x16_t Y0raw = vld1q_u8(&pY[x]);
		const uint8x8x2_t Y0 = { { vget_low_u8(Y0raw), vget_high_u8(Y0raw) } };
		const int16x8x2_t D = loadUV(pU, x);
		const int16x8x2_t E = loadUV(pV, x);
		neon_YuvToRgbPixel(&pRGB[4ULL * x], Y0, D, E, rPos, gPos, bPos, aPos);
	}

	for (; x < width - width % 2; x += 2)
	{
		const BYTE U = pU[x / 2];
		const BYTE V = pV[x / 2];

		neon_write_pixel(&pRGB[4 * x], pY[x], U, V, rPos, gPos, bPos, aPos);
		neon_write_pixel(&pRGB[4 * (1ULL + x)], pY[1ULL + x], U, V, rPos, gPos, bPos, aPos);
	}
	for (; x < width; x++)
	{
		const BYTE U = pU[x / 2];
		const BYTE V = pV[x / 2];

		neon_write_pixel(&pRGB[4 * x], pY[x], U, V, rPos, gPos, bPos, aPos);
	}
}

static inline pstatus_t neon_YUV420ToX(const BYTE* WINPR_RESTRICT pSrc[3], const UINT32 srcStep[3],
                                       BYTE* WINPR_RESTRICT pDst, UINT32 dstStep,
                                       const prim_size_t* WINPR_RESTRICT roi, const uint8_t rPos,
                                       const uint8_t gPos, const uint8_t bPos, const uint8_t aPos)
{
	const UINT32 nWidth = roi->width;
	const UINT32 nHeight = roi->height;

	WINPR_ASSERT(nHeight > 0);
	UINT32 y = 0;
	for (; y < (nHeight - 1); y += 2)
	{
		const uint8_t* pY[2] = { pSrc[0] + y * srcStep[0], pSrc[0] + (1ULL + y) * srcStep[0] };
		const uint8_t* pU = pSrc[1] + (y / 2) * srcStep[1];
		const uint8_t* pV = pSrc[2] + (y / 2) * srcStep[2];
		uint8_t* pRGB[2] = { pDst + y * dstStep, pDst + (1ULL + y) * dstStep };

		neon_YUV420ToX_DOUBLE_ROW(pY, pU, pV, pRGB, nWidth, rPos, gPos, bPos, aPos);
	}
	for (; y < nHeight; y++)
	{
		const uint8_t* pY = pSrc[0] + y * srcStep[0];
		const uint8_t* pU = pSrc[1] + (y / 2) * srcStep[1];
		const uint8_t* pV = pSrc[2] + (y / 2) * srcStep[2];
		uint8_t* pRGB = pDst + y * dstStep;

		neon_YUV420ToX_SINGLE_ROW(pY, pU, pV, pRGB, nWidth, rPos, gPos, bPos, aPos);
	}
	return PRIMITIVES_SUCCESS;
}

static pstatus_t neon_YUV420ToRGB_8u_P3AC4R(const BYTE* WINPR_RESTRICT pSrc[3],
                                            const UINT32 srcStep[3], BYTE* WINPR_RESTRICT pDst,
                                            UINT32 dstStep, UINT32 DstFormat,
                                            const prim_size_t* WINPR_RESTRICT roi)
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

static inline int16x8_t loadUVreg(uint8x8_t Vraw)
{
	const int16x8_t V = vreinterpretq_s16_u16(vmovl_u8(Vraw));
	const int16x8_t c128 = vdupq_n_s16(128);
	const int16x8_t E = vsubq_s16(V, c128);
	return E;
}

static inline int16x8x2_t loadUV444(uint8x16_t Vld)
{
	const uint8x8x2_t V = { { vget_low_u8(Vld), vget_high_u8(Vld) } };
	const int16x8x2_t res = { {
		loadUVreg(V.val[0]),
		loadUVreg(V.val[1]),
	} };
	return res;
}

static inline void avgUV(BYTE U[2][2])
{
	const BYTE u00 = U[0][0];
	const INT16 umul = (INT16)u00 << 2;
	const INT16 sum = (INT16)U[0][1] + U[1][0] + U[1][1];
	const INT16 wavg = umul - sum;
	const BYTE val = CONDITIONAL_CLIP(wavg, u00);
	U[0][0] = val;
}

static inline void neon_avgUV(uint8x16_t pU[2])
{
	/* put even and odd values into different registers.
	 * U 0/0 is in lower half */
	const uint8x16x2_t usplit = vuzpq_u8(pU[0], pU[1]);
	const uint8x16_t ueven = usplit.val[0];
	const uint8x16_t uodd = usplit.val[1];

	const uint8x8_t u00 = vget_low_u8(ueven);
	const uint8x8_t u01 = vget_low_u8(uodd);
	const uint8x8_t u10 = vget_high_u8(ueven);
	const uint8x8_t u11 = vget_high_u8(uodd);

	/* Create sum of U01 + U10 + U11 */
	const uint16x8_t uoddsum = vaddl_u8(u01, u10);
	const uint16x8_t usum = vaddq_u16(uoddsum, vmovl_u8(u11));

	/* U00 * 4 */
	const uint16x8_t umul = vshll_n_u8(u00, 2);

	/* U00 - (U01 + U10 + U11) */
	const int16x8_t wavg = vsubq_s16(vreinterpretq_s16_u16(umul), vreinterpretq_s16_u16(usum));
	const uint8x8_t avg = vqmovun_s16(wavg);

	/* abs(u00 - avg) */
	const uint8x8_t absdiff = vabd_u8(avg, u00);

	/* (diff < 30) ? u00 : avg */
	const uint8x8_t mask = vclt_u8(absdiff, vdup_n_u8(30));

	/* out1 = u00 & mask */
	const uint8x8_t out1 = vand_u8(u00, mask);

	/* invmask = ~mask */
	const uint8x8_t notmask = vmvn_u8(mask);

	/* out2 = avg & invmask */
	const uint8x8_t out2 = vand_u8(avg, notmask);

	/* out = out1 | out2 */
	const uint8x8_t out = vorr_u8(out1, out2);

	const uint8x8x2_t ua = vzip_u8(out, u01);
	const uint8x16_t u = vcombine_u8(ua.val[0], ua.val[1]);
	pU[0] = u;
}

static inline pstatus_t neon_YUV444ToX_SINGLE_ROW(const BYTE* WINPR_RESTRICT pY,
                                                  const BYTE* WINPR_RESTRICT pU,
                                                  const BYTE* WINPR_RESTRICT pV,
                                                  BYTE* WINPR_RESTRICT pRGB, size_t width,
                                                  const uint8_t rPos, const uint8_t gPos,
                                                  const uint8_t bPos, const uint8_t aPos)
{
	size_t x = 0;

	for (; x < width - width % 16; x += 16)
	{
		uint8x16_t U = vld1q_u8(&pU[x]);
		uint8x16_t V = vld1q_u8(&pV[x]);
		const uint8x16_t Y0raw = vld1q_u8(&pY[x]);
		const uint8x8x2_t Y0 = { { vget_low_u8(Y0raw), vget_high_u8(Y0raw) } };
		const int16x8x2_t D0 = loadUV444(U);
		const int16x8x2_t E0 = loadUV444(V);
		neon_YuvToRgbPixel(&pRGB[4ULL * x], Y0, D0, E0, rPos, gPos, bPos, aPos);
	}

	for (; x < width; x += 2)
	{
		BYTE* rgb = &pRGB[x * 4];

		for (size_t j = 0; j < 2; j++)
		{
			const BYTE y = pY[x + j];
			const BYTE u = pU[x + j];
			const BYTE v = pV[x + j];

			neon_write_pixel(&rgb[4 * (j)], y, u, v, rPos, gPos, bPos, aPos);
		}
	}

	return PRIMITIVES_SUCCESS;
}

static inline pstatus_t neon_YUV444ToX_DOUBLE_ROW(const BYTE* WINPR_RESTRICT pY[2],
                                                  const BYTE* WINPR_RESTRICT pU[2],
                                                  const BYTE* WINPR_RESTRICT pV[2],
                                                  BYTE* WINPR_RESTRICT pRGB[2], size_t width,
                                                  const uint8_t rPos, const uint8_t gPos,
                                                  const uint8_t bPos, const uint8_t aPos)
{
	size_t x = 0;

	for (; x < width - width % 16; x += 16)
	{
		uint8x16_t U[2] = { vld1q_u8(&pU[0][x]), vld1q_u8(&pU[1][x]) };
		neon_avgUV(U);

		uint8x16_t V[2] = { vld1q_u8(&pV[0][x]), vld1q_u8(&pV[1][x]) };
		neon_avgUV(V);

		const uint8x16_t Y0raw = vld1q_u8(&pY[0][x]);
		const uint8x8x2_t Y0 = { { vget_low_u8(Y0raw), vget_high_u8(Y0raw) } };
		const int16x8x2_t D0 = loadUV444(U[0]);
		const int16x8x2_t E0 = loadUV444(V[0]);
		neon_YuvToRgbPixel(&pRGB[0][4ULL * x], Y0, D0, E0, rPos, gPos, bPos, aPos);

		const uint8x16_t Y1raw = vld1q_u8(&pY[1][x]);
		const uint8x8x2_t Y1 = { { vget_low_u8(Y1raw), vget_high_u8(Y1raw) } };
		const int16x8x2_t D1 = loadUV444(U[1]);
		const int16x8x2_t E1 = loadUV444(V[1]);
		neon_YuvToRgbPixel(&pRGB[1][4ULL * x], Y1, D1, E1, rPos, gPos, bPos, aPos);
	}

	for (; x < width; x += 2)
	{
		BYTE* rgb[2] = { &pRGB[0][x * 4], &pRGB[1][x * 4] };
		BYTE U[2][2] = { { pU[0][x], pU[0][x + 1] }, { pU[1][x], pU[1][x + 1] } };
		avgUV(U);

		BYTE V[2][2] = { { pV[0][x], pV[0][x + 1] }, { pV[1][x], pV[1][x + 1] } };
		avgUV(V);

		for (size_t i = 0; i < 2; i++)
		{
			for (size_t j = 0; j < 2; j++)
			{
				const BYTE y = pY[i][x + j];
				const BYTE u = U[i][j];
				const BYTE v = V[i][j];

				neon_write_pixel(&rgb[i][4 * (j)], y, u, v, rPos, gPos, bPos, aPos);
			}
		}
	}

	return PRIMITIVES_SUCCESS;
}

static inline pstatus_t neon_YUV444ToX(const BYTE* WINPR_RESTRICT pSrc[3], const UINT32 srcStep[3],
                                       BYTE* WINPR_RESTRICT pDst, UINT32 dstStep,
                                       const prim_size_t* WINPR_RESTRICT roi, const uint8_t rPos,
                                       const uint8_t gPos, const uint8_t bPos, const uint8_t aPos)
{
	WINPR_ASSERT(roi);
	const UINT32 nWidth = roi->width;
	const UINT32 nHeight = roi->height;

	size_t y = 0;
	for (; y < nHeight - nHeight % 2; y += 2)
	{
		const uint8_t* WINPR_RESTRICT pY[2] = { pSrc[0] + y * srcStep[0],
			                                    pSrc[0] + (y + 1) * srcStep[0] };
		const uint8_t* WINPR_RESTRICT pU[2] = { pSrc[1] + y * srcStep[1],
			                                    pSrc[1] + (y + 1) * srcStep[1] };
		const uint8_t* WINPR_RESTRICT pV[2] = { pSrc[2] + y * srcStep[2],
			                                    pSrc[2] + (y + 1) * srcStep[2] };

		uint8_t* WINPR_RESTRICT pRGB[2] = { &pDst[y * dstStep], &pDst[(y + 1) * dstStep] };

		const pstatus_t rc =
		    neon_YUV444ToX_DOUBLE_ROW(pY, pU, pV, pRGB, nWidth, rPos, gPos, bPos, aPos);
		if (rc != PRIMITIVES_SUCCESS)
			return rc;
	}
	for (; y < nHeight; y++)
	{
		const uint8_t* WINPR_RESTRICT pY = pSrc[0] + y * srcStep[0];
		const uint8_t* WINPR_RESTRICT pU = pSrc[1] + y * srcStep[1];
		const uint8_t* WINPR_RESTRICT pV = pSrc[2] + y * srcStep[2];
		uint8_t* WINPR_RESTRICT pRGB = &pDst[y * dstStep];

		const pstatus_t rc =
		    neon_YUV444ToX_SINGLE_ROW(pY, pU, pV, pRGB, nWidth, rPos, gPos, bPos, aPos);
		if (rc != PRIMITIVES_SUCCESS)
			return rc;
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t neon_YUV444ToRGB_8u_P3AC4R(const BYTE* WINPR_RESTRICT pSrc[3],
                                            const UINT32 srcStep[3], BYTE* WINPR_RESTRICT pDst,
                                            UINT32 dstStep, UINT32 DstFormat,
                                            const prim_size_t* WINPR_RESTRICT roi)
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

static inline void neon_I444ToX_ROW(const BYTE* WINPR_RESTRICT pY, const BYTE* WINPR_RESTRICT pU,
                                    const BYTE* WINPR_RESTRICT pV, BYTE* WINPR_RESTRICT pRGB,
                                    size_t width, const uint8_t rPos, const uint8_t gPos,
                                    const uint8_t bPos, const uint8_t aPos)
{
	size_t x = 0;

	for (; x < width - width % 16; x += 16)
	{
		const uint8x16_t Yraw = vld1q_u8(&pY[x]);
		const uint8x8x2_t Y = { { vget_low_u8(Yraw), vget_high_u8(Yraw) } };
		const int16x8x2_t D = loadUV444(vld1q_u8(&pU[x]));
		const int16x8x2_t E = loadUV444(vld1q_u8(&pV[x]));
		neon_YuvToRgbPixel(&pRGB[4ULL * x], Y, D, E, rPos, gPos, bPos, aPos);
	}

	for (; x < width; x++)
		neon_write_pixel(&pRGB[4ULL * x], pY[x], pU[x], pV[x], rPos, gPos, bPos, aPos);
}

static inline pstatus_t neon_I444ToX(const BYTE* WINPR_RESTRICT pSrc[3], const UINT32 srcStep[3],
                                     BYTE* WINPR_RESTRICT pDst, UINT32 dstStep,
                                     const prim_size_t* WINPR_RESTRICT roi, const uint8_t rPos,
                                     const uint8_t gPos, const uint8_t bPos, const uint8_t aPos)
{
	WINPR_ASSERT(roi);

	for (size_t y = 0; y < roi->height; y++)
	{
		const uint8_t* WINPR_RESTRICT pY = pSrc[0] + y * srcStep[0];
		const uint8_t* WINPR_RESTRICT pU = pSrc[1] + y * srcStep[1];
		const uint8_t* WINPR_RESTRICT pV = pSrc[2] + y * srcStep[2];
		uint8_t* WINPR_RESTRICT pRGB = &pDst[y * dstStep];

		neon_I444ToX_ROW(pY, pU, pV, pRGB, roi->width, rPos, gPos, bPos, aPos);
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t neon_I444ToRGB_8u(const BYTE* WINPR_RESTRICT pSrc[3], const UINT32 srcStep[3],
                                   BYTE* WINPR_RESTRICT pDst, UINT32 dstStep, UINT32 DstFormat,
                                   const prim_size_t* WINPR_RESTRICT roi)
{
	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return neon_I444ToX(pSrc, srcStep, pDst, dstStep, roi, 2, 1, 0, 3);

		case PIXEL_FORMAT_RGBA32:
		case PIXEL_FORMAT_RGBX32:
			return neon_I444ToX(pSrc, srcStep, pDst, dstStep, roi, 0, 1, 2, 3);

		case PIXEL_FORMAT_ARGB32:
		case PIXEL_FORMAT_XRGB32:
			return neon_I444ToX(pSrc, srcStep, pDst, dstStep, roi, 1, 2, 3, 0);

		case PIXEL_FORMAT_ABGR32:
		case PIXEL_FORMAT_XBGR32:
			return neon_I444ToX(pSrc, srcStep, pDst, dstStep, roi, 3, 2, 1, 0);

		default:
			return generic->I444ToRGB_8u(pSrc, srcStep, pDst, dstStep, DstFormat, roi);
	}
}

/* RGBToYUV420_8u_P3AC4R: Y for every pixel, U and V from the average R, G and B of
 * each 2x2 block, with the exact integer math of the generic routine (RGB2Y,
 * RGB2U, RGB2V; the average is the sum of four >> 2), so the result matches it
 * byte for byte. 16 bit lanes are wide enough for every intermediate value. */

/* the low byte of 16 bit lanes: (54 R + 183 G + 18 B) >> 8, at most 65025 */
static inline uint8x8_t neon_YUV420_Y8(uint16x8_t r, uint16x8_t g, uint16x8_t b)
{
	uint16x8_t t = vmulq_n_u16(r, 54);
	t = vmlaq_n_u16(t, g, 183);
	t = vmlaq_n_u16(t, b, 18);
	return vshrn_n_u16(t, 8);
}

static inline uint8x16_t neon_YUV420_Y(uint8x16_t r, uint8x16_t g, uint8x16_t b)
{
	const uint8x8_t lo = neon_YUV420_Y8(vmovl_u8(vget_low_u8(r)), vmovl_u8(vget_low_u8(g)),
	                                    vmovl_u8(vget_low_u8(b)));
	const uint8x8_t hi = neon_YUV420_Y8(vmovl_u8(vget_high_u8(r)), vmovl_u8(vget_high_u8(g)),
	                                    vmovl_u8(vget_high_u8(b)));
	return vcombine_u8(lo, hi);
}

/* U and V of eight 2x2 blocks given as their average R, G and B in 16 bit lanes:
 * ((128 B - (29 R + 99 G)) >> 8) + 128 and ((128 R - (116 G + 12 B)) >> 8) + 128,
 * both terms at most 32640 */
static inline void neon_YUV420_UV(uint16x8_t r, uint16x8_t g, uint16x8_t b,
                                  uint8x8_t* WINPR_RESTRICT u, uint8x8_t* WINPR_RESTRICT v)
{
	const int16x8_t c128 = vdupq_n_s16(128);
	const int16x8_t rs = vreinterpretq_s16_u16(r);
	const int16x8_t gs = vreinterpretq_s16_u16(g);
	const int16x8_t bs = vreinterpretq_s16_u16(b);

	int16x8_t s = vmlaq_n_s16(vmulq_n_s16(rs, 29), gs, 99);
	s = vsubq_s16(vshlq_n_s16(bs, 7), s);
	*u = vqmovun_s16(vaddq_s16(vshrq_n_s16(s, 8), c128));

	s = vmlaq_n_s16(vmulq_n_s16(gs, 116), bs, 12);
	s = vsubq_s16(vshlq_n_s16(rs, 7), s);
	*v = vqmovun_s16(vaddq_s16(vshrq_n_s16(s, 8), c128));
}

/* one of the four byte planes of vld4q_u8, picked with constant indices: indexing
 * px.val[] with a variable makes the compiler spill all four to the stack */
static inline uint8x16_t neon_YUV420_plane(const uint8x16x4_t px, const size_t off)
{
	switch (off)
	{
		case 0:
			return px.val[0];
		case 1:
			return px.val[1];
		case 2:
			return px.val[2];
		default:
			return px.val[3];
	}
}

static inline pstatus_t neon_RGBToYUV420_32bpp(const BYTE* WINPR_RESTRICT pSrc, UINT32 SrcFormat,
                                               UINT32 srcStep, BYTE* WINPR_RESTRICT pDst[3],
                                               const UINT32 dstStep[3],
                                               const prim_size_t* WINPR_RESTRICT roi,
                                               const size_t rOff, const size_t bOff)
{
	const size_t gOff = 1;
	/* the vector loop takes 16x2 pixel blocks; the generic routine does the
	 * columns right of them and an odd last row, so the edges keep its exact
	 * behaviour */
	const size_t width = roi->width - roi->width % 16;
	const size_t height = roi->height - roi->height % 2;

	for (size_t row = 0; row < height; row += 2)
	{
		const BYTE* src0 = &pSrc[row * srcStep];
		const BYTE* src1 = &pSrc[(row + 1) * srcStep];
		BYTE* y0 = &pDst[0][row * dstStep[0]];
		BYTE* y1 = &pDst[0][(row + 1) * dstStep[0]];
		BYTE* ud = &pDst[1][(row / 2) * dstStep[1]];
		BYTE* vd = &pDst[2][(row / 2) * dstStep[2]];

		for (size_t x = 0; x < width; x += 16)
		{
			const uint8x16x4_t p0 = vld4q_u8(&src0[4 * x]);
			const uint8x16x4_t p1 = vld4q_u8(&src1[4 * x]);
			const uint8x16_t r0 = neon_YUV420_plane(p0, rOff);
			const uint8x16_t g0 = neon_YUV420_plane(p0, gOff);
			const uint8x16_t b0 = neon_YUV420_plane(p0, bOff);
			const uint8x16_t r1 = neon_YUV420_plane(p1, rOff);
			const uint8x16_t g1 = neon_YUV420_plane(p1, gOff);
			const uint8x16_t b1 = neon_YUV420_plane(p1, bOff);

			vst1q_u8(&y0[x], neon_YUV420_Y(r0, g0, b0));
			vst1q_u8(&y1[x], neon_YUV420_Y(r1, g1, b1));

			/* the sum of each 2x2 block: pairs in a row, then the two rows */
			const uint16x8_t r = vshrq_n_u16(vpadalq_u8(vpaddlq_u8(r0), r1), 2);
			const uint16x8_t g = vshrq_n_u16(vpadalq_u8(vpaddlq_u8(g0), g1), 2);
			const uint16x8_t b = vshrq_n_u16(vpadalq_u8(vpaddlq_u8(b0), b1), 2);
			uint8x8_t u = vdup_n_u8(0);
			uint8x8_t v = vdup_n_u8(0);
			neon_YUV420_UV(r, g, b, &u, &v);
			vst1_u8(&ud[x / 2], u);
			vst1_u8(&vd[x / 2], v);
		}
	}

	/* the columns right of the vector blocks, every row */
	if (width < roi->width)
	{
		const BYTE* src = &pSrc[4 * width];
		BYTE* dst[3] = { &pDst[0][width], &pDst[1][width / 2], &pDst[2][width / 2] };
		const prim_size_t rest = { roi->width - (UINT32)width, roi->height };
		const pstatus_t rc =
		    generic->RGBToYUV420_8u_P3AC4R(src, SrcFormat, srcStep, dst, dstStep, &rest);
		if (rc != PRIMITIVES_SUCCESS)
			return rc;
	}

	/* an odd last row, left of those columns */
	if ((height < roi->height) && (width > 0))
	{
		const BYTE* src = &pSrc[height * srcStep];
		BYTE* dst[3] = { &pDst[0][height * dstStep[0]], &pDst[1][(height / 2) * dstStep[1]],
			             &pDst[2][(height / 2) * dstStep[2]] };
		const prim_size_t rest = { (UINT32)width, 1 };
		return generic->RGBToYUV420_8u_P3AC4R(src, SrcFormat, srcStep, dst, dstStep, &rest);
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t neon_RGBToYUV420_8u_P3AC4R(const BYTE* WINPR_RESTRICT pSrc, UINT32 SrcFormat,
                                            UINT32 srcStep, BYTE* WINPR_RESTRICT pDst[3],
                                            const UINT32 dstStep[3],
                                            const prim_size_t* WINPR_RESTRICT roi)
{
	/* FreeRDPReadColor reads 32 bit pixels most significant byte first: the
	 * first channel of the format name is the first byte in memory. The other
	 * formats average differently in the generic routine, so they stay there. */
	switch (SrcFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return neon_RGBToYUV420_32bpp(pSrc, SrcFormat, srcStep, pDst, dstStep, roi, 2, 0);
		case PIXEL_FORMAT_RGBA32:
		case PIXEL_FORMAT_RGBX32:
			return neon_RGBToYUV420_32bpp(pSrc, SrcFormat, srcStep, pDst, dstStep, roi, 0, 2);
		default:
			return generic->RGBToYUV420_8u_P3AC4R(pSrc, SrcFormat, srcStep, pDst, dstStep, roi);
	}
}

/* RGBToAVC444YUV and RGBToAVC444YUVv2 for BGRX/BGRA: Y, U and V of every pixel with
 * the exact integer math of the generic routines, then laid out in the main and
 * auxiliary views as they do, so the result matches them byte for byte. The
 * columns right of the last 16 pixel block go to the generic
 * general_RGBToAVC444YUV*_BGRX_DOUBLE_ROW, an odd last row too: they keep the
 * generic edge behaviour. */

/* Y, U and V of sixteen BGRX pixels */
static inline void neon_AVC444_row16(const BYTE* WINPR_RESTRICT src, uint8x16_t* WINPR_RESTRICT y,
                                     uint8x16_t* WINPR_RESTRICT u, uint8x16_t* WINPR_RESTRICT v)
{
	const uint8x16x4_t px = vld4q_u8(src);
	const uint8x16_t r = px.val[2];
	const uint8x16_t g = px.val[1];
	const uint8x16_t b = px.val[0];
	uint8x8_t ul = vdup_n_u8(0);
	uint8x8_t vl = vdup_n_u8(0);
	uint8x8_t uh = vdup_n_u8(0);
	uint8x8_t vh = vdup_n_u8(0);

	*y = neon_YUV420_Y(r, g, b);
	neon_YUV420_UV(vmovl_u8(vget_low_u8(r)), vmovl_u8(vget_low_u8(g)), vmovl_u8(vget_low_u8(b)),
	               &ul, &vl);
	neon_YUV420_UV(vmovl_u8(vget_high_u8(r)), vmovl_u8(vget_high_u8(g)), vmovl_u8(vget_high_u8(b)),
	               &uh, &vh);
	*u = vcombine_u8(ul, uh);
	*v = vcombine_u8(vl, vh);
}

/* (a + b + c + d) / 4 of each 2x2 block of two rows of samples */
static inline uint8x8_t neon_AVC444_avg(uint8x16_t even, uint8x16_t odd)
{
	return vmovn_u16(vshrq_n_u16(vpadalq_u8(vpaddlq_u8(even), odd), 2));
}

/* the samples of the even or of the odd columns */
static inline uint8x8_t neon_AVC444_even(uint8x16_t a)
{
	return vget_low_u8(vuzpq_u8(a, a).val[0]);
}

static inline uint8x8_t neon_AVC444_odd(uint8x16_t a)
{
	return vget_low_u8(vuzpq_u8(a, a).val[1]);
}

/* store the first four bytes of a */
static inline void neon_AVC444_store4(BYTE* WINPR_RESTRICT dst, uint8x8_t a)
{
	BYTE tmp[8];
	vst1_u8(tmp, a);
	memcpy(dst, tmp, 4);
}

static pstatus_t neon_RGBToAVC444YUV_BGRX(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcStep,
                                          BYTE* WINPR_RESTRICT pDst1[3], const UINT32 dst1Step[3],
                                          BYTE* WINPR_RESTRICT pDst2[3], const UINT32 dst2Step[3],
                                          const prim_size_t* WINPR_RESTRICT roi)
{
	const size_t width = roi->width - roi->width % 16;

	size_t y = 0;
	for (; y < roi->height - roi->height % 2; y += 2)
	{
		const BYTE* srcEven = &pSrc[y * srcStep];
		const BYTE* srcOdd = &pSrc[(y + 1) * srcStep];
		const size_t i = y >> 1;
		const size_t n = (i & (uint32_t)~7) + i;
		BYTE* b1Even = &pDst1[0][y * dst1Step[0]];
		BYTE* b1Odd = b1Even + dst1Step[0];
		BYTE* b2 = &pDst1[1][(y / 2) * dst1Step[1]];
		BYTE* b3 = &pDst1[2][(y / 2) * dst1Step[2]];
		BYTE* b4 = &pDst2[0][dst2Step[0] * n];
		BYTE* b5 = b4 + 8ULL * dst2Step[0];
		BYTE* b6 = &pDst2[1][(y / 2) * dst2Step[1]];
		BYTE* b7 = &pDst2[2][(y / 2) * dst2Step[2]];

		for (size_t x = 0; x < width; x += 16)
		{
			uint8x16_t ye = vdupq_n_u8(0);
			uint8x16_t ue = vdupq_n_u8(0);
			uint8x16_t ve = vdupq_n_u8(0);
			uint8x16_t yo = vdupq_n_u8(0);
			uint8x16_t uo = vdupq_n_u8(0);
			uint8x16_t vo = vdupq_n_u8(0);
			neon_AVC444_row16(&srcEven[4 * x], &ye, &ue, &ve);
			neon_AVC444_row16(&srcOdd[4 * x], &yo, &uo, &vo);

			/* main view: Y, and U and V averaged over each 2x2 block */
			vst1q_u8(&b1Even[x], ye);
			vst1q_u8(&b1Odd[x], yo);
			vst1_u8(&b2[x / 2], neon_AVC444_avg(ue, uo));
			vst1_u8(&b3[x / 2], neon_AVC444_avg(ve, vo));
			/* auxiliary view: U and V of the odd row, and of the odd columns of
			 * the even row */
			vst1q_u8(&b4[x], uo);
			vst1q_u8(&b5[x], vo);
			vst1_u8(&b6[x / 2], neon_AVC444_odd(ue));
			vst1_u8(&b7[x / 2], neon_AVC444_odd(ve));
		}

		general_RGBToAVC444YUV_BGRX_DOUBLE_ROW(
		    width, srcEven, srcOdd, &b1Even[width], &b1Odd[width], &b2[width / 2], &b3[width / 2],
		    &b4[width], &b5[width], &b6[width / 2], &b7[width / 2], roi->width);
	}

	for (; y < roi->height; y++)
	{
		const BYTE* srcEven = &pSrc[y * srcStep];
		BYTE* b1Even = &pDst1[0][y * dst1Step[0]];
		BYTE* b2 = &pDst1[1][(y / 2) * dst1Step[1]];
		BYTE* b3 = &pDst1[2][(y / 2) * dst1Step[2]];
		BYTE* b6 = &pDst2[1][(y / 2) * dst2Step[1]];
		BYTE* b7 = &pDst2[2][(y / 2) * dst2Step[2]];
		general_RGBToAVC444YUV_BGRX_DOUBLE_ROW(0, srcEven, nullptr, b1Even, nullptr, b2, b3,
		                                       nullptr, nullptr, b6, b7, roi->width);
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t neon_RGBToAVC444YUV(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcFormat,
                                     UINT32 srcStep, BYTE* WINPR_RESTRICT pDst1[3],
                                     const UINT32 dst1Step[3], BYTE* WINPR_RESTRICT pDst2[3],
                                     const UINT32 dst2Step[3],
                                     const prim_size_t* WINPR_RESTRICT roi)
{
	switch (srcFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return neon_RGBToAVC444YUV_BGRX(pSrc, srcStep, pDst1, dst1Step, pDst2, dst2Step, roi);
		default:
			return generic->RGBToAVC444YUV(pSrc, srcFormat, srcStep, pDst1, dst1Step, pDst2,
			                               dst2Step, roi);
	}
}

static pstatus_t neon_RGBToAVC444YUVv2_BGRX(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcStep,
                                            BYTE* WINPR_RESTRICT pDst1[3], const UINT32 dst1Step[3],
                                            BYTE* WINPR_RESTRICT pDst2[3], const UINT32 dst2Step[3],
                                            const prim_size_t* WINPR_RESTRICT roi)
{
	if (roi->height < 1 || roi->width < 1)
		return !PRIMITIVES_SUCCESS;

	const size_t width = roi->width - roi->width % 16;

	size_t y = 0;
	for (; y < roi->height - roi->height % 2; y += 2)
	{
		const BYTE* srcEven = &pSrc[y * srcStep];
		const BYTE* srcOdd = srcEven + srcStep;
		BYTE* yLumaEven = &pDst1[0][y * dst1Step[0]];
		BYTE* yLumaOdd = yLumaEven + dst1Step[0];
		BYTE* uLuma = &pDst1[1][(y / 2) * dst1Step[1]];
		BYTE* vLuma = &pDst1[2][(y / 2) * dst1Step[2]];
		BYTE* yEvenChroma1 = &pDst2[0][y * dst2Step[0]];
		BYTE* yEvenChroma2 = yEvenChroma1 + roi->width / 2;
		BYTE* yOddChroma1 = yEvenChroma1 + dst2Step[0];
		BYTE* yOddChroma2 = yEvenChroma2 + dst2Step[0];
		BYTE* uChroma1 = &pDst2[1][(y / 2) * dst2Step[1]];
		BYTE* vChroma1 = &pDst2[2][(y / 2) * dst2Step[2]];
		BYTE* uChroma2 = uChroma1 + roi->width / 4;
		BYTE* vChroma2 = vChroma1 + roi->width / 4;

		for (size_t x = 0; x < width; x += 16)
		{
			uint8x16_t ye = vdupq_n_u8(0);
			uint8x16_t ue = vdupq_n_u8(0);
			uint8x16_t ve = vdupq_n_u8(0);
			uint8x16_t yo = vdupq_n_u8(0);
			uint8x16_t uo = vdupq_n_u8(0);
			uint8x16_t vo = vdupq_n_u8(0);
			neon_AVC444_row16(&srcEven[4 * x], &ye, &ue, &ve);
			neon_AVC444_row16(&srcOdd[4 * x], &yo, &uo, &vo);

			/* main view: Y, and U and V averaged over each 2x2 block */
			vst1q_u8(&yLumaEven[x], ye);
			vst1q_u8(&yLumaOdd[x], yo);
			vst1_u8(&uLuma[x / 2], neon_AVC444_avg(ue, uo));
			vst1_u8(&vLuma[x / 2], neon_AVC444_avg(ve, vo));

			/* auxiliary view, Y plane: U and V of the odd columns, both rows */
			vst1_u8(&yEvenChroma1[x / 2], neon_AVC444_odd(ue));
			vst1_u8(&yEvenChroma2[x / 2], neon_AVC444_odd(ve));
			vst1_u8(&yOddChroma1[x / 2], neon_AVC444_odd(uo));
			vst1_u8(&yOddChroma2[x / 2], neon_AVC444_odd(vo));

			/* auxiliary view, U and V planes: U and V of the even columns of the
			 * odd row; columns 4k go to the U plane, columns 4k+2 to the V plane */
			const uint8x8_t uc = neon_AVC444_even(uo);
			const uint8x8_t vc = neon_AVC444_even(vo);
			const uint8x8x2_t us = vuzp_u8(uc, uc);
			const uint8x8x2_t vs = vuzp_u8(vc, vc);
			neon_AVC444_store4(&uChroma1[x / 4], us.val[0]);
			neon_AVC444_store4(&uChroma2[x / 4], vs.val[0]);
			neon_AVC444_store4(&vChroma1[x / 4], us.val[1]);
			neon_AVC444_store4(&vChroma2[x / 4], vs.val[1]);
		}

		general_RGBToAVC444YUVv2_BGRX_DOUBLE_ROW(
		    width, srcEven, srcOdd, &yLumaEven[width], &yLumaOdd[width], &uLuma[width / 2],
		    &vLuma[width / 2], &yEvenChroma1[width / 2], &yEvenChroma2[width / 2],
		    &yOddChroma1[width / 2], &yOddChroma2[width / 2], &uChroma1[width / 4],
		    &uChroma2[width / 4], &vChroma1[width / 4], &vChroma2[width / 4], roi->width);
	}

	for (; y < roi->height; y++)
	{
		const BYTE* srcEven = &pSrc[y * srcStep];
		BYTE* yLumaEven = &pDst1[0][y * dst1Step[0]];
		BYTE* uLuma = &pDst1[1][(y / 2) * dst1Step[1]];
		BYTE* vLuma = &pDst1[2][(y / 2) * dst1Step[2]];
		BYTE* yEvenChroma1 = &pDst2[0][y * dst2Step[0]];
		BYTE* yEvenChroma2 = yEvenChroma1 + roi->width / 2;
		general_RGBToAVC444YUVv2_BGRX_DOUBLE_ROW(
		    0, srcEven, nullptr, yLumaEven, nullptr, uLuma, vLuma, yEvenChroma1, yEvenChroma2,
		    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, roi->width);
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t neon_RGBToAVC444YUVv2(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcFormat,
                                       UINT32 srcStep, BYTE* WINPR_RESTRICT pDst1[3],
                                       const UINT32 dst1Step[3], BYTE* WINPR_RESTRICT pDst2[3],
                                       const UINT32 dst2Step[3],
                                       const prim_size_t* WINPR_RESTRICT roi)
{
	switch (srcFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return neon_RGBToAVC444YUVv2_BGRX(pSrc, srcStep, pDst1, dst1Step, pDst2, dst2Step, roi);
		default:
			return generic->RGBToAVC444YUVv2(pSrc, srcFormat, srcStep, pDst1, dst1Step, pDst2,
			                                 dst2Step, roi);
	}
}

static pstatus_t neon_LumaToYUV444(const BYTE* WINPR_RESTRICT pSrcRaw[3], const UINT32 srcStep[3],
                                   BYTE* WINPR_RESTRICT pDstRaw[3], const UINT32 dstStep[3],
                                   const RECTANGLE_16* WINPR_RESTRICT roi)
{
	const UINT32 nWidth = roi->right - roi->left;
	const UINT32 nHeight = roi->bottom - roi->top;
	const UINT32 halfWidth = (nWidth + 1) / 2;
	const UINT32 halfHeight = (nHeight + 1) / 2;
	const UINT32 evenY = 0;
	const BYTE* pSrc[3] = { pSrcRaw[0] + roi->top * srcStep[0] + roi->left,
		                    pSrcRaw[1] + roi->top / 2 * srcStep[1] + roi->left / 2,
		                    pSrcRaw[2] + roi->top / 2 * srcStep[2] + roi->left / 2 };
	BYTE* pDst[3] = { pDstRaw[0] + roi->top * dstStep[0] + roi->left,
		              pDstRaw[1] + roi->top * dstStep[1] + roi->left,
		              pDstRaw[2] + roi->top * dstStep[2] + roi->left };

	/* Y data is already here... */
	/* B1 */
	for (UINT32 y = 0; y < nHeight; y++)
	{
		const BYTE* Ym = pSrc[0] + srcStep[0] * y;
		BYTE* pY = pDst[0] + dstStep[0] * y;
		memcpy(pY, Ym, nWidth);
	}

	/* The first half of U, V are already here part of this frame. */
	/* B2 and B3 */
	for (UINT32 y = 0; y < halfHeight; y++)
	{
		const UINT32 val2y = (2 * y + evenY);
		const BYTE* Um = pSrc[1] + srcStep[1] * y;
		const BYTE* Vm = pSrc[2] + srcStep[2] * y;
		BYTE* pU = pDst[1] + dstStep[1] * val2y;
		BYTE* pV = pDst[2] + dstStep[2] * val2y;
		BYTE* pU1 = pU + dstStep[1];
		BYTE* pV1 = pV + dstStep[2];

		UINT32 x = 0;
		for (; x + 16 < halfWidth; x += 16)
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

static pstatus_t neon_ChromaV1ToYUV444(const BYTE* WINPR_RESTRICT pSrcRaw[3],
                                       const UINT32 srcStep[3], BYTE* WINPR_RESTRICT pDstRaw[3],
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
	const UINT32 padHeight = nHeight + 16 - nHeight % 16;
	const UINT32 halfPad = halfWidth % 16;
	const BYTE* pSrc[3] = { pSrcRaw[0] + roi->top * srcStep[0] + roi->left,
		                    pSrcRaw[1] + roi->top / 2 * srcStep[1] + roi->left / 2,
		                    pSrcRaw[2] + roi->top / 2 * srcStep[2] + roi->left / 2 };
	BYTE* pDst[3] = { pDstRaw[0] + roi->top * dstStep[0] + roi->left,
		              pDstRaw[1] + roi->top * dstStep[1] + roi->left,
		              pDstRaw[2] + roi->top * dstStep[2] + roi->left };

	/* The second half of U and V is a bit more tricky... */
	/* B4 and B5 */
	for (UINT32 y = 0; y < padHeight; y++)
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

		if (y < nHeight)
			memcpy(pX, Ya, nWidth);
	}

	/* B6 and B7 */
	for (UINT32 y = 0; y < halfHeight; y++)
	{
		const UINT32 val2y = (y * 2 + evenY);
		const BYTE* Ua = pSrc[1] + srcStep[1] * y;
		const BYTE* Va = pSrc[2] + srcStep[2] * y;
		BYTE* pU = pDst[1] + dstStep[1] * val2y;
		BYTE* pV = pDst[2] + dstStep[2] * val2y;

		UINT32 x = 0;
		for (; x < halfWidth - halfPad; x += 16)
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

	return PRIMITIVES_SUCCESS;
}

static pstatus_t neon_ChromaV2ToYUV444(const BYTE* WINPR_RESTRICT pSrc[3], const UINT32 srcStep[3],
                                       UINT32 nTotalWidth, UINT32 nTotalHeight,
                                       BYTE* WINPR_RESTRICT pDst[3], const UINT32 dstStep[3],
                                       const RECTANGLE_16* WINPR_RESTRICT roi)
{
	const UINT32 nWidth = roi->right - roi->left;
	const UINT32 nHeight = roi->bottom - roi->top;
	const UINT32 halfWidth = (nWidth + 1) / 2;
	const UINT32 halfPad = halfWidth % 16;
	const UINT32 halfHeight = (nHeight + 1) / 2;
	const UINT32 quaterWidth = (nWidth + 3) / 4;
	const UINT32 quaterPad = quaterWidth % 16;

	/* B4 and B5: odd UV values for width/2, height */
	for (UINT32 y = 0; y < nHeight; y++)
	{
		const UINT32 yTop = y + roi->top;
		const BYTE* pYaU = pSrc[0] + srcStep[0] * yTop + roi->left / 2;
		const BYTE* pYaV = pYaU + nTotalWidth / 2;
		BYTE* pU = pDst[1] + dstStep[1] * yTop + roi->left;
		BYTE* pV = pDst[2] + dstStep[2] * yTop + roi->left;

		UINT32 x = 0;
		for (; x < halfWidth - halfPad; x += 16)
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
	for (UINT32 y = 0; y < halfHeight; y++)
	{
		const BYTE* pUaU = pSrc[1] + srcStep[1] * (y + roi->top / 2) + roi->left / 4;
		const BYTE* pUaV = pUaU + nTotalWidth / 4;
		const BYTE* pVaU = pSrc[2] + srcStep[2] * (y + roi->top / 2) + roi->left / 4;
		const BYTE* pVaV = pVaU + nTotalWidth / 4;
		BYTE* pU = pDst[1] + dstStep[1] * (2 * y + 1 + roi->top) + roi->left;
		BYTE* pV = pDst[2] + dstStep[2] * (2 * y + 1 + roi->top) + roi->left;

		UINT32 x = 0;
		for (; x < quaterWidth - quaterPad; x += 16)
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

	return PRIMITIVES_SUCCESS;
}

static pstatus_t neon_YUV420CombineToYUV444(avc444_frame_type type,
                                            const BYTE* WINPR_RESTRICT pSrc[3],
                                            const UINT32 srcStep[3], UINT32 nWidth, UINT32 nHeight,
                                            BYTE* WINPR_RESTRICT pDst[3], const UINT32 dstStep[3],
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
			return neon_LumaToYUV444(pSrc, srcStep, pDst, dstStep, roi);

		case AVC444_CHROMAv1:
			return neon_ChromaV1ToYUV444(pSrc, srcStep, pDst, dstStep, roi);

		case AVC444_CHROMAv2:
			return neon_ChromaV2ToYUV444(pSrc, srcStep, nWidth, nHeight, pDst, dstStep, roi);

		default:
			return -1;
	}
}

/* RGBToI444_8u: one Y, U and V sample per pixel, with the exact integer math of
 * RGB2Y/RGB2U/RGB2V (16 bit lanes are wide enough for every intermediate value),
 * so the result matches the generic routine byte for byte. */

/* Y, U and V of eight pixels given as 16 bit R, G and B lanes */
static inline void neon_I444_YUV(uint16x8_t r, uint16x8_t g, uint16x8_t b,
                                 uint8x8_t* WINPR_RESTRICT y, uint8x8_t* WINPR_RESTRICT u,
                                 uint8x8_t* WINPR_RESTRICT v)
{
	const int16x8_t c128 = vdupq_n_s16(128);

	/* (54 R + 183 G + 18 B) >> 8: at most 65025, exact as unsigned 16 bit */
	uint16x8_t t = vmulq_n_u16(r, 54);
	t = vmlaq_n_u16(t, g, 183);
	t = vmlaq_n_u16(t, b, 18);
	*y = vshrn_n_u16(t, 8);

	const int16x8_t rs = vreinterpretq_s16_u16(r);
	const int16x8_t gs = vreinterpretq_s16_u16(g);
	const int16x8_t bs = vreinterpretq_s16_u16(b);

	/* ((128 B - (29 R + 99 G)) >> 8) + 128: both terms are at most 32640 */
	int16x8_t s = vmlaq_n_s16(vmulq_n_s16(rs, 29), gs, 99);
	s = vsubq_s16(vshlq_n_s16(bs, 7), s);
	*u = vqmovun_s16(vaddq_s16(vshrq_n_s16(s, 8), c128));

	/* ((128 R - (116 G + 12 B)) >> 8) + 128 */
	s = vmlaq_n_s16(vmulq_n_s16(gs, 116), bs, 12);
	s = vsubq_s16(vshlq_n_s16(rs, 7), s);
	*v = vqmovun_s16(vaddq_s16(vshrq_n_s16(s, 8), c128));
}

/* one of the four byte planes of vld4q_u8, picked with constant indices: indexing
 * px.val[] with a variable makes the compiler spill all four to the stack */
static inline uint8x16_t neon_I444_plane(const uint8x16x4_t px, const size_t off)
{
	switch (off)
	{
		case 0:
			return px.val[0];
		case 1:
			return px.val[1];
		case 2:
			return px.val[2];
		default:
			return px.val[3];
	}
}

static inline pstatus_t neon_RGBToI444_32bpp(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcStep,
                                             BYTE* WINPR_RESTRICT pDst[3], const UINT32 dstStep[3],
                                             const prim_size_t* WINPR_RESTRICT roi,
                                             const size_t rOff, const size_t gOff,
                                             const size_t bOff)
{
	const size_t width = roi->width;

	for (size_t row = 0; row < roi->height; row++)
	{
		const BYTE* src = &pSrc[row * srcStep];
		BYTE* yd = &pDst[0][row * dstStep[0]];
		BYTE* ud = &pDst[1][row * dstStep[1]];
		BYTE* vd = &pDst[2][row * dstStep[2]];

		size_t x = 0;
		for (; x + 16 <= width; x += 16)
		{
			/* sixteen pixels, split into one register per byte of the pixel */
			const uint8x16x4_t px = vld4q_u8(&src[4 * x]);
			const uint8x16_t R = neon_I444_plane(px, rOff);
			const uint8x16_t G = neon_I444_plane(px, gOff);
			const uint8x16_t B = neon_I444_plane(px, bOff);

			uint8x8_t y0 = vdup_n_u8(0);
			uint8x8_t u0 = vdup_n_u8(0);
			uint8x8_t v0 = vdup_n_u8(0);
			uint8x8_t y1 = vdup_n_u8(0);
			uint8x8_t u1 = vdup_n_u8(0);
			uint8x8_t v1 = vdup_n_u8(0);
			neon_I444_YUV(vmovl_u8(vget_low_u8(R)), vmovl_u8(vget_low_u8(G)),
			              vmovl_u8(vget_low_u8(B)), &y0, &u0, &v0);
			neon_I444_YUV(vmovl_u8(vget_high_u8(R)), vmovl_u8(vget_high_u8(G)),
			              vmovl_u8(vget_high_u8(B)), &y1, &u1, &v1);
			vst1q_u8(&yd[x], vcombine_u8(y0, y1));
			vst1q_u8(&ud[x], vcombine_u8(u0, u1));
			vst1q_u8(&vd[x], vcombine_u8(v0, v1));
		}

		for (; x < width; x++)
		{
			const BYTE* p = &src[4 * x];
			const BYTE r = p[rOff];
			const BYTE g = p[gOff];
			const BYTE b = p[bOff];
			yd[x] = RGB2Y(r, g, b);
			ud[x] = RGB2U(r, g, b);
			vd[x] = RGB2V(r, g, b);
		}
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t neon_RGBToI444_8u(const BYTE* WINPR_RESTRICT pSrc, UINT32 SrcFormat,
                                   const UINT32 srcStep, BYTE* WINPR_RESTRICT pDst[3],
                                   const UINT32 dstStep[3], const prim_size_t* WINPR_RESTRICT roi)
{
	/* byte offsets of R, G and B in memory, as in general_RGBToI444_8u */
	switch (SrcFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return neon_RGBToI444_32bpp(pSrc, srcStep, pDst, dstStep, roi, 2, 1, 0);
		case PIXEL_FORMAT_RGBA32:
		case PIXEL_FORMAT_RGBX32:
			return neon_RGBToI444_32bpp(pSrc, srcStep, pDst, dstStep, roi, 0, 1, 2);
		case PIXEL_FORMAT_ARGB32:
		case PIXEL_FORMAT_XRGB32:
			return neon_RGBToI444_32bpp(pSrc, srcStep, pDst, dstStep, roi, 1, 2, 3);
		case PIXEL_FORMAT_ABGR32:
		case PIXEL_FORMAT_XBGR32:
			return neon_RGBToI444_32bpp(pSrc, srcStep, pDst, dstStep, roi, 3, 2, 1);
		default:
			return generic->RGBToI444_8u(pSrc, SrcFormat, srcStep, pDst, dstStep, roi);
	}
}
#endif

void primitives_init_YUV_neon_int(primitives_t* WINPR_RESTRICT prims)
{
#if defined(NEON_INTRINSICS_ENABLED)
	generic = primitives_get_generic();
	WLog_VRB(PRIM_TAG, "NEON optimizations");
	prims->RGBToYUV420_8u_P3AC4R = neon_RGBToYUV420_8u_P3AC4R;
	prims->RGBToAVC444YUV = neon_RGBToAVC444YUV;
	prims->RGBToAVC444YUVv2 = neon_RGBToAVC444YUVv2;
	prims->YUV420ToRGB_8u_P3AC4R = neon_YUV420ToRGB_8u_P3AC4R;
	prims->YUV444ToRGB_8u_P3AC4R = neon_YUV444ToRGB_8u_P3AC4R;
	prims->I444ToRGB_8u = neon_I444ToRGB_8u;
	prims->RGBToI444_8u = neon_RGBToI444_8u;
	prims->YUV420CombineToYUV444 = neon_YUV420CombineToYUV444;
#else
	WLog_VRB(PRIM_TAG, "undefined WITH_SIMD or neon intrinsics not available");
	WINPR_UNUSED(prims);
#endif
}
