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

#include <errno.h>

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <winpr/string.h>

#include <freerdp/log.h>

#ifdef HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif

#include "http.h"

#define TAG FREERDP_TAG("core.gateway.http")

#define RESPONSE_SIZE_LIMIT 64 * 1024 * 1024

static char* string_strnstr(const char* str1, const char* str2, size_t slen)
{
	char c, sc;
	size_t len;

	if ((c = *str2++) != '\0')
	{
		len = strlen(str2);

		do
		{
			do
			{
				if (slen-- < 1 || (sc = *str1++) == '\0')
					return NULL;
			}
			while (sc != c);

			if (len > slen)
				return NULL;
		}
		while (strncmp(str1, str2, len) != 0);

		str1--;
	}

	return ((char*) str1);
}

static BOOL strings_equals_nocase(void* obj1, void* obj2)
{
	if (!obj1 || !obj2)
		return FALSE;

	return _stricmp(obj1, obj2) == 0;
}

HttpContext* http_context_new(void)
{
	return (HttpContext*) calloc(1, sizeof(HttpContext));
}

BOOL http_context_set_method(HttpContext* context, const char* Method)
{
	free(context->Method);
	context->Method = _strdup(Method);

	if (!context->Method)
		return FALSE;

	return TRUE;
}

BOOL http_context_set_uri(HttpContext* context, const char* URI)
{
	free(context->URI);
	context->URI = _strdup(URI);

	if (!context->URI)
		return FALSE;

	return TRUE;
}

BOOL http_context_set_user_agent(HttpContext* context, const char* UserAgent)
{
	free(context->UserAgent);
	context->UserAgent = _strdup(UserAgent);

	if (!context->UserAgent)
		return FALSE;

	return TRUE;
}

BOOL http_context_set_host(HttpContext* context, const char* Host)
{
	free(context->Host);
	context->Host = _strdup(Host);

	if (!context->Host)
		return FALSE;

	return TRUE;
}

BOOL http_context_set_accept(HttpContext* context, const char* Accept)
{
	free(context->Accept);
	context->Accept = _strdup(Accept);

	if (!context->Accept)
		return FALSE;

	return TRUE;
}

BOOL http_context_set_cache_control(HttpContext* context, const char* CacheControl)
{
	free(context->CacheControl);
	context->CacheControl = _strdup(CacheControl);

	if (!context->CacheControl)
		return FALSE;

	return TRUE;
}

BOOL http_context_set_connection(HttpContext* context, const char* Connection)
{
	free(context->Connection);
	context->Connection = _strdup(Connection);

	if (!context->Connection)
		return FALSE;

	return TRUE;
}

BOOL http_context_set_pragma(HttpContext* context, const char* Pragma)
{
	free(context->Pragma);
	context->Pragma = _strdup(Pragma);

	if (!context->Pragma)
		return FALSE;

	return TRUE;
}

BOOL http_context_set_rdg_connection_id(HttpContext* context, const char* RdgConnectionId)
{
	free(context->RdgConnectionId);
	context->RdgConnectionId = _strdup(RdgConnectionId);

	if (!context->RdgConnectionId)
		return FALSE;

	return TRUE;
}

BOOL http_context_set_rdg_auth_scheme(HttpContext* context, const char* RdgAuthScheme)
{
	free(context->RdgAuthScheme);
	context->RdgAuthScheme = _strdup(RdgAuthScheme);
	return context->RdgAuthScheme != NULL;
}

void http_context_free(HttpContext* context)
{
	if (context)
	{
		free(context->UserAgent);
		free(context->Host);
		free(context->URI);
		free(context->Accept);
		free(context->Method);
		free(context->CacheControl);
		free(context->Connection);
		free(context->Pragma);
		free(context->RdgConnectionId);
		free(context->RdgAuthScheme);
		free(context);
	}
}

BOOL http_request_set_method(HttpRequest* request, const char* Method)
{
	free(request->Method);
	request->Method = _strdup(Method);

	if (!request->Method)
		return FALSE;

	return TRUE;
}

BOOL http_request_set_uri(HttpRequest* request, const char* URI)
{
	free(request->URI);
	request->URI = _strdup(URI);

	if (!request->URI)
		return FALSE;

	return TRUE;
}

BOOL http_request_set_auth_scheme(HttpRequest* request, const char* AuthScheme)
{
	free(request->AuthScheme);
	request->AuthScheme = _strdup(AuthScheme);

	if (!request->AuthScheme)
		return FALSE;

	return TRUE;
}

