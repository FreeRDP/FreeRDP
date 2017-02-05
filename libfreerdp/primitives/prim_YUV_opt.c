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

static primitives_t* generic = NULL;

#ifdef WITH_SSE2

#include <emmintrin.h>
#include <tmmintrin.h>


/****************************************************************************/
/* SSSE3 YUV420 -> RGB conversion                                           */
/****************************************************************************/

static pstatus_t ssse3_YUV420ToRGB_BGRX(
	const BYTE** pSrc, const UINT32* srcStep,
	BYTE* pDst, UINT32 dstStep, UINT32 dstFormat,
	const prim_size_t* roi)
{
	UINT32 lastRow, lastCol;
	BYTE* UData, *VData, *YData;
	UINT32 i, nWidth, nHeight, VaddDst, VaddY, VaddU, VaddV;
	__m128i r0, r1, r2, r3, r4, r5, r6, r7;
	__m128i* buffer;
	/* last_line: if the last (U,V doubled) line should be skipped, set to 10B
	 * last_column: if it's the last column in a line, set to 10B (for handling line-endings not multiple by four) */
	buffer = _aligned_malloc(4 * 16, 16);
	YData = (BYTE*) pSrc[0];
	UData = (BYTE*) pSrc[1];
	VData = (BYTE*) pSrc[2];
	nWidth = roi->width;
	nHeight = roi->height;

	if ((lastCol = (nWidth & 3)))
	{
		switch (lastCol)
		{
			case 1:
				r7 = _mm_set_epi32(0, 0, 0, 0xFFFFFFFF);
				break;

			case 2:
				r7 = _mm_set_epi32(0, 0, 0xFFFFFFFF, 0xFFFFFFFF);
				break;

			case 3:
				r7 = _mm_set_epi32(0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
				break;
		}

		_mm_store_si128(buffer + 3, r7);
		lastCol = 1;
	}

	nWidth += 3;
	nWidth = nWidth >> 2;
	lastRow = nHeight & 1;
	nHeight++;
	nHeight = nHeight >> 1;
	VaddDst = (dstStep << 1) - (nWidth << 4);
	VaddY = (srcStep[0] << 1) - (nWidth << 2);
	VaddU = srcStep[1] - (((nWidth << 1) + 2) & 0xFFFC);
	VaddV = srcStep[2] - (((nWidth << 1) + 2) & 0xFFFC);

	while (nHeight-- > 0)
	{
		if (nHeight == 0)
			lastRow <<= 1;

		i = 0;

		do
		{
			if (!(i & 0x01))
			{
				/* Y-, U- and V-data is stored in different arrays.
				* We start with processing U-data.
				*
				* at first we fetch four U-values from its array and shuffle them like this:
				*	0d0d 0c0c 0b0b 0a0a
				* we've done two things: converting the values to signed words and duplicating
				* each value, because always two pixel "share" the same U- (and V-) data */
				r0 = _mm_cvtsi32_si128(*(UINT32*)UData);
				r5 = _mm_set_epi32(0x80038003, 0x80028002, 0x80018001, 0x80008000);
				r0 = _mm_shuffle_epi8(r0, r5);
				UData += 4;
				/* then we subtract 128 from each value, so we get D */
				r3 = _mm_set_epi16(128, 128, 128, 128, 128, 128, 128, 128);
				r0 = _mm_subs_epi16(r0, r3);
				/* we need to do two things with our D, so let's store it for later use */
				r2 = r0;
				/* now we can multiply our D with 48 and unpack it to xmm4:xmm0
				 * this is what we need to get G data later on */
				r4 = r0;
				r7 = _mm_set_epi16(48, 48, 48, 48, 48, 48, 48, 48);
				r0 = _mm_mullo_epi16(r0, r7);
				r4 = _mm_mulhi_epi16(r4, r7);
				r7 = r0;
				r0 = _mm_unpacklo_epi16(r0, r4);
				r4 = _mm_unpackhi_epi16(r7, r4);
				/* to get B data, we need to prepare a second value, D*475 */
				r1 = r2;
				r7 = _mm_set_epi16(475, 475, 475, 475, 475, 475, 475, 475);
				r1 = _mm_mullo_epi16(r1, r7);
				r2 = _mm_mulhi_epi16(r2, r7);
				r7 = r1;
				r1 = _mm_unpacklo_epi16(r1, r2);
				r7 = _mm_unpackhi_epi16(r7, r2);
				/* so we got something like this: xmm7:xmm1
				 * this pair contains values for 16 pixel:
				 * aabbccdd
				 * aabbccdd, but we can only work on four pixel at once, so we need to save upper values */
				_mm_store_si128(buffer + 1, r7);
				/* Now we've prepared U-data. Preparing V-data is actually the same, just with other coefficients */
				r2 = _mm_cvtsi32_si128(*(UINT32*)VData);
				r2 = _mm_shuffle_epi8(r2, r5);
				VData += 4;
				r2 = _mm_subs_epi16(r2, r3);
				r5 = r2;
				/* this is also known as E*403, we need it to convert R data */
				r3 = r2;
				r7 = _mm_set_epi16(403, 403, 403, 403, 403, 403, 403, 403);
				r2 = _mm_mullo_epi16(r2, r7);
				r3 = _mm_mulhi_epi16(r3, r7);
				r7 = r2;
				r2 = _mm_unpacklo_epi16(r2, r3);
				r7 = _mm_unpackhi_epi16(r7, r3);
				/* and preserve upper four values for future ... */
				_mm_store_si128(buffer + 2, r7);
				/* doing this step: E*120 */
				r3 = r5;
				r7 = _mm_set_epi16(120, 120, 120, 120, 120, 120, 120, 120);
				r3 = _mm_mullo_epi16(r3, r7);
				r5 = _mm_mulhi_epi16(r5, r7);
				r7 = r3;
				r3 = _mm_unpacklo_epi16(r3, r5);
				r7 = _mm_unpackhi_epi16(r7, r5);
				/* now we complete what we've begun above:
				 * (48*D) + (120*E) = (48*D +120*E) */
				r0 = _mm_add_epi32(r0, r3);
				r4 = _mm_add_epi32(r4, r7);
				/* and store to memory ! */
				_mm_store_si128(buffer, r4);
			}
			else
			{
				/* maybe you've wondered about the conditional above ?
				 * Well, we prepared UV data for eight pixel in each line, but can only process four
				 * per loop. So we need to load the upper four pixel data from memory each secound loop! */
				r1 = _mm_load_si128(buffer + 1);
				r2 = _mm_load_si128(buffer + 2);
				r0 = _mm_load_si128(buffer);
			}

			if (++i == nWidth)
				lastCol <<= 1;

			/* We didn't produce any output yet, so let's do so!
			 * Ok, fetch four pixel from the Y-data array and shuffle them like this:
			 * 00d0 00c0 00b0 00a0, to get signed dwords and multiply by 256 */
			r4 = _mm_cvtsi32_si128(*(UINT32*)YData);
			r7 = _mm_set_epi32(0x80800380, 0x80800280, 0x80800180, 0x80800080);
			r4 = _mm_shuffle_epi8(r4, r7);
			r5 = r4;
			r6 = r4;
			/* no we can perform the "real" conversion itself and produce output! */
			r4 = _mm_add_epi32(r4, r2);
			r5 = _mm_sub_epi32(r5, r0);
			r6 = _mm_add_epi32(r6, r1);
			/* in the end, we only need bytes for RGB values.
			 * So, what do we do? right! shifting left makes values bigger and thats always good.
			 * before we had dwords of data, and by shifting left and treating the result
			 * as packed words, we get not only signed words, but do also divide by 256
			 * imagine, data is now ordered this way: ddx0 ccx0 bbx0 aax0, and x is the least
			 * significant byte, that we don't need anymore, because we've done some rounding */
			r4 = _mm_slli_epi32(r4, 8);
			r5 = _mm_slli_epi32(r5, 8);
			r6 = _mm_slli_epi32(r6, 8);
			/* one thing we still have to face is the clip() function ...
			 * we have still signed words, and there are those min/max instructions in SSE2 ...
			 * the max instruction takes always the bigger of the two operands and stores it in the first one,
			 * and it operates with signs !
			 * if we feed it with our values and zeros, it takes the zeros if our values are smaller than
			 * zero and otherwise our values */
			r7 = _mm_set_epi32(0, 0, 0, 0);
			r4 = _mm_max_epi16(r4, r7);
			r5 = _mm_max_epi16(r5, r7);
			r6 = _mm_max_epi16(r6, r7);
			/* the same thing just completely different can be used to limit our values to 255,
			 * but now using the min instruction and 255s */
			r7 = _mm_set_epi32(0x00FF0000, 0x00FF0000, 0x00FF0000, 0x00FF0000);
			r4 = _mm_min_epi16(r4, r7);
			r5 = _mm_min_epi16(r5, r7);
			r6 = _mm_min_epi16(r6, r7);
			/* Now we got our bytes.
			 * the moment has come to assemble the three channels R,G and B to the xrgb dwords
			 * on Red channel we just have to and each futural dword with 00FF0000H */
			//r7=_mm_set_epi32(0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000);
			r4 = _mm_and_si128(r4, r7);
			/* on Green channel we have to shuffle somehow, so we get something like this:
			 * 00d0 00c0 00b0 00a0 */
			r7 = _mm_set_epi32(0x80800E80, 0x80800A80, 0x80800680, 0x80800280);
			r5 = _mm_shuffle_epi8(r5, r7);
			/* and on Blue channel that one:
			 * 000d 000c 000b 000a */
			r7 = _mm_set_epi32(0x8080800E, 0x8080800A, 0x80808006, 0x80808002);
			r6 = _mm_shuffle_epi8(r6, r7);
			/* and at last we or it together and get this one:
			 * xrgb xrgb xrgb xrgb */
			r4 = _mm_or_si128(r4, r5);
			r4 = _mm_or_si128(r4, r6);

			/* Only thing to do know is writing data to memory, but this gets a bit more
			 * complicated if the width is not a multiple of four and it is the last column in line. */
			if (lastCol & 0x02)
			{
				/* let's say, we need to only convert six pixel in width
				 * Ok, the first 4 pixel will be converted just like every 4 pixel else, but
				 * if it's the last loop in line, last_column is shifted left by one (curious? have a look above),
				 * and we land here. Through initialisation a mask was prepared. In this case it looks like
				 * 0000FFFFH 0000FFFFH 0000FFFFH 0000FFFFH */
				r6 = _mm_load_si128(buffer + 3);
				/* we and our output data with this mask to get only the valid pixel */
				r4 = _mm_and_si128(r4, r6);
				/* then we fetch memory from the destination array ... */
				r5 = _mm_lddqu_si128((__m128i*)pDst);
				/* ... and and it with the inverse mask. We get only those pixel, which should not be updated */
				r6 = _mm_andnot_si128(r6, r5);
				/* we only have to or the two values together and write it back to the destination array,
				 * and only the pixel that should be updated really get changed. */
				r4 = _mm_or_si128(r4, r6);
			}

			_mm_storeu_si128((__m128i*)pDst, r4);

			if (!(lastRow & 0x02))
			{
				/* Because UV data is the same for two lines, we can process the secound line just here,
				 * in the same loop. Only thing we need to do is to add some offsets to the Y- and destination
				 * pointer. These offsets are iStride[0] and the target scanline.
				 * But if we don't need to process the secound line, like if we are in the last line of processing nine lines,
				 * we just skip all this. */
				r4 = _mm_cvtsi32_si128(*(UINT32*)(YData + srcStep[0]));
				r7 = _mm_set_epi32(0x80800380, 0x80800280, 0x80800180, 0x80800080);
				r4 = _mm_shuffle_epi8(r4, r7);
				r5 = r4;
				r6 = r4;
				r4 = _mm_add_epi32(r4, r2);
				r5 = _mm_sub_epi32(r5, r0);
				r6 = _mm_add_epi32(r6, r1);
				r4 = _mm_slli_epi32(r4, 8);
				r5 = _mm_slli_epi32(r5, 8);
				r6 = _mm_slli_epi32(r6, 8);
				r7 = _mm_set_epi32(0, 0, 0, 0);
				r4 = _mm_max_epi16(r4, r7);
				r5 = _mm_max_epi16(r5, r7);
				r6 = _mm_max_epi16(r6, r7);
				r7 = _mm_set_epi32(0x00FF0000, 0x00FF0000, 0x00FF0000, 0x00FF0000);
				r4 = _mm_min_epi16(r4, r7);
				r5 = _mm_min_epi16(r5, r7);
				r6 = _mm_min_epi16(r6, r7);
				r7 = _mm_set_epi32(0x00FF0000, 0x00FF0000, 0x00FF0000, 0x00FF0000);
				r4 = _mm_and_si128(r4, r7);
				r7 = _mm_set_epi32(0x80800E80, 0x80800A80, 0x80800680, 0x80800280);
				r5 = _mm_shuffle_epi8(r5, r7);
				r7 = _mm_set_epi32(0x8080800E, 0x8080800A, 0x80808006, 0x80808002);
				r6 = _mm_shuffle_epi8(r6, r7);
				r4 = _mm_or_si128(r4, r5);
				r4 = _mm_or_si128(r4, r6);

				if (lastCol & 0x02)
				{
					r6 = _mm_load_si128(buffer + 3);
					r4 = _mm_and_si128(r4, r6);
					r5 = _mm_lddqu_si128((__m128i*)(pDst + dstStep));
					r6 = _mm_andnot_si128(r6, r5);
					r4 = _mm_or_si128(r4, r6);
					/* only thing is, we should shift [rbp-42] back here, because we have processed the last column,
					 * and this "special condition" can be released */
					lastCol >>= 1;
				}

				_mm_storeu_si128((__m128i*)(pDst + dstStep), r4);
			}

			/* after all we have to increase the destination- and Y-data pointer by four pixel */
			pDst += 16;
			YData += 4;
		}
		while (i < nWidth);

		/* after each line we have to add the scanline to the destination pointer, because
		 * we are processing two lines at once, but only increasing the destination pointer
		 * in the first line. Well, we only have one pointer, so it's the easiest way to access
		 * the secound line with the one pointer and an offset (scanline)
		 * if we're not converting the full width of the scanline, like only 64 pixel, but the
		 * output buffer was "designed" for 1920p HD, we have to add the remaining length for each line,
		 * to get into the next line. */
		pDst += VaddDst;
		/* same thing has to be done for Y-data, but with iStride[0] instead of the target scanline */
		YData += VaddY;
		/* and again for UV data, but here it's enough to add the remaining length, because
		 * UV data is the same for two lines and there exists only one "UV line" on two "real lines" */
		UData += VaddU;
		VData += VaddV;
	}

	_aligned_free(buffer);
	return PRIMITIVES_SUCCESS;
}

static pstatus_t ssse3_YUV420ToRGB(
	const BYTE** pSrc, const UINT32* srcStep,
	BYTE* pDst, UINT32 dstStep, UINT32 DstFormat,
	const prim_size_t* roi)
{
	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRX32:
		case PIXEL_FORMAT_BGRA32:
			return ssse3_YUV420ToRGB_BGRX(pSrc, srcStep, pDst, dstStep, DstFormat, roi);

		default:
			return generic->YUV420ToRGB_8u_P3AC4R(pSrc, srcStep, pDst, dstStep, DstFormat, roi);
	}
}




