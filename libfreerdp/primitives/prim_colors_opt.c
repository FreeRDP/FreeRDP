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
#include "prim_colors.h"

#ifdef WITH_SSE2

#ifdef __GNUC__
# define GNU_INLINE \
	__attribute__((__gnu_inline__, __always_inline__, __artificial__))
#else
# define GNU_INLINE
#endif

#define CACHE_LINE_BYTES	64

#define _mm_between_epi16(_val, _min, _max) \
	do { _val = _mm_min_epi16(_max, _mm_max_epi16(_val, _min)); } while (0)

#ifdef DO_PREFETCH
/*---------------------------------------------------------------------------*/
static inline void GNU_INLINE _mm_prefetch_buffer(
	char * buffer, 
	int num_bytes)
{
	__m128i * buf = (__m128i*) buffer;
	unsigned int i;
	for (i = 0; i < (num_bytes / sizeof(__m128i)); 
		i+=(CACHE_LINE_BYTES / sizeof(__m128i)))
	{
		_mm_prefetch((char*)(&buf[i]), _MM_HINT_NTA);
	}
}
#endif /* DO_PREFETCH */

/*---------------------------------------------------------------------------*/
pstatus_t sse2_yCbCrToRGB_16s16s_P3P3(
	const INT16 *pSrc[3],
	int srcStep,
	INT16 *pDst[3],
	int dstStep,
	const prim_size_t *roi)	/* region of interest */
{
	__m128i zero, max, r_cr, g_cb, g_cr, b_cb, c4096;
	__m128i *y_buf, *cb_buf, *cr_buf, *r_buf, *g_buf, *b_buf;
	int srcbump, dstbump, yp, imax;

	if (((ULONG_PTR) (pSrc[0]) & 0x0f)
			|| ((ULONG_PTR) (pSrc[1]) & 0x0f)
			|| ((ULONG_PTR) (pSrc[2]) & 0x0f)
			|| ((ULONG_PTR) (pDst[0]) & 0x0f)
			|| ((ULONG_PTR) (pDst[1]) & 0x0f)
			|| ((ULONG_PTR) (pDst[2]) & 0x0f)
			|| (roi->width & 0x07)
			|| (srcStep & 127)
			|| (dstStep & 127))
	{
		/* We can't maintain 16-byte alignment. */
		return general_yCbCrToRGB_16s16s_P3P3(pSrc, srcStep,
			pDst, dstStep, roi);
	}

	zero = _mm_setzero_si128();
	max = _mm_set1_epi16(255);

	y_buf  = (__m128i*) (pSrc[0]);
	cb_buf = (__m128i*) (pSrc[1]);
	cr_buf = (__m128i*) (pSrc[2]);
	r_buf  = (__m128i*) (pDst[0]);
	g_buf  = (__m128i*) (pDst[1]);
	b_buf  = (__m128i*) (pDst[2]);

	r_cr = _mm_set1_epi16(22986);	/*  1.403 << 14 */
	g_cb = _mm_set1_epi16(-5636);	/* -0.344 << 14 */
	g_cr = _mm_set1_epi16(-11698);	/* -0.714 << 14 */
	b_cb = _mm_set1_epi16(28999);	/*  1.770 << 14 */
	c4096 = _mm_set1_epi16(4096);
	srcbump = srcStep / sizeof(__m128i);
	dstbump = dstStep / sizeof(__m128i);

#ifdef DO_PREFETCH
	/* Prefetch Y's, Cb's, and Cr's. */
	for (yp=0; yp<roi->height; yp++)
	{
		int i;
		for (i=0; i<roi->width * sizeof(INT16) / sizeof(__m128i);
			i += (CACHE_LINE_BYTES / sizeof(__m128i)))
		{
			_mm_prefetch((char*)(&y_buf[i]),  _MM_HINT_NTA);
			_mm_prefetch((char*)(&cb_buf[i]), _MM_HINT_NTA);
			_mm_prefetch((char*)(&cr_buf[i]), _MM_HINT_NTA);
		}
		y_buf  += srcbump;
		cb_buf += srcbump;
		cr_buf += srcbump;
	}
	y_buf  = (__m128i*) (pSrc[0]);
	cb_buf = (__m128i*) (pSrc[1]);
	cr_buf = (__m128i*) (pSrc[2]);
#endif /* DO_PREFETCH */

	imax = roi->width * sizeof(INT16) / sizeof(__m128i);
	for (yp=0; yp<roi->height; ++yp)
	{
		int i;
		for (i=0; i<imax; i++)
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

			/* r_buf[i] = MINMAX(r, 0, 255); */
			_mm_between_epi16(r, zero, max);
			_mm_store_si128(r_buf + i, r);

			/* (y + HIWORD(cb*-5636) + HIWORD(cr*-11698)) >> 3 */
			g = _mm_add_epi16(y, _mm_mulhi_epi16(cb, g_cb));
			g = _mm_add_epi16(g, _mm_mulhi_epi16(cr, g_cr));
			g = _mm_srai_epi16(g, 3);

			/* g_buf[i] = MINMAX(g, 0, 255); */
			_mm_between_epi16(g, zero, max);
			_mm_store_si128(g_buf + i, g);

			/* (y + HIWORD(cb*28999)) >> 3 */
			b = _mm_add_epi16(y, _mm_mulhi_epi16(cb, b_cb));
			b = _mm_srai_epi16(b, 3);
			/* b_buf[i] = MINMAX(b, 0, 255); */
			_mm_between_epi16(b, zero, max);
			_mm_store_si128(b_buf + i, b);
		}
		y_buf  += srcbump;
		cb_buf += srcbump;
		cr_buf += srcbump;
		r_buf += dstbump;
		g_buf += dstbump;
		b_buf += dstbump;
	}

	return PRIMITIVES_SUCCESS;
}

