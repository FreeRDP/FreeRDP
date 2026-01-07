/**
 * WinPR: Windows Portable Runtime
 * Cast macros
 *
 * Copyright 2024 Armin Novak <anovak@thincast.com>
 * Copyright 2024 Thincast Technologies GmbH
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

#include <stdint.h>

#include <winpr/assert-api.h>

/**
 * @brief C++ safe cast macro
 * @since version 3.10.1
 */
#ifdef __cplusplus
#define WINPR_CXX_COMPAT_CAST(t, val) static_cast<t>(val)
#else
#define WINPR_CXX_COMPAT_CAST(t, val) (t)(val)
#endif

#if defined(__GNUC__) || defined(__clang__)
/**
 * @brief A macro to do dirty casts. Do not use without a good justification!
 * @param ptr The pointer to cast
 * @param dstType The data type to cast to
 * @return The casted pointer
 * @since version 3.9.0
 */
#define WINPR_REINTERPRET_CAST(ptr, srcType, dstType)            \
	__extension__({                                              \
		union                                                    \
		{                                                        \
			srcType src;                                         \
			dstType dst;                                         \
		} cnv;                                                   \
		WINPR_STATIC_ASSERT(sizeof(srcType) == sizeof(dstType)); \
		cnv.src = ptr;                                           \
		cnv.dst;                                                 \
	})

/**
 * @brief A macro to do dirty casts. Do not use without a good justification!
 * @param ptr The pointer to cast
 * @param dstType The data type to cast to
 * @return The casted pointer
 * @since version 3.9.0
 */
#define WINPR_CAST_CONST_PTR_AWAY(ptr, dstType) \
	__extension__({                             \
		union                                   \
		{                                       \
			__typeof(ptr) src;                  \
			dstType dst;                        \
		} cnv;                                  \
		cnv.src = ptr;                          \
		cnv.dst;                                \
	})

/**
 * @brief A macro to do function pointer casts. Do not use without a good justification!
 * @param ptr The pointer to cast
 * @param dstType The data type to cast to
 * @return The casted pointer
 * @since version 3.9.0
 */
#define WINPR_FUNC_PTR_CAST(ptr, dstType)                              \
	__extension__({                                                    \
		union                                                          \
		{                                                              \
			__typeof(ptr) src;                                         \
			dstType dst;                                               \
		} cnv;                                                         \
		WINPR_STATIC_ASSERT(sizeof(dstType) == sizeof(__typeof(ptr))); \
		cnv.src = ptr;                                                 \
		cnv.dst;                                                       \
	})

#else
#define WINPR_REINTERPRET_CAST(ptr, srcType, dstType) (dstType) ptr
#define WINPR_CAST_CONST_PTR_AWAY(ptr, dstType) (dstType) ptr
#define WINPR_FUNC_PTR_CAST(ptr, dstType) (dstType)(uintptr_t) ptr
#endif

#if defined(__GNUC__) || defined(__clang__)

/**
 * @brief A macro to do checked integer casts.
 * will check if the value does change by casting to and from the target type and comparing the
 * values. will also check if the sign of a value changes during conversion.
 *
 * @param type the type to cast to
 * @param var the integer of unknown type to cast
 * @return The casted integer
 * @since version 3.10.1
 */
#define WINPR_ASSERTING_INT_CAST(type, ivar)                                                    \
	__extension__({                                                                             \
		__typeof(ivar) var = ivar;                                                              \
		WINPR_ASSERT((var) ==                                                                   \
		             WINPR_CXX_COMPAT_CAST(__typeof(var), WINPR_CXX_COMPAT_CAST(type, (var)))); \
		WINPR_ASSERT((((var) > 0) && (WINPR_CXX_COMPAT_CAST(type, (var)) > 0)) ||               \
		             (((var) <= 0) && WINPR_CXX_COMPAT_CAST(type, (var)) <= 0));                \
		WINPR_CXX_COMPAT_CAST(type, (var));                                                     \
	})

#else
#define WINPR_ASSERTING_INT_CAST(type, var) WINPR_CXX_COMPAT_CAST(type, var)
#endif
