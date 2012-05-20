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

void http_request_set_auth_scheme(HttpRequest* http_request, char* auth_scheme)
{
	http_request->AuthScheme = xstrdup(auth_scheme);
}

void http_request_set_auth_param(HttpRequest* http_request, char* auth_param)
{
	http_request->AuthParam = xstrdup(auth_param);
}

#ifdef _WIN32
#define http_encode_line(_str, _fmt, ...) \
	_str = xmalloc(sprintf_s(NULL, 0, _fmt, ## __VA_ARGS__) + 1); \
	sprintf_s(_str, sprintf_s(NULL, 0, _fmt, ## __VA_ARGS__) + 1, _fmt, ## __VA_ARGS__);
#else
#define http_encode_line(_str, _fmt, ...) \
	_str = xmalloc(snprintf(NULL, 0, _fmt, ## __VA_ARGS__) + 1); \
	snprintf(_str, snprintf(NULL, 0, _fmt, ## __VA_ARGS__) + 1, _fmt, ## __VA_ARGS__);
#endif

STREAM* http_request_write(HttpContext* http_context, HttpRequest* http_request)
{
	int i;
	STREAM* s;
	int length = 0;

	http_request->count = 9;
	http_request->lines = (char**) xmalloc(sizeof(char*) * http_request->count);

	http_encode_line(http_request->lines[0], "%s %s HTTP/1.1", http_request->Method, http_request->URI);
	http_encode_line(http_request->lines[1], "Cache-Control: %s", http_context->CacheControl);
	http_encode_line(http_request->lines[2], "Connection: %s", http_context->Connection);
	http_encode_line(http_request->lines[3], "Pragma: %s", http_context->Pragma);
	http_encode_line(http_request->lines[4], "Accept: %s", http_context->Accept);
	http_encode_line(http_request->lines[5], "User-Agent: %s", http_context->UserAgent);
	http_encode_line(http_request->lines[6], "Content-Length: %d", http_request->ContentLength);
	http_encode_line(http_request->lines[7], "Host: %s", http_context->Host);

	if (http_request->Authorization != NULL)
	{
		http_encode_line(http_request->lines[8], "Authorization: %s", http_request->Authorization);
	}
	else if ((http_request->AuthScheme != NULL) && (http_request->AuthParam != NULL))
	{
		http_encode_line(http_request->lines[8], "Authorization: %s %s",
				http_request->AuthScheme, http_request->AuthParam);
	}

	for (i = 0; i < http_request->count; i++)
	{
		length += (strlen(http_request->lines[i]) + 1); /* add +1 for each '\n' character */
	}
	length += 1; /* empty line "\n" at end of header */
	length += 1; /* null terminator */

	s = stream_new(length);

	for (i = 0; i < http_request->count; i++)
	{
		stream_write(s, http_request->lines[i], strlen(http_request->lines[i]));
		stream_write(s, "\n", 1);
		xfree(http_request->lines[i]);
	}
	stream_write(s, "\n", 1);

	xfree(http_request->lines);

	stream_write(s, "\0", 1); /* append null terminator */
	stream_rewind(s, 1); /* don't include null terminator in length */
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

void http_response_parse_header_status_line(HttpResponse* http_response, char* status_line)
{
	char* separator;
	char* status_code;
	char* reason_phrase;

	separator = strchr(status_line, ' ');
	status_code = separator + 1;

	separator = strchr(status_code, ' ');
	reason_phrase = separator + 1;

	*separator = '\0';
	http_response->StatusCode = atoi(status_code);
	http_response->ReasonPhrase = xstrdup(reason_phrase);
	*separator = ' ';
}

void http_response_parse_header_field(HttpResponse* http_response, char* name, char* value)
{
	if (strcmp(name, "Content-Length") == 0)
	{
		http_response->ContentLength = atoi(value);
	}
	else if (strcmp(name, "Authorization") == 0)
	{
		char* separator;

		http_response->Authorization = xstrdup(value);

		separator = strchr(value, ' ');

		if (separator != NULL)
		{
			*separator = '\0';
			http_response->AuthScheme = xstrdup(value);
			http_response->AuthParam = xstrdup(separator + 1);
			*separator = ' ';
		}
	}
	else if (strcmp(name, "WWW-Authenticate") == 0)
	{
		char* separator;

		separator = strstr(value, "=\"");

		if (separator != NULL)
		{
			/* WWW-Authenticate: parameter with spaces="value" */
			return;
		}

		separator = strchr(value, ' ');

		if (separator != NULL)
		{
			/* WWW-Authenticate: NTLM base64token */

			*separator = '\0';
			http_response->AuthScheme = xstrdup(value);
			http_response->AuthParam = xstrdup(separator + 1);
			*separator = ' ';

			return;
		}
	}
}

void http_response_parse_header(HttpResponse* http_response)
{
	int count;
	char* line;
	char* name;
	char* value;
	char* separator;

	http_response_parse_header_status_line(http_response, http_response->lines[0]);

	for (count = 1; count < http_response->count; count++)
	{
		line = http_response->lines[count];

		separator = strstr(line, ": ");

		if (separator == NULL)
			continue;

		separator[0] = '\0';
		separator[1] = '\0';

		name = line;
		value = separator + 2;

		http_response_parse_header_field(http_response, name, value);

		separator[0] = ':';
		separator[1] = ' ';
	}
}

void http_response_print(HttpResponse* http_response)
{
	int i;

	for (i = 0; i < http_response->count; i++)
	{
		printf("%s\n", http_response->lines[i]);
	}
	printf("\n");
}

HttpResponse* http_response_recv(rdpTls* tls)
{
	uint8* p;
	int nbytes;
	int length;
	int status;
	uint8* buffer;
	char* content;
	char* header_end;
	HttpResponse* http_response;

	nbytes = 0;
	length = 0xFFFF;
	buffer = xmalloc(length);
	http_response = http_response_new();

	p = buffer;

	while (true)
	{
		status = tls_read(tls, p, length - nbytes);

		if (status > 0)
		{
			nbytes += status;
			p = (uint8*) &buffer[nbytes];
		}
		else if (status == 0)
		{
			continue;
		}
		else
		{
			http_response_free(http_response) ;
			return NULL;
			break;
		}

		header_end = strstr((char*) buffer, "\r\n\r\n") + 2;

		if (header_end != NULL)
		{
			int count;
			char* line;

			header_end[0] = '\0';
			header_end[1] = '\0';
			content = &header_end[2];

			count = 0;
			line = (char*) buffer;

			while ((line = strstr(line, "\r\n")) != NULL)
			{
				line++;
				count++;
			}

			http_response->count = count;
			http_response->lines = (char**) xmalloc(sizeof(char*) * http_response->count);

			count = 0;
			line = strtok((char*) buffer, "\r\n");

			while (line != NULL)
			{
				http_response->lines[count] = xstrdup(line);
				line = strtok(NULL, "\r\n");
				count++;
			}

			http_response_parse_header(http_response);

			if (http_response->ContentLength > 0)
			{
				http_response->Content = xstrdup(content);
			}

			break;
		}

		if ((length - nbytes) <= 0)
		{
			length *= 2;
			buffer = xrealloc(buffer, length);
			p = (uint8*) &buffer[nbytes];
		}
	}

	return http_response;
}

HttpResponse* http_response_new()
{
	HttpResponse* http_response = xnew(HttpResponse);

	if (http_response != NULL)
	{

	}

	return http_response;
}

void http_response_free(HttpResponse* http_response)
{
	int i;

	if (http_response != NULL)
	{
		for (i = 0; i < http_response->count; i++)
			xfree(http_response->lines[i]);

		xfree(http_response->lines);

		xfree(http_response->ReasonPhrase);

		xfree(http_response->AuthParam);
		xfree(http_response->AuthScheme);
		xfree(http_response->Authorization);

		if (http_response->ContentLength > 0)
			xfree(http_response->Content);

		xfree(http_response);
	}
}
