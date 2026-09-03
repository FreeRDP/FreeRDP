/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Simple HTTP client request utility
 *
 * Copyright 2023 Isaac Klein <fifthdegree@protonmail.com>
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
#include <freerdp/utils/http.h>

#include "http.h"

#include <winpr/assert.h>
#include <winpr/string.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <freerdp/log.h>
#define TAG FREERDP_TAG("utils.http")

static const char get_header_fmt[] = "GET %s HTTP/1.1\r\n"
                                     "Host: %s\r\n"
                                     "\r\n";

static const char post_header_fmt[] = "POST %s HTTP/1.1\r\n"
                                      "Host: %s\r\n"
                                      "Content-Type: application/x-www-form-urlencoded\r\n"
                                      "Content-Length: %lu\r\n"
                                      "\r\n";

/* Form fields of an OAuth token request and JSON fields of a token response whose value is a
 * credential. Sorted, but only for reading; the lookup below is linear. */
static const char* const redact_fields[] = { "access_token",  "assertion", "client_secret", "code",
	                                         "code_verifier", "id_token",  "refresh_token" };

static BOOL redact_is_space(char c)
{
	return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}

/* Length of the longest field name matching at [pos], 0 if none of them does. 'code' is a prefix
 * of 'code_verifier', so the longest match is the name. A match always leaves at least one
 * character behind the name for the caller to check. */
static size_t redact_field_len(const char* data, size_t length, size_t pos)
{
	size_t match = 0;
	for (size_t x = 0; x < ARRAYSIZE(redact_fields); x++)
	{
		const size_t flen = strlen(redact_fields[x]);
		if ((flen > match) && (length - pos > flen) &&
		    (strncmp(&data[pos], redact_fields[x], flen) == 0))
			match = flen;
	}
	return match;
}

/* name=value of an application/x-www-form-urlencoded body, at its start or behind a '&'. The
 * value ends at the next '&' or at the end of the body. */
static BOOL redact_form_value(const char* data, size_t length, size_t pos, size_t* start,
                              size_t* end)
{
	if ((pos > 0) && (data[pos - 1] != '&'))
		return FALSE;

	const size_t flen = redact_field_len(data, length, pos);
	if ((flen == 0) || (data[pos + flen] != '='))
		return FALSE;

	*start = pos + flen + 1;
	for (*end = *start; (*end < length) && (data[*end] != '&'); (*end)++)
		;
	return TRUE;
}

/* "name": value of a JSON object. A string value keeps its quotes, any other value ends at the
 * next delimiter. A name that is not followed by a ':' is a value itself, not a field. */
static BOOL redact_json_value(const char* data, size_t length, size_t pos, size_t* start,
                              size_t* end)
{
	if (data[pos] != '"')
		return FALSE;

	const size_t flen = redact_field_len(data, length, pos + 1);
	if ((flen == 0) || (data[pos + 1 + flen] != '"'))
		return FALSE;

	size_t cur = pos + flen + 2;
	for (; (cur < length) && redact_is_space(data[cur]); cur++)
		;
	if ((cur >= length) || (data[cur] != ':'))
		return FALSE;
	for (cur++; (cur < length) && redact_is_space(data[cur]); cur++)
		;
	if (cur >= length)
		return FALSE;

	if (data[cur] == '"')
	{
		*start = cur + 1;
		for (*end = *start; (*end < length) && (data[*end] != '"'); (*end)++)
		{
			if (data[*end] == '\\')
				(*end)++;
		}
		if (*end > length)
			*end = length;
	}
	else
	{
		*start = cur;
		for (*end = *start;
		     (*end < length) && !redact_is_space(data[*end]) && !strchr(",}]", data[*end]);
		     (*end)++)
			;
	}
	return TRUE;
}

/* Appends to a buffer that starts out large enough for the whole input, so it only ever grows by
 * the markers written in place of a value. */
static BOOL redact_append(char** buffer, size_t* length, size_t* capacity, const char* data,
                          size_t size)
{
	if (*length + size + 1 > *capacity)
	{
		const size_t capacity_new = *capacity + size + 1024;
		char* tmp = realloc(*buffer, capacity_new);
		if (!tmp)
			return FALSE;
		*buffer = tmp;
		*capacity = capacity_new;
	}
	memcpy(&(*buffer)[*length], data, size);
	*length += size;
	(*buffer)[*length] = '\0';
	return TRUE;
}

