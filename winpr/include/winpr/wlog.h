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

WINPR_API void WLog_LogA(int logLevel, LPCSTR loggerName, LPCSTR logMessage, ...);
WINPR_API void WLog_TraceA(LPCSTR loggerName, LPCSTR logMessage, ...);
WINPR_API void WLog_DebugA(LPCSTR loggerName, LPCSTR logMessage, ...);
WINPR_API void WLog_InfoA(LPCSTR loggerName, LPCSTR logMessage, ...);
WINPR_API void WLog_WarnA(LPCSTR loggerName, LPCSTR logMessage, ...);
WINPR_API void WLog_ErrorA(LPCSTR loggerName, LPCSTR logMessage, ...);
WINPR_API void WLog_FatalA(LPCSTR loggerName, LPCSTR logMessage, ...);

#ifdef UNICODE
#define WLog_Log	WLog_LogW
#define WLog_Trace	WLog_TraceW
#define WLog_Debug	WLog_DebugW
#define WLog_Info	WLog_InfoW
#define WLog_Warn	WLog_WarnW
#define WLog_Error	WLog_ErrorW
#define WLog_Fatal	WLog_FatalW
#else
#define WLog_Log	WLog_LogA
#define WLog_Trace	WLog_TraceA
#define WLog_Debug	WLog_DebugA
#define WLog_Info	WLog_InfoA
#define WLog_Warn	WLog_WarnA
#define WLog_Error	WLog_ErrorA
#define WLog_Fatal	WLog_FatalA
#endif

#endif /* WINPR_WLOG_H */
