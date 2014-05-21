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

#include <openssl/rand.h>

wStream* rpc_ntlm_http_request(rdpRpc* rpc, SecBuffer* ntlm_token, int content_length, TSG_CHANNEL channel)
{
	wStream* s;
	char* base64_ntlm_token;
	HttpContext* http_context;
	HttpRequest* http_request;

	http_request = http_request_new();
	base64_ntlm_token = crypto_base64_encode(ntlm_token->pvBuffer, ntlm_token->cbBuffer);

	if (channel == TSG_CHANNEL_IN)
	{
		http_context = rpc->NtlmHttpIn->context;
		http_request_set_method(http_request, "RPC_IN_DATA");
	}
	else if (channel == TSG_CHANNEL_OUT)
	{
		http_context = rpc->NtlmHttpOut->context;
		http_request_set_method(http_request, "RPC_OUT_DATA");
	}
	else
	{
		return NULL;
	}

	http_request->ContentLength = content_length;
	http_request_set_uri(http_request, http_context->URI);

	http_request_set_auth_scheme(http_request, "NTLM");
	http_request_set_auth_param(http_request, base64_ntlm_token);

	s = http_request_write(http_context, http_request);
	http_request_free(http_request);

	free(base64_ntlm_token);

	return s;
}

int rpc_ncacn_http_send_in_channel_request(rdpRpc* rpc)
{
	wStream* s;
	int content_length;
	BOOL continue_needed;
	rdpNtlm* ntlm = rpc->NtlmHttpIn->ntlm;

	continue_needed = ntlm_authenticate(ntlm);

	content_length = (continue_needed) ? 0 : 0x40000000;

	s = rpc_ntlm_http_request(rpc, &ntlm->outputBuffer[0], content_length, TSG_CHANNEL_IN);

	DEBUG_RPC("\n%s", Stream_Buffer(s));
	rpc_in_write(rpc, Stream_Buffer(s), Stream_Length(s));
	Stream_Free(s, TRUE);

	return 0;
}

int rpc_ncacn_http_recv_in_channel_response(rdpRpc* rpc)
{
	int ntlm_token_length = 0;
	BYTE* ntlm_token_data = NULL;
	HttpResponse* http_response;
	rdpNtlm* ntlm = rpc->NtlmHttpIn->ntlm;

	http_response = http_response_recv(rpc->TlsIn);

	if (ListDictionary_Contains(http_response->Authenticates, "NTLM"))
	{
		char *token64 = ListDictionary_GetItemValue(http_response->Authenticates, "NTLM");
		if (!token64)
			goto out;

		ntlm_token_data = NULL;
		crypto_base64_decode(token64, strlen(token64), &ntlm_token_data, &ntlm_token_length);
	}

	ntlm->inputBuffer[0].pvBuffer = ntlm_token_data;
	ntlm->inputBuffer[0].cbBuffer = ntlm_token_length;

out:
	http_response_free(http_response);

	return 0;
}

int rpc_ncacn_http_ntlm_init(rdpRpc* rpc, TSG_CHANNEL channel)
{
	rdpNtlm* ntlm = NULL;
	rdpSettings* settings = rpc->settings;
	freerdp* instance = (freerdp*) rpc->settings->instance;
	BOOL promptPassword = FALSE;

	if (channel == TSG_CHANNEL_IN)
		ntlm = rpc->NtlmHttpIn->ntlm;
	else if (channel == TSG_CHANNEL_OUT)
		ntlm = rpc->NtlmHttpOut->ntlm;

	if ((!settings->GatewayPassword) || (!settings->GatewayUsername)
			|| (!strlen(settings->GatewayPassword)) || (!strlen(settings->GatewayUsername)))
	{
		promptPassword = TRUE;
	}

	if (promptPassword)
	{
		if (instance->GatewayAuthenticate)
		{
			BOOL proceed = instance->GatewayAuthenticate(instance,
					&settings->GatewayUsername, &settings->GatewayPassword, &settings->GatewayDomain);

			if (!proceed)
			{
				connectErrorCode = CANCELEDBYUSER;
				freerdp_set_last_error(instance->context, FREERDP_ERROR_CONNECT_CANCELLED);
				return 0;
			}

			if (settings->GatewayUseSameCredentials)
			{
				settings->Username = _strdup(settings->GatewayUsername);
				settings->Domain = _strdup(settings->GatewayDomain);
				settings->Password = _strdup(settings->GatewayPassword);
			}
		}
	}

	if (!ntlm_client_init(ntlm, TRUE, settings->GatewayUsername,
			settings->GatewayDomain, settings->GatewayPassword,
			rpc->TlsIn->Bindings))
	{
		return 0;
	}

	if (!ntlm_client_make_spn(ntlm, _T("HTTP"), settings->GatewayHostname))
	{
		return 0;
	}

	return 1;
}

