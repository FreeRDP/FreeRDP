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

#define TAG FREERDP_TAG("core.gateway.http")

#define RESPONSE_SIZE_LIMIT (64ULL * 1024ULL * 1024ULL)

#define WEBSOCKET_MAGIC_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

struct s_http_context
{
	char* Method;
	char* URI;
	char* UserAgent;
	char* X_MS_UserAgent;
	char* Host;
	char* Accept;
	char* CacheControl;
	char* Connection;
	char* Pragma;
	char* RdgConnectionId;
	char* RdgCorrelationId;
	char* RdgAuthScheme;
	BOOL websocketUpgrade;
	char* SecWebsocketKey;
	wListDictionary* cookies;
};

struct s_http_request
{
	char* Method;
	char* URI;
	char* AuthScheme;
	char* AuthParam;
	char* Authorization;
	size_t ContentLength;
	char* ContentType;
	TRANSFER_ENCODING TransferEncoding;
};

struct s_http_response
{
	size_t count;
	char** lines;

	INT16 StatusCode;
	const char* ReasonPhrase;

	size_t ContentLength;
	const char* ContentType;
	TRANSFER_ENCODING TransferEncoding;
	const char* SecWebsocketVersion;
	const char* SecWebsocketAccept;

	size_t BodyLength;
	BYTE* BodyContent;

	wListDictionary* Authenticates;
	wListDictionary* SetCookie;
	wStream* data;
};

static char* string_strnstr(char* str1, const char* str2, size_t slen)
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

	context->cookies = ListDictionary_New(FALSE);
	if (!context->cookies)
		goto fail;

	wObject* key = ListDictionary_KeyObject(context->cookies);
	wObject* value = ListDictionary_ValueObject(context->cookies);
	if (!key || !value)
		goto fail;

	key->fnObjectFree = winpr_ObjectStringFree;
	key->fnObjectNew = winpr_ObjectStringClone;
	value->fnObjectFree = winpr_ObjectStringFree;
	value->fnObjectNew = winpr_ObjectStringClone;

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

	free(request->ContentType);
	request->ContentType = _strdup(ContentType);

	if (!request->ContentType)
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

