/* prim_templates.h
 * vi:ts=4 sw=4
 *
 * (c) Copyright 2012 Hewlett-Packard Development Company, L.P.
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0.
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * permissions and limitations under the License.  Algorithms used by
 * this code may be covered by patents by HP, Microsoft, or other parties.
 */

#ifdef __GNUC__
#pragma once
#endif

#ifndef FREERDP_LIB_PRIM_TEMPLATES_H
#define FREERDP_LIB_PRIM_TEMPLATES_H

/* These are prototypes for SSE (potentially NEON) routines that do a
 * simple SSE operation over an array of data.  Since so much of this
 * code is shared except for the operation itself, these prototypes are
 * used rather than duplicating code.  The naming convention depends on
 * the parameters:  S=Source param; C=Constant; D=Destination.
 * All the macros have parameters for a fallback procedure if the data
 * is too small and an operation "the slow way" for use at 16-byte edges.
 */

/* SSE3 note:  If someone needs to support an SSE2 version of these without
 * SSE3 support, an alternative version could be added that merely checks
 * that 16-byte alignment on both destination and source(s) can be
 * achieved, rather than use LDDQU for unaligned reads.
 */

/* Note: the compiler is good at turning (16/sizeof(_type_)) into a constant.
 * It easily can't do that if the value is stored in a variable.
 * So don't save it as an intermediate value.
 */

/* ----------------------------------------------------------------------------
 * SCD = Source, Constant, Destination
 */
