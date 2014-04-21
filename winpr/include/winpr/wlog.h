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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#include <winpr/synch.h>
#include <winpr/thread.h>

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
#define WLOG_LEVEL_INHERIT 0xFFFF

/**
 * Log Message
 */

#define WLOG_MESSAGE_TEXT	0
#define WLOG_MESSAGE_DATA	1
#define WLOG_MESSAGE_IMAGE	2
#define WLOG_MESSAGE_PACKET	3

struct _wLogMessage
{
	DWORD Type;

	DWORD Level;

	LPSTR PrefixString;

	LPCSTR FormatString;
	LPSTR TextString;

	DWORD LineNumber; /* __LINE__ */
	LPCSTR FileName; /* __FILE__ */
	LPCSTR FunctionName; /* __FUNCTION__ */

	/* Data Message */

	void* Data;
	int Length;

	/* Image Message */

	void* ImageData;
	int ImageWidth;
	int ImageHeight;
	int ImageBpp;

	/* Packet Message */

	void* PacketData;
	int PacketLength;
	DWORD PacketFlags;
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
#define WLOG_APPENDER_BINARY	2

#define WLOG_PACKET_INBOUND	1
#define WLOG_PACKET_OUTBOUND	2

typedef int (*WLOG_APPENDER_OPEN_FN)(wLog* log, wLogAppender* appender);
typedef int (*WLOG_APPENDER_CLOSE_FN)(wLog* log, wLogAppender* appender);
typedef int (*WLOG_APPENDER_WRITE_MESSAGE_FN)(wLog* log, wLogAppender* appender, wLogMessage* message);
typedef int (*WLOG_APPENDER_WRITE_DATA_MESSAGE_FN)(wLog* log, wLogAppender* appender, wLogMessage* message);
typedef int (*WLOG_APPENDER_WRITE_IMAGE_MESSAGE_FN)(wLog* log, wLogAppender* appender, wLogMessage* message);
typedef int (*WLOG_APPENDER_WRITE_PACKET_MESSAGE_FN)(wLog* log, wLogAppender* appender, wLogMessage* message);

#define WLOG_APPENDER_COMMON() \
	DWORD Type; \
	DWORD State; \
	wLogLayout* Layout; \
	CRITICAL_SECTION lock; \
	void* TextMessageContext; \
	void* DataMessageContext; \
	void* ImageMessageContext; \
	void* PacketMessageContext; \
	WLOG_APPENDER_OPEN_FN Open; \
	WLOG_APPENDER_CLOSE_FN Close; \
	WLOG_APPENDER_WRITE_MESSAGE_FN WriteMessage; \
	WLOG_APPENDER_WRITE_DATA_MESSAGE_FN WriteDataMessage; \
	WLOG_APPENDER_WRITE_IMAGE_MESSAGE_FN WriteImageMessage; \
	WLOG_APPENDER_WRITE_PACKET_MESSAGE_FN WritePacketMessage

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
	char* FilePath;
	char* FullFileName;
	FILE* FileDescriptor;
};
typedef struct _wLogFileAppender wLogFileAppender;

struct _wLogBinaryAppender
{
	WLOG_APPENDER_COMMON();

	char* FileName;
	char* FilePath;
	char* FullFileName;
	FILE* FileDescriptor;
};
typedef struct _wLogBinaryAppender wLogBinaryAppender;

/**
 * Logger
 */

struct _wLog
{
	LPSTR Name;
	DWORD Level;

	BOOL IsRoot;
	LPSTR* Names;
	DWORD NameCount;
	wLogAppender* Appender;

	wLog* Parent;
	wLog** Children;
	DWORD ChildrenCount;
	DWORD ChildrenSize;
};

WINPR_API void WLog_PrintMessage(wLog* log, wLogMessage* message, ...);

#define WLog_Print(_log, _log_level, _fmt, ...) \
	if (_log_level >= WLog_GetLogLevel(_log)) { \
		wLogMessage _log_message; \
		_log_message.Type = WLOG_MESSAGE_TEXT; \
		_log_message.Level = _log_level; \
		_log_message.FormatString = _fmt; \
		_log_message.LineNumber = __LINE__; \
		_log_message.FileName = __FILE__; \
		_log_message.FunctionName = __FUNCTION__; \
		WLog_PrintMessage(_log, &(_log_message), ## __VA_ARGS__ ); \
	}

#define WLog_Data(_log, _log_level, ...) \
	if (_log_level >= WLog_GetLogLevel(_log)) { \
		wLogMessage _log_message; \
		_log_message.Type = WLOG_MESSAGE_DATA; \
		_log_message.Level = _log_level; \
		_log_message.FormatString = NULL; \
		_log_message.LineNumber = __LINE__; \
		_log_message.FileName = __FILE__; \
		_log_message.FunctionName = __FUNCTION__; \
		WLog_PrintMessage(_log, &(_log_message), ## __VA_ARGS__ ); \
	}

#define WLog_Image(_log, _log_level, ...) \
	if (_log_level >= WLog_GetLogLevel(_log)) { \
		wLogMessage _log_message; \
		_log_message.Type = WLOG_MESSAGE_IMAGE; \
		_log_message.Level = _log_level; \
		_log_message.FormatString = NULL; \
		_log_message.LineNumber = __LINE__; \
		_log_message.FileName = __FILE__; \
		_log_message.FunctionName = __FUNCTION__; \
		WLog_PrintMessage(_log, &(_log_message), ## __VA_ARGS__ ); \
	}

#define WLog_Packet(_log, _log_level, ...) \
	if (_log_level >= WLog_GetLogLevel(_log)) { \
		wLogMessage _log_message; \
		_log_message.Type = WLOG_MESSAGE_PACKET; \
		_log_message.Level = _log_level; \
		_log_message.FormatString = NULL; \
		_log_message.LineNumber = __LINE__; \
		_log_message.FileName = __FILE__; \
		_log_message.FunctionName = __FUNCTION__; \
		WLog_PrintMessage(_log, &(_log_message), ## __VA_ARGS__ ); \
	}

#define WLog_IsLevelActive(_log, _log_level) \
	(_log_level >= WLog_GetLogLevel(_log))

WINPR_API DWORD WLog_GetLogLevel(wLog* log);
WINPR_API void WLog_SetLogLevel(wLog* log, DWORD logLevel);

WINPR_API wLogAppender* WLog_GetLogAppender(wLog* log);
WINPR_API void WLog_SetLogAppenderType(wLog* log, DWORD logAppenderType);

WINPR_API int WLog_OpenAppender(wLog* log);
WINPR_API int WLog_CloseAppender(wLog* log);

WINPR_API void WLog_ConsoleAppender_SetOutputStream(wLog* log, wLogConsoleAppender* appender, int outputStream);

WINPR_API void WLog_FileAppender_SetOutputFileName(wLog* log, wLogFileAppender* appender, const char* filename);
WINPR_API void WLog_FileAppender_SetOutputFilePath(wLog* log, wLogFileAppender* appender, const char* filepath);

WINPR_API wLogLayout* WLog_GetLogLayout(wLog* log);
WINPR_API void WLog_Layout_SetPrefixFormat(wLog* log, wLogLayout* layout, const char* format);

WINPR_API wLog* WLog_GetRoot(void);
WINPR_API wLog* WLog_Get(LPCSTR name);

WINPR_API void WLog_Init(void);
WINPR_API void WLog_Uninit(void);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_WLOG_H */
