/**
 * WinPR: Windows Portable Runtime
 * Runtime ASSERT macros
 *
 * Copyright 2021 Armin Novak <armin.novak@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#ifndef WINPR_ASSERT_H
#define WINPR_ASSERT_H

#include <stdlib.h>
#include <assert.h>

#include <winpr/winpr.h>
#include <winpr/wtypes.h>
#include <winpr/wlog.h>
#include <winpr/debug.h>

#if defined(WITH_VERBOSE_WINPR_ASSERT) && (WITH_VERBOSE_WINPR_ASSERT != 0)
#define winpr_internal_assert(cond, file, fkt, line)        \
	do                                                      \
	{                                                       \
		if (!(cond))                                        \
			winpr_int_assert(#cond, (file), (fkt), (line)); \
	} while (0)

#ifdef __cplusplus
extern "C"
{
#endif

	static INLINE WINPR_NORETURN(void winpr_int_assert(const char* condstr, const char* file,
	                                                   const char* fkt, size_t line))
	{
		wLog* _log_cached_ptr = WLog_Get("com.freerdp.winpr.assert");
		WLog_Print(_log_cached_ptr, WLOG_FATAL, "%s [%s:%s:%" PRIuz "]", condstr, file, fkt, line);
		winpr_log_backtrace_ex(_log_cached_ptr, WLOG_FATAL, 20);
		abort();
	}

#ifdef __cplusplus
}
#endif

#else
#define winpr_internal_assert(cond, file, fkt, line) assert(cond)
#endif

#define WINPR_ASSERT_AT(cond, file, fkt, line)                                                    \
	do                                                                                            \
	{                                                                                             \
		WINPR_PRAGMA_DIAG_PUSH                                                                    \
		WINPR_PRAGMA_DIAG_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE                              \
		WINPR_PRAGMA_DIAG_TAUTOLOGICAL_VALUE_RANGE_COMPARE                                        \
		WINPR_PRAGMA_DIAG_IGNORED_UNKNOWN_PRAGMAS                                                 \
		WINPR_DO_COVERITY_PRAGMA(coverity compliance block \(deviate "CONSTANT_EXPRESSION_RESULT" \
		                                                             "WINPR_ASSERT" \)          \
	    \(deviate "NO_EFFECT"                                                                       \
		        "WINPR_ASSERT" \))                                                                \
                                                                                                  \
		winpr_internal_assert((cond), (file), (fkt), (line));                                     \
                                                                                                  \
		WINPR_DO_COVERITY_PRAGMA(coverity compliance end_block "CONSTANT_EXPRESSION_RESULT"       \
		                                                       "NO_EFFECT")                       \
		WINPR_PRAGMA_DIAG_POP                                                                     \
	} while (0)
#define WINPR_ASSERT(cond) WINPR_ASSERT_AT((cond), __FILE__, __func__, __LINE__)

#ifdef __cplusplus
extern "C"
{
#endif
#if defined(__cplusplus) && (__cplusplus >= 201703L) // C++ 17
#define WINPR_STATIC_ASSERT(cond) static_assert(cond)
#elif defined(__cplusplus) && (__cplusplus >= 201103L) // C++ 11
#define WINPR_STATIC_ASSERT(cond) static_assert(cond, #cond)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202311L) // C23
#define WINPR_STATIC_ASSERT(cond) static_assert(cond)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) // C11
#define WINPR_STATIC_ASSERT(cond) _Static_assert(cond, #cond)
#else
WINPR_PRAGMA_WARNING("static-assert macro not supported on this platform")
#define WINPR_STATIC_ASSERT(cond) assert(cond)
#endif

#ifdef __cplusplus
}
#endif

#endif /* WINPR_ERROR_H */
