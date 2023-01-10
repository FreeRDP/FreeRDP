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

#include <freerdp/config.h>

#include <errno.h>

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <winpr/string.h>

#include <freerdp/log.h>

/* websocket need sha1 for Sec-Websocket-Accept */
#include <winpr/crypto.h>

#ifdef FREERDP_HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif

#include "http.h"

#define TAG FREERDP_TAG("core.gateway.http")

#define RESPONSE_SIZE_LIMIT 64 * 1024 * 1024

#define WEBSOCKET_MAGIC_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

struct s_http_context
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
	BOOL websocketUpgrade;
	char* SecWebsocketKey;
};

struct s_http_request
{
	char* Method;
	char* URI;
	char* AuthScheme;
	char* AuthParam;
	char* Authorization;
	size_t ContentLength;
	char* Content;
	TRANSFER_ENCODING TransferEncoding;
};

struct s_http_response
{
	size_t count;
	char** lines;

	long StatusCode;
	const char* ReasonPhrase;

	size_t ContentLength;
	const char* ContentType;
	TRANSFER_ENCODING TransferEncoding;
	const char* SecWebsocketVersion;
	const char* SecWebsocketAccept;

	size_t BodyLength;
	BYTE* BodyContent;

	wListDictionary* Authenticates;
	wStream* data;
};

static char* string_strnstr(char* str1, const char* str2, size_t slen)
{
	char c, sc;
	size_t len;

	if ((c = *str2++) != '\0')
	{
		len = strnlen(str2, slen + 1);

		do
		{
			do
			{
				if (slen-- < 1 || (sc = *str1++) == '\0')
					return NULL;
			} while (sc != c);

			if (len > slen)
				return NULL;
		} while (strncmp(str1, str2, len) != 0);

		str1--;
	}

	return str1;
}

static BOOL strings_equals_nocase(const void* obj1, const void* obj2)
{
	if (!obj1 || !obj2)
		return FALSE;

	return _stricmp(obj1, obj2) == 0;
}

HttpContext* http_context_new(void)
{
	return (HttpContext*)calloc(1, sizeof(HttpContext));
}

BOOL http_context_set_method(HttpContext* context, const char* Method)
{
	if (!context || !Method)
		return FALSE;

	free(context->Method);
	context->Method = _strdup(Method);

	if (!context->Method)
		return FALSE;

	return TRUE;
}

const char* http_context_get_uri(HttpContext* context)
{
	if (!context)
		return NULL;

	return context->URI;
}

BOOL http_context_set_uri(HttpContext* context, const char* URI)
{
	if (!context || !URI)
		return FALSE;

	free(context->URI);
	context->URI = _strdup(URI);

	if (!context->URI)
		return FALSE;

	return TRUE;
}

BOOL http_context_set_user_agent(HttpContext* context, const char* UserAgent)
{
	if (!context || !UserAgent)
		return FALSE;

	free(context->UserAgent);
	context->UserAgent = _strdup(UserAgent);

	if (!context->UserAgent)
		return FALSE;

	return TRUE;
}

BOOL http_context_set_host(HttpContext* context, const char* Host)
{
	if (!context || !Host)
		return FALSE;

	free(context->Host);
	context->Host = _strdup(Host);

	if (!context->Host)
		return FALSE;

	return TRUE;
}

BOOL http_context_set_accept(HttpContext* context, const char* Accept)
{
	if (!context || !Accept)
		return FALSE;

	free(context->Accept);
	context->Accept = _strdup(Accept);

	if (!context->Accept)
		return FALSE;

	return TRUE;
}

BOOL http_context_set_cache_control(HttpContext* context, const char* CacheControl)
{
	if (!context || !CacheControl)
		return FALSE;

	free(context->CacheControl);
	context->CacheControl = _strdup(CacheControl);

	if (!context->CacheControl)
		return FALSE;

	return TRUE;
}

BOOL http_context_set_connection(HttpContext* context, const char* Connection)
{
	if (!context || !Connection)
		return FALSE;

	free(context->Connection);
	context->Connection = _strdup(Connection);

	if (!context->Connection)
		return FALSE;

	return TRUE;
}

