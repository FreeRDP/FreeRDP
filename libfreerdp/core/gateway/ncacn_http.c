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

static wStream* rpc_ntlm_http_request(HttpContext* http, const char* method,
                                      size_t contentLength, const SecBuffer* ntlmToken)
{
	wStream* s = NULL;
	HttpRequest* request;
	char* base64NtlmToken = NULL;
	request = http_request_new();

	if (ntlmToken)
		base64NtlmToken = crypto_base64_encode(ntlmToken->pvBuffer, ntlmToken->cbBuffer);

	if (base64NtlmToken)
	{
		BOOL rc = http_request_set_auth_param(request, base64NtlmToken);
		free(base64NtlmToken);

		if (!rc || !http_request_set_auth_scheme(request, "NTLM"))
			goto fail;
	}

	{
		const char* uri = http_context_get_uri(http);

		if (!http_request_set_method(request, method) ||
		    !http_request_set_content_length(request, contentLength) ||
		    !http_request_set_uri(request, uri))
			goto fail;
	}

	s = http_request_write(http, request);
fail:
	http_request_free(request);
	return s;
}

BOOL rpc_ncacn_http_send_in_channel_request(RpcInChannel* inChannel)
{
	wStream* s;
	int status;
	int contentLength;
	BOOL continueNeeded;
	rdpNtlm* ntlm;
	HttpContext* http;
	const SecBuffer* out;

	if (!inChannel)
		return FALSE;

	ntlm = inChannel->common.ntlm;
	http = inChannel->common.http;

	if (!ntlm || !http)
		return FALSE;

	out = ntlm_client_get_output_buffer(ntlm);

	if (!out)
		return FALSE;

	continueNeeded = ntlm_authenticate(ntlm);
	contentLength = (continueNeeded) ? 0 : 0x40000000;
	s = rpc_ntlm_http_request(http, "RPC_IN_DATA", contentLength, out);

	if (!s)
		return -1;

	status = rpc_in_channel_write(inChannel, Stream_Buffer(s), Stream_Length(s));
	Stream_Free(s, TRUE);
	return (status > 0) ? TRUE : FALSE;
}

BOOL rpc_ncacn_http_recv_in_channel_response(RpcInChannel* inChannel,
        HttpResponse* response)
{
	int ntlmTokenLength = 0;
	BYTE* ntlmTokenData = NULL;
	rdpNtlm* ntlm;
	const char* token64;

	if (!inChannel)
		return FALSE;

	ntlm = inChannel->common.ntlm;

	if (!ntlm)
		return FALSE;

	token64 = http_response_get_auth_token(response, "NTLM");

	if (!token64)
		return FALSE;

	crypto_base64_decode(token64, strlen(token64), &ntlmTokenData, &ntlmTokenLength);

	if (ntlmTokenData && ntlmTokenLength)
	{
		if (!ntlm_client_set_input_buffer(ntlm, FALSE, ntlmTokenData, ntlmTokenLength))
			return FALSE;
	}

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

	if (!tls || !ntlm || !settings || !instance)
		return FALSE;

	if (!settings->GatewayPassword || !settings->GatewayUsername ||
	    !strlen(settings->GatewayPassword) || !strlen(settings->GatewayUsername))
	{
		if (instance->GatewayAuthenticate)
		{
			BOOL proceed = instance->GatewayAuthenticate(instance, &settings->GatewayUsername,
			               &settings->GatewayPassword, &settings->GatewayDomain);

			if (!proceed)
			{
				freerdp_set_last_error(context, FREERDP_ERROR_CONNECT_CANCELLED);
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

	if (!ntlm_client_init(ntlm, TRUE, settings->GatewayUsername,
	                      settings->GatewayDomain, settings->GatewayPassword, tls->Bindings))
	{
		return FALSE;
	}

	if (!ntlm_client_make_spn(ntlm, _T("HTTP"), settings->GatewayHostname))
	{
		return FALSE;
	}

	return TRUE;
}

void rpc_ncacn_http_ntlm_uninit(RpcChannel* channel)
{
	if (channel)
	{
		ntlm_client_uninit(channel->ntlm);
		ntlm_free(channel->ntlm);
		channel->ntlm = NULL;
	}
}

BOOL rpc_ncacn_http_send_out_channel_request(RpcOutChannel* outChannel,
        BOOL replacement)
{
	wStream* s;
	int status;
	int contentLength;
	BOOL continueNeeded;
	rdpNtlm* ntlm;
	HttpContext* http;

	if (!outChannel)
		return FALSE;

	ntlm = outChannel->common.ntlm;
	http = outChannel->common.http;

	if (!ntlm || !http)
		return FALSE;

	continueNeeded = ntlm_authenticate(ntlm);

	if (!replacement)
		contentLength = (continueNeeded) ? 0 : 76;
	else
		contentLength = (continueNeeded) ? 0 : 120;

	{
		const SecBuffer* out = ntlm_client_get_output_buffer(ntlm);

		if (!out)
			return FALSE;

		s = rpc_ntlm_http_request(http, "RPC_OUT_DATA", contentLength, out);

		if (!s)
			return FALSE;
	}
	status = rpc_out_channel_write(outChannel, Stream_Buffer(s), Stream_Length(s));
	Stream_Free(s, TRUE);
	return (status > 0) ? TRUE : FALSE;
}

BOOL rpc_ncacn_http_recv_out_channel_response(RpcOutChannel* outChannel,
        HttpResponse* response)
{
	int ntlmTokenLength = 0;
	BYTE* ntlmTokenData = NULL;
	rdpNtlm* ntlm;
	const char* token64;

	if (!outChannel)
		return FALSE;

	ntlm = outChannel->common.ntlm;

	if (!ntlm)
		return FALSE;

	token64 = http_response_get_auth_token(response, "NTLM");

	if (!token64)
		return -1;

	crypto_base64_decode(token64, strlen(token64), &ntlmTokenData, &ntlmTokenLength);

	if (ntlmTokenData && ntlmTokenLength)
	{
		if (!ntlm_client_set_input_buffer(ntlm, FALSE, ntlmTokenData, ntlmTokenLength))
			return FALSE;
	}

	return TRUE;
}
