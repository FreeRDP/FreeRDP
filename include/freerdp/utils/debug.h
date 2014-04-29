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

#define DEBUG_NULL(fmt, ...) do { } while (0)

/* When building for android redirect all debug messages
 * to logcat. */
#if defined(ANDROID)
#include <android/log.h>

#define APP_NAME "freerdp-debug"

#define ANDROID_DEBUG_PRINT(_dbg_str, fmt, ...) do { \
		__android_log_print(_dbg_str, fmt, ##__VA_ARGS__); \
		} while( 0 )

#define DEBUG_CLASS(_dbg_class, fmt, ...) \
	ANDROID_DEBUG_PRINT(ANDROID_LOG_DEBUG, APP_NAME, \
			"DBG_" #_dbg_class " %s (%s:%d): " \
			fmt, __FUNCTION__, __FILE__, __LINE__, ## __VA_ARGS__)

#define DEBUG_WARN(fmt, ...) \
	ANDROID_DEBUG_PRINT(ANDROID_LOG_WARN, APP_NAME, "Warning %s (%s:%d): " \
			fmt, __FUNCTION__, __FILE__, __LINE__, ## __VA_ARGS__)

#else
/* By default all log messages are written to stdout */
#include <stdio.h>

#define DEBUG_PRINT(_dbg_str, fmt, ...) do { \
				fprintf(stderr, _dbg_str, __FUNCTION__, __FILE__, __LINE__); \
				fprintf(stderr, fmt, ## __VA_ARGS__); \
				fprintf(stderr, "\n"); \
			} while( 0 )


#define DEBUG_CLASS(_dbg_class, fmt, ...) DEBUG_PRINT("DBG_" #_dbg_class " %s (%s:%d): ", fmt, ## __VA_ARGS__)
#define DEBUG_WARN(fmt, ...) DEBUG_PRINT("Warning %s (%s:%d): ", fmt, ## __VA_ARGS__)
#endif

#endif /* FREERDP_UTILS_DEBUG_H */
