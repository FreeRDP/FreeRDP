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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <winpr/string.h>

#include "http.h"

HttpContext* http_context_new()
{
	HttpContext* http_context = (HttpContext*) malloc(sizeof(HttpContext));

	if (http_context != NULL)
	{
		ZeroMemory(http_context, sizeof(HttpContext));
	}

	return http_context;
}

void http_context_set_method(HttpContext* http_context, char* method)
{
	if (http_context->Method)
		free(http_context->Method);

	http_context->Method = _strdup(method);
}

void http_context_set_uri(HttpContext* http_context, char* uri)
{
	if (http_context->URI)
		free(http_context->URI);

	http_context->URI = _strdup(uri);
}

void http_context_set_user_agent(HttpContext* http_context, char* user_agent)
{
	if (http_context->UserAgent)
		free(http_context->UserAgent);

	http_context->UserAgent = _strdup(user_agent);
}

void http_context_set_host(HttpContext* http_context, char* host)
{
	if (http_context->Host)
		free(http_context->Host);

	http_context->Host = _strdup(host);
}

void http_context_set_accept(HttpContext* http_context, char* accept)
{
	if (http_context->Accept)
		free(http_context->Accept);

	http_context->Accept = _strdup(accept);
}

void http_context_set_cache_control(HttpContext* http_context, char* cache_control)
{
	if (http_context->CacheControl)
		free(http_context->CacheControl);

	http_context->CacheControl = _strdup(cache_control);
}

void http_context_set_connection(HttpContext* http_context, char* connection)
{
	if (http_context->Connection)
		free(http_context->Connection);

	http_context->Connection = _strdup(connection);
}

void http_context_set_pragma(HttpContext* http_context, char* pragma)
{
	if (http_context->Pragma)
		free(http_context->Pragma);

	http_context->Pragma = _strdup(pragma);
}

void http_context_free(HttpContext* http_context)
{
	if (http_context != NULL)
	{
		free(http_context->UserAgent);
		free(http_context->Host);
		free(http_context->URI);
		free(http_context->Accept);
		free(http_context->Method);
		free(http_context->CacheControl);
		free(http_context->Connection);
		free(http_context->Pragma);
		free(http_context);
	}
}

void http_request_set_method(HttpRequest* http_request, char* method)
{
	if (http_request->Method)
		free(http_request->Method);

	http_request->Method = _strdup(method);
}

void http_request_set_uri(HttpRequest* http_request, char* uri)
{
	if (http_request->URI)
		free(http_request->URI);

	http_request->URI = _strdup(uri);
}

void http_request_set_auth_scheme(HttpRequest* http_request, char* auth_scheme)
{
	if (http_request->AuthScheme)
		free(http_request->AuthScheme);

	http_request->AuthScheme = _strdup(auth_scheme);
}

void http_request_set_auth_param(HttpRequest* http_request, char* auth_param)
{
	if (http_request->AuthParam)
		free(http_request->AuthParam);

	http_request->AuthParam = _strdup(auth_param);
}

char* http_encode_body_line(char* param, char* value)
{
	char* line;
	int length;

	length = strlen(param) + strlen(value) + 2;
	line = (char*) malloc(length + 1);
	sprintf_s(line, length + 1, "%s: %s", param, value);

	return line;
}

char* http_encode_content_length_line(int ContentLength)
{
	char* line;
	int length;
	char str[32];

	_itoa_s(ContentLength, str, sizeof(str), 10);
	length = strlen("Content-Length") + strlen(str) + 2;
	line = (char*) malloc(length + 1);
	sprintf_s(line, length + 1, "Content-Length: %s", str);

	return line;
}

char* http_encode_header_line(char* Method, char* URI)
{
	char* line;
	int length;

	length = strlen("HTTP/1.1") + strlen(Method) + strlen(URI) + 2;
	line = (char*) malloc(length + 1);
	sprintf_s(line, length + 1, "%s %s HTTP/1.1", Method, URI);

	return line;
}

char* http_encode_authorization_line(char* AuthScheme, char* AuthParam)
{
	char* line;
	int length;

	length = strlen("Authorization") + strlen(AuthScheme) + strlen(AuthParam) + 3;
	line = (char*) malloc(length + 1);
	sprintf_s(line, length + 1, "Authorization: %s %s", AuthScheme, AuthParam);

	return line;
}

