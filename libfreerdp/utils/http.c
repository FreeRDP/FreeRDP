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

static void log_errors(const char* msg)
{
	unsigned long ec = 0;
	while ((ec = ERR_get_error()))
		WLog_ERR(TAG, "%s: %s", msg, ERR_error_string(ec, NULL));
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

	WINPR_ASSERT(status_code);
	WINPR_ASSERT(response);
	WINPR_ASSERT(response_length);

	*response = NULL;

	if (!url || strlen(url) < 8 || strncmp(url, "https://", 8) != 0 ||
	    !(path = strchr(url + 8, '/')))
	{
		WLog_ERR(TAG, "invalid url provided");
		goto out;
	}

	hostname = calloc(1, path - (url + 8) + 1);
	if (!hostname)
		return FALSE;
	strncpy(hostname, url + 8, path - (url + 8));

	if (body)
	{
		if (winpr_asprintf(&headers, &size, post_header_fmt, path, hostname, strlen(body)) < 0)
			return FALSE;
	}
	else
	{
		if (winpr_asprintf(&headers, &size, get_header_fmt, path, hostname) < 0)
			return FALSE;
	}

	ssl_ctx = SSL_CTX_new(TLS_client_method());

	if (!ssl_ctx)
	{
		log_errors("could not set up ssl context");
		goto out;
	}

	if (!SSL_CTX_set_default_verify_paths(ssl_ctx))
	{
		log_errors("could not set ssl context verify paths");
		goto out;
	}

	SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY);

	bio = BIO_new_ssl_connect(ssl_ctx);
	if (!bio)
	{
		log_errors("could not set up connection");
		goto out;
	}

	if (BIO_set_conn_port(bio, "https") <= 0)
	{
		log_errors("could not set port");
		goto out;
	}

	if (!BIO_set_conn_hostname(bio, hostname))
	{
		log_errors("could not set hostname");
		goto out;
	}

	WLog_DBG(TAG, "headers:\n%s", headers);
	ERR_clear_error();
	if (BIO_write(bio, headers, strlen(headers)) < 0)
	{
		log_errors("could not write headers");
		goto out;
	}

	if (body)
	{
		WLog_DBG(TAG, "body:\n%s", body);

		if (strlen(body) > INT_MAX)
		{
			WLog_ERR(TAG, "body too long!");
			goto out;
		}

		ERR_clear_error();
		if (BIO_write(bio, body, strlen(body)) < 0)
		{
			log_errors("could not write body");
			goto out;
		}
	}

	status = BIO_get_line(bio, buffer, sizeof(buffer));
	if (status <= 0)
	{
		log_errors("could not read response");
		goto out;
	}

	if (sscanf(buffer, "HTTP/1.1 %li %*[^\r\n]\r\n", status_code) < 1)
	{
		WLog_ERR(TAG, "invalid HTTP status line");
		goto out;
	}

	do
	{
		status = BIO_get_line(bio, buffer, sizeof(buffer));
		if (status <= 0)
		{
			log_errors("could not read response");
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
				WLog_ERR(TAG, "could not parse content length (%s): %s [%d]", val, strerror(errno),
				         errno);
				goto out;
			}
		}
	} while (strcmp(buffer, "\r\n") != 0);

	if (*response_length > 0)
	{
		*response = calloc(1, *response_length + 1);
		if (!*response)
			goto out;

		if (*response_length > INT_MAX)
		{
			WLog_ERR(TAG, "response too long!");
			goto out;
		}

		BYTE* p = *response;
		int left = *response_length;
		while (left > 0)
		{
			status = BIO_read(bio, p, left);
			if (status <= 0)
			{
				log_errors("could not read response");
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
