/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include <freerdp/utils/memory.h>

#include "http.h"

HttpContext* http_context_new()
{
	HttpContext* http_context = xnew(HttpContext);

	if (http_context != NULL)
	{

	}

	return http_context;
}

void http_context_set_method(HttpContext* http_context, char* method)
{
	http_context->Method = xstrdup(method);
}

void http_context_set_uri(HttpContext* http_context, char* uri)
{
	http_context->URI = xstrdup(uri);
}

void http_context_set_user_agent(HttpContext* http_context, char* user_agent)
{
	http_context->UserAgent = xstrdup(user_agent);
}

void http_context_set_host(HttpContext* http_context, char* host)
{
	http_context->Host = xstrdup(host);
}

void http_context_set_accept(HttpContext* http_context, char* accept)
{
	http_context->Accept = xstrdup(accept);
}

void http_context_set_cache_control(HttpContext* http_context, char* cache_control)
{
	http_context->CacheControl = xstrdup(cache_control);
}

void http_context_set_connection(HttpContext* http_context, char* connection)
{
	http_context->Connection = xstrdup(connection);
}

void http_context_set_pragma(HttpContext* http_context, char* pragma)
{
	http_context->Pragma = xstrdup(pragma);
}

void http_context_free(HttpContext* http_context)
{
	if (http_context != NULL)
	{
		xfree(http_context->UserAgent);
		xfree(http_context->Host);
		xfree(http_context->Accept);
		xfree(http_context->CacheControl);
		xfree(http_context->Connection);
		xfree(http_context->Pragma);
		xfree(http_context);
	}
}

void http_request_set_method(HttpRequest* http_request, char* method)
{
	http_request->Method = xstrdup(method);
}

void http_request_set_uri(HttpRequest* http_request, char* uri)
{
	http_request->URI = xstrdup(uri);
}

#define http_encode_line(_str, _fmt, ...) \
	_str = xmalloc(snprintf(NULL, 0, _fmt, ## __VA_ARGS__)); \
	snprintf(_str, snprintf(NULL, 0, _fmt, ## __VA_ARGS__), _fmt, ## __VA_ARGS__);

STREAM* http_request_write(HttpContext* http_context, HttpRequest* http_request)
{
	int i;
	STREAM* s;
	int length = 0;

	http_request->count = 10;
	http_request->lines = (char**) xmalloc(sizeof(char*) * http_request->count);

	http_encode_line(http_request->lines[0], "%s %s HTTP/1.1\n", http_request->Method, http_request->URI);
	http_encode_line(http_request->lines[1], "Accept: %s\n", http_context->Accept);
	http_encode_line(http_request->lines[2], "Cache-Control: %s\n", http_context->CacheControl);
	http_encode_line(http_request->lines[3], "Connection: %s\n", http_context->Connection);
	http_encode_line(http_request->lines[4], "Content-Length: %d\n", http_request->ContentLength);
	http_encode_line(http_request->lines[5], "User-Agent: %s\n", http_context->UserAgent);
	http_encode_line(http_request->lines[6], "Host: %s\n", http_context->Host);
	http_encode_line(http_request->lines[7], "Pragma: %s\n", http_context->Pragma);
	http_encode_line(http_request->lines[8], "Authorization: %s\n", http_request->Authorization);
	http_encode_line(http_request->lines[9], "\n\n");

	for (i = 0; i < http_request->count; i++)
	{
		length += strlen(http_request->lines[i]);
	}

	s = stream_new(length);

	for (i = 0; i < http_request->count; i++)
	{
		stream_write(s, http_request->lines[i], strlen(http_request->lines[i]));
		xfree(http_request->lines[i]);
	}

	xfree(http_request->lines);
	stream_seal(s);

	return s;
}

HttpRequest* http_request_new()
{
	HttpRequest* http_request = xnew(HttpRequest);

	if (http_request != NULL)
	{

	}

	return http_request;
}

void http_request_free(HttpRequest* http_request)
{
	if (http_request != NULL)
	{
		xfree(http_request->Method);
		xfree(http_request->URI);
		xfree(http_request);
	}
}
