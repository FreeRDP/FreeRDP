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

void http_context_set_method(HttpContext* http_context, char* method);
void http_context_set_uri(HttpContext* http_context, char* uri);
void http_context_set_user_agent(HttpContext* http_context, char* user_agent);
void http_context_set_host(HttpContext* http_context, char* host);
void http_context_set_accept(HttpContext* http_context, char* accept);
void http_context_set_cache_control(HttpContext* http_context, char* cache_control);
void http_context_set_connection(HttpContext* http_context, char* connection);
void http_context_set_pragma(HttpContext* http_context, char* pragma);

HttpContext* http_context_new(void);
void http_context_free(HttpContext* http_context);

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

void http_request_set_method(HttpRequest* http_request, char* method);
void http_request_set_uri(HttpRequest* http_request, char* uri);
void http_request_set_auth_scheme(HttpRequest* http_request, char* auth_scheme);
void http_request_set_auth_param(HttpRequest* http_request, char* auth_param);

wStream* http_request_write(HttpContext* http_context, HttpRequest* http_request);

HttpRequest* http_request_new(void);
void http_request_free(HttpRequest* http_request);

struct _http_response
{
	int count;
	char** lines;

	int StatusCode;
	char* ReasonPhrase;

	wListDictionary *Authenticates;
	int ContentLength;
	char* Content;
};

void http_response_print(HttpResponse* http_response);

HttpResponse* http_response_recv(rdpTls* tls);

HttpResponse* http_response_new(void);
void http_response_free(HttpResponse* http_response);

#endif /* FREERDP_CORE_HTTP_H */
