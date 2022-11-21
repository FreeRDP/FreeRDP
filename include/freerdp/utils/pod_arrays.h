/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * POD arrays
 *
 * Copyright 2022 David Fort <contact@hardening-consulting.com>
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
#ifndef FREERDP_UTILS_POD_ARRAYS_H_
#define FREERDP_UTILS_POD_ARRAYS_H_

#include <winpr/wtypes.h>
#include <winpr/assert.h>

#define POD_ARRAYS_IMPL(T, TLOWER)                                                        \
	typedef struct                                                                        \
	{                                                                                     \
		T* values;                                                                        \
		size_t nvalues;                                                                   \
	} Array##T;                                                                           \
	typedef BOOL Array##T##Cb(T* v, void* data);                                          \
                                                                                          \
	static INLINE void array_##TLOWER##_init(Array##T* a)                                 \
	{                                                                                     \
		WINPR_ASSERT(a);                                                                  \
		a->values = NULL;                                                                 \
		a->nvalues = 0;                                                                   \
	}                                                                                     \
                                                                                          \
	static INLINE size_t array_##TLOWER##_size(const Array##T* a)                         \
	{                                                                                     \
		WINPR_ASSERT(a);                                                                  \
		return a->nvalues;                                                                \
	}                                                                                     \
                                                                                          \
	static INLINE T* array_##TLOWER##_data(const Array##T* a)                             \
	{                                                                                     \
		WINPR_ASSERT(a);                                                                  \
		return a->values;                                                                 \
	}                                                                                     \
                                                                                          \
	static INLINE const T* array_##TLOWER##_cdata(const Array##T* a)                      \
	{                                                                                     \
		WINPR_ASSERT(a);                                                                  \
		return (const T*)a->values;                                                       \
	}                                                                                     \
                                                                                          \
	static INLINE T array_##TLOWER##_get(const Array##T* a, size_t idx)                   \
	{                                                                                     \
		WINPR_ASSERT(a);                                                                  \
		WINPR_ASSERT(a->nvalues > idx);                                                   \
		return a->values[idx];                                                            \
	}                                                                                     \
                                                                                          \
	static INLINE void array_##TLOWER##_set(Array##T* a, size_t idx, T v)                 \
	{                                                                                     \
		WINPR_ASSERT(a);                                                                  \
		WINPR_ASSERT(a->nvalues > idx);                                                   \
		a->values[idx] = v;                                                               \
	}                                                                                     \
                                                                                          \
	static INLINE BOOL array_##TLOWER##_append(Array##T* a, T v)                          \
	{                                                                                     \
		WINPR_ASSERT(a);                                                                  \
		T* tmp = realloc(a->values, sizeof(T) * (a->nvalues + 1));                        \
		if (!tmp)                                                                         \
			return FALSE;                                                                 \
                                                                                          \
		tmp[a->nvalues] = v;                                                              \
		a->values = tmp;                                                                  \
		a->nvalues++;                                                                     \
		return TRUE;                                                                      \
	}                                                                                     \
                                                                                          \
	static INLINE BOOL array_##TLOWER##_contains(const Array##T* a, T v)                  \
	{                                                                                     \
		WINPR_ASSERT(a);                                                                  \
		UINT32 i;                                                                         \
                                                                                          \
		for (i = 0; i < a->nvalues; i++)                                                  \
		{                                                                                 \
			if (memcmp(&a->values[i], &v, sizeof(T)) == 0)                                \
				return TRUE;                                                              \
		}                                                                                 \
                                                                                          \
		return FALSE;                                                                     \
	}                                                                                     \
                                                                                          \
	static INLINE BOOL array_##TLOWER##_foreach(Array##T* a, Array##T##Cb cb, void* data) \
	{                                                                                     \
		WINPR_ASSERT(a);                                                                  \
		size_t i;                                                                         \
		for (i = 0; i < a->nvalues; i++)                                                  \
		{                                                                                 \
			if (!cb(&a->values[i], data))                                                 \
				return FALSE;                                                             \
		}                                                                                 \
                                                                                          \
		return TRUE;                                                                      \
	}                                                                                     \
                                                                                          \
	static INLINE void array_##TLOWER##_reset(Array##T* a)                                \
	{                                                                                     \
		WINPR_ASSERT(a);                                                                  \
		a->nvalues = 0;                                                                   \
	}                                                                                     \
                                                                                          \
	static INLINE void array_##TLOWER##_uninit(Array##T* a)                               \
	{                                                                                     \
		WINPR_ASSERT(a);                                                                  \
		free(a->values);                                                                  \
                                                                                          \
		a->values = NULL;                                                                 \
		a->nvalues = 0;                                                                   \
	}

#ifdef __cplusplus
extern "C"
{
#endif

	POD_ARRAYS_IMPL(UINT16, uint16)
	POD_ARRAYS_IMPL(UINT32, uint32)
	POD_ARRAYS_IMPL(UINT64, uint64)

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_UTILS_POD_ARRAYS_H_ */
