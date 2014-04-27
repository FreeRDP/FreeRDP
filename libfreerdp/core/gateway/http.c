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
	return (HttpContext *)calloc(1, sizeof(HttpContext));
}

void http_context_set_method(HttpContext* http_context, char* method)
{
	if (http_context->Method)
		free(http_context->Method);

	http_context->Method = _strdup(method);
	// TODO: check result
}

void http_context_set_uri(HttpContext* http_context, char* uri)
{
	if (http_context->URI)
		free(http_context->URI);

	http_context->URI = _strdup(uri);
	// TODO: check result
}

void http_context_set_user_agent(HttpContext* http_context, char* user_agent)
{
	if (http_context->UserAgent)
		free(http_context->UserAgent);

	http_context->UserAgent = _strdup(user_agent);
	// TODO: check result
}

void http_context_set_host(HttpContext* http_context, char* host)
{
	if (http_context->Host)
		free(http_context->Host);

	http_context->Host = _strdup(host);
	// TODO: check result
}

void http_context_set_accept(HttpContext* http_context, char* accept)
{
	if (http_context->Accept)
		free(http_context->Accept);

	http_context->Accept = _strdup(accept);
	// TODO: check result
}

void http_context_set_cache_control(HttpContext* http_context, char* cache_control)
{
	if (http_context->CacheControl)
		free(http_context->CacheControl);

	http_context->CacheControl = _strdup(cache_control);
	// TODO: check result
}

void http_context_set_connection(HttpContext* http_context, char* connection)
{
	if (http_context->Connection)
		free(http_context->Connection);

	http_context->Connection = _strdup(connection);
	// TODO: check result
}

void http_context_set_pragma(HttpContext* http_context, char* pragma)
{
	if (http_context->Pragma)
		free(http_context->Pragma);

	http_context->Pragma = _strdup(pragma);
	// TODO: check result
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
	// TODO: check result
}

void http_request_set_uri(HttpRequest* http_request, char* uri)
{
	if (http_request->URI)
		free(http_request->URI);

	http_request->URI = _strdup(uri);
	// TODO: check result
}

void http_request_set_auth_scheme(HttpRequest* http_request, char* auth_scheme)
{
	if (http_request->AuthScheme)
		free(http_request->AuthScheme);

	http_request->AuthScheme = _strdup(auth_scheme);
	// TODO: check result
}

void http_request_set_auth_param(HttpRequest* http_request, char* auth_param)
{
	if (http_request->AuthParam)
		free(http_request->AuthParam);

	http_request->AuthParam = _strdup(auth_param);
	// TODO: check result
}

char* http_encode_body_line(char* param, char* value)
{
	char* line;
	int length;

	length = strlen(param) + strlen(value) + 2;
	line = (char*) malloc(length + 1);
	if (!line)
		return NULL;
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
	line = (char *)malloc(length + 1);
	if (!line)
		return NULL;
	sprintf_s(line, length + 1, "Content-Length: %s", str);

	return line;
}

char* http_encode_header_line(char* Method, char* URI)
{
	char* line;
	int length;

	length = strlen("HTTP/1.1") + strlen(Method) + strlen(URI) + 2;
	line = (char *)malloc(length + 1);
	if (!line)
		return NULL;
	sprintf_s(line, length + 1, "%s %s HTTP/1.1", Method, URI);

	return line;
}

char* http_encode_authorization_line(char* AuthScheme, char* AuthParam)
{
	char* line;
	int length;

	length = strlen("Authorization") + strlen(AuthScheme) + strlen(AuthParam) + 3;
	line = (char*) malloc(length + 1);
	if (!line)
		return NULL;
	sprintf_s(line, length + 1, "Authorization: %s %s", AuthScheme, AuthParam);

	return line;
}

wStream* http_request_write(HttpContext* http_context, HttpRequest* http_request)
{
	int i, count;
	char **lines;
	wStream* s;
	int length = 0;

	count = 9;
	lines = (char **)calloc(count, sizeof(char *));
	if (!lines)
		return NULL;

	lines[0] = http_encode_header_line(http_request->Method, http_request->URI);
	lines[1] = http_encode_body_line("Cache-Control", http_context->CacheControl);
	lines[2] = http_encode_body_line("Connection", http_context->Connection);
	lines[3] = http_encode_body_line("Pragma", http_context->Pragma);
	lines[4] = http_encode_body_line("Accept", http_context->Accept);
	lines[5] = http_encode_body_line("User-Agent", http_context->UserAgent);
	lines[6] = http_encode_content_length_line(http_request->ContentLength);
	lines[7] = http_encode_body_line("Host", http_context->Host);

	/* check that everything went well */
	for (i = 0; i < 8; i++)
	{
		if (!lines[i])
			goto out_free;
	}

	if (http_request->Authorization != NULL)
	{
		lines[8] = http_encode_body_line("Authorization", http_request->Authorization);
		if (!lines[8])
			goto out_free;
	}
	else if ((http_request->AuthScheme != NULL) && (http_request->AuthParam != NULL))
	{
		lines[8] = http_encode_authorization_line(http_request->AuthScheme, http_request->AuthParam);
		if (!lines[8])
			goto out_free;
	}

	for (i = 0; i < count; i++)
	{
		length += (strlen(lines[i]) + 2); /* add +2 for each '\r\n' character */
	}
	length += 2; /* empty line "\r\n" at end of header */
	length += 1; /* null terminator */

	s = Stream_New(NULL, length);
	if (!s)
		goto out_free;

	for (i = 0; i < count; i++)
	{
		Stream_Write(s, lines[i], strlen(lines[i]));
		Stream_Write(s, "\r\n", 2);
		free(lines[i]);
	}
	Stream_Write(s, "\r\n", 2);

	free(lines);

	Stream_Write(s, "\0", 1); /* append null terminator */
	Stream_Rewind(s, 1); /* don't include null terminator in length */
	Stream_Length(s) = Stream_GetPosition(s);
	return s;

out_free:
	for (i = 0; i < 9; i++)
	{
		if (lines[i])
			free(lines[i]);
	}
	free(lines);
	return NULL;
}

