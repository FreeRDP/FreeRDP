/* FreeRDP: A Remote Desktop Protocol Client
 * Optimized Color conversion operations.
 * vi:ts=4 sw=4:
 *
 * Copyright 2011 Stephen Erisman
 * Copyright 2011 Norbert Federa <norbert.federa@thincast.com>
 * Copyright 2011 Martin Fleisz <martin.fleisz@thincast.com>
 * (c) Copyright 2012 Hewlett-Packard Development Company, L.P.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0.
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <freerdp/config.h>

#include <freerdp/types.h>
#include <freerdp/primitives.h>
#include <winpr/sysinfo.h>

#include "prim_colors.h"

#include "prim_internal.h"
#include "prim_templates.h"

#if defined(SSE_AVX_INTRINSICS_ENABLED)
#include <emmintrin.h>

static primitives_t* generic = NULL;

#define CACHE_LINE_BYTES 64

/*  1.403 << 14 */
/* -0.344 << 14 */
/* -0.714 << 14 */
/*  1.770 << 14 */

static const int32_t ycbcr_table[][4] = { { 1, 0, -1, 2 },
	                                      { 3, -1, -1, 4 },
	                                      { 6, -1, -3, 7 },
	                                      { 11, -3, -6, 14 },
	                                      { 22, -6, -11, 28 },
	                                      { 45, -11, -23, 57 },
	                                      { 90, -22, -46, 113 },
	                                      { 180, -44, -91, 227 },
	                                      { 359, -88, -183, 453 },
	                                      { 718, -176, -366, 906 },
	                                      { 1437, -352, -731, 1812 },
	                                      { 2873, -705, -1462, 3625 },
	                                      { 5747, -1409, -2925, 7250 },
	                                      { 11493, -2818, -5849, 14500 },
	                                      { 22987, -5636, -11698, 29000 },
	                                      { 45974, -11272, -23396, 57999 },
	                                      { 91947, -22544, -46793, 115999 },
	                                      { 183894, -45089, -93585, 231997 },
	                                      { 367788, -90178, -187171, 463995 },
	                                      { 735576, -180355, -374342, 927990 },
	                                      { 1471152, -360710, -748683, 1855980 },
	                                      { 2942304, -721420, -1497367, 3711959 },
	                                      { 5884609, -1442841, -2994733, 7423918 },
	                                      { 11769217, -2885681, -5989466, 14847836 },
	                                      { 23538434, -5771362, -11978932, 29695672 },
	                                      { 47076868, -11542725, -23957864, 59391345 },
	                                      { 94153736, -23085449, -47915729, 118782689 },
	                                      { 188307472, -46170898, -95831458, 237565379 },
	                                      { 376614945, -92341797, -191662916, 475130757 },
	                                      { 753229890, -184683594, -383325831, 950261514 },
	                                      { 1506459779, -369367187, -766651662, 1900523028 } };

static inline __m128i mm_between_epi16_int(__m128i val, __m128i min, __m128i max)
{
	return _mm_min_epi16(max, _mm_max_epi16(val, min));
}

#define mm_between_epi16(_val, _min, _max) (_val) = mm_between_epi16_int((_val), (_min), (_max))

static inline void mm_prefetch_buffer(const void* WINPR_RESTRICT buffer, size_t width,
                                      size_t stride, size_t height)
{
	const size_t srcbump = stride / sizeof(__m128i);
	const __m128i* buf = (const __m128i*)buffer;

	for (size_t y = 0; y < height; y++)
	{
		const __m128i* line = &buf[y * srcbump];
		for (size_t x = 0; x < width * sizeof(INT16) / sizeof(__m128i);
		     x += (CACHE_LINE_BYTES / sizeof(__m128i)))
		{
			const char* ptr = (const char*)&line[x];
			_mm_prefetch(ptr, _MM_HINT_NTA);
		}
	}
}

