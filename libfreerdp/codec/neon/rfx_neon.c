/*
   FreeRDP: A Remote Desktop Protocol Implementation
   RemoteFX Codec Library - NEON Optimizations

   Copyright 2011 Martin Fleisz <martin.fleisz@thincast.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <winpr/platform.h>
#include <freerdp/config.h>
#include <freerdp/log.h>

#include "../rfx_types.h"
#include "rfx_neon.h"

#include "../../core/simd.h"

#if defined(NEON_INTRINSICS_ENABLED)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arm_neon.h>
#include <winpr/sysinfo.h>

/* rfx_decode_YCbCr_to_RGB_NEON code now resides in the primitives library. */

static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
rfx_quantization_decode_block_NEON(INT16* buffer, const size_t buffer_size, const UINT32 factor)
{
	int16x8_t quantFactors = vdupq_n_s16(factor);
	int16x8_t* buf = (int16x8_t*)buffer;
	int16x8_t* buf_end = (int16x8_t*)(buffer + buffer_size);

	do
	{
		int16x8_t val = vld1q_s16((INT16*)buf);
		val = vshlq_s16(val, quantFactors);
		vst1q_s16((INT16*)buf, val);
		buf++;
	} while (buf < buf_end);
}

static void rfx_quantization_decode_NEON(INT16* buffer, const UINT32* WINPR_RESTRICT quantVals)
{
	WINPR_ASSERT(buffer);
	WINPR_ASSERT(quantVals);

	rfx_quantization_decode_block_NEON(&buffer[0], 1024, quantVals[8] - 1);    /* HL1 */
	rfx_quantization_decode_block_NEON(&buffer[1024], 1024, quantVals[7] - 1); /* LH1 */
	rfx_quantization_decode_block_NEON(&buffer[2048], 1024, quantVals[9] - 1); /* HH1 */
	rfx_quantization_decode_block_NEON(&buffer[3072], 256, quantVals[5] - 1);  /* HL2 */
	rfx_quantization_decode_block_NEON(&buffer[3328], 256, quantVals[4] - 1);  /* LH2 */
	rfx_quantization_decode_block_NEON(&buffer[3584], 256, quantVals[6] - 1);  /* HH2 */
	rfx_quantization_decode_block_NEON(&buffer[3840], 64, quantVals[2] - 1);   /* HL3 */
	rfx_quantization_decode_block_NEON(&buffer[3904], 64, quantVals[1] - 1);   /* LH3 */
	rfx_quantization_decode_block_NEON(&buffer[3968], 64, quantVals[3] - 1);   /* HH3 */
	rfx_quantization_decode_block_NEON(&buffer[4032], 64, quantVals[0] - 1);   /* LL3 */
}

static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
rfx_dwt_2d_decode_block_horiz_NEON(INT16* WINPR_RESTRICT l, INT16* WINPR_RESTRICT h,
                                   INT16* WINPR_RESTRICT dst, size_t subband_width)
{
	INT16* l_ptr = l;
	INT16* h_ptr = h;
	INT16* dst_ptr = dst;

	for (size_t y = 0; y < subband_width; y++)
	{
		/* Even coefficients */
		for (size_t n = 0; n < subband_width; n += 8)
		{
			// dst[2n] = l[n] - ((h[n-1] + h[n] + 1) >> 1);
			int16x8_t l_n = vld1q_s16(l_ptr);
			int16x8_t h_n = vld1q_s16(h_ptr);
			int16x8_t h_n_m = vld1q_s16(h_ptr - 1);

			if (n == 0)
			{
				int16_t first = vgetq_lane_s16(h_n_m, 1);
				h_n_m = vsetq_lane_s16(first, h_n_m, 0);
			}

			int16x8_t tmp_n = vaddq_s16(h_n, h_n_m);
			tmp_n = vaddq_s16(tmp_n, vdupq_n_s16(1));
			tmp_n = vshrq_n_s16(tmp_n, 1);
			int16x8_t dst_n = vsubq_s16(l_n, tmp_n);
			vst1q_s16(l_ptr, dst_n);
			l_ptr += 8;
			h_ptr += 8;
		}

		l_ptr -= subband_width;
		h_ptr -= subband_width;

		/* Odd coefficients */
		for (size_t n = 0; n < subband_width; n += 8)
		{
			// dst[2n + 1] = (h[n] << 1) + ((dst[2n] + dst[2n + 2]) >> 1);
			int16x8_t h_n = vld1q_s16(h_ptr);
			h_n = vshlq_n_s16(h_n, 1);
			int16x8x2_t dst_n;
			dst_n.val[0] = vld1q_s16(l_ptr);
			int16x8_t dst_n_p = vld1q_s16(l_ptr + 1);

			if (n == subband_width - 8)
			{
				int16_t last = vgetq_lane_s16(dst_n_p, 6);
				dst_n_p = vsetq_lane_s16(last, dst_n_p, 7);
			}

			dst_n.val[1] = vaddq_s16(dst_n_p, dst_n.val[0]);
			dst_n.val[1] = vshrq_n_s16(dst_n.val[1], 1);
			dst_n.val[1] = vaddq_s16(dst_n.val[1], h_n);
			vst2q_s16(dst_ptr, dst_n);
			l_ptr += 8;
			h_ptr += 8;
			dst_ptr += 16;
		}
	}
}

static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
rfx_dwt_2d_decode_block_vert_NEON(INT16* WINPR_RESTRICT l, INT16* WINPR_RESTRICT h,
                                  INT16* WINPR_RESTRICT dst, size_t subband_width)
{
	INT16* l_ptr = l;
	INT16* h_ptr = h;
	INT16* dst_ptr = dst;
	const size_t total_width = subband_width + subband_width;

	/* Even coefficients */
	for (size_t n = 0; n < subband_width; n++)
	{
		for (size_t x = 0; x < total_width; x += 8)
		{
			// dst[2n] = l[n] - ((h[n-1] + h[n] + 1) >> 1);
			int16x8_t l_n = vld1q_s16(l_ptr);
			int16x8_t h_n = vld1q_s16(h_ptr);
			int16x8_t tmp_n = vaddq_s16(h_n, vdupq_n_s16(1));

			if (n == 0)
				tmp_n = vaddq_s16(tmp_n, h_n);
			else
			{
				int16x8_t h_n_m = vld1q_s16((h_ptr - total_width));
				tmp_n = vaddq_s16(tmp_n, h_n_m);
			}

			tmp_n = vshrq_n_s16(tmp_n, 1);
			int16x8_t dst_n = vsubq_s16(l_n, tmp_n);
			vst1q_s16(dst_ptr, dst_n);
			l_ptr += 8;
			h_ptr += 8;
			dst_ptr += 8;
		}

		dst_ptr += total_width;
	}

	h_ptr = h;
	dst_ptr = dst + total_width;

	/* Odd coefficients */
	for (size_t n = 0; n < subband_width; n++)
	{
		for (size_t x = 0; x < total_width; x += 8)
		{
			// dst[2n + 1] = (h[n] << 1) + ((dst[2n] + dst[2n + 2]) >> 1);
			int16x8_t h_n = vld1q_s16(h_ptr);
			int16x8_t dst_n_m = vld1q_s16(dst_ptr - total_width);
			h_n = vshlq_n_s16(h_n, 1);
			int16x8_t tmp_n = dst_n_m;

			if (n == subband_width - 1)
				tmp_n = vaddq_s16(tmp_n, dst_n_m);
			else
			{
				int16x8_t dst_n_p = vld1q_s16((dst_ptr + total_width));
				tmp_n = vaddq_s16(tmp_n, dst_n_p);
			}

			tmp_n = vshrq_n_s16(tmp_n, 1);
			int16x8_t dst_n = vaddq_s16(tmp_n, h_n);
			vst1q_s16(dst_ptr, dst_n);
			h_ptr += 8;
			dst_ptr += 8;
		}

		dst_ptr += total_width;
	}
}

static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
rfx_dwt_2d_decode_block_NEON(INT16* WINPR_RESTRICT buffer, INT16* WINPR_RESTRICT idwt,
                             size_t subband_width)
{
	INT16 *hl, *lh, *hh, *ll;
	INT16 *l_dst, *h_dst;
	/* Inverse DWT in horizontal direction, results in 2 sub-bands in L, H order in tmp buffer idwt.
	 */
	/* The 4 sub-bands are stored in HL(0), LH(1), HH(2), LL(3) order. */
	/* The lower part L uses LL(3) and HL(0). */
	/* The higher part H uses LH(1) and HH(2). */
	ll = buffer + subband_width * subband_width * 3;
	hl = buffer;
	l_dst = idwt;
	rfx_dwt_2d_decode_block_horiz_NEON(ll, hl, l_dst, subband_width);
	lh = buffer + subband_width * subband_width;
	hh = buffer + subband_width * subband_width * 2;
	h_dst = idwt + subband_width * subband_width * 2;
	rfx_dwt_2d_decode_block_horiz_NEON(lh, hh, h_dst, subband_width);
	/* Inverse DWT in vertical direction, results are stored in original buffer. */
	rfx_dwt_2d_decode_block_vert_NEON(l_dst, h_dst, buffer, subband_width);
}

static void rfx_dwt_2d_decode_NEON(INT16* buffer, INT16* dwt_buffer)
{
	rfx_dwt_2d_decode_block_NEON(buffer + 3840, dwt_buffer, 8);
	rfx_dwt_2d_decode_block_NEON(buffer + 3072, dwt_buffer, 16);
	rfx_dwt_2d_decode_block_NEON(buffer, dwt_buffer, 32);
}

static INLINE void rfx_idwt_extrapolate_horiz_neon(INT16* restrict pLowBand, size_t nLowStep,
                                                   const INT16* restrict pHighBand,
                                                   size_t nHighStep, INT16* restrict pDstBand,
                                                   size_t nDstStep, size_t nLowCount,
                                                   size_t nHighCount, size_t nDstCount)
{
	WINPR_ASSERT(pLowBand);
	WINPR_ASSERT(pHighBand);
	WINPR_ASSERT(pDstBand);

	INT16* l_ptr = pLowBand;
	const INT16* h_ptr = pHighBand;
	INT16* dst_ptr = pDstBand;
	size_t batchSize = (nLowCount + nHighCount) >> 1;

	for (size_t y = 0; y < nDstCount; y++)
	{
		/* Even coefficients */
		size_t n = 0;
		for (; n < batchSize; n += 8)
		{
			// dst[2n] = l[n] - ((h[n-1] + h[n] + 1) >> 1);
			int16x8_t l_n = vld1q_s16(l_ptr);
			int16x8_t h_n = vld1q_s16(h_ptr);
			int16x8_t h_n_m = vld1q_s16(h_ptr - 1);

			if (n == 0)
			{
				int16_t first = vgetq_lane_s16(h_n_m, 1);
				h_n_m = vsetq_lane_s16(first, h_n_m, 0);
			}
			else if (n == 24)
				h_n = vsetq_lane_s16(0, h_n, 7);

			int16x8_t tmp_n = vaddq_s16(h_n, h_n_m);
			tmp_n = vaddq_s16(tmp_n, vdupq_n_s16(1));
			tmp_n = vshrq_n_s16(tmp_n, 1);
			int16x8_t dst_n = vsubq_s16(l_n, tmp_n);
			vst1q_s16(l_ptr, dst_n);
			l_ptr += 8;
			h_ptr += 8;
		}
		if (n < 32)
			*l_ptr -= *(h_ptr - 1);

		l_ptr -= batchSize;
		h_ptr -= batchSize;

		/* Odd coefficients */
		n = 0;
		for (; n < batchSize; n += 8)
		{
			// dst[2n + 1] = (h[n] << 1) + ((dst[2n] + dst[2n + 2]) >> 1);
			int16x8_t h_n = vld1q_s16(h_ptr);
			h_n = vshlq_n_s16(h_n, 1);
			int16x8x2_t dst_n;
			dst_n.val[0] = vld1q_s16(l_ptr);
			int16x8_t dst_n_p = vld1q_s16(l_ptr + 1);

			if (n == 24)
				h_n = vsetq_lane_s16(0, h_n, 7);

			dst_n.val[1] = vaddq_s16(dst_n_p, dst_n.val[0]);
			dst_n.val[1] = vshrq_n_s16(dst_n.val[1], 1);
			dst_n.val[1] = vaddq_s16(dst_n.val[1], h_n);
			vst2q_s16(dst_ptr, dst_n);
			l_ptr += 8;
			h_ptr += 8;
			dst_ptr += 16;
		}
		if (n == 32)
		{
			h_ptr -= 1;
			l_ptr += 1;
		}
		else
		{
			*dst_ptr = *l_ptr;
			l_ptr += 1;
			dst_ptr += 1;
		}
	}
}

