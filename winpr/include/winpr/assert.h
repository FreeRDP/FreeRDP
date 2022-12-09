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
#include <winpr/winpr.h>
#include <winpr/wtypes.h>
#include <winpr/wlog.h>
#include <winpr/debug.h>

#if defined(WITH_VERBOSE_WINPR_ASSERT) && (WITH_VERBOSE_WINPR_ASSERT != 0)
#define WINPR_ASSERT(cond)                                                                    \
	do                                                                                        \
	{                                                                                         \
		if (!(cond))                                                                          \
		{                                                                                     \
			wLog* _log_cached_ptr = WLog_Get("com.freerdp.winpr.assert");                     \
			WLog_Print(_log_cached_ptr, WLOG_FATAL, "%s [%s:%s:%" PRIuz "]", #cond, __FILE__, \
			           __FUNCTION__, __LINE__);                                               \
			winpr_log_backtrace_ex(_log_cached_ptr, WLOG_FATAL, 20);                          \
			abort();                                                                          \
		}                                                                                     \
	} while (0)
#else
#include <assert.h>
#define WINPR_ASSERT(cond) assert(cond)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef __cplusplus
}
#endif

#endif /* WINPR_ERROR_H */
