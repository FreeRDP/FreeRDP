/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Debug Utils
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_UTILS_DEBUG_H
#define FREERDP_UTILS_DEBUG_H

#include <winpr/wlog.h>

#define DEBUG_PRINT(level, file, fkt, line, dbg_str, fmt, ...) \
	do { \
		wLog *log = WLog_Get("com.freerdp.legacy"); \
		wLogMessage msg; \
		\
		msg.Type = WLOG_MESSAGE_TEXT; \
		msg.Level = level; \
		msg.FormatString = fmt; \
		msg.LineNumber = line; \
		msg.FileName = file; \
		msg.FunctionName = fkt; \
		WLog_PrintMessage(log, &msg, ##__VA_ARGS__); \
	} while (0 )

#define DEBUG_NULL(fmt, ...) do { } while (0)
#define DEBUG_CLASS(_dbg_class, fmt, ...) DEBUG_PRINT(WLOG_ERROR, __FILE__, \
		__FUNCTION__, 	__LINE__, #_dbg_class, fmt, ## __VA_ARGS__)
#define DEBUG_MSG(fmt, ...) DEBUG_PRINT(WLOG_DEBUG, __FILE__, __FUNCTION__, \
										__LINE__, "freerdp", fmt, ## __VA_ARGS__)
#define DEBUG_WARN(fmt, ...) DEBUG_PRINT(WLOG_ERROR, __FILE__, __FUNCTION__, \
		__LINE__, "freerdp", fmt, ## __VA_ARGS__)

#endif /* FREERDP_UTILS_DEBUG_H */
