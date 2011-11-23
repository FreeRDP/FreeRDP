/*
   FreeRDP: A Remote Desktop Protocol client.
   RemoteFX Codec Library - NEON Optimizations

   Copyright 2011 Martin Fleisz <mfleisz@thinstuff.com>

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

#if defined(__ARM_NEON__)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arm_neon.h>

#include "rfx_types.h"
#include "rfx_neon.h"

#if defined(ANDROID)
#include <cpu-features.h>
#endif


void rfx_decode_YCbCr_to_RGB_NEON(sint16 * y_r_buffer, sint16 * cb_g_buffer, sint16 * cr_b_buffer)
{
	int16x8_t zero = vdupq_n_s16(0);
	int16x8_t max = vdupq_n_s16(255);
	int16x8_t y_add = vdupq_n_s16(128);

	int16x8_t* y_r_buf = (int16x8_t*)y_r_buffer;
	int16x8_t* cb_g_buf = (int16x8_t*)cb_g_buffer;
	int16x8_t* cr_b_buf = (int16x8_t*)cr_b_buffer;

	int i;
	for (i = 0; i < 4096 / 8; i++)
	{
		int16x8_t y = vld1q_s16((sint16*)&y_r_buf[i]);
		y = vaddq_s16(y, y_add);

		int16x8_t cr = vld1q_s16((sint16*)&cr_b_buf[i]);

		// r = between((y + cr + (cr >> 2) + (cr >> 3) + (cr >> 5)), 0, 255);
		int16x8_t r = vaddq_s16(y, cr);
		r = vaddq_s16(r, vshrq_n_s16(cr, 2));
		r = vaddq_s16(r, vshrq_n_s16(cr, 3));
		r = vaddq_s16(r, vshrq_n_s16(cr, 5));
		r = vminq_s16(vmaxq_s16(r, zero), max);
		vst1q_s16((sint16*)&y_r_buf[i], r);

		// cb = cb_g_buf[i];
		int16x8_t cb = vld1q_s16((sint16*)&cb_g_buf[i]);

		// g = between(y - (cb >> 2) - (cb >> 4) - (cb >> 5) - (cr >> 1) - (cr >> 3) - (cr >> 4) - (cr >> 5), 0, 255);
		int16x8_t g = vsubq_s16(y, vshrq_n_s16(cb, 2));
		g = vsubq_s16(g, vshrq_n_s16(cb, 4));
		g = vsubq_s16(g, vshrq_n_s16(cb, 5));
		g = vsubq_s16(g, vshrq_n_s16(cr, 1));
		g = vsubq_s16(g, vshrq_n_s16(cr, 3));
		g = vsubq_s16(g, vshrq_n_s16(cr, 4));
		g = vsubq_s16(g, vshrq_n_s16(cr, 5));
		g = vminq_s16(vmaxq_s16(g, zero), max);
		vst1q_s16((sint16*)&cb_g_buf[i], g);

		// b = between((y + cb + (cb >> 1) + (cb >> 2) + (cb >> 6)), 0, 255);
		int16x8_t b = vaddq_s16(y, cb);
		b = vaddq_s16(b, vshrq_n_s16(cb, 1));
		b = vaddq_s16(b, vshrq_n_s16(cb, 2));
		b = vaddq_s16(b, vshrq_n_s16(cb, 6));
		b = vminq_s16(vmaxq_s16(b, zero), max);
		vst1q_s16((sint16*)&cr_b_buf[i], b);
	}

}

static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
rfx_quantization_decode_block_NEON(sint16 * buffer, const int buffer_size, const uint32 factor)
{
	if (factor <= 6)
		return;
	int16x8_t quantFactors = vdupq_n_s16(factor - 6);
	int16x8_t* buf = (int16x8_t*)buffer;
	int16x8_t* buf_end = (int16x8_t*)(buffer + buffer_size);

	do
	{
		int16x8_t val = vld1q_s16((sint16*)buf);
		val = vshlq_s16(val, quantFactors);
		vst1q_s16((sint16*)buf, val);
		buf++;
	}
	while(buf < buf_end);
}

void
rfx_quantization_decode_NEON(sint16 * buffer, const uint32 * quantization_values)
{
	rfx_quantization_decode_block_NEON(buffer, 1024, quantization_values[8]); /* HL1 */
	rfx_quantization_decode_block_NEON(buffer + 1024, 1024, quantization_values[7]); /* LH1 */
	rfx_quantization_decode_block_NEON(buffer + 2048, 1024, quantization_values[9]); /* HH1 */
	rfx_quantization_decode_block_NEON(buffer + 3072, 256, quantization_values[5]); /* HL2 */
	rfx_quantization_decode_block_NEON(buffer + 3328, 256, quantization_values[4]); /* LH2 */
	rfx_quantization_decode_block_NEON(buffer + 3584, 256, quantization_values[6]); /* HH2 */
	rfx_quantization_decode_block_NEON(buffer + 3840, 64, quantization_values[2]); /* HL3 */
	rfx_quantization_decode_block_NEON(buffer + 3904, 64, quantization_values[1]); /* LH3 */
	rfx_quantization_decode_block_NEON(buffer + 3968, 64, quantization_values[3]); /* HH3 */
	rfx_quantization_decode_block_NEON(buffer + 4032, 64, quantization_values[0]); /* LL3 */
}



