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
		WLog_PrintMessage(log, WLOG_MESSAGE_TEXT, level, line, file, fkt, "%s: %s", msg,
		                  ERR_error_string(ec, NULL));
	}
	if (!error_logged)
		WLog_PrintMessage(log, WLOG_MESSAGE_TEXT, level, line, file, fkt,
		                  "%s (no details available)", msg);
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
	return BIO_get_line(bio, buffer, size);
#endif
}

BOOL freerdp_http_request(const char* url, const char* body, long* status_code, BYTE** response,
                          size_t* response_length)
{
	BOOL ret = FALSE;
	char* hostname = NULL;
	const char* path = NULL;
	char* headers = NULL;
	size_t size = 0;
	int status = 0;
	char buffer[1024] = { 0 };
	BIO* bio = NULL;
	SSL_CTX* ssl_ctx = NULL;
	SSL* ssl = NULL;

	WINPR_ASSERT(status_code);
	WINPR_ASSERT(response);
	WINPR_ASSERT(response_length);

	wLog* log = WLog_Get(TAG);
	WINPR_ASSERT(log);

	*response = NULL;

	if (!url || strnlen(url, 8) < 8 || strncmp(url, "https://", 8) != 0 ||
	    !(path = strchr(url + 8, '/')))
	{
		WLog_Print(log, WLOG_ERROR, "invalid url provided");
		goto out;
	}

	const size_t len = path - (url + 8);
	hostname = strndup(&url[8], len);
	if (!hostname)
		return FALSE;

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
	if (BIO_write(bio, headers, strnlen(headers, size)) < 0)
	{
		log_errors(log, "could not write headers");
		goto out;
	}

	if (body)
	{
		WLog_Print(log, WLOG_DEBUG, "body:\n%s", body);

		if (blen > INT_MAX)
		{
			WLog_Print(log, WLOG_ERROR, "body too long!");
			goto out;
		}

		ERR_clear_error();
		if (BIO_write(bio, body, blen) < 0)
		{
			log_errors(log, "could not write body");
			goto out;
		}
	}

	status = get_line(bio, buffer, sizeof(buffer));
	if (status <= 0)
	{
		log_errors(log, "could not read response");
		goto out;
	}

	// NOLINTNEXTLINE(cert-err34-c)
	if (sscanf(buffer, "HTTP/1.1 %li %*[^\r\n]\r\n", status_code) < 1)
	{
		WLog_Print(log, WLOG_ERROR, "invalid HTTP status line");
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

		char* val = NULL;
		char* name = strtok_s(buffer, ":", &val);
		if (name && (_stricmp(name, "content-length") == 0))
		{
			errno = 0;
			*response_length = strtoul(val, NULL, 10);
			if (errno)
			{
				char ebuffer[256] = { 0 };
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
		int left = *response_length;
		while (left > 0)
		{
			status = BIO_read(bio, p, left);
			if (status <= 0)
			{
				log_errors(log, "could not read response");
				goto out;
			}
			p += status;
			left -= status;
		}
	}

	ret = TRUE;

out:
	if (!ret)
	{
		free(*response);
		*response = NULL;
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
