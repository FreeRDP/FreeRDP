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
#include <stdint.h>

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <winpr/string.h>
#include <winpr/rpc.h>
#include <winpr/sysinfo.h>

#include <freerdp/log.h>
#include <freerdp/crypto/crypto.h>

/* websocket need sha1 for Sec-Websocket-Accept */
#include <winpr/crypto.h>

#ifdef FREERDP_HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif

#include "http.h"
#include "../tcp.h"
#include "../utils.h"

#define TAG FREERDP_TAG("core.gateway.http")

#define RESPONSE_SIZE_LIMIT (64ULL * 1024ULL * 1024ULL)

#define WEBSOCKET_MAGIC_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

struct s_http_context
{
	char* Method;
	char* URI;
	char* Connection;
	char* Pragma;
	BOOL websocketUpgrade;
	char* SecWebsocketKey;
	wListDictionary* cookies;
	wHashTable* headers;
};

struct s_http_request
{
	char* Method;
	char* URI;
	char* AuthScheme;
	char* AuthParam;
	char* Authorization;
	size_t ContentLength;
	TRANSFER_ENCODING TransferEncoding;
	wHashTable* headers;
};

struct s_http_response
{
	size_t count;
	char** lines;

	UINT16 StatusCode;
	char* ReasonPhrase;

	size_t ContentLength;
	char* ContentType;
	TRANSFER_ENCODING TransferEncoding;
	char* SecWebsocketVersion;
	char* SecWebsocketAccept;

	size_t BodyLength;
	char* BodyContent;

	wHashTable* Authenticates;
	wHashTable* SetCookie;
	wStream* data;
};

static wHashTable* HashTable_New_String(void);