BOOL http_context_set_pragma(HttpContext* context, const char* Pragma)
{
	if (!context || !Pragma)
		return FALSE;

	free(context->Pragma);
	context->Pragma = _strdup(Pragma);

	if (!context->Pragma)
		return FALSE;

	return TRUE;
}

BOOL http_context_set_rdg_connection_id(HttpContext* context, const char* RdgConnectionId)
{
	if (!context || !RdgConnectionId)
		return FALSE;

	free(context->RdgConnectionId);
	context->RdgConnectionId = _strdup(RdgConnectionId);

	if (!context->RdgConnectionId)
		return FALSE;

	return TRUE;
}

BOOL http_context_enable_websocket_upgrade(HttpContext* context, BOOL enable)
{
	if (!context)
		return FALSE;

	if (enable)
	{
		BYTE key[16];
		if (winpr_RAND(key, sizeof(key)) != 0)
			return FALSE;

		context->SecWebsocketKey = crypto_base64_encode(key, sizeof(key));
		if (!context->SecWebsocketKey)
			return FALSE;
	}

	context->websocketUpgrade = enable;
	return TRUE;
}

BOOL http_context_is_websocket_upgrade_enabled(HttpContext* context)
{
	return context->websocketUpgrade;
}

BOOL http_context_set_rdg_auth_scheme(HttpContext* context, const char* RdgAuthScheme)
{
	if (!context || !RdgAuthScheme)
		return FALSE;

	free(context->RdgAuthScheme);
	context->RdgAuthScheme = _strdup(RdgAuthScheme);
	return context->RdgAuthScheme != NULL;
}

