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
#ifdef __cplusplus
extern "C"
{
#endif
#define WINPR_ASSERT(cond)                                           \
	do                                                               \
	{                                                                \
		WINPR_PRAGMA_DIAG_PUSH                                       \
		WINPR_PRAGMA_DIAG_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE \
		if (!(cond))                                                 \
			winpr_int_assert(#cond, __FILE__, __func__, __LINE__);   \
		WINPR_PRAGMA_DIAG_POP                                        \
	} while (0)

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
#define WINPR_ASSERT(cond) assert(cond)
#endif

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
