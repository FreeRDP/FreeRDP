/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - SSE2 Optimizations
 *
 * Copyright 2011 Stephen Erisman
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>
#include <emmintrin.h>

#include "rfx_types.h"
#include "rfx_sse2.h"

#ifdef _MSC_VER
#define	__attribute__(...)
#endif

#define CACHE_LINE_BYTES	64

#define _mm_between_epi16(_val, _min, _max) \
	do { _val = _mm_min_epi16(_max, _mm_max_epi16(_val, _min)); } while (0)

static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
_mm_prefetch_buffer(char * buffer, int num_bytes)
{
	__m128i * buf = (__m128i*) buffer;
	int i;
	for (i = 0; i < (num_bytes / sizeof(__m128i)); i+=(CACHE_LINE_BYTES / sizeof(__m128i)))
	{
		_mm_prefetch((char*)(&buf[i]), _MM_HINT_NTA);
	}
}

static void rfx_decode_ycbcr_to_rgb_sse2(sint16* y_r_buffer, sint16* cb_g_buffer, sint16* cr_b_buffer)
{	
	__m128i zero = _mm_setzero_si128();
	__m128i max = _mm_set1_epi16(255);

	__m128i* y_r_buf = (__m128i*) y_r_buffer;
	__m128i* cb_g_buf = (__m128i*) cb_g_buffer;
	__m128i* cr_b_buf = (__m128i*) cr_b_buffer;

	__m128i y;
	__m128i cr;
	__m128i cb;
	__m128i r;
	__m128i g;
	__m128i b;

	int i;

	for (i = 0; i < (4096 * sizeof(sint16) / sizeof(__m128i)); i += (CACHE_LINE_BYTES / sizeof(__m128i)))
	{
		_mm_prefetch((char*)(&y_r_buf[i]), _MM_HINT_NTA);
		_mm_prefetch((char*)(&cb_g_buf[i]), _MM_HINT_NTA);
		_mm_prefetch((char*)(&cr_b_buf[i]), _MM_HINT_NTA);
	}
	for (i = 0; i < (4096 * sizeof(sint16) / sizeof(__m128i)); i++)
	{
		/* y = (y_r_buf[i] >> 5) + 128; */
		y = _mm_load_si128(&y_r_buf[i]);
		y = _mm_add_epi16(_mm_srai_epi16(y, 5), _mm_set1_epi16(128));

		/* cr = cr_b_buf[i]; */
		cr = _mm_load_si128(&cr_b_buf[i]);

		/* r = y + ((cr >> 5) + (cr >> 7) + (cr >> 8) + (cr >> 11) + (cr >> 12) + (cr >> 13)); */
		/* y_r_buf[i] = MINMAX(r, 0, 255); */
		r = _mm_add_epi16(y, _mm_srai_epi16(cr, 5));
		r = _mm_add_epi16(r, _mm_srai_epi16(cr, 7));
		r = _mm_add_epi16(r, _mm_srai_epi16(cr, 8));
		r = _mm_add_epi16(r, _mm_srai_epi16(cr, 11));
		r = _mm_add_epi16(r, _mm_srai_epi16(cr, 12));
		r = _mm_add_epi16(r, _mm_srai_epi16(cr, 13));
		_mm_between_epi16(r, zero, max);
		_mm_store_si128(&y_r_buf[i], r);

		/* cb = cb_g_buf[i]; */
		cb = _mm_load_si128(&cb_g_buf[i]);

		/* g = y - ((cb >> 7) + (cb >> 9) + (cb >> 10)) -
			((cr >> 6) + (cr >> 8) + (cr >> 9) + (cr >> 11) + (cr >> 12) + (cr >> 13)); */
		/* cb_g_buf[i] = MINMAX(g, 0, 255); */
		g = _mm_sub_epi16(y, _mm_srai_epi16(cb, 7));
		g = _mm_sub_epi16(g, _mm_srai_epi16(cb, 9));
		g = _mm_sub_epi16(g, _mm_srai_epi16(cb, 10));
		g = _mm_sub_epi16(g, _mm_srai_epi16(cr, 6));
		g = _mm_sub_epi16(g, _mm_srai_epi16(cr, 8));
		g = _mm_sub_epi16(g, _mm_srai_epi16(cr, 9));
		g = _mm_sub_epi16(g, _mm_srai_epi16(cr, 11));
		g = _mm_sub_epi16(g, _mm_srai_epi16(cr, 12));
		g = _mm_sub_epi16(g, _mm_srai_epi16(cr, 13));
		_mm_between_epi16(g, zero, max);
		_mm_store_si128(&cb_g_buf[i], g);

		/* b = y + ((cb >> 5) + (cb >> 6) + (cb >> 7) + (cb >> 11) + (cb >> 13)); */
		/* cr_b_buf[i] = MINMAX(b, 0, 255); */
		b = _mm_add_epi16(y, _mm_srai_epi16(cb, 5));
		b = _mm_add_epi16(b, _mm_srai_epi16(cb, 6));
		b = _mm_add_epi16(b, _mm_srai_epi16(cb, 7));
		b = _mm_add_epi16(b, _mm_srai_epi16(cb, 11));
		b = _mm_add_epi16(b, _mm_srai_epi16(cb, 13));
		_mm_between_epi16(b, zero, max);
		_mm_store_si128(&cr_b_buf[i], b);
	}
}

