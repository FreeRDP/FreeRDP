/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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
BOOL client_to_proxy_context_new(freerdp_peer* client, clientToProxyContext* context)
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
void client_to_proxy_context_free(freerdp_peer* client, clientToProxyContext* context)
{
	if (context)
	{
		WTSCloseServer((HANDLE) context->vcm);
	}
}

rdpContext* proxy_to_server_context_create(rdpSettings* baseSettings, char* host,
        DWORD port, char* username, char* password)
{
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints;
	rdpContext* context;
	rdpSettings* settings;
	RdpClientEntry(&clientEntryPoints);
	context = freerdp_client_context_new(&clientEntryPoints);

	if (!context)
		return NULL;

	settings = context->settings;
	proxy_settings_mirror(settings, baseSettings);
	settings->Username = _strdup(baseSettings->Username);
	settings->Password = _strdup(baseSettings->Password);
	settings->ServerHostname = host;
	settings->ServerPort = port;
	settings->Username = username;
	settings->Password = password;
	settings->SoftwareGdi = FALSE;
	settings->RedirectClipboard = FALSE;
	return context;
}