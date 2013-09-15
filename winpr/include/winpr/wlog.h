/**
 * WinPR: Windows Portable Runtime
 * WinPR Logger
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_LOG_H
#define WINPR_LOG_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#define WLOG_TRACE	0
#define WLOG_DEBUG	1
#define WLOG_INFO	2
#define WLOG_WARN	3
#define WLOG_ERROR	4
#define WLOG_FATAL	5
#define WLOG_OFF	6

struct _wLog
{
	LPSTR Name;
	DWORD Level;
};
typedef struct _wLog wLog;

WINPR_API void WLog_Print(wLog* log, DWORD logLevel, LPCSTR logMessage, ...);

WINPR_API DWORD WLog_GetLogLevel(wLog* log);
WINPR_API void WLog_SetLogLevel(wLog* log, DWORD logLevel);

WINPR_API wLog* WLog_New(LPCSTR name);
WINPR_API void WLog_Free(wLog* log);

#endif /* WINPR_WLOG_H */