/* The encodec YCbCr coeffectients are represented as 11.5 fixed-point numbers. See rfx_encode.c */
static void rfx_encode_rgb_to_ycbcr_sse2(sint16* y_r_buffer, sint16* cb_g_buffer, sint16* cr_b_buffer)
{
	__m128i min = _mm_set1_epi16(-128 << 5);
	__m128i max = _mm_set1_epi16(127 << 5);

	__m128i* y_r_buf = (__m128i*) y_r_buffer;
	__m128i* cb_g_buf = (__m128i*) cb_g_buffer;
	__m128i* cr_b_buf = (__m128i*) cr_b_buffer;

	__m128i y;
	__m128i cr;
	__m128i cb;
	__m128i r;
	__m128i g;
	__m128i b;

	int i;

	for (i = 0; i < (4096 * sizeof(sint16) / sizeof(__m128i)); i += (CACHE_LINE_BYTES / sizeof(__m128i)))
	{
		_mm_prefetch((char*)(&y_r_buf[i]), _MM_HINT_NTA);
		_mm_prefetch((char*)(&cb_g_buf[i]), _MM_HINT_NTA);
		_mm_prefetch((char*)(&cr_b_buf[i]), _MM_HINT_NTA);
	}
	for (i = 0; i < (4096 * sizeof(sint16) / sizeof(__m128i)); i++)
	{
		/* r = y_r_buf[i]; */
		r = _mm_load_si128(&y_r_buf[i]);

		/* g = cb_g_buf[i]; */
		g = _mm_load_si128(&cb_g_buf[i]);

		/* b = cr_b_buf[i]; */
		b = _mm_load_si128(&cr_b_buf[i]);

		/* y = ((r << 3) + (r) + (r >> 1) + (r >> 4) + (r >> 7)) +
			((g << 4) + (g << 1) + (g >> 1) + (g >> 2) + (g >> 5)) +
			((b << 1) + (b) + (b >> 1) + (b >> 3) + (b >> 6) + (b >> 7)); */
		/* y_r_buf[i] = MINMAX(y, 0, (255 << 5)) - (128 << 5); */
		y = _mm_add_epi16(_mm_slli_epi16(r, 3), r);
		y = _mm_add_epi16(y, _mm_srai_epi16(r, 1));
		y = _mm_add_epi16(y, _mm_srai_epi16(r, 4));
		y = _mm_add_epi16(y, _mm_srai_epi16(r, 7));
		y = _mm_add_epi16(y, _mm_slli_epi16(g, 4));
		y = _mm_add_epi16(y, _mm_slli_epi16(g, 1));
		y = _mm_add_epi16(y, _mm_srai_epi16(g, 1));
		y = _mm_add_epi16(y, _mm_srai_epi16(g, 2));
		y = _mm_add_epi16(y, _mm_srai_epi16(g, 5));
		y = _mm_add_epi16(y, _mm_slli_epi16(b, 1));
		y = _mm_add_epi16(y, b);
		y = _mm_add_epi16(y, _mm_srai_epi16(b, 1));
		y = _mm_add_epi16(y, _mm_srai_epi16(b, 3));
		y = _mm_add_epi16(y, _mm_srai_epi16(b, 6));
		y = _mm_add_epi16(y, _mm_srai_epi16(b, 7));
		y = _mm_add_epi16(y, min);
		_mm_between_epi16(y, min, max);
		_mm_store_si128(&y_r_buf[i], y);

		/* cb = 0 - ((r << 2) + (r) + (r >> 2) + (r >> 3) + (r >> 5)) -
			((g << 3) + (g << 1) + (g >> 1) + (g >> 4) + (g >> 5) + (g >> 6)) +
			((b << 4) + (b >> 6)); */
		/* cb_g_buf[i] = MINMAX(cb, (-128 << 5), (127 << 5)); */
		cb = _mm_add_epi16(_mm_slli_epi16(b, 4), _mm_srai_epi16(b, 6));
		cb = _mm_sub_epi16(cb, _mm_slli_epi16(r, 2));
		cb = _mm_sub_epi16(cb, r);
		cb = _mm_sub_epi16(cb, _mm_srai_epi16(r, 2));
		cb = _mm_sub_epi16(cb, _mm_srai_epi16(r, 3));
		cb = _mm_sub_epi16(cb, _mm_srai_epi16(r, 5));
		cb = _mm_sub_epi16(cb, _mm_slli_epi16(g, 3));
		cb = _mm_sub_epi16(cb, _mm_slli_epi16(g, 1));
		cb = _mm_sub_epi16(cb, _mm_srai_epi16(g, 1));
		cb = _mm_sub_epi16(cb, _mm_srai_epi16(g, 4));
		cb = _mm_sub_epi16(cb, _mm_srai_epi16(g, 5));
		cb = _mm_sub_epi16(cb, _mm_srai_epi16(g, 6));
		_mm_between_epi16(cb, min, max);
		_mm_store_si128(&cb_g_buf[i], cb);

		/* cr = ((r << 4) - (r >> 7)) -
			((g << 3) + (g << 2) + (g) + (g >> 2) + (g >> 3) + (g >> 6)) -
			((b << 1) + (b >> 1) + (b >> 4) + (b >> 5) + (b >> 7)); */
		/* cr_b_buf[i] = MINMAX(cr, (-128 << 5), (127 << 5)); */
		cr = _mm_sub_epi16(_mm_slli_epi16(r, 4), _mm_srai_epi16(r, 7));
		cr = _mm_sub_epi16(cr, _mm_slli_epi16(g, 3));
		cr = _mm_sub_epi16(cr, _mm_slli_epi16(g, 2));
		cr = _mm_sub_epi16(cr, g);
		cr = _mm_sub_epi16(cr, _mm_srai_epi16(g, 2));
		cr = _mm_sub_epi16(cr, _mm_srai_epi16(g, 3));
		cr = _mm_sub_epi16(cr, _mm_srai_epi16(g, 6));
		cr = _mm_sub_epi16(cr, _mm_slli_epi16(b, 1));
		cr = _mm_sub_epi16(cr, _mm_srai_epi16(b, 1));
		cr = _mm_sub_epi16(cr, _mm_srai_epi16(b, 4));
		cr = _mm_sub_epi16(cr, _mm_srai_epi16(b, 5));
		cr = _mm_sub_epi16(cr, _mm_srai_epi16(b, 7));
		_mm_between_epi16(cr, min, max);
		_mm_store_si128(&cr_b_buf[i], cr);
	}
}