static INLINE void rfx_idwt_extrapolate_vert_neon(const INT16* restrict pLowBand, size_t nLowStep,
                                                  const INT16* restrict pHighBand, size_t nHighStep,
                                                  INT16* restrict pDstBand, size_t nDstStep,
                                                  size_t nLowCount, size_t nHighCount,
                                                  size_t nDstCount)
{
	WINPR_ASSERT(pLowBand);
	WINPR_ASSERT(pHighBand);
	WINPR_ASSERT(pDstBand);

	const INT16* l_ptr = pLowBand;
	const INT16* h_ptr = pHighBand;
	INT16* dst_ptr = pDstBand;
	size_t batchSize = (nDstCount >> 3) << 3;
	size_t forceBandSize = (nLowCount + nHighCount) >> 1;

	/* Even coefficients */
	for (size_t n = 0; n < forceBandSize; n++)
	{
		for (size_t x = 0; x < batchSize; x += 8)
		{
			// dst[2n] = l[n] - ((h[n-1] + h[n] + 1) >> 1);
			int16x8_t l_n = vld1q_s16(l_ptr);
			int16x8_t h_n = vld1q_s16((n == 31) ? (h_ptr - nHighStep) : h_ptr);
			int16x8_t tmp_n = vaddq_s16(h_n, vdupq_n_s16(1));

			if (n == 0)
				tmp_n = vaddq_s16(tmp_n, h_n);
			else if (n < 31)
			{
				int16x8_t h_n_m = vld1q_s16((h_ptr - nHighStep));
				tmp_n = vaddq_s16(tmp_n, h_n_m);
			}

			tmp_n = vshrq_n_s16(tmp_n, 1);
			int16x8_t dst_n = vsubq_s16(l_n, tmp_n);
			vst1q_s16(dst_ptr, dst_n);
			l_ptr += 8;
			h_ptr += 8;
			dst_ptr += 8;
		}

		if (nDstCount > batchSize)
		{
			int16_t h_n = (n == 31) ? *(h_ptr - nHighStep) : *h_ptr;
			int16_t tmp_n = h_n + 1;
			if (n == 0)
				tmp_n += h_n;
			else if (n < 31)
				tmp_n += *(h_ptr - nHighStep);
			tmp_n >>= 1;
			*dst_ptr = *l_ptr - tmp_n;
			l_ptr += 1;
			h_ptr += 1;
			dst_ptr += 1;
		}

		dst_ptr += nDstStep;
	}

	if (forceBandSize < 32)
	{
		for (size_t x = 0; x < batchSize; x += 8)
		{
			int16x8_t l_n = vld1q_s16(l_ptr);
			int16x8_t h_n = vld1q_s16(h_ptr - nHighStep);
			int16x8_t tmp_n = vsubq_s16(l_n, h_n);
			vst1q_s16(dst_ptr, tmp_n);
			l_ptr += 8;
			h_ptr += 8;
			dst_ptr += 8;
		}

		if (nDstCount > batchSize)
		{
			*dst_ptr = *l_ptr - *(h_ptr - nHighStep);
			l_ptr += 1;
			h_ptr += 1;
			dst_ptr += 1;
		}
	}

	h_ptr = pHighBand;
	dst_ptr = pDstBand + nDstStep;

	/* Odd coefficients */
	for (size_t n = 0; n < forceBandSize; n++)
	{
		for (size_t x = 0; x < batchSize; x += 8)
		{
			// dst[2n + 1] = (h[n] << 1) + ((dst[2n] + dst[2n + 2]) >> 1);
			int16x8_t tmp_n = vld1q_s16(dst_ptr - nDstStep);
			if (n == 31)
			{
				int16x8_t dst_n_p = vld1q_s16(l_ptr);
				l_ptr += 8;
				tmp_n = vaddq_s16(tmp_n, dst_n_p);
				tmp_n = vshrq_n_s16(tmp_n, 1);
			}
			else
			{
				int16x8_t dst_n_p = vld1q_s16(dst_ptr + nDstStep);
				tmp_n = vaddq_s16(tmp_n, dst_n_p);
				tmp_n = vshrq_n_s16(tmp_n, 1);
				int16x8_t h_n = vld1q_s16(h_ptr);
				h_n = vshlq_n_s16(h_n, 1);
				tmp_n = vaddq_s16(tmp_n, h_n);
			}
			vst1q_s16(dst_ptr, tmp_n);
			h_ptr += 8;
			dst_ptr += 8;
		}

		if (nDstCount > batchSize)
		{
			int16_t tmp_n = *(dst_ptr - nDstStep);
			if (n == 31)
			{
				int16_t dst_n_p = *l_ptr;
				l_ptr += 1;
				tmp_n += dst_n_p;
				tmp_n >>= 1;
			}
			else
			{
				int16_t dst_n_p = *(dst_ptr + nDstStep);
				tmp_n += dst_n_p;
				tmp_n >>= 1;
				int16_t h_n = *h_ptr;
				h_n <<= 1;
				tmp_n += h_n;
			}
			*dst_ptr = tmp_n;
			h_ptr += 1;
			dst_ptr += 1;
		}

		dst_ptr += nDstStep;
	}
}

