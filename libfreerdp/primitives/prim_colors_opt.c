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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/types.h>
#include <freerdp/primitives.h>
#include <winpr/sysinfo.h>

#ifdef WITH_SSE2
#include <emmintrin.h>
#elif defined(WITH_NEON)
#include <arm_neon.h>
#endif /* WITH_SSE2 else WITH_NEON */

#include "prim_internal.h"
#include "prim_templates.h"

static primitives_t* generic = NULL;

#ifdef WITH_SSE2

#ifdef __GNUC__
#define GNU_INLINE __attribute__((__gnu_inline__, __always_inline__, __artificial__))
#else
#define GNU_INLINE
#endif

#define CACHE_LINE_BYTES 64

#define _mm_between_epi16(_val, _min, _max)                    \
	do                                                         \
	{                                                          \
		_val = _mm_min_epi16(_max, _mm_max_epi16(_val, _min)); \
	} while (0)

#ifdef DO_PREFETCH
/*---------------------------------------------------------------------------*/
static inline void GNU_INLINE _mm_prefetch_buffer(char* buffer, int num_bytes)
{
	__m128i* buf = (__m128i*)buffer;
	unsigned int i;

	for (i = 0; i < (num_bytes / sizeof(__m128i)); i += (CACHE_LINE_BYTES / sizeof(__m128i)))
	{
		_mm_prefetch((char*)(&buf[i]), _MM_HINT_NTA);
	}
}
#endif /* DO_PREFETCH */