char* freerdp_http_redact_for_log(const char* data, size_t length)
{
	if (!data && (length > 0))
		return nullptr;

	size_t capacity = length + 1;
	size_t used = 0;
	char* buffer = calloc(1, capacity);
	if (!buffer)
		return nullptr;

	for (size_t pos = 0; pos < length;)
	{
		size_t start = 0;
		size_t end = 0;
		if (redact_form_value(data, length, pos, &start, &end) ||
		    redact_json_value(data, length, pos, &start, &end))
		{
			char marker[64] = WINPR_C_ARRAY_INIT;
			const int rc =
			    _snprintf(marker, sizeof(marker), "<redacted, %" PRIuz " bytes>", end - start);
			if ((rc <= 0) || ((size_t)rc >= sizeof(marker)) ||
			    !redact_append(&buffer, &used, &capacity, &data[pos], start - pos) ||
			    !redact_append(&buffer, &used, &capacity, marker, (size_t)rc))
				goto fail;
			pos = end;
		}
		else
		{
			if (!redact_append(&buffer, &used, &capacity, &data[pos], 1))
				goto fail;
			pos++;
		}
	}

	return buffer;

fail:
	free(buffer);
	return nullptr;
}

#define log_errors(log, msg) log_errors_(log, msg, __FILE__, __func__, __LINE__)
static void log_errors_(wLog* log, const char* msg, const char* file, const char* fkt, size_t line)
{
	const DWORD level = WLOG_ERROR;
	unsigned long ec = 0;

	if (!WLog_IsLevelActive(log, level))
		return;

	BOOL error_logged = FALSE;
	while ((ec = ERR_get_error()))
	{
		error_logged = TRUE;
		WLog_PrintTextMessage(log, level, line, file, fkt, "%s: %s", msg,
		                      ERR_error_string(ec, nullptr));
	}
	if (!error_logged)
		WLog_PrintTextMessage(log, level, line, file, fkt, "%s (no details available)", msg);
}

static int get_line(BIO* bio, char* buffer, size_t size)
{
#if !defined(OPENSSL_VERSION_MAJOR) || (OPENSSL_VERSION_MAJOR < 3)
	if (size <= 1)
		return -1;

	size_t pos = 0;
	do
	{
		int rc = BIO_read(bio, &buffer[pos], 1);
		if (rc <= 0)
			return rc;
		char cur = buffer[pos];
		pos += rc;
		if ((cur == '\n') || (pos >= size - 1))
		{
			buffer[pos] = '\0';
			return (int)pos;
		}
	} while (1);
#else
	if (size > INT32_MAX)
		return -1;
	return BIO_get_line(bio, buffer, (int)size);
#endif
}