static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
rfx_dwt_2d_decode_block_horiz_NEON(sint16 * l, sint16 * h, sint16 * dst, int subband_width)
{
	int y, n;
	sint16 * l_ptr = l;
	sint16 * h_ptr = h;
	sint16 * dst_ptr = dst;

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
rfx_dwt_2d_decode_block_vert_NEON(sint16 * l, sint16 * h, sint16 * dst, int subband_width)
{
	int x, n;
	sint16 * l_ptr = l;
	sint16 * h_ptr = h;
	sint16 * dst_ptr = dst;

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
rfx_dwt_2d_decode_block_NEON(sint16 * buffer, sint16 * idwt, int subband_width)
{
	sint16 * hl, * lh, * hh, * ll;
	sint16 * l_dst, * h_dst;

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

void
rfx_dwt_2d_decode_NEON(sint16 * buffer, sint16 * dwt_buffer)
{
	rfx_dwt_2d_decode_block_NEON(buffer + 3840, dwt_buffer, 8);
	rfx_dwt_2d_decode_block_NEON(buffer + 3072, dwt_buffer, 16);
	rfx_dwt_2d_decode_block_NEON(buffer, dwt_buffer, 32);
}



int isNeonSupported()
{
#if defined(ANDROID)
	if (android_getCpuFamily() != ANDROID_CPU_FAMILY_ARM)
	{
		DEBUG_RFX("NEON optimization disabled - No ARM CPU found");
		return 0;
	}

	uint64_t features = android_getCpuFeatures();
	if ((features & ANDROID_CPU_ARM_FEATURE_ARMv7))
	{
		if (features & ANDROID_CPU_ARM_FEATURE_NEON)
		{
			DEBUG_RFX("NEON optimization enabled!");
			return 1;
		}
		DEBUG_RFX("NEON optimization disabled - CPU not NEON capable");
	}
	else
		DEBUG_RFX("NEON optimization disabled - No ARMv7 CPU found");

	return 0;
#else
	return 1;
#endif
}


void rfx_init_neon(RFX_CONTEXT * context)
{


	if(isNeonSupported())
	{
		DEBUG_RFX("Using NEON optimizations");

		IF_PROFILER(context->priv->prof_rfx_decode_ycbcr_to_rgb->name = "rfx_decode_YCbCr_to_RGB_NEON");
		IF_PROFILER(context->priv->prof_rfx_quantization_decode->name = "rfx_quantization_decode_NEON");
		IF_PROFILER(context->priv->prof_rfx_dwt_2d_decode->name = "rfx_dwt_2d_decode_NEON");

		context->decode_ycbcr_to_rgb = rfx_decode_YCbCr_to_RGB_NEON;
		context->quantization_decode = rfx_quantization_decode_NEON;
		context->dwt_2d_decode = rfx_dwt_2d_decode_NEON;
	}
}

#endif // __ARM_NEON__

