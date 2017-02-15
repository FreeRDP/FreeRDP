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

#ifdef WITH_SSE2

#include <emmintrin.h>
#include <tmmintrin.h>
#elif defined(WITH_NEON)
#include <arm_neon.h>
#endif /* WITH_SSE2 else WITH_NEON */

static primitives_t* generic = NULL;

#ifdef WITH_SSE2
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

static pstatus_t ssse3_YUV444ToRGB_8u_P3AC4R_BGRX(
    const BYTE** pSrc, const UINT32* srcStep,
    BYTE* pDst, UINT32 dstStep,
    const prim_size_t* roi)
{
	const UINT32 nWidth = roi->width;
	const UINT32 nHeight = roi->height;
	const __m128i c128 = _mm_set1_epi16(128);
	const __m128i mapY = _mm_set_epi32(0x80800380, 0x80800280, 0x80800180, 0x80800080);
	const __m128i map = _mm_set_epi32(0x80038002, 0x80018000, 0x80808080, 0x80808080);
	UINT32 y;

	for (y = 0; y < nHeight; y++)
	{
		UINT32 x;
		__m128i* dst = (__m128i*)(pDst + dstStep * y);
		const BYTE* YData = pSrc[0] + y * srcStep[0];
		const BYTE* UData = pSrc[1] + y * srcStep[1];
		const BYTE* VData = pSrc[2] + y * srcStep[2];

		for (x = 0; x < nWidth; x += 4)
		{
			__m128i BGRX = _mm_set_epi32(0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000);
			{
				__m128i C, D, E;
				/* Load Y values and expand to 32 bit */
				{
					const __m128i Yraw = _mm_loadu_si128((__m128i*)YData);
					C = _mm_shuffle_epi8(Yraw, mapY); /* Reorder and multiply by 256 */
				}
				/* Load U values and expand to 32 bit */
				{
					const __m128i Uraw = _mm_loadu_si128((__m128i*)UData);
					const __m128i U = _mm_shuffle_epi8(Uraw, map); /* Reorder dcba */
					D = _mm_sub_epi16(U, c128); /* D = U - 128 */
				}
				/* Load V values and expand to 32 bit */
				{
					const __m128i Vraw = _mm_loadu_si128((__m128i*)VData);
					const __m128i V = _mm_shuffle_epi8(Vraw, map); /* Reorder dcba */
					E = _mm_sub_epi16(V, c128); /* E = V - 128 */
				}
				/* Get the R value */
				{
					const __m128i c403 = _mm_set1_epi16(403);
					const __m128i e403 = _mm_unpackhi_epi16(_mm_mullo_epi16(E, c403), _mm_mulhi_epi16(E, c403));
					const __m128i Rs = _mm_add_epi32(C, e403);
					const __m128i R32 = _mm_srai_epi32(Rs, 8);
					const __m128i R16 = _mm_packs_epi32(R32, _mm_setzero_si128());
					const __m128i R = _mm_packus_epi16(R16, _mm_setzero_si128());
					const __m128i mask = _mm_set_epi32(0x80038080, 0x80028080, 0x80018080, 0x80008080);
					const __m128i packed = _mm_shuffle_epi8(R, mask);
					BGRX = _mm_or_si128(BGRX, packed);
				}
				/* Get the G value */
				{
					const __m128i c48 = _mm_set1_epi16(48);
					const __m128i d48 = _mm_unpackhi_epi16(_mm_mullo_epi16(D, c48), _mm_mulhi_epi16(D, c48));
					const __m128i c120 = _mm_set1_epi16(120);
					const __m128i e120 = _mm_unpackhi_epi16(_mm_mullo_epi16(E, c120), _mm_mulhi_epi16(E, c120));
					const __m128i de = _mm_add_epi32(d48, e120);
					const __m128i Gs = _mm_sub_epi32(C, de);
					const __m128i G32 = _mm_srai_epi32(Gs, 8);
					const __m128i G16 = _mm_packs_epi32(G32, _mm_setzero_si128());
					const __m128i G = _mm_packus_epi16(G16, _mm_setzero_si128());
					const __m128i mask = _mm_set_epi32(0x80800380, 0x80800280, 0x80800180, 0x80800080);
					const __m128i packed = _mm_shuffle_epi8(G, mask);
					BGRX = _mm_or_si128(BGRX, packed);
				}
				/* Get the B value */
				{
					const __m128i c475 = _mm_set1_epi16(475);
					const __m128i d475 = _mm_unpackhi_epi16(_mm_mullo_epi16(D, c475), _mm_mulhi_epi16(D, c475));
					const __m128i Bs = _mm_add_epi32(C, d475);
					const __m128i B32 = _mm_srai_epi32(Bs, 8);
					const __m128i B16 = _mm_packs_epi32(B32, _mm_setzero_si128());
					const __m128i B = _mm_packus_epi16(B16, _mm_setzero_si128());
					const __m128i mask = _mm_set_epi32(0x80808003, 0x80808002, 0x80808001, 0x80808000);
					const __m128i packed = _mm_shuffle_epi8(B, mask);
					BGRX = _mm_or_si128(BGRX, packed);
				}
			}
			_mm_storeu_si128(dst++, BGRX);
			YData += 4;
			UData += 4;
			VData += 4;
		}
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t ssse3_YUV444ToRGB_8u_P3AC4R(const BYTE** pSrc, const UINT32* srcStep,
        BYTE* pDst, UINT32 dstStep, UINT32 DstFormat,
        const prim_size_t* roi)
{
	if (roi->width % 4 != 0)
		return generic->YUV444ToRGB_8u_P3AC4R(pSrc, srcStep, pDst, dstStep, DstFormat, roi);

	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRX32:
		case PIXEL_FORMAT_BGRA32:
			return ssse3_YUV444ToRGB_8u_P3AC4R_BGRX(pSrc, srcStep, pDst, dstStep, roi);

		default:
			return generic->YUV444ToRGB_8u_P3AC4R(pSrc, srcStep, pDst, dstStep, DstFormat, roi);
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

PRIM_ALIGN_128 static const BYTE bgrx_y_factors[] =
{
	9,  92,  27,   0,   9,  92,  27,   0,   9,  92,  27,   0,   9,  92,  27,   0
};
PRIM_ALIGN_128 static const BYTE bgrx_u_factors[] =
{
	64, -49, -15,   0,  64, -49, -15,   0,  64, -49, -15,   0,  64, -49, -15,   0
};
PRIM_ALIGN_128 static const BYTE bgrx_v_factors[] =
{
	-6, -58,  64,   0,  -6, -58,  64,   0,  -6, -58,  64,   0,  -6, -58,  64,   0
};

PRIM_ALIGN_128 static const BYTE const_buf_128b[] =
{
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
		x4 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(x0), _mm_castsi128_ps(x1), 0x88));
		x0 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(x0), _mm_castsi128_ps(x1), 0xdd));
		x0 = _mm_avg_epu8(x0, x4);
		x4 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(x2), _mm_castsi128_ps(x3), 0x88));
		x1 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(x2), _mm_castsi128_ps(x3), 0xdd));
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

	for (y = 0; y < roi->height - 1; y += 2)
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