BOOL freerdp_http_request(const char* url, const char* body, long* status_code, BYTE** response,
                          size_t* response_length)
{
	BOOL ret = FALSE;
	char* hostname = nullptr;
	const char* path = nullptr;
	char* headers = nullptr;
	size_t size = 0;
	int status = 0;
	char buffer[1024] = WINPR_C_ARRAY_INIT;
	BIO* bio = nullptr;
	SSL_CTX* ssl_ctx = nullptr;
	SSL* ssl = nullptr;

	WINPR_ASSERT(status_code);
	WINPR_ASSERT(response);
	WINPR_ASSERT(response_length);

	wLog* log = WLog_Get(TAG);
	WINPR_ASSERT(log);

	*response = nullptr;

	if (!url || strnlen(url, 8) < 8 || strncmp(url, "https://", 8) != 0 ||
	    !(path = strchr(url + 8, '/')))
	{
		WLog_Print(log, WLOG_ERROR, "invalid url provided");
		goto out;
	}

	{
		const size_t len = WINPR_ASSERTING_INT_CAST(size_t, path - (url + 8));
		hostname = strndup(&url[8], len);
	}
	if (!hostname)
		return FALSE;

	{
		size_t blen = 0;
		if (body)
		{
			blen = strlen(body);
			if (winpr_asprintf(&headers, &size, post_header_fmt, path, hostname, blen) < 0)
			{
				free(hostname);
				return FALSE;
			}
		}
		else
		{
			if (winpr_asprintf(&headers, &size, get_header_fmt, path, hostname) < 0)
			{
				free(hostname);
				return FALSE;
			}
		}

		ssl_ctx = SSL_CTX_new(TLS_client_method());

		if (!ssl_ctx)
		{
			log_errors(log, "could not set up ssl context");
			goto out;
		}

		if (!SSL_CTX_set_default_verify_paths(ssl_ctx))
		{
			log_errors(log, "could not set ssl context verify paths");
			goto out;
		}

		SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY);

		bio = BIO_new_ssl_connect(ssl_ctx);
		if (!bio)
		{
			log_errors(log, "could not set up connection");
			goto out;
		}

		if (BIO_set_conn_port(bio, "https") <= 0)
		{
			log_errors(log, "could not set port");
			goto out;
		}

		if (!BIO_set_conn_hostname(bio, hostname))
		{
			log_errors(log, "could not set hostname");
			goto out;
		}

		BIO_get_ssl(bio, &ssl);
		if (!ssl)
		{
			log_errors(log, "could not get ssl");
			goto out;
		}

		if (!SSL_set_tlsext_host_name(ssl, hostname))
		{
			log_errors(log, "could not set sni hostname");
			goto out;
		}

		WLog_Print(log, WLOG_DEBUG, "headers:\n%s", headers);
		ERR_clear_error();

		{
			const size_t hlen = strnlen(headers, size);
			if (hlen > INT32_MAX)
				goto out;

			if (BIO_write(bio, headers, (int)hlen) < 0)
			{
				log_errors(log, "could not write headers");
				goto out;
			}

			if (body)
			{
				if (WLog_IsLevelActive(log, WLOG_DEBUG))
				{
					char* redacted = freerdp_http_redact_for_log(body, blen);
					WLog_Print(log, WLOG_DEBUG, "body:\n%s",
					           redacted ? redacted : "<could not redact>");
					free(redacted);
				}

				if (blen > INT_MAX)
				{
					WLog_Print(log, WLOG_ERROR, "body too long!");
					goto out;
				}

				ERR_clear_error();
				if (BIO_write(bio, body, (int)blen) < 0)
				{
					log_errors(log, "could not write body");
					goto out;
				}
			}
		}
	}

	status = get_line(bio, buffer, sizeof(buffer));
	if (status <= 0)
	{
		log_errors(log, "could not read response");
		goto out;
	}

	const char header[9] = { 'H', 'T', 'T', 'P', '/', '1', '.', '1', ' ' };
	if ((status < (INT64)sizeof(header)) || (strncmp(header, buffer, sizeof(header)) != 0))
	{
		WLog_Print(log, WLOG_ERROR, "invalid HTTP status header");
		goto out;
	}

	errno = 0;
	*status_code = strtol(&buffer[sizeof(header)], nullptr, 0);
	if (errno != 0)
	{
		char ebuffer[256] = WINPR_C_ARRAY_INIT;
		WLog_Print(log, WLOG_ERROR, "invalid HTTP status line: %s [%d]",
		           winpr_strerror(errno, ebuffer, sizeof(ebuffer)), errno);
		goto out;
	}

	do
	{
		status = get_line(bio, buffer, sizeof(buffer));
		if (status <= 0)
		{
			log_errors(log, "could not read response");
			goto out;
		}

		char* val = nullptr;
		char* name = strtok_s(buffer, ":", &val);
		if (name && (_stricmp(name, "content-length") == 0))
		{
			errno = 0;
			*response_length = strtoul(val, nullptr, 10);
			if (errno)
			{
				char ebuffer[256] = WINPR_C_ARRAY_INIT;
				WLog_Print(log, WLOG_ERROR, "could not parse content length (%s): %s [%d]", val,
				           winpr_strerror(errno, ebuffer, sizeof(ebuffer)), errno);
				goto out;
			}
		}
	} while (strcmp(buffer, "\r\n") != 0);

	if (*response_length > 0)
	{
		if (*response_length > INT_MAX)
		{
			WLog_Print(log, WLOG_ERROR, "response too long!");
			goto out;
		}

		*response = calloc(1, *response_length + 1);
		if (!*response)
			goto out;

		BYTE* p = *response;
		size_t left = *response_length;
		while (left > 0)
		{
			const int rd = (left < INT32_MAX) ? (int)left : INT32_MAX;
			status = BIO_read(bio, p, rd);
			if (status <= 0)
			{
				log_errors(log, "could not read response");
				goto out;
			}
			p += status;
			if ((size_t)status > left)
				break;
			left -= (size_t)status;
		}
	}

	if (WLog_IsLevelActive(log, WLOG_DEBUG))
	{
		char* redacted = freerdp_http_redact_for_log((const char*)(*response), *response_length);
		WLog_Print(log, WLOG_DEBUG, "response[%" PRIuz "]:\n%s", *response_length,
		           redacted ? redacted : "<could not redact>");
		free(redacted);
	}
	ret = TRUE;

out:
	if (!ret)
	{
		free(*response);
		*response = nullptr;
		*response_length = 0;
	}
	free(hostname);
	free(headers);
	BIO_free_all(bio);
	SSL_CTX_free(ssl_ctx);
	return ret;
}