static INLINE size_t prfx_get_band_l_count(size_t level)
{
	return (64 >> level) + 1;
}

static INLINE size_t prfx_get_band_h_count(size_t level)
{
	if (level == 1)
		return (64 >> 1) - 1;
	else
		return (64 + (1 << (level - 1))) >> level;
}

static INLINE void rfx_dwt_2d_decode_extrapolate_block_neon(INT16* buffer, INT16* temp,
                                                            size_t level)
{
	size_t nDstStepX;
	size_t nDstStepY;
	INT16 *HL, *LH;
	INT16 *HH, *LL;
	INT16 *L, *H, *LLx;

	const size_t nBandL = prfx_get_band_l_count(level);
	const size_t nBandH = prfx_get_band_h_count(level);
	size_t offset = 0;

	WINPR_ASSERT(buffer);
	WINPR_ASSERT(temp);

	HL = &buffer[offset];
	offset += (nBandH * nBandL);
	LH = &buffer[offset];
	offset += (nBandL * nBandH);
	HH = &buffer[offset];
	offset += (nBandH * nBandH);
	LL = &buffer[offset];
	nDstStepX = (nBandL + nBandH);
	nDstStepY = (nBandL + nBandH);
	offset = 0;
	L = &temp[offset];
	offset += (nBandL * nDstStepX);
	H = &temp[offset];
	LLx = &buffer[0];

	/* horizontal (LL + HL -> L) */
	rfx_idwt_extrapolate_horiz_neon(LL, nBandL, HL, nBandH, L, nDstStepX, nBandL, nBandH, nBandL);

	/* horizontal (LH + HH -> H) */
	rfx_idwt_extrapolate_horiz_neon(LH, nBandL, HH, nBandH, H, nDstStepX, nBandL, nBandH, nBandH);

	/* vertical (L + H -> LL) */
	rfx_idwt_extrapolate_vert_neon(L, nDstStepX, H, nDstStepX, LLx, nDstStepY, nBandL, nBandH,
	                               nBandL + nBandH);
}

static void rfx_dwt_2d_extrapolate_decode_neon(INT16* buffer, INT16* temp)
{
	WINPR_ASSERT(buffer);
	WINPR_ASSERT(temp);
	rfx_dwt_2d_decode_extrapolate_block_neon(&buffer[3807], temp, 3);
	rfx_dwt_2d_decode_extrapolate_block_neon(&buffer[3007], temp, 2);
	rfx_dwt_2d_decode_extrapolate_block_neon(&buffer[0], temp, 1);
}
#endif // NEON_INTRINSICS_ENABLED

void rfx_init_neon(RFX_CONTEXT* context)
{
#if defined(NEON_INTRINSICS_ENABLED)
	if (IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE))
	{
		DEBUG_RFX("Using NEON optimizations");
		PROFILER_RENAME(context->priv->prof_rfx_ycbcr_to_rgb, "rfx_decode_YCbCr_to_RGB_NEON");
		PROFILER_RENAME(context->priv->prof_rfx_quantization_decode,
		                "rfx_quantization_decode_NEON");
		PROFILER_RENAME(context->priv->prof_rfx_dwt_2d_decode, "rfx_dwt_2d_decode_NEON");
		context->quantization_decode = rfx_quantization_decode_NEON;
		context->dwt_2d_decode = rfx_dwt_2d_decode_NEON;
		context->dwt_2d_extrapolate_decode = rfx_dwt_2d_extrapolate_decode_neon;
	}
#else
	WINPR_UNUSED(context);
#endif
}
