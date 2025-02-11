/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP primitives SSE implementation
 *
 * Copyright 2025 Armin Novak <armin.novak@thincast.com>
 * Copyright 2025 Thincast Technologies GmbH
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
#pragma once

#include <winpr/cast.h>

#include "../../core/simd.h"

#if defined(SSE_AVX_INTRINSICS_ENABLED)
#include <emmintrin.h>
#include <pmmintrin.h>

static inline __m128i mm_set_epu32(uint32_t val1, uint32_t val2, uint32_t val3, uint32_t val4)
{
	return _mm_set_epi32(WINPR_CXX_COMPAT_CAST(int32_t, val1), WINPR_CXX_COMPAT_CAST(int32_t, val2),
	                     WINPR_CXX_COMPAT_CAST(int32_t, val3),
	                     WINPR_CXX_COMPAT_CAST(int32_t, val4));
}

static inline __m128i mm_set_epu8(uint8_t val1, uint8_t val2, uint8_t val3, uint8_t val4,
                                  uint8_t val5, uint8_t val6, uint8_t val7, uint8_t val8,
                                  uint8_t val9, uint8_t val10, uint8_t val11, uint8_t val12,
                                  uint8_t val13, uint8_t val14, uint8_t val15, uint8_t val16)
{
	return _mm_set_epi8(WINPR_CXX_COMPAT_CAST(int8_t, val1), WINPR_CXX_COMPAT_CAST(int8_t, val2),
	                    WINPR_CXX_COMPAT_CAST(int8_t, val3), WINPR_CXX_COMPAT_CAST(int8_t, val4),
	                    WINPR_CXX_COMPAT_CAST(int8_t, val5), WINPR_CXX_COMPAT_CAST(int8_t, val6),
	                    WINPR_CXX_COMPAT_CAST(int8_t, val7), WINPR_CXX_COMPAT_CAST(int8_t, val8),
	                    WINPR_CXX_COMPAT_CAST(int8_t, val9), WINPR_CXX_COMPAT_CAST(int8_t, val10),
	                    WINPR_CXX_COMPAT_CAST(int8_t, val11), WINPR_CXX_COMPAT_CAST(int8_t, val12),
	                    WINPR_CXX_COMPAT_CAST(int8_t, val13), WINPR_CXX_COMPAT_CAST(int8_t, val14),
	                    WINPR_CXX_COMPAT_CAST(int8_t, val15), WINPR_CXX_COMPAT_CAST(int8_t, val16));
}

static inline __m128i mm_set1_epu32(uint32_t val)
{
	return _mm_set1_epi32(WINPR_CXX_COMPAT_CAST(int32_t, val));
}

static inline __m128i mm_set1_epu8(uint8_t val)
{
	return _mm_set1_epi8(WINPR_CXX_COMPAT_CAST(int8_t, val));
}

static inline __m128i LOAD_SI128(const void* ptr)
{
	const __m128i* mptr = WINPR_CXX_COMPAT_CAST(const __m128i*, ptr);
	return _mm_lddqu_si128(mptr);
}

static inline void STORE_SI128(void* ptr, __m128i val)
{
	__m128i* mptr = WINPR_CXX_COMPAT_CAST(__m128i*, ptr);
	_mm_storeu_si128(mptr, val);
}

#endif