BOOL http_request_set_auth_param(HttpRequest* request, const char* AuthParam)
{
	free(request->AuthParam);
	request->AuthParam = _strdup(AuthParam);

	if (!request->AuthParam)
		return FALSE;

	return TRUE;
}

BOOL http_request_set_transfer_encoding(HttpRequest* request, const char* TransferEncoding)
{
	free(request->TransferEncoding);
	request->TransferEncoding = _strdup(TransferEncoding);

	if (!request->TransferEncoding)
		return FALSE;

	return TRUE;
}

static char* http_encode_body_line(const char* param, const char* value)
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

static char* http_encode_content_length_line(int ContentLength)
{
	const char* key = "Content-Length:";
	char* line;
	int length;
	char str[32];
	_itoa_s(ContentLength, str, sizeof(str), 10);
	length = strlen(key) + strlen(str) + 2;
	line = (char*) malloc(length + 1);

	if (!line)
		return NULL;

	sprintf_s(line, length + 1, "%s %s", key, str);
	return line;
}

static char* http_encode_header_line(const char* Method, const char* URI)
{
	const char* key = "HTTP/1.1";
	char* line;
	int length;
	length = strlen(key) + strlen(Method) + strlen(URI) + 2;
	line = (char*)malloc(length + 1);

	if (!line)
		return NULL;

	sprintf_s(line, length + 1, "%s %s %s", Method, URI, key);
	return line;
}

static char* http_encode_authorization_line(const char* AuthScheme, const char* AuthParam)
{
	const char* key = "Authorization:";
	char* line;
	int length;
	length = strlen(key) + strlen(AuthScheme) + strlen(AuthParam) + 3;
	line = (char*) malloc(length + 1);

	if (!line)
		return NULL;

	sprintf_s(line, length + 1, "%s %s %s", key, AuthScheme, AuthParam);
	return line;
}