static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
rfx_quantization_decode_block_sse2(sint16* buffer, const int buffer_size, const uint32 factor)
{
	__m128i a;
	__m128i * ptr = (__m128i*) buffer;
	__m128i * buf_end = (__m128i*) (buffer + buffer_size);

	if (factor == 0)
		return;

	do
	{
		a = _mm_load_si128(ptr);
		a = _mm_slli_epi16(a, factor);
		_mm_store_si128(ptr, a);

		ptr++;
	} while(ptr < buf_end);
}

static void rfx_quantization_decode_sse2(sint16* buffer, const uint32* quantization_values)
{
	_mm_prefetch_buffer((char*) buffer, 4096 * sizeof(sint16));

	rfx_quantization_decode_block_sse2(buffer, 4096, 5);

	rfx_quantization_decode_block_sse2(buffer, 1024, quantization_values[8] - 6); /* HL1 */
	rfx_quantization_decode_block_sse2(buffer + 1024, 1024, quantization_values[7] - 6); /* LH1 */
	rfx_quantization_decode_block_sse2(buffer + 2048, 1024, quantization_values[9] - 6); /* HH1 */
	rfx_quantization_decode_block_sse2(buffer + 3072, 256, quantization_values[5] - 6); /* HL2 */
	rfx_quantization_decode_block_sse2(buffer + 3328, 256, quantization_values[4] - 6); /* LH2 */
	rfx_quantization_decode_block_sse2(buffer + 3584, 256, quantization_values[6] - 6); /* HH2 */
	rfx_quantization_decode_block_sse2(buffer + 3840, 64, quantization_values[2] - 6); /* HL3 */
	rfx_quantization_decode_block_sse2(buffer + 3904, 64, quantization_values[1] - 6); /* LH3 */
	rfx_quantization_decode_block_sse2(buffer + 3968, 64, quantization_values[3] - 6); /* HH3 */
	rfx_quantization_decode_block_sse2(buffer + 4032, 64, quantization_values[0] - 6); /* LL3 */
}