#elif defined(WITH_NEON)

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

static INLINE pstatus_t neon_YUV420ToX(
    const BYTE* pSrc[3], const UINT32 srcStep[3],
    BYTE* pDst, UINT32 dstStep,
    const prim_size_t* roi, const uint8_t rPos,  const uint8_t gPos,
    const uint8_t bPos, const uint8_t aPos)
{
	UINT32 y;
	const UINT32 nWidth = roi->width;
	const UINT32 nHeight = roi->height;
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

		for (x = 0; x < nWidth;)
		{
			const BOOL lastX = (nWidth - x) < 16;
			const uint8x8_t Uraw = vld1_u8(pU);
			const uint8x8x2_t Uu = vzip_u8(Uraw, Uraw);
			const int16x8_t U1 = vreinterpretq_s16_u16(vmovl_u8(Uu.val[0]));
			const int16x8_t U2 = vreinterpretq_s16_u16(vmovl_u8(Uu.val[1]));
			const uint8x8_t Vraw = vld1_u8(pV);
			const uint8x8x2_t Vu = vzip_u8(Vraw, Vraw);
			const int16x8_t V1 = vreinterpretq_s16_u16(vmovl_u8(Vu.val[0]));
			const int16x8_t V2 = vreinterpretq_s16_u16(vmovl_u8(Vu.val[1]));
			const int16x8_t D1 = vsubq_s16(U1, c128);
			const int16x4_t D1h = vget_high_s16(D1);
			const int16x4_t D1l = vget_low_s16(D1);
			const int16x8_t E1 = vsubq_s16(V1, c128);
			const int16x4_t E1h = vget_high_s16(E1);
			const int16x4_t E1l = vget_low_s16(E1);
			const int16x8_t D2 = vsubq_s16(U2, c128);
			const int16x4_t D2h = vget_high_s16(D2);
			const int16x4_t D2l = vget_low_s16(D2);
			const int16x8_t E2 = vsubq_s16(V2, c128);
			const int16x4_t E2h = vget_high_s16(E2);
			const int16x4_t E2l = vget_low_s16(E2);
			uint8x8x4_t bgrx;
			bgrx.val[aPos] = vdup_n_u8(0xFF);
			{
				const uint8x8_t Y1u = vld1_u8(pY1);
				const int16x8_t Y1 = vreinterpretq_s16_u16(vmovl_u8(Y1u));
				const int32x4_t Ch = vmulq_n_s32(vmovl_s16(vget_high_s16(Y1)), 256);  /* Y * 256 */
				const int32x4_t Cl = vmulq_n_s32(vmovl_s16(vget_low_s16(Y1)), 256);  /* Y * 256 */
				bgrx.val[rPos] = neon_YUV2R(Ch, Cl, D1h, D1l, E1h, E1l);
				bgrx.val[gPos] = neon_YUV2G(Ch, Cl, D1h, D1l, E1h, E1l);
				bgrx.val[bPos] = neon_YUV2B(Ch, Cl, D1h, D1l, E1h, E1l);
				vst4_u8(pRGB1, bgrx);
				pRGB1 += 32;
				pY1 += 8;
				x += 8;
			}

			if (!lastX)
			{
				const uint8x8_t Y1u = vld1_u8(pY1);
				const int16x8_t Y1 = vreinterpretq_s16_u16(vmovl_u8(Y1u));
				const int32x4_t Ch = vmulq_n_s32(vmovl_s16(vget_high_s16(Y1)), 256);  /* Y * 256 */
				const int32x4_t Cl = vmulq_n_s32(vmovl_s16(vget_low_s16(Y1)), 256);  /* Y * 256 */
				bgrx.val[rPos] = neon_YUV2R(Ch, Cl, D2h, D2l, E2h, E2l);
				bgrx.val[gPos] = neon_YUV2G(Ch, Cl, D2h, D2l, E2h, E2l);
				bgrx.val[bPos] = neon_YUV2B(Ch, Cl, D2h, D2l, E2h, E2l);
				vst4_u8(pRGB1, bgrx);
				pRGB1 += 32;
				pY1 += 8;
				x += 8;
			}

			if (!lastY)
			{
				const uint8x8_t Y2u = vld1_u8(pY2);
				const int16x8_t Y2 = vreinterpretq_s16_u16(vmovl_u8(Y2u));
				const int32x4_t Ch = vmulq_n_s32(vmovl_s16(vget_high_s16(Y2)), 256);  /* Y * 256 */
				const int32x4_t Cl = vmulq_n_s32(vmovl_s16(vget_low_s16(Y2)), 256);  /* Y * 256 */
				bgrx.val[rPos] = neon_YUV2R(Ch, Cl, D1h, D1l, E1h, E1l);
				bgrx.val[gPos] = neon_YUV2G(Ch, Cl, D1h, D1l, E1h, E1l);
				bgrx.val[bPos] = neon_YUV2B(Ch, Cl, D1h, D1l, E1h, E1l);
				vst4_u8(pRGB2, bgrx);
				pRGB2 += 32;
				pY2 += 8;

				if (!lastX)
				{
					const uint8x8_t Y2u = vld1_u8(pY2);
					const int16x8_t Y2 = vreinterpretq_s16_u16(vmovl_u8(Y2u));
					const int32x4_t Ch = vmulq_n_s32(vmovl_s16(vget_high_s16(Y2)), 256);  /* Y * 256 */
					const int32x4_t Cl = vmulq_n_s32(vmovl_s16(vget_low_s16(Y2)), 256);  /* Y * 256 */
					bgrx.val[rPos] = neon_YUV2R(Ch, Cl, D2h, D2l, E2h, E2l);
					bgrx.val[gPos] = neon_YUV2G(Ch, Cl, D2h, D2l, E2h, E2l);
					bgrx.val[bPos] = neon_YUV2B(Ch, Cl, D2h, D2l, E2h, E2l);
					vst4_u8(pRGB2, bgrx);
					pRGB2 += 32;
					pY2 += 8;
				}
			}

			pU += 8;
			pV += 8;
		}
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
	const int16x4_t c48 = vdup_n_s16(48);
	const int16x4_t c120 = vdup_n_s16(120);
	const int16x4_t c403 = vdup_n_s16(403);
	const int16x4_t c475 = vdup_n_s16(475);
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
			const int32x4_t Ch = vmulq_n_s32(vmovl_s16(vget_high_s16(Y)), 256);  /* Y * 256 */
			const int32x4_t Cl = vmulq_n_s32(vmovl_s16(vget_low_s16(Y)), 256);  /* Y * 256 */
			const int16x8_t D = vsubq_s16(U, c128);
			const int16x4_t Dh = vget_high_s16(D);
			const int16x4_t Dl = vget_low_s16(D);
			const int16x8_t E = vsubq_s16(V, c128);
			const int16x4_t Eh = vget_high_s16(E);
			const int16x4_t El = vget_low_s16(E);
			uint8x8x4_t bgrx;
			{
				/* B = (256L * Y + 475 * (U - 128)) >> 8*/
				const int32x4_t CDh = vmlal_s16(Ch, Dh, c475);
				const int32x4_t CDl = vmlal_s16(Cl, Dl, c475);
				const int32x4_t Bh = vrshrq_n_s32(CDh, 8);
				const int32x4_t Bl = vrshrq_n_s32(CDl, 8);
				const int16x8_t B = vcombine_s16(vqmovn_s32(Bl), vqmovn_s32(Bh));
				bgrx.val[bPos] = vqmovun_s16(B);
			}
			{
				/* G = (256L * Y -  48 * (U - 128) - 120 * (V - 128)) >> 8 */
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

static pstatus_t neon_YUV420CombineToYUV444(
    const BYTE* pMainSrc[3], const UINT32 srcMainStep[3],
    const BYTE* pAuxSrc[3], const UINT32 srcAuxStep[3],
    BYTE* pDst[3], const UINT32 dstStep[3],
    const prim_size_t* roi)
{
	UINT32 x, y;
	const UINT32 nWidth = roi->width;
	const UINT32 nHeight = roi->height;
	const UINT32 halfWidth = nWidth / 2, halfHeight = nHeight / 2;
	const UINT32 oddY = 1;
	const UINT32 evenY = 0;
	/* The auxilary frame is aligned to multiples of 16x16.
	 * We need the padded height for B4 and B5 conversion. */
	const UINT32 padHeigth = roi->height + 16 - roi->height % 16;

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
			const BYTE* Um = pMainSrc[1] + srcMainStep[1] * y;
			const BYTE* Vm = pMainSrc[2] + srcMainStep[2] * y;
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
	}

	if (!pAuxSrc || !pAuxSrc[0] || !pAuxSrc[1] || !pAuxSrc[2])
		return PRIMITIVES_SUCCESS;

	/* The second half of U and V is a bit more tricky... */
	/* B4 and B5 */
	for (y = 0; y < padHeigth; y += 16)
	{
		const BYTE* Ya = pAuxSrc[0] + srcAuxStep[0] * y;
		UINT32 x;
		BYTE* pU = pDst[1] + dstStep[1] * (y + 1);
		BYTE* pV = pDst[2] + dstStep[2] * (y + 1);

		for (x = 0; x < 8; x++)
		{
			if (y + x >= nHeight)
				continue;

			memcpy(pU, Ya, nWidth);
			pU += dstStep[1] * 2;
			Ya += srcAuxStep[0];
		}

		for (x = 0; x < 8; x++)
		{
			if (y + x >= nHeight)
				continue;

			memcpy(pV, Ya, nWidth);
			pV += dstStep[1] * 2;
			Ya += srcAuxStep[0];
		}
	}

	/* B6 and B7 */
	for (y = 0; y < halfHeight; y++)
	{
		const UINT32 val2y = (y * 2 + evenY);
		const BYTE* Ua = pAuxSrc[1] + srcAuxStep[1] * y;
		const BYTE* Va = pAuxSrc[2] + srcAuxStep[2] * y;
		BYTE* pU = pDst[1] + dstStep[1] * val2y;
		BYTE* pV = pDst[2] + dstStep[2] * val2y;

		for (x = 0; x + 16 < halfWidth; x += 16)
		{
			{
				const uint8x16_t u = vld1q_u8(Ua);
				uint8x16x2_t uu = vld2q_u8(pU);
				uu.val[1] = u;
				vst2q_u8(pU, uu);
				Ua += 16;
				pU += 32;
			}
			{
				const uint8x16_t v = vld1q_u8(Va);
				uint8x16x2_t vv = vld2q_u8(pV);
				vv.val[1] = v;
				vst2q_u8(pV, vv);
				Va += 16;
				pV += 32;
			}
		}

		for (; x < halfWidth; x++)
		{
			pU++;
			*pU++ = *Ua++;
			pV++;
			*pV++ = *Va++;
		}
	}

	/* Filter */
	for (y = 0; y < halfHeight; y++)
	{
		const UINT32 val2y = (y * 2 + evenY);
		const UINT32 val2y1 = val2y + oddY;
		BYTE* pU = pDst[1] + dstStep[1] * val2y;
		BYTE* pV = pDst[2] + dstStep[2] * val2y;
		BYTE* pU1 = pU + dstStep[1];
		BYTE* pV1 = pV + dstStep[2];

		if (val2y1 > nHeight)
			continue;

		for (x = 0; x + 16 < halfWidth; x += 16)
		{
			{
				/* U = (U2x,2y << 2) - U2x1,2y - U2x,2y1 - U2x1,2y1 */
				uint8x8x2_t u = vld2_u8(pU);
				const int16x8_t up = vreinterpretq_s16_u16(vshll_n_u8(u.val[0], 2)); /* Ux2,2y << 2 */
				const uint8x8x2_t u1 = vld2_u8(pU1);
				const uint16x8_t usub = vaddl_u8(u1.val[1], u1.val[0]); /* U2x,2y1 + U2x1,2y1 */
				const int16x8_t us = vreinterpretq_s16_u16(vaddw_u8(usub,
				                     u.val[1])); /* U2x1,2y + U2x,2y1 + U2x1,2y1 */
				const int16x8_t un = vsubq_s16(up, us);
				const uint8x8_t u8 = vqmovun_s16(un); /* CLIP(un) */
				u.val[0] = u8;
				vst2_u8(pU, u);
				pU += 16;
				pU1 += 16;
			}
			{
				/* V = (V2x,2y << 2) - V2x1,2y - V2x,2y1 - V2x1,2y1 */
				uint8x8x2_t v = vld2_u8(pV);
				const int16x8_t vp = vreinterpretq_s16_u16(vshll_n_u8(v.val[0], 2)); /* Vx2,2y << 2 */
				const uint8x8x2_t v1 = vld2_u8(pV1);
				const uint16x8_t vsub = vaddl_u8(v1.val[1], v1.val[0]); /* V2x,2y1 + V2x1,2y1 */
				const int16x8_t vs = vreinterpretq_s16_u16(vaddw_u8(vsub,
				                     v.val[1])); /* V2x1,2y + V2x,2y1 + V2x1,2y1 */
				const int16x8_t vn = vsubq_s16(vp, vs);
				const uint8x8_t v8 = vqmovun_s16(vn); /* CLIP(vn) */
				v.val[0] = v8;
				vst2_u8(pV, v);
				pV += 16;
				pV1 += 16;
			}
		}

		for (; x < halfWidth; x++)
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
			*pU = CLIP(u2020);
			*pV = CLIP(v2020);
			pU += 2;
			pV += 2;
		}
	}

	return PRIMITIVES_SUCCESS;
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
		prims->YUV444ToRGB_8u_P3AC4R = ssse3_YUV444ToRGB_8u_P3AC4R;
	}

#elif defined(WITH_NEON)

	if (IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE))
	{
		prims->YUV420ToRGB_8u_P3AC4R = neon_YUV420ToRGB_8u_P3AC4R;
		prims->YUV444ToRGB_8u_P3AC4R = neon_YUV444ToRGB_8u_P3AC4R;
		prims->YUV420CombineToYUV444 = neon_YUV420CombineToYUV444;
	}

#endif
}