/****************************************************************************/
/* SSSE3 RGB -> YUV420 conversion                                          **/
/****************************************************************************/


/**
 * Note (nfedera):
 * The used forward transformation factors from RGB to YUV are based on the
 * values specified in [Rec. ITU-R BT.709-6] Section 3:
 * http://www.itu.int/rec/R-REC-BT.709-6-201506-I/en
 *
 * Y =  0.21260 * R + 0.71520 * G + 0.07220 * B +   0;
 * U = -0.11457 * R - 0.38543 * G + 0.50000 * B + 128;
 * V =  0.50000 * R - 0.45415 * G - 0.04585 * B + 128;
 *
 * The most accurate integer artmethic approximation when using 8-bit signed
 * integer factors with 16-bit signed integer intermediate results is:
 *
 * Y = ( ( 27 * R + 92 * G +  9 * B) >> 7 );
 * U = ( (-15 * R - 49 * G + 64 * B) >> 7 ) + 128;
 * V = ( ( 64 * R - 58 * G -  6 * B) >> 7 ) + 128;
 *
 */

PRIM_ALIGN_128 static const BYTE bgrx_y_factors[] = {
	   9,  92,  27,   0,   9,  92,  27,   0,   9,  92,  27,   0,   9,  92,  27,   0
};
PRIM_ALIGN_128 static const BYTE bgrx_u_factors[] = {
	  64, -49, -15,   0,  64, -49, -15,   0,  64, -49, -15,   0,  64, -49, -15,   0
};
PRIM_ALIGN_128 static const BYTE bgrx_v_factors[] = {
	  -6, -58,  64,   0,  -6, -58,  64,   0,  -6, -58,  64,   0,  -6, -58,  64,   0
};

