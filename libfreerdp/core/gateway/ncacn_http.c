/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RPC over HTTP (ncacn_http)
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

#include "ncacn_http.h"

#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/stream.h>
#include <winpr/dsparse.h>

#include "../utils.h"

#define TAG FREERDP_TAG("core.gateway.ntlm")

#define AUTH_PKG NTLM_SSP_NAME

static wStream* rpc_auth_http_request(HttpContext* http, const char* method, int contentLength,
                                      const SecBuffer* authToken, const char* auth_scheme)
{
	wStream* s = NULL;
	HttpRequest* request = NULL;
	char* base64AuthToken = NULL;
	const char* uri;

	if (!http || !method)
		goto fail;

	request = http_request_new();

	if (!request)
		goto fail;

	if (authToken)
		base64AuthToken = crypto_base64_encode(authToken->pvBuffer, authToken->cbBuffer);

	uri = http_context_get_uri(http);

	if (!http_request_set_method(request, method) ||
	    !http_request_set_content_length(request, contentLength) ||
	    !http_request_set_uri(request, uri))
		goto fail;

	if (base64AuthToken)
	{
		if (!http_request_set_auth_scheme(request, auth_scheme) ||
		    !http_request_set_auth_param(request, base64AuthToken))
			goto fail;
	}

	s = http_request_write(http, request);
fail:
	http_request_free(request);
	free(base64AuthToken);
	return s;
}

BOOL rpc_ncacn_http_send_in_channel_request(RpcChannel* inChannel)
{
	wStream* s;
	SSIZE_T status;
	int contentLength;
	rdpCredsspAuth* auth;
	HttpContext* http;
	const SecBuffer* buffer;
	int rc;

	if (!inChannel || !inChannel->auth || !inChannel->http)
		return FALSE;

	auth = inChannel->auth;
	http = inChannel->http;

	rc = credssp_auth_authenticate(auth);
	if (rc < 0)
		return FALSE;

	contentLength = (rc == 0) ? 0 : 0x40000000;
	buffer = credssp_auth_have_output_token(auth) ? credssp_auth_get_output_buffer(auth) : NULL;
	s = rpc_auth_http_request(http, "RPC_IN_DATA", contentLength, buffer,
	                          credssp_auth_pkg_name(auth));

	if (!s)
		return -1;

	status = rpc_channel_write(inChannel, Stream_Buffer(s), Stream_Length(s));
	Stream_Free(s, TRUE);
	return (status > 0) ? 1 : -1;
}

BOOL rpc_ncacn_http_recv_in_channel_response(RpcChannel* inChannel, HttpResponse* response)
{
	const char* token64 = NULL;
	size_t authTokenLength = 0;
	BYTE* authTokenData = NULL;
	rdpCredsspAuth* auth;
	SecBuffer buffer = { 0 };

	if (!inChannel || !response || !inChannel->auth)
		return FALSE;

	auth = inChannel->auth;
	token64 = http_response_get_auth_token(response, credssp_auth_pkg_name(auth));

	if (token64)
		crypto_base64_decode(token64, strlen(token64), &authTokenData, &authTokenLength);

	buffer.pvBuffer = authTokenData;
	buffer.cbBuffer = authTokenLength;

	if (authTokenData && authTokenLength)
	{
		credssp_auth_take_input_buffer(auth, &buffer);
		return TRUE;
	}

	sspi_SecBufferFree(&buffer);
	return TRUE;
}

BOOL rpc_ncacn_http_auth_init(rdpContext* context, RpcChannel* channel)
{
	rdpTls* tls;
	rdpCredsspAuth* auth;
	rdpSettings* settings;
	freerdp* instance;
	auth_status rc;
	SEC_WINNT_AUTH_IDENTITY identity = { 0 };

	if (!context || !channel)
		return FALSE;

	tls = channel->tls;
	auth = channel->auth;
	settings = context->settings;
	instance = context->instance;

	if (!tls || !auth || !instance || !settings)
		return FALSE;

	rc = utils_authenticate_gateway(instance, GW_AUTH_HTTP);
	switch (rc)
	{
		case AUTH_SUCCESS:
		case AUTH_SKIP:
			break;
		case AUTH_NO_CREDENTIALS:
			freerdp_set_last_error_log(instance->context,
			                           FREERDP_ERROR_CONNECT_NO_OR_MISSING_CREDENTIALS);
			return TRUE;
		case AUTH_FAILED:
		default:
			return FALSE;
	}

	if (!credssp_auth_init(auth, AUTH_PKG, tls->Bindings))
		return TRUE;

	if (sspi_SetAuthIdentityA(&identity, settings->GatewayUsername, settings->GatewayDomain,
	                          settings->GatewayPassword) < 0)
		return TRUE;

	credssp_auth_setup_client(auth, "HTTP", settings->GatewayHostname, &identity, NULL);

	sspi_FreeAuthIdentity(&identity);

	credssp_auth_set_flags(auth, ISC_REQ_CONFIDENTIALITY);

	return TRUE;
}

void rpc_ncacn_http_auth_uninit(RpcChannel* channel)
{
	if (!channel)
		return;

	credssp_auth_free(channel->auth);
	channel->auth = NULL;
}

BOOL rpc_ncacn_http_send_out_channel_request(RpcChannel* outChannel, BOOL replacement)
{
	BOOL status = TRUE;
	wStream* s;
	int contentLength;
	rdpCredsspAuth* auth;
	HttpContext* http;
	const SecBuffer* buffer;
	int rc;

	if (!outChannel || !outChannel->auth || !outChannel->http)
		return FALSE;

	auth = outChannel->auth;
	http = outChannel->http;

	rc = credssp_auth_authenticate(auth);
	if (rc < 0)
		return FALSE;

	if (!replacement)
		contentLength = (rc == 0) ? 0 : 76;
	else
		contentLength = (rc == 0) ? 0 : 120;

	buffer = credssp_auth_have_output_token(auth) ? credssp_auth_get_output_buffer(auth) : NULL;
	s = rpc_auth_http_request(http, "RPC_OUT_DATA", contentLength, buffer,
	                          credssp_auth_pkg_name(auth));

	if (!s)
		return -1;

	if (rpc_channel_write(outChannel, Stream_Buffer(s), Stream_Length(s)) < 0)
		status = FALSE;

	Stream_Free(s, TRUE);
	return status;
}

BOOL rpc_ncacn_http_recv_out_channel_response(RpcChannel* outChannel, HttpResponse* response)
{
	const char* token64 = NULL;
	size_t authTokenLength = 0;
	BYTE* authTokenData = NULL;
	rdpCredsspAuth* auth;
	SecBuffer buffer = { 0 };

	if (!outChannel || !response || !outChannel->auth)
		return FALSE;

	auth = outChannel->auth;
	token64 = http_response_get_auth_token(response, credssp_auth_pkg_name(auth));

	if (token64)
		crypto_base64_decode(token64, strlen(token64), &authTokenData, &authTokenLength);

	buffer.pvBuffer = authTokenData;
	buffer.cbBuffer = authTokenLength;

	if (authTokenData && authTokenLength)
	{
		credssp_auth_take_input_buffer(auth, &buffer);
		return TRUE;
	}

	sspi_SecBufferFree(&buffer);
	return TRUE;
}

BOOL rpc_ncacn_http_is_final_request(RpcChannel* channel)
{
	WINPR_ASSERT(channel);
	return credssp_auth_is_complete(channel->auth);
}
