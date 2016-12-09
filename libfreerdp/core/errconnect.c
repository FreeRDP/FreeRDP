/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Error Connect
 *
 * Copyright 2015 Armin Novak <armin.novak@thincast.com>
 * Copyright 2015 Thincast Technologies GmbH
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

#include <stdio.h>

#include <freerdp/log.h>

#include "errinfo.h"

#define TAG FREERDP_TAG("core")

#define ERRCONNECT_DEFINE(_code)	{ ERRCONNECT_##_code , "ERRCONNECT_" #_code , ERRCONNECT_##_code##_STRING }

/* Protocol-independent codes */

#define ERRCONNECT_PRE_CONNECT_FAILED_STRING \
		"A configuration error prevented a connection to be established."

#define ERRCONNECT_CONNECT_UNDEFINED_STRING \
		"A undefined connection error occurred."

#define ERRCONNECT_POST_CONNECT_FAILED_STRING \
		"The connection attempt was aborted due to post connect configuration errors."

#define ERRCONNECT_DNS_ERROR_STRING \
		"The DNS entry could not be resolved."

#define ERRCONNECT_DNS_NAME_NOT_FOUND_STRING \
		"The DNS host name was not found."

#define ERRCONNECT_CONNECT_FAILED_STRING \
		"The connection failed."

#define ERRCONNECT_MCS_CONNECT_INITIAL_ERROR_STRING \
		"The connection failed at initial MCS connect"

#define ERRCONNECT_TLS_CONNECT_FAILED_STRING \
		"The connection failed at TLS connect."

#define ERRCONNECT_AUTHENTICATION_FAILED_STRING \
		"An authentication failure aborted the connection."

#define ERRCONNECT_INSUFFICIENT_PRIVILEGES_STRING \
		"Insufficient privileges to establish a connection."

#define ERRCONNECT_CONNECT_CANCELLED_STRING \
		"The connection was cancelled."

#define ERRCONNECT_SECURITY_NEGO_CONNECT_FAILED_STRING \
		"The connection failed at negociating security settings."

#define ERRCONNECT_CONNECT_TRANSPORT_FAILED_STRING \
		"The connection transport layer failed."

/* Special codes */
#define ERRCONNECT_SUCCESS_STRING "Success."
#define ERRCONNECT_NONE_STRING ""

static const ERRINFO ERRCONNECT_CODES[] =
{
		ERRCONNECT_DEFINE(SUCCESS),

		ERRCONNECT_DEFINE(PRE_CONNECT_FAILED),
		ERRCONNECT_DEFINE(CONNECT_UNDEFINED),
		ERRCONNECT_DEFINE(POST_CONNECT_FAILED),
		ERRCONNECT_DEFINE(DNS_ERROR),
		ERRCONNECT_DEFINE(DNS_NAME_NOT_FOUND),
		ERRCONNECT_DEFINE(CONNECT_FAILED),
		ERRCONNECT_DEFINE(MCS_CONNECT_INITIAL_ERROR),
		ERRCONNECT_DEFINE(TLS_CONNECT_FAILED),
		ERRCONNECT_DEFINE(AUTHENTICATION_FAILED),
		ERRCONNECT_DEFINE(INSUFFICIENT_PRIVILEGES),
		ERRCONNECT_DEFINE(CONNECT_CANCELLED),
		ERRCONNECT_DEFINE(SECURITY_NEGO_CONNECT_FAILED),
		ERRCONNECT_DEFINE(CONNECT_TRANSPORT_FAILED),

		ERRCONNECT_DEFINE(NONE)
};

const char* freerdp_get_error_connect_string(UINT32 code)
{
	const ERRINFO* errInfo;

	errInfo = &ERRCONNECT_CODES[0];

	while (errInfo->code != ERRCONNECT_NONE)
	{
		if (code == errInfo->code)
		{
			return errInfo->info;
		}

		errInfo++;
	}

	return "ERRCONNECT_UNKNOWN";
}

const char* freerdp_get_error_connect_name(UINT32 code)
{
	const ERRINFO* errInfo;

	errInfo = &ERRCONNECT_CODES[0];

	while (errInfo->code != ERRCONNECT_NONE)
	{
		if (code == errInfo->code)
		{
			return errInfo->name;
		}

		errInfo++;
	}

	return "ERRCONNECT_UNKNOWN";
}

