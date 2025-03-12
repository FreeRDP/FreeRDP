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

#pragma once

#include "prim_avxsse.h"

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
#define SSE3_SCD_ROUTINE(_name_, _type_, _fallback_, _op_, _op_type_, _slowWay_) \
	static pstatus_t _name_(const _type_* WINPR_RESTRICT pSrc, UINT32 val,       \
	                        _type_* WINPR_RESTRICT pDst, UINT32 ulen)            \
	{                                                                            \
		size_t len = ulen;                                                       \
		INT32 shifts = 0;                                                        \
		const _type_* sptr = pSrc;                                               \
		_type_* dptr = pDst;                                                     \
		if (val == 0)                                                            \
			return PRIMITIVES_SUCCESS;                                           \
		if (val >= 16)                                                           \
			return -1;                                                           \
		if (sizeof(_type_) == 1)                                                 \
			shifts = 1;                                                          \
		else if (sizeof(_type_) == 2)                                            \
			shifts = 2;                                                          \
		else if (sizeof(_type_) == 4)                                            \
			shifts = 3;                                                          \
		else if (sizeof(_type_) == 8)                                            \
			shifts = 4;                                                          \
		/* Use 8 128-bit SSE registers. */                                       \
		size_t count = len >> (8 - shifts);                                      \
		len -= count << (8 - shifts);                                            \
                                                                                 \
		while (count--)                                                          \
		{                                                                        \
			__m128i xmm0 = LOAD_SI128(sptr);                                     \
			sptr += (16 / sizeof(_type_));                                       \
			__m128i xmm1 = LOAD_SI128(sptr);                                     \
			sptr += (16 / sizeof(_type_));                                       \
			__m128i xmm2 = LOAD_SI128(sptr);                                     \
			sptr += (16 / sizeof(_type_));                                       \
			__m128i xmm3 = LOAD_SI128(sptr);                                     \
			sptr += (16 / sizeof(_type_));                                       \
			__m128i xmm4 = LOAD_SI128(sptr);                                     \
			sptr += (16 / sizeof(_type_));                                       \
			__m128i xmm5 = LOAD_SI128(sptr);                                     \
			sptr += (16 / sizeof(_type_));                                       \
			__m128i xmm6 = LOAD_SI128(sptr);                                     \
			sptr += (16 / sizeof(_type_));                                       \
			__m128i xmm7 = LOAD_SI128(sptr);                                     \
			sptr += (16 / sizeof(_type_));                                       \
			xmm0 = _op_(xmm0, (_op_type_)val);                                   \
			xmm1 = _op_(xmm1, (_op_type_)val);                                   \
			xmm2 = _op_(xmm2, (_op_type_)val);                                   \
			xmm3 = _op_(xmm3, (_op_type_)val);                                   \
			xmm4 = _op_(xmm4, (_op_type_)val);                                   \
			xmm5 = _op_(xmm5, (_op_type_)val);                                   \
			xmm6 = _op_(xmm6, (_op_type_)val);                                   \
			xmm7 = _op_(xmm7, (_op_type_)val);                                   \
			STORE_SI128(dptr, xmm0);                                             \
			dptr += (16 / sizeof(_type_));                                       \
			STORE_SI128(dptr, xmm1);                                             \
			dptr += (16 / sizeof(_type_));                                       \
			STORE_SI128(dptr, xmm2);                                             \
			dptr += (16 / sizeof(_type_));                                       \
			STORE_SI128(dptr, xmm3);                                             \
			dptr += (16 / sizeof(_type_));                                       \
			STORE_SI128(dptr, xmm4);                                             \
			dptr += (16 / sizeof(_type_));                                       \
			STORE_SI128(dptr, xmm5);                                             \
			dptr += (16 / sizeof(_type_));                                       \
			STORE_SI128(dptr, xmm6);                                             \
			dptr += (16 / sizeof(_type_));                                       \
			STORE_SI128(dptr, xmm7);                                             \
			dptr += (16 / sizeof(_type_));                                       \
		}                                                                        \
                                                                                 \
		/* Use a single 128-bit SSE register. */                                 \
		count = len >> (5 - shifts);                                             \
		len -= count << (5 - shifts);                                            \
		while (count--)                                                          \
		{                                                                        \
			__m128i xmm0 = LOAD_SI128(sptr);                                     \
			sptr += (16 / sizeof(_type_));                                       \
			xmm0 = _op_(xmm0, (_op_type_)val);                                   \
			STORE_SI128(dptr, xmm0);                                             \
			dptr += (16 / sizeof(_type_));                                       \
		}                                                                        \
		/* Finish off the remainder. */                                          \
		while (len--)                                                            \
		{                                                                        \
			_slowWay_;                                                           \
		}                                                                        \
		return PRIMITIVES_SUCCESS;                                               \
	}