PRIM_ALIGN_128 static const BYTE const_buf_128b[] = {
	 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128
};

/*
TODO:
RGB[AX] can simply be supported using the following factors. And instead of loading the
globals directly the functions below could be passed pointers to the correct vectors
depending on the source picture format.

PRIM_ALIGN_128 static const BYTE rgbx_y_factors[] = {
	  27,  92,   9,   0,  27,  92,   9,   0,  27,  92,   9,   0,  27,  92,   9,   0
};
PRIM_ALIGN_128 static const BYTE rgbx_u_factors[] = {
	 -15, -49,  64,   0, -15, -49,  64,   0, -15, -49,  64,   0, -15, -49,  64,   0
};
PRIM_ALIGN_128 static const BYTE rgbx_v_factors[] = {
	  64, -58,  -6,   0,  64, -58,  -6,   0,  64, -58,  -6,   0,  64, -58,  -6,   0
};
*/


/* compute the luma (Y) component from a single rgb source line */

static INLINE void ssse3_RGBToYUV420_BGRX_Y(
	const BYTE* src, BYTE* dst, UINT32 width)
{
	UINT32 x;
	__m128i y_factors, x0, x1, x2, x3;
	const __m128i* argb = (const __m128i*) src;
	__m128i* ydst = (__m128i*) dst;

	y_factors = _mm_load_si128((__m128i*)bgrx_y_factors);

	for (x = 0; x < width; x += 16)
	{
		/* store 16 rgba pixels in 4 128 bit registers */
		x0 = _mm_load_si128(argb++); // 1st 4 pixels
		x1 = _mm_load_si128(argb++); // 2nd 4 pixels
		x2 = _mm_load_si128(argb++); // 3rd 4 pixels
		x3 = _mm_load_si128(argb++); // 4th 4 pixels

		/* multiplications and subtotals */
		x0 = _mm_maddubs_epi16(x0, y_factors);
		x1 = _mm_maddubs_epi16(x1, y_factors);
		x2 = _mm_maddubs_epi16(x2, y_factors);
		x3 = _mm_maddubs_epi16(x3, y_factors);

		/* the total sums */
		x0 = _mm_hadd_epi16(x0, x1);
		x2 = _mm_hadd_epi16(x2, x3);

		/* shift the results */
		x0 = _mm_srli_epi16(x0, 7);
		x2 = _mm_srli_epi16(x2, 7);

		/* pack the 16 words into bytes */
		x0 = _mm_packus_epi16(x0, x2);

		/* save to y plane */
		_mm_storeu_si128(ydst++, x0);
	}
}

