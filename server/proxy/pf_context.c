/**
* FreeRDP: A Remote Desktop Protocol Implementation
* FreeRDP Proxy Server
*
* Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
* Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
* Copyright 2019 Idan Freiberg <speidy@gmail.com>
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

#include "pf_client.h"
#include "pf_context.h"
#include "pf_common.h"

/* Proxy context initialization callback */
static BOOL client_to_proxy_context_new(freerdp_peer* client,
                                        pServerContext* context)
{
	context->vcm = WTSOpenServerA((LPSTR) client->context);

	if (!context->vcm || context->vcm == INVALID_HANDLE_VALUE)
		goto fail_open_server;

	return TRUE;
fail_open_server:
	context->vcm = NULL;
	return FALSE;
}

/* Proxy context free callback */
static void client_to_proxy_context_free(freerdp_peer* client,
        pServerContext* context)
{
	WINPR_UNUSED(client);

	if (context)
		WTSCloseServer((HANDLE) context->vcm);
}

BOOL init_p_server_context(freerdp_peer* client)
{
	client->ContextSize = sizeof(pServerContext);
	client->ContextNew = (psPeerContextNew) client_to_proxy_context_new;
	client->ContextFree = (psPeerContextFree) client_to_proxy_context_free;
	return freerdp_peer_context_new(client);
}

rdpContext* p_client_context_create(rdpSettings* clientSettings,
                                    char* host, DWORD port)
{
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints;
	rdpContext* context;
	rdpSettings* settings;
	RdpClientEntry(&clientEntryPoints);
	context = freerdp_client_context_new(&clientEntryPoints);

	if (!context)
		return NULL;

	settings = context->settings;
	pf_common_copy_settings(settings, clientSettings);
	settings->Username = _strdup(clientSettings->Username);
	settings->Password = _strdup(clientSettings->Password);
	settings->Domain = _strdup(clientSettings->Domain);
	settings->ServerHostname = host;
	settings->ServerPort = port;
	settings->SoftwareGdi = FALSE;
	settings->RedirectClipboard = FALSE;
	return context;
}