/* ----------------------------------------------------------------------------
 * SCD = Source, Constant, Destination
 * PRE = preload xmm0 with the constant.
 */
#define SSE3_SCD_PRE_ROUTINE(_name_, _type_, _fallback_, _op_, _slowWay_)  \
	static pstatus_t _name_(const _type_* WINPR_RESTRICT pSrc, _type_ val, \
	                        _type_* WINPR_RESTRICT pDst, INT32 ilen)       \
	{                                                                      \
		size_t len = WINPR_ASSERTING_INT_CAST(size_t, ilen);               \
		int shifts = 0;                                                    \
		const _type_* sptr = pSrc;                                         \
		_type_* dptr = pDst;                                               \
		__m128i xmm0;                                                      \
		if (sizeof(_type_) == 1)                                           \
			shifts = 1;                                                    \
		else if (sizeof(_type_) == 2)                                      \
			shifts = 2;                                                    \
		else if (sizeof(_type_) == 4)                                      \
			shifts = 3;                                                    \
		else if (sizeof(_type_) == 8)                                      \
			shifts = 4;                                                    \
		/* Use 4 128-bit SSE registers. */                                 \
		size_t count = len >> (7 - shifts);                                \
		len -= count << (7 - shifts);                                      \
		xmm0 = mm_set1_epu32(val);                                         \
		for (size_t x = 0; x < count; x++)                                 \
		{                                                                  \
			__m128i xmm1 = LOAD_SI128(sptr);                               \
			sptr += (16 / sizeof(_type_));                                 \
			__m128i xmm2 = LOAD_SI128(sptr);                               \
			sptr += (16 / sizeof(_type_));                                 \
			__m128i xmm3 = LOAD_SI128(sptr);                               \
			sptr += (16 / sizeof(_type_));                                 \
			__m128i xmm4 = LOAD_SI128(sptr);                               \
			sptr += (16 / sizeof(_type_));                                 \
			xmm1 = _op_(xmm1, xmm0);                                       \
			xmm2 = _op_(xmm2, xmm0);                                       \
			xmm3 = _op_(xmm3, xmm0);                                       \
			xmm4 = _op_(xmm4, xmm0);                                       \
			STORE_SI128(dptr, xmm1);                                       \
			dptr += (16 / sizeof(_type_));                                 \
			STORE_SI128(dptr, xmm2);                                       \
			dptr += (16 / sizeof(_type_));                                 \
			STORE_SI128(dptr, xmm3);                                       \
			dptr += (16 / sizeof(_type_));                                 \
			STORE_SI128(dptr, xmm4);                                       \
			dptr += (16 / sizeof(_type_));                                 \
		}                                                                  \
		/* Use a single 128-bit SSE register. */                           \
		count = len >> (5 - shifts);                                       \
		len -= count << (5 - shifts);                                      \
		for (size_t x = 0; x < count; x++)                                 \
		{                                                                  \
			__m128i xmm1 = LOAD_SI128(sptr);                               \
			sptr += (16 / sizeof(_type_));                                 \
			xmm1 = _op_(xmm1, xmm0);                                       \
			STORE_SI128(dptr, xmm1);                                       \
			dptr += (16 / sizeof(_type_));                                 \
		}                                                                  \
		/* Finish off the remainder. */                                    \
		for (size_t x = 0; x < len; x++)                                   \
		{                                                                  \
			_slowWay_;                                                     \
		}                                                                  \
		return PRIMITIVES_SUCCESS;                                         \
	}

/* ----------------------------------------------------------------------------
 * SSD = Source1, Source2, Destination
 */