wStream* http_request_write(HttpContext* http_context, HttpRequest* http_request)
{
	int i;
	wStream* s;
	int length = 0;

	http_request->count = 9;
	http_request->lines = (char**) malloc(sizeof(char*) * http_request->count);

	http_request->lines[0] = http_encode_header_line(http_request->Method, http_request->URI);
	http_request->lines[1] = http_encode_body_line("Cache-Control", http_context->CacheControl);
	http_request->lines[2] = http_encode_body_line("Connection", http_context->Connection);
	http_request->lines[3] = http_encode_body_line("Pragma", http_context->Pragma);
	http_request->lines[4] = http_encode_body_line("Accept", http_context->Accept);
	http_request->lines[5] = http_encode_body_line("User-Agent", http_context->UserAgent);
	http_request->lines[6] = http_encode_content_length_line(http_request->ContentLength);
	http_request->lines[7] = http_encode_body_line("Host", http_context->Host);

	if (http_request->Authorization != NULL)
	{
		http_request->lines[8] = http_encode_body_line("Authorization", http_request->Authorization);
	}
	else if ((http_request->AuthScheme != NULL) && (http_request->AuthParam != NULL))
	{
		http_request->lines[8] = http_encode_authorization_line(http_request->AuthScheme, http_request->AuthParam);
	}

	for (i = 0; i < http_request->count; i++)
	{
		length += (strlen(http_request->lines[i]) + 2); /* add +2 for each '\r\n' character */
	}
	length += 2; /* empty line "\r\n" at end of header */
	length += 1; /* null terminator */

	s = Stream_New(NULL, length);

	for (i = 0; i < http_request->count; i++)
	{
		Stream_Write(s, http_request->lines[i], strlen(http_request->lines[i]));
		Stream_Write(s, "\r\n", 2);
		free(http_request->lines[i]);
	}
	Stream_Write(s, "\r\n", 2);

	free(http_request->lines);

	Stream_Write(s, "\0", 1); /* append null terminator */
	Stream_Rewind(s, 1); /* don't include null terminator in length */
	Stream_Length(s) = Stream_GetPosition(s);

	return s;
}

HttpRequest* http_request_new()
{
	HttpRequest* http_request = (HttpRequest*) malloc(sizeof(HttpRequest));

	if (http_request != NULL)
	{
		ZeroMemory(http_request, sizeof(HttpRequest));
	}

	return http_request;
}

void http_request_free(HttpRequest* http_request)
{
	if (http_request != NULL)
	{
		free(http_request->AuthParam);
		free(http_request->AuthScheme);
		free(http_request->Authorization);
		free(http_request->Content);
		free(http_request->Method);
		free(http_request->URI);
		free(http_request);
	}
}

BOOL http_response_parse_header_status_line(HttpResponse* http_response, char* status_line)
{
	char* separator;
	char* status_code;
	char* reason_phrase;

	separator = strchr(status_line, ' ');
	if (!separator)
		return FALSE;
	status_code = separator + 1;

	separator = strchr(status_code, ' ');
	if (!separator)
		return FALSE;
	reason_phrase = separator + 1;

	*separator = '\0';
	http_response->StatusCode = atoi(status_code);
	http_response->ReasonPhrase = _strdup(reason_phrase);
	*separator = ' ';
	return TRUE;
}

void http_response_parse_header_field(HttpResponse* http_response, char* name, char* value)
{
	if (_stricmp(name, "Content-Length") == 0)
	{
		http_response->ContentLength = atoi(value);
	}
	else if (_stricmp(name, "Authorization") == 0)
	{
		char* separator;

		http_response->Authorization = _strdup(value);

		separator = strchr(value, ' ');

		if (separator != NULL)
		{
			*separator = '\0';
			http_response->AuthScheme = _strdup(value);
			http_response->AuthParam = _strdup(separator + 1);
			*separator = ' ';
		}
	}
	else if (_stricmp(name, "WWW-Authenticate") == 0)
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
			http_response->AuthScheme = _strdup(value);
			http_response->AuthParam = _strdup(separator + 1);
			*separator = ' ';

			return;
		}
	}
}

