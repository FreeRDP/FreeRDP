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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ncacn_http.h"

#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/stream.h>
#include <winpr/dsparse.h>
#include <winpr/winhttp.h>

#define TAG FREERDP_TAG("core.gateway.ntlm")

static wStream* rpc_ntlm_http_request(HttpContext* http, const char* method, int contentLength,
                                      const SecBuffer* ntlmToken)
{
	wStream* s = NULL;
	HttpRequest* request = NULL;
	char* base64NtlmToken = NULL;
	const char* uri;

	if (!http || !method || !ntlmToken)
		goto fail;

	request = http_request_new();

	if (!request)
		goto fail;

	if (ntlmToken)
		base64NtlmToken = crypto_base64_encode(ntlmToken->pvBuffer, ntlmToken->cbBuffer);

	uri = http_context_get_uri(http);

	if (!http_request_set_method(request, method) ||
	    !http_request_set_content_length(request, contentLength) ||
	    !http_request_set_uri(request, uri))
		goto fail;

	if (base64NtlmToken)
	{
		if (!http_request_set_auth_scheme(request, "NTLM") ||
		    !http_request_set_auth_param(request, base64NtlmToken))
			goto fail;
	}

	s = http_request_write(http, request);
fail:
	http_request_free(request);
	free(base64NtlmToken);
	return s;
}

BOOL rpc_ncacn_http_send_in_channel_request(RpcChannel* inChannel)
{
	wStream* s;
	int status;
	int contentLength;
	BOOL continueNeeded = FALSE;
	rdpNtlm* ntlm;
	HttpContext* http;
	const SecBuffer* buffer;

	if (!inChannel || !inChannel->ntlm || !inChannel->http)
		return FALSE;

	ntlm = inChannel->ntlm;
	http = inChannel->http;

	if (!ntlm_authenticate(ntlm, &continueNeeded))
		return FALSE;

	contentLength = (continueNeeded) ? 0 : 0x40000000;
	buffer = ntlm_client_get_output_buffer(ntlm);
	s = rpc_ntlm_http_request(http, "RPC_IN_DATA", contentLength, buffer);

	if (!s)
		return -1;

	status = rpc_channel_write(inChannel, Stream_Buffer(s), Stream_Length(s));
	Stream_Free(s, TRUE);
	return (status > 0) ? 1 : -1;
}

BOOL rpc_ncacn_http_recv_in_channel_response(RpcChannel* inChannel, HttpResponse* response)
{
	const char* token64 = NULL;
	int ntlmTokenLength = 0;
	BYTE* ntlmTokenData = NULL;
	rdpNtlm* ntlm;

	if (!inChannel || !response || !inChannel->ntlm)
		return FALSE;

	ntlm = inChannel->ntlm;
	token64 = http_response_get_auth_token(response, "NTLM");

	if (token64)
		crypto_base64_decode(token64, strlen(token64), &ntlmTokenData, &ntlmTokenLength);

	if (ntlmTokenData && ntlmTokenLength)
		return ntlm_client_set_input_buffer(ntlm, FALSE, ntlmTokenData, ntlmTokenLength);

	return TRUE;
}

BOOL rpc_ncacn_http_ntlm_init(rdpContext* context, RpcChannel* channel)
{
	rdpTls* tls;
	rdpNtlm* ntlm;
	rdpSettings* settings;
	freerdp* instance;

	if (!context || !channel)
		return FALSE;

	tls = channel->tls;
	ntlm = channel->ntlm;
	settings = context->settings;
	instance = context->instance;

	if (!tls || !ntlm || !instance || !settings)
		return FALSE;

	if (!settings->GatewayPassword || !settings->GatewayUsername ||
	    !strlen(settings->GatewayPassword) || !strlen(settings->GatewayUsername))
	{
		if (freerdp_shall_disconnect(instance))
			return FALSE;

		if (!instance->GatewayAuthenticate)
		{
			freerdp_set_last_error_log(context, FREERDP_ERROR_CONNECT_NO_OR_MISSING_CREDENTIALS);
			return TRUE;
		}
		else
		{
			BOOL proceed =
			    instance->GatewayAuthenticate(instance, &settings->GatewayUsername,
			                                  &settings->GatewayPassword, &settings->GatewayDomain);

			if (!proceed)
			{
				freerdp_set_last_error_log(context,
				                           FREERDP_ERROR_CONNECT_NO_OR_MISSING_CREDENTIALS);
				return TRUE;
			}

			if (settings->GatewayUseSameCredentials)
			{
				if (settings->GatewayUsername)
				{
					free(settings->Username);

					if (!(settings->Username = _strdup(settings->GatewayUsername)))
						return FALSE;
				}

				if (settings->GatewayDomain)
				{
					free(settings->Domain);

					if (!(settings->Domain = _strdup(settings->GatewayDomain)))
						return FALSE;
				}

				if (settings->GatewayPassword)
				{
					free(settings->Password);

					if (!(settings->Password = _strdup(settings->GatewayPassword)))
						return FALSE;
				}
			}
		}
	}

	if (!ntlm_client_init(ntlm, TRUE, settings->GatewayUsername, settings->GatewayDomain,
	                      settings->GatewayPassword, tls->Bindings))
	{
		return TRUE;
	}

	if (!ntlm_client_make_spn(ntlm, _T("HTTP"), settings->GatewayHostname))
	{
		return TRUE;
	}

	return TRUE;
}

void rpc_ncacn_http_ntlm_uninit(RpcChannel* channel)
{
	if (!channel)
		return;

	ntlm_free(channel->ntlm);
	channel->ntlm = NULL;
}

BOOL rpc_ncacn_http_send_out_channel_request(RpcChannel* outChannel, BOOL replacement)
{
	BOOL rc = TRUE;
	wStream* s;
	int contentLength;
	BOOL continueNeeded = FALSE;
	rdpNtlm* ntlm;
	HttpContext* http;
	const SecBuffer* buffer;

	if (!outChannel || !outChannel->ntlm || !outChannel->http)
		return FALSE;

	ntlm = outChannel->ntlm;
	http = outChannel->http;

	if (!ntlm_authenticate(ntlm, &continueNeeded))
		return FALSE;

	if (!replacement)
		contentLength = (continueNeeded) ? 0 : 76;
	else
		contentLength = (continueNeeded) ? 0 : 120;

	buffer = ntlm_client_get_output_buffer(ntlm);
	s = rpc_ntlm_http_request(http, "RPC_OUT_DATA", contentLength, buffer);

	if (!s)
		return -1;

	if (rpc_channel_write(outChannel, Stream_Buffer(s), Stream_Length(s)) < 0)
		rc = FALSE;

	Stream_Free(s, TRUE);
	return rc;
}

BOOL rpc_ncacn_http_recv_out_channel_response(RpcChannel* outChannel, HttpResponse* response)
{
	const char* token64 = NULL;
	int ntlmTokenLength = 0;
	BYTE* ntlmTokenData = NULL;
	rdpNtlm* ntlm;

	if (!outChannel || !response || !outChannel->ntlm)
		return FALSE;

	ntlm = outChannel->ntlm;
	token64 = http_response_get_auth_token(response, "NTLM");

	if (token64)
		crypto_base64_decode(token64, strlen(token64), &ntlmTokenData, &ntlmTokenLength);

	if (ntlmTokenData && ntlmTokenLength)
		return ntlm_client_set_input_buffer(ntlm, FALSE, ntlmTokenData, ntlmTokenLength);

	return TRUE;
}