const char* freerdp_http_status_string(long status)
{
	switch (status)
	{
		case HTTP_STATUS_CONTINUE:
			return "HTTP_STATUS_CONTINUE";
		case HTTP_STATUS_SWITCH_PROTOCOLS:
			return "HTTP_STATUS_SWITCH_PROTOCOLS";
		case HTTP_STATUS_OK:
			return "HTTP_STATUS_OK";
		case HTTP_STATUS_CREATED:
			return "HTTP_STATUS_CREATED";
		case HTTP_STATUS_ACCEPTED:
			return "HTTP_STATUS_ACCEPTED";
		case HTTP_STATUS_PARTIAL:
			return "HTTP_STATUS_PARTIAL";
		case HTTP_STATUS_NO_CONTENT:
			return "HTTP_STATUS_NO_CONTENT";
		case HTTP_STATUS_RESET_CONTENT:
			return "HTTP_STATUS_RESET_CONTENT";
		case HTTP_STATUS_PARTIAL_CONTENT:
			return "HTTP_STATUS_PARTIAL_CONTENT";
		case HTTP_STATUS_WEBDAV_MULTI_STATUS:
			return "HTTP_STATUS_WEBDAV_MULTI_STATUS";
		case HTTP_STATUS_AMBIGUOUS:
			return "HTTP_STATUS_AMBIGUOUS";
		case HTTP_STATUS_MOVED:
			return "HTTP_STATUS_MOVED";
		case HTTP_STATUS_REDIRECT:
			return "HTTP_STATUS_REDIRECT";
		case HTTP_STATUS_REDIRECT_METHOD:
			return "HTTP_STATUS_REDIRECT_METHOD";
		case HTTP_STATUS_NOT_MODIFIED:
			return "HTTP_STATUS_NOT_MODIFIED";
		case HTTP_STATUS_USE_PROXY:
			return "HTTP_STATUS_USE_PROXY";
		case HTTP_STATUS_REDIRECT_KEEP_VERB:
			return "HTTP_STATUS_REDIRECT_KEEP_VERB";
		case HTTP_STATUS_BAD_REQUEST:
			return "HTTP_STATUS_BAD_REQUEST";
		case HTTP_STATUS_DENIED:
			return "HTTP_STATUS_DENIED";
		case HTTP_STATUS_PAYMENT_REQ:
			return "HTTP_STATUS_PAYMENT_REQ";
		case HTTP_STATUS_FORBIDDEN:
			return "HTTP_STATUS_FORBIDDEN";
		case HTTP_STATUS_NOT_FOUND:
			return "HTTP_STATUS_NOT_FOUND";
		case HTTP_STATUS_BAD_METHOD:
			return "HTTP_STATUS_BAD_METHOD";
		case HTTP_STATUS_NONE_ACCEPTABLE:
			return "HTTP_STATUS_NONE_ACCEPTABLE";
		case HTTP_STATUS_PROXY_AUTH_REQ:
			return "HTTP_STATUS_PROXY_AUTH_REQ";
		case HTTP_STATUS_REQUEST_TIMEOUT:
			return "HTTP_STATUS_REQUEST_TIMEOUT";
		case HTTP_STATUS_CONFLICT:
			return "HTTP_STATUS_CONFLICT";
		case HTTP_STATUS_GONE:
			return "HTTP_STATUS_GONE";
		case HTTP_STATUS_LENGTH_REQUIRED:
			return "HTTP_STATUS_LENGTH_REQUIRED";
		case HTTP_STATUS_PRECOND_FAILED:
			return "HTTP_STATUS_PRECOND_FAILED";
		case HTTP_STATUS_REQUEST_TOO_LARGE:
			return "HTTP_STATUS_REQUEST_TOO_LARGE";
		case HTTP_STATUS_URI_TOO_LONG:
			return "HTTP_STATUS_URI_TOO_LONG";
		case HTTP_STATUS_UNSUPPORTED_MEDIA:
			return "HTTP_STATUS_UNSUPPORTED_MEDIA";
		case HTTP_STATUS_RETRY_WITH:
			return "HTTP_STATUS_RETRY_WITH";
		case HTTP_STATUS_SERVER_ERROR:
			return "HTTP_STATUS_SERVER_ERROR";
		case HTTP_STATUS_NOT_SUPPORTED:
			return "HTTP_STATUS_NOT_SUPPORTED";
		case HTTP_STATUS_BAD_GATEWAY:
			return "HTTP_STATUS_BAD_GATEWAY";
		case HTTP_STATUS_SERVICE_UNAVAIL:
			return "HTTP_STATUS_SERVICE_UNAVAIL";
		case HTTP_STATUS_GATEWAY_TIMEOUT:
			return "HTTP_STATUS_GATEWAY_TIMEOUT";
		case HTTP_STATUS_VERSION_NOT_SUP:
			return "HTTP_STATUS_VERSION_NOT_SUP";
		default:
			return "HTTP_STATUS_UNKNOWN";
	}
}

const char* freerdp_http_status_string_format(long status, char* buffer, size_t size)
{
	const char* code = freerdp_http_status_string(status);
	(void)_snprintf(buffer, size, "%s [%ld]", code, status);
	return buffer;
}