static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
rfx_quantization_encode_block_sse2(sint16* buffer, const int buffer_size, const uint32 factor)
{
	__m128i a;
	__m128i* ptr = (__m128i*) buffer;
	__m128i* buf_end = (__m128i*) (buffer + buffer_size);
	__m128i half;

	if (factor == 0)
		return;

	half = _mm_set1_epi16(1 << (factor - 1));
	do
	{
		a = _mm_load_si128(ptr);
		a = _mm_add_epi16(a, half);
		a = _mm_srai_epi16(a, factor);
		_mm_store_si128(ptr, a);

		ptr++;
	} while(ptr < buf_end);
}

static void rfx_quantization_encode_sse2(sint16* buffer, const uint32* quantization_values)
{
	_mm_prefetch_buffer((char*) buffer, 4096 * sizeof(sint16));

	rfx_quantization_encode_block_sse2(buffer, 1024, quantization_values[8] - 6); /* HL1 */
	rfx_quantization_encode_block_sse2(buffer + 1024, 1024, quantization_values[7] - 6); /* LH1 */
	rfx_quantization_encode_block_sse2(buffer + 2048, 1024, quantization_values[9] - 6); /* HH1 */
	rfx_quantization_encode_block_sse2(buffer + 3072, 256, quantization_values[5] - 6); /* HL2 */
	rfx_quantization_encode_block_sse2(buffer + 3328, 256, quantization_values[4] - 6); /* LH2 */
	rfx_quantization_encode_block_sse2(buffer + 3584, 256, quantization_values[6] - 6); /* HH2 */
	rfx_quantization_encode_block_sse2(buffer + 3840, 64, quantization_values[2] - 6); /* HL3 */
	rfx_quantization_encode_block_sse2(buffer + 3904, 64, quantization_values[1] - 6); /* LH3 */
	rfx_quantization_encode_block_sse2(buffer + 3968, 64, quantization_values[3] - 6); /* HH3 */
	rfx_quantization_encode_block_sse2(buffer + 4032, 64, quantization_values[0] - 6); /* LL3 */

	rfx_quantization_encode_block_sse2(buffer, 4096, 5);
}

