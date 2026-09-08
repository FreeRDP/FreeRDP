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

#include <winpr/platform.h>
#include <winpr/assert-api.h>

/**
 * @brief C++ safe cast macro
 * @since version 3.10.1
 */
#ifdef __cplusplus
#define WINPR_CXX_COMPAT_CAST(t, val) static_cast<t>(val)
#else
#define WINPR_CXX_COMPAT_CAST(t, val) ((t)(val))
#endif

/**! @brief Checks alignment requirements.
 * - On supported platforms (arm, arm64) check that unaligned access is enabled or die
 * - On unsupported platforms print a compiler warning
 */
#if defined(DISABLE_SUPPORTED_ARCH_CHECKS)
#elif defined(_M_ARM) || defined(_M_ARM64)
#if !defined(__ARM_FEATURE_UNALIGNED)
#warning \
    "-munaligned-access is required on arm. Use -DDISABLE_SUPPORTED_ARCH_CHECKS=ON to ignore, but there will be dragons ahead!"
#else
#define WINPR_ARCH_SUPPORTED 1
#endif
#elif defined(_M_IX86) || defined(_M_AMD64)
#define WINPR_ARCH_SUPPORTED 1
#elif defined(_M_RISCV32) || defined(_M_RISCV64)
#if !defined(__riscv_misaligned_fast)
#warning \
    "RISCV must support __riscv_misaligned_fast. Use -DDISABLE_SUPPORTED_ARCH_CHECKS=ON to ignore, but there will be dragons ahead!"
#else
#define WINPR_ARCH_SUPPORTED 1
#endif
#endif

#if !defined(WINPR_ARCH_SUPPORTED)
#warning "unaligned pointer access not verified on platform, SIGBUS may happen!"
#endif

/**! @brief Cast to \ref t and silence Wcast-align warnings.
 *
 * This just silences compiler warnings on supported architectures.
 * Support is checked at compile time.
 *
 * @since version 3.32.0
 */
#if defined(WINPR_ARCH_SUPPORTED) && !defined(_WIN32)
#define WINPR_PACKED_ALIGN_CAST(t, val)                     \
	__extension__({                                         \
		WINPR_PRAGMA_DIAG_PUSH;                             \
		WINPR_PRAGMA_DIAG_IGNORED_CAST_ALIGN;               \
		typeof(t) aligntmp = WINPR_CXX_COMPAT_CAST(t, val); \
		WINPR_PRAGMA_DIAG_POP;                              \
		aligntmp;                                           \
	})
#else
// Promote any -Wcast-align warnings to errors on platforms not supporting unaligned access
#if defined(DISABLE_SUPPORTED_ARCH_CHECKS)
#if defined(__clang__)
WINPR_DO_PRAGMA(clang diagnostic warning "-Wcast-align")
#elif defined(__GNUC__)
WINPR_DO_PRAGMA(GCC diagnostic warning "-Wcast-align")
#endif
#else
#if defined(__clang__)
WINPR_DO_PRAGMA(clang diagnostic error "-Wcast-align")
#elif defined(__GNUC__)
WINPR_DO_PRAGMA(GCC diagnostic error "-Wcast-align")
#endif
#endif
#define WINPR_PACKED_ALIGN_CAST(t, val) WINPR_CXX_COMPAT_CAST(t, val)
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
