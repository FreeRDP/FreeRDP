/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Azure Virtual Desktop Gateway / Azure Resource Manager
 *
 * Copyright 2023 Michael Saxl <mike@mwsys.mine.bz>
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

#include <winpr/assert.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <winpr/winsock.h>
#include <winpr/cred.h>

#include <freerdp/log.h>
#include <freerdp/error.h>
#include <freerdp/utils/ringbuffer.h>
#include <freerdp/utils/smartcardlogon.h>

#include "wst.h"
#include "websocket.h"
#include "http.h"
#include "../credssp_auth.h"
#include "../proxy.h"
#include "../rdp.h"
#include "../../crypto/opensslcompat.h"
#include "rpc_fault.h"
#include "../utils.h"
#include "../settings.h"

#ifdef WITH_AAD
#include <cjson/cJSON.h>
#endif

struct rdp_arm
{
	rdpContext* context;
	rdpTls* tls;
	HttpContext* http;
};

typedef struct rdp_arm rdpArm;

#define TAG FREERDP_TAG("core.gateway.arm")

#ifdef WITH_AAD
static BOOL arm_tls_connect(rdpArm* arm, rdpTls* tls, int timeout)
{
	WINPR_ASSERT(arm);
	WINPR_ASSERT(tls);
	int sockfd = 0;
	long status = 0;
	BIO* socketBio = NULL;
	BIO* bufferedBio = NULL;
	rdpSettings* settings = arm->context->settings;
	if (!settings)
		return FALSE;

	const char* peerHostname = freerdp_settings_get_string(settings, FreeRDP_GatewayHostname);
	if (!peerHostname)
		return FALSE;

	UINT16 peerPort = (UINT16)freerdp_settings_get_uint32(settings, FreeRDP_GatewayPort);
	const char *proxyUsername, *proxyPassword;
	BOOL isProxyConnection =
	    proxy_prepare(settings, &peerHostname, &peerPort, &proxyUsername, &proxyPassword);

	sockfd = freerdp_tcp_connect(arm->context, peerHostname, peerPort, timeout);

	WLog_DBG(TAG, "connecting to %s %d", peerHostname, peerPort);
	if (sockfd < 0)
		return FALSE;

	socketBio = BIO_new(BIO_s_simple_socket());

	if (!socketBio)
	{
		closesocket((SOCKET)sockfd);
		return FALSE;
	}

	BIO_set_fd(socketBio, sockfd, BIO_CLOSE);
	bufferedBio = BIO_new(BIO_s_buffered_socket());

	if (!bufferedBio)
	{
		BIO_free_all(socketBio);
		return FALSE;
	}

	bufferedBio = BIO_push(bufferedBio, socketBio);
	if (!bufferedBio)
		return FALSE;

	status = BIO_set_nonblock(bufferedBio, TRUE);

	if (isProxyConnection)
	{
		if (!proxy_connect(settings, bufferedBio, proxyUsername, proxyPassword,
		                   freerdp_settings_get_string(settings, FreeRDP_GatewayHostname),
		                   (UINT16)freerdp_settings_get_uint32(settings, FreeRDP_GatewayPort)))
		{
			BIO_free_all(bufferedBio);
			return FALSE;
		}
	}

	if (!status)
	{
		BIO_free_all(bufferedBio);
		return FALSE;
	}

	tls->hostname = freerdp_settings_get_string(settings, FreeRDP_GatewayHostname);
	tls->port = settings->GatewayPort;
	tls->isGatewayTransport = TRUE;
	status = freerdp_tls_connect(tls, bufferedBio);
	if (status < 1)
	{
		rdpContext* context = arm->context;
		if (status < 0)
			freerdp_set_last_error_if_not(context, FREERDP_ERROR_TLS_CONNECT_FAILED);
		else
			freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_CANCELLED);

		return FALSE;
	}
	return (status >= 1);
}