static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
rfx_dwt_2d_decode_block_horiz_sse2(sint16* l, sint16* h, sint16* dst, int subband_width)
{
	int y, n;
	sint16* l_ptr = l;
	sint16* h_ptr = h;
	sint16* dst_ptr = dst;
	int first;
	int last;
	__m128i l_n;
	__m128i h_n;
	__m128i h_n_m;
	__m128i tmp_n;
	__m128i dst_n;
	__m128i dst_n_p;
	__m128i dst1;
	__m128i dst2;

	for (y = 0; y < subband_width; y++)
	{
		/* Even coefficients */
		for (n = 0; n < subband_width; n+=8)
		{
			/* dst[2n] = l[n] - ((h[n-1] + h[n] + 1) >> 1); */
			
			l_n = _mm_load_si128((__m128i*) l_ptr);

			h_n = _mm_load_si128((__m128i*) h_ptr);
			h_n_m = _mm_loadu_si128((__m128i*) (h_ptr - 1));
			if (n == 0)
			{
				first = _mm_extract_epi16(h_n_m, 1);
				h_n_m = _mm_insert_epi16(h_n_m, first, 0);
			}
			
			tmp_n = _mm_add_epi16(h_n, h_n_m);
			tmp_n = _mm_add_epi16(tmp_n, _mm_set1_epi16(1));
			tmp_n = _mm_srai_epi16(tmp_n, 1);
			
			dst_n = _mm_sub_epi16(l_n, tmp_n);
			
			_mm_store_si128((__m128i*) l_ptr, dst_n);
			
			l_ptr+=8;
			h_ptr+=8;
		}
		l_ptr -= subband_width;
		h_ptr -= subband_width;
		
		/* Odd coefficients */
		for (n = 0; n < subband_width; n+=8)
		{
			/* dst[2n + 1] = (h[n] << 1) + ((dst[2n] + dst[2n + 2]) >> 1); */
			
			h_n = _mm_load_si128((__m128i*) h_ptr);
			
			h_n = _mm_slli_epi16(h_n, 1);
			
			dst_n = _mm_load_si128((__m128i*) (l_ptr));
			dst_n_p = _mm_loadu_si128((__m128i*) (l_ptr + 1));
			if (n == subband_width - 8)
			{
				last = _mm_extract_epi16(dst_n_p, 6);
				dst_n_p = _mm_insert_epi16(dst_n_p, last, 7);
			}
			
			tmp_n = _mm_add_epi16(dst_n_p, dst_n);
			tmp_n = _mm_srai_epi16(tmp_n, 1);
			
			tmp_n = _mm_add_epi16(tmp_n, h_n);
			
			dst1 = _mm_unpacklo_epi16(dst_n, tmp_n);
			dst2 = _mm_unpackhi_epi16(dst_n, tmp_n);
			
			_mm_store_si128((__m128i*) dst_ptr, dst1);
			_mm_store_si128((__m128i*) (dst_ptr + 8), dst2);
			
			l_ptr+=8;
			h_ptr+=8;
			dst_ptr+=16;
		}
	}
}

static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
rfx_dwt_2d_decode_block_vert_sse2(sint16* l, sint16* h, sint16* dst, int subband_width)
{
	int x, n;
	sint16* l_ptr = l;
	sint16* h_ptr = h;
	sint16* dst_ptr = dst;
	__m128i l_n;
	__m128i h_n;
	__m128i tmp_n;
	__m128i h_n_m;
	__m128i dst_n;
	__m128i dst_n_m;
	__m128i dst_n_p;
	
	int total_width = subband_width + subband_width;

	/* Even coefficients */
	for (n = 0; n < subband_width; n++)
	{
		for (x = 0; x < total_width; x+=8)
		{
			/* dst[2n] = l[n] - ((h[n-1] + h[n] + 1) >> 1); */
			
			l_n = _mm_load_si128((__m128i*) l_ptr);
			h_n = _mm_load_si128((__m128i*) h_ptr);
			
			tmp_n = _mm_add_epi16(h_n, _mm_set1_epi16(1));;
			if (n == 0)
				tmp_n = _mm_add_epi16(tmp_n, h_n);
			else
			{
				h_n_m = _mm_loadu_si128((__m128i*) (h_ptr - total_width));
				tmp_n = _mm_add_epi16(tmp_n, h_n_m);
			}
			tmp_n = _mm_srai_epi16(tmp_n, 1);
			
			dst_n = _mm_sub_epi16(l_n, tmp_n);
			_mm_store_si128((__m128i*) dst_ptr, dst_n);
			
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
			/* dst[2n + 1] = (h[n] << 1) + ((dst[2n] + dst[2n + 2]) >> 1); */
			
			h_n = _mm_load_si128((__m128i*) h_ptr);
			dst_n_m = _mm_load_si128((__m128i*) (dst_ptr - total_width));
			h_n = _mm_slli_epi16(h_n, 1);
			
			tmp_n = dst_n_m;
			if (n == subband_width - 1)
				tmp_n = _mm_add_epi16(tmp_n, dst_n_m);
			else
			{
				dst_n_p = _mm_loadu_si128((__m128i*) (dst_ptr + total_width));
				tmp_n = _mm_add_epi16(tmp_n, dst_n_p);
			}
			tmp_n = _mm_srai_epi16(tmp_n, 1);
			
			dst_n = _mm_add_epi16(tmp_n, h_n);
			_mm_store_si128((__m128i*) dst_ptr, dst_n);

			h_ptr+=8;
			dst_ptr+=8;
		}
		dst_ptr+=total_width;
	}
}

