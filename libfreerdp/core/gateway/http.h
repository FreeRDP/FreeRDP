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

typedef struct _http_context HttpContext;
typedef struct _http_request HttpRequest;
typedef struct _http_response HttpResponse;

#include <freerdp/types.h>
#include <freerdp/crypto/tls.h>
#include <freerdp/api.h>

#include <winpr/stream.h>
#include <winpr/winhttp.h>

struct _http_context
{
	char* Method;
	char* URI;
	char* UserAgent;
	char* Host;
	char* Accept;
	char* CacheControl;
	char* Connection;
	char* Pragma;
	char* RdgConnectionId;
	char* RdgAuthScheme;
};

FREERDP_LOCAL BOOL http_context_set_method(HttpContext* context,
        const char* Method);
FREERDP_LOCAL BOOL http_context_set_uri(HttpContext* context, const char* URI);
FREERDP_LOCAL BOOL http_context_set_user_agent(HttpContext* context,
        const char* UserAgent);
FREERDP_LOCAL BOOL http_context_set_host(HttpContext* context,
        const char* Host);
FREERDP_LOCAL BOOL http_context_set_accept(HttpContext* context,
        const char* Accept);
FREERDP_LOCAL BOOL http_context_set_cache_control(HttpContext* context,
        const char* CacheControl);
FREERDP_LOCAL BOOL http_context_set_connection(HttpContext* context,
        const char* Connection);
FREERDP_LOCAL BOOL http_context_set_pragma(HttpContext* context,
        const char* Pragma);
FREERDP_LOCAL BOOL http_context_set_rdg_connection_id(HttpContext* context,
        const char* RdgConnectionId);
FREERDP_LOCAL BOOL http_context_set_rdg_auth_scheme(HttpContext* context,
        const char* RdgAuthScheme);

HttpContext* http_context_new(void);
void http_context_free(HttpContext* context);

struct _http_request
{
	char* Method;
	char* URI;
	char* AuthScheme;
	char* AuthParam;
	char* Authorization;
	int ContentLength;
	char* Content;
	char* TransferEncoding;
};

FREERDP_LOCAL BOOL http_request_set_method(HttpRequest* request,
        const char* Method);
FREERDP_LOCAL BOOL http_request_set_uri(HttpRequest* request, const char* URI);
FREERDP_LOCAL BOOL http_request_set_auth_scheme(HttpRequest* request,
        const char* AuthScheme);
FREERDP_LOCAL BOOL http_request_set_auth_param(HttpRequest* request,
        const char* AuthParam);
FREERDP_LOCAL BOOL http_request_set_transfer_encoding(HttpRequest* request,
        const char* TransferEncoding);

FREERDP_LOCAL wStream* http_request_write(HttpContext* context,
        HttpRequest* request);

FREERDP_LOCAL HttpRequest* http_request_new(void);
FREERDP_LOCAL void http_request_free(HttpRequest* request);

struct _http_response
{
	size_t count;
	char** lines;

	long StatusCode;
	const char* ReasonPhrase;

	size_t ContentLength;
	const char* ContentType;

	size_t BodyLength;
	BYTE* BodyContent;

	wListDictionary* Authenticates;
	wStream* data;
};

FREERDP_LOCAL void http_response_print(HttpResponse* response);

FREERDP_LOCAL HttpResponse* http_response_recv(rdpTls* tls);

FREERDP_LOCAL HttpResponse* http_response_new(void);
FREERDP_LOCAL void http_response_free(HttpResponse* response);

#endif /* FREERDP_LIB_CORE_GATEWAY_HTTP_H */