static const char* string_strnstr(const char* str1, const char* str2, size_t slen)
{
	char c = 0;
	char sc = 0;
	size_t len = 0;

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
	HttpContext* context = (HttpContext*)calloc(1, sizeof(HttpContext));
	if (!context)
		return NULL;

	context->headers = HashTable_New_String();
	if (!context->headers)
		goto fail;

	context->cookies = ListDictionary_New(FALSE);
	if (!context->cookies)
		goto fail;

	{
		wObject* key = ListDictionary_KeyObject(context->cookies);
		wObject* value = ListDictionary_ValueObject(context->cookies);
		if (!key || !value)
			goto fail;

		key->fnObjectFree = winpr_ObjectStringFree;
		key->fnObjectNew = winpr_ObjectStringClone;
		value->fnObjectFree = winpr_ObjectStringFree;
		value->fnObjectNew = winpr_ObjectStringClone;
	}

	return context;

fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	http_context_free(context);
	WINPR_PRAGMA_DIAG_POP
	return NULL;
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

BOOL http_request_set_content_type(HttpRequest* request, const char* ContentType)
{
	if (!request || !ContentType)
		return FALSE;

	return http_request_set_header(request, "Content-Type", "%s", ContentType);
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

	return http_context_set_header(context, "User-Agent", "%s", UserAgent);
}

BOOL http_context_set_x_ms_user_agent(HttpContext* context, const char* X_MS_UserAgent)
{
	if (!context || !X_MS_UserAgent)
		return FALSE;

	return http_context_set_header(context, "X-MS-User-Agent", "%s", X_MS_UserAgent);
}

BOOL http_context_set_host(HttpContext* context, const char* Host)
{
	if (!context || !Host)
		return FALSE;

	return http_context_set_header(context, "Host", "%s", Host);
}

BOOL http_context_set_accept(HttpContext* context, const char* Accept)
{
	if (!context || !Accept)
		return FALSE;

	return http_context_set_header(context, "Accept", "%s", Accept);
}

BOOL http_context_set_cache_control(HttpContext* context, const char* CacheControl)
{
	if (!context || !CacheControl)
		return FALSE;

	return http_context_set_header(context, "Cache-Control", "%s", CacheControl);
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

WINPR_ATTR_FORMAT_ARG(2, 0)
static BOOL list_append(HttpContext* context, WINPR_FORMAT_ARG const char* str, va_list ap)
{
	BOOL rc = FALSE;
	va_list vat;
	char* Pragma = NULL;
	size_t PragmaSize = 0;

	va_copy(vat, ap);
	const int size = winpr_vasprintf(&Pragma, &PragmaSize, str, ap);
	va_end(vat);

	if (size <= 0)
		goto fail;

	{
		char* sstr = NULL;
		size_t slen = 0;
		if (context->Pragma)
		{
			winpr_asprintf(&sstr, &slen, "%s, %s", context->Pragma, Pragma);
			free(Pragma);
		}
		else
			sstr = Pragma;
		Pragma = NULL;

		free(context->Pragma);
		context->Pragma = sstr;
	}

	rc = TRUE;

fail:
	va_end(ap);
	return rc;
}

WINPR_ATTR_FORMAT_ARG(2, 3)
BOOL http_context_set_pragma(HttpContext* context, WINPR_FORMAT_ARG const char* Pragma, ...)
{
	if (!context || !Pragma)
		return FALSE;

	free(context->Pragma);
	context->Pragma = NULL;

	va_list ap = { 0 };
	va_start(ap, Pragma);
	return list_append(context, Pragma, ap);
}

WINPR_ATTR_FORMAT_ARG(2, 3)
BOOL http_context_append_pragma(HttpContext* context, const char* Pragma, ...)
{
	if (!context || !Pragma)
		return FALSE;

	va_list ap = { 0 };
	va_start(ap, Pragma);
	return list_append(context, Pragma, ap);
}

static char* guid2str(const GUID* guid, char* buffer, size_t len)
{
	if (!guid)
		return NULL;
	RPC_CSTR strguid = NULL;

	RPC_STATUS rpcStatus = UuidToStringA(guid, &strguid);

	if (rpcStatus != RPC_S_OK)
		return NULL;

	(void)sprintf_s(buffer, len, "{%s}", strguid);
	RpcStringFreeA(&strguid);
	return buffer;
}

BOOL http_context_set_rdg_connection_id(HttpContext* context, const GUID* RdgConnectionId)
{
	if (!context || !RdgConnectionId)
		return FALSE;

	char buffer[64] = { 0 };
	return http_context_set_header(context, "RDG-Connection-Id", "%s",
	                               guid2str(RdgConnectionId, buffer, sizeof(buffer)));
}

BOOL http_context_set_rdg_correlation_id(HttpContext* context, const GUID* RdgCorrelationId)
{
	if (!context || !RdgCorrelationId)
		return FALSE;

	char buffer[64] = { 0 };
	return http_context_set_header(context, "RDG-Correlation-Id", "%s",
	                               guid2str(RdgCorrelationId, buffer, sizeof(buffer)));
}

BOOL http_context_enable_websocket_upgrade(HttpContext* context, BOOL enable)
{
	if (!context)
		return FALSE;

	if (enable)
	{
		GUID key = { 0 };
		if (RPC_S_OK != UuidCreate(&key))
			return FALSE;

		free(context->SecWebsocketKey);
		context->SecWebsocketKey = crypto_base64_encode((BYTE*)&key, sizeof(key));
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

	return http_context_set_header(context, "RDG-Auth-Scheme", "%s", RdgAuthScheme);
}

BOOL http_context_set_cookie(HttpContext* context, const char* CookieName, const char* CookieValue)
{
	if (!context || !CookieName || !CookieValue)
		return FALSE;
	if (ListDictionary_Contains(context->cookies, CookieName))
	{
		if (!ListDictionary_SetItemValue(context->cookies, CookieName, CookieValue))
			return FALSE;
	}
	else
	{
		if (!ListDictionary_Add(context->cookies, CookieName, CookieValue))
			return FALSE;
	}
	return TRUE;
}

void http_context_free(HttpContext* context)
{
	if (context)
	{
		free(context->SecWebsocketKey);
		free(context->URI);
		free(context->Method);
		free(context->Connection);
		free(context->Pragma);
		HashTable_Free(context->headers);
		ListDictionary_Free(context->cookies);
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

WINPR_ATTR_FORMAT_ARG(2, 3)
static BOOL http_encode_print(wStream* s, WINPR_FORMAT_ARG const char* fmt, ...)
{
	char* str = NULL;
	va_list ap = { 0 };
	int length = 0;
	int used = 0;

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
	return http_encode_print(s, "Content-Length: %" PRIuz "\r\n", ContentLength);
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

static BOOL http_encode_cookie_line(wStream* s, wListDictionary* cookies)
{
	ULONG_PTR* keys = NULL;
	BOOL status = TRUE;

	if (!s && !cookies)
		return FALSE;

	ListDictionary_Lock(cookies);
	const size_t count = ListDictionary_GetKeys(cookies, &keys);

	if (count == 0)
		goto unlock;

	status = http_encode_print(s, "Cookie: ");
	if (!status)
		goto unlock;

	for (size_t x = 0; status && x < count; x++)
	{
		char* cur = (char*)ListDictionary_GetItemValue(cookies, (void*)keys[x]);
		if (!cur)
		{
			status = FALSE;
			continue;
		}
		if (x > 0)
		{
			status = http_encode_print(s, "; ");
			if (!status)
				continue;
		}
		status = http_encode_print(s, "%s=%s", (char*)keys[x], cur);
	}

	status = http_encode_print(s, "\r\n");
unlock:
	free(keys);
	ListDictionary_Unlock(cookies);
	return status;
}

static BOOL write_headers(const void* pkey, void* pvalue, void* arg)
{
	const char* key = pkey;
	const char* value = pvalue;
	wStream* s = arg;

	WINPR_ASSERT(key);
	WINPR_ASSERT(value);
	WINPR_ASSERT(s);

	return http_encode_body_line(s, key, value);
}

wStream* http_request_write(HttpContext* context, HttpRequest* request)
{
	wStream* s = NULL;

	if (!context || !request)
		return NULL;

	s = Stream_New(NULL, 1024);

	if (!s)
		return NULL;

	if (!http_encode_header_line(s, request->Method, request->URI) ||

	    !http_encode_body_line(s, "Pragma", context->Pragma))
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

	if (!utils_str_is_empty(request->Authorization))
	{
		if (!http_encode_body_line(s, "Authorization", request->Authorization))
			goto fail;
	}
	else if (!utils_str_is_empty(request->AuthScheme) && !utils_str_is_empty(request->AuthParam))
	{
		if (!http_encode_authorization_line(s, request->AuthScheme, request->AuthParam))
			goto fail;
	}

	if (!HashTable_Foreach(context->headers, write_headers, s))
		goto fail;

	if (!HashTable_Foreach(request->headers, write_headers, s))
		goto fail;

	if (!http_encode_cookie_line(s, context->cookies))
		goto fail;

	if (!http_encode_print(s, "\r\n"))
		goto fail;

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

	request->headers = HashTable_New_String();
	if (!request->headers)
		goto fail;
	request->TransferEncoding = TransferEncodingIdentity;
	return request;
fail:
	http_request_free(request);
	return NULL;
}

void http_request_free(HttpRequest* request)
{
	if (!request)
		return;

	free(request->AuthParam);
	free(request->AuthScheme);
	free(request->Authorization);
	free(request->Method);
	free(request->URI);
	HashTable_Free(request->headers);
	free(request);
}

static BOOL http_response_parse_header_status_line(HttpResponse* response, const char* status_line)
{
	BOOL rc = FALSE;
	char* separator = NULL;
	char* status_code = NULL;

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

	{
		const char* reason_phrase = separator + 1;
		*separator = '\0';
		errno = 0;
		{
			long val = strtol(status_code, NULL, 0);

			if ((errno != 0) || (val < 0) || (val > INT16_MAX))
				goto fail;

			response->StatusCode = (UINT16)val;
		}
		free(response->ReasonPhrase);
		response->ReasonPhrase = _strdup(reason_phrase);
	}

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
	WINPR_ASSERT(response);

	if (!name || !value)
		return FALSE;

	if (_stricmp(name, "Content-Length") == 0)
	{
		unsigned long long val = 0;
		errno = 0;
		val = _strtoui64(value, NULL, 0);

		if ((errno != 0) || (val > INT32_MAX))
			return FALSE;

		response->ContentLength = WINPR_ASSERTING_INT_CAST(size_t, val);
		return TRUE;
	}

	if (_stricmp(name, "Content-Type") == 0)
	{
		free(response->ContentType);
		response->ContentType = _strdup(value);

		return response->ContentType != NULL;
	}

	if (_stricmp(name, "Transfer-Encoding") == 0)
	{
		if (_stricmp(value, "identity") == 0)
			response->TransferEncoding = TransferEncodingIdentity;
		else if (_stricmp(value, "chunked") == 0)
			response->TransferEncoding = TransferEncodingChunked;
		else
			response->TransferEncoding = TransferEncodingUnknown;

		return TRUE;
	}

	if (_stricmp(name, "Sec-WebSocket-Version") == 0)
	{
		free(response->SecWebsocketVersion);
		response->SecWebsocketVersion = _strdup(value);

		return response->SecWebsocketVersion != NULL;
	}

	if (_stricmp(name, "Sec-WebSocket-Accept") == 0)
	{
		free(response->SecWebsocketAccept);
		response->SecWebsocketAccept = _strdup(value);

		return response->SecWebsocketAccept != NULL;
	}

	if (_stricmp(name, "WWW-Authenticate") == 0)
	{
		const char* authScheme = value;
		const char* authValue = "";
		char* separator = strchr(value, ' ');

		if (separator)
		{
			/* WWW-Authenticate: Basic realm=""
			 * WWW-Authenticate: NTLM base64token
			 * WWW-Authenticate: Digest realm="testrealm@host.com", qop="auth, auth-int",
			 * 					nonce="dcd98b7102dd2f0e8b11d0f600bfb0c093",
			 * 					opaque="5ccc069c403ebaf9f0171e9517f40e41"
			 */
			*separator = '\0';
			authValue = separator + 1;
		}

		return HashTable_Insert(response->Authenticates, authScheme, authValue);
	}

	if (_stricmp(name, "Set-Cookie") == 0)
	{
		char* separator = strchr(value, '=');

		if (!separator)
			return FALSE;

		/* Set-Cookie: name=value
		 * Set-Cookie: name=value; Attribute=value
		 * Set-Cookie: name="value with spaces"; Attribute=value
		 */
		*separator = '\0';
		const char* CookieName = value;
		char* CookieValue = separator + 1;

		if (*CookieValue == '"')
		{
			char* p = CookieValue;
			while (*p != '"' && *p != '\0')
			{
				p++;
				if (*p == '\\')
					p++;
			}
			*p = '\0';
		}
		else
		{
			char* p = CookieValue;
			while (*p != ';' && *p != '\0' && *p != ' ')
			{
				p++;
			}
			*p = '\0';
		}
		return HashTable_Insert(response->SetCookie, CookieName, CookieValue);
	}

	/* Ignore unknown lines */
	return TRUE;
}

static BOOL http_response_parse_header(HttpResponse* response)
{
	BOOL rc = FALSE;
	char c = 0;
	char* line = NULL;
	char* name = NULL;
	char* colon_pos = NULL;
	char* end_of_header = NULL;
	char end_of_header_char = 0;

	if (!response)
		goto fail;

	if (!response->lines)
		goto fail;

	if (!http_response_parse_header_status_line(response, response->lines[0]))
		goto fail;

	for (size_t count = 1; count < response->count; count++)
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
		char* value = colon_pos + 1;
		for (; *value; value++)
		{
			if ((*value != ' ') && (*value != '\t'))
				break;
		}

		const int res = http_response_parse_header_field(response, name, value);
		*end_of_header = end_of_header_char;
		if (!res)
			goto fail;
	}

	rc = TRUE;
fail:

	if (!rc)
		WLog_ERR(TAG, "parsing failed");

	return rc;
}

static void http_response_print(wLog* log, DWORD level, const HttpResponse* response,
                                const char* file, size_t line, const char* fkt)
{
	char buffer[64] = { 0 };

	WINPR_ASSERT(log);
	WINPR_ASSERT(response);

	if (!WLog_IsLevelActive(log, level))
		return;

	const long status = http_response_get_status_code(response);
	WLog_PrintTextMessage(log, level, line, file, fkt, "HTTP status: %s",
	                      freerdp_http_status_string_format(status, buffer, ARRAYSIZE(buffer)));

	if (WLog_IsLevelActive(log, WLOG_DEBUG))
	{
		for (size_t i = 0; i < response->count; i++)
			WLog_PrintTextMessage(log, WLOG_DEBUG, line, file, fkt, "[%" PRIuz "] %s", i,
			                      response->lines[i]);
	}

	if (response->ReasonPhrase)
		WLog_PrintTextMessage(log, level, line, file, fkt, "[reason] %s", response->ReasonPhrase);

	if (WLog_IsLevelActive(log, WLOG_TRACE))
	{
		WLog_PrintTextMessage(log, WLOG_TRACE, line, file, fkt, "[body][%" PRIuz "] %s",
		                      response->BodyLength, response->BodyContent);
	}
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
	else if (_strnicmp(cur, "application/json", 16) == 0)
		pos = 16;

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
	wLog* log = bp;

	WINPR_UNUSED(bp);
	WLog_Print(log, WLOG_ERROR, "%s", str);
	if (len > INT32_MAX)
		return -1;
	return (int)len;
}

int http_chuncked_read(BIO* bio, BYTE* pBuffer, size_t size,
                       http_encoding_chunked_context* encodingContext)
{
	int status = 0;
	int effectiveDataLen = 0;
	WINPR_ASSERT(bio);
	WINPR_ASSERT(pBuffer);
	WINPR_ASSERT(encodingContext != NULL);
	WINPR_ASSERT(size <= INT32_MAX);
	while (TRUE)
	{
		switch (encodingContext->state)
		{
			case ChunkStateData:
			{
				const size_t rd =
				    (size > encodingContext->nextOffset ? encodingContext->nextOffset : size);
				if (rd > INT32_MAX)
					return -1;

				ERR_clear_error();
				status = BIO_read(bio, pBuffer, (int)rd);
				if (status <= 0)
					return (effectiveDataLen > 0 ? effectiveDataLen : status);

				encodingContext->nextOffset -= WINPR_ASSERTING_INT_CAST(uint32_t, status);
				if (encodingContext->nextOffset == 0)
				{
					encodingContext->state = ChunkStateFooter;
					encodingContext->headerFooterPos = 0;
				}
				effectiveDataLen += status;

				if ((size_t)status == size)
					return effectiveDataLen;

				pBuffer += status;
				size -= (size_t)status;
			}
			break;
			case ChunkStateFooter:
			{
				char _dummy[2] = { 0 };
				WINPR_ASSERT(encodingContext->nextOffset == 0);
				WINPR_ASSERT(encodingContext->headerFooterPos < 2);
				ERR_clear_error();
				status = BIO_read(bio, _dummy, (int)(2 - encodingContext->headerFooterPos));
				if (status >= 0)
				{
					encodingContext->headerFooterPos += (size_t)status;
					if (encodingContext->headerFooterPos == 2)
					{
						encodingContext->state = ChunkStateLenghHeader;
						encodingContext->headerFooterPos = 0;
					}
				}
				else
					return (effectiveDataLen > 0 ? effectiveDataLen : status);
			}
			break;
			case ChunkStateLenghHeader:
			{
				BOOL _haveNewLine = FALSE;
				char* dst = &encodingContext->lenBuffer[encodingContext->headerFooterPos];
				WINPR_ASSERT(encodingContext->nextOffset == 0);
				while (encodingContext->headerFooterPos < 10 && !_haveNewLine)
				{
					ERR_clear_error();
					status = BIO_read(bio, dst, 1);
					if (status >= 0)
					{
						if (*dst == '\n')
							_haveNewLine = TRUE;
						encodingContext->headerFooterPos += (size_t)status;
						dst += status;
					}
					else
						return (effectiveDataLen > 0 ? effectiveDataLen : status);
				}
				*dst = '\0';
				/* strtoul is tricky, error are reported via errno, we also need
				 * to ensure the result does not overflow */
				errno = 0;
				size_t tmp = strtoul(encodingContext->lenBuffer, NULL, 16);
				if ((errno != 0) || (tmp > SIZE_MAX))
				{
					/* denote end of stream if something bad happens */
					encodingContext->nextOffset = 0;
					encodingContext->state = ChunkStateEnd;
					return -1;
				}
				encodingContext->nextOffset = tmp;
				encodingContext->state = ChunkStateData;

				if (encodingContext->nextOffset == 0)
				{ /* end of stream */
					WLog_DBG(TAG, "chunked encoding end of stream received");
					encodingContext->headerFooterPos = 0;
					encodingContext->state = ChunkStateEnd;
					return (effectiveDataLen > 0 ? effectiveDataLen : 0);
				}
			}
			break;
			default:
				/* invalid state / ChunkStateEnd */
				return -1;
		}
	}
}

#define sleep_or_timeout(tls, startMS, timeoutMS) \
	sleep_or_timeout_((tls), (startMS), (timeoutMS), __FILE__, __func__, __LINE__)
static BOOL sleep_or_timeout_(rdpTls* tls, UINT64 startMS, UINT32 timeoutMS, const char* file,
                              const char* fkt, size_t line)
{
	WINPR_ASSERT(tls);

	USleep(100);
	const UINT64 nowMS = GetTickCount64();
	if (nowMS - startMS > timeoutMS)
	{
		DWORD level = WLOG_ERROR;
		wLog* log = WLog_Get(TAG);
		if (WLog_IsLevelActive(log, level))
			WLog_PrintTextMessage(log, level, line, file, fkt, "timeout [%" PRIu32 "ms] exceeded",
			                      timeoutMS);
		return TRUE;
	}
	if (!BIO_should_retry(tls->bio))
	{
		DWORD level = WLOG_ERROR;
		wLog* log = WLog_Get(TAG);
		if (WLog_IsLevelActive(log, level))
		{
			WLog_PrintTextMessage(log, level, line, file, fkt, "Retries exceeded");
			ERR_print_errors_cb(print_bio_error, log);
		}
		return TRUE;
	}
	if (freerdp_shall_disconnect_context(tls->context))
		return TRUE;

	return FALSE;
}

static SSIZE_T http_response_recv_line(rdpTls* tls, HttpResponse* response)
{
	WINPR_ASSERT(tls);
	WINPR_ASSERT(response);

	SSIZE_T payloadOffset = -1;
	const UINT32 timeoutMS =
	    freerdp_settings_get_uint32(tls->context->settings, FreeRDP_TcpConnectTimeout);
	const UINT64 startMS = GetTickCount64();
	while (payloadOffset <= 0)
	{
		size_t bodyLength = 0;
		size_t position = 0;
		int status = -1;
		size_t s = 0;
		char* end = NULL;
		/* Read until we encounter \r\n\r\n */
		ERR_clear_error();

		status = BIO_read(tls->bio, Stream_Pointer(response->data), 1);
		if (status <= 0)
		{
			if (sleep_or_timeout(tls, startMS, timeoutMS))
				goto out_error;
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
			WLog_ERR(TAG, "Request header too large! (%" PRIuz " bytes) Aborting!", bodyLength);
			goto out_error;
		}

		/* Always check at most the lase 8 bytes for occurrence of the desired
		 * sequence of \r\n\r\n */
		s = (position > 8) ? 8 : position;
		end = (char*)Stream_Pointer(response->data) - s;

		if (string_strnstr(end, "\r\n\r\n", s) != NULL)
			payloadOffset = WINPR_ASSERTING_INT_CAST(SSIZE_T, Stream_GetPosition(response->data));
	}

out_error:
	return payloadOffset;
}

static BOOL http_response_recv_body(rdpTls* tls, HttpResponse* response, BOOL readContentLength,
                                    size_t payloadOffset, size_t bodyLength)
{
	BOOL rc = FALSE;

	WINPR_ASSERT(tls);
	WINPR_ASSERT(response);

	const UINT64 startMS = GetTickCount64();
	const UINT32 timeoutMS =
	    freerdp_settings_get_uint32(tls->context->settings, FreeRDP_TcpConnectTimeout);

	if ((response->TransferEncoding == TransferEncodingChunked) && readContentLength)
	{
		http_encoding_chunked_context ctx = { 0 };
		ctx.state = ChunkStateLenghHeader;
		ctx.nextOffset = 0;
		ctx.headerFooterPos = 0;
		int full_len = 0;
		do
		{
			if (!Stream_EnsureRemainingCapacity(response->data, 2048))
				goto out_error;

			int status = http_chuncked_read(tls->bio, Stream_Pointer(response->data),
			                                Stream_GetRemainingCapacity(response->data), &ctx);
			if (status <= 0)
			{
				if (sleep_or_timeout(tls, startMS, timeoutMS))
					goto out_error;
			}
			else
			{
				Stream_Seek(response->data, (size_t)status);
				full_len += status;
			}
		} while (ctx.state != ChunkStateEnd);
		response->BodyLength = WINPR_ASSERTING_INT_CAST(uint32_t, full_len);
		if (response->BodyLength > 0)
			response->BodyContent = &(Stream_BufferAs(response->data, char))[payloadOffset];
	}
	else
	{
		while (response->BodyLength < bodyLength)
		{
			int status = 0;

			if (!Stream_EnsureRemainingCapacity(response->data, bodyLength - response->BodyLength))
				goto out_error;

			ERR_clear_error();
			size_t diff = bodyLength - response->BodyLength;
			if (diff > INT32_MAX)
				diff = INT32_MAX;
			status = BIO_read(tls->bio, Stream_Pointer(response->data), (int)diff);

			if (status <= 0)
			{
				if (sleep_or_timeout(tls, startMS, timeoutMS))
					goto out_error;
				continue;
			}

			Stream_Seek(response->data, (size_t)status);
			response->BodyLength += (unsigned long)status;

			if (response->BodyLength > RESPONSE_SIZE_LIMIT)
			{
				WLog_ERR(TAG, "Request body too large! (%" PRIuz " bytes) Aborting!",
				         response->BodyLength);
				goto out_error;
			}
		}

		if (response->BodyLength > 0)
			response->BodyContent = &(Stream_BufferAs(response->data, char))[payloadOffset];

		if (bodyLength != response->BodyLength)
		{
			WLog_WARN(TAG, "%s unexpected body length: actual: %" PRIuz ", expected: %" PRIuz,
			          response->ContentType, response->BodyLength, bodyLength);

			if (bodyLength > 0)
				response->BodyLength = MIN(bodyLength, response->BodyLength);
		}

		/* '\0' terminate the http body */
		if (!Stream_EnsureRemainingCapacity(response->data, sizeof(UINT16)))
			goto out_error;
		Stream_Write_UINT16(response->data, 0);
	}

	rc = TRUE;
out_error:
	return rc;
}

static void clear_lines(HttpResponse* response)
{
	WINPR_ASSERT(response);

	for (size_t x = 0; x < response->count; x++)
	{
		WINPR_ASSERT(response->lines);
		char* line = response->lines[x];
		free(line);
	}

	free((void*)response->lines);
	response->lines = NULL;
	response->count = 0;
}

HttpResponse* http_response_recv(rdpTls* tls, BOOL readContentLength)
{
	size_t bodyLength = 0;
	HttpResponse* response = http_response_new();

	if (!response)
		return NULL;

	response->ContentLength = 0;

	const SSIZE_T payloadOffset = http_response_recv_line(tls, response);
	if (payloadOffset < 0)
		goto out_error;

	if (payloadOffset)
	{
		size_t count = 0;
		char* buffer = Stream_BufferAs(response->data, char);
		const char* line = Stream_BufferAs(response->data, char);
		char* context = NULL;

		while ((line = string_strnstr(line, "\r\n",
		                              WINPR_ASSERTING_INT_CAST(size_t, payloadOffset) -
		                                  WINPR_ASSERTING_INT_CAST(size_t, (line - buffer)) - 2UL)))
		{
			line += 2;
			count++;
		}

		clear_lines(response);
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
			response->lines[count] = _strdup(line);
			if (!response->lines[count])
				goto out_error;
			line = strtok_s(NULL, "\r\n", &context);
			count++;
		}

		if (!http_response_parse_header(response))
			goto out_error;

		response->BodyLength =
		    Stream_GetPosition(response->data) - WINPR_ASSERTING_INT_CAST(size_t, payloadOffset);

		WINPR_ASSERT(response->BodyLength == 0);
		bodyLength = response->BodyLength; /* expected body length */

		if (readContentLength && (response->ContentLength > 0))
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
				else
					readContentLength = FALSE; /* prevent chunked read */

				cur = strchr(cur, ';');
			}
		}

		if (bodyLength > RESPONSE_SIZE_LIMIT)
		{
			WLog_ERR(TAG, "Expected request body too large! (%" PRIuz " bytes) Aborting!",
			         bodyLength);
			goto out_error;
		}

		/* Fetch remaining body! */
		if (!http_response_recv_body(tls, response, readContentLength,
		                             WINPR_ASSERTING_INT_CAST(size_t, payloadOffset), bodyLength))
			goto out_error;
	}
	Stream_SealLength(response->data);

	/* Ensure '\0' terminated string */
	if (!Stream_EnsureRemainingCapacity(response->data, 2))
		goto out_error;
	Stream_Write_UINT16(response->data, 0);

	return response;
out_error:
	WLog_ERR(TAG, "No response");
	http_response_free(response);
	return NULL;
}

const char* http_response_get_body(const HttpResponse* response)
{
	if (!response)
		return NULL;

	return response->BodyContent;
}

wHashTable* HashTable_New_String(void)
{
	wHashTable* table = HashTable_New(FALSE);
	if (!table)
		return NULL;

	if (!HashTable_SetupForStringData(table, TRUE))
	{
		HashTable_Free(table);
		return NULL;
	}
	HashTable_KeyObject(table)->fnObjectEquals = strings_equals_nocase;
	HashTable_ValueObject(table)->fnObjectEquals = strings_equals_nocase;
	return table;
}

HttpResponse* http_response_new(void)
{
	HttpResponse* response = (HttpResponse*)calloc(1, sizeof(HttpResponse));

	if (!response)
		return NULL;

	response->Authenticates = HashTable_New_String();

	if (!response->Authenticates)
		goto fail;

	response->SetCookie = HashTable_New_String();

	if (!response->SetCookie)
		goto fail;

	response->data = Stream_New(NULL, 2048);

	if (!response->data)
		goto fail;

	response->TransferEncoding = TransferEncodingIdentity;
	return response;
fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	http_response_free(response);
	WINPR_PRAGMA_DIAG_POP
	return NULL;
}

void http_response_free(HttpResponse* response)
{
	if (!response)
		return;

	clear_lines(response);
	free(response->ReasonPhrase);
	free(response->ContentType);
	free(response->SecWebsocketAccept);
	free(response->SecWebsocketVersion);
	HashTable_Free(response->Authenticates);
	HashTable_Free(response->SetCookie);
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

UINT16 http_response_get_status_code(const HttpResponse* response)
{
	WINPR_ASSERT(response);

	return response->StatusCode;
}

size_t http_response_get_body_length(const HttpResponse* response)
{
	WINPR_ASSERT(response);

	return response->BodyLength;
}

const char* http_response_get_auth_token(const HttpResponse* response, const char* method)
{
	if (!response || !method)
		return NULL;

	return HashTable_GetItemValue(response->Authenticates, method);
}

const char* http_response_get_setcookie(const HttpResponse* response, const char* cookie)
{
	if (!response || !cookie)
		return NULL;

	return HashTable_GetItemValue(response->SetCookie, cookie);
}

TRANSFER_ENCODING http_response_get_transfer_encoding(const HttpResponse* response)
{
	if (!response)
		return TransferEncodingUnknown;

	return response->TransferEncoding;
}

BOOL http_response_is_websocket(const HttpContext* http, const HttpResponse* response)
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

void http_response_log_error_status_(wLog* log, DWORD level, const HttpResponse* response,
                                     const char* file, size_t line, const char* fkt)
{
	WINPR_ASSERT(log);
	WINPR_ASSERT(response);

	if (!WLog_IsLevelActive(log, level))
		return;

	char buffer[64] = { 0 };
	const UINT16 status = http_response_get_status_code(response);
	WLog_PrintTextMessage(log, level, line, file, fkt, "Unexpected HTTP status: %s",
	                      freerdp_http_status_string_format(status, buffer, ARRAYSIZE(buffer)));
	http_response_print(log, level, response, file, line, fkt);
}

static BOOL extract_cookie(const void* pkey, void* pvalue, void* arg)
{
	const char* key = pkey;
	const char* value = pvalue;
	HttpContext* context = arg;

	WINPR_ASSERT(arg);
	WINPR_ASSERT(key);
	WINPR_ASSERT(value);

	return http_context_set_cookie(context, key, value);
}

BOOL http_response_extract_cookies(const HttpResponse* response, HttpContext* context)
{
	WINPR_ASSERT(response);
	WINPR_ASSERT(context);

	return HashTable_Foreach(response->SetCookie, extract_cookie, context);
}

FREERDP_LOCAL BOOL http_context_set_header(HttpContext* context, const char* key, const char* value,
                                           ...)
{
	WINPR_ASSERT(context);
	va_list ap;
	va_start(ap, value);
	const BOOL rc = http_context_set_header_va(context, key, value, ap);
	va_end(ap);
	return rc;
}

BOOL http_request_set_header(HttpRequest* request, const char* key, const char* value, ...)
{
	WINPR_ASSERT(request);
	char* v = NULL;
	size_t vlen = 0;
	va_list ap;
	va_start(ap, value);
	winpr_vasprintf(&v, &vlen, value, ap);
	va_end(ap);
	const BOOL rc = HashTable_Insert(request->headers, key, v);
	free(v);
	return rc;
}

BOOL http_context_set_header_va(HttpContext* context, const char* key, const char* value,
                                va_list ap)
{
	char* v = NULL;
	size_t vlen = 0;
	winpr_vasprintf(&v, &vlen, value, ap);
	const BOOL rc = HashTable_Insert(context->headers, key, v);
	free(v);
	return rc;
}