static wStream* arm_build_http_request(rdpArm* arm, const char* method,
                                       TRANSFER_ENCODING transferEncoding, const char* content_type,
                                       size_t content_length)
{
	wStream* s = NULL;
	HttpRequest* request = NULL;
	const char* uri;

	WINPR_ASSERT(arm);
	WINPR_ASSERT(method);
	WINPR_ASSERT(content_type);

	WINPR_ASSERT(arm->context);

	freerdp* instance = arm->context->instance;
	WINPR_ASSERT(instance);

	uri = http_context_get_uri(arm->http);
	request = http_request_new();

	if (!request)
		return NULL;

	if (!http_request_set_method(request, method) || !http_request_set_uri(request, uri))
		goto out;

	if (!freerdp_settings_get_string(arm->context->settings, FreeRDP_GatewayHttpExtAuthBearer))
	{
		char* token = NULL;

		if (!instance->GetAccessToken)
		{
			WLog_ERR(TAG, "No authorization token provided");
			goto out;
		}

		if (!instance->GetAccessToken(instance, ACCESS_TOKEN_TYPE_AVD, &token, 0))
		{
			WLog_ERR(TAG, "Unable to obtain access token");
			goto out;
		}

		if (!freerdp_settings_set_string(arm->context->settings, FreeRDP_GatewayHttpExtAuthBearer,
		                                 token))
		{
			free(token);
			goto out;
		}
		free(token);
	}

	if (!http_request_set_auth_scheme(request, "Bearer") ||
	    !http_request_set_auth_param(
	        request,
	        freerdp_settings_get_string(arm->context->settings, FreeRDP_GatewayHttpExtAuthBearer)))
		goto out;

	if (!http_request_set_transfer_encoding(request, transferEncoding) ||
	    !http_request_set_content_length(request, content_length) ||
	    !http_request_set_content_type(request, content_type))
		goto out;

	s = http_request_write(arm->http, request);
out:
	http_request_free(request);

	if (s)
		Stream_SealLength(s);

	return s;
}

static BOOL arm_send_http_request(rdpArm* arm, rdpTls* tls, const char* method,
                                  const char* content_type, const char* data, size_t content_length)
{
	int status = -1;
	wStream* s =
	    arm_build_http_request(arm, method, TransferEncodingIdentity, content_type, content_length);

	if (!s)
		return FALSE;

	const size_t sz = Stream_Length(s);

	if (sz <= INT_MAX)
		status = freerdp_tls_write_all(tls, Stream_Buffer(s), (int)sz);

	Stream_Free(s, TRUE);
	if (status >= 0 && content_length > 0 && data)
		status = freerdp_tls_write_all(tls, (const BYTE*)data, content_length);

	return (status >= 0);
}

static void arm_free(rdpArm* arm)
{
	if (!arm)
		return;

	freerdp_tls_free(arm->tls);
	http_context_free(arm->http);

	free(arm);
}

static rdpArm* arm_new(rdpContext* context)
{
	WINPR_ASSERT(context);

	rdpArm* arm = (rdpArm*)calloc(1, sizeof(rdpArm));
	if (!arm)
		goto fail;

	arm->context = context;
	arm->tls = freerdp_tls_new(context->settings);
	if (!arm->tls)
		goto fail;

	arm->http = http_context_new();

	if (!arm->http)
		goto fail;

	return arm;

fail:
	arm_free(arm);
	return NULL;
}

static char* arm_create_request_json(rdpArm* arm)
{
	char* lbi = NULL;
	char* message = NULL;

	WINPR_ASSERT(arm);

	cJSON* json = cJSON_CreateObject();
	if (!json)
		goto arm_create_cleanup;
	cJSON_AddStringToObject(
	    json, "application",
	    freerdp_settings_get_string(arm->context->settings, FreeRDP_RemoteApplicationProgram));

	lbi = calloc(
	    freerdp_settings_get_uint32(arm->context->settings, FreeRDP_LoadBalanceInfoLength) + 1,
	    sizeof(char));
	if (!lbi)
		goto arm_create_cleanup;

	const size_t len =
	    freerdp_settings_get_uint32(arm->context->settings, FreeRDP_LoadBalanceInfoLength);
	memcpy(lbi, freerdp_settings_get_pointer(arm->context->settings, FreeRDP_LoadBalanceInfo), len);

	cJSON_AddStringToObject(json, "loadBalanceInfo", lbi);
	cJSON_AddNullToObject(json, "LogonToken");
	cJSON_AddNullToObject(json, "gatewayLoadBalancerToken");

	message = cJSON_PrintUnformatted(json);
arm_create_cleanup:
	if (json)
		cJSON_Delete(json);
	free(lbi);
	return message;
}

