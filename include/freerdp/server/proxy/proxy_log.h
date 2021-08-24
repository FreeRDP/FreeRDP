/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
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

#ifndef FREERDP_SERVER_PROXY_LOG_H
#define FREERDP_SERVER_PROXY_LOG_H

#include <winpr/wlog.h>

#define PROXY_TAG(tag) "proxy." tag

/*
 * log format in proxy is:
 * "[SessionID=%s] - [__FUNCTION__]: Log message"
 * both SessionID and __FUNCTION__ are optional, but if they should be written to the log,
 * that's the format.
 */

/* log macros that prepends session id and function name tp the log message */
#define PROXY_LOG_INFO(_tag, _context, _format, ...)                                          \
	WLog_INFO(TAG, "[SessionID=%s][%s]: " _format, _context->pdata->session_id, __FUNCTION__, \
	          ##__VA_ARGS__)
#define PROXY_LOG_ERR(_tag, _context, _format, ...)                                          \
	WLog_ERR(TAG, "[SessionID=%s][%s]: " _format, _context->pdata->session_id, __FUNCTION__, \
	         ##__VA_ARGS__)
#define PROXY_LOG_DBG(_tag, _context, _format, ...)                                          \
	WLog_DBG(TAG, "[SessionID=%s][%s]: " _format, _context->pdata->session_id, __FUNCTION__, \
	         ##__VA_ARGS__)
#define PROXY_LOG_WARN(_tag, _context, _format, ...)                                          \
	WLog_WARN(TAG, "[SessionID=%s][%s]: " _format, _context->pdata->session_id, __FUNCTION__, \
	          ##__VA_ARGS__)

#endif /* FREERDP_SERVER_PROXY_LOG_H */
