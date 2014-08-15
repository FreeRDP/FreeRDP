/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Channel log defines
 *
 * Copyright 2014 Armin Novak <armin.novak@gmail.com>
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

#ifndef FREERDP_CHANNELS_LOG_H
#define FREERDP_CHANNELS_LOG_H

#include <winpr/wlog.h>

#define CLOG_PRINT(level, file, fkt, line, dbg_str, fmt, ...) \
	do { \
		char tag[1024] = { 0 }; \
		wLogMessage msg; \
		wLog *log; \
		\
		strncat(tag, "com.freerdp.channels.", sizeof(tag)); \
		strncat(tag, dbg_str, sizeof(tag) - sizeof("com.freerdp.channels.")); \
		log = WLog_Get(tag); \
		\
		msg.Type = WLOG_MESSAGE_TEXT; \
		msg.Level = level; \
		msg.FormatString = fmt; \
		msg.LineNumber = line; \
		msg.FileName = file; \
		msg.FunctionName = fkt; \
		WLog_PrintMessage(log, &msg, ##__VA_ARGS__); \
	} while (0 )

#define CLOG_NULL(fmt, ...) do { } while (0)
#define CLOG_CLASS(_dbg_class, fmt, ...) CLOG_PRINT(WLOG_ERROR, __FILE__, \
	__FUNCTION__, 	__LINE__, #_dbg_class, fmt, ## __VA_ARGS__)
#define CLOG_DBG(fmt, ...) CLOG_PRINT(WLOG_DEBUG, __FILE__, __FUNCTION__, \
	__LINE__, __FUNCTION__, fmt, ## __VA_ARGS__)
#define CLOG_INFO(fmt, ...) CLOG_PRINT(WLOG_INFO, __FILE__, __FUNCTION__, \
	__LINE__, __FUNCTION__, fmt, ## __VA_ARGS__)
#define CLOG_WARN(fmt, ...) CLOG_PRINT(WLOG_WARN, __FILE__, __FUNCTION__, \
	__LINE__, __FUNCTION__, fmt, ## __VA_ARGS__)
#define CLOG_ERR(fmt, ...) CLOG_PRINT(WLOG_ERROR, __FILE__, __FUNCTION__, \
	__LINE__, __FUNCTION__, fmt, ## __VA_ARGS__)
#define CLOG_FATAL(fmt, ...) CLOG_PRINT(WLOG_FATAL, __FILE__, __FUNCTION__, \
	__LINE__, __FUNCTION__, fmt, ## __VA_ARGS__)

#endif /* FREERDP_UTILS_DEBUG_H */