static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
rfx_dwt_2d_decode_block_sse2(sint16* buffer, sint16* idwt, int subband_width)
{
	sint16 *hl, *lh, *hh, *ll;
	sint16 *l_dst, *h_dst;

	_mm_prefetch_buffer((char*) idwt, subband_width * 4 * sizeof(sint16));

	/* Inverse DWT in horizontal direction, results in 2 sub-bands in L, H order in tmp buffer idwt. */
	/* The 4 sub-bands are stored in HL(0), LH(1), HH(2), LL(3) order. */
	/* The lower part L uses LL(3) and HL(0). */
	/* The higher part H uses LH(1) and HH(2). */

	ll = buffer + subband_width * subband_width * 3;
	hl = buffer;
	l_dst = idwt;

	rfx_dwt_2d_decode_block_horiz_sse2(ll, hl, l_dst, subband_width);

	lh = buffer + subband_width * subband_width;
	hh = buffer + subband_width * subband_width * 2;
	h_dst = idwt + subband_width * subband_width * 2;
	
	rfx_dwt_2d_decode_block_horiz_sse2(lh, hh, h_dst, subband_width);

	/* Inverse DWT in vertical direction, results are stored in original buffer. */
	rfx_dwt_2d_decode_block_vert_sse2(l_dst, h_dst, buffer, subband_width);
}

static void rfx_dwt_2d_decode_sse2(sint16* buffer, sint16* dwt_buffer)
{
	_mm_prefetch_buffer((char*) buffer, 4096 * sizeof(sint16));
	
	rfx_dwt_2d_decode_block_sse2(buffer + 3840, dwt_buffer, 8);
	rfx_dwt_2d_decode_block_sse2(buffer + 3072, dwt_buffer, 16);
	rfx_dwt_2d_decode_block_sse2(buffer, dwt_buffer, 32);
}

static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
rfx_dwt_2d_encode_block_vert_sse2(sint16* src, sint16* l, sint16* h, int subband_width)
{
	int total_width;
	int x;
	int n;
	__m128i src_2n;
	__m128i src_2n_1;
	__m128i src_2n_2;
	__m128i h_n;
	__m128i h_n_m;
	__m128i l_n;

	total_width = subband_width << 1;

	for (n = 0; n < subband_width; n++)
	{
		for (x = 0; x < total_width; x += 8)
		{
			src_2n = _mm_load_si128((__m128i*) src);
			src_2n_1 = _mm_load_si128((__m128i*) (src + total_width));
			if (n < subband_width - 1)
				src_2n_2 = _mm_load_si128((__m128i*) (src + 2 * total_width));
			else
				src_2n_2 = src_2n;

			/* h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1 */

			h_n = _mm_add_epi16(src_2n, src_2n_2);
			h_n = _mm_srai_epi16(h_n, 1);
			h_n = _mm_sub_epi16(src_2n_1, h_n);
			h_n = _mm_srai_epi16(h_n, 1);

			_mm_store_si128((__m128i*) h, h_n);

			if (n == 0)
				h_n_m = h_n;
			else
				h_n_m = _mm_load_si128((__m128i*) (h - total_width));

			/* l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1) */

			l_n = _mm_add_epi16(h_n_m, h_n);
			l_n = _mm_srai_epi16(l_n, 1);
			l_n = _mm_add_epi16(l_n, src_2n);

			_mm_store_si128((__m128i*) l, l_n);

			src += 8;
			l += 8;
			h += 8;
		}
		src += total_width;
	}
}