BOOL rpc_ntlm_http_in_connect(rdpRpc* rpc)
{
	rdpNtlm* ntlm = rpc->NtlmHttpIn->ntlm;
	BOOL success = FALSE;

	if (rpc_ncacn_http_ntlm_init(rpc, TSG_CHANNEL_IN) == 1)
	{
		success = TRUE;

		/* Send IN Channel Request */

		rpc_ncacn_http_send_in_channel_request(rpc);

		/* Receive IN Channel Response */

		rpc_ncacn_http_recv_in_channel_response(rpc);

		/* Send IN Channel Request */

		rpc_ncacn_http_send_in_channel_request(rpc);

		ntlm_client_uninit(ntlm);
	}

	ntlm_free(ntlm);

	rpc->NtlmHttpIn->ntlm = NULL;

	return success;
}

int rpc_ncacn_http_send_out_channel_request(rdpRpc* rpc)
{
	wStream* s;
	int content_length;
	BOOL continue_needed;
	rdpNtlm* ntlm = rpc->NtlmHttpOut->ntlm;

	continue_needed = ntlm_authenticate(ntlm);

	content_length = (continue_needed) ? 0 : 76;

	s = rpc_ntlm_http_request(rpc, &ntlm->outputBuffer[0], content_length, TSG_CHANNEL_OUT);

	DEBUG_RPC("\n%s", Stream_Buffer(s));
	rpc_out_write(rpc, Stream_Buffer(s), Stream_Length(s));
	Stream_Free(s, TRUE);

	return 0;
}

int rpc_ncacn_http_recv_out_channel_response(rdpRpc* rpc)
{
	int ntlm_token_length = 0;
	BYTE* ntlm_token_data;
	HttpResponse* http_response;
	rdpNtlm* ntlm = rpc->NtlmHttpOut->ntlm;

	http_response = http_response_recv(rpc->TlsOut);

	ntlm_token_data = NULL;
	if (http_response && ListDictionary_Contains(http_response->Authenticates, "NTLM"))
	{
		char *token64 = ListDictionary_GetItemValue(http_response->Authenticates, "NTLM");
		crypto_base64_decode(token64, strlen(token64), &ntlm_token_data, &ntlm_token_length);
	}

	ntlm->inputBuffer[0].pvBuffer = ntlm_token_data;
	ntlm->inputBuffer[0].cbBuffer = ntlm_token_length;

	http_response_free(http_response);

	return 0;
}

BOOL rpc_ntlm_http_out_connect(rdpRpc* rpc)
{
	rdpNtlm* ntlm = rpc->NtlmHttpOut->ntlm;
	BOOL success = FALSE;

	if (rpc_ncacn_http_ntlm_init(rpc, TSG_CHANNEL_OUT) == 1)
	{
		success = TRUE;

		/* Send OUT Channel Request */

		rpc_ncacn_http_send_out_channel_request(rpc);

		/* Receive OUT Channel Response */

		rpc_ncacn_http_recv_out_channel_response(rpc);

		/* Send OUT Channel Request */

		rpc_ncacn_http_send_out_channel_request(rpc);

		ntlm_client_uninit(ntlm);
	}

	ntlm_free(ntlm);

	rpc->NtlmHttpOut->ntlm = NULL;

	return success;
}

void rpc_ntlm_http_init_channel(rdpRpc* rpc, rdpNtlmHttp* ntlm_http, TSG_CHANNEL channel)
{
	if (channel == TSG_CHANNEL_IN)
		http_context_set_method(ntlm_http->context, "RPC_IN_DATA");
	else if (channel == TSG_CHANNEL_OUT)
		http_context_set_method(ntlm_http->context, "RPC_OUT_DATA");

	http_context_set_uri(ntlm_http->context, "/rpc/rpcproxy.dll?localhost:3388");
	http_context_set_accept(ntlm_http->context, "application/rpc");
	http_context_set_cache_control(ntlm_http->context, "no-cache");
	http_context_set_connection(ntlm_http->context, "Keep-Alive");
	http_context_set_user_agent(ntlm_http->context, "MSRPC");
	http_context_set_host(ntlm_http->context, rpc->settings->GatewayHostname);

	if (channel == TSG_CHANNEL_IN)
	{
		http_context_set_pragma(ntlm_http->context,
			"ResourceTypeUuid=44e265dd-7daf-42cd-8560-3cdb6e7a2729");
	}
	else if (channel == TSG_CHANNEL_OUT)
	{
		http_context_set_pragma(ntlm_http->context,
				"ResourceTypeUuid=44e265dd-7daf-42cd-8560-3cdb6e7a2729" ", "
				"SessionId=fbd9c34f-397d-471d-a109-1b08cc554624");
	}
}

rdpNtlmHttp* ntlm_http_new()
{
	rdpNtlmHttp* ntlm_http;

	ntlm_http = (rdpNtlmHttp *)calloc(1, sizeof(rdpNtlmHttp));

	if (!ntlm_http)
		return NULL;

	ntlm_http->ntlm = ntlm_new();
	if (!ntlm_http->ntlm)
		goto out_free;

	ntlm_http->context = http_context_new();
	if (!ntlm_http->context)
		goto out_free_ntlm;

	return ntlm_http;

out_free_ntlm:
	ntlm_free(ntlm_http->ntlm);
out_free:
	free(ntlm_http);
	return NULL;
}

void ntlm_http_free(rdpNtlmHttp* ntlm_http)
{
	if (!ntlm_http)
		return;

	ntlm_free(ntlm_http->ntlm);
	http_context_free(ntlm_http->context);

	free(ntlm_http);
}