void http_context_free(HttpContext* context)
{
	if (context)
	{
		free(context->SecWebsocketKey);
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
	if (!request || !Method)
		return FALSE;

	free(request->Method);
	request->Method = _strdup(Method);

	if (!request->Method)
		return FALSE;

	return TRUE;
}

BOOL http_request_set_uri(HttpRequest* request, const char* URI)
{
	if (!request || !URI)
		return FALSE;

	free(request->URI);
	request->URI = _strdup(URI);

	if (!request->URI)
		return FALSE;

	return TRUE;
}

BOOL http_request_set_auth_scheme(HttpRequest* request, const char* AuthScheme)
{
	if (!request || !AuthScheme)
		return FALSE;

	free(request->AuthScheme);
	request->AuthScheme = _strdup(AuthScheme);

	if (!request->AuthScheme)
		return FALSE;

	return TRUE;
}

BOOL http_request_set_auth_param(HttpRequest* request, const char* AuthParam)
{
	if (!request || !AuthParam)
		return FALSE;

	free(request->AuthParam);
	request->AuthParam = _strdup(AuthParam);

	if (!request->AuthParam)
		return FALSE;

	return TRUE;
}

BOOL http_request_set_transfer_encoding(HttpRequest* request, TRANSFER_ENCODING TransferEncoding)
{
	if (!request || TransferEncoding == TransferEncodingUnknown)
		return FALSE;

	request->TransferEncoding = TransferEncoding;

	return TRUE;
}

static BOOL http_encode_print(wStream* s, const char* fmt, ...)
{
	char* str;
	va_list ap;
	int length, used;

	if (!s || !fmt)
		return FALSE;

	va_start(ap, fmt);
	length = vsnprintf(NULL, 0, fmt, ap) + 1;
	va_end(ap);

	if (!Stream_EnsureRemainingCapacity(s, (size_t)length))
		return FALSE;

	str = (char*)Stream_Pointer(s);
	va_start(ap, fmt);
	used = vsnprintf(str, (size_t)length, fmt, ap);
	va_end(ap);

	/* Strip the trailing '\0' from the string. */
	if ((used + 1) != length)
		return FALSE;

	Stream_Seek(s, (size_t)used);
	return TRUE;
}

static BOOL http_encode_body_line(wStream* s, const char* param, const char* value)
{
	if (!s || !param || !value)
		return FALSE;

	return http_encode_print(s, "%s: %s\r\n", param, value);
}

static BOOL http_encode_content_length_line(wStream* s, size_t ContentLength)
{
	return http_encode_print(s, "Content-Length: %" PRIdz "\r\n", ContentLength);
}

static BOOL http_encode_header_line(wStream* s, const char* Method, const char* URI)
{
	if (!s || !Method || !URI)
		return FALSE;

	return http_encode_print(s, "%s %s HTTP/1.1\r\n", Method, URI);
}

static BOOL http_encode_authorization_line(wStream* s, const char* AuthScheme,
                                           const char* AuthParam)
{
	if (!s || !AuthScheme || !AuthParam)
		return FALSE;

	return http_encode_print(s, "Authorization: %s %s\r\n", AuthScheme, AuthParam);
}

wStream* http_request_write(HttpContext* context, HttpRequest* request)
{
	wStream* s;

	if (!context || !request)
		return NULL;

	s = Stream_New(NULL, 1024);

	if (!s)
		return NULL;

	if (!http_encode_header_line(s, request->Method, request->URI) ||
	    !http_encode_body_line(s, "Cache-Control", context->CacheControl) ||
	    !http_encode_body_line(s, "Pragma", context->Pragma) ||
	    !http_encode_body_line(s, "Accept", context->Accept) ||
	    !http_encode_body_line(s, "User-Agent", context->UserAgent) ||
	    !http_encode_body_line(s, "Host", context->Host))
		goto fail;

	if (!context->websocketUpgrade)
	{
		if (!http_encode_body_line(s, "Connection", context->Connection))
			goto fail;
	}
	else
	{
		if (!http_encode_body_line(s, "Connection", "Upgrade") ||
		    !http_encode_body_line(s, "Upgrade", "websocket") ||
		    !http_encode_body_line(s, "Sec-Websocket-Version", "13") ||
		    !http_encode_body_line(s, "Sec-Websocket-Key", context->SecWebsocketKey))
			goto fail;
	}

	if (context->RdgConnectionId)
	{
		if (!http_encode_body_line(s, "RDG-Connection-Id", context->RdgConnectionId))
			goto fail;
	}

	if (context->RdgAuthScheme)
	{
		if (!http_encode_body_line(s, "RDG-Auth-Scheme", context->RdgAuthScheme))
			goto fail;
	}

	if (request->TransferEncoding != TransferEncodingIdentity)
	{
		if (request->TransferEncoding == TransferEncodingChunked)
		{
			if (!http_encode_body_line(s, "Transfer-Encoding", "chunked"))
				goto fail;
		}
		else
			goto fail;
	}
	else
	{
		if (!http_encode_content_length_line(s, request->ContentLength))
			goto fail;
	}

	if (request->Authorization)
	{
		if (!http_encode_body_line(s, "Authorization", request->Authorization))
			goto fail;
	}
	else if (request->AuthScheme && request->AuthParam)
	{
		if (!http_encode_authorization_line(s, request->AuthScheme, request->AuthParam))
			goto fail;
	}

	Stream_Write(s, "\r\n", 2);
	Stream_SealLength(s);
	return s;
fail:
	Stream_Free(s, TRUE);
	return NULL;
}

HttpRequest* http_request_new(void)
{
	HttpRequest* request = (HttpRequest*)calloc(1, sizeof(HttpRequest));
	if (!request)
		return NULL;

	request->TransferEncoding = TransferEncodingIdentity;
	return request;
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
	free(request);
}

static BOOL http_response_parse_header_status_line(HttpResponse* response, const char* status_line)
{
	BOOL rc = FALSE;
	char* separator = NULL;
	char* status_code;
	char* reason_phrase;

	if (!response)
		goto fail;

	if (status_line)
		separator = strchr(status_line, ' ');

	if (!separator)
		goto fail;

	status_code = separator + 1;
	separator = strchr(status_code, ' ');

	if (!separator)
		goto fail;

	reason_phrase = separator + 1;
	*separator = '\0';
	errno = 0;
	{
		long val = strtol(status_code, NULL, 0);

		if ((errno != 0) || (val < 0) || (val > INT16_MAX))
			goto fail;

		response->StatusCode = strtol(status_code, NULL, 0);
	}
	response->ReasonPhrase = reason_phrase;

	if (!response->ReasonPhrase)
		goto fail;

	*separator = ' ';
	rc = TRUE;
fail:

	if (!rc)
		WLog_ERR(TAG, "http_response_parse_header_status_line failed [%s]", status_line);

	return rc;
}

static BOOL http_response_parse_header_field(HttpResponse* response, const char* name,
                                             const char* value)
{
	BOOL status = TRUE;
	if (!response || !name)
		return FALSE;

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
	else if (_stricmp(name, "Transfer-Encoding") == 0)
	{
		if (_stricmp(value, "identity") == 0)
			response->TransferEncoding = TransferEncodingIdentity;
		else if (_stricmp(value, "chunked") == 0)
			response->TransferEncoding = TransferEncodingChunked;
		else
			response->TransferEncoding = TransferEncodingUnknown;
	}
	else if (_stricmp(name, "Sec-WebSocket-Version") == 0)
	{
		response->SecWebsocketVersion = value;

		if (!response->SecWebsocketVersion)
			return FALSE;
	}
	else if (_stricmp(name, "Sec-WebSocket-Accept") == 0)
	{
		response->SecWebsocketAccept = value;

		if (!response->SecWebsocketAccept)
			return FALSE;
	}
	else if (_stricmp(name, "WWW-Authenticate") == 0)
	{
		char* separator = NULL;
		const char* authScheme = NULL;
		char* authValue = NULL;
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

		status = ListDictionary_Add(response->Authenticates, authScheme, authValue);
	}

	return status;
}

static BOOL http_response_parse_header(HttpResponse* response)
{
	BOOL rc = FALSE;
	char c;
	size_t count;
	char* line;
	char* name;
	char* value;
	char* colon_pos;
	char* end_of_header;
	char end_of_header_char;

	if (!response)
		goto fail;

	if (!response->lines)
		goto fail;

	if (!http_response_parse_header_status_line(response, response->lines[0]))
		goto fail;

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
			goto fail;

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
			goto fail;

		*end_of_header = end_of_header_char;
	}

	rc = TRUE;
fail:

	if (!rc)
		WLog_ERR(TAG, "%s: parsing failed", __FUNCTION__);

	return rc;
}

