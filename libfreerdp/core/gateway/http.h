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

#ifndef FREERDP_CORE_HTTP_H
#define FREERDP_CORE_HTTP_H

typedef struct _http_context HttpContext;
typedef struct _http_request HttpRequest;
typedef struct _http_response HttpResponse;

#include <freerdp/types.h>
#include <freerdp/crypto/tls.h>

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
};

BOOL http_context_set_method(HttpContext* context, const char* Method);
BOOL http_context_set_uri(HttpContext* context, const char* URI);
BOOL http_context_set_user_agent(HttpContext* context, const char* UserAgent);
BOOL http_context_set_host(HttpContext* context, const char* Host);
BOOL http_context_set_accept(HttpContext* context, const char* Accept);
BOOL http_context_set_cache_control(HttpContext* context, const char* CacheControl);
BOOL http_context_set_connection(HttpContext* context, const char* Connection);
BOOL http_context_set_pragma(HttpContext* context, const char* Pragma);

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
};

BOOL http_request_set_method(HttpRequest* request, const char* Method);
BOOL http_request_set_uri(HttpRequest* request, const char* URI);
BOOL http_request_set_auth_scheme(HttpRequest* request, const char* AuthScheme);
BOOL http_request_set_auth_param(HttpRequest* request, const char* AuthParam);

wStream* http_request_write(HttpContext* context, HttpRequest* request);

HttpRequest* http_request_new(void);
void http_request_free(HttpRequest* request);

struct _http_response
{
	int count;
	char** lines;

	int StatusCode;
	char* ReasonPhrase;

	int ContentLength;
	char* ContentType;

	int BodyLength;
	BYTE* BodyContent;

	wListDictionary* Authenticates;
};

void http_response_print(HttpResponse* response);

HttpResponse* http_response_recv(rdpTls* tls);

HttpResponse* http_response_new(void);
void http_response_free(HttpResponse* response);

#endif /* FREERDP_CORE_HTTP_H */