#define SSE3_SCD_ROUTINE(_name_, _type_, _fallback_, _op_, _slowWay_)                 \
	static pstatus_t _name_(const _type_* pSrc, UINT32 val, _type_* pDst, UINT32 len) \
	{                                                                                 \
		INT32 shifts = 0;                                                             \
		UINT32 offBeatMask;                                                           \
		const _type_* sptr = pSrc;                                                    \
		_type_* dptr = pDst;                                                          \
		int count;                                                                    \
		if (val == 0)                                                                 \
			return PRIMITIVES_SUCCESS;                                                \
		if (val >= 16)                                                                \
			return -1;                                                                \
		if (len < 16) /* pointless if too small */                                    \
		{                                                                             \
			return _fallback_(pSrc, val, pDst, len);                                  \
		}                                                                             \
		if (sizeof(_type_) == 1)                                                      \
			shifts = 1;                                                               \
		else if (sizeof(_type_) == 2)                                                 \
			shifts = 2;                                                               \
		else if (sizeof(_type_) == 4)                                                 \
			shifts = 3;                                                               \
		else if (sizeof(_type_) == 8)                                                 \
			shifts = 4;                                                               \
		offBeatMask = (1 << (shifts - 1)) - 1;                                        \
		if ((ULONG_PTR)pDst & offBeatMask)                                            \
		{                                                                             \
			/* Incrementing the pointer skips over 16-byte boundary. */               \
			return _fallback_(pSrc, val, pDst, len);                                  \
		}                                                                             \
		/* Get to the 16-byte boundary now. */                                        \
		while ((ULONG_PTR)dptr & 0x0f)                                                \
		{                                                                             \
			_slowWay_;                                                                \
			if (--len == 0)                                                           \
				return PRIMITIVES_SUCCESS;                                            \
		}                                                                             \
		/* Use 8 128-bit SSE registers. */                                            \
		count = len >> (8 - shifts);                                                  \
		len -= count << (8 - shifts);                                                 \
		if ((const ULONG_PTR)sptr & 0x0f)                                             \
		{                                                                             \
			while (count--)                                                           \
			{                                                                         \
				__m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;               \
				xmm0 = _mm_lddqu_si128((const __m128i*)sptr);                         \
				sptr += (16 / sizeof(_type_));                                        \
				xmm1 = _mm_lddqu_si128((const __m128i*)sptr);                         \
				sptr += (16 / sizeof(_type_));                                        \
				xmm2 = _mm_lddqu_si128((const __m128i*)sptr);                         \
				sptr += (16 / sizeof(_type_));                                        \
				xmm3 = _mm_lddqu_si128((const __m128i*)sptr);                         \
				sptr += (16 / sizeof(_type_));                                        \
				xmm4 = _mm_lddqu_si128((const __m128i*)sptr);                         \
				sptr += (16 / sizeof(_type_));                                        \
				xmm5 = _mm_lddqu_si128((const __m128i*)sptr);                         \
				sptr += (16 / sizeof(_type_));                                        \
				xmm6 = _mm_lddqu_si128((const __m128i*)sptr);                         \
				sptr += (16 / sizeof(_type_));                                        \
				xmm7 = _mm_lddqu_si128((const __m128i*)sptr);                         \
				sptr += (16 / sizeof(_type_));                                        \
				xmm0 = _op_(xmm0, val);                                               \
				xmm1 = _op_(xmm1, val);                                               \
				xmm2 = _op_(xmm2, val);                                               \
				xmm3 = _op_(xmm3, val);                                               \
				xmm4 = _op_(xmm4, val);                                               \
				xmm5 = _op_(xmm5, val);                                               \
				xmm6 = _op_(xmm6, val);                                               \
				xmm7 = _op_(xmm7, val);                                               \
				_mm_store_si128((__m128i*)dptr, xmm0);                                \
				dptr += (16 / sizeof(_type_));                                        \
				_mm_store_si128((__m128i*)dptr, xmm1);                                \
				dptr += (16 / sizeof(_type_));                                        \
				_mm_store_si128((__m128i*)dptr, xmm2);                                \
				dptr += (16 / sizeof(_type_));                                        \
				_mm_store_si128((__m128i*)dptr, xmm3);                                \
				dptr += (16 / sizeof(_type_));                                        \
				_mm_store_si128((__m128i*)dptr, xmm4);                                \
				dptr += (16 / sizeof(_type_));                                        \
				_mm_store_si128((__m128i*)dptr, xmm5);                                \
				dptr += (16 / sizeof(_type_));                                        \
				_mm_store_si128((__m128i*)dptr, xmm6);                                \
				dptr += (16 / sizeof(_type_));                                        \
				_mm_store_si128((__m128i*)dptr, xmm7);                                \
				dptr += (16 / sizeof(_type_));                                        \
			}                                                                         \
		}                                                                             \
		else                                                                          \
		{                                                                             \
			while (count--)                                                           \
			{                                                                         \
				__m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;               \
				xmm0 = _mm_load_si128((const __m128i*)sptr);                          \
				sptr += (16 / sizeof(_type_));                                        \
				xmm1 = _mm_load_si128((const __m128i*)sptr);                          \
				sptr += (16 / sizeof(_type_));                                        \
				xmm2 = _mm_load_si128((const __m128i*)sptr);                          \
				sptr += (16 / sizeof(_type_));                                        \
				xmm3 = _mm_load_si128((const __m128i*)sptr);                          \
				sptr += (16 / sizeof(_type_));                                        \
				xmm4 = _mm_load_si128((const __m128i*)sptr);                          \
				sptr += (16 / sizeof(_type_));                                        \
				xmm5 = _mm_load_si128((const __m128i*)sptr);                          \
				sptr += (16 / sizeof(_type_));                                        \
				xmm6 = _mm_load_si128((const __m128i*)sptr);                          \
				sptr += (16 / sizeof(_type_));                                        \
				xmm7 = _mm_load_si128((const __m128i*)sptr);                          \
				sptr += (16 / sizeof(_type_));                                        \
				xmm0 = _op_(xmm0, val);                                               \
				xmm1 = _op_(xmm1, val);                                               \
				xmm2 = _op_(xmm2, val);                                               \
				xmm3 = _op_(xmm3, val);                                               \
				xmm4 = _op_(xmm4, val);                                               \
				xmm5 = _op_(xmm5, val);                                               \
				xmm6 = _op_(xmm6, val);                                               \
				xmm7 = _op_(xmm7, val);                                               \
				_mm_store_si128((__m128i*)dptr, xmm0);                                \
				dptr += (16 / sizeof(_type_));                                        \
				_mm_store_si128((__m128i*)dptr, xmm1);                                \
				dptr += (16 / sizeof(_type_));                                        \
				_mm_store_si128((__m128i*)dptr, xmm2);                                \
				dptr += (16 / sizeof(_type_));                                        \
				_mm_store_si128((__m128i*)dptr, xmm3);                                \
				dptr += (16 / sizeof(_type_));                                        \
				_mm_store_si128((__m128i*)dptr, xmm4);                                \
				dptr += (16 / sizeof(_type_));                                        \
				_mm_store_si128((__m128i*)dptr, xmm5);                                \
				dptr += (16 / sizeof(_type_));                                        \
				_mm_store_si128((__m128i*)dptr, xmm6);                                \
				dptr += (16 / sizeof(_type_));                                        \
				_mm_store_si128((__m128i*)dptr, xmm7);                                \
				dptr += (16 / sizeof(_type_));                                        \
			}                                                                         \
		}                                                                             \
		/* Use a single 128-bit SSE register. */                                      \
		count = len >> (5 - shifts);                                                  \
		len -= count << (5 - shifts);                                                 \
		while (count--)                                                               \
		{                                                                             \
			__m128i xmm0 = LOAD_SI128(sptr);                                          \
			sptr += (16 / sizeof(_type_));                                            \
			xmm0 = _op_(xmm0, val);                                                   \
			_mm_store_si128((__m128i*)dptr, xmm0);                                    \
			dptr += (16 / sizeof(_type_));                                            \
		}                                                                             \
		/* Finish off the remainder. */                                               \
		while (len--)                                                                 \
		{                                                                             \
			_slowWay_;                                                                \
		}                                                                             \
		return PRIMITIVES_SUCCESS;                                                    \
	}