/*---------------------------------------------------------------------------*/
static pstatus_t
sse2_yCbCrToRGB_16s8u_P3AC4R_BGRX(const INT16* WINPR_RESTRICT pSrc[3],
                                  WINPR_ATTR_UNUSED UINT32 srcStep, BYTE* WINPR_RESTRICT pDst,
                                  UINT32 dstStep,
                                  const prim_size_t* WINPR_RESTRICT roi) /* region of interest */
{
	const __m128i zero = _mm_setzero_si128();
	const __m128i max = _mm_set1_epi16(255);
	const __m128i r_cr =
	    _mm_set1_epi16(WINPR_ASSERTING_INT_CAST(int16_t, ycbcr_table[14][0])); /*  1.403 << 14 */
	const __m128i g_cb =
	    _mm_set1_epi16(WINPR_ASSERTING_INT_CAST(int16_t, ycbcr_table[14][1])); /* -0.344 << 14 */
	const __m128i g_cr =
	    _mm_set1_epi16(WINPR_ASSERTING_INT_CAST(int16_t, ycbcr_table[14][2])); /* -0.714 << 14 */
	const __m128i b_cb =
	    _mm_set1_epi16(WINPR_ASSERTING_INT_CAST(int16_t, ycbcr_table[14][3])); /*  1.770 << 14 */
	const __m128i c4096 = _mm_set1_epi16(4096);
	const INT16* y_buf = pSrc[0];
	const INT16* cb_buf = pSrc[1];
	const INT16* cr_buf = pSrc[2];
	const UINT32 pad = roi->width % 16;
	const UINT32 step = sizeof(__m128i) / sizeof(INT16);
	const size_t imax = (roi->width - pad) * sizeof(INT16) / sizeof(__m128i);
	BYTE* d_buf = pDst;
	const size_t dstPad = (dstStep - roi->width * 4);

	mm_prefetch_buffer(y_buf, roi->width, (size_t)srcStep, roi->height);
	mm_prefetch_buffer(cr_buf, roi->width, (size_t)srcStep, roi->height);
	mm_prefetch_buffer(cb_buf, roi->width, (size_t)srcStep, roi->height);

	for (UINT32 yp = 0; yp < roi->height; ++yp)
	{
		for (size_t i = 0; i < imax; i += 2)
		{
			/* In order to use SSE2 signed 16-bit integer multiplication
			 * we need to convert the floating point factors to signed int
			 * without losing information.
			 * The result of this multiplication is 32 bit and we have two
			 * SSE instructions that return either the hi or lo word.
			 * Thus we will multiply the factors by the highest possible 2^n,
			 * take the upper 16 bits of the signed 32-bit result
			 * (_mm_mulhi_epi16) and correct this result by multiplying
			 * it by 2^(16-n).
			 *
			 * For the given factors in the conversion matrix the best
			 * possible n is 14.
			 *
			 * Example for calculating r:
			 * r = (y>>5) + 128 + (cr*1.403)>>5             // our base formula
			 * r = (y>>5) + 128 + (HIWORD(cr*(1.403<<14)<<2))>>5   // see above
			 * r = (y+4096)>>5 + (HIWORD(cr*22986)<<2)>>5     // simplification
			 * r = ((y+4096)>>2 + HIWORD(cr*22986)) >> 3
			 */
			/* y = (y_r_buf[i] + 4096) >> 2 */
			__m128i y1 = LOAD_SI128(y_buf);
			y_buf += step;
			y1 = _mm_add_epi16(y1, c4096);
			y1 = _mm_srai_epi16(y1, 2);
			/* cb = cb_g_buf[i]; */
			__m128i cb1 = LOAD_SI128(cb_buf);
			cb_buf += step;
			/* cr = cr_b_buf[i]; */
			__m128i cr1 = LOAD_SI128(cr_buf);
			cr_buf += step;
			/* (y + HIWORD(cr*22986)) >> 3 */
			__m128i r1 = _mm_add_epi16(y1, _mm_mulhi_epi16(cr1, r_cr));
			r1 = _mm_srai_epi16(r1, 3);
			/* r_buf[i] = CLIP(r); */
			mm_between_epi16(r1, zero, max);
			/* (y + HIWORD(cb*-5636) + HIWORD(cr*-11698)) >> 3 */
			__m128i g1 = _mm_add_epi16(y1, _mm_mulhi_epi16(cb1, g_cb));
			g1 = _mm_add_epi16(g1, _mm_mulhi_epi16(cr1, g_cr));
			g1 = _mm_srai_epi16(g1, 3);
			/* g_buf[i] = CLIP(g); */
			mm_between_epi16(g1, zero, max);
			/* (y + HIWORD(cb*28999)) >> 3 */
			__m128i b1 = _mm_add_epi16(y1, _mm_mulhi_epi16(cb1, b_cb));
			b1 = _mm_srai_epi16(b1, 3);
			/* b_buf[i] = CLIP(b); */
			mm_between_epi16(b1, zero, max);
			__m128i y2 = LOAD_SI128(y_buf);
			y_buf += step;
			y2 = _mm_add_epi16(y2, c4096);
			y2 = _mm_srai_epi16(y2, 2);
			/* cb = cb_g_buf[i]; */
			__m128i cb2 = LOAD_SI128(cb_buf);
			cb_buf += step;
			/* cr = cr_b_buf[i]; */
			__m128i cr2 = LOAD_SI128(cr_buf);
			cr_buf += step;
			/* (y + HIWORD(cr*22986)) >> 3 */
			__m128i r2 = _mm_add_epi16(y2, _mm_mulhi_epi16(cr2, r_cr));
			r2 = _mm_srai_epi16(r2, 3);
			/* r_buf[i] = CLIP(r); */
			mm_between_epi16(r2, zero, max);
			/* (y + HIWORD(cb*-5636) + HIWORD(cr*-11698)) >> 3 */
			__m128i g2 = _mm_add_epi16(y2, _mm_mulhi_epi16(cb2, g_cb));
			g2 = _mm_add_epi16(g2, _mm_mulhi_epi16(cr2, g_cr));
			g2 = _mm_srai_epi16(g2, 3);
			/* g_buf[i] = CLIP(g); */
			mm_between_epi16(g2, zero, max);
			/* (y + HIWORD(cb*28999)) >> 3 */
			__m128i b2 = _mm_add_epi16(y2, _mm_mulhi_epi16(cb2, b_cb));
			b2 = _mm_srai_epi16(b2, 3);
			/* b_buf[i] = CLIP(b); */
			mm_between_epi16(b2, zero, max);
			{
				/* The comments below pretend these are 8-byte registers
				 * rather than 16-byte, for readability.
				 */
				__m128i R0 = b1;                 /* R0 = 00B300B200B100B0 */
				__m128i R1 = b2;                 /* R1 = 00B700B600B500B4 */
				R0 = _mm_packus_epi16(R0, R1);   /* R0 = B7B6B5B4B3B2B1B0 */
				R1 = g1;                         /* R1 = 00G300G200G100G0 */
				__m128i R2 = g2;                 /* R2 = 00G700G600G500G4 */
				R1 = _mm_packus_epi16(R1, R2);   /* R1 = G7G6G5G4G3G2G1G0 */
				R2 = R1;                         /* R2 = G7G6G5G4G3G2G1G0 */
				R2 = _mm_unpacklo_epi8(R0, R2);  /* R2 = B3G3B2G2B1G1B0G0 */
				R1 = _mm_unpackhi_epi8(R0, R1);  /* R1 = B7G7B6G6B5G5B4G4 */
				R0 = r1;                         /* R0 = 00R300R200R100R0 */
				__m128i R3 = r2;                 /* R3 = 00R700R600R500R4 */
				R0 = _mm_packus_epi16(R0, R3);   /* R0 = R7R6R5R4R3R2R1R0 */
				R3 = mm_set1_epu32(0xFFFFFFFFU); /* R3 = FFFFFFFFFFFFFFFF */
				__m128i R4 = R3;                 /* R4 = FFFFFFFFFFFFFFFF */
				R4 = _mm_unpacklo_epi8(R0, R4);  /* R4 = R3FFR2FFR1FFR0FF */
				R3 = _mm_unpackhi_epi8(R0, R3);  /* R3 = R7FFR6FFR5FFR4FF */
				R0 = R4;                         /* R0 = R4               */
				R0 = _mm_unpacklo_epi16(R2, R0); /* R0 = B1G1R1FFB0G0R0FF */
				R4 = _mm_unpackhi_epi16(R2, R4); /* R4 = B3G3R3FFB2G2R2FF */
				R2 = R3;                         /* R2 = R3               */
				R2 = _mm_unpacklo_epi16(R1, R2); /* R2 = B5G5R5FFB4G4R4FF */
				R3 = _mm_unpackhi_epi16(R1, R3); /* R3 = B7G7R7FFB6G6R6FF */
				STORE_SI128(d_buf, R0);          /* B1G1R1FFB0G0R0FF      */
				d_buf += sizeof(__m128i);
				STORE_SI128(d_buf, R4); /* B3G3R3FFB2G2R2FF      */
				d_buf += sizeof(__m128i);
				STORE_SI128(d_buf, R2); /* B5G5R5FFB4G4R4FF      */
				d_buf += sizeof(__m128i);
				STORE_SI128(d_buf, R3); /* B7G7R7FFB6G6R6FF      */
				d_buf += sizeof(__m128i);
			}
		}

		for (UINT32 i = 0; i < pad; i++)
		{
			const INT32 divisor = 16;
			const INT32 Y = ((*y_buf++) + 4096) << divisor;
			const INT32 Cb = (*cb_buf++);
			const INT32 Cr = (*cr_buf++);
			const INT32 CrR = Cr * ycbcr_table[divisor][0];
			const INT32 CrG = Cr * ycbcr_table[divisor][1];
			const INT32 CbG = Cb * ycbcr_table[divisor][2];
			const INT32 CbB = Cb * ycbcr_table[divisor][3];
			const INT16 R = WINPR_ASSERTING_INT_CAST(int16_t, (((CrR + Y) >> divisor) >> 5));
			const INT16 G = WINPR_ASSERTING_INT_CAST(int16_t, (((Y - CbG - CrG) >> divisor) >> 5));
			const INT16 B = WINPR_ASSERTING_INT_CAST(int16_t, (((CbB + Y) >> divisor) >> 5));
			*d_buf++ = CLIP(B);
			*d_buf++ = CLIP(G);
			*d_buf++ = CLIP(R);
			*d_buf++ = 0xFF;
		}

		d_buf += dstPad;
	}

	return PRIMITIVES_SUCCESS;
}