BOOL http_response_print(HttpResponse* response)
{
	size_t i;

	if (!response)
		return FALSE;

	for (i = 0; i < response->count; i++)
		WLog_ERR(TAG, "%s", response->lines[i]);

	return TRUE;
}

static BOOL http_use_content_length(const char* cur)
{
	size_t pos = 0;

	if (!cur)
		return FALSE;

	if (_strnicmp(cur, "application/rpc", 15) == 0)
		pos = 15;
	else if (_strnicmp(cur, "text/plain", 10) == 0)
		pos = 10;
	else if (_strnicmp(cur, "text/html", 9) == 0)
		pos = 9;

	if (pos > 0)
	{
		char end = cur[pos];

		switch (end)
		{
			case ' ':
			case ';':
			case '\0':
			case '\r':
			case '\n':
				return TRUE;

			default:
				return FALSE;
		}
	}

	return FALSE;
}

static int print_bio_error(const char* str, size_t len, void* bp)
{
	WINPR_UNUSED(len);
	WINPR_UNUSED(bp);
	WLog_ERR(TAG, "%s", str);
	return len;
}

HttpResponse* http_response_recv(rdpTls* tls, BOOL readContentLength)
{
	size_t position;
	size_t bodyLength = 0;
	size_t payloadOffset = 0;
	HttpResponse* response = http_response_new();

	if (!response)
		return NULL;

	response->ContentLength = 0;

	while (payloadOffset == 0)
	{
		size_t s;
		char* end;
		/* Read until we encounter \r\n\r\n */
		ERR_clear_error();
		int status = BIO_read(tls->bio, Stream_Pointer(response->data), 1);

		if (status <= 0)
		{
			if (!BIO_should_retry(tls->bio))
			{
				WLog_ERR(TAG, "%s: Retries exceeded", __FUNCTION__);
				ERR_print_errors_cb(print_bio_error, NULL);
				goto out_error;
			}

			USleep(100);
			continue;
		}

#ifdef FREERDP_HAVE_VALGRIND_MEMCHECK_H
		VALGRIND_MAKE_MEM_DEFINED(Stream_Pointer(response->data), status);
#endif
		Stream_Seek(response->data, (size_t)status);

		if (!Stream_EnsureRemainingCapacity(response->data, 1024))
			goto out_error;

		position = Stream_GetPosition(response->data);

		if (position < 4)
			continue;
		else if (position > RESPONSE_SIZE_LIMIT)
		{
			WLog_ERR(TAG, "Request header too large! (%" PRIdz " bytes) Aborting!", bodyLength);
			goto out_error;
		}

		/* Always check at most the lase 8 bytes for occurance of the desired
		 * sequence of \r\n\r\n */
		s = (position > 8) ? 8 : position;
		end = (char*)Stream_Pointer(response->data) - s;

		if (string_strnstr(end, "\r\n\r\n", s) != NULL)
			payloadOffset = Stream_GetPosition(response->data);
	}

	if (payloadOffset)
	{
		size_t count = 0;
		char* buffer = (char*)Stream_Buffer(response->data);
		char* line = (char*)Stream_Buffer(response->data);
		char* context = NULL;

		while ((line = string_strnstr(line, "\r\n", payloadOffset - (line - buffer) - 2UL)))
		{
			line += 2;
			count++;
		}

		response->count = count;

		if (count)
		{
			response->lines = (char**)calloc(response->count, sizeof(char*));

			if (!response->lines)
				goto out_error;
		}

		buffer[payloadOffset - 1] = '\0';
		buffer[payloadOffset - 2] = '\0';
		count = 0;
		line = strtok_s(buffer, "\r\n", &context);

		while (line && (response->count > count))
		{
			response->lines[count] = line;
			line = strtok_s(NULL, "\r\n", &context);
			count++;
		}

		if (!http_response_parse_header(response))
			goto out_error;

		response->BodyLength = Stream_GetPosition(response->data) - payloadOffset;
		bodyLength = response->BodyLength; /* expected body length */

		if (readContentLength)
		{
			const char* cur = response->ContentType;

			while (cur != NULL)
			{
				if (http_use_content_length(cur))
				{
					if (response->ContentLength < RESPONSE_SIZE_LIMIT)
						bodyLength = response->ContentLength;

					break;
				}

				cur = strchr(cur, ';');
			}
		}

		if (bodyLength > RESPONSE_SIZE_LIMIT)
		{
			WLog_ERR(TAG, "Expected request body too large! (%" PRIdz " bytes) Aborting!",
			         bodyLength);
			goto out_error;
		}

		/* Fetch remaining body! */
		while (response->BodyLength < bodyLength)
		{
			int status;

			if (!Stream_EnsureRemainingCapacity(response->data, bodyLength - response->BodyLength))
				goto out_error;

			ERR_clear_error();
			status = BIO_read(tls->bio, Stream_Pointer(response->data),
			                  bodyLength - response->BodyLength);

			if (status <= 0)
			{
				if (!BIO_should_retry(tls->bio))
				{
					WLog_ERR(TAG, "%s: Retries exceeded", __FUNCTION__);
					ERR_print_errors_cb(print_bio_error, NULL);
					goto out_error;
				}

				USleep(100);
				continue;
			}

			Stream_Seek(response->data, (size_t)status);
			response->BodyLength += (unsigned long)status;

			if (response->BodyLength > RESPONSE_SIZE_LIMIT)
			{
				WLog_ERR(TAG, "Request body too large! (%" PRIdz " bytes) Aborting!",
				         response->BodyLength);
				goto out_error;
			}
		}

		if (response->BodyLength > 0)
			response->BodyContent = &(Stream_Buffer(response->data))[payloadOffset];

		if (bodyLength != response->BodyLength)
		{
			WLog_WARN(TAG, "%s: %s unexpected body length: actual: %" PRIuz ", expected: %" PRIuz,
			          __FUNCTION__, response->ContentType, response->BodyLength, bodyLength);

			if (bodyLength > 0)
				response->BodyLength = MIN(bodyLength, response->BodyLength);
		}
	}

	return response;
out_error:
	http_response_free(response);
	return NULL;
}