/* ----------------------------------------------------------------------------
 * SCD = Source, Constant, Destination
 * PRE = preload xmm0 with the constant.
 */
#define SSE3_SCD_PRE_ROUTINE(_name_, _type_, _fallback_, _op_, _slowWay_)            \
	static pstatus_t _name_(const _type_* pSrc, _type_ val, _type_* pDst, INT32 len) \
	{                                                                                \
		int shifts = 0;                                                              \
		UINT32 offBeatMask;                                                          \
		const _type_* sptr = pSrc;                                                   \
		_type_* dptr = pDst;                                                         \
		size_t count;                                                                \
		__m128i xmm0;                                                                \
		if (len < 16) /* pointless if too small */                                   \
		{                                                                            \
			return _fallback_(pSrc, val, pDst, len);                                 \
		}                                                                            \
		if (sizeof(_type_) == 1)                                                     \
			shifts = 1;                                                              \
		else if (sizeof(_type_) == 2)                                                \
			shifts = 2;                                                              \
		else if (sizeof(_type_) == 4)                                                \
			shifts = 3;                                                              \
		else if (sizeof(_type_) == 8)                                                \
			shifts = 4;                                                              \
		offBeatMask = (1 << (shifts - 1)) - 1;                                       \
		if ((ULONG_PTR)pDst & offBeatMask)                                           \
		{                                                                            \
			/* Incrementing the pointer skips over 16-byte boundary. */              \
			return _fallback_(pSrc, val, pDst, len);                                 \
		}                                                                            \
		/* Get to the 16-byte boundary now. */                                       \
		while ((ULONG_PTR)dptr & 0x0f)                                               \
		{                                                                            \
			_slowWay_;                                                               \
			if (--len == 0)                                                          \
				return PRIMITIVES_SUCCESS;                                           \
		}                                                                            \
		/* Use 4 128-bit SSE registers. */                                           \
		count = len >> (7 - shifts);                                                 \
		len -= count << (7 - shifts);                                                \
		xmm0 = _mm_set1_epi32(val);                                                  \
		if ((const ULONG_PTR)sptr & 0x0f)                                            \
		{                                                                            \
			while (count--)                                                          \
			{                                                                        \
				__m128i xmm1, xmm2, xmm3, xmm4;                                      \
				xmm1 = _mm_lddqu_si128((const __m128i*)sptr);                        \
				sptr += (16 / sizeof(_type_));                                       \
				xmm2 = _mm_lddqu_si128((const __m128i*)sptr);                        \
				sptr += (16 / sizeof(_type_));                                       \
				xmm3 = _mm_lddqu_si128((const __m128i*)sptr);                        \
				sptr += (16 / sizeof(_type_));                                       \
				xmm4 = _mm_lddqu_si128((const __m128i*)sptr);                        \
				sptr += (16 / sizeof(_type_));                                       \
				xmm1 = _op_(xmm1, xmm0);                                             \
				xmm2 = _op_(xmm2, xmm0);                                             \
				xmm3 = _op_(xmm3, xmm0);                                             \
				xmm4 = _op_(xmm4, xmm0);                                             \
				_mm_store_si128((__m128i*)dptr, xmm1);                               \
				dptr += (16 / sizeof(_type_));                                       \
				_mm_store_si128((__m128i*)dptr, xmm2);                               \
				dptr += (16 / sizeof(_type_));                                       \
				_mm_store_si128((__m128i*)dptr, xmm3);                               \
				dptr += (16 / sizeof(_type_));                                       \
				_mm_store_si128((__m128i*)dptr, xmm4);                               \
				dptr += (16 / sizeof(_type_));                                       \
			}                                                                        \
		}                                                                            \
		else                                                                         \
		{                                                                            \
			while (count--)                                                          \
			{                                                                        \
				__m128i xmm1, xmm2, xmm3, xmm4;                                      \
				xmm1 = _mm_load_si128((const __m128i*)sptr);                         \
				sptr += (16 / sizeof(_type_));                                       \
				xmm2 = _mm_load_si128((const __m128i*)sptr);                         \
				sptr += (16 / sizeof(_type_));                                       \
				xmm3 = _mm_load_si128((const __m128i*)sptr);                         \
				sptr += (16 / sizeof(_type_));                                       \
				xmm4 = _mm_load_si128((const __m128i*)sptr);                         \
				sptr += (16 / sizeof(_type_));                                       \
				xmm1 = _op_(xmm1, xmm0);                                             \
				xmm2 = _op_(xmm2, xmm0);                                             \
				xmm3 = _op_(xmm3, xmm0);                                             \
				xmm4 = _op_(xmm4, xmm0);                                             \
				_mm_store_si128((__m128i*)dptr, xmm1);                               \
				dptr += (16 / sizeof(_type_));                                       \
				_mm_store_si128((__m128i*)dptr, xmm2);                               \
				dptr += (16 / sizeof(_type_));                                       \
				_mm_store_si128((__m128i*)dptr, xmm3);                               \
				dptr += (16 / sizeof(_type_));                                       \
				_mm_store_si128((__m128i*)dptr, xmm4);                               \
				dptr += (16 / sizeof(_type_));                                       \
			}                                                                        \
		}                                                                            \
		/* Use a single 128-bit SSE register. */                                     \
		count = len >> (5 - shifts);                                                 \
		len -= count << (5 - shifts);                                                \
		while (count--)                                                              \
		{                                                                            \
			__m128i xmm1 = LOAD_SI128(sptr);                                         \
			sptr += (16 / sizeof(_type_));                                           \
			xmm1 = _op_(xmm1, xmm0);                                                 \
			_mm_store_si128((__m128i*)dptr, xmm1);                                   \
			dptr += (16 / sizeof(_type_));                                           \
		}                                                                            \
		/* Finish off the remainder. */                                              \
		while (len--)                                                                \
		{                                                                            \
			_slowWay_;                                                               \
		}                                                                            \
		return PRIMITIVES_SUCCESS;                                                   \
	}

