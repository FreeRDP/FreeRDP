/** function for converting YUV420p data to the RGB format (but without any special upconverting)
 * It's completely written in nasm-x86-assembly for intel processors supporting SSSE3 and higher.
 * The target dstStep (6th parameter) must be a multiple of 16.
 * srcStep[0] must be (target dstStep) / 4 or bigger and srcStep[1] the next multiple of four
 * of the half of srcStep[0] or bigger
 */

#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/sysinfo.h>
#include <winpr/crt.h>
#include <freerdp/types.h>
#include <freerdp/primitives.h>


#ifdef WITH_SSE2

#include <emmintrin.h>
#include <tmmintrin.h>

pstatus_t ssse3_YUV420ToRGB_8u_P3AC4R(const BYTE **pSrc, const UINT32 *srcStep,
		BYTE *pDst, UINT32 dstStep, const prim_size_t *roi)
{
	UINT32 lastRow, lastCol;
	BYTE *UData,*VData,*YData;
	UINT32 i,nWidth,nHeight,VaddDst,VaddY,VaddU,VaddV;
	__m128i r0,r1,r2,r3,r4,r5,r6,r7;
	__m128i *buffer;

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
				r7 = _mm_set_epi32(0,0,0,0xFFFFFFFF);
				break;

			case 2:
				r7 = _mm_set_epi32(0,0,0xFFFFFFFF,0xFFFFFFFF);
				break;

			case 3:
				r7 = _mm_set_epi32(0,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF);
				break;
		}

		_mm_store_si128(buffer+3,r7);
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
				r0 = _mm_cvtsi32_si128(*(UINT32 *)UData);
				r5 = _mm_set_epi32(0x80038003,0x80028002,0x80018001,0x80008000);
				r0 = _mm_shuffle_epi8(r0,r5);

				UData += 4;

			/* then we subtract 128 from each value, so we get D */
				r3 = _mm_set_epi16(128,128,128,128,128,128,128,128);
				r0 = _mm_subs_epi16(r0,r3);

			/* we need to do two things with our D, so let's store it for later use */
				r2 = r0;

			/* now we can multiply our D with 48 and unpack it to xmm4:xmm0
			 * this is what we need to get G data later on */
				r4 = r0;
				r7 = _mm_set_epi16(48,48,48,48,48,48,48,48);
				r0 = _mm_mullo_epi16(r0,r7);
				r4 = _mm_mulhi_epi16(r4,r7);
				r7 = r0;
				r0 = _mm_unpacklo_epi16(r0,r4);
				r4 = _mm_unpackhi_epi16(r7,r4);

			/* to get B data, we need to prepare a second value, D*475 */
				r1 = r2;
				r7 = _mm_set_epi16(475,475,475,475,475,475,475,475);
				r1 = _mm_mullo_epi16(r1,r7);
				r2 = _mm_mulhi_epi16(r2,r7);
				r7 = r1;
				r1 = _mm_unpacklo_epi16(r1,r2);
				r7 = _mm_unpackhi_epi16(r7,r2);

			/* so we got something like this: xmm7:xmm1
			 * this pair contains values for 16 pixel:
			 * aabbccdd
			 * aabbccdd, but we can only work on four pixel at once, so we need to save upper values */
				_mm_store_si128(buffer+1,r7);

			/* Now we've prepared U-data. Preparing V-data is actually the same, just with other coefficients */
				r2 = _mm_cvtsi32_si128(*(UINT32 *)VData);
				r2 = _mm_shuffle_epi8(r2,r5);

				VData += 4;

				r2 = _mm_subs_epi16(r2,r3);

				r5 = r2;

			/* this is also known as E*403, we need it to convert R data */
				r3 = r2;
				r7 = _mm_set_epi16(403,403,403,403,403,403,403,403);
				r2 = _mm_mullo_epi16(r2,r7);
				r3 = _mm_mulhi_epi16(r3,r7);
				r7 = r2;
				r2 = _mm_unpacklo_epi16(r2,r3);
				r7 = _mm_unpackhi_epi16(r7,r3);

			/* and preserve upper four values for future ... */
				_mm_store_si128(buffer+2,r7);

			/* doing this step: E*120 */
				r3 = r5;
				r7 = _mm_set_epi16(120,120,120,120,120,120,120,120);
				r3 = _mm_mullo_epi16(r3,r7);
				r5 = _mm_mulhi_epi16(r5,r7);
				r7 = r3;
				r3 = _mm_unpacklo_epi16(r3,r5);
				r7 = _mm_unpackhi_epi16(r7,r5);

			/* now we complete what we've begun above:
			 * (48*D) + (120*E) = (48*D +120*E) */
				r0 = _mm_add_epi32(r0,r3);
				r4 = _mm_add_epi32(r4,r7);

			/* and store to memory ! */
				_mm_store_si128(buffer,r4);
			}
			else
			{
			/* maybe you've wondered about the conditional above ?
			 * Well, we prepared UV data for eight pixel in each line, but can only process four
			 * per loop. So we need to load the upper four pixel data from memory each secound loop! */
				r1 = _mm_load_si128(buffer+1);
				r2 = _mm_load_si128(buffer+2);
				r0 = _mm_load_si128(buffer);
			}

			if (++i == nWidth)
				lastCol <<= 1;

		/* We didn't produce any output yet, so let's do so!
		 * Ok, fetch four pixel from the Y-data array and shuffle them like this:
		 * 00d0 00c0 00b0 00a0, to get signed dwords and multiply by 256 */
			r4 = _mm_cvtsi32_si128(*(UINT32 *)YData);
			r7 = _mm_set_epi32(0x80800380,0x80800280,0x80800180,0x80800080);
			r4 = _mm_shuffle_epi8(r4,r7);

			r5 = r4;
			r6 = r4;

		/* no we can perform the "real" conversion itself and produce output! */
			r4 = _mm_add_epi32(r4,r2);
			r5 = _mm_sub_epi32(r5,r0);
			r6 = _mm_add_epi32(r6,r1);

		/* in the end, we only need bytes for RGB values.
		 * So, what do we do? right! shifting left makes values bigger and thats always good.
		 * before we had dwords of data, and by shifting left and treating the result
		 * as packed words, we get not only signed words, but do also divide by 256
		 * imagine, data is now ordered this way: ddx0 ccx0 bbx0 aax0, and x is the least
		 * significant byte, that we don't need anymore, because we've done some rounding */
			r4 = _mm_slli_epi32(r4,8);
			r5 = _mm_slli_epi32(r5,8);
			r6 = _mm_slli_epi32(r6,8);

		/* one thing we still have to face is the clip() function ...
		 * we have still signed words, and there are those min/max instructions in SSE2 ...
		 * the max instruction takes always the bigger of the two operands and stores it in the first one,
		 * and it operates with signs !
		 * if we feed it with our values and zeros, it takes the zeros if our values are smaller than
		 * zero and otherwise our values */
			r7 = _mm_set_epi32(0,0,0,0);
			r4 = _mm_max_epi16(r4,r7);
			r5 = _mm_max_epi16(r5,r7);
			r6 = _mm_max_epi16(r6,r7);

		/* the same thing just completely different can be used to limit our values to 255,
		 * but now using the min instruction and 255s */
			r7 = _mm_set_epi32(0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000);
			r4 = _mm_min_epi16(r4,r7);
			r5 = _mm_min_epi16(r5,r7);
			r6 = _mm_min_epi16(r6,r7);

		/* Now we got our bytes.
		 * the moment has come to assemble the three channels R,G and B to the xrgb dwords
		 * on Red channel we just have to and each futural dword with 00FF0000H */
			//r7=_mm_set_epi32(0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000);
			r4 = _mm_and_si128(r4,r7);

		/* on Green channel we have to shuffle somehow, so we get something like this:
		 * 00d0 00c0 00b0 00a0 */
			r7 = _mm_set_epi32(0x80800E80,0x80800A80,0x80800680,0x80800280);
			r5 = _mm_shuffle_epi8(r5,r7);

		/* and on Blue channel that one:
		 * 000d 000c 000b 000a */
			r7 = _mm_set_epi32(0x8080800E,0x8080800A,0x80808006,0x80808002);
			r6 = _mm_shuffle_epi8(r6,r7);

		/* and at last we or it together and get this one:
		 * xrgb xrgb xrgb xrgb */
			r4 = _mm_or_si128(r4,r5);
			r4 = _mm_or_si128(r4,r6);

		/* Only thing to do know is writing data to memory, but this gets a bit more
		 * complicated if the width is not a multiple of four and it is the last column in line. */
			if (lastCol & 0x02)
			{
			/* let's say, we need to only convert six pixel in width
			 * Ok, the first 4 pixel will be converted just like every 4 pixel else, but
			 * if it's the last loop in line, last_column is shifted left by one (curious? have a look above),
			 * and we land here. Through initialisation a mask was prepared. In this case it looks like
			 * 0000FFFFH 0000FFFFH 0000FFFFH 0000FFFFH */
				r6 = _mm_load_si128(buffer+3);
			/* we and our output data with this mask to get only the valid pixel */
				r4 = _mm_and_si128(r4,r6);
			/* then we fetch memory from the destination array ... */
				r5 = _mm_lddqu_si128((__m128i *)pDst);
			/* ... and and it with the inverse mask. We get only those pixel, which should not be updated */
				r6 = _mm_andnot_si128(r6,r5);
			/* we only have to or the two values together and write it back to the destination array,
			 * and only the pixel that should be updated really get changed. */
				r4 = _mm_or_si128(r4,r6);
			}
			_mm_storeu_si128((__m128i *)pDst,r4);

			if (!(lastRow & 0x02))
			{
			/* Because UV data is the same for two lines, we can process the secound line just here,
			 * in the same loop. Only thing we need to do is to add some offsets to the Y- and destination
			 * pointer. These offsets are iStride[0] and the target scanline.
			 * But if we don't need to process the secound line, like if we are in the last line of processing nine lines,
			 * we just skip all this. */
				r4 = _mm_cvtsi32_si128(*(UINT32 *)(YData+srcStep[0]));
				r7 = _mm_set_epi32(0x80800380,0x80800280,0x80800180,0x80800080);
				r4 = _mm_shuffle_epi8(r4,r7);

				r5 = r4;
				r6 = r4;

				r4 = _mm_add_epi32(r4,r2);
				r5 = _mm_sub_epi32(r5,r0);
				r6 = _mm_add_epi32(r6,r1);

				r4 = _mm_slli_epi32(r4,8);
				r5 = _mm_slli_epi32(r5,8);
				r6 = _mm_slli_epi32(r6,8);

				r7 = _mm_set_epi32(0,0,0,0);
				r4 = _mm_max_epi16(r4,r7);
				r5 = _mm_max_epi16(r5,r7);
				r6 = _mm_max_epi16(r6,r7);

				r7 = _mm_set_epi32(0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000);
				r4 = _mm_min_epi16(r4,r7);
				r5 = _mm_min_epi16(r5,r7);
				r6 = _mm_min_epi16(r6,r7);

				r7 = _mm_set_epi32(0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000);
				r4 = _mm_and_si128(r4,r7);

				r7 = _mm_set_epi32(0x80800E80,0x80800A80,0x80800680,0x80800280);
				r5 = _mm_shuffle_epi8(r5,r7);

				r7 = _mm_set_epi32(0x8080800E,0x8080800A,0x80808006,0x80808002);
				r6 = _mm_shuffle_epi8(r6,r7);

				r4 = _mm_or_si128(r4,r5);
				r4 = _mm_or_si128(r4,r6);

				if (lastCol & 0x02)
				{
					r6 = _mm_load_si128(buffer+3);
					r4 = _mm_and_si128(r4,r6);
					r5 = _mm_lddqu_si128((__m128i *)(pDst+dstStep));
					r6 = _mm_andnot_si128(r6,r5);
					r4 = _mm_or_si128(r4,r6);

				/* only thing is, we should shift [rbp-42] back here, because we have processed the last column,
				 * and this "special condition" can be released */
					lastCol >>= 1;
				}
				_mm_storeu_si128((__m128i *)(pDst+dstStep),r4);
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
#endif

void primitives_init_YUV_opt(primitives_t *prims)
{
#ifdef WITH_SSE2
	if (IsProcessorFeaturePresentEx(PF_EX_SSSE3) && IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE))
	{
		prims->YUV420ToRGB_8u_P3AC4R = ssse3_YUV420ToRGB_8u_P3AC4R;
	}
#endif
}