HttpResponse* http_response_new(void)
{
	HttpResponse* response = (HttpResponse*)calloc(1, sizeof(HttpResponse));

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

	response->TransferEncoding = TransferEncodingIdentity;
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

const char* http_request_get_uri(HttpRequest* request)
{
	if (!request)
		return NULL;

	return request->URI;
}

SSIZE_T http_request_get_content_length(HttpRequest* request)
{
	if (!request)
		return -1;

	return (SSIZE_T)request->ContentLength;
}

BOOL http_request_set_content_length(HttpRequest* request, size_t length)
{
	if (!request)
		return FALSE;

	request->ContentLength = length;
	return TRUE;
}

long http_response_get_status_code(HttpResponse* response)
{
	if (!response)
		return -1;

	return response->StatusCode;
}

SSIZE_T http_response_get_body_length(HttpResponse* response)
{
	if (!response)
		return -1;

	return (SSIZE_T)response->BodyLength;
}

const char* http_response_get_auth_token(HttpResponse* response, const char* method)
{
	if (!response || !method)
		return NULL;

	if (!ListDictionary_Contains(response->Authenticates, method))
		return NULL;

	return ListDictionary_GetItemValue(response->Authenticates, method);
}

TRANSFER_ENCODING http_response_get_transfer_encoding(HttpResponse* response)
{
	if (!response)
		return TransferEncodingUnknown;

	return response->TransferEncoding;
}

BOOL http_response_is_websocket(HttpContext* http, HttpResponse* response)
{
	BOOL isWebsocket = FALSE;
	WINPR_DIGEST_CTX* sha1 = NULL;
	char* base64accept = NULL;
	BYTE sha1_digest[WINPR_SHA1_DIGEST_LENGTH];

	if (!http || !response)
		return FALSE;

	if (!http->websocketUpgrade || response->StatusCode != HTTP_STATUS_SWITCH_PROTOCOLS)
		return FALSE;

	if (response->SecWebsocketVersion && _stricmp(response->SecWebsocketVersion, "13") != 0)
		return FALSE;

	if (!response->SecWebsocketAccept)
		return FALSE;

	/* now check if Sec-Websocket-Accept is correct */

	sha1 = winpr_Digest_New();
	if (!sha1)
		goto out;

	if (!winpr_Digest_Init(sha1, WINPR_MD_SHA1))
		goto out;

	if (!winpr_Digest_Update(sha1, (BYTE*)http->SecWebsocketKey, strlen(http->SecWebsocketKey)))
		goto out;
	if (!winpr_Digest_Update(sha1, (const BYTE*)WEBSOCKET_MAGIC_GUID, strlen(WEBSOCKET_MAGIC_GUID)))
		goto out;

	if (!winpr_Digest_Final(sha1, sha1_digest, sizeof(sha1_digest)))
		goto out;

	base64accept = crypto_base64_encode(sha1_digest, WINPR_SHA1_DIGEST_LENGTH);
	if (!base64accept)
		goto out;

	if (_stricmp(response->SecWebsocketAccept, base64accept) != 0)
	{
		WLog_WARN(TAG, "Webserver gave Websocket Upgrade response but sanity check failed");
		goto out;
	}
	isWebsocket = TRUE;
out:
	winpr_Digest_Free(sha1);
	free(base64accept);
	return isWebsocket;
}