/* ----------------------------------------------------------------------------
 * SSD = Source1, Source2, Destination
 */
#define SSE3_SSD_ROUTINE(_name_, _type_, _fallback_, _op_, _slowWay_)                           \
	static pstatus_t _name_(const _type_* pSrc1, const _type_* pSrc2, _type_* pDst, UINT32 len) \
	{                                                                                           \
		int shifts = 0;                                                                         \
		UINT32 offBeatMask;                                                                     \
		const _type_* sptr1 = pSrc1;                                                            \
		const _type_* sptr2 = pSrc2;                                                            \
		_type_* dptr = pDst;                                                                    \
		size_t count;                                                                           \
		if (len < 16) /* pointless if too small */                                              \
		{                                                                                       \
			return _fallback_(pSrc1, pSrc2, pDst, len);                                         \
		}                                                                                       \
		if (sizeof(_type_) == 1)                                                                \
			shifts = 1;                                                                         \
		else if (sizeof(_type_) == 2)                                                           \
			shifts = 2;                                                                         \
		else if (sizeof(_type_) == 4)                                                           \
			shifts = 3;                                                                         \
		else if (sizeof(_type_) == 8)                                                           \
			shifts = 4;                                                                         \
		offBeatMask = (1 << (shifts - 1)) - 1;                                                  \
		if ((ULONG_PTR)pDst & offBeatMask)                                                      \
		{                                                                                       \
			/* Incrementing the pointer skips over 16-byte boundary. */                         \
			return _fallback_(pSrc1, pSrc2, pDst, len);                                         \
		}                                                                                       \
		/* Get to the 16-byte boundary now. */                                                  \
		while ((ULONG_PTR)dptr & 0x0f)                                                          \
		{                                                                                       \
			pstatus_t status;                                                                   \
			status = _slowWay_;                                                                 \
			if (status != PRIMITIVES_SUCCESS)                                                   \
				return status;                                                                  \
			if (--len == 0)                                                                     \
				return PRIMITIVES_SUCCESS;                                                      \
		}                                                                                       \
		/* Use 4 128-bit SSE registers. */                                                      \
		count = len >> (7 - shifts);                                                            \
		len -= count << (7 - shifts);                                                           \
		if (((const ULONG_PTR)sptr1 & 0x0f) || ((const ULONG_PTR)sptr2 & 0x0f))                 \
		{                                                                                       \
			/* Unaligned loads */                                                               \
			while (count--)                                                                     \
			{                                                                                   \
				__m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;                         \
				xmm0 = _mm_lddqu_si128((const __m128i*)sptr1);                                  \
				sptr1 += (16 / sizeof(_type_));                                                 \
				xmm1 = _mm_lddqu_si128((const __m128i*)sptr1);                                  \
				sptr1 += (16 / sizeof(_type_));                                                 \
				xmm2 = _mm_lddqu_si128((const __m128i*)sptr1);                                  \
				sptr1 += (16 / sizeof(_type_));                                                 \
				xmm3 = _mm_lddqu_si128((const __m128i*)sptr1);                                  \
				sptr1 += (16 / sizeof(_type_));                                                 \
				xmm4 = _mm_lddqu_si128((const __m128i*)sptr2);                                  \
				sptr2 += (16 / sizeof(_type_));                                                 \
				xmm5 = _mm_lddqu_si128((const __m128i*)sptr2);                                  \
				sptr2 += (16 / sizeof(_type_));                                                 \
				xmm6 = _mm_lddqu_si128((const __m128i*)sptr2);                                  \
				sptr2 += (16 / sizeof(_type_));                                                 \
				xmm7 = _mm_lddqu_si128((const __m128i*)sptr2);                                  \
				sptr2 += (16 / sizeof(_type_));                                                 \
				xmm0 = _op_(xmm0, xmm4);                                                        \
				xmm1 = _op_(xmm1, xmm5);                                                        \
				xmm2 = _op_(xmm2, xmm6);                                                        \
				xmm3 = _op_(xmm3, xmm7);                                                        \
				_mm_store_si128((__m128i*)dptr, xmm0);                                          \
				dptr += (16 / sizeof(_type_));                                                  \
				_mm_store_si128((__m128i*)dptr, xmm1);                                          \
				dptr += (16 / sizeof(_type_));                                                  \
				_mm_store_si128((__m128i*)dptr, xmm2);                                          \
				dptr += (16 / sizeof(_type_));                                                  \
				_mm_store_si128((__m128i*)dptr, xmm3);                                          \
				dptr += (16 / sizeof(_type_));                                                  \
			}                                                                                   \
		}                                                                                       \
		else                                                                                    \
		{                                                                                       \
			/* Aligned loads */                                                                 \
			while (count--)                                                                     \
			{                                                                                   \
				__m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;                         \
				xmm0 = _mm_load_si128((const __m128i*)sptr1);                                   \
				sptr1 += (16 / sizeof(_type_));                                                 \
				xmm1 = _mm_load_si128((const __m128i*)sptr1);                                   \
				sptr1 += (16 / sizeof(_type_));                                                 \
				xmm2 = _mm_load_si128((const __m128i*)sptr1);                                   \
				sptr1 += (16 / sizeof(_type_));                                                 \
				xmm3 = _mm_load_si128((const __m128i*)sptr1);                                   \
				sptr1 += (16 / sizeof(_type_));                                                 \
				xmm4 = _mm_load_si128((const __m128i*)sptr2);                                   \
				sptr2 += (16 / sizeof(_type_));                                                 \
				xmm5 = _mm_load_si128((const __m128i*)sptr2);                                   \
				sptr2 += (16 / sizeof(_type_));                                                 \
				xmm6 = _mm_load_si128((const __m128i*)sptr2);                                   \
				sptr2 += (16 / sizeof(_type_));                                                 \
				xmm7 = _mm_load_si128((const __m128i*)sptr2);                                   \
				sptr2 += (16 / sizeof(_type_));                                                 \
				xmm0 = _op_(xmm0, xmm4);                                                        \
				xmm1 = _op_(xmm1, xmm5);                                                        \
				xmm2 = _op_(xmm2, xmm6);                                                        \
				xmm3 = _op_(xmm3, xmm7);                                                        \
				_mm_store_si128((__m128i*)dptr, xmm0);                                          \
				dptr += (16 / sizeof(_type_));                                                  \
				_mm_store_si128((__m128i*)dptr, xmm1);                                          \
				dptr += (16 / sizeof(_type_));                                                  \
				_mm_store_si128((__m128i*)dptr, xmm2);                                          \
				dptr += (16 / sizeof(_type_));                                                  \
				_mm_store_si128((__m128i*)dptr, xmm3);                                          \
				dptr += (16 / sizeof(_type_));                                                  \
			}                                                                                   \
		}                                                                                       \
		/* Use a single 128-bit SSE register. */                                                \
		count = len >> (5 - shifts);                                                            \
		len -= count << (5 - shifts);                                                           \
		while (count--)                                                                         \
		{                                                                                       \
			__m128i xmm0, xmm1;                                                                 \
			xmm0 = LOAD_SI128(sptr1);                                                           \
			sptr1 += (16 / sizeof(_type_));                                                     \
			xmm1 = LOAD_SI128(sptr2);                                                           \
			sptr2 += (16 / sizeof(_type_));                                                     \
			xmm0 = _op_(xmm0, xmm1);                                                            \
			_mm_store_si128((__m128i*)dptr, xmm0);                                              \
			dptr += (16 / sizeof(_type_));                                                      \
		}                                                                                       \
		/* Finish off the remainder. */                                                         \
		while (len--)                                                                           \
		{                                                                                       \
			_slowWay_;                                                                          \
		}                                                                                       \
		return PRIMITIVES_SUCCESS;                                                              \
	}

#endif /* FREERDP_LIB_PRIM_TEMPLATES_H */
