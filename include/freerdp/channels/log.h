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
#include <freerdp/log.h>

#define CHANNELS_TAG(tag) FREERDP_TAG("channels.") tag

/* NOTE: Do not use these defines, they will be removed soon! */
#define CLOG_NULL(fmt, ...) do { } while (0)
#define CLOG_CLASS(_dbg_class, fmt, ...) WLog_LVL(CHANNELS_TAG("legacy." #_dbg_class), \
		WLOG_ERROR, fmt, ## __VA_ARGS__)
#define CLOG_DBG(fmt, ...) WLog_LVL(CHANNELS_TAG("legacy"), \
									WLOG_DEBUG, fmt, ## __VA_ARGS__)
#define CLOG_INFO(fmt, ...) WLog_LVL(CHANNELS_TAG("legacy"), \
									 WLOG_INFO, fmt, ## __VA_ARGS__)
#define CLOG_WARN(fmt, ...) WLog_LVL(CHANNELS_TAG("legacy"), \
									 WLOG_WARN, fmt, ## __VA_ARGS__)
#define CLOG_ERR(fmt, ...) WLog_LVL(CHANNELS_TAG("legacy"), \
									WLOG_ERROR, fmt, ## __VA_ARGS__)
#define CLOG_FATAL(fmt, ...) WLog_LVL(CHANNELS_TAG("legacy"), \
									  WLOG_FATAL, fmt, ## __VA_ARGS__)

#endif /* FREERDP_UTILS_DEBUG_H */
