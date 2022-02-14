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
#include <freerdp/crypto/tls.h>

#define HTTP_STATUS_CONTINUE 100
#define HTTP_STATUS_SWITCH_PROTOCOLS 101

#define HTTP_STATUS_OK 200
#define HTTP_STATUS_CREATED 201
#define HTTP_STATUS_ACCEPTED 202
#define HTTP_STATUS_PARTIAL 203
#define HTTP_STATUS_NO_CONTENT 204
#define HTTP_STATUS_RESET_CONTENT 205
#define HTTP_STATUS_PARTIAL_CONTENT 206
#define HTTP_STATUS_WEBDAV_MULTI_STATUS 207

#define HTTP_STATUS_AMBIGUOUS 300
#define HTTP_STATUS_MOVED 301
#define HTTP_STATUS_REDIRECT 302
#define HTTP_STATUS_REDIRECT_METHOD 303
#define HTTP_STATUS_NOT_MODIFIED 304
#define HTTP_STATUS_USE_PROXY 305
#define HTTP_STATUS_REDIRECT_KEEP_VERB 307

#define HTTP_STATUS_BAD_REQUEST 400
#define HTTP_STATUS_DENIED 401
#define HTTP_STATUS_PAYMENT_REQ 402
#define HTTP_STATUS_FORBIDDEN 403
#define HTTP_STATUS_NOT_FOUND 404
#define HTTP_STATUS_BAD_METHOD 405
#define HTTP_STATUS_NONE_ACCEPTABLE 406
#define HTTP_STATUS_PROXY_AUTH_REQ 407
#define HTTP_STATUS_REQUEST_TIMEOUT 408
#define HTTP_STATUS_CONFLICT 409
#define HTTP_STATUS_GONE 410
#define HTTP_STATUS_LENGTH_REQUIRED 411
#define HTTP_STATUS_PRECOND_FAILED 412
#define HTTP_STATUS_REQUEST_TOO_LARGE 413
#define HTTP_STATUS_URI_TOO_LONG 414
#define HTTP_STATUS_UNSUPPORTED_MEDIA 415
#define HTTP_STATUS_RETRY_WITH 449

#define HTTP_STATUS_SERVER_ERROR 500
#define HTTP_STATUS_NOT_SUPPORTED 501
#define HTTP_STATUS_BAD_GATEWAY 502
#define HTTP_STATUS_SERVICE_UNAVAIL 503
#define HTTP_STATUS_GATEWAY_TIMEOUT 504
#define HTTP_STATUS_VERSION_NOT_SUP 505

typedef enum
{
	TransferEncodingUnknown,
	TransferEncodingIdentity,
	TransferEncodingChunked
} TRANSFER_ENCODING;

/* HTTP context */
typedef struct s_http_context HttpContext;

FREERDP_LOCAL HttpContext* http_context_new(void);
FREERDP_LOCAL void http_context_free(HttpContext* context);

FREERDP_LOCAL BOOL http_context_set_method(HttpContext* context, const char* Method);
FREERDP_LOCAL const char* http_context_get_uri(HttpContext* context);
FREERDP_LOCAL BOOL http_context_set_uri(HttpContext* context, const char* URI);
FREERDP_LOCAL BOOL http_context_set_user_agent(HttpContext* context, const char* UserAgent);
FREERDP_LOCAL BOOL http_context_set_host(HttpContext* context, const char* Host);
FREERDP_LOCAL BOOL http_context_set_accept(HttpContext* context, const char* Accept);
FREERDP_LOCAL BOOL http_context_set_cache_control(HttpContext* context, const char* CacheControl);
FREERDP_LOCAL BOOL http_context_set_connection(HttpContext* context, const char* Connection);
FREERDP_LOCAL BOOL http_context_set_pragma(HttpContext* context, const char* Pragma);
FREERDP_LOCAL BOOL http_context_set_rdg_connection_id(HttpContext* context,
                                                      const char* RdgConnectionId);
FREERDP_LOCAL BOOL http_context_set_rdg_auth_scheme(HttpContext* context,
                                                    const char* RdgAuthScheme);
FREERDP_LOCAL BOOL http_context_enable_websocket_upgrade(HttpContext* context, BOOL enable);
FREERDP_LOCAL BOOL http_context_is_websocket_upgrade_enabled(HttpContext* context);

/* HTTP request */
typedef struct s_http_request HttpRequest;

FREERDP_LOCAL HttpRequest* http_request_new(void);
FREERDP_LOCAL void http_request_free(HttpRequest* request);

FREERDP_LOCAL BOOL http_request_set_method(HttpRequest* request, const char* Method);
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

FREERDP_LOCAL HttpResponse* http_response_new(void);
FREERDP_LOCAL void http_response_free(HttpResponse* response);

FREERDP_LOCAL BOOL http_response_print(HttpResponse* response);
FREERDP_LOCAL HttpResponse* http_response_recv(rdpTls* tls, BOOL readContentLength);

FREERDP_LOCAL long http_response_get_status_code(HttpResponse* response);
FREERDP_LOCAL SSIZE_T http_response_get_body_length(HttpResponse* response);
FREERDP_LOCAL const char* http_response_get_auth_token(HttpResponse* response, const char* method);
FREERDP_LOCAL TRANSFER_ENCODING http_response_get_transfer_encoding(HttpResponse* response);
FREERDP_LOCAL BOOL http_response_is_websocket(HttpContext* http, HttpResponse* response);

#endif /* FREERDP_LIB_CORE_GATEWAY_HTTP_H */
