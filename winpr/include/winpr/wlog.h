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

#include <stdio.h>

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

typedef struct _wLog wLog;
typedef struct _wLogMessage wLogMessage;
typedef struct _wLogLayout wLogLayout;
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

	DWORD Level;

	LPSTR PrefixString;

	LPSTR FormatString;
	LPSTR TextString;

	DWORD LineNumber; /* __LINE__ */
	LPCSTR FileName; /* __FILE__ */
	LPCSTR FunctionName; /* __FUNCTION__ */
};

/**
 * Log Layout
 */

struct _wLogLayout
{
	DWORD Type;

	LPSTR FormatString;
};

/**
 * Log Appenders
 */

#define WLOG_APPENDER_CONSOLE	0
#define WLOG_APPENDER_FILE	1

typedef int (*WLOG_APPENDER_OPEN_FN)(wLog* log, wLogAppender* appender);
typedef int (*WLOG_APPENDER_CLOSE_FN)(wLog* log, wLogAppender* appender);
typedef int (*WLOG_APPENDER_WRITE_MESSAGE_FN)(wLog* log, wLogAppender* appender, wLogMessage* message);

#define WLOG_APPENDER_COMMON() \
	DWORD Type; \
	wLogLayout* Layout; \
	WLOG_APPENDER_OPEN_FN Open; \
	WLOG_APPENDER_CLOSE_FN Close; \
	WLOG_APPENDER_WRITE_MESSAGE_FN WriteMessage

struct _wLogAppender
{
	WLOG_APPENDER_COMMON();
};

#define WLOG_CONSOLE_STDOUT	1
#define WLOG_CONSOLE_STDERR	2

struct _wLogConsoleAppender
{
	WLOG_APPENDER_COMMON();

	int outputStream;
};
typedef struct _wLogConsoleAppender wLogConsoleAppender;

struct _wLogFileAppender
{
	WLOG_APPENDER_COMMON();

	char* FileName;
	FILE* FileDescriptor;
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

WINPR_API void WLog_PrintMessage(wLog* log, wLogMessage* message, ...);

#define WLog_Print(_log, _log_level, _fmt, ...) \
	if (_log_level <= _log->Level) { \
		wLogMessage _log_message; \
		_log_message.Type = WLOG_MESSAGE_STRING; \
		_log_message.Level = _log_level; \
		_log_message.FormatString = _fmt; \
		_log_message.LineNumber = __LINE__; \
		_log_message.FileName = __FILE__; \
		_log_message.FunctionName = __FUNCTION__; \
		WLog_PrintMessage(_log, &(_log_message), ## __VA_ARGS__ ); \
	}

WINPR_API DWORD WLog_GetLogLevel(wLog* log);
WINPR_API void WLog_SetLogLevel(wLog* log, DWORD logLevel);

WINPR_API wLogAppender* WLog_GetLogAppender(wLog* log);
WINPR_API void WLog_SetLogAppenderType(wLog* log, DWORD logAppenderType);

WINPR_API int WLog_OpenAppender(wLog* log);
WINPR_API int WLog_CloseAppender(wLog* log);

WINPR_API void WLog_ConsoleAppender_SetOutputStream(wLog* log, wLogConsoleAppender* appender, int outputStream);

WINPR_API void WLog_FileAppender_SetOutputFileName(wLog* log, wLogFileAppender* appender, const char* filename);

WINPR_API wLogLayout* WLog_GetLogLayout(wLog* log);
WINPR_API void WLog_Layout_SetPrefixFormat(wLog* log, wLogLayout* layout, const char* format);

WINPR_API wLog* WLog_New(LPCSTR name);
WINPR_API void WLog_Free(wLog* log);

#endif /* WINPR_WLOG_H */