static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
rfx_dwt_2d_encode_block_horiz_sse2(sint16* src, sint16* l, sint16* h, int subband_width)
{
	int y;
	int n;
	int first;
	__m128i src_2n;
	__m128i src_2n_1;
	__m128i src_2n_2;
	__m128i h_n;
	__m128i h_n_m;
	__m128i l_n;

	for (y = 0; y < subband_width; y++)
	{
		for (n = 0; n < subband_width; n += 8)
		{
			/* The following 3 Set operations consumes more than half of the total DWT processing time! */
			src_2n = _mm_set_epi16(src[14], src[12], src[10], src[8], src[6], src[4], src[2], src[0]);
			src_2n_1 = _mm_set_epi16(src[15], src[13], src[11], src[9], src[7], src[5], src[3], src[1]);
			src_2n_2 = _mm_set_epi16(n == subband_width - 8 ? src[14] : src[16],
				src[14], src[12], src[10], src[8], src[6], src[4], src[2]);

			/* h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1 */

			h_n = _mm_add_epi16(src_2n, src_2n_2);
			h_n = _mm_srai_epi16(h_n, 1);
			h_n = _mm_sub_epi16(src_2n_1, h_n);
			h_n = _mm_srai_epi16(h_n, 1);

			_mm_store_si128((__m128i*) h, h_n);

			h_n_m = _mm_loadu_si128((__m128i*) (h - 1));
			if (n == 0)
			{
				first = _mm_extract_epi16(h_n_m, 1);
				h_n_m = _mm_insert_epi16(h_n_m, first, 0);
			}

			/* l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1) */

			l_n = _mm_add_epi16(h_n_m, h_n);
			l_n = _mm_srai_epi16(l_n, 1);
			l_n = _mm_add_epi16(l_n, src_2n);

			_mm_store_si128((__m128i*) l, l_n);

			src += 16;
			l += 8;
			h += 8;
		}
	}
}

static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
rfx_dwt_2d_encode_block_sse2(sint16* buffer, sint16* dwt, int subband_width)
{
	sint16 *hl, *lh, *hh, *ll;
	sint16 *l_src, *h_src;

	_mm_prefetch_buffer((char*) dwt, subband_width * 4 * sizeof(sint16));

	/* DWT in vertical direction, results in 2 sub-bands in L, H order in tmp buffer dwt. */

	l_src = dwt;
	h_src = dwt + subband_width * subband_width * 2;

	rfx_dwt_2d_encode_block_vert_sse2(buffer, l_src, h_src, subband_width);

	/* DWT in horizontal direction, results in 4 sub-bands in HL(0), LH(1), HH(2), LL(3) order, stored in original buffer. */
	/* The lower part L generates LL(3) and HL(0). */
	/* The higher part H generates LH(1) and HH(2). */

	ll = buffer + subband_width * subband_width * 3;
	hl = buffer;

	lh = buffer + subband_width * subband_width;
	hh = buffer + subband_width * subband_width * 2;

	rfx_dwt_2d_encode_block_horiz_sse2(l_src, ll, hl, subband_width);
	rfx_dwt_2d_encode_block_horiz_sse2(h_src, lh, hh, subband_width);
}

static void rfx_dwt_2d_encode_sse2(sint16* buffer, sint16* dwt_buffer)
{
	_mm_prefetch_buffer((char*) buffer, 4096 * sizeof(sint16));
	
	rfx_dwt_2d_encode_block_sse2(buffer, dwt_buffer, 32);
	rfx_dwt_2d_encode_block_sse2(buffer + 3072, dwt_buffer, 16);
	rfx_dwt_2d_encode_block_sse2(buffer + 3840, dwt_buffer, 8);
}

void rfx_init_sse2(RFX_CONTEXT* context)
{
	DEBUG_RFX("Using SSE2 optimizations");

	IF_PROFILER(context->priv->prof_rfx_decode_ycbcr_to_rgb->name = "rfx_decode_ycbcr_to_rgb_sse2");
	IF_PROFILER(context->priv->prof_rfx_encode_rgb_to_ycbcr->name = "rfx_encode_rgb_to_ycbcr_sse2");
	IF_PROFILER(context->priv->prof_rfx_quantization_decode->name = "rfx_quantization_decode_sse2");
	IF_PROFILER(context->priv->prof_rfx_quantization_encode->name = "rfx_quantization_encode_sse2");
	IF_PROFILER(context->priv->prof_rfx_dwt_2d_decode->name = "rfx_dwt_2d_decode_sse2");
	IF_PROFILER(context->priv->prof_rfx_dwt_2d_encode->name = "rfx_dwt_2d_encode_sse2");

	context->decode_ycbcr_to_rgb = rfx_decode_ycbcr_to_rgb_sse2;
	context->encode_rgb_to_ycbcr = rfx_encode_rgb_to_ycbcr_sse2;
	context->quantization_decode = rfx_quantization_decode_sse2;
	context->quantization_encode = rfx_quantization_encode_sse2;
	context->dwt_2d_decode = rfx_dwt_2d_decode_sse2;
	context->dwt_2d_encode = rfx_dwt_2d_encode_sse2;
}
