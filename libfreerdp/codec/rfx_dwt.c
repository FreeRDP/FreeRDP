/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - DWT
 *
 * Copyright 2011 Vic Lee
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

#include "rfx_dwt.h"

static void rfx_dwt_2d_decode_block(sint16* buffer, sint16* idwt, int subband_width)
{
	sint16 *dst, *l, *h;
	sint16 *l_dst, *h_dst;
	sint16 *hl, *lh, *hh, *ll;
	int total_width;
	int x, y;
	int n;

	total_width = subband_width << 1;

	/* Inverse DWT in horizontal direction, results in 2 sub-bands in L, H order in tmp buffer idwt. */
	/* The 4 sub-bands are stored in HL(0), LH(1), HH(2), LL(3) order. */
	/* The lower part L uses LL(3) and HL(0). */
	/* The higher part H uses LH(1) and HH(2). */

	ll = buffer + subband_width * subband_width * 3;
	hl = buffer;
	l_dst = idwt;

	lh = buffer + subband_width * subband_width;
	hh = buffer + subband_width * subband_width * 2;
	h_dst = idwt + subband_width * subband_width * 2;

	for (y = 0; y < subband_width; y++)
	{
		/* Even coefficients */
		l_dst[0] = ll[0] - ((hl[0] + hl[0] + 1) >> 1);
		h_dst[0] = lh[0] - ((hh[0] + hh[0] + 1) >> 1);
		for (n = 1; n < subband_width; n++)
		{
			x = n << 1;
			l_dst[x] = ll[n] - ((hl[n-1] + hl[n] + 1) >> 1);
			h_dst[x] = lh[n] - ((hh[n-1] + hh[n] + 1) >> 1);
		}

		/* Odd coefficients */
		for (n = 0; n < subband_width-1; n++)
		{
			x = n << 1;
			l_dst[x + 1] = (hl[n] << 1) + ((l_dst[x] + l_dst[x + 2]) >> 1);
			h_dst[x + 1] = (hh[n] << 1) + ((h_dst[x] + h_dst[x + 2]) >> 1);
		}
		x = n << 1;
		l_dst[x + 1] = (hl[n] << 1) + (l_dst[x]);
		h_dst[x + 1] = (hh[n] << 1) + (h_dst[x]);		

		ll += subband_width;
		hl += subband_width;
		l_dst += total_width;

		lh += subband_width;
		hh += subband_width;
		h_dst += total_width;
	}

	/* Inverse DWT in vertical direction, results are stored in original buffer. */
	for (x = 0; x < total_width; x++)
	{
		/* Even coefficients */
		for (n = 0; n < subband_width; n++)
		{
			y = n << 1;
			dst = buffer + y * total_width + x;
			l = idwt + n * total_width + x;
			h = l + subband_width * total_width;
			dst[0] = *l - (((n > 0 ? *(h - total_width) : *h) + (*h) + 1) >> 1);
		}

		/* Odd coefficients */
		for (n = 0; n < subband_width; n++)
		{
			y = n << 1;
			dst = buffer + y * total_width + x;
			l = idwt + n * total_width + x;
			h = l + subband_width * total_width;
			dst[total_width] = (*h << 1) + ((dst[0] + dst[n < subband_width - 1 ? 2 * total_width : 0]) >> 1);
		}
	}
}

void rfx_dwt_2d_decode(sint16* buffer, sint16* dwt_buffer)
{
	rfx_dwt_2d_decode_block(buffer + 3840, dwt_buffer, 8);
	rfx_dwt_2d_decode_block(buffer + 3072, dwt_buffer, 16);
	rfx_dwt_2d_decode_block(buffer, dwt_buffer, 32);
}

static void rfx_dwt_2d_encode_block(sint16* buffer, sint16* dwt, int subband_width)
{
	sint16 *src, *l, *h;
	sint16 *l_src, *h_src;
	sint16 *hl, *lh, *hh, *ll;
	int total_width;
	int x, y;
	int n;

	total_width = subband_width << 1;

	/* DWT in vertical direction, results in 2 sub-bands in L, H order in tmp buffer dwt. */
	for (x = 0; x < total_width; x++)
	{
		for (n = 0; n < subband_width; n++)
		{
			y = n << 1;
			l = dwt + n * total_width + x;
			h = l + subband_width * total_width;
			src = buffer + y * total_width + x;

			/* H */
			*h = (src[total_width] - ((src[0] + src[n < subband_width - 1 ? 2 * total_width : 0]) >> 1)) >> 1;

			/* L */
			*l = src[0] + (n == 0 ? *h : (*(h - total_width) + *h) >> 1);
		}
	}

	/* DWT in horizontal direction, results in 4 sub-bands in HL(0), LH(1), HH(2), LL(3) order, stored in original buffer. */
	/* The lower part L generates LL(3) and HL(0). */
	/* The higher part H generates LH(1) and HH(2). */

	ll = buffer + subband_width * subband_width * 3;
	hl = buffer;
	l_src = dwt;

	lh = buffer + subband_width * subband_width;
	hh = buffer + subband_width * subband_width * 2;
	h_src = dwt + subband_width * subband_width * 2;

	for (y = 0; y < subband_width; y++)
	{
		/* L */
		for (n = 0; n < subband_width; n++)
		{
			x = n << 1;

			/* HL */
			hl[n] = (l_src[x + 1] - ((l_src[x] + l_src[n < subband_width - 1 ? x + 2 : x]) >> 1)) >> 1;
			/* LL */
			ll[n] = l_src[x] + (n == 0 ? hl[n] : (hl[n - 1] + hl[n]) >> 1);
		}

		/* H */
		for (n = 0; n < subband_width; n++)
		{
			x = n << 1;

			/* HH */
			hh[n] = (h_src[x + 1] - ((h_src[x] + h_src[n < subband_width - 1 ? x + 2 : x]) >> 1)) >> 1;
			/* LH */
			lh[n] = h_src[x] + (n == 0 ? hh[n] : (hh[n - 1] + hh[n]) >> 1);
		}

		ll += subband_width;
		hl += subband_width;
		l_src += total_width;

		lh += subband_width;
		hh += subband_width;
		h_src += total_width;
	}
}

void rfx_dwt_2d_encode(sint16* buffer, sint16* dwt_buffer)
{
	rfx_dwt_2d_encode_block(buffer, dwt_buffer, 32);
	rfx_dwt_2d_encode_block(buffer + 3072, dwt_buffer, 16);
	rfx_dwt_2d_encode_block(buffer + 3840, dwt_buffer, 8);
}