HttpRequest* http_request_new()
{
	return (HttpRequest*) calloc(1, sizeof(HttpRequest));
}

void http_request_free(HttpRequest* http_request)
{
	if (!http_request)
		return;

	if (http_request->AuthParam)
		free(http_request->AuthParam);
	if (http_request->AuthScheme)
		free(http_request->AuthScheme);
	if (http_request->Authorization)
		free(http_request->Authorization);
	free(http_request->Content);
	free(http_request->Method);
	free(http_request->URI);
	free(http_request);
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
	if (!http_response->ReasonPhrase)
		return FALSE;
	*separator = ' ';
	return TRUE;
}

BOOL http_response_parse_header_field(HttpResponse* http_response, char* name, char* value)
{
	if (_stricmp(name, "Content-Length") == 0)
	{
		http_response->ContentLength = atoi(value);
	}
	else if (_stricmp(name, "WWW-Authenticate") == 0)
	{
		char* separator;
		char *authScheme, *authValue;

		separator = strchr(value, ' ');

		if (separator != NULL)
		{
			/* WWW-Authenticate: Basic realm=""
			 * WWW-Authenticate: NTLM base64token
			 * WWW-Authenticate: Digest realm="testrealm@host.com", qop="auth, auth-int",
			 * 					nonce="dcd98b7102dd2f0e8b11d0f600bfb0c093",
			 * 					opaque="5ccc069c403ebaf9f0171e9517f40e41"
			 */
			*separator = '\0';

			authScheme = _strdup(value);
			authValue = _strdup(separator + 1);
			if (!authScheme || !authValue)
				return FALSE;
			*separator = ' ';
		}
		else
		{
			authScheme = _strdup(value);
			if (!authScheme)
				return FALSE;
			authValue = NULL;
		}

		return ListDictionary_Add(http_response->Authenticates, authScheme, authValue);
	}
	return TRUE;
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

	if (!http_response)
		return FALSE;

	if (!http_response->lines)
		return FALSE;

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

		if (!http_response_parse_header_field(http_response, name, value))
			return FALSE;

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
	if (!buffer)
		return NULL;

	http_response = http_response_new();
	if (!http_response)
		goto out_free;

	p = buffer;
	http_response->ContentLength = 0;

	while (TRUE)
	{
		while (nbytes < 5)
		{
			status = tls_read(tls, p, length - nbytes);
            
			if (status < 0)
				goto out_error;

			if (!status)
				continue;

			nbytes += status;
			p = (BYTE*) &buffer[nbytes];
		}

		header_end = strstr((char*) buffer, "\r\n\r\n");
        
		if (!header_end)
		{
			fprintf(stderr, "http_response_recv: invalid response:\n");
			winpr_HexDump(buffer, status);
			goto out_error;
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
			if (count)
			{
				http_response->lines = (char **)calloc(http_response->count, sizeof(char *));
				if (!http_response->lines)
					goto out_error;
			}

			count = 0;
			line = strtok((char*) buffer, "\r\n");

			while (line != NULL)
			{
				http_response->lines[count] = _strdup(line);
				if (!http_response->lines[count])
					goto out_error;

				line = strtok(NULL, "\r\n");
				count++;
			}

			if (!http_response_parse_header(http_response))
				goto out_error;

			if (http_response->ContentLength > 0)
			{
				http_response->Content = _strdup(content);
				if (!http_response->Content)
					goto out_error;
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

out_error:
	http_response_free(http_response);
out_free:
	free(buffer);
	return NULL;
}

static BOOL strings_equals_nocase(void *obj1, void *obj2)
{
	if (!obj1 || !obj2)
		return FALSE;

	return _stricmp(obj1, obj2) == 0;
}

static void string_free(void *obj1)
{
	if (!obj1)
		return;
	free(obj1);
}

HttpResponse* http_response_new()
{
	HttpResponse *ret = (HttpResponse *)calloc(1, sizeof(HttpResponse));
	if (!ret)
		return NULL;

	ret->Authenticates = ListDictionary_New(FALSE);
	ListDictionary_KeyObject(ret->Authenticates)->fnObjectEquals = strings_equals_nocase;
	ListDictionary_KeyObject(ret->Authenticates)->fnObjectFree = string_free;
	ListDictionary_ValueObject(ret->Authenticates)->fnObjectEquals = strings_equals_nocase;
	ListDictionary_ValueObject(ret->Authenticates)->fnObjectFree = string_free;
	return ret;
}

void http_response_free(HttpResponse* http_response)
{
	int i;

	if (!http_response)
		return;

	for (i = 0; i < http_response->count; i++)
		free(http_response->lines[i]);

	free(http_response->lines);

	free(http_response->ReasonPhrase);

	ListDictionary_Free(http_response->Authenticates);

	if (http_response->ContentLength > 0)
		free(http_response->Content);

	free(http_response);
}