#define SSE3_SSD_ROUTINE(_name_, _type_, _fallback_, _op_, _slowWay_)                        \
	static pstatus_t _name_(const _type_* WINPR_RESTRICT pSrc1,                              \
	                        const _type_* WINPR_RESTRICT pSrc2, _type_* WINPR_RESTRICT pDst, \
	                        UINT32 ulen)                                                     \
	{                                                                                        \
		size_t len = ulen;                                                                   \
		int shifts = 0;                                                                      \
		const _type_* sptr1 = pSrc1;                                                         \
		const _type_* sptr2 = pSrc2;                                                         \
		_type_* dptr = pDst;                                                                 \
		size_t count;                                                                        \
		if (sizeof(_type_) == 1)                                                             \
			shifts = 1;                                                                      \
		else if (sizeof(_type_) == 2)                                                        \
			shifts = 2;                                                                      \
		else if (sizeof(_type_) == 4)                                                        \
			shifts = 3;                                                                      \
		else if (sizeof(_type_) == 8)                                                        \
			shifts = 4;                                                                      \
		/* Use 4 128-bit SSE registers. */                                                   \
		count = len >> (7 - shifts);                                                         \
		len -= count << (7 - shifts);                                                        \
		/* Aligned loads */                                                                  \
		while (count--)                                                                      \
		{                                                                                    \
			__m128i xmm0 = LOAD_SI128(sptr1);                                                \
			sptr1 += (16 / sizeof(_type_));                                                  \
			__m128i xmm1 = LOAD_SI128(sptr1);                                                \
			sptr1 += (16 / sizeof(_type_));                                                  \
			__m128i xmm2 = LOAD_SI128(sptr1);                                                \
			sptr1 += (16 / sizeof(_type_));                                                  \
			__m128i xmm3 = LOAD_SI128(sptr1);                                                \
			sptr1 += (16 / sizeof(_type_));                                                  \
			__m128i xmm4 = LOAD_SI128(sptr2);                                                \
			sptr2 += (16 / sizeof(_type_));                                                  \
			__m128i xmm5 = LOAD_SI128(sptr2);                                                \
			sptr2 += (16 / sizeof(_type_));                                                  \
			__m128i xmm6 = LOAD_SI128(sptr2);                                                \
			sptr2 += (16 / sizeof(_type_));                                                  \
			__m128i xmm7 = LOAD_SI128(sptr2);                                                \
			sptr2 += (16 / sizeof(_type_));                                                  \
			xmm0 = _op_(xmm0, xmm4);                                                         \
			xmm1 = _op_(xmm1, xmm5);                                                         \
			xmm2 = _op_(xmm2, xmm6);                                                         \
			xmm3 = _op_(xmm3, xmm7);                                                         \
			STORE_SI128(dptr, xmm0);                                                         \
			dptr += (16 / sizeof(_type_));                                                   \
			STORE_SI128(dptr, xmm1);                                                         \
			dptr += (16 / sizeof(_type_));                                                   \
			STORE_SI128(dptr, xmm2);                                                         \
			dptr += (16 / sizeof(_type_));                                                   \
			STORE_SI128(dptr, xmm3);                                                         \
			dptr += (16 / sizeof(_type_));                                                   \
		}                                                                                    \
		/* Use a single 128-bit SSE register. */                                             \
		count = len >> (5 - shifts);                                                         \
		len -= count << (5 - shifts);                                                        \
		while (count--)                                                                      \
		{                                                                                    \
			__m128i xmm0 = LOAD_SI128(sptr1);                                                \
			sptr1 += (16 / sizeof(_type_));                                                  \
			__m128i xmm1 = LOAD_SI128(sptr2);                                                \
			sptr2 += (16 / sizeof(_type_));                                                  \
			xmm0 = _op_(xmm0, xmm1);                                                         \
			STORE_SI128(dptr, xmm0);                                                         \
			dptr += (16 / sizeof(_type_));                                                   \
		}                                                                                    \
		/* Finish off the remainder. */                                                      \
		while (len--)                                                                        \
		{                                                                                    \
			_slowWay_;                                                                       \
		}                                                                                    \
		return PRIMITIVES_SUCCESS;                                                           \
	}
