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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(__ARM_NEON__) || defined(__aarch64__)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arm_neon.h>
#include <winpr/sysinfo.h>

#include "rfx_types.h"
#include "rfx_neon.h"

/* rfx_decode_YCbCr_to_RGB_NEON code now resides in the primitives library. */

static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
rfx_quantization_decode_block_NEON(INT16 * buffer, const int buffer_size, const UINT32 factor)
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
	}
	while(buf < buf_end);
}

void rfx_quantization_decode_NEON(INT16 * buffer, const UINT32 * quantVals)
{
	rfx_quantization_decode_block_NEON(&buffer[0], 1024, quantVals[8] - 1); /* HL1 */
	rfx_quantization_decode_block_NEON(&buffer[1024], 1024, quantVals[7] - 1); /* LH1 */
	rfx_quantization_decode_block_NEON(&buffer[2048], 1024, quantVals[9] - 1); /* HH1 */
	rfx_quantization_decode_block_NEON(&buffer[3072], 256, quantVals[5] - 1); /* HL2 */
	rfx_quantization_decode_block_NEON(&buffer[3328], 256, quantVals[4] - 1); /* LH2 */
	rfx_quantization_decode_block_NEON(&buffer[3584], 256, quantVals[6] - 1); /* HH2 */
	rfx_quantization_decode_block_NEON(&buffer[3840], 64, quantVals[2] - 1); /* HL3 */
	rfx_quantization_decode_block_NEON(&buffer[3904], 64, quantVals[1] - 1); /* LH3 */
	rfx_quantization_decode_block_NEON(&buffer[3968], 64, quantVals[3] - 1); /* HH3 */
	rfx_quantization_decode_block_NEON(&buffer[4032], 64, quantVals[0] - 1); /* LL3 */
}



static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
rfx_dwt_2d_decode_block_horiz_NEON(INT16 * l, INT16 * h, INT16 * dst, int subband_width)
{
	int y, n;
	INT16 * l_ptr = l;
	INT16 * h_ptr = h;
	INT16 * dst_ptr = dst;

	for (y = 0; y < subband_width; y++)
	{
		/* Even coefficients */
		for (n = 0; n < subband_width; n+=8)
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

			l_ptr+=8;
			h_ptr+=8;
		}
		l_ptr -= subband_width;
		h_ptr -= subband_width;

		/* Odd coefficients */
		for (n = 0; n < subband_width; n+=8)
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

			l_ptr+=8;
			h_ptr+=8;
			dst_ptr+=16;
		}
	}
}

static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
rfx_dwt_2d_decode_block_vert_NEON(INT16 * l, INT16 * h, INT16 * dst, int subband_width)
{
	int x, n;
	INT16 * l_ptr = l;
	INT16 * h_ptr = h;
	INT16 * dst_ptr = dst;

	int total_width = subband_width + subband_width;

	/* Even coefficients */
	for (n = 0; n < subband_width; n++)
	{
		for (x = 0; x < total_width; x+=8)
		{
			// dst[2n] = l[n] - ((h[n-1] + h[n] + 1) >> 1);

			int16x8_t l_n = vld1q_s16(l_ptr);
			int16x8_t h_n = vld1q_s16(h_ptr);

			int16x8_t tmp_n = vaddq_s16(h_n, vdupq_n_s16(1));;
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

			l_ptr+=8;
			h_ptr+=8;
			dst_ptr+=8;
		}
		dst_ptr+=total_width;
	}

	h_ptr = h;
	dst_ptr = dst + total_width;

	/* Odd coefficients */
	for (n = 0; n < subband_width; n++)
	{
		for (x = 0; x < total_width; x+=8)
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

		h_ptr+=8;
		dst_ptr+=8;
	}
	dst_ptr+=total_width;
}
}

static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
rfx_dwt_2d_decode_block_NEON(INT16 * buffer, INT16 * idwt, int subband_width)
{
	INT16 * hl, * lh, * hh, * ll;
	INT16 * l_dst, * h_dst;

	/* Inverse DWT in horizontal direction, results in 2 sub-bands in L, H order in tmp buffer idwt. */
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

void rfx_dwt_2d_decode_NEON(INT16 * buffer, INT16 * dwt_buffer)
{
	rfx_dwt_2d_decode_block_NEON(buffer + 3840, dwt_buffer, 8);
	rfx_dwt_2d_decode_block_NEON(buffer + 3072, dwt_buffer, 16);
	rfx_dwt_2d_decode_block_NEON(buffer, dwt_buffer, 32);
}

void rfx_init_neon(RFX_CONTEXT * context)
{
	if (IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE))
	{
		DEBUG_RFX("Using NEON optimizations");

		IF_PROFILER(context->priv->prof_rfx_ycbcr_to_rgb->name = "rfx_decode_YCbCr_to_RGB_NEON");
		IF_PROFILER(context->priv->prof_rfx_quantization_decode->name = "rfx_quantization_decode_NEON");
		IF_PROFILER(context->priv->prof_rfx_dwt_2d_decode->name = "rfx_dwt_2d_decode_NEON");

		context->quantization_decode = rfx_quantization_decode_NEON;
		context->dwt_2d_decode = rfx_dwt_2d_decode_NEON;
	}
}

#endif // __ARM_NEON__

