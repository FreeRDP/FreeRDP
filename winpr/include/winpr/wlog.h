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

typedef struct _wLog wLog;
typedef struct _wLogMessage wLogMessage;
typedef struct _wLogAppender wLogAppender;

/**
 * Log Levels
 */

#define WLOG_TRACE		0
#define WLOG_DEBUG		1
#define WLOG_INFO		2
#define WLOG_WARN		3
#define WLOG_ERROR		4
#define WLOG_FATAL		5
#define WLOG_OFF		6

/**
 * Log Message
 */

#define WLOG_MESSAGE_STRING	0

struct _wLogMessage
{
	DWORD Type;

	LPSTR TextString;
};

/**
 * Log Appenders
 */

#define WLOG_APPENDER_CONSOLE	0
#define WLOG_APPENDER_FILE	1

typedef int (*WLOG_APPENDER_WRITE_MESSAGE_FN)(wLog* log, wLogAppender* appender, DWORD logLevel, wLogMessage* logMessage);

struct _wLogAppender
{
	DWORD Type;
	WLOG_APPENDER_WRITE_MESSAGE_FN WriteMessage;
};

#define WLOG_CONSOLE_STDOUT	1
#define WLOG_CONSOLE_STDERR	2

struct _wLogConsoleAppender
{
	DWORD Type;
	WLOG_APPENDER_WRITE_MESSAGE_FN WriteMessage;

	int outputStream;
};
typedef struct _wLogConsoleAppender wLogConsoleAppender;

struct _wLogFileAppender
{
	DWORD Type;
	WLOG_APPENDER_WRITE_MESSAGE_FN WriteMessage;


};
typedef struct _wLogFileAppender wLogFileAppender;

/**
 * Logger
 */

struct _wLog
{
	LPSTR Name;
	DWORD Level;

	wLogAppender* Appender;
};

WINPR_API void WLog_Print(wLog* log, DWORD logLevel, LPCSTR logMessage, ...);

WINPR_API DWORD WLog_GetLogLevel(wLog* log);
WINPR_API void WLog_SetLogLevel(wLog* log, DWORD logLevel);

WINPR_API wLogAppender* WLog_GetLogAppender(wLog* log);
WINPR_API void WLog_SetLogAppenderType(wLog* log, DWORD logAppenderType);

WINPR_API void WLog_ConsoleAppender_SetOutputStream(wLog* log, wLogConsoleAppender* consoleAppender, int outputStream);

WINPR_API wLog* WLog_New(LPCSTR name);
WINPR_API void WLog_Free(wLog* log);

#endif /* WINPR_WLOG_H */