/*---------------------------------------------------------------------------*/
static pstatus_t sse2_yCbCrToRGB_16s16s_P3P3(const INT16* const pSrc[3], int srcStep,
                                             INT16* pDst[3], int dstStep,
                                             const prim_size_t* roi) /* region of interest */
{
	__m128i zero, max, r_cr, g_cb, g_cr, b_cb, c4096;
	__m128i *y_buf, *cb_buf, *cr_buf, *r_buf, *g_buf, *b_buf;
	UINT32 yp;
	int srcbump, dstbump, imax;

	if (((ULONG_PTR)(pSrc[0]) & 0x0f) || ((ULONG_PTR)(pSrc[1]) & 0x0f) ||
	    ((ULONG_PTR)(pSrc[2]) & 0x0f) || ((ULONG_PTR)(pDst[0]) & 0x0f) ||
	    ((ULONG_PTR)(pDst[1]) & 0x0f) || ((ULONG_PTR)(pDst[2]) & 0x0f) || (roi->width & 0x07) ||
	    (srcStep & 127) || (dstStep & 127))
	{
		/* We can't maintain 16-byte alignment. */
		return generic->yCbCrToRGB_16s16s_P3P3(pSrc, srcStep, pDst, dstStep, roi);
	}

	zero = _mm_setzero_si128();
	max = _mm_set1_epi16(255);
	y_buf = (__m128i*)(pSrc[0]);
	cb_buf = (__m128i*)(pSrc[1]);
	cr_buf = (__m128i*)(pSrc[2]);
	r_buf = (__m128i*)(pDst[0]);
	g_buf = (__m128i*)(pDst[1]);
	b_buf = (__m128i*)(pDst[2]);
	r_cr = _mm_set1_epi16(22986);  /*  1.403 << 14 */
	g_cb = _mm_set1_epi16(-5636);  /* -0.344 << 14 */
	g_cr = _mm_set1_epi16(-11698); /* -0.714 << 14 */
	b_cb = _mm_set1_epi16(28999);  /*  1.770 << 14 */
	c4096 = _mm_set1_epi16(4096);
	srcbump = srcStep / sizeof(__m128i);
	dstbump = dstStep / sizeof(__m128i);
#ifdef DO_PREFETCH

	/* Prefetch Y's, Cb's, and Cr's. */
	for (yp = 0; yp < roi->height; yp++)
	{
		int i;

		for (i = 0; i < roi->width * sizeof(INT16) / sizeof(__m128i);
		     i += (CACHE_LINE_BYTES / sizeof(__m128i)))
		{
			_mm_prefetch((char*)(&y_buf[i]), _MM_HINT_NTA);
			_mm_prefetch((char*)(&cb_buf[i]), _MM_HINT_NTA);
			_mm_prefetch((char*)(&cr_buf[i]), _MM_HINT_NTA);
		}

		y_buf += srcbump;
		cb_buf += srcbump;
		cr_buf += srcbump;
	}

	y_buf = (__m128i*)(pSrc[0]);
	cb_buf = (__m128i*)(pSrc[1]);
	cr_buf = (__m128i*)(pSrc[2]);
#endif /* DO_PREFETCH */
	imax = roi->width * sizeof(INT16) / sizeof(__m128i);

	for (yp = 0; yp < roi->height; ++yp)
	{
		int i;

		for (i = 0; i < imax; i++)
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
			__m128i y, cb, cr, r, g, b;
			y = _mm_load_si128(y_buf + i);
			y = _mm_add_epi16(y, c4096);
			y = _mm_srai_epi16(y, 2);
			/* cb = cb_g_buf[i]; */
			cb = _mm_load_si128(cb_buf + i);
			/* cr = cr_b_buf[i]; */
			cr = _mm_load_si128(cr_buf + i);
			/* (y + HIWORD(cr*22986)) >> 3 */
			r = _mm_add_epi16(y, _mm_mulhi_epi16(cr, r_cr));
			r = _mm_srai_epi16(r, 3);
			/* r_buf[i] = CLIP(r); */
			_mm_between_epi16(r, zero, max);
			_mm_store_si128(r_buf + i, r);
			/* (y + HIWORD(cb*-5636) + HIWORD(cr*-11698)) >> 3 */
			g = _mm_add_epi16(y, _mm_mulhi_epi16(cb, g_cb));
			g = _mm_add_epi16(g, _mm_mulhi_epi16(cr, g_cr));
			g = _mm_srai_epi16(g, 3);
			/* g_buf[i] = CLIP(g); */
			_mm_between_epi16(g, zero, max);
			_mm_store_si128(g_buf + i, g);
			/* (y + HIWORD(cb*28999)) >> 3 */
			b = _mm_add_epi16(y, _mm_mulhi_epi16(cb, b_cb));
			b = _mm_srai_epi16(b, 3);
			/* b_buf[i] = CLIP(b); */
			_mm_between_epi16(b, zero, max);
			_mm_store_si128(b_buf + i, b);
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
static pstatus_t sse2_yCbCrToRGB_16s8u_P3AC4R_BGRX(const INT16* const pSrc[3], UINT32 srcStep,
                                                   BYTE* pDst, UINT32 dstStep,
                                                   const prim_size_t* roi) /* region of interest */
{
	const __m128i zero = _mm_setzero_si128();
	const __m128i max = _mm_set1_epi16(255);
	const __m128i r_cr = _mm_set1_epi16(22986);  /*  1.403 << 14 */
	const __m128i g_cb = _mm_set1_epi16(-5636);  /* -0.344 << 14 */
	const __m128i g_cr = _mm_set1_epi16(-11698); /* -0.714 << 14 */
	const __m128i b_cb = _mm_set1_epi16(28999);  /*  1.770 << 14 */
	const __m128i c4096 = _mm_set1_epi16(4096);
	const INT16* y_buf = (INT16*)pSrc[0];
	const INT16* cb_buf = (INT16*)pSrc[1];
	const INT16* cr_buf = (INT16*)pSrc[2];
	const UINT32 pad = roi->width % 16;
	const UINT32 step = sizeof(__m128i) / sizeof(INT16);
	const UINT32 imax = (roi->width - pad) * sizeof(INT16) / sizeof(__m128i);
	BYTE* d_buf = pDst;
	UINT32 yp;
	const size_t dstPad = (dstStep - roi->width * 4);
#ifdef DO_PREFETCH

	/* Prefetch Y's, Cb's, and Cr's. */
	for (yp = 0; yp < roi->height; yp++)
	{
		int i;

		for (i = 0; i < imax; i += (CACHE_LINE_BYTES / sizeof(__m128i)))
		{
			_mm_prefetch((char*)(&((__m128i*)y_buf)[i]), _MM_HINT_NTA);
			_mm_prefetch((char*)(&((__m128i*)cb_buf)[i]), _MM_HINT_NTA);
			_mm_prefetch((char*)(&((__m128i*)cr_buf)[i]), _MM_HINT_NTA);
		}

		y_buf += srcStep / sizeof(INT16);
		cb_buf += srcStep / sizeof(INT16);
		cr_buf += srcStep / sizeof(INT16);
	}

	y_buf = (INT16*)pSrc[0];
	cb_buf = (INT16*)pSrc[1];
	cr_buf = (INT16*)pSrc[2];
#endif /* DO_PREFETCH */

	for (yp = 0; yp < roi->height; ++yp)
	{
		UINT32 i;

		for (i = 0; i < imax; i += 2)
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
			__m128i y1, y2, cb1, cb2, cr1, cr2, r1, r2, g1, g2, b1, b2;
			y1 = _mm_load_si128((__m128i*)y_buf);
			y_buf += step;
			y1 = _mm_add_epi16(y1, c4096);
			y1 = _mm_srai_epi16(y1, 2);
			/* cb = cb_g_buf[i]; */
			cb1 = _mm_load_si128((__m128i*)cb_buf);
			cb_buf += step;
			/* cr = cr_b_buf[i]; */
			cr1 = _mm_load_si128((__m128i*)cr_buf);
			cr_buf += step;
			/* (y + HIWORD(cr*22986)) >> 3 */
			r1 = _mm_add_epi16(y1, _mm_mulhi_epi16(cr1, r_cr));
			r1 = _mm_srai_epi16(r1, 3);
			/* r_buf[i] = CLIP(r); */
			_mm_between_epi16(r1, zero, max);
			/* (y + HIWORD(cb*-5636) + HIWORD(cr*-11698)) >> 3 */
			g1 = _mm_add_epi16(y1, _mm_mulhi_epi16(cb1, g_cb));
			g1 = _mm_add_epi16(g1, _mm_mulhi_epi16(cr1, g_cr));
			g1 = _mm_srai_epi16(g1, 3);
			/* g_buf[i] = CLIP(g); */
			_mm_between_epi16(g1, zero, max);
			/* (y + HIWORD(cb*28999)) >> 3 */
			b1 = _mm_add_epi16(y1, _mm_mulhi_epi16(cb1, b_cb));
			b1 = _mm_srai_epi16(b1, 3);
			/* b_buf[i] = CLIP(b); */
			_mm_between_epi16(b1, zero, max);
			y2 = _mm_load_si128((__m128i*)y_buf);
			y_buf += step;
			y2 = _mm_add_epi16(y2, c4096);
			y2 = _mm_srai_epi16(y2, 2);
			/* cb = cb_g_buf[i]; */
			cb2 = _mm_load_si128((__m128i*)cb_buf);
			cb_buf += step;
			/* cr = cr_b_buf[i]; */
			cr2 = _mm_load_si128((__m128i*)cr_buf);
			cr_buf += step;
			/* (y + HIWORD(cr*22986)) >> 3 */
			r2 = _mm_add_epi16(y2, _mm_mulhi_epi16(cr2, r_cr));
			r2 = _mm_srai_epi16(r2, 3);
			/* r_buf[i] = CLIP(r); */
			_mm_between_epi16(r2, zero, max);
			/* (y + HIWORD(cb*-5636) + HIWORD(cr*-11698)) >> 3 */
			g2 = _mm_add_epi16(y2, _mm_mulhi_epi16(cb2, g_cb));
			g2 = _mm_add_epi16(g2, _mm_mulhi_epi16(cr2, g_cr));
			g2 = _mm_srai_epi16(g2, 3);
			/* g_buf[i] = CLIP(g); */
			_mm_between_epi16(g2, zero, max);
			/* (y + HIWORD(cb*28999)) >> 3 */
			b2 = _mm_add_epi16(y2, _mm_mulhi_epi16(cb2, b_cb));
			b2 = _mm_srai_epi16(b2, 3);
			/* b_buf[i] = CLIP(b); */
			_mm_between_epi16(b2, zero, max);
			{
				__m128i R0, R1, R2, R3, R4;
				/* The comments below pretend these are 8-byte registers
				 * rather than 16-byte, for readability.
				 */
				R0 = b1;                              /* R0 = 00B300B200B100B0 */
				R1 = b2;                              /* R1 = 00B700B600B500B4 */
				R0 = _mm_packus_epi16(R0, R1);        /* R0 = B7B6B5B4B3B2B1B0 */
				R1 = g1;                              /* R1 = 00G300G200G100G0 */
				R2 = g2;                              /* R2 = 00G700G600G500G4 */
				R1 = _mm_packus_epi16(R1, R2);        /* R1 = G7G6G5G4G3G2G1G0 */
				R2 = R1;                              /* R2 = G7G6G5G4G3G2G1G0 */
				R2 = _mm_unpacklo_epi8(R0, R2);       /* R2 = B3G3B2G2B1G1B0G0 */
				R1 = _mm_unpackhi_epi8(R0, R1);       /* R1 = B7G7B6G6B5G5B4G4 */
				R0 = r1;                              /* R0 = 00R300R200R100R0 */
				R3 = r2;                              /* R3 = 00R700R600R500R4 */
				R0 = _mm_packus_epi16(R0, R3);        /* R0 = R7R6R5R4R3R2R1R0 */
				R3 = _mm_set1_epi32(0xFFFFFFFFU);     /* R3 = FFFFFFFFFFFFFFFF */
				R4 = R3;                              /* R4 = FFFFFFFFFFFFFFFF */
				R4 = _mm_unpacklo_epi8(R0, R4);       /* R4 = R3FFR2FFR1FFR0FF */
				R3 = _mm_unpackhi_epi8(R0, R3);       /* R3 = R7FFR6FFR5FFR4FF */
				R0 = R4;                              /* R0 = R4               */
				R0 = _mm_unpacklo_epi16(R2, R0);      /* R0 = B1G1R1FFB0G0R0FF */
				R4 = _mm_unpackhi_epi16(R2, R4);      /* R4 = B3G3R3FFB2G2R2FF */
				R2 = R3;                              /* R2 = R3               */
				R2 = _mm_unpacklo_epi16(R1, R2);      /* R2 = B5G5R5FFB4G4R4FF */
				R3 = _mm_unpackhi_epi16(R1, R3);      /* R3 = B7G7R7FFB6G6R6FF */
				_mm_store_si128((__m128i*)d_buf, R0); /* B1G1R1FFB0G0R0FF      */
				d_buf += sizeof(__m128i);
				_mm_store_si128((__m128i*)d_buf, R4); /* B3G3R3FFB2G2R2FF      */
				d_buf += sizeof(__m128i);
				_mm_store_si128((__m128i*)d_buf, R2); /* B5G5R5FFB4G4R4FF      */
				d_buf += sizeof(__m128i);
				_mm_store_si128((__m128i*)d_buf, R3); /* B7G7R7FFB6G6R6FF      */
				d_buf += sizeof(__m128i);
			}
		}

		for (i = 0; i < pad; i++)
		{
			const INT32 divisor = 16;
			const INT32 Y = ((*y_buf++) + 4096) << divisor;
			const INT32 Cb = (*cb_buf++);
			const INT32 Cr = (*cr_buf++);
			const INT32 CrR = Cr * (INT32)(1.402525f * (1 << divisor));
			const INT32 CrG = Cr * (INT32)(0.714401f * (1 << divisor));
			const INT32 CbG = Cb * (INT32)(0.343730f * (1 << divisor));
			const INT32 CbB = Cb * (INT32)(1.769905f * (1 << divisor));
			const INT16 R = ((INT16)((CrR + Y) >> divisor) >> 5);
			const INT16 G = ((INT16)((Y - CbG - CrG) >> divisor) >> 5);
			const INT16 B = ((INT16)((CbB + Y) >> divisor) >> 5);
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
static pstatus_t sse2_yCbCrToRGB_16s8u_P3AC4R_RGBX(const INT16* const pSrc[3], UINT32 srcStep,
                                                   BYTE* pDst, UINT32 dstStep,
                                                   const prim_size_t* roi) /* region of interest */
{
	const __m128i zero = _mm_setzero_si128();
	const __m128i max = _mm_set1_epi16(255);
	const __m128i r_cr = _mm_set1_epi16(22986);  /*  1.403 << 14 */
	const __m128i g_cb = _mm_set1_epi16(-5636);  /* -0.344 << 14 */
	const __m128i g_cr = _mm_set1_epi16(-11698); /* -0.714 << 14 */
	const __m128i b_cb = _mm_set1_epi16(28999);  /*  1.770 << 14 */
	const __m128i c4096 = _mm_set1_epi16(4096);
	const INT16* y_buf = (INT16*)pSrc[0];
	const INT16* cb_buf = (INT16*)pSrc[1];
	const INT16* cr_buf = (INT16*)pSrc[2];
	const UINT32 pad = roi->width % 16;
	const UINT32 step = sizeof(__m128i) / sizeof(INT16);
	const UINT32 imax = (roi->width - pad) * sizeof(INT16) / sizeof(__m128i);
	BYTE* d_buf = pDst;
	UINT32 yp;
	const size_t dstPad = (dstStep - roi->width * 4);
#ifdef DO_PREFETCH

	/* Prefetch Y's, Cb's, and Cr's. */
	for (yp = 0; yp < roi->height; yp++)
	{
		int i;

		for (i = 0; i < imax; i += (CACHE_LINE_BYTES / sizeof(__m128i)))
		{
			_mm_prefetch((char*)(&((__m128i*)y_buf)[i]), _MM_HINT_NTA);
			_mm_prefetch((char*)(&((__m128i*)cb_buf)[i]), _MM_HINT_NTA);
			_mm_prefetch((char*)(&((__m128i*)cr_buf)[i]), _MM_HINT_NTA);
		}

		y_buf += srcStep / sizeof(INT16);
		cb_buf += srcStep / sizeof(INT16);
		cr_buf += srcStep / sizeof(INT16);
	}

	y_buf = (INT16*)(pSrc[0]);
	cb_buf = (INT16*)(pSrc[1]);
	cr_buf = (INT16*)(pSrc[2]);
#endif /* DO_PREFETCH */

	for (yp = 0; yp < roi->height; ++yp)
	{
		UINT32 i;

		for (i = 0; i < imax; i += 2)
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
			__m128i y1, y2, cb1, cb2, cr1, cr2, r1, r2, g1, g2, b1, b2;
			y1 = _mm_load_si128((__m128i*)y_buf);
			y_buf += step;
			y1 = _mm_add_epi16(y1, c4096);
			y1 = _mm_srai_epi16(y1, 2);
			/* cb = cb_g_buf[i]; */
			cb1 = _mm_load_si128((__m128i*)cb_buf);
			cb_buf += step;
			/* cr = cr_b_buf[i]; */
			cr1 = _mm_load_si128((__m128i*)cr_buf);
			cr_buf += step;
			/* (y + HIWORD(cr*22986)) >> 3 */
			r1 = _mm_add_epi16(y1, _mm_mulhi_epi16(cr1, r_cr));
			r1 = _mm_srai_epi16(r1, 3);
			/* r_buf[i] = CLIP(r); */
			_mm_between_epi16(r1, zero, max);
			/* (y + HIWORD(cb*-5636) + HIWORD(cr*-11698)) >> 3 */
			g1 = _mm_add_epi16(y1, _mm_mulhi_epi16(cb1, g_cb));
			g1 = _mm_add_epi16(g1, _mm_mulhi_epi16(cr1, g_cr));
			g1 = _mm_srai_epi16(g1, 3);
			/* g_buf[i] = CLIP(g); */
			_mm_between_epi16(g1, zero, max);
			/* (y + HIWORD(cb*28999)) >> 3 */
			b1 = _mm_add_epi16(y1, _mm_mulhi_epi16(cb1, b_cb));
			b1 = _mm_srai_epi16(b1, 3);
			/* b_buf[i] = CLIP(b); */
			_mm_between_epi16(b1, zero, max);
			y2 = _mm_load_si128((__m128i*)y_buf);
			y_buf += step;
			y2 = _mm_add_epi16(y2, c4096);
			y2 = _mm_srai_epi16(y2, 2);
			/* cb = cb_g_buf[i]; */
			cb2 = _mm_load_si128((__m128i*)cb_buf);
			cb_buf += step;
			/* cr = cr_b_buf[i]; */
			cr2 = _mm_load_si128((__m128i*)cr_buf);
			cr_buf += step;
			/* (y + HIWORD(cr*22986)) >> 3 */
			r2 = _mm_add_epi16(y2, _mm_mulhi_epi16(cr2, r_cr));
			r2 = _mm_srai_epi16(r2, 3);
			/* r_buf[i] = CLIP(r); */
			_mm_between_epi16(r2, zero, max);
			/* (y + HIWORD(cb*-5636) + HIWORD(cr*-11698)) >> 3 */
			g2 = _mm_add_epi16(y2, _mm_mulhi_epi16(cb2, g_cb));
			g2 = _mm_add_epi16(g2, _mm_mulhi_epi16(cr2, g_cr));
			g2 = _mm_srai_epi16(g2, 3);
			/* g_buf[i] = CLIP(g); */
			_mm_between_epi16(g2, zero, max);
			/* (y + HIWORD(cb*28999)) >> 3 */
			b2 = _mm_add_epi16(y2, _mm_mulhi_epi16(cb2, b_cb));
			b2 = _mm_srai_epi16(b2, 3);
			/* b_buf[i] = CLIP(b); */
			_mm_between_epi16(b2, zero, max);
			{
				__m128i R0, R1, R2, R3, R4;
				/* The comments below pretend these are 8-byte registers
				 * rather than 16-byte, for readability.
				 */
				R0 = r1;                              /* R0 = 00R300R200R100R0 */
				R1 = r2;                              /* R1 = 00R700R600R500R4 */
				R0 = _mm_packus_epi16(R0, R1);        /* R0 = R7R6R5R4R3R2R1R0 */
				R1 = g1;                              /* R1 = 00G300G200G100G0 */
				R2 = g2;                              /* R2 = 00G700G600G500G4 */
				R1 = _mm_packus_epi16(R1, R2);        /* R1 = G7G6G5G4G3G2G1G0 */
				R2 = R1;                              /* R2 = G7G6G5G4G3G2G1G0 */
				R2 = _mm_unpacklo_epi8(R0, R2);       /* R2 = R3G3R2G2R1G1R0G0 */
				R1 = _mm_unpackhi_epi8(R0, R1);       /* R1 = R7G7R6G6R5G5R4G4 */
				R0 = b1;                              /* R0 = 00B300B200B100B0 */
				R3 = b2;                              /* R3 = 00B700B600B500B4 */
				R0 = _mm_packus_epi16(R0, R3);        /* R0 = B7B6B5B4B3B2B1B0 */
				R3 = _mm_set1_epi32(0xFFFFFFFFU);     /* R3 = FFFFFFFFFFFFFFFF */
				R4 = R3;                              /* R4 = FFFFFFFFFFFFFFFF */
				R4 = _mm_unpacklo_epi8(R0, R4);       /* R4 = B3FFB2FFB1FFB0FF */
				R3 = _mm_unpackhi_epi8(R0, R3);       /* R3 = B7FFB6FFB5FFB4FF */
				R0 = R4;                              /* R0 = R4               */
				R0 = _mm_unpacklo_epi16(R2, R0);      /* R0 = R1G1B1FFR0G0B0FF */
				R4 = _mm_unpackhi_epi16(R2, R4);      /* R4 = R3G3B3FFR2G2B2FF */
				R2 = R3;                              /* R2 = R3               */
				R2 = _mm_unpacklo_epi16(R1, R2);      /* R2 = R5G5B5FFR4G4B4FF */
				R3 = _mm_unpackhi_epi16(R1, R3);      /* R3 = R7G7B7FFR6G6B6FF */
				_mm_store_si128((__m128i*)d_buf, R0); /* R1G1B1FFR0G0B0FF      */
				d_buf += sizeof(__m128i);
				_mm_store_si128((__m128i*)d_buf, R4); /* R3G3B3FFR2G2B2FF      */
				d_buf += sizeof(__m128i);
				_mm_store_si128((__m128i*)d_buf, R2); /* R5G5B5FFR4G4B4FF      */
				d_buf += sizeof(__m128i);
				_mm_store_si128((__m128i*)d_buf, R3); /* R7G7B7FFR6G6B6FF      */
				d_buf += sizeof(__m128i);
			}
		}

		for (i = 0; i < pad; i++)
		{
			const INT32 divisor = 16;
			const INT32 Y = ((*y_buf++) + 4096) << divisor;
			const INT32 Cb = (*cb_buf++);
			const INT32 Cr = (*cr_buf++);
			const INT32 CrR = Cr * (INT32)(1.402525f * (1 << divisor));
			const INT32 CrG = Cr * (INT32)(0.714401f * (1 << divisor));
			const INT32 CbG = Cb * (INT32)(0.343730f * (1 << divisor));
			const INT32 CbB = Cb * (INT32)(1.769905f * (1 << divisor));
			const INT16 R = ((INT16)((CrR + Y) >> divisor) >> 5);
			const INT16 G = ((INT16)((Y - CbG - CrG) >> divisor) >> 5);
			const INT16 B = ((INT16)((CbB + Y) >> divisor) >> 5);
			*d_buf++ = CLIP(R);
			*d_buf++ = CLIP(G);
			*d_buf++ = CLIP(B);
			*d_buf++ = 0xFF;
		}

		d_buf += dstPad;
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t sse2_yCbCrToRGB_16s8u_P3AC4R(const INT16* const pSrc[3], UINT32 srcStep,
                                              BYTE* pDst, UINT32 dstStep, UINT32 DstFormat,
                                              const prim_size_t* roi) /* region of interest */
{
	if (((ULONG_PTR)(pSrc[0]) & 0x0f) || ((ULONG_PTR)(pSrc[1]) & 0x0f) ||
	    ((ULONG_PTR)(pSrc[2]) & 0x0f) || ((ULONG_PTR)(pDst)&0x0f) || (srcStep & 0x0f) ||
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
static pstatus_t sse2_RGBToYCbCr_16s16s_P3P3(const INT16* const pSrc[3], int srcStep,
                                             INT16* pDst[3], int dstStep,
                                             const prim_size_t* roi) /* region of interest */
{
	__m128i min, max, y_r, y_g, y_b, cb_r, cb_g, cb_b, cr_r, cr_g, cr_b;
	__m128i *r_buf, *g_buf, *b_buf, *y_buf, *cb_buf, *cr_buf;
	UINT32 yp;
	int srcbump, dstbump, imax;

	if (((ULONG_PTR)(pSrc[0]) & 0x0f) || ((ULONG_PTR)(pSrc[1]) & 0x0f) ||
	    ((ULONG_PTR)(pSrc[2]) & 0x0f) || ((ULONG_PTR)(pDst[0]) & 0x0f) ||
	    ((ULONG_PTR)(pDst[1]) & 0x0f) || ((ULONG_PTR)(pDst[2]) & 0x0f) || (roi->width & 0x07) ||
	    (srcStep & 127) || (dstStep & 127))
	{
		/* We can't maintain 16-byte alignment. */
		return generic->RGBToYCbCr_16s16s_P3P3(pSrc, srcStep, pDst, dstStep, roi);
	}

	min = _mm_set1_epi16(-128 * 32);
	max = _mm_set1_epi16(127 * 32);
	r_buf = (__m128i*)(pSrc[0]);
	g_buf = (__m128i*)(pSrc[1]);
	b_buf = (__m128i*)(pSrc[2]);
	y_buf = (__m128i*)(pDst[0]);
	cb_buf = (__m128i*)(pDst[1]);
	cr_buf = (__m128i*)(pDst[2]);
	y_r = _mm_set1_epi16(9798);    /*  0.299000 << 15 */
	y_g = _mm_set1_epi16(19235);   /*  0.587000 << 15 */
	y_b = _mm_set1_epi16(3735);    /*  0.114000 << 15 */
	cb_r = _mm_set1_epi16(-5535);  /* -0.168935 << 15 */
	cb_g = _mm_set1_epi16(-10868); /* -0.331665 << 15 */
	cb_b = _mm_set1_epi16(16403);  /*  0.500590 << 15 */
	cr_r = _mm_set1_epi16(16377);  /*  0.499813 << 15 */
	cr_g = _mm_set1_epi16(-13714); /* -0.418531 << 15 */
	cr_b = _mm_set1_epi16(-2663);  /* -0.081282 << 15 */
	srcbump = srcStep / sizeof(__m128i);
	dstbump = dstStep / sizeof(__m128i);
#ifdef DO_PREFETCH

	/* Prefetch RGB's. */
	for (yp = 0; yp < roi->height; yp++)
	{
		int i;

		for (i = 0; i < roi->width * sizeof(INT16) / sizeof(__m128i);
		     i += (CACHE_LINE_BYTES / sizeof(__m128i)))
		{
			_mm_prefetch((char*)(&r_buf[i]), _MM_HINT_NTA);
			_mm_prefetch((char*)(&g_buf[i]), _MM_HINT_NTA);
			_mm_prefetch((char*)(&b_buf[i]), _MM_HINT_NTA);
		}

		r_buf += srcbump;
		g_buf += srcbump;
		b_buf += srcbump;
	}

	r_buf = (__m128i*)(pSrc[0]);
	g_buf = (__m128i*)(pSrc[1]);
	b_buf = (__m128i*)(pSrc[2]);
#endif /* DO_PREFETCH */
	imax = roi->width * sizeof(INT16) / sizeof(__m128i);

	for (yp = 0; yp < roi->height; ++yp)
	{
		int i;

		for (i = 0; i < imax; i++)
		{
			/* In order to use SSE2 signed 16-bit integer multiplication we
			 * need to convert the floating point factors to signed int
			 * without loosing information.  The result of this multiplication
			 * is 32 bit and using SSE2 we get either the product's hi or lo
			 * word.  Thus we will multiply the factors by the highest
			 * possible 2^n and take the upper 16 bits of the signed 32-bit
			 * result (_mm_mulhi_epi16).  Since the final result needs to
			 * be scaled by << 5 and also in in order to keep the precision
			 * within the upper 16 bits we will also have to scale the RGB
			 * values used in the multiplication by << 5+(16-n).
			 */
			__m128i r, g, b, y, cb, cr;
			r = _mm_load_si128(y_buf + i);
			g = _mm_load_si128(g_buf + i);
			b = _mm_load_si128(b_buf + i);
			/* r<<6; g<<6; b<<6 */
			r = _mm_slli_epi16(r, 6);
			g = _mm_slli_epi16(g, 6);
			b = _mm_slli_epi16(b, 6);
			/* y = HIWORD(r*y_r) + HIWORD(g*y_g) + HIWORD(b*y_b) + min */
			y = _mm_mulhi_epi16(r, y_r);
			y = _mm_add_epi16(y, _mm_mulhi_epi16(g, y_g));
			y = _mm_add_epi16(y, _mm_mulhi_epi16(b, y_b));
			y = _mm_add_epi16(y, min);
			/* y_r_buf[i] = MINMAX(y, 0, (255 << 5)) - (128 << 5); */
			_mm_between_epi16(y, min, max);
			_mm_store_si128(y_buf + i, y);
			/* cb = HIWORD(r*cb_r) + HIWORD(g*cb_g) + HIWORD(b*cb_b) */
			cb = _mm_mulhi_epi16(r, cb_r);
			cb = _mm_add_epi16(cb, _mm_mulhi_epi16(g, cb_g));
			cb = _mm_add_epi16(cb, _mm_mulhi_epi16(b, cb_b));
			/* cb_g_buf[i] = MINMAX(cb, (-128 << 5), (127 << 5)); */
			_mm_between_epi16(cb, min, max);
			_mm_store_si128(cb_buf + i, cb);
			/* cr = HIWORD(r*cr_r) + HIWORD(g*cr_g) + HIWORD(b*cr_b) */
			cr = _mm_mulhi_epi16(r, cr_r);
			cr = _mm_add_epi16(cr, _mm_mulhi_epi16(g, cr_g));
			cr = _mm_add_epi16(cr, _mm_mulhi_epi16(b, cr_b));
			/* cr_b_buf[i] = MINMAX(cr, (-128 << 5), (127 << 5)); */
			_mm_between_epi16(cr, min, max);
			_mm_store_si128(cr_buf + i, cr);
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
static pstatus_t
sse2_RGBToRGB_16s8u_P3AC4R_BGRX(const INT16* const pSrc[3], /* 16-bit R,G, and B arrays */
                                UINT32 srcStep,             /* bytes between rows in source data */
                                BYTE* pDst,             /* 32-bit interleaved ARGB (ABGR?) data */
                                UINT32 dstStep,         /* bytes between rows in dest data */
                                const prim_size_t* roi) /* region of interest */
{
	const UINT16* pr = (const UINT16*)(pSrc[0]);
	const UINT16* pg = (const UINT16*)(pSrc[1]);
	const UINT16* pb = (const UINT16*)(pSrc[2]);
	const UINT32 pad = roi->width % 16;
	const __m128i a = _mm_set1_epi32(0xFFFFFFFFU);
	BYTE* out;
	UINT32 srcbump, dstbump, y;
	out = (BYTE*)pDst;
	srcbump = (srcStep - (roi->width * sizeof(UINT16))) / sizeof(UINT16);
	dstbump = (dstStep - (roi->width * sizeof(UINT32)));

	for (y = 0; y < roi->height; ++y)
	{
		UINT32 x;

		for (x = 0; x < roi->width - pad; x += 16)
		{
			__m128i r, g, b;
			/* The comments below pretend these are 8-byte registers
			 * rather than 16-byte, for readability.
			 */
			{
				__m128i R0, R1;
				R0 = _mm_load_si128((__m128i*)pb);
				pb += 8; /* R0 = 00B300B200B100B0 */
				R1 = _mm_load_si128((__m128i*)pb);
				pb += 8;                      /* R1 = 00B700B600B500B4 */
				b = _mm_packus_epi16(R0, R1); /* b = B7B6B5B4B3B2B1B0 */
			}
			{
				__m128i R0, R1;
				R0 = _mm_load_si128((__m128i*)pg);
				pg += 8; /* R1 = 00G300G200G100G0 */
				R1 = _mm_load_si128((__m128i*)pg);
				pg += 8;                      /* R2 = 00G700G600G500G4 */
				g = _mm_packus_epi16(R0, R1); /* g = G7G6G5G4G3G2G1G0 */
			}
			{
				__m128i R0, R1;
				R0 = _mm_load_si128((__m128i*)pr);
				pr += 8; /* R0 = 00R300R200R100R0 */
				R1 = _mm_load_si128((__m128i*)pr);
				pr += 8;                      /* R3 = 00R700R600R500R4 */
				r = _mm_packus_epi16(R0, R1); /* r = R7R6R5R4R3R2R1R0 */
			}
			{
				__m128i gbHi, gbLo, arHi, arLo;
				{
					gbLo = _mm_unpacklo_epi8(b, g); /* R0 = G7G6G5G4G3G2G1G0 */
					gbHi = _mm_unpackhi_epi8(b, g); /* R1 = G7B7G6B7G5B5G4B4 */
					arLo = _mm_unpacklo_epi8(r, a); /* R4 = FFR3FFR2FFR1FFR0 */
					arHi = _mm_unpackhi_epi8(r, a); /* R3 = FFR7FFR6FFR5FFR4 */
				}
				{
					const __m128i bgrx = _mm_unpacklo_epi16(gbLo, arLo);
					_mm_store_si128((__m128i*)out, bgrx);
					out += 16; /* FFR1G1B1FFR0G0B0      */
				}
				{
					const __m128i bgrx = _mm_unpackhi_epi16(gbLo, arLo);
					_mm_store_si128((__m128i*)out, bgrx);
					out += 16; /* FFR3G3B3FFR2G2B2      */
				}
				{
					const __m128i bgrx = _mm_unpacklo_epi16(gbHi, arHi);
					_mm_store_si128((__m128i*)out, bgrx);
					out += 16; /* FFR5G5B5FFR4G4B4      */
				}
				{
					const __m128i bgrx = _mm_unpackhi_epi16(gbHi, arHi);
					_mm_store_si128((__m128i*)out, bgrx);
					out += 16; /* FFR7G7B7FFR6G6B6      */
				}
			}
		}

		for (x = 0; x < pad; x++)
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

static pstatus_t
sse2_RGBToRGB_16s8u_P3AC4R_RGBX(const INT16* const pSrc[3], /* 16-bit R,G, and B arrays */
                                UINT32 srcStep,             /* bytes between rows in source data */
                                BYTE* pDst,             /* 32-bit interleaved ARGB (ABGR?) data */
                                UINT32 dstStep,         /* bytes between rows in dest data */
                                const prim_size_t* roi) /* region of interest */
{
	const UINT16* pr = (const UINT16*)(pSrc[0]);
	const UINT16* pg = (const UINT16*)(pSrc[1]);
	const UINT16* pb = (const UINT16*)(pSrc[2]);
	const UINT32 pad = roi->width % 16;
	const __m128i a = _mm_set1_epi32(0xFFFFFFFFU);
	BYTE* out;
	UINT32 srcbump, dstbump, y;
	out = (BYTE*)pDst;
	srcbump = (srcStep - (roi->width * sizeof(UINT16))) / sizeof(UINT16);
	dstbump = (dstStep - (roi->width * sizeof(UINT32)));

	for (y = 0; y < roi->height; ++y)
	{
		UINT32 x;

		for (x = 0; x < roi->width - pad; x += 16)
		{
			__m128i r, g, b;
			/* The comments below pretend these are 8-byte registers
			 * rather than 16-byte, for readability.
			 */
			{
				__m128i R0, R1;
				R0 = _mm_load_si128((__m128i*)pb);
				pb += 8; /* R0 = 00B300B200B100B0 */
				R1 = _mm_load_si128((__m128i*)pb);
				pb += 8;                      /* R1 = 00B700B600B500B4 */
				b = _mm_packus_epi16(R0, R1); /* b = B7B6B5B4B3B2B1B0 */
			}
			{
				__m128i R0, R1;
				R0 = _mm_load_si128((__m128i*)pg);
				pg += 8; /* R1 = 00G300G200G100G0 */
				R1 = _mm_load_si128((__m128i*)pg);
				pg += 8;                      /* R2 = 00G700G600G500G4 */
				g = _mm_packus_epi16(R0, R1); /* g = G7G6G5G4G3G2G1G0 */
			}
			{
				__m128i R0, R1;
				R0 = _mm_load_si128((__m128i*)pr);
				pr += 8; /* R0 = 00R300R200R100R0 */
				R1 = _mm_load_si128((__m128i*)pr);
				pr += 8;                      /* R3 = 00R700R600R500R4 */
				r = _mm_packus_epi16(R0, R1); /* r = R7R6R5R4R3R2R1R0 */
			}
			{
				__m128i gbHi, gbLo, arHi, arLo;
				{
					gbLo = _mm_unpacklo_epi8(r, g); /* R0 = G7G6G5G4G3G2G1G0 */
					gbHi = _mm_unpackhi_epi8(r, g); /* R1 = G7B7G6B7G5B5G4B4 */
					arLo = _mm_unpacklo_epi8(b, a); /* R4 = FFR3FFR2FFR1FFR0 */
					arHi = _mm_unpackhi_epi8(b, a); /* R3 = FFR7FFR6FFR5FFR4 */
				}
				{
					const __m128i bgrx = _mm_unpacklo_epi16(gbLo, arLo);
					_mm_store_si128((__m128i*)out, bgrx);
					out += 16; /* FFR1G1B1FFR0G0B0      */
				}
				{
					const __m128i bgrx = _mm_unpackhi_epi16(gbLo, arLo);
					_mm_store_si128((__m128i*)out, bgrx);
					out += 16; /* FFR3G3B3FFR2G2B2      */
				}
				{
					const __m128i bgrx = _mm_unpacklo_epi16(gbHi, arHi);
					_mm_store_si128((__m128i*)out, bgrx);
					out += 16; /* FFR5G5B5FFR4G4B4      */
				}
				{
					const __m128i bgrx = _mm_unpackhi_epi16(gbHi, arHi);
					_mm_store_si128((__m128i*)out, bgrx);
					out += 16; /* FFR7G7B7FFR6G6B6      */
				}
			}
		}

		for (x = 0; x < pad; x++)
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

static pstatus_t
sse2_RGBToRGB_16s8u_P3AC4R_XBGR(const INT16* const pSrc[3], /* 16-bit R,G, and B arrays */
                                UINT32 srcStep,             /* bytes between rows in source data */
                                BYTE* pDst,             /* 32-bit interleaved ARGB (ABGR?) data */
                                UINT32 dstStep,         /* bytes between rows in dest data */
                                const prim_size_t* roi) /* region of interest */
{
	const UINT16* pr = (const UINT16*)(pSrc[0]);
	const UINT16* pg = (const UINT16*)(pSrc[1]);
	const UINT16* pb = (const UINT16*)(pSrc[2]);
	const UINT32 pad = roi->width % 16;
	const __m128i a = _mm_set1_epi32(0xFFFFFFFFU);
	BYTE* out;
	UINT32 srcbump, dstbump, y;
	out = (BYTE*)pDst;
	srcbump = (srcStep - (roi->width * sizeof(UINT16))) / sizeof(UINT16);
	dstbump = (dstStep - (roi->width * sizeof(UINT32)));

	for (y = 0; y < roi->height; ++y)
	{
		UINT32 x;

		for (x = 0; x < roi->width - pad; x += 16)
		{
			__m128i r, g, b;
			/* The comments below pretend these are 8-byte registers
			 * rather than 16-byte, for readability.
			 */
			{
				__m128i R0, R1;
				R0 = _mm_load_si128((__m128i*)pb);
				pb += 8; /* R0 = 00B300B200B100B0 */
				R1 = _mm_load_si128((__m128i*)pb);
				pb += 8;                      /* R1 = 00B700B600B500B4 */
				b = _mm_packus_epi16(R0, R1); /* b = B7B6B5B4B3B2B1B0 */
			}
			{
				__m128i R0, R1;
				R0 = _mm_load_si128((__m128i*)pg);
				pg += 8; /* R1 = 00G300G200G100G0 */
				R1 = _mm_load_si128((__m128i*)pg);
				pg += 8;                      /* R2 = 00G700G600G500G4 */
				g = _mm_packus_epi16(R0, R1); /* g = G7G6G5G4G3G2G1G0 */
			}
			{
				__m128i R0, R1;
				R0 = _mm_load_si128((__m128i*)pr);
				pr += 8; /* R0 = 00R300R200R100R0 */
				R1 = _mm_load_si128((__m128i*)pr);
				pr += 8;                      /* R3 = 00R700R600R500R4 */
				r = _mm_packus_epi16(R0, R1); /* r = R7R6R5R4R3R2R1R0 */
			}
			{
				__m128i gbHi, gbLo, arHi, arLo;
				{
					gbLo = _mm_unpacklo_epi8(a, b); /* R0 = G7G6G5G4G3G2G1G0 */
					gbHi = _mm_unpackhi_epi8(a, b); /* R1 = G7B7G6B7G5B5G4B4 */
					arLo = _mm_unpacklo_epi8(g, r); /* R4 = FFR3FFR2FFR1FFR0 */
					arHi = _mm_unpackhi_epi8(g, r); /* R3 = FFR7FFR6FFR5FFR4 */
				}
				{
					const __m128i bgrx = _mm_unpacklo_epi16(gbLo, arLo);
					_mm_store_si128((__m128i*)out, bgrx);
					out += 16; /* FFR1G1B1FFR0G0B0      */
				}
				{
					const __m128i bgrx = _mm_unpackhi_epi16(gbLo, arLo);
					_mm_store_si128((__m128i*)out, bgrx);
					out += 16; /* FFR3G3B3FFR2G2B2      */
				}
				{
					const __m128i bgrx = _mm_unpacklo_epi16(gbHi, arHi);
					_mm_store_si128((__m128i*)out, bgrx);
					out += 16; /* FFR5G5B5FFR4G4B4      */
				}
				{
					const __m128i bgrx = _mm_unpackhi_epi16(gbHi, arHi);
					_mm_store_si128((__m128i*)out, bgrx);
					out += 16; /* FFR7G7B7FFR6G6B6      */
				}
			}
		}

		for (x = 0; x < pad; x++)
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

static pstatus_t
sse2_RGBToRGB_16s8u_P3AC4R_XRGB(const INT16* const pSrc[3], /* 16-bit R,G, and B arrays */
                                UINT32 srcStep,             /* bytes between rows in source data */
                                BYTE* pDst,             /* 32-bit interleaved ARGB (ABGR?) data */
                                UINT32 dstStep,         /* bytes between rows in dest data */
                                const prim_size_t* roi) /* region of interest */
{
	const UINT16* pr = (const UINT16*)(pSrc[0]);
	const UINT16* pg = (const UINT16*)(pSrc[1]);
	const UINT16* pb = (const UINT16*)(pSrc[2]);
	const __m128i a = _mm_set1_epi32(0xFFFFFFFFU);
	const UINT32 pad = roi->width % 16;
	BYTE* out;
	UINT32 srcbump, dstbump, y;
	out = (BYTE*)pDst;
	srcbump = (srcStep - (roi->width * sizeof(UINT16))) / sizeof(UINT16);
	dstbump = (dstStep - (roi->width * sizeof(UINT32)));

	for (y = 0; y < roi->height; ++y)
	{
		UINT32 x;

		for (x = 0; x < roi->width - pad; x += 16)
		{
			__m128i r, g, b;
			/* The comments below pretend these are 8-byte registers
			 * rather than 16-byte, for readability.
			 */
			{
				__m128i R0, R1;
				R0 = _mm_load_si128((__m128i*)pb);
				pb += 8; /* R0 = 00B300B200B100B0 */
				R1 = _mm_load_si128((__m128i*)pb);
				pb += 8;                      /* R1 = 00B700B600B500B4 */
				b = _mm_packus_epi16(R0, R1); /* b = B7B6B5B4B3B2B1B0 */
			}
			{
				__m128i R0, R1;
				R0 = _mm_load_si128((__m128i*)pg);
				pg += 8; /* R1 = 00G300G200G100G0 */
				R1 = _mm_load_si128((__m128i*)pg);
				pg += 8;                      /* R2 = 00G700G600G500G4 */
				g = _mm_packus_epi16(R0, R1); /* g = G7G6G5G4G3G2G1G0 */
			}
			{
				__m128i R0, R1;
				R0 = _mm_load_si128((__m128i*)pr);
				pr += 8; /* R0 = 00R300R200R100R0 */
				R1 = _mm_load_si128((__m128i*)pr);
				pr += 8;                      /* R3 = 00R700R600R500R4 */
				r = _mm_packus_epi16(R0, R1); /* r = R7R6R5R4R3R2R1R0 */
			}
			{
				__m128i gbHi, gbLo, arHi, arLo;
				{
					gbLo = _mm_unpacklo_epi8(a, r); /* R0 = G7G6G5G4G3G2G1G0 */
					gbHi = _mm_unpackhi_epi8(a, r); /* R1 = G7B7G6B7G5B5G4B4 */
					arLo = _mm_unpacklo_epi8(g, b); /* R4 = FFR3FFR2FFR1FFR0 */
					arHi = _mm_unpackhi_epi8(g, b); /* R3 = FFR7FFR6FFR5FFR4 */
				}
				{
					const __m128i bgrx = _mm_unpacklo_epi16(gbLo, arLo);
					_mm_store_si128((__m128i*)out, bgrx);
					out += 16; /* FFR1G1B1FFR0G0B0      */
				}
				{
					const __m128i bgrx = _mm_unpackhi_epi16(gbLo, arLo);
					_mm_store_si128((__m128i*)out, bgrx);
					out += 16; /* FFR3G3B3FFR2G2B2      */
				}
				{
					const __m128i bgrx = _mm_unpacklo_epi16(gbHi, arHi);
					_mm_store_si128((__m128i*)out, bgrx);
					out += 16; /* FFR5G5B5FFR4G4B4      */
				}
				{
					const __m128i bgrx = _mm_unpackhi_epi16(gbHi, arHi);
					_mm_store_si128((__m128i*)out, bgrx);
					out += 16; /* FFR7G7B7FFR6G6B6      */
				}
			}
		}

		for (x = 0; x < pad; x++)
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
sse2_RGBToRGB_16s8u_P3AC4R(const INT16* const pSrc[3], /* 16-bit R,G, and B arrays */
                           UINT32 srcStep,             /* bytes between rows in source data */
                           BYTE* pDst,                 /* 32-bit interleaved ARGB (ABGR?) data */
                           UINT32 dstStep,             /* bytes between rows in dest data */
                           UINT32 DstFormat, const prim_size_t* roi)
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
#endif /* WITH_SSE2 */

/*---------------------------------------------------------------------------*/
#ifdef WITH_NEON
static pstatus_t neon_yCbCrToRGB_16s16s_P3P3(const INT16* const pSrc[3], INT32 srcStep,
                                             INT16* pDst[3], INT32 dstStep,
                                             const prim_size_t* roi) /* region of interest */
{
	/* TODO: If necessary, check alignments and call the general version. */
	int16x8_t zero = vdupq_n_s16(0);
	int16x8_t max = vdupq_n_s16(255);
	int16x8_t r_cr = vdupq_n_s16(22986);  //  1.403 << 14
	int16x8_t g_cb = vdupq_n_s16(-5636);  // -0.344 << 14
	int16x8_t g_cr = vdupq_n_s16(-11698); // -0.714 << 14
	int16x8_t b_cb = vdupq_n_s16(28999);  //  1.770 << 14
	int16x8_t c4096 = vdupq_n_s16(4096);
	int16x8_t* y_buf = (int16x8_t*)pSrc[0];
	int16x8_t* cb_buf = (int16x8_t*)pSrc[1];
	int16x8_t* cr_buf = (int16x8_t*)pSrc[2];
	int16x8_t* r_buf = (int16x8_t*)pDst[0];
	int16x8_t* g_buf = (int16x8_t*)pDst[1];
	int16x8_t* b_buf = (int16x8_t*)pDst[2];
	int srcbump = srcStep / sizeof(int16x8_t);
	int dstbump = dstStep / sizeof(int16x8_t);
	int yp;
	int imax = roi->width * sizeof(INT16) / sizeof(int16x8_t);

	for (yp = 0; yp < roi->height; ++yp)
	{
		int i;

		for (i = 0; i < imax; i++)
		{
			/*
			    In order to use NEON signed 16-bit integer multiplication we need to convert
			    the floating point factors to signed int without loosing information.
			    The result of this multiplication is 32 bit and we have a NEON instruction
			    that returns the hi word of the saturated double.
			    Thus we will multiply the factors by the highest possible 2^n, take the
			    upper 16 bits of the signed 32-bit result (vqdmulhq_s16 followed by a right
			    shift by 1 to reverse the doubling) and correct	this result by multiplying it
			    by 2^(16-n).
			    For the given factors in the conversion matrix the best possible n is 14.

			    Example for calculating r:
			    r = (y>>5) + 128 + (cr*1.403)>>5                       // our base formula
			    r = (y>>5) + 128 + (HIWORD(cr*(1.403<<14)<<2))>>5      // see above
			    r = (y+4096)>>5 + (HIWORD(cr*22986)<<2)>>5             // simplification
			    r = ((y+4096)>>2 + HIWORD(cr*22986)) >> 3
			*/
			/* y = (y_buf[i] + 4096) >> 2 */
			int16x8_t y = vld1q_s16((INT16*)&y_buf[i]);
			y = vaddq_s16(y, c4096);
			y = vshrq_n_s16(y, 2);
			/* cb = cb_buf[i]; */
			int16x8_t cb = vld1q_s16((INT16*)&cb_buf[i]);
			/* cr = cr_buf[i]; */
			int16x8_t cr = vld1q_s16((INT16*)&cr_buf[i]);
			/* (y + HIWORD(cr*22986)) >> 3 */
			int16x8_t r = vaddq_s16(y, vshrq_n_s16(vqdmulhq_s16(cr, r_cr), 1));
			r = vshrq_n_s16(r, 3);
			/* r_buf[i] = CLIP(r); */
			r = vminq_s16(vmaxq_s16(r, zero), max);
			vst1q_s16((INT16*)&r_buf[i], r);
			/* (y + HIWORD(cb*-5636) + HIWORD(cr*-11698)) >> 3 */
			int16x8_t g = vaddq_s16(y, vshrq_n_s16(vqdmulhq_s16(cb, g_cb), 1));
			g = vaddq_s16(g, vshrq_n_s16(vqdmulhq_s16(cr, g_cr), 1));
			g = vshrq_n_s16(g, 3);
			/* g_buf[i] = CLIP(g); */
			g = vminq_s16(vmaxq_s16(g, zero), max);
			vst1q_s16((INT16*)&g_buf[i], g);
			/* (y + HIWORD(cb*28999)) >> 3 */
			int16x8_t b = vaddq_s16(y, vshrq_n_s16(vqdmulhq_s16(cb, b_cb), 1));
			b = vshrq_n_s16(b, 3);
			/* b_buf[i] = CLIP(b); */
			b = vminq_s16(vmaxq_s16(b, zero), max);
			vst1q_s16((INT16*)&b_buf[i], b);
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

static pstatus_t neon_yCbCrToRGB_16s8u_P3AC4R_X(const INT16* const pSrc[3], UINT32 srcStep,
                                                BYTE* pDst, UINT32 dstStep, const prim_size_t* roi,
                                                uint8_t rPos, uint8_t gPos, uint8_t bPos,
                                                uint8_t aPos)
{
	UINT32 x, y;
	BYTE* pRGB = pDst;
	const INT16* pY = pSrc[0];
	const INT16* pCb = pSrc[1];
	const INT16* pCr = pSrc[2];
	const size_t srcPad = (srcStep - (roi->width * sizeof(INT16))) / sizeof(INT16);
	const size_t dstPad = (dstStep - (roi->width * 4)) / 4;
	const size_t pad = roi->width % 8;
	const int16x4_t c4096 = vdup_n_s16(4096);

	for (y = 0; y < roi->height; y++)
	{
		for (x = 0; x < roi->width - pad; x += 8)
		{
			const int16x8_t Y = vld1q_s16(pY);
			const int16x4_t Yh = vget_high_s16(Y);
			const int16x4_t Yl = vget_low_s16(Y);
			const int32x4_t YhAdd = vaddl_s16(Yh, c4096); /* Y + 4096 */
			const int32x4_t YlAdd = vaddl_s16(Yl, c4096); /* Y + 4096 */
			const int32x4_t YhW = vshlq_n_s32(YhAdd, 16);
			const int32x4_t YlW = vshlq_n_s32(YlAdd, 16);
			const int16x8_t Cr = vld1q_s16(pCr);
			const int16x4_t Crh = vget_high_s16(Cr);
			const int16x4_t Crl = vget_low_s16(Cr);
			const int16x8_t Cb = vld1q_s16(pCb);
			const int16x4_t Cbh = vget_high_s16(Cb);
			const int16x4_t Cbl = vget_low_s16(Cb);
			uint8x8x4_t bgrx;
			{
				/* R */
				const int32x4_t CrhR = vmulq_n_s32(vmovl_s16(Crh), 91916); /* 1.402525 * 2^16 */
				const int32x4_t CrlR = vmulq_n_s32(vmovl_s16(Crl), 91916); /* 1.402525 * 2^16 */
				const int32x4_t CrhRa = vaddq_s32(CrhR, YhW);
				const int32x4_t CrlRa = vaddq_s32(CrlR, YlW);
				const int16x4_t Rsh = vmovn_s32(vshrq_n_s32(CrhRa, 21));
				const int16x4_t Rsl = vmovn_s32(vshrq_n_s32(CrlRa, 21));
				const int16x8_t Rs = vcombine_s16(Rsl, Rsh);
				bgrx.val[rPos] = vqmovun_s16(Rs);
			}
			{
				/* G */
				const int32x4_t CbGh = vmull_n_s16(Cbh, 22527);            /* 0.343730 * 2^16 */
				const int32x4_t CbGl = vmull_n_s16(Cbl, 22527);            /* 0.343730 * 2^16 */
				const int32x4_t CrGh = vmulq_n_s32(vmovl_s16(Crh), 46819); /* 0.714401 * 2^16 */
				const int32x4_t CrGl = vmulq_n_s32(vmovl_s16(Crl), 46819); /* 0.714401 * 2^16 */
				const int32x4_t CbCrGh = vaddq_s32(CbGh, CrGh);
				const int32x4_t CbCrGl = vaddq_s32(CbGl, CrGl);
				const int32x4_t YCbCrGh = vsubq_s32(YhW, CbCrGh);
				const int32x4_t YCbCrGl = vsubq_s32(YlW, CbCrGl);
				const int16x4_t Gsh = vmovn_s32(vshrq_n_s32(YCbCrGh, 21));
				const int16x4_t Gsl = vmovn_s32(vshrq_n_s32(YCbCrGl, 21));
				const int16x8_t Gs = vcombine_s16(Gsl, Gsh);
				const uint8x8_t G = vqmovun_s16(Gs);
				bgrx.val[gPos] = G;
			}
			{
				/* B */
				const int32x4_t CbBh = vmulq_n_s32(vmovl_s16(Cbh), 115992); /* 1.769905 * 2^16 */
				const int32x4_t CbBl = vmulq_n_s32(vmovl_s16(Cbl), 115992); /* 1.769905 * 2^16 */
				const int32x4_t YCbBh = vaddq_s32(CbBh, YhW);
				const int32x4_t YCbBl = vaddq_s32(CbBl, YlW);
				const int16x4_t Bsh = vmovn_s32(vshrq_n_s32(YCbBh, 21));
				const int16x4_t Bsl = vmovn_s32(vshrq_n_s32(YCbBl, 21));
				const int16x8_t Bs = vcombine_s16(Bsl, Bsh);
				const uint8x8_t B = vqmovun_s16(Bs);
				bgrx.val[bPos] = B;
			}
			/* A */
			{
				bgrx.val[aPos] = vdup_n_u8(0xFF);
			}
			vst4_u8(pRGB, bgrx);
			pY += 8;
			pCb += 8;
			pCr += 8;
			pRGB += 32;
		}

		for (x = 0; x < pad; x++)
		{
			const INT32 divisor = 16;
			const INT32 Y = ((*pY++) + 4096) << divisor;
			const INT32 Cb = (*pCb++);
			const INT32 Cr = (*pCr++);
			const INT32 CrR = Cr * (INT32)(1.402525f * (1 << divisor));
			const INT32 CrG = Cr * (INT32)(0.714401f * (1 << divisor));
			const INT32 CbG = Cb * (INT32)(0.343730f * (1 << divisor));
			const INT32 CbB = Cb * (INT32)(1.769905f * (1 << divisor));
			INT16 R = ((INT16)((CrR + Y) >> divisor) >> 5);
			INT16 G = ((INT16)((Y - CbG - CrG) >> divisor) >> 5);
			INT16 B = ((INT16)((CbB + Y) >> divisor) >> 5);
			BYTE bgrx[4];
			bgrx[bPos] = CLIP(B);
			bgrx[gPos] = CLIP(G);
			bgrx[rPos] = CLIP(R);
			bgrx[aPos] = 0xFF;
			*pRGB++ = bgrx[0];
			*pRGB++ = bgrx[1];
			*pRGB++ = bgrx[2];
			*pRGB++ = bgrx[3];
		}

		pY += srcPad;
		pCb += srcPad;
		pCr += srcPad;
		pRGB += dstPad;
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t neon_yCbCrToRGB_16s8u_P3AC4R(const INT16* const pSrc[3], UINT32 srcStep,
                                              BYTE* pDst, UINT32 dstStep, UINT32 DstFormat,
                                              const prim_size_t* roi)
{
	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return neon_yCbCrToRGB_16s8u_P3AC4R_X(pSrc, srcStep, pDst, dstStep, roi, 2, 1, 0, 3);

		case PIXEL_FORMAT_RGBA32:
		case PIXEL_FORMAT_RGBX32:
			return neon_yCbCrToRGB_16s8u_P3AC4R_X(pSrc, srcStep, pDst, dstStep, roi, 0, 1, 2, 3);

		case PIXEL_FORMAT_ARGB32:
		case PIXEL_FORMAT_XRGB32:
			return neon_yCbCrToRGB_16s8u_P3AC4R_X(pSrc, srcStep, pDst, dstStep, roi, 1, 2, 3, 0);

		case PIXEL_FORMAT_ABGR32:
		case PIXEL_FORMAT_XBGR32:
			return neon_yCbCrToRGB_16s8u_P3AC4R_X(pSrc, srcStep, pDst, dstStep, roi, 3, 2, 1, 0);

		default:
			return generic->yCbCrToRGB_16s8u_P3AC4R(pSrc, srcStep, pDst, dstStep, DstFormat, roi);
	}
}

static pstatus_t
neon_RGBToRGB_16s8u_P3AC4R_X(const INT16* const pSrc[3], /* 16-bit R,G, and B arrays */
                             UINT32 srcStep,             /* bytes between rows in source data */
                             BYTE* pDst,                 /* 32-bit interleaved ARGB (ABGR?) data */
                             UINT32 dstStep,             /* bytes between rows in dest data */
                             const prim_size_t* roi,     /* region of interest */
                             uint8_t rPos, uint8_t gPos, uint8_t bPos, uint8_t aPos)
{
	UINT32 x, y;
	UINT32 pad = roi->width % 8;

	for (y = 0; y < roi->height; y++)
	{
		const INT16* pr = (INT16*)(((BYTE*)pSrc[0]) + y * srcStep);
		const INT16* pg = (INT16*)(((BYTE*)pSrc[1]) + y * srcStep);
		const INT16* pb = (INT16*)(((BYTE*)pSrc[2]) + y * srcStep);
		BYTE* dst = pDst + y * dstStep;

		for (x = 0; x < roi->width - pad; x += 8)
		{
			int16x8_t r = vld1q_s16(pr);
			int16x8_t g = vld1q_s16(pg);
			int16x8_t b = vld1q_s16(pb);
			uint8x8x4_t bgrx;
			bgrx.val[aPos] = vdup_n_u8(0xFF);
			bgrx.val[rPos] = vqmovun_s16(r);
			bgrx.val[gPos] = vqmovun_s16(g);
			bgrx.val[bPos] = vqmovun_s16(b);
			vst4_u8(dst, bgrx);
			pr += 8;
			pg += 8;
			pb += 8;
			dst += 32;
		}

		for (x = 0; x < pad; x++)
		{
			BYTE bgrx[4];
			bgrx[bPos] = *pb++;
			bgrx[gPos] = *pg++;
			bgrx[rPos] = *pr++;
			bgrx[aPos] = 0xFF;
			*dst++ = bgrx[0];
			*dst++ = bgrx[1];
			*dst++ = bgrx[2];
			*dst++ = bgrx[3];
		}
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t
neon_RGBToRGB_16s8u_P3AC4R(const INT16* const pSrc[3], /* 16-bit R,G, and B arrays */
                           UINT32 srcStep,             /* bytes between rows in source data */
                           BYTE* pDst,                 /* 32-bit interleaved ARGB (ABGR?) data */
                           UINT32 dstStep,             /* bytes between rows in dest data */
                           UINT32 DstFormat, const prim_size_t* roi) /* region of interest */
{
	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return neon_RGBToRGB_16s8u_P3AC4R_X(pSrc, srcStep, pDst, dstStep, roi, 2, 1, 0, 3);

		case PIXEL_FORMAT_RGBA32:
		case PIXEL_FORMAT_RGBX32:
			return neon_RGBToRGB_16s8u_P3AC4R_X(pSrc, srcStep, pDst, dstStep, roi, 0, 1, 2, 3);

		case PIXEL_FORMAT_ARGB32:
		case PIXEL_FORMAT_XRGB32:
			return neon_RGBToRGB_16s8u_P3AC4R_X(pSrc, srcStep, pDst, dstStep, roi, 1, 2, 3, 0);

		case PIXEL_FORMAT_ABGR32:
		case PIXEL_FORMAT_XBGR32:
			return neon_RGBToRGB_16s8u_P3AC4R_X(pSrc, srcStep, pDst, dstStep, roi, 3, 2, 1, 0);

		default:
			return generic->RGBToRGB_16s8u_P3AC4R(pSrc, srcStep, pDst, dstStep, DstFormat, roi);
	}
}
#endif /* WITH_NEON */
/* I don't see a direct IPP version of this, since the input is INT16
 * YCbCr.  It may be possible via  Deinterleave and then YCbCrToRGB_<mod>.
 * But that would likely be slower.
 */

/* ------------------------------------------------------------------------- */
void primitives_init_colors_opt(primitives_t* prims)
{
	generic = primitives_get_generic();
	primitives_init_colors(prims);
#if defined(WITH_SSE2)

	if (IsProcessorFeaturePresent(PF_SSE2_INSTRUCTIONS_AVAILABLE))
	{
		prims->RGBToRGB_16s8u_P3AC4R = sse2_RGBToRGB_16s8u_P3AC4R;
		prims->yCbCrToRGB_16s16s_P3P3 = sse2_yCbCrToRGB_16s16s_P3P3;
		prims->yCbCrToRGB_16s8u_P3AC4R = sse2_yCbCrToRGB_16s8u_P3AC4R;
		prims->RGBToYCbCr_16s16s_P3P3 = sse2_RGBToYCbCr_16s16s_P3P3;
	}

#elif defined(WITH_NEON)

	if (IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE))
	{
		prims->RGBToRGB_16s8u_P3AC4R = neon_RGBToRGB_16s8u_P3AC4R;
		prims->yCbCrToRGB_16s8u_P3AC4R = neon_yCbCrToRGB_16s8u_P3AC4R;
		prims->yCbCrToRGB_16s16s_P3P3 = neon_yCbCrToRGB_16s16s_P3P3;
	}

#endif /* WITH_SSE2 */
}