static BOOL arm_fill_gateway_parameters(rdpArm* arm, const char* message)
{
	WINPR_ASSERT(arm);
	WINPR_ASSERT(arm->context);
	WINPR_ASSERT(message);

	cJSON* json = cJSON_Parse(message);
	BOOL status = FALSE;
	if (!json)
		return FALSE;
	else
	{
		const cJSON* gwurl = cJSON_GetObjectItemCaseSensitive(json, "gatewayLocation");
		if (cJSON_IsString(gwurl) && (gwurl->valuestring != NULL))
		{
			WLog_DBG(TAG, "extracted target url %s", gwurl->valuestring);
			if (!freerdp_settings_set_string(arm->context->settings, FreeRDP_GatewayUrl,
			                                 gwurl->valuestring))
				status = FALSE;
			else
				status = TRUE;
		}

		const cJSON* hostname = cJSON_GetObjectItem(json, "redirectedServerName");
		if (cJSON_GetStringValue(hostname))
		{
			if (!freerdp_settings_set_string(arm->context->settings, FreeRDP_ServerHostname,
			                                 cJSON_GetStringValue(hostname)))
				status = FALSE;
			else
				status = TRUE;
		}

		cJSON_Delete(json);
	}
	return status;
}

#endif

BOOL arm_resolve_endpoint(rdpContext* context, DWORD timeout)
{
#ifndef WITH_AAD
	WLog_ERR(TAG, "arm gateway support not compiled in");
	return FALSE;
#else

	if (!context)
		return FALSE;

	if (!context->settings)
		return FALSE;

	if ((freerdp_settings_get_uint32(context->settings, FreeRDP_LoadBalanceInfoLength) == 0) ||
	    (freerdp_settings_get_string(context->settings, FreeRDP_RemoteApplicationProgram) == NULL))
	{
		WLog_ERR(TAG, "loadBalanceInfo and RemoteApplicationProgram needed");
		return FALSE;
	}

	rdpArm* arm = arm_new(context);
	if (!arm)
		return FALSE;

	char* message = NULL;
	BOOL rc = FALSE;

	HttpResponse* response = NULL;
	long StatusCode;

	if (!http_context_set_uri(arm->http, "/api/arm/v2/connections/") ||
	    !http_context_set_accept(arm->http, "application/json") ||
	    !http_context_set_cache_control(arm->http, "no-cache") ||
	    !http_context_set_pragma(arm->http, "no-cache") ||
	    !http_context_set_connection(arm->http, "Keep-Alive") ||
	    !http_context_set_user_agent(arm->http, "FreeRDP/3.0") ||
	    !http_context_set_x_ms_user_agent(arm->http, "FreeRDP/3.0") ||
	    !http_context_set_host(arm->http, freerdp_settings_get_string(arm->context->settings,
	                                                                  FreeRDP_GatewayHostname)))
		goto arm_error;

	if (!arm_tls_connect(arm, arm->tls, timeout))
		goto arm_error;

	message = arm_create_request_json(arm);
	if (!message)
		goto arm_error;

	if (!arm_send_http_request(arm, arm->tls, "POST", "application/json", message, strlen(message)))
		goto arm_error;

	response = http_response_recv(arm->tls, TRUE);
	if (!response)
		goto arm_error;

	StatusCode = http_response_get_status_code(response);
	if (StatusCode == HTTP_STATUS_OK)
	{
		char* msg = calloc(http_response_get_body_length(response) + 1, sizeof(char));
		if (!msg)
			goto arm_error;

		memcpy(msg, http_response_get_body(response), http_response_get_body_length(response));

		WLog_DBG(TAG, "Got HTTP Response data: %s", msg);
		const BOOL res = arm_fill_gateway_parameters(arm, msg);
		free(msg);
		if (!res)
			goto arm_error;
	}
	else
	{
		char buffer[64] = { 0 };
		WLog_ERR(TAG, "Unexpected HTTP status: %s",
		         freerdp_http_status_string_format(StatusCode, buffer, ARRAYSIZE(buffer)));
		goto arm_error;
	}

	rc = TRUE;
arm_error:
	http_response_free(response);
	arm_free(arm);
	free(message);
	return rc;
#endif
}
