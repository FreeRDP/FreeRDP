/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Hypertext Transfer Protocol (HTTP)
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_LIB_CORE_GATEWAY_HTTP_H
#define FREERDP_LIB_CORE_GATEWAY_HTTP_H

#include <winpr/stream.h>

#include <freerdp/api.h>
#include <freerdp/utils/http.h>

#include "../../crypto/tls.h"

typedef enum
{
	TransferEncodingUnknown,
	TransferEncodingIdentity,
	TransferEncodingChunked
} TRANSFER_ENCODING;

typedef enum
{
	ChunkStateLenghHeader,
	ChunkStateData,
	ChunkStateFooter,
	ChunkStateEnd
} CHUNK_STATE;

typedef struct
{
	size_t nextOffset;
	size_t headerFooterPos;
	CHUNK_STATE state;
	char lenBuffer[11];
} http_encoding_chunked_context;

/* HTTP context */
typedef struct s_http_context HttpContext;

FREERDP_LOCAL void http_context_free(HttpContext* context);

WINPR_ATTR_MALLOC(http_context_free, 1)
FREERDP_LOCAL HttpContext* http_context_new(void);

FREERDP_LOCAL BOOL http_context_set_method(HttpContext* context, const char* Method);
FREERDP_LOCAL const char* http_context_get_uri(HttpContext* context);
FREERDP_LOCAL BOOL http_context_set_uri(HttpContext* context, const char* URI);
FREERDP_LOCAL BOOL http_context_set_user_agent(HttpContext* context, const char* UserAgent);
FREERDP_LOCAL BOOL http_context_set_x_ms_user_agent(HttpContext* context, const char* UserAgent);
FREERDP_LOCAL BOOL http_context_set_host(HttpContext* context, const char* Host);
FREERDP_LOCAL BOOL http_context_set_accept(HttpContext* context, const char* Accept);
FREERDP_LOCAL BOOL http_context_set_cache_control(HttpContext* context, const char* CacheControl);
FREERDP_LOCAL BOOL http_context_set_connection(HttpContext* context, const char* Connection);
FREERDP_LOCAL BOOL http_context_set_pragma(HttpContext* context,
                                           WINPR_FORMAT_ARG const char* Pragma, ...);
FREERDP_LOCAL BOOL http_context_append_pragma(HttpContext* context,
                                              WINPR_FORMAT_ARG const char* Pragma, ...);
FREERDP_LOCAL BOOL http_context_set_cookie(HttpContext* context, const char* CookieName,
                                           const char* CookieValue);
FREERDP_LOCAL BOOL http_context_set_rdg_connection_id(HttpContext* context,
                                                      const GUID* RdgConnectionId);
FREERDP_LOCAL BOOL http_context_set_rdg_correlation_id(HttpContext* context,
                                                       const GUID* RdgConnectionId);
FREERDP_LOCAL BOOL http_context_set_rdg_auth_scheme(HttpContext* context,
                                                    const char* RdgAuthScheme);
FREERDP_LOCAL BOOL http_context_enable_websocket_upgrade(HttpContext* context, BOOL enable);
FREERDP_LOCAL BOOL http_context_is_websocket_upgrade_enabled(HttpContext* context);

/* HTTP request */
typedef struct s_http_request HttpRequest;

FREERDP_LOCAL void http_request_free(HttpRequest* request);

WINPR_ATTR_MALLOC(http_request_free, 1)
FREERDP_LOCAL HttpRequest* http_request_new(void);

FREERDP_LOCAL BOOL http_request_set_method(HttpRequest* request, const char* Method);
FREERDP_LOCAL BOOL http_request_set_content_type(HttpRequest* request, const char* ContentType);
FREERDP_LOCAL SSIZE_T http_request_get_content_length(HttpRequest* request);
FREERDP_LOCAL BOOL http_request_set_content_length(HttpRequest* request, size_t length);

FREERDP_LOCAL const char* http_request_get_uri(HttpRequest* request);
FREERDP_LOCAL BOOL http_request_set_uri(HttpRequest* request, const char* URI);
FREERDP_LOCAL BOOL http_request_set_auth_scheme(HttpRequest* request, const char* AuthScheme);
FREERDP_LOCAL BOOL http_request_set_auth_param(HttpRequest* request, const char* AuthParam);
FREERDP_LOCAL BOOL http_request_set_transfer_encoding(HttpRequest* request,
                                                      TRANSFER_ENCODING TransferEncoding);

FREERDP_LOCAL wStream* http_request_write(HttpContext* context, HttpRequest* request);

/* HTTP response */
typedef struct s_http_response HttpResponse;

FREERDP_LOCAL void http_response_free(HttpResponse* response);

WINPR_ATTR_MALLOC(http_response_free, 1)
FREERDP_LOCAL HttpResponse* http_response_new(void);

FREERDP_LOCAL HttpResponse* http_response_recv(rdpTls* tls, BOOL readContentLength);

FREERDP_LOCAL INT16 http_response_get_status_code(const HttpResponse* response);
FREERDP_LOCAL size_t http_response_get_body_length(const HttpResponse* response);
FREERDP_LOCAL const BYTE* http_response_get_body(const HttpResponse* response);
FREERDP_LOCAL const char* http_response_get_auth_token(const HttpResponse* response,
                                                       const char* method);
FREERDP_LOCAL const char* http_response_get_setcookie(const HttpResponse* response,
                                                      const char* cookie);
FREERDP_LOCAL TRANSFER_ENCODING http_response_get_transfer_encoding(const HttpResponse* response);
FREERDP_LOCAL BOOL http_response_is_websocket(const HttpContext* http,
                                              const HttpResponse* response);

#define http_response_log_error_status(log, level, response) \
	http_response_log_error_status_((log), (level), (response), __FILE__, __LINE__, __func__)
FREERDP_LOCAL void http_response_log_error_status_(wLog* log, DWORD level,
                                                   const HttpResponse* response, const char* file,
                                                   size_t line, const char* fkt);

/* chunked read helper */
FREERDP_LOCAL int http_chuncked_read(BIO* bio, BYTE* pBuffer, size_t size,
                                     http_encoding_chunked_context* encodingContext);

#endif /* FREERDP_LIB_CORE_GATEWAY_HTTP_H */
