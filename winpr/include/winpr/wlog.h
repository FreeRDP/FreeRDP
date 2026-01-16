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
extern "C"
{
#endif

#include <stdarg.h>

#include <winpr/platform.h>
#include <winpr/wtypes.h>
#include <winpr/winpr.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

/**
 * Log Levels
 */
#define WLOG_TRACE 0
#define WLOG_DEBUG 1
#define WLOG_INFO 2
#define WLOG_WARN 3
#define WLOG_ERROR 4
#define WLOG_FATAL 5
#define WLOG_OFF 6
#define WLOG_LEVEL_INHERIT 0xFFFF

/** @defgroup LogMessageTypes Log Message
 *  @{
 */
#define WLOG_MESSAGE_TEXT 0
#define WLOG_MESSAGE_DATA 1
#define WLOG_MESSAGE_IMAGE 2
#define WLOG_MESSAGE_PACKET 3
/**
 * @}
 */

/**
 * Log Appenders
 */
#define WLOG_APPENDER_CONSOLE 0
#define WLOG_APPENDER_FILE 1
#define WLOG_APPENDER_BINARY 2
#define WLOG_APPENDER_CALLBACK 3
#define WLOG_APPENDER_SYSLOG 4
#define WLOG_APPENDER_JOURNALD 5
#define WLOG_APPENDER_UDP 6

	typedef struct
	{
		DWORD Type;

		DWORD Level;

		LPSTR PrefixString;

		LPCSTR FormatString;
		LPCSTR TextString;

		size_t LineNumber;   /* __LINE__ */
		LPCSTR FileName;     /* __FILE__ */
		LPCSTR FunctionName; /* __func__ */

		/* Data Message */

		void* Data;
		size_t Length;

		/* Image Message */

		void* ImageData;
		size_t ImageWidth;
		size_t ImageHeight;
		size_t ImageBpp;

		/* Packet Message */

		void* PacketData;
		size_t PacketLength;
		DWORD PacketFlags;
	} wLogMessage;
	typedef struct s_wLogLayout wLogLayout;
	typedef struct s_wLogAppender wLogAppender;
	typedef struct s_wLog wLog;

#define WLOG_PACKET_INBOUND 1
#define WLOG_PACKET_OUTBOUND 2

	/** @brief specialized function to print text log messages.
	 *  Same as @ref WLog_PrintMessage with \b type = WLOG_MESSAGE_TEXT but with compile time checks
	 * for issues in format string.
	 *
	 *  @param log A pointer to the logger to use
	 *  @param line the file line the log message originates from
	 *  @param file the file name the log message originates from
	 *  @param function the function name the log message originates from
	 *  @param fmt the printf style format string
	 *
	 *  @return \b TRUE for success, \b FALSE otherwise.
	 *  @since version 3.17.0
	 */
	WINPR_ATTR_FORMAT_ARG(6, 7)
	WINPR_API BOOL WLog_PrintTextMessage(wLog* log, DWORD level, size_t line, const char* file,
	                                     const char* function, WINPR_FORMAT_ARG const char* fmt,
	                                     ...);

	/** @brief specialized function to print text log messages.
	 *  Same as @ref WLog_PrintMessageVA with \b type = WLOG_MESSAGE_TEXT but with compile time
	 * checks for issues in format string.
	 *
	 *  @param log A pointer to the logger to use
	 *  @param line the file line the log message originates from
	 *  @param file the file name the log message originates from
	 *  @param function the function name the log message originates from
	 *  @param fmt the printf style format string
	 *
	 *  @return \b TRUE for success, \b FALSE otherwise.
	 *  @since version 3.17.0
	 */
	WINPR_ATTR_FORMAT_ARG(6, 0)
	WINPR_API BOOL WLog_PrintTextMessageVA(wLog* log, DWORD level, size_t line, const char* file,
	                                       const char* function, WINPR_FORMAT_ARG const char* fmt,
	                                       va_list args);

	/** @brief log something of a specified type.
	 *  @bug For /b WLOG_MESSAGE_TEXT the format string is not validated at compile time. Use \ref
	 * WLog_PrintTextMessage instead.
	 *
	 *  @param log A pointer to the logger to use
	 *  @param type The type of message to log, can be any of \ref LogMessageTypes
	 *  @param line the file line the log message originates from
	 *  @param file the file name the log message originates from
	 *  @param function the function name the log message originates from
	 *
	 *  @return \b TRUE for success, \b FALSE otherwise.
	 */
	WINPR_API BOOL WLog_PrintMessage(wLog* log, DWORD type, DWORD level, size_t line,
	                                 const char* file, const char* function, ...);

	/** @brief log something of a specified type.
	 *  @bug For /b WLOG_MESSAGE_TEXT the format string is not validated at compile time. Use \ref
	 * WLog_PrintTextMessageVA instead.
	 *
	 *  @param log A pointer to the logger to use
	 *  @param type The type of message to log, can be any of \ref LogMessageTypes
	 *  @param line the file line the log message originates from
	 *  @param file the file name the log message originates from
	 *  @param function the function name the log message originates from
	 *
	 *  @return \b TRUE for success, \b FALSE otherwise.
	 */
	WINPR_API BOOL WLog_PrintMessageVA(wLog* log, DWORD type, DWORD level, size_t line,
	                                   const char* file, const char* function, va_list args);

	WINPR_API wLog* WLog_GetRoot(void);
	WINPR_API wLog* WLog_Get(LPCSTR name);
	WINPR_API DWORD WLog_GetLogLevel(wLog* log);
	WINPR_API BOOL WLog_IsLevelActive(wLog* _log, DWORD _log_level);

	/** @brief Set a custom context for a dynamic logger.
	 *  This can be used to print a customized prefix, e.g. some session id for a specific context
	 *
	 *  @param log The logger to ste the context for. Must not be \b NULL
	 *  @param fkt A function pointer that is called to get the custimized string.
	 *  @param context A context \b fkt is called with. Caller must ensure it is still allocated
	 * when \b log is used
	 *
	 *  @return \b TRUE for success, \b FALSE otherwise.
	 */
	WINPR_API BOOL WLog_SetContext(wLog* log, const char* (*fkt)(void*), void* context);

#define WLog_Print_unchecked(_log, _log_level, ...)                                         \
	do                                                                                      \
	{                                                                                       \
		WLog_PrintTextMessage(_log, _log_level, __LINE__, __FILE__, __func__, __VA_ARGS__); \
	} while (0)

#define WLog_Print(_log, _log_level, ...)                        \
	do                                                           \
	{                                                            \
		if (WLog_IsLevelActive(_log, _log_level))                \
		{                                                        \
			WLog_Print_unchecked(_log, _log_level, __VA_ARGS__); \
		}                                                        \
	} while (0)

#define WLog_Print_tag(_tag, _log_level, ...)                 \
	do                                                        \
	{                                                         \
		static wLog* _log_cached_ptr = NULL;                  \
		if (!_log_cached_ptr)                                 \
			_log_cached_ptr = WLog_Get(_tag);                 \
		WLog_Print(_log_cached_ptr, _log_level, __VA_ARGS__); \
	} while (0)

#define WLog_PrintVA_unchecked(_log, _log_level, _args)                                 \
	do                                                                                  \
	{                                                                                   \
		WLog_PrintTextMessageVA(_log, _log_level, __LINE__, __FILE__, __func__, _args); \
	} while (0)

#define WLog_PrintVA(_log, _log_level, _args)                \
	do                                                       \
	{                                                        \
		if (WLog_IsLevelActive(_log, _log_level))            \
		{                                                    \
			WLog_PrintVA_unchecked(_log, _log_level, _args); \
		}                                                    \
	} while (0)

#define WLog_Data(_log, _log_level, ...)                                                         \
	do                                                                                           \
	{                                                                                            \
		if (WLog_IsLevelActive(_log, _log_level))                                                \
		{                                                                                        \
			WLog_PrintMessage(_log, WLOG_MESSAGE_DATA, _log_level, __LINE__, __FILE__, __func__, \
			                  __VA_ARGS__);                                                      \
		}                                                                                        \
	} while (0)

#define WLog_Image(_log, _log_level, ...)                                                        \
	do                                                                                           \
	{                                                                                            \
		if (WLog_IsLevelActive(_log, _log_level))                                                \
		{                                                                                        \
			WLog_PrintMessage(_log, WLOG_MESSAGE_DATA, _log_level, __LINE__, __FILE__, __func__, \
			                  __VA_ARGS__);                                                      \
		}                                                                                        \
	} while (0)

#define WLog_Packet(_log, _log_level, ...)                                                         \
	do                                                                                             \
	{                                                                                              \
		if (WLog_IsLevelActive(_log, _log_level))                                                  \
		{                                                                                          \
			WLog_PrintMessage(_log, WLOG_MESSAGE_PACKET, _log_level, __LINE__, __FILE__, __func__, \
			                  __VA_ARGS__);                                                        \
		}                                                                                          \
	} while (0)

	WINPR_ATTR_FORMAT_ARG(6, 7)
	static inline void WLog_Print_dbg_tag(const char* WINPR_RESTRICT tag, DWORD log_level,
	                                      size_t line, const char* file, const char* fkt,
	                                      WINPR_FORMAT_ARG const char* fmt, ...)
	{
		static wLog* log_cached_ptr = NULL;
		if (!log_cached_ptr)
			log_cached_ptr = WLog_Get(tag);

		if (WLog_IsLevelActive(log_cached_ptr, log_level))
		{
			va_list ap;
			va_start(ap, fmt);
			WLog_PrintTextMessageVA(log_cached_ptr, log_level, line, file, fkt, fmt, ap);
			va_end(ap);
		}
	}

#define WLog_LVL(tag, lvl, ...) \
	WLog_Print_dbg_tag(tag, lvl, __LINE__, __FILE__, __func__, __VA_ARGS__)
#define WLog_VRB(tag, ...) \
	WLog_Print_dbg_tag(tag, WLOG_TRACE, __LINE__, __FILE__, __func__, __VA_ARGS__)
#define WLog_DBG(tag, ...) \
	WLog_Print_dbg_tag(tag, WLOG_DEBUG, __LINE__, __FILE__, __func__, __VA_ARGS__)
#define WLog_INFO(tag, ...) \
	WLog_Print_dbg_tag(tag, WLOG_INFO, __LINE__, __FILE__, __func__, __VA_ARGS__)
#define WLog_WARN(tag, ...) \
	WLog_Print_dbg_tag(tag, WLOG_WARN, __LINE__, __FILE__, __func__, __VA_ARGS__)
#define WLog_ERR(tag, ...) \
	WLog_Print_dbg_tag(tag, WLOG_ERROR, __LINE__, __FILE__, __func__, __VA_ARGS__)
#define WLog_FATAL(tag, ...) \
	WLog_Print_dbg_tag(tag, WLOG_FATAL, __LINE__, __FILE__, __func__, __VA_ARGS__)

	WINPR_API BOOL WLog_SetLogLevel(wLog* log, DWORD logLevel);
	WINPR_API BOOL WLog_SetStringLogLevel(wLog* log, LPCSTR level);
	WINPR_API BOOL WLog_AddStringLogFilters(LPCSTR filter);

	WINPR_API BOOL WLog_SetLogAppenderType(wLog* log, DWORD logAppenderType);
	WINPR_API wLogAppender* WLog_GetLogAppender(wLog* log);
	WINPR_API BOOL WLog_OpenAppender(wLog* log);
	WINPR_API BOOL WLog_CloseAppender(wLog* log);
	WINPR_API BOOL WLog_ConfigureAppender(wLogAppender* appender, const char* setting, void* value);

	WINPR_API wLogLayout* WLog_GetLogLayout(wLog* log);
	WINPR_API BOOL WLog_Layout_SetPrefixFormat(wLog* log, wLogLayout* layout, const char* format);

#if defined(WITH_WINPR_DEPRECATED)
	/** Deprecated */
	WINPR_DEPRECATED(WINPR_API BOOL WLog_Init(void));
	/** Deprecated */
	WINPR_DEPRECATED(WINPR_API BOOL WLog_Uninit(void));
#endif

	typedef BOOL (*wLogCallbackMessage_t)(const wLogMessage* msg);
	typedef BOOL (*wLogCallbackData_t)(const wLogMessage* msg);
	typedef BOOL (*wLogCallbackImage_t)(const wLogMessage* msg);
	typedef BOOL (*wLogCallbackPackage_t)(const wLogMessage* msg);

	typedef struct
	{
		wLogCallbackData_t data;
		wLogCallbackImage_t image;
		wLogCallbackMessage_t message;
		wLogCallbackPackage_t package;
	} wLogCallbacks;

#ifdef __cplusplus
}
#endif

#endif /* WINPR_WLOG_H */