BOOL http_context_set_x_ms_user_agent(HttpContext* context, const char* X_MS_UserAgent)
{
	if (!context || !X_MS_UserAgent)
		return FALSE;

	free(context->X_MS_UserAgent);
	context->X_MS_UserAgent = _strdup(X_MS_UserAgent);

	if (!context->X_MS_UserAgent)
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

static char* guid2str(const GUID* guid)
{
	if (!guid)
		return NULL;
	char* strguid = NULL;
	char bracedGuid[64] = { 0 };

	RPC_STATUS rpcStatus = UuidToStringA(guid, &strguid);

	if (rpcStatus != RPC_S_OK)
		return NULL;

	(void)sprintf_s(bracedGuid, sizeof(bracedGuid), "{%s}", strguid);
	RpcStringFreeA(&strguid);
	return _strdup(bracedGuid);
}

BOOL http_context_set_rdg_connection_id(HttpContext* context, const GUID* RdgConnectionId)
{
	if (!context || !RdgConnectionId)
		return FALSE;

	free(context->RdgConnectionId);
	context->RdgConnectionId = guid2str(RdgConnectionId);

	if (!context->RdgConnectionId)
		return FALSE;

	return TRUE;
}

BOOL http_context_set_rdg_correlation_id(HttpContext* context, const GUID* RdgCorrelationId)
{
	if (!context || !RdgCorrelationId)
		return FALSE;

	free(context->RdgCorrelationId);
	context->RdgCorrelationId = guid2str(RdgCorrelationId);

	if (!context->RdgCorrelationId)
		return FALSE;

	return TRUE;
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

	free(context->RdgAuthScheme);
	context->RdgAuthScheme = _strdup(RdgAuthScheme);
	return context->RdgAuthScheme != NULL;
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
		free(context->UserAgent);
		free(context->X_MS_UserAgent);
		free(context->Host);
		free(context->URI);
		free(context->Accept);
		free(context->Method);
		free(context->CacheControl);
		free(context->Connection);
		free(context->Pragma);
		free(context->RdgConnectionId);
		free(context->RdgCorrelationId);
		free(context->RdgAuthScheme);
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

wStream* http_request_write(HttpContext* context, HttpRequest* request)
{
	wStream* s = NULL;

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

	if (context->RdgCorrelationId)
	{
		if (!http_encode_body_line(s, "RDG-Correlation-Id", context->RdgCorrelationId))
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

	if (context->cookies)
	{
		if (!http_encode_cookie_line(s, context->cookies))
			goto fail;
	}

	if (request->ContentType)
	{
		if (!http_encode_body_line(s, "Content-Type", request->ContentType))
			goto fail;
	}

	if (context->X_MS_UserAgent)
	{
		if (!http_encode_body_line(s, "X-MS-User-Agent", context->X_MS_UserAgent))
			goto fail;
	}

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
	free(request->ContentType);
	free(request->Method);
	free(request->URI);
	free(request);
}

static BOOL http_response_parse_header_status_line(HttpResponse* response, const char* status_line)
{
	BOOL rc = FALSE;
	char* separator = NULL;
	char* status_code = NULL;
	char* reason_phrase = NULL;

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

		response->StatusCode = (INT16)val;
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

	WINPR_ASSERT(response);

	if (!name)
		return FALSE;

	if (_stricmp(name, "Content-Length") == 0)
	{
		unsigned long long val = 0;
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
	else if (_stricmp(name, "Set-Cookie") == 0)
	{
		char* separator = NULL;
		const char* CookieName = NULL;
		char* CookieValue = NULL;
		separator = strchr(value, '=');

		if (separator)
		{
			/* Set-Cookie: name=value
			 * Set-Cookie: name=value; Attribute=value
			 * Set-Cookie: name="value with spaces"; Attribute=value
			 */
			*separator = '\0';
			CookieName = value;
			CookieValue = separator + 1;

			if (!CookieName || !CookieValue)
				return FALSE;

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
		}
		else
		{
			return FALSE;
		}

		status = ListDictionary_Add(response->SetCookie, CookieName, CookieValue);
	}

	return status;
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

static void http_response_print(wLog* log, DWORD level, const HttpResponse* response)
{
	char buffer[64] = { 0 };

	WINPR_ASSERT(log);
	WINPR_ASSERT(response);

	if (!WLog_IsLevelActive(log, level))
		return;

	const long status = http_response_get_status_code(response);
	WLog_Print(log, level, "HTTP status: %s",
	           freerdp_http_status_string_format(status, buffer, ARRAYSIZE(buffer)));

	for (size_t i = 0; i < response->count; i++)
		WLog_Print(log, WLOG_DEBUG, "[%" PRIuz "] %s", i, response->lines[i]);

	if (response->ReasonPhrase)
		WLog_Print(log, level, "[reason] %s", response->ReasonPhrase);

	WLog_Print(log, WLOG_TRACE, "[body][%" PRIuz "] %s", response->BodyLength,
	           response->BodyContent);
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

				encodingContext->nextOffset -= status;
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
			WLog_PrintMessage(log, WLOG_MESSAGE_TEXT, level, line, file, fkt,
			                  "timeout [%" PRIu32 "ms] exceeded", timeoutMS);
		return TRUE;
	}
	if (!BIO_should_retry(tls->bio))
	{
		DWORD level = WLOG_ERROR;
		wLog* log = WLog_Get(TAG);
		if (WLog_IsLevelActive(log, level))
		{
			WLog_PrintMessage(log, WLOG_MESSAGE_TEXT, level, line, file, fkt, "Retries exceeded");
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
			WLog_ERR(TAG, "Request header too large! (%" PRIdz " bytes) Aborting!", bodyLength);
			goto out_error;
		}

		/* Always check at most the lase 8 bytes for occurrence of the desired
		 * sequence of \r\n\r\n */
		s = (position > 8) ? 8 : position;
		end = (char*)Stream_Pointer(response->data) - s;

		if (string_strnstr(end, "\r\n\r\n", s) != NULL)
			payloadOffset = Stream_GetPosition(response->data);
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
		response->BodyLength = full_len;
		if (response->BodyLength > 0)
			response->BodyContent = &(Stream_Buffer(response->data))[payloadOffset];
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
				WLog_ERR(TAG, "Request body too large! (%" PRIdz " bytes) Aborting!",
				         response->BodyLength);
				goto out_error;
			}
		}

		if (response->BodyLength > 0)
			response->BodyContent = &(Stream_Buffer(response->data))[payloadOffset];

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
		char* line = Stream_BufferAs(response->data, char);
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

		WINPR_ASSERT(response->BodyLength == 0);
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
				else
					readContentLength = FALSE; /* prevent chunked read */

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
		if (!http_response_recv_body(tls, response, readContentLength, payloadOffset, bodyLength))
			goto out_error;
	}
	Stream_SealLength(response->data);

	/* Ensure '\0' terminated string */
	if (!Stream_EnsureRemainingCapacity(response->data, 2))
		goto out_error;
	Stream_Write_UINT16(response->data, 0);

	return response;
out_error:
	http_response_free(response);
	return NULL;
}

const BYTE* http_response_get_body(const HttpResponse* response)
{
	if (!response)
		return NULL;

	return response->BodyContent;
}

static BOOL set_compare(wListDictionary* dict)
{
	WINPR_ASSERT(dict);
	wObject* key = ListDictionary_KeyObject(dict);
	wObject* value = ListDictionary_KeyObject(dict);
	if (!key || !value)
		return FALSE;
	key->fnObjectEquals = strings_equals_nocase;
	value->fnObjectEquals = strings_equals_nocase;
	return TRUE;
}

HttpResponse* http_response_new(void)
{
	HttpResponse* response = (HttpResponse*)calloc(1, sizeof(HttpResponse));

	if (!response)
		return NULL;

	response->Authenticates = ListDictionary_New(FALSE);

	if (!response->Authenticates)
		goto fail;

	if (!set_compare(response->Authenticates))
		goto fail;

	response->SetCookie = ListDictionary_New(FALSE);

	if (!response->SetCookie)
		goto fail;

	if (!set_compare(response->SetCookie))
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

	free((void*)response->lines);
	ListDictionary_Free(response->Authenticates);
	ListDictionary_Free(response->SetCookie);
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

INT16 http_response_get_status_code(const HttpResponse* response)
{
	WINPR_ASSERT(response);

	return response->StatusCode;
}

size_t http_response_get_body_length(const HttpResponse* response)
{
	WINPR_ASSERT(response);

	return (SSIZE_T)response->BodyLength;
}

const char* http_response_get_auth_token(const HttpResponse* response, const char* method)
{
	if (!response || !method)
		return NULL;

	if (!ListDictionary_Contains(response->Authenticates, method))
		return NULL;

	return ListDictionary_GetItemValue(response->Authenticates, method);
}

const char* http_response_get_setcookie(const HttpResponse* response, const char* cookie)
{
	if (!response || !cookie)
		return NULL;

	if (!ListDictionary_Contains(response->SetCookie, cookie))
		return NULL;

	return ListDictionary_GetItemValue(response->SetCookie, cookie);
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

void http_response_log_error_status(wLog* log, DWORD level, const HttpResponse* response)
{
	WINPR_ASSERT(log);
	WINPR_ASSERT(response);

	if (!WLog_IsLevelActive(log, level))
		return;

	char buffer[64] = { 0 };
	const long status = http_response_get_status_code(response);
	WLog_Print(log, level, "Unexpected HTTP status: %s",
	           freerdp_http_status_string_format(status, buffer, ARRAYSIZE(buffer)));
	http_response_print(log, level, response);
}