/*---------------------------------------------------------------------------*/
/* The encodec YCbCr coeffectients are represented as 11.5 fixed-point
 * numbers. See the general code above.
 */
pstatus_t sse2_RGBToYCbCr_16s16s_P3P3(
	const INT16 *pSrc[3],
	int srcStep,
	INT16 *pDst[3],
	int dstStep,
	const prim_size_t *roi)	/* region of interest */
{
	__m128i min, max, y_r, y_g, y_b, cb_r, cb_g, cb_b, cr_r, cr_g, cr_b;
	__m128i *r_buf, *g_buf, *b_buf, *y_buf, *cb_buf, *cr_buf;
	int srcbump, dstbump, yp, imax;

	if (((ULONG_PTR) (pSrc[0]) & 0x0f)
			|| ((ULONG_PTR) (pSrc[1]) & 0x0f)
			|| ((ULONG_PTR) (pSrc[2]) & 0x0f)
			|| ((ULONG_PTR) (pDst[0]) & 0x0f)
			|| ((ULONG_PTR) (pDst[1]) & 0x0f)
			|| ((ULONG_PTR) (pDst[2]) & 0x0f)
			|| (roi->width & 0x07)
			|| (srcStep & 127)
			|| (dstStep & 127))
	{
		/* We can't maintain 16-byte alignment. */
		return general_RGBToYCbCr_16s16s_P3P3(pSrc, srcStep,
			pDst, dstStep, roi);
	}

	min = _mm_set1_epi16(-128 * 32);
	max = _mm_set1_epi16(127 * 32);

	r_buf  = (__m128i*) (pSrc[0]);
	g_buf  = (__m128i*) (pSrc[1]);
	b_buf  = (__m128i*) (pSrc[2]);
	y_buf  = (__m128i*) (pDst[0]);
	cb_buf = (__m128i*) (pDst[1]);
	cr_buf = (__m128i*) (pDst[2]);

	y_r  = _mm_set1_epi16(9798);   /*  0.299000 << 15 */
	y_g  = _mm_set1_epi16(19235);  /*  0.587000 << 15 */
	y_b  = _mm_set1_epi16(3735);   /*  0.114000 << 15 */
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
	for (yp=0; yp<roi->height; yp++)
	{
		int i;
		for (i=0; i<roi->width * sizeof(INT16) / sizeof(__m128i);
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
	r_buf = (__m128i*) (pSrc[0]);
	g_buf = (__m128i*) (pSrc[1]);
	b_buf = (__m128i*) (pSrc[2]);
#endif /* DO_PREFETCH */

	imax = roi->width * sizeof(INT16) / sizeof(__m128i);
	for (yp=0; yp<roi->height; ++yp)
	{
		int i;
		for (i=0; i<imax; i++)
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
			r = _mm_load_si128(y_buf+i);
			g = _mm_load_si128(g_buf+i);
			b = _mm_load_si128(b_buf+i);

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
			_mm_store_si128(y_buf+i, y);

			/* cb = HIWORD(r*cb_r) + HIWORD(g*cb_g) + HIWORD(b*cb_b) */
			cb = _mm_mulhi_epi16(r, cb_r);
			cb = _mm_add_epi16(cb, _mm_mulhi_epi16(g, cb_g));
			cb = _mm_add_epi16(cb, _mm_mulhi_epi16(b, cb_b));
			/* cb_g_buf[i] = MINMAX(cb, (-128 << 5), (127 << 5)); */
			_mm_between_epi16(cb, min, max);
			_mm_store_si128(cb_buf+i, cb);

			/* cr = HIWORD(r*cr_r) + HIWORD(g*cr_g) + HIWORD(b*cr_b) */
			cr = _mm_mulhi_epi16(r, cr_r);
			cr = _mm_add_epi16(cr, _mm_mulhi_epi16(g, cr_g));
			cr = _mm_add_epi16(cr, _mm_mulhi_epi16(b, cr_b));
			/* cr_b_buf[i] = MINMAX(cr, (-128 << 5), (127 << 5)); */
			_mm_between_epi16(cr, min, max);
			_mm_store_si128(cr_buf+i, cr);
		}
		y_buf  += srcbump;
		cb_buf += srcbump;
		cr_buf += srcbump;
		r_buf += dstbump;
		g_buf += dstbump;
		b_buf += dstbump;
	}

	return PRIMITIVES_SUCCESS;
}

/*---------------------------------------------------------------------------*/
#define LOAD128(_src_) \
	_mm_load_si128((__m128i *) _src_)
#define STORE128(_dst_, _src_) \
	_mm_store_si128((__m128i *) _dst_, _src_)
#define PUNPCKLBW(_dst_, _src_) \
	_dst_ = _mm_unpacklo_epi8(_src_, _dst_)
#define PUNPCKHBW(_dst_, _src_) \
	_dst_ = _mm_unpackhi_epi8(_src_, _dst_)
#define PUNPCKLWD(_dst_, _src_) \
	_dst_ = _mm_unpacklo_epi16(_src_, _dst_)
#define PUNPCKHWD(_dst_, _src_) \
	_dst_ = _mm_unpackhi_epi16(_src_, _dst_)
#define PACKUSWB(_dst_, _src_) \
	_dst_ = _mm_packus_epi16(_dst_, _src_)
#define PREFETCH(_ptr_) \
	_mm_prefetch((const void *) _ptr_, _MM_HINT_T0)
#define XMM_ALL_ONES \
	_mm_set1_epi32(0xFFFFFFFFU)

pstatus_t sse2_RGBToRGB_16s8u_P3AC4R(
	const INT16 *pSrc[3],	/* 16-bit R,G, and B arrays */
	INT32 srcStep,			/* bytes between rows in source data */
	BYTE *pDst,				/* 32-bit interleaved ARGB (ABGR?) data */
	INT32 dstStep,			/* bytes between rows in dest data */
	const prim_size_t *roi)	/* region of interest */
{
	const UINT16 *r = (const UINT16 *) (pSrc[0]);
	const UINT16 *g = (const UINT16 *) (pSrc[1]);
	const UINT16 *b = (const UINT16 *) (pSrc[2]);
	BYTE *out;
	int srcbump, dstbump, y;

	/* Ensure 16-byte alignment on all pointers,
	 * that width is a multiple of 8,
	 * and that the next row will also remain aligned.
	 * Since this is usually used for 64x64 aligned arrays,
	 * these checks should presumably pass.
	 */
	if ((((ULONG_PTR) (pSrc[0]) & 0x0f) != 0)
			|| (((ULONG_PTR) (pSrc[1]) & 0x0f) != 0)
			|| (((ULONG_PTR) (pSrc[2]) & 0x0f) != 0)
			|| (((ULONG_PTR) pDst & 0x0f) != 0)
			|| (roi->width & 0x0f)
			|| (srcStep & 0x0f)
			|| (dstStep & 0x0f))
	{
		return general_RGBToRGB_16s8u_P3AC4R(pSrc, srcStep, pDst, dstStep, roi);
	}

	out = (BYTE *) pDst;
	srcbump = (srcStep - (roi->width * sizeof(UINT16))) / sizeof(UINT16);
	dstbump = (dstStep - (roi->width * sizeof(UINT32)));

	for (y=0; y<roi->height; ++y)
	{
		int width = roi->width;
		do {
			__m128i R0, R1, R2, R3, R4;
			/* The comments below pretend these are 8-byte registers
			 * rather than 16-byte, for readability.
			 */
			R0 = LOAD128(b);  b += 8;		/* R0 = 00B300B200B100B0 */
			R1 = LOAD128(b);  b += 8;		/* R1 = 00B700B600B500B4 */
			PACKUSWB(R0,R1);				/* R0 = B7B6B5B4B3B2B1B0 */
			R1 = LOAD128(g);  g += 8;		/* R1 = 00G300G200G100G0 */
			R2 = LOAD128(g);  g += 8;		/* R2 = 00G700G600G500G4 */
			PACKUSWB(R1,R2);				/* R1 = G7G6G5G4G3G2G1G0 */
			R2 = R1;						/* R2 = G7G6G5G4G3G2G1G0 */
			PUNPCKLBW(R2,R0);				/* R2 = G3B3G2B2G1B1G0B0 */
			PUNPCKHBW(R1,R0);				/* R1 = G7B7G6B7G5B5G4B4 */
			R0 = LOAD128(r);  r += 8;		/* R0 = 00R300R200R100R0 */
			R3 = LOAD128(r);  r += 8;		/* R3 = 00R700R600R500R4 */
			PACKUSWB(R0,R3);				/* R0 = R7R6R5R4R3R2R1R0 */
			R3 = XMM_ALL_ONES;				/* R3 = FFFFFFFFFFFFFFFF */
			R4 = R3;						/* R4 = FFFFFFFFFFFFFFFF */
			PUNPCKLBW(R4,R0);				/* R4 = FFR3FFR2FFR1FFR0 */
			PUNPCKHBW(R3,R0);				/* R3 = FFR7FFR6FFR5FFR4 */
			R0 = R4;						/* R0 = R4               */
			PUNPCKLWD(R0,R2);				/* R0 = FFR1G1B1FFR0G0B0 */
			PUNPCKHWD(R4,R2);				/* R4 = FFR3G3B3FFR2G2B2 */
			R2 = R3;						/* R2 = R3               */
			PUNPCKLWD(R2,R1);				/* R2 = FFR5G5B5FFR4G4B4 */
			PUNPCKHWD(R3,R1);				/* R3 = FFR7G7B7FFR6G6B6 */
			STORE128(out, R0);  out += 16;	/* FFR1G1B1FFR0G0B0      */
			STORE128(out, R4);  out += 16;	/* FFR3G3B3FFR2G2B2      */
			STORE128(out, R2);  out += 16;	/* FFR5G5B5FFR4G4B4      */
			STORE128(out, R3);  out += 16;	/* FFR7G7B7FFR6G6B6      */
		} while (width -= 16);
		/* Jump to next row. */
		r += srcbump;
		g += srcbump;
		b += srcbump;
		out += dstbump;
	}
	return PRIMITIVES_SUCCESS;
}
#endif /* WITH_SSE2 */

/*---------------------------------------------------------------------------*/
#ifdef WITH_NEON
pstatus_t neon_yCbCrToRGB_16s16s_P3P3(
	const INT16 *pSrc[3],
	int srcStep,
	INT16 *pDst[3],
	int dstStep,
	const prim_size_t *roi)	/* region of interest */
{
	/* TODO: If necessary, check alignments and call the general version. */

	int16x8_t zero = vdupq_n_s16(0);
	int16x8_t max = vdupq_n_s16(255);

	int16x8_t r_cr = vdupq_n_s16(22986);	//  1.403 << 14
	int16x8_t g_cb = vdupq_n_s16(-5636);	// -0.344 << 14
	int16x8_t g_cr = vdupq_n_s16(-11698);	// -0.714 << 14
	int16x8_t b_cb = vdupq_n_s16(28999);	//  1.770 << 14
	int16x8_t c4096 = vdupq_n_s16(4096);

	int16x8_t* y_buf  = (int16x8_t*) pSrc[0];
	int16x8_t* cb_buf = (int16x8_t*) pSrc[1];
	int16x8_t* cr_buf = (int16x8_t*) pSrc[2];
	int16x8_t* r_buf  = (int16x8_t*) pDst[0];
	int16x8_t* g_buf  = (int16x8_t*) pDst[1];
	int16x8_t* b_buf  = (int16x8_t*) pDst[2];

	int srcbump = srcStep / sizeof(int16x8_t);
	int dstbump = dstStep / sizeof(int16x8_t);
	int yp;

	int imax = roi->width * sizeof(INT16) / sizeof(int16x8_t);
	for (yp=0; yp<roi->height; ++yp)
	{
		int i;
		for (i=0; i<imax; i++)
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
			int16x8_t y = vld1q_s16((INT16*) &y_buf[i]);
			y = vaddq_s16(y, c4096);
			y = vshrq_n_s16(y, 2);
			/* cb = cb_buf[i]; */
			int16x8_t cb = vld1q_s16((INT16*)&cb_buf[i]);
			/* cr = cr_buf[i]; */
			int16x8_t cr = vld1q_s16((INT16*) &cr_buf[i]);

			/* (y + HIWORD(cr*22986)) >> 3 */
			int16x8_t r = vaddq_s16(y, vshrq_n_s16(vqdmulhq_s16(cr, r_cr), 1));
			r = vshrq_n_s16(r, 3);
			/* r_buf[i] = MINMAX(r, 0, 255); */
			r = vminq_s16(vmaxq_s16(r, zero), max);
			vst1q_s16((INT16*)&r_buf[i], r);

			/* (y + HIWORD(cb*-5636) + HIWORD(cr*-11698)) >> 3 */
			int16x8_t g = vaddq_s16(y, vshrq_n_s16(vqdmulhq_s16(cb, g_cb), 1));
			g = vaddq_s16(g, vshrq_n_s16(vqdmulhq_s16(cr, g_cr), 1));
			g = vshrq_n_s16(g, 3);
			/* g_buf[i] = MINMAX(g, 0, 255); */
			g = vminq_s16(vmaxq_s16(g, zero), max);
			vst1q_s16((INT16*)&g_buf[i], g);

			/* (y + HIWORD(cb*28999)) >> 3 */
			int16x8_t b = vaddq_s16(y, vshrq_n_s16(vqdmulhq_s16(cb, b_cb), 1));
			b = vshrq_n_s16(b, 3);
			/* b_buf[i] = MINMAX(b, 0, 255); */
			b = vminq_s16(vmaxq_s16(b, zero), max);
			vst1q_s16((INT16*)&b_buf[i], b);
		}

		y_buf  += srcbump;
		cb_buf += srcbump;
		cr_buf += srcbump;
		r_buf += dstbump;
		g_buf += dstbump;
		b_buf += dstbump;
	}
	return PRIMITIVES_SUCCESS;
}
#endif /* WITH_NEON */


/* I don't see a direct IPP version of this, since the input is INT16
 * YCbCr.  It may be possible via  Deinterleave and then YCbCrToRGB_<mod>.
 * But that would likely be slower.
 */

/* ------------------------------------------------------------------------- */
void primitives_init_colors_opt(primitives_t* prims)
{
#if defined(WITH_SSE2)
	if (IsProcessorFeaturePresent(PF_SSE2_INSTRUCTIONS_AVAILABLE))
	{
		prims->RGBToRGB_16s8u_P3AC4R  = sse2_RGBToRGB_16s8u_P3AC4R;
		prims->yCbCrToRGB_16s16s_P3P3 = sse2_yCbCrToRGB_16s16s_P3P3;
		prims->RGBToYCbCr_16s16s_P3P3 = sse2_RGBToYCbCr_16s16s_P3P3;
	}
#elif defined(WITH_NEON)
	if (IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE))
	{
		prims->yCbCrToRGB_16s16s_P3P3 = neon_yCbCrToRGB_16s16s_P3P3;
	}
#endif /* WITH_SSE2 */
}

