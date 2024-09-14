/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Smartcard Device Service Virtual Channel
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

#ifndef FREERDP_UTILS_HTTP_H
#define FREERDP_UTILS_HTTP_H

#include <freerdp/api.h>

typedef enum
{
	HTTP_STATUS_CONTINUE = 100,
	HTTP_STATUS_SWITCH_PROTOCOLS = 101,
	HTTP_STATUS_OK = 200,
	HTTP_STATUS_CREATED = 201,
	HTTP_STATUS_ACCEPTED = 202,
	HTTP_STATUS_PARTIAL = 203,
	HTTP_STATUS_NO_CONTENT = 204,
	HTTP_STATUS_RESET_CONTENT = 205,
	HTTP_STATUS_PARTIAL_CONTENT = 206,
	HTTP_STATUS_WEBDAV_MULTI_STATUS = 207,
	HTTP_STATUS_AMBIGUOUS = 300,
	HTTP_STATUS_MOVED = 301,
	HTTP_STATUS_REDIRECT = 302,
	HTTP_STATUS_REDIRECT_METHOD = 303,
	HTTP_STATUS_NOT_MODIFIED = 304,
	HTTP_STATUS_USE_PROXY = 305,
	HTTP_STATUS_REDIRECT_KEEP_VERB = 307,
	HTTP_STATUS_BAD_REQUEST = 400,
	HTTP_STATUS_DENIED = 401,
	HTTP_STATUS_PAYMENT_REQ = 402,
	HTTP_STATUS_FORBIDDEN = 403,
	HTTP_STATUS_NOT_FOUND = 404,
	HTTP_STATUS_BAD_METHOD = 405,
	HTTP_STATUS_NONE_ACCEPTABLE = 406,
	HTTP_STATUS_PROXY_AUTH_REQ = 407,
	HTTP_STATUS_REQUEST_TIMEOUT = 408,
	HTTP_STATUS_CONFLICT = 409,
	HTTP_STATUS_GONE = 410,
	HTTP_STATUS_LENGTH_REQUIRED = 411,
	HTTP_STATUS_PRECOND_FAILED = 412,
	HTTP_STATUS_REQUEST_TOO_LARGE = 413,
	HTTP_STATUS_URI_TOO_LONG = 414,
	HTTP_STATUS_UNSUPPORTED_MEDIA = 415,
	HTTP_STATUS_RETRY_WITH = 449,
	HTTP_STATUS_SERVER_ERROR = 500,
	HTTP_STATUS_NOT_SUPPORTED = 501,
	HTTP_STATUS_BAD_GATEWAY = 502,
	HTTP_STATUS_SERVICE_UNAVAIL = 503,
	HTTP_STATUS_GATEWAY_TIMEOUT = 504,
	HTTP_STATUS_VERSION_NOT_SUP = 505
} FREERDP_HTTP_STATUS;

FREERDP_API BOOL freerdp_http_request(const char* url, const char* body, long* status_code,
                                      BYTE** response, size_t* response_length);

FREERDP_API const char* freerdp_http_status_string(long status);
FREERDP_API const char* freerdp_http_status_string_format(long status, char* buffer, size_t size);

#endif /* FREERDP_UTILS_HTTP_H */