BOOL http_response_parse_header(HttpResponse* http_response)
{
	int count;
	char* line;
	char* name;
	char* value;
	char* colon_pos;
	char* end_of_header;
	char end_of_header_char;
	char c;

	if (!http_response_parse_header_status_line(http_response, http_response->lines[0]))
		return FALSE;

	for (count = 1; count < http_response->count; count++)
	{
		line = http_response->lines[count];

		/**
		 * name         end_of_header
		 * |            |
		 * v            v
		 * <header name>   :     <header value>
		 *                 ^     ^
		 *                 |     |
		 *         colon_pos     value
		 */
		colon_pos = strchr(line, ':');
		if ((colon_pos == NULL) || (colon_pos == line))
			return FALSE;

		/* retrieve the position just after header name */
		for(end_of_header = colon_pos; end_of_header != line; end_of_header--)
		{
			c = end_of_header[-1];
			if (c != ' ' && c != '\t' && c != ':')
				break;
		}
		if (end_of_header == line)
			return FALSE;
		end_of_header_char = *end_of_header;
		*end_of_header = '\0';

		name = line;

		/* eat space and tabs before header value */
		for (value = colon_pos + 1; *value; value++)
		{
			if ((*value != ' ') && (*value != '\t'))
				break;
		}

		http_response_parse_header_field(http_response, name, value);

		*end_of_header = end_of_header_char;
	}
	return TRUE;
}

void http_response_print(HttpResponse* http_response)
{
	int i;

	for (i = 0; i < http_response->count; i++)
	{
		fprintf(stderr, "%s\n", http_response->lines[i]);
	}
	fprintf(stderr, "\n");
}

HttpResponse* http_response_recv(rdpTls* tls)
{
	BYTE* p;
	int nbytes;
	int length;
	int status;
	BYTE* buffer;
	char* content;
	char* header_end;
	HttpResponse* http_response;

	nbytes = 0;
	length = 10000;
	content = NULL;
	buffer = malloc(length);
	http_response = http_response_new();

	p = buffer;
	http_response->ContentLength = 0;

	while (TRUE)
	{
		while (nbytes < 5)
		{
			status = tls_read(tls, p, length - nbytes);
            
			if (status > 0)
			{
				nbytes += status;
				p = (BYTE*) &buffer[nbytes];
			}
			else if (status == 0)
			{
				continue;
			}
			else
			{
				http_response_free(http_response);
				return NULL;
			}
		}

		header_end = strstr((char*) buffer, "\r\n\r\n");
        
		if (!header_end)
		{
			fprintf(stderr, "http_response_recv: invalid response:\n");
			winpr_HexDump(buffer, status);
			http_response_free(http_response);
			return NULL;
		}

		header_end += 2;

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
			http_response->lines = (char**) malloc(sizeof(char*) * http_response->count);

			count = 0;
			line = strtok((char*) buffer, "\r\n");

			while (line != NULL)
			{
				http_response->lines[count] = _strdup(line);
				line = strtok(NULL, "\r\n");
				count++;
			}

			if (!http_response_parse_header(http_response))
			{
				http_response_free(http_response);
				return NULL;
			}

			if (http_response->ContentLength > 0)
			{
				http_response->Content = _strdup(content);
			}

			break;
		}

		if ((length - nbytes) <= 0)
		{
			length *= 2;
			buffer = realloc(buffer, length);
			p = (BYTE*) &buffer[nbytes];
		}
	}

	free(buffer);

	return http_response;
}

HttpResponse* http_response_new()
{
	HttpResponse* http_response;

	http_response = (HttpResponse*) malloc(sizeof(HttpResponse));

	if (http_response != NULL)
	{
		ZeroMemory(http_response, sizeof(HttpResponse));
	}

	return http_response;
}

void http_response_free(HttpResponse* http_response)
{
	int i;

	if (http_response != NULL)
	{
		for (i = 0; i < http_response->count; i++)
			free(http_response->lines[i]);

		free(http_response->lines);

		free(http_response->ReasonPhrase);

		free(http_response->AuthParam);
		free(http_response->AuthScheme);
		free(http_response->Authorization);

		if (http_response->ContentLength > 0)
			free(http_response->Content);

		free(http_response);
	}
}