/* compute the chrominance (UV) components from two rgb source lines */

static INLINE void ssse3_RGBToYUV420_BGRX_UV(
	const BYTE* src1, const BYTE* src2,
	BYTE* dst1, BYTE* dst2, UINT32 width)
{
	UINT32 x;
	__m128i vector128, u_factors, v_factors, x0, x1, x2, x3, x4, x5;

	const __m128i* rgb1 = (const __m128i*)src1;
	const __m128i* rgb2 = (const __m128i*)src2;

	__m64* udst = (__m64*)dst1;
	__m64* vdst = (__m64*)dst2;

	vector128 = _mm_load_si128((__m128i*)const_buf_128b);
	u_factors = _mm_load_si128((__m128i*)bgrx_u_factors);
	v_factors = _mm_load_si128((__m128i*)bgrx_v_factors);

	for (x = 0; x < width; x += 16)
	{
		/* subsample 16x2 pixels into 16x1 pixels */
		x0 = _mm_load_si128(rgb1++);
		x4 = _mm_load_si128(rgb2++);
		x0 = _mm_avg_epu8(x0, x4);

		x1 = _mm_load_si128(rgb1++);
		x4 = _mm_load_si128(rgb2++);
		x1 = _mm_avg_epu8(x1, x4);

		x2 = _mm_load_si128(rgb1++);
		x4 = _mm_load_si128(rgb2++);
		x2 = _mm_avg_epu8(x2, x4);

		x3 = _mm_load_si128(rgb1++);
		x4 = _mm_load_si128(rgb2++);
		x3 = _mm_avg_epu8(x3, x4);

		// subsample these 16x1 pixels into 8x1 pixels */

		/**
		 * shuffle controls
		 * c = a[0],a[2],b[0],b[2] == 10 00 10 00 = 0x88
		 * c = a[1],a[3],b[1],b[3] == 11 01 11 01 = 0xdd
		 */

		x4 = _mm_castps_si128( _mm_shuffle_ps(_mm_castsi128_ps(x0), _mm_castsi128_ps(x1), 0x88) );
		x0 = _mm_castps_si128( _mm_shuffle_ps(_mm_castsi128_ps(x0), _mm_castsi128_ps(x1), 0xdd) );
		x0 = _mm_avg_epu8(x0, x4);

		x4 = _mm_castps_si128( _mm_shuffle_ps(_mm_castsi128_ps(x2), _mm_castsi128_ps(x3), 0x88) );
		x1 = _mm_castps_si128( _mm_shuffle_ps(_mm_castsi128_ps(x2), _mm_castsi128_ps(x3), 0xdd) );
		x1 = _mm_avg_epu8(x1, x4);

		/* multiplications and subtotals */
		x2 = _mm_maddubs_epi16(x0, u_factors);
		x3 = _mm_maddubs_epi16(x1, u_factors);

		x4 = _mm_maddubs_epi16(x0, v_factors);
		x5 = _mm_maddubs_epi16(x1, v_factors);

		/* the total sums */
		x0 = _mm_hadd_epi16(x2, x3);
		x1 = _mm_hadd_epi16(x4, x5);

		/* shift the results */
		x0 = _mm_srai_epi16(x0, 7);
		x1 = _mm_srai_epi16(x1, 7);

		/* pack the 16 words into bytes */
		x0 = _mm_packs_epi16(x0, x1);

		/* add 128 */
		x0 = _mm_add_epi8(x0, vector128);

		/* the lower 8 bytes go to the u plane */
		_mm_storel_pi(udst++, _mm_castsi128_ps(x0));

		/* the upper 8 bytes go to the v plane */
		_mm_storeh_pi(vdst++, _mm_castsi128_ps(x0));
	}
}