wStream* http_request_write(HttpContext* context, HttpRequest* request)
{
	wStream* s;
	int i, count;
	char** lines;
	int length = 0;
	count = 0;
	lines = (char**) calloc(32, sizeof(char*));

	if (!lines)
		return NULL;

	lines[count++] = http_encode_header_line(request->Method, request->URI);
	lines[count++] = http_encode_body_line("Cache-Control", context->CacheControl);
	lines[count++] = http_encode_body_line("Connection", context->Connection);
	lines[count++] = http_encode_body_line("Pragma", context->Pragma);
	lines[count++] = http_encode_body_line("Accept", context->Accept);
	lines[count++] = http_encode_body_line("User-Agent", context->UserAgent);
	lines[count++] = http_encode_body_line("Host", context->Host);

	if (context->RdgConnectionId)
		lines[count++] = http_encode_body_line("RDG-Connection-Id", context->RdgConnectionId);

	if (context->RdgAuthScheme)
		lines[count++] = http_encode_body_line("RDG-Auth-Scheme", context->RdgAuthScheme);

	if (request->TransferEncoding)
	{
		lines[count++] = http_encode_body_line("Transfer-Encoding", request->TransferEncoding);
	}
	else
	{
		lines[count++] = http_encode_content_length_line(request->ContentLength);
	}

	if (request->Authorization)
	{
		lines[count++] = http_encode_body_line("Authorization", request->Authorization);
	}
	else if (request->AuthScheme && request->AuthParam)
	{
		lines[count++] = http_encode_authorization_line(request->AuthScheme, request->AuthParam);
	}

	/* check that everything went well */
	for (i = 0; i < count; i++)
	{
		if (!lines[i])
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
	Stream_SetLength(s, Stream_GetPosition(s));
	return s;
out_free:

	for (i = 0; i < count; i++)
		free(lines[i]);

	free(lines);
	return NULL;
}

HttpRequest* http_request_new(void)
{
	return (HttpRequest*) calloc(1, sizeof(HttpRequest));
}

void http_request_free(HttpRequest* request)
{
	if (!request)
		return;

	free(request->AuthParam);
	free(request->AuthScheme);
	free(request->Authorization);
	free(request->Content);
	free(request->Method);
	free(request->URI);
	free(request->TransferEncoding);
	free(request);
}

static BOOL http_response_parse_header_status_line(HttpResponse* response, char* status_line)
{
	char* separator = NULL;
	char* status_code;
	char* reason_phrase;

	if (status_line)
		separator = strchr(status_line, ' ');

	if (!separator)
		return FALSE;

	status_code = separator + 1;
	separator = strchr(status_code, ' ');

	if (!separator)
		return FALSE;

	reason_phrase = separator + 1;
	*separator = '\0';
	errno = 0;
	{
		long val = strtol(status_code, NULL, 0);

		if ((errno != 0) || (val < 0) || (val > INT16_MAX))
			return FALSE;

		response->StatusCode = strtol(status_code, NULL, 0);
	}
	response->ReasonPhrase = reason_phrase;

	if (!response->ReasonPhrase)
		return FALSE;

	*separator = ' ';
	return TRUE;
}

static BOOL http_response_parse_header_field(HttpResponse* response, const char* name,
        const char* value)
{
	BOOL status = TRUE;

	if (_stricmp(name, "Content-Length") == 0)
	{
		unsigned long long val;
		errno = 0;
		val = _strtoui64(value, NULL, 0);

		if ((errno != 0) || (val > INT32_MAX))
			return FALSE;

		response->ContentLength = val;
	}
	else if (_stricmp(name, "Content-Type") == 0)
	{
		response->ContentType = value;

		if (!response->ContentType)
			return FALSE;
	}
	else if (_stricmp(name, "WWW-Authenticate") == 0)
	{
		char* separator = NULL;
		const char* authScheme = NULL;
		const char* authValue = NULL;
		separator = strchr(value, ' ');

		if (separator)
		{
			/* WWW-Authenticate: Basic realm=""
			 * WWW-Authenticate: NTLM base64token
			 * WWW-Authenticate: Digest realm="testrealm@host.com", qop="auth, auth-int",
			 * 					nonce="dcd98b7102dd2f0e8b11d0f600bfb0c093",
			 * 					opaque="5ccc069c403ebaf9f0171e9517f40e41"
			 */
			*separator = '\0';
			authScheme = value;
			authValue = separator + 1;

			if (!authScheme || !authValue)
				return FALSE;
		}
		else
		{
			authScheme = value;

			if (!authScheme)
				return FALSE;

			authValue = NULL;
		}

		status = ListDictionary_Add(response->Authenticates, (void*)authScheme, (void*)authValue);
	}

	return status;
}

static BOOL http_response_parse_header(HttpResponse* response)
{
	char c;
	size_t count;
	char* line;
	char* name;
	char* value;
	char* colon_pos;
	char* end_of_header;
	char end_of_header_char;

	if (!response)
		return FALSE;

	if (!response->lines)
		return FALSE;

	if (!http_response_parse_header_status_line(response, response->lines[0]))
		return FALSE;

	for (count = 1; count < response->count; count++)
	{
		line = response->lines[count];

		/**
		 * name         end_of_header
		 * |            |
		 * v            v
		 * <header name>   :     <header value>
		 *                 ^     ^
		 *                 |     |
		 *         colon_pos     value
		 */
		if (line)
			colon_pos = strchr(line, ':');
		else
			colon_pos = NULL;

		if ((colon_pos == NULL) || (colon_pos == line))
			return FALSE;

		/* retrieve the position just after header name */
		for (end_of_header = colon_pos; end_of_header != line; end_of_header--)
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

		if (!http_response_parse_header_field(response, name, value))
			return FALSE;

		*end_of_header = end_of_header_char;
	}

	return TRUE;
}

void http_response_print(HttpResponse* response)
{
	int i;

	for (i = 0; i < response->count; i++)
	{
		WLog_ERR(TAG, "%s", response->lines[i]);
	}
}

HttpResponse* http_response_recv(rdpTls* tls)
{
	size_t size;
	size_t position;
	size_t bodyLength = 0;
	size_t payloadOffset;
	HttpResponse* response;
	size = 2048;
	payloadOffset = 0;
	response = http_response_new();

	if (!response)
		return NULL;

	response->ContentLength = 0;

	while (!payloadOffset)
	{
		int status = BIO_read(tls->bio, Stream_Pointer(response->data),
		                      Stream_Capacity(response->data) - Stream_GetPosition(response->data));

		if (status <= 0)
		{
			if (!BIO_should_retry(tls->bio))
				goto out_error;

			USleep(100);
			continue;
		}

#ifdef HAVE_VALGRIND_MEMCHECK_H
		VALGRIND_MAKE_MEM_DEFINED(Stream_Pointer(response->data), status);
#endif
		Stream_Seek(response->data, status);

		if (Stream_GetRemainingLength(response->data) < 1024)
		{
			if (!Stream_EnsureRemainingCapacity(response->data, 1024))
				goto out_error;
		}

		position = Stream_GetPosition(response->data);

		if (position > RESPONSE_SIZE_LIMIT)
		{
			WLog_ERR(TAG, "Request header too large! (%"PRIdz" bytes) Aborting!", bodyLength);
			goto out_error;
		}

		if (position >= 4)
		{
			char* buffer = (char*)Stream_Buffer(response->data);
			const char* line = string_strnstr(buffer, "\r\n\r\n", position);

			if (line)
				payloadOffset = (line - buffer) + 4;
		}
	}

	if (payloadOffset)
	{
		size_t count = 0;
		char* buffer = (char*)Stream_Buffer(response->data);
		char* line = (char*) Stream_Buffer(response->data);

		while ((line = string_strnstr(line, "\r\n", payloadOffset - (line - buffer) - 2)))
		{
			line += 2;
			count++;
		}

		response->count = count;

		if (count)
		{
			response->lines = (char**) calloc(response->count, sizeof(char*));

			if (!response->lines)
				goto out_error;
		}

		buffer[payloadOffset - 1] = '\0';
		buffer[payloadOffset - 2] = '\0';
		count = 0;
		line = strtok(buffer, "\r\n");

		while (line && response->lines)
		{
			response->lines[count] = line;

			if (!response->lines[count])
				goto out_error;

			line = strtok(NULL, "\r\n");
			count++;
		}

		if (!http_response_parse_header(response))
			goto out_error;

		response->BodyLength = Stream_GetPosition(response->data) - payloadOffset;
		bodyLength = 0; /* expected body length */

		if (response->ContentType)
		{
			if (_stricmp(response->ContentType, "application/rpc") != 0)
				bodyLength = response->ContentLength;
			else if (_stricmp(response->ContentType, "text/plain") == 0)
				bodyLength = response->ContentLength;
			else if (_stricmp(response->ContentType, "text/html") == 0)
				bodyLength = response->ContentLength;
		}
		else
			bodyLength = response->BodyLength;

		if (bodyLength > RESPONSE_SIZE_LIMIT)
		{
			WLog_ERR(TAG, "Expected request body too large! (%"PRIdz" bytes) Aborting!", bodyLength);
			goto out_error;
		}

		/* Fetch remaining body! */
		while (response->BodyLength < bodyLength)
		{
			int status;

			if (!Stream_EnsureRemainingCapacity(response->data, bodyLength - response->BodyLength))
				goto out_error;

			status = BIO_read(tls->bio, Stream_Pointer(response->data), bodyLength - response->BodyLength);

			if (status <= 0)
			{
				if (!BIO_should_retry(tls->bio))
					goto out_error;

				USleep(100);
				continue;
			}

			Stream_Seek(response->data, status);
			response->BodyLength += status;

			if (response->BodyLength > RESPONSE_SIZE_LIMIT)
			{
				WLog_ERR(TAG, "Request body too large! (%"PRIdz" bytes) Aborting!", response->BodyLength);
				goto out_error;
			}
		}

		if (response->BodyLength > 0)
		{
			response->BodyContent = &(Stream_Buffer(response->data))[payloadOffset];
			response->BodyLength = bodyLength;
		}

		if (bodyLength != response->BodyLength)
		{
			WLog_WARN(TAG, "http_response_recv: %s unexpected body length: actual: %d, expected: %d",
			          response->ContentType, bodyLength, response->BodyLength);
		}
	}

	return response;
out_error:
	http_response_free(response);
	return NULL;
}

HttpResponse* http_response_new(void)
{
	HttpResponse* response = (HttpResponse*) calloc(1, sizeof(HttpResponse));

	if (!response)
		return NULL;

	response->Authenticates = ListDictionary_New(FALSE);

	if (!response->Authenticates)
		goto fail;

	response->data = Stream_New(NULL, 2048);

	if (!response->data)
		goto fail;

	ListDictionary_KeyObject(response->Authenticates)->fnObjectEquals = strings_equals_nocase;
	ListDictionary_ValueObject(response->Authenticates)->fnObjectEquals = strings_equals_nocase;
	return response;
fail:
	http_response_free(response);
	return NULL;
}

void http_response_free(HttpResponse* response)
{
	if (!response)
		return;

	free(response->lines);
	ListDictionary_Free(response->Authenticates);
	Stream_Free(response->data, TRUE);
	free(response);
}
