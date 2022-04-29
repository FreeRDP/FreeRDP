/**
 * WinPR: Windows Portable Runtime
 * WinPR Debugging helpers
 *
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
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

#ifndef WINPR_DEBUG_EXECINFO_H
#define WINPR_DEBUG_EXECINFO_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <winpr/wtypes.h>

	void* winpr_execinfo_backtrace(DWORD size);
	void winpr_execinfo_backtrace_free(void* buffer);
	char** winpr_execinfo_backtrace_symbols(void* buffer, size_t* used);
	void winpr_execinfo_backtrace_symbols_fd(void* buffer, int fd);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_DEBUG_EXECINFO_H */