/*---------------------------------------------------------------------------*/
static pstatus_t
sse2_yCbCrToRGB_16s8u_P3AC4R_RGBX(const INT16* WINPR_RESTRICT pSrc[3],
                                  WINPR_ATTR_UNUSED UINT32 srcStep, BYTE* WINPR_RESTRICT pDst,
                                  UINT32 dstStep,
                                  const prim_size_t* WINPR_RESTRICT roi) /* region of interest */
{
	const __m128i zero = _mm_setzero_si128();
	const __m128i max = _mm_set1_epi16(255);
	const __m128i r_cr =
	    _mm_set1_epi16(WINPR_ASSERTING_INT_CAST(int16_t, ycbcr_table[14][0])); /*  1.403 << 14 */
	const __m128i g_cb =
	    _mm_set1_epi16(WINPR_ASSERTING_INT_CAST(int16_t, ycbcr_table[14][1])); /* -0.344 << 14 */
	const __m128i g_cr =
	    _mm_set1_epi16(WINPR_ASSERTING_INT_CAST(int16_t, ycbcr_table[14][2])); /* -0.714 << 14 */
	const __m128i b_cb =
	    _mm_set1_epi16(WINPR_ASSERTING_INT_CAST(int16_t, ycbcr_table[14][3])); /*  1.770 << 14 */
	const __m128i c4096 = _mm_set1_epi16(4096);
	const INT16* y_buf = pSrc[0];
	const INT16* cb_buf = pSrc[1];
	const INT16* cr_buf = pSrc[2];
	const UINT32 pad = roi->width % 16;
	const UINT32 step = sizeof(__m128i) / sizeof(INT16);
	const size_t imax = (roi->width - pad) * sizeof(INT16) / sizeof(__m128i);
	BYTE* d_buf = pDst;
	const size_t dstPad = (dstStep - roi->width * 4);

	mm_prefetch_buffer(y_buf, roi->width, (size_t)srcStep, roi->height);
	mm_prefetch_buffer(cb_buf, roi->width, (size_t)srcStep, roi->height);
	mm_prefetch_buffer(cr_buf, roi->width, (size_t)srcStep, roi->height);

	for (UINT32 yp = 0; yp < roi->height; ++yp)
	{
		for (size_t i = 0; i < imax; i += 2)
		{
			/* In order to use SSE2 signed 16-bit integer multiplication
			 * we need to convert the floating point factors to signed int
			 * without losing information.
			 * The result of this multiplication is 32 bit and we have two
			 * SSE instructions that return either the hi or lo word.
			 * Thus we will multiply the factors by the highest possible 2^n,
			 * take the upper 16 bits of the signed 32-bit result
			 * (_mm_mulhi_epi16) and correct this result by multiplying
			 * it by 2^(16-n).
			 *
			 * For the given factors in the conversion matrix the best
			 * possible n is 14.
			 *
			 * Example for calculating r:
			 * r = (y>>5) + 128 + (cr*1.403)>>5             // our base formula
			 * r = (y>>5) + 128 + (HIWORD(cr*(1.403<<14)<<2))>>5   // see above
			 * r = (y+4096)>>5 + (HIWORD(cr*22986)<<2)>>5     // simplification
			 * r = ((y+4096)>>2 + HIWORD(cr*22986)) >> 3
			 */
			/* y = (y_r_buf[i] + 4096) >> 2 */
			__m128i y1 = LOAD_SI128(y_buf);
			y_buf += step;
			y1 = _mm_add_epi16(y1, c4096);
			y1 = _mm_srai_epi16(y1, 2);
			/* cb = cb_g_buf[i]; */
			__m128i cb1 = LOAD_SI128(cb_buf);
			cb_buf += step;
			/* cr = cr_b_buf[i]; */
			__m128i cr1 = LOAD_SI128(cr_buf);
			cr_buf += step;
			/* (y + HIWORD(cr*22986)) >> 3 */
			__m128i r1 = _mm_add_epi16(y1, _mm_mulhi_epi16(cr1, r_cr));
			r1 = _mm_srai_epi16(r1, 3);
			/* r_buf[i] = CLIP(r); */
			mm_between_epi16(r1, zero, max);
			/* (y + HIWORD(cb*-5636) + HIWORD(cr*-11698)) >> 3 */
			__m128i g1 = _mm_add_epi16(y1, _mm_mulhi_epi16(cb1, g_cb));
			g1 = _mm_add_epi16(g1, _mm_mulhi_epi16(cr1, g_cr));
			g1 = _mm_srai_epi16(g1, 3);
			/* g_buf[i] = CLIP(g); */
			mm_between_epi16(g1, zero, max);
			/* (y + HIWORD(cb*28999)) >> 3 */
			__m128i b1 = _mm_add_epi16(y1, _mm_mulhi_epi16(cb1, b_cb));
			b1 = _mm_srai_epi16(b1, 3);
			/* b_buf[i] = CLIP(b); */
			mm_between_epi16(b1, zero, max);
			__m128i y2 = LOAD_SI128(y_buf);
			y_buf += step;
			y2 = _mm_add_epi16(y2, c4096);
			y2 = _mm_srai_epi16(y2, 2);
			/* cb = cb_g_buf[i]; */
			__m128i cb2 = LOAD_SI128(cb_buf);
			cb_buf += step;
			/* cr = cr_b_buf[i]; */
			__m128i cr2 = LOAD_SI128(cr_buf);
			cr_buf += step;
			/* (y + HIWORD(cr*22986)) >> 3 */
			__m128i r2 = _mm_add_epi16(y2, _mm_mulhi_epi16(cr2, r_cr));
			r2 = _mm_srai_epi16(r2, 3);
			/* r_buf[i] = CLIP(r); */
			mm_between_epi16(r2, zero, max);
			/* (y + HIWORD(cb*-5636) + HIWORD(cr*-11698)) >> 3 */
			__m128i g2 = _mm_add_epi16(y2, _mm_mulhi_epi16(cb2, g_cb));
			g2 = _mm_add_epi16(g2, _mm_mulhi_epi16(cr2, g_cr));
			g2 = _mm_srai_epi16(g2, 3);
			/* g_buf[i] = CLIP(g); */
			mm_between_epi16(g2, zero, max);
			/* (y + HIWORD(cb*28999)) >> 3 */
			__m128i b2 = _mm_add_epi16(y2, _mm_mulhi_epi16(cb2, b_cb));
			b2 = _mm_srai_epi16(b2, 3);
			/* b_buf[i] = CLIP(b); */
			mm_between_epi16(b2, zero, max);
			{
				/* The comments below pretend these are 8-byte registers
				 * rather than 16-byte, for readability.
				 */
				__m128i R0 = r1;                 /* R0 = 00R300R200R100R0 */
				__m128i R1 = r2;                 /* R1 = 00R700R600R500R4 */
				R0 = _mm_packus_epi16(R0, R1);   /* R0 = R7R6R5R4R3R2R1R0 */
				R1 = g1;                         /* R1 = 00G300G200G100G0 */
				__m128i R2 = g2;                 /* R2 = 00G700G600G500G4 */
				R1 = _mm_packus_epi16(R1, R2);   /* R1 = G7G6G5G4G3G2G1G0 */
				R2 = R1;                         /* R2 = G7G6G5G4G3G2G1G0 */
				R2 = _mm_unpacklo_epi8(R0, R2);  /* R2 = R3G3R2G2R1G1R0G0 */
				R1 = _mm_unpackhi_epi8(R0, R1);  /* R1 = R7G7R6G6R5G5R4G4 */
				R0 = b1;                         /* R0 = 00B300B200B100B0 */
				__m128i R3 = b2;                 /* R3 = 00B700B600B500B4 */
				R0 = _mm_packus_epi16(R0, R3);   /* R0 = B7B6B5B4B3B2B1B0 */
				R3 = mm_set1_epu32(0xFFFFFFFFU); /* R3 = FFFFFFFFFFFFFFFF */
				__m128i R4 = R3;                 /* R4 = FFFFFFFFFFFFFFFF */
				R4 = _mm_unpacklo_epi8(R0, R4);  /* R4 = B3FFB2FFB1FFB0FF */
				R3 = _mm_unpackhi_epi8(R0, R3);  /* R3 = B7FFB6FFB5FFB4FF */
				R0 = R4;                         /* R0 = R4               */
				R0 = _mm_unpacklo_epi16(R2, R0); /* R0 = R1G1B1FFR0G0B0FF */
				R4 = _mm_unpackhi_epi16(R2, R4); /* R4 = R3G3B3FFR2G2B2FF */
				R2 = R3;                         /* R2 = R3               */
				R2 = _mm_unpacklo_epi16(R1, R2); /* R2 = R5G5B5FFR4G4B4FF */
				R3 = _mm_unpackhi_epi16(R1, R3); /* R3 = R7G7B7FFR6G6B6FF */
				STORE_SI128(d_buf, R0);          /* R1G1B1FFR0G0B0FF      */
				d_buf += sizeof(__m128i);
				STORE_SI128(d_buf, R4); /* R3G3B3FFR2G2B2FF      */
				d_buf += sizeof(__m128i);
				STORE_SI128(d_buf, R2); /* R5G5B5FFR4G4B4FF      */
				d_buf += sizeof(__m128i);
				STORE_SI128(d_buf, R3); /* R7G7B7FFR6G6B6FF      */
				d_buf += sizeof(__m128i);
			}
		}

		for (UINT32 i = 0; i < pad; i++)
		{
			const INT32 divisor = 16;
			const INT32 Y = ((*y_buf++) + 4096) << divisor;
			const INT32 Cb = (*cb_buf++);
			const INT32 Cr = (*cr_buf++);
			const INT32 CrR = Cr * ycbcr_table[divisor][0];
			const INT32 CrG = Cr * ycbcr_table[divisor][1];
			const INT32 CbG = Cb * ycbcr_table[divisor][2];
			const INT32 CbB = Cb * ycbcr_table[divisor][3];
			const INT16 R = WINPR_ASSERTING_INT_CAST(int16_t, (((CrR + Y) >> divisor) >> 5));
			const INT16 G = WINPR_ASSERTING_INT_CAST(int16_t, (((Y - CbG - CrG) >> divisor) >> 5));
			const INT16 B = WINPR_ASSERTING_INT_CAST(int16_t, (((CbB + Y) >> divisor) >> 5));
			*d_buf++ = CLIP(R);
			*d_buf++ = CLIP(G);
			*d_buf++ = CLIP(B);
			*d_buf++ = 0xFF;
		}

		d_buf += dstPad;
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t
sse2_yCbCrToRGB_16s8u_P3AC4R(const INT16* WINPR_RESTRICT pSrc[3], UINT32 srcStep,
                             BYTE* WINPR_RESTRICT pDst, UINT32 dstStep, UINT32 DstFormat,
                             const prim_size_t* WINPR_RESTRICT roi) /* region of interest */
{
	if (((ULONG_PTR)(pSrc[0]) & 0x0f) || ((ULONG_PTR)(pSrc[1]) & 0x0f) ||
	    ((ULONG_PTR)(pSrc[2]) & 0x0f) || ((ULONG_PTR)(pDst) & 0x0f) || (srcStep & 0x0f) ||
	    (dstStep & 0x0f))
	{
		/* We can't maintain 16-byte alignment. */
		return generic->yCbCrToRGB_16s8u_P3AC4R(pSrc, srcStep, pDst, dstStep, DstFormat, roi);
	}

	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return sse2_yCbCrToRGB_16s8u_P3AC4R_BGRX(pSrc, srcStep, pDst, dstStep, roi);

		case PIXEL_FORMAT_RGBA32:
		case PIXEL_FORMAT_RGBX32:
			return sse2_yCbCrToRGB_16s8u_P3AC4R_RGBX(pSrc, srcStep, pDst, dstStep, roi);

		default:
			return generic->yCbCrToRGB_16s8u_P3AC4R(pSrc, srcStep, pDst, dstStep, DstFormat, roi);
	}
}
/* The encodec YCbCr coeffectients are represented as 11.5 fixed-point
 * numbers. See the general code above.
 */
static pstatus_t
sse2_RGBToYCbCr_16s16s_P3P3(const INT16* WINPR_RESTRICT pSrc[3], int srcStep,
                            INT16* WINPR_RESTRICT pDst[3], int dstStep,
                            const prim_size_t* WINPR_RESTRICT roi) /* region of interest */
{
	const __m128i* r_buf = (const __m128i*)(pSrc[0]);
	const __m128i* g_buf = (const __m128i*)(pSrc[1]);
	const __m128i* b_buf = (const __m128i*)(pSrc[2]);
	__m128i* y_buf = (__m128i*)(pDst[0]);
	__m128i* cb_buf = (__m128i*)(pDst[1]);
	__m128i* cr_buf = (__m128i*)(pDst[2]);

	if (((ULONG_PTR)(pSrc[0]) & 0x0f) || ((ULONG_PTR)(pSrc[1]) & 0x0f) ||
	    ((ULONG_PTR)(pSrc[2]) & 0x0f) || ((ULONG_PTR)(pDst[0]) & 0x0f) ||
	    ((ULONG_PTR)(pDst[1]) & 0x0f) || ((ULONG_PTR)(pDst[2]) & 0x0f) || (roi->width & 0x07) ||
	    (srcStep & 127) || (dstStep & 127))
	{
		/* We can't maintain 16-byte alignment. */
		return generic->RGBToYCbCr_16s16s_P3P3(pSrc, srcStep, pDst, dstStep, roi);
	}

	const __m128i min = _mm_set1_epi16(-128 * 32);
	const __m128i max = _mm_set1_epi16(127 * 32);

	__m128i y_r = _mm_set1_epi16(9798);    /*  0.299000 << 15 */
	__m128i y_g = _mm_set1_epi16(19235);   /*  0.587000 << 15 */
	__m128i y_b = _mm_set1_epi16(3735);    /*  0.114000 << 15 */
	__m128i cb_r = _mm_set1_epi16(-5535);  /* -0.168935 << 15 */
	__m128i cb_g = _mm_set1_epi16(-10868); /* -0.331665 << 15 */
	__m128i cb_b = _mm_set1_epi16(16403);  /*  0.500590 << 15 */
	__m128i cr_r = _mm_set1_epi16(16377);  /*  0.499813 << 15 */
	__m128i cr_g = _mm_set1_epi16(-13714); /* -0.418531 << 15 */
	__m128i cr_b = _mm_set1_epi16(-2663);  /* -0.081282 << 15 */
	const size_t srcbump = WINPR_ASSERTING_INT_CAST(size_t, srcStep) / sizeof(__m128i);
	const size_t dstbump = WINPR_ASSERTING_INT_CAST(size_t, dstStep) / sizeof(__m128i);

	mm_prefetch_buffer(r_buf, roi->width, (size_t)srcStep, roi->height);
	mm_prefetch_buffer(g_buf, roi->width, (size_t)srcStep, roi->height);
	mm_prefetch_buffer(b_buf, roi->width, (size_t)srcStep, roi->height);

	const size_t imax = roi->width * sizeof(INT16) / sizeof(__m128i);

	for (UINT32 yp = 0; yp < roi->height; ++yp)
	{
		for (size_t i = 0; i < imax; i++)
		{
			/* In order to use SSE2 signed 16-bit integer multiplication we
			 * need to convert the floating point factors to signed int
			 * without losing information.  The result of this multiplication
			 * is 32 bit and using SSE2 we get either the product's hi or lo
			 * word.  Thus we will multiply the factors by the highest
			 * possible 2^n and take the upper 16 bits of the signed 32-bit
			 * result (_mm_mulhi_epi16).  Since the final result needs to
			 * be scaled by << 5 and also in in order to keep the precision
			 * within the upper 16 bits we will also have to scale the RGB
			 * values used in the multiplication by << 5+(16-n).
			 */
			__m128i r = LOAD_SI128(r_buf + i);
			__m128i g = LOAD_SI128(g_buf + i);
			__m128i b = LOAD_SI128(b_buf + i);
			/* r<<6; g<<6; b<<6 */
			r = _mm_slli_epi16(r, 6);
			g = _mm_slli_epi16(g, 6);
			b = _mm_slli_epi16(b, 6);
			/* y = HIWORD(r*y_r) + HIWORD(g*y_g) + HIWORD(b*y_b) + min */
			__m128i y = _mm_mulhi_epi16(r, y_r);
			y = _mm_add_epi16(y, _mm_mulhi_epi16(g, y_g));
			y = _mm_add_epi16(y, _mm_mulhi_epi16(b, y_b));
			y = _mm_add_epi16(y, min);
			/* y_r_buf[i] = MINMAX(y, 0, (255 << 5)) - (128 << 5); */
			mm_between_epi16(y, min, max);
			STORE_SI128(y_buf + i, y);
			/* cb = HIWORD(r*cb_r) + HIWORD(g*cb_g) + HIWORD(b*cb_b) */
			__m128i cb = _mm_mulhi_epi16(r, cb_r);
			cb = _mm_add_epi16(cb, _mm_mulhi_epi16(g, cb_g));
			cb = _mm_add_epi16(cb, _mm_mulhi_epi16(b, cb_b));
			/* cb_g_buf[i] = MINMAX(cb, (-128 << 5), (127 << 5)); */
			mm_between_epi16(cb, min, max);
			STORE_SI128(cb_buf + i, cb);
			/* cr = HIWORD(r*cr_r) + HIWORD(g*cr_g) + HIWORD(b*cr_b) */
			__m128i cr = _mm_mulhi_epi16(r, cr_r);
			cr = _mm_add_epi16(cr, _mm_mulhi_epi16(g, cr_g));
			cr = _mm_add_epi16(cr, _mm_mulhi_epi16(b, cr_b));
			/* cr_b_buf[i] = MINMAX(cr, (-128 << 5), (127 << 5)); */
			mm_between_epi16(cr, min, max);
			STORE_SI128(cr_buf + i, cr);
		}

		y_buf += srcbump;
		cb_buf += srcbump;
		cr_buf += srcbump;
		r_buf += dstbump;
		g_buf += dstbump;
		b_buf += dstbump;
	}

	return PRIMITIVES_SUCCESS;
}

/*---------------------------------------------------------------------------*/
static pstatus_t sse2_RGBToRGB_16s8u_P3AC4R_BGRX(
    const INT16* WINPR_RESTRICT pSrc[3],   /* 16-bit R,G, and B arrays */
    UINT32 srcStep,                        /* bytes between rows in source data */
    BYTE* WINPR_RESTRICT pDst,             /* 32-bit interleaved ARGB (ABGR?) data */
    UINT32 dstStep,                        /* bytes between rows in dest data */
    const prim_size_t* WINPR_RESTRICT roi) /* region of interest */
{
	const UINT16* pr = (const UINT16*)(pSrc[0]);
	const UINT16* pg = (const UINT16*)(pSrc[1]);
	const UINT16* pb = (const UINT16*)(pSrc[2]);
	const UINT32 pad = roi->width % 16;
	const __m128i a = mm_set1_epu32(0xFFFFFFFFU);
	BYTE* out = NULL;
	UINT32 srcbump = 0;
	UINT32 dstbump = 0;
	out = pDst;
	srcbump = (srcStep - (roi->width * sizeof(UINT16))) / sizeof(UINT16);
	dstbump = (dstStep - (roi->width * sizeof(UINT32)));

	for (UINT32 y = 0; y < roi->height; ++y)
	{
		for (UINT32 x = 0; x < roi->width - pad; x += 16)
		{
			__m128i r;
			__m128i g;
			__m128i b;
			/* The comments below pretend these are 8-byte registers
			 * rather than 16-byte, for readability.
			 */
			{
				__m128i R0;
				__m128i R1;
				R0 = LOAD_SI128(pb);
				pb += 8; /* R0 = 00B300B200B100B0 */
				R1 = LOAD_SI128(pb);
				pb += 8;                      /* R1 = 00B700B600B500B4 */
				b = _mm_packus_epi16(R0, R1); /* b = B7B6B5B4B3B2B1B0 */
			}
			{
				__m128i R0;
				__m128i R1;
				R0 = LOAD_SI128(pg);
				pg += 8; /* R1 = 00G300G200G100G0 */
				R1 = LOAD_SI128(pg);
				pg += 8;                      /* R2 = 00G700G600G500G4 */
				g = _mm_packus_epi16(R0, R1); /* g = G7G6G5G4G3G2G1G0 */
			}
			{
				__m128i R0;
				__m128i R1;
				R0 = LOAD_SI128(pr);
				pr += 8; /* R0 = 00R300R200R100R0 */
				R1 = LOAD_SI128(pr);
				pr += 8;                      /* R3 = 00R700R600R500R4 */
				r = _mm_packus_epi16(R0, R1); /* r = R7R6R5R4R3R2R1R0 */
			}
			{
				const __m128i gbLo = _mm_unpacklo_epi8(b, g); /* R0 = G7G6G5G4G3G2G1G0 */
				const __m128i gbHi = _mm_unpackhi_epi8(b, g); /* R1 = G7B7G6B7G5B5G4B4 */
				const __m128i arLo = _mm_unpacklo_epi8(r, a); /* R4 = FFR3FFR2FFR1FFR0 */
				const __m128i arHi = _mm_unpackhi_epi8(r, a); /* R3 = FFR7FFR6FFR5FFR4 */

				{
					const __m128i bgrx = _mm_unpacklo_epi16(gbLo, arLo);
					STORE_SI128(out, bgrx);
					out += 16; /* FFR1G1B1FFR0G0B0      */
				}
				{
					const __m128i bgrx = _mm_unpackhi_epi16(gbLo, arLo);
					STORE_SI128(out, bgrx);
					out += 16; /* FFR3G3B3FFR2G2B2      */
				}
				{
					const __m128i bgrx = _mm_unpacklo_epi16(gbHi, arHi);
					STORE_SI128(out, bgrx);
					out += 16; /* FFR5G5B5FFR4G4B4      */
				}
				{
					const __m128i bgrx = _mm_unpackhi_epi16(gbHi, arHi);
					STORE_SI128(out, bgrx);
					out += 16; /* FFR7G7B7FFR6G6B6      */
				}
			}
		}

		for (UINT32 x = 0; x < pad; x++)
		{
			const BYTE R = CLIP(*pr++);
			const BYTE G = CLIP(*pg++);
			const BYTE B = CLIP(*pb++);
			*out++ = B;
			*out++ = G;
			*out++ = R;
			*out++ = 0xFF;
		}

		/* Jump to next row. */
		pr += srcbump;
		pg += srcbump;
		pb += srcbump;
		out += dstbump;
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t sse2_RGBToRGB_16s8u_P3AC4R_RGBX(
    const INT16* WINPR_RESTRICT pSrc[3],   /* 16-bit R,G, and B arrays */
    UINT32 srcStep,                        /* bytes between rows in source data */
    BYTE* WINPR_RESTRICT pDst,             /* 32-bit interleaved ARGB (ABGR?) data */
    UINT32 dstStep,                        /* bytes between rows in dest data */
    const prim_size_t* WINPR_RESTRICT roi) /* region of interest */
{
	const UINT16* pr = (const UINT16*)(pSrc[0]);
	const UINT16* pg = (const UINT16*)(pSrc[1]);
	const UINT16* pb = (const UINT16*)(pSrc[2]);
	const UINT32 pad = roi->width % 16;
	const __m128i a = mm_set1_epu32(0xFFFFFFFFU);
	BYTE* out = NULL;
	UINT32 srcbump = 0;
	UINT32 dstbump = 0;
	out = pDst;
	srcbump = (srcStep - (roi->width * sizeof(UINT16))) / sizeof(UINT16);
	dstbump = (dstStep - (roi->width * sizeof(UINT32)));

	for (UINT32 y = 0; y < roi->height; ++y)
	{
		for (UINT32 x = 0; x < roi->width - pad; x += 16)
		{
			__m128i r;
			__m128i g;
			__m128i b;
			/* The comments below pretend these are 8-byte registers
			 * rather than 16-byte, for readability.
			 */
			{
				__m128i R0;
				__m128i R1;
				R0 = LOAD_SI128(pb);
				pb += 8; /* R0 = 00B300B200B100B0 */
				R1 = LOAD_SI128(pb);
				pb += 8;                      /* R1 = 00B700B600B500B4 */
				b = _mm_packus_epi16(R0, R1); /* b = B7B6B5B4B3B2B1B0 */
			}
			{
				__m128i R0;
				__m128i R1;
				R0 = LOAD_SI128(pg);
				pg += 8; /* R1 = 00G300G200G100G0 */
				R1 = LOAD_SI128(pg);
				pg += 8;                      /* R2 = 00G700G600G500G4 */
				g = _mm_packus_epi16(R0, R1); /* g = G7G6G5G4G3G2G1G0 */
			}
			{
				__m128i R0;
				__m128i R1;
				R0 = LOAD_SI128(pr);
				pr += 8; /* R0 = 00R300R200R100R0 */
				R1 = LOAD_SI128(pr);
				pr += 8;                      /* R3 = 00R700R600R500R4 */
				r = _mm_packus_epi16(R0, R1); /* r = R7R6R5R4R3R2R1R0 */
			}
			{
				__m128i gbHi;
				__m128i gbLo;
				__m128i arHi;
				__m128i arLo;
				{
					gbLo = _mm_unpacklo_epi8(r, g); /* R0 = G7G6G5G4G3G2G1G0 */
					gbHi = _mm_unpackhi_epi8(r, g); /* R1 = G7B7G6B7G5B5G4B4 */
					arLo = _mm_unpacklo_epi8(b, a); /* R4 = FFR3FFR2FFR1FFR0 */
					arHi = _mm_unpackhi_epi8(b, a); /* R3 = FFR7FFR6FFR5FFR4 */
				}
				{
					const __m128i bgrx = _mm_unpacklo_epi16(gbLo, arLo);
					STORE_SI128(out, bgrx);
					out += 16; /* FFR1G1B1FFR0G0B0      */
				}
				{
					const __m128i bgrx = _mm_unpackhi_epi16(gbLo, arLo);
					STORE_SI128(out, bgrx);
					out += 16; /* FFR3G3B3FFR2G2B2      */
				}
				{
					const __m128i bgrx = _mm_unpacklo_epi16(gbHi, arHi);
					STORE_SI128(out, bgrx);
					out += 16; /* FFR5G5B5FFR4G4B4      */
				}
				{
					const __m128i bgrx = _mm_unpackhi_epi16(gbHi, arHi);
					STORE_SI128(out, bgrx);
					out += 16; /* FFR7G7B7FFR6G6B6      */
				}
			}
		}

		for (UINT32 x = 0; x < pad; x++)
		{
			const BYTE R = CLIP(*pr++);
			const BYTE G = CLIP(*pg++);
			const BYTE B = CLIP(*pb++);
			*out++ = R;
			*out++ = G;
			*out++ = B;
			*out++ = 0xFF;
		}

		/* Jump to next row. */
		pr += srcbump;
		pg += srcbump;
		pb += srcbump;
		out += dstbump;
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t sse2_RGBToRGB_16s8u_P3AC4R_XBGR(
    const INT16* WINPR_RESTRICT pSrc[3],   /* 16-bit R,G, and B arrays */
    UINT32 srcStep,                        /* bytes between rows in source data */
    BYTE* WINPR_RESTRICT pDst,             /* 32-bit interleaved ARGB (ABGR?) data */
    UINT32 dstStep,                        /* bytes between rows in dest data */
    const prim_size_t* WINPR_RESTRICT roi) /* region of interest */
{
	const UINT16* pr = (const UINT16*)(pSrc[0]);
	const UINT16* pg = (const UINT16*)(pSrc[1]);
	const UINT16* pb = (const UINT16*)(pSrc[2]);
	const UINT32 pad = roi->width % 16;
	const __m128i a = mm_set1_epu32(0xFFFFFFFFU);
	BYTE* out = NULL;
	UINT32 srcbump = 0;
	UINT32 dstbump = 0;
	out = pDst;
	srcbump = (srcStep - (roi->width * sizeof(UINT16))) / sizeof(UINT16);
	dstbump = (dstStep - (roi->width * sizeof(UINT32)));

	for (UINT32 y = 0; y < roi->height; ++y)
	{
		for (UINT32 x = 0; x < roi->width - pad; x += 16)
		{
			__m128i r;
			__m128i g;
			__m128i b;
			/* The comments below pretend these are 8-byte registers
			 * rather than 16-byte, for readability.
			 */
			{
				__m128i R0;
				__m128i R1;
				R0 = LOAD_SI128(pb);
				pb += 8; /* R0 = 00B300B200B100B0 */
				R1 = LOAD_SI128(pb);
				pb += 8;                      /* R1 = 00B700B600B500B4 */
				b = _mm_packus_epi16(R0, R1); /* b = B7B6B5B4B3B2B1B0 */
			}
			{
				__m128i R0;
				__m128i R1;
				R0 = LOAD_SI128(pg);
				pg += 8; /* R1 = 00G300G200G100G0 */
				R1 = LOAD_SI128(pg);
				pg += 8;                      /* R2 = 00G700G600G500G4 */
				g = _mm_packus_epi16(R0, R1); /* g = G7G6G5G4G3G2G1G0 */
			}
			{
				__m128i R0;
				__m128i R1;
				R0 = LOAD_SI128(pr);
				pr += 8; /* R0 = 00R300R200R100R0 */
				R1 = LOAD_SI128(pr);
				pr += 8;                      /* R3 = 00R700R600R500R4 */
				r = _mm_packus_epi16(R0, R1); /* r = R7R6R5R4R3R2R1R0 */
			}
			{
				__m128i gbHi;
				__m128i gbLo;
				__m128i arHi;
				__m128i arLo;
				{
					gbLo = _mm_unpacklo_epi8(a, b); /* R0 = G7G6G5G4G3G2G1G0 */
					gbHi = _mm_unpackhi_epi8(a, b); /* R1 = G7B7G6B7G5B5G4B4 */
					arLo = _mm_unpacklo_epi8(g, r); /* R4 = FFR3FFR2FFR1FFR0 */
					arHi = _mm_unpackhi_epi8(g, r); /* R3 = FFR7FFR6FFR5FFR4 */
				}
				{
					const __m128i bgrx = _mm_unpacklo_epi16(gbLo, arLo);
					STORE_SI128(out, bgrx);
					out += 16; /* FFR1G1B1FFR0G0B0      */
				}
				{
					const __m128i bgrx = _mm_unpackhi_epi16(gbLo, arLo);
					STORE_SI128(out, bgrx);
					out += 16; /* FFR3G3B3FFR2G2B2      */
				}
				{
					const __m128i bgrx = _mm_unpacklo_epi16(gbHi, arHi);
					STORE_SI128(out, bgrx);
					out += 16; /* FFR5G5B5FFR4G4B4      */
				}
				{
					const __m128i bgrx = _mm_unpackhi_epi16(gbHi, arHi);
					STORE_SI128(out, bgrx);
					out += 16; /* FFR7G7B7FFR6G6B6      */
				}
			}
		}

		for (UINT32 x = 0; x < pad; x++)
		{
			const BYTE R = CLIP(*pr++);
			const BYTE G = CLIP(*pg++);
			const BYTE B = CLIP(*pb++);
			*out++ = 0xFF;
			*out++ = B;
			*out++ = G;
			*out++ = R;
		}

		/* Jump to next row. */
		pr += srcbump;
		pg += srcbump;
		pb += srcbump;
		out += dstbump;
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t sse2_RGBToRGB_16s8u_P3AC4R_XRGB(
    const INT16* WINPR_RESTRICT pSrc[3],   /* 16-bit R,G, and B arrays */
    UINT32 srcStep,                        /* bytes between rows in source data */
    BYTE* WINPR_RESTRICT pDst,             /* 32-bit interleaved ARGB (ABGR?) data */
    UINT32 dstStep,                        /* bytes between rows in dest data */
    const prim_size_t* WINPR_RESTRICT roi) /* region of interest */
{
	const UINT16* pr = (const UINT16*)(pSrc[0]);
	const UINT16* pg = (const UINT16*)(pSrc[1]);
	const UINT16* pb = (const UINT16*)(pSrc[2]);
	const __m128i a = mm_set1_epu32(0xFFFFFFFFU);
	const UINT32 pad = roi->width % 16;
	BYTE* out = NULL;
	UINT32 srcbump = 0;
	UINT32 dstbump = 0;
	out = pDst;
	srcbump = (srcStep - (roi->width * sizeof(UINT16))) / sizeof(UINT16);
	dstbump = (dstStep - (roi->width * sizeof(UINT32)));

	for (UINT32 y = 0; y < roi->height; ++y)
	{
		for (UINT32 x = 0; x < roi->width - pad; x += 16)
		{
			__m128i r;
			__m128i g;
			__m128i b;
			/* The comments below pretend these are 8-byte registers
			 * rather than 16-byte, for readability.
			 */
			{
				__m128i R0;
				__m128i R1;
				R0 = LOAD_SI128(pb);
				pb += 8; /* R0 = 00B300B200B100B0 */
				R1 = LOAD_SI128(pb);
				pb += 8;                      /* R1 = 00B700B600B500B4 */
				b = _mm_packus_epi16(R0, R1); /* b = B7B6B5B4B3B2B1B0 */
			}
			{
				__m128i R0;
				__m128i R1;
				R0 = LOAD_SI128(pg);
				pg += 8; /* R1 = 00G300G200G100G0 */
				R1 = LOAD_SI128(pg);
				pg += 8;                      /* R2 = 00G700G600G500G4 */
				g = _mm_packus_epi16(R0, R1); /* g = G7G6G5G4G3G2G1G0 */
			}
			{
				__m128i R0;
				__m128i R1;
				R0 = LOAD_SI128(pr);
				pr += 8; /* R0 = 00R300R200R100R0 */
				R1 = LOAD_SI128(pr);
				pr += 8;                      /* R3 = 00R700R600R500R4 */
				r = _mm_packus_epi16(R0, R1); /* r = R7R6R5R4R3R2R1R0 */
			}
			{
				__m128i gbHi;
				__m128i gbLo;
				__m128i arHi;
				__m128i arLo;
				{
					gbLo = _mm_unpacklo_epi8(a, r); /* R0 = G7G6G5G4G3G2G1G0 */
					gbHi = _mm_unpackhi_epi8(a, r); /* R1 = G7B7G6B7G5B5G4B4 */
					arLo = _mm_unpacklo_epi8(g, b); /* R4 = FFR3FFR2FFR1FFR0 */
					arHi = _mm_unpackhi_epi8(g, b); /* R3 = FFR7FFR6FFR5FFR4 */
				}
				{
					const __m128i bgrx = _mm_unpacklo_epi16(gbLo, arLo);
					STORE_SI128(out, bgrx);
					out += 16; /* FFR1G1B1FFR0G0B0      */
				}
				{
					const __m128i bgrx = _mm_unpackhi_epi16(gbLo, arLo);
					STORE_SI128(out, bgrx);
					out += 16; /* FFR3G3B3FFR2G2B2      */
				}
				{
					const __m128i bgrx = _mm_unpacklo_epi16(gbHi, arHi);
					STORE_SI128(out, bgrx);
					out += 16; /* FFR5G5B5FFR4G4B4      */
				}
				{
					const __m128i bgrx = _mm_unpackhi_epi16(gbHi, arHi);
					STORE_SI128(out, bgrx);
					out += 16; /* FFR7G7B7FFR6G6B6      */
				}
			}
		}

		for (UINT32 x = 0; x < pad; x++)
		{
			const BYTE R = CLIP(*pr++);
			const BYTE G = CLIP(*pg++);
			const BYTE B = CLIP(*pb++);
			*out++ = 0xFF;
			*out++ = R;
			*out++ = G;
			*out++ = B;
		}

		/* Jump to next row. */
		pr += srcbump;
		pg += srcbump;
		pb += srcbump;
		out += dstbump;
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t
sse2_RGBToRGB_16s8u_P3AC4R(const INT16* WINPR_RESTRICT pSrc[3], /* 16-bit R,G, and B arrays */
                           UINT32 srcStep,            /* bytes between rows in source data */
                           BYTE* WINPR_RESTRICT pDst, /* 32-bit interleaved ARGB (ABGR?) data */
                           UINT32 dstStep,            /* bytes between rows in dest data */
                           UINT32 DstFormat, const prim_size_t* WINPR_RESTRICT roi)
{
	if (((ULONG_PTR)pSrc[0] & 0x0f) || ((ULONG_PTR)pSrc[1] & 0x0f) || ((ULONG_PTR)pSrc[2] & 0x0f) ||
	    (srcStep & 0x0f) || ((ULONG_PTR)pDst & 0x0f) || (dstStep & 0x0f))
		return generic->RGBToRGB_16s8u_P3AC4R(pSrc, srcStep, pDst, dstStep, DstFormat, roi);

	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return sse2_RGBToRGB_16s8u_P3AC4R_BGRX(pSrc, srcStep, pDst, dstStep, roi);

		case PIXEL_FORMAT_RGBA32:
		case PIXEL_FORMAT_RGBX32:
			return sse2_RGBToRGB_16s8u_P3AC4R_RGBX(pSrc, srcStep, pDst, dstStep, roi);

		case PIXEL_FORMAT_ABGR32:
		case PIXEL_FORMAT_XBGR32:
			return sse2_RGBToRGB_16s8u_P3AC4R_XBGR(pSrc, srcStep, pDst, dstStep, roi);

		case PIXEL_FORMAT_ARGB32:
		case PIXEL_FORMAT_XRGB32:
			return sse2_RGBToRGB_16s8u_P3AC4R_XRGB(pSrc, srcStep, pDst, dstStep, roi);

		default:
			return generic->RGBToRGB_16s8u_P3AC4R(pSrc, srcStep, pDst, dstStep, DstFormat, roi);
	}
}
#endif

void primitives_init_colors_sse2_int(primitives_t* WINPR_RESTRICT prims)
{
#if defined(SSE_AVX_INTRINSICS_ENABLED)
	generic = primitives_get_generic();

	WLog_VRB(PRIM_TAG, "SSE2/SSE3 optimizations");
	prims->RGBToRGB_16s8u_P3AC4R = sse2_RGBToRGB_16s8u_P3AC4R;
	prims->yCbCrToRGB_16s8u_P3AC4R = sse2_yCbCrToRGB_16s8u_P3AC4R;
	prims->RGBToYCbCr_16s16s_P3P3 = sse2_RGBToYCbCr_16s16s_P3P3;

#else
	WLog_VRB(PRIM_TAG, "undefined WITH_SIMD or SSE2 intrinsics not available");
	WINPR_UNUSED(prims);
#endif
}