static pstatus_t ssse3_RGBToYUV420_BGRX(
	const BYTE* pSrc, UINT32 srcFormat, UINT32 srcStep,
	BYTE* pDst[3], UINT32 dstStep[3],
	const prim_size_t* roi)
{
	UINT32 y;
	const BYTE* argb = pSrc;
	BYTE* ydst = pDst[0];
	BYTE* udst = pDst[1];
	BYTE* vdst = pDst[2];

	if (roi->height < 1 || roi->width < 1)
	{
		return !PRIMITIVES_SUCCESS;
	}

	if (roi->width % 16 || (unsigned long)pSrc % 16 || srcStep % 16)
	{
		return generic->RGBToYUV420_8u_P3AC4R(pSrc, srcFormat, srcStep, pDst, dstStep, roi);
	}

	for (y = 0; y < roi->height-1; y+=2)
	{
		const BYTE* line1 = argb;
		const BYTE* line2 = argb + srcStep;

		ssse3_RGBToYUV420_BGRX_UV(line1, line2, udst, vdst, roi->width);
		ssse3_RGBToYUV420_BGRX_Y(line1, ydst, roi->width);
		ssse3_RGBToYUV420_BGRX_Y(line2, ydst + dstStep[0], roi->width);

		argb += 2 * srcStep;
		ydst += 2 * dstStep[0];
		udst += 1 * dstStep[1];
		vdst += 1 * dstStep[2];
	}

	if (roi->height & 1)
	{
		/* pass the same last line of an odd height twice for UV */
		ssse3_RGBToYUV420_BGRX_UV(argb, argb, udst, vdst, roi->width);
		ssse3_RGBToYUV420_BGRX_Y(argb, ydst, roi->width);
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t ssse3_RGBToYUV420(
	const BYTE* pSrc, UINT32 srcFormat, UINT32 srcStep,
	BYTE* pDst[3], UINT32 dstStep[3],
	const prim_size_t* roi)
{
	switch (srcFormat)
	{
		case PIXEL_FORMAT_BGRX32:
		case PIXEL_FORMAT_BGRA32:
			return ssse3_RGBToYUV420_BGRX(pSrc, srcFormat, srcStep, pDst, dstStep, roi);
		default:
			return generic->RGBToYUV420_8u_P3AC4R(pSrc, srcFormat, srcStep, pDst, dstStep, roi);
	}
}


#endif

void primitives_init_YUV_opt(primitives_t* prims)
{
	generic = primitives_get_generic();
	primitives_init_YUV(prims);
#ifdef WITH_SSE2

	if (IsProcessorFeaturePresentEx(PF_EX_SSSE3)
	    && IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE))
	{
		prims->RGBToYUV420_8u_P3AC4R = ssse3_RGBToYUV420;
		prims->YUV420ToRGB_8u_P3AC4R = ssse3_YUV420ToRGB;
	}

#endif
}
