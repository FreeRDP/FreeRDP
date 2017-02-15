/**
 * WinPR: Windows Portable Runtime
 * WinPR Logger
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 Bernhard Miklautz <bernhard.miklautz@thincast.com>
 *
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

#include <stdarg.h>
#include <winpr/wtypes.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

/**
 * Log Levels
 */
#define WLOG_TRACE  0
#define WLOG_DEBUG  1
#define WLOG_INFO   2
#define WLOG_WARN   3
#define WLOG_ERROR  4
#define WLOG_FATAL  5
#define WLOG_OFF    6
#define WLOG_LEVEL_INHERIT 0xFFFF

/**
 * Log Message
 */
#define WLOG_MESSAGE_TEXT      0
#define WLOG_MESSAGE_DATA      1
#define WLOG_MESSAGE_IMAGE     2
#define WLOG_MESSAGE_PACKET    3

/**
 * Log Appenders
 */
#define WLOG_APPENDER_CONSOLE   0
#define WLOG_APPENDER_FILE      1
#define WLOG_APPENDER_BINARY    2
#define WLOG_APPENDER_CALLBACK  3
#define WLOG_APPENDER_SYSLOG    4
#define WLOG_APPENDER_JOURNALD  5
#define WLOG_APPENDER_UDP       6

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
typedef struct _wLogMessage wLogMessage;
typedef struct _wLogLayout wLogLayout;
typedef struct _wLogAppender wLogAppender;
typedef struct _wLog wLog;

#define WLOG_PACKET_INBOUND     1
#define WLOG_PACKET_OUTBOUND    2

WINPR_API BOOL WLog_PrintMessage(wLog* log, DWORD type, DWORD level, DWORD line,
                                 const char* file, const char* function, ...);
WINPR_API BOOL WLog_PrintMessageVA(wLog* log, DWORD type, DWORD level,
                                   DWORD line,
                                   const char* file, const char* function, va_list args);

#define WLog_Print(_log, _log_level, ...) \
	do { \
		if (_log && _log_level >= WLog_GetLogLevel(_log)) { \
			WLog_PrintMessage(_log, WLOG_MESSAGE_TEXT, _log_level, \
			                  __LINE__, __FILE__, __FUNCTION__, __VA_ARGS__ ); \
		} \
	} while (0)

#define WLog_PrintVA(_log, _log_level, _args) \
	do { \
		if (_log && _log_level >= WLog_GetLogLevel(_log)) { \
			WLog_PrintMessageVA(_log, WLOG_MESSAGE_TEXT, _log_level, \
			                    __LINE__, __FILE__, __FUNCTION__, _args ); \
		} \
	} while (0)

#define WLog_Data(_log, _log_level, ...) \
	do { \
		if (_log && _log_level >= WLog_GetLogLevel(_log)) { \
			WLog_PrintMessage(_log, WLOG_MESSAGE_DATA, _log_level, \
			                  __LINE__, __FILE__, __FUNCTION__, __VA_ARGS__ ); \
		} \
	} while (0)

#define WLog_Image(_log, _log_level, ...) \
	do { \
		if (_log && _log_level >= WLog_GetLogLevel(_log)) { \
			WLog_PrintMessage(_log, WLOG_MESSAGE_DATA, _log_level, \
			                  __LINE__, __FILE__, __FUNCTION__, __VA_ARGS__ ); \
		} \
	} while (0)

#define WLog_Packet(_log, _log_level, ...) \
	do { \
		if (_log && _log_level >= WLog_GetLogLevel(_log)) { \
			WLog_PrintMessage(_log, WLOG_MESSAGE_PACKET, _log_level, \
			                  __LINE__, __FILE__, __FUNCTION__, __VA_ARGS__ ); \
		} \
	} while (0)

#define WLog_LVL(tag, lvl, ...) WLog_Print(WLog_Get(tag), lvl, __VA_ARGS__)
#define WLog_VRB(tag, ...) WLog_Print(WLog_Get(tag), WLOG_TRACE, __VA_ARGS__)
#define WLog_DBG(tag, ...) WLog_Print(WLog_Get(tag), WLOG_DEBUG, __VA_ARGS__)
#define WLog_INFO(tag, ...) WLog_Print(WLog_Get(tag), WLOG_INFO, __VA_ARGS__)
#define WLog_WARN(tag, ...) WLog_Print(WLog_Get(tag), WLOG_WARN, __VA_ARGS__)
#define WLog_ERR(tag, ...) WLog_Print(WLog_Get(tag), WLOG_ERROR, __VA_ARGS__)
#define WLog_FATAL(tag, ...) WLog_Print(WLog_Get(tag), WLOG_FATAL, __VA_ARGS__)

WINPR_API DWORD WLog_GetLogLevel(wLog* log);
WINPR_API BOOL WLog_SetLogLevel(wLog* log, DWORD logLevel);
WINPR_API BOOL WLog_SetStringLogLevel(wLog* log, LPCSTR level);
WINPR_API BOOL WLog_AddStringLogFilters(LPCSTR filter);

static INLINE BOOL WLog_IsLevelActive(wLog* _log, DWORD _log_level)
{
	return _log ? _log_level >= WLog_GetLogLevel(_log) : FALSE;
}

WINPR_API BOOL WLog_SetLogAppenderType(wLog* log, DWORD logAppenderType);
WINPR_API wLogAppender* WLog_GetLogAppender(wLog* log);
WINPR_API BOOL WLog_OpenAppender(wLog* log);
WINPR_API BOOL WLog_CloseAppender(wLog* log);
WINPR_API BOOL WLog_ConfigureAppender(wLogAppender* appender,
                                      const char* setting, void* value);

WINPR_API wLogLayout* WLog_GetLogLayout(wLog* log);
WINPR_API BOOL WLog_Layout_SetPrefixFormat(wLog* log, wLogLayout* layout,
        const char* format);

WINPR_API wLog* WLog_GetRoot(void);
WINPR_API wLog* WLog_Get(LPCSTR name);

WINPR_API BOOL WLog_Init(void);
WINPR_API BOOL WLog_Uninit(void);

typedef BOOL (*wLogCallbackMessage_t)(const wLogMessage* msg);
typedef BOOL (*wLogCallbackData_t)(const wLogMessage* msg);
typedef BOOL (*wLogCallbackImage_t)(const wLogMessage* msg);
typedef BOOL (*wLogCallbackPackage_t)(const wLogMessage* msg);

struct _wLogCallbacks
{
	wLogCallbackData_t data;
	wLogCallbackImage_t  image;
	wLogCallbackMessage_t message;
	wLogCallbackPackage_t  package;
};
typedef struct _wLogCallbacks wLogCallbacks;

#ifdef __cplusplus
}
#endif

#endif /* WINPR_WLOG_H */
