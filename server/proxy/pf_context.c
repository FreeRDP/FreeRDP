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

#include <winpr/crypto.h>
#include <winpr/print.h>

#include "pf_client.h"
#include "pf_context.h"

static wHashTable* create_channel_ids_map()
{
	wHashTable* table = HashTable_New(TRUE);
	if (!table)
		return NULL;

	table->hash = HashTable_StringHash;
	table->keyCompare = HashTable_StringCompare;
	table->keyClone = HashTable_StringClone;
	table->keyFree = HashTable_StringFree;
	return table;
}

/* Proxy context initialization callback */
static BOOL client_to_proxy_context_new(freerdp_peer* client, pServerContext* context)
{
	proxyServer* server = (proxyServer*)client->ContextExtra;
	proxyConfig* config = server->config;

	context->dynvcReady = NULL;

	context->vcm = WTSOpenServerA((LPSTR)client->context);

	if (!context->vcm || context->vcm == INVALID_HANDLE_VALUE)
		goto error;

	if (!(context->dynvcReady = CreateEvent(NULL, TRUE, FALSE, NULL)))
		goto error;

	context->vc_handles = (HANDLE*)calloc(config->PassthroughCount, sizeof(HANDLE));
	if (!context->vc_handles)
		goto error;

	context->vc_ids = create_channel_ids_map();
	if (!context->vc_ids)
		goto error;

	return TRUE;

error:
	WTSCloseServer((HANDLE)context->vcm);
	context->vcm = NULL;

	if (context->dynvcReady)
	{
		CloseHandle(context->dynvcReady);
		context->dynvcReady = NULL;
	}

	free(context->vc_handles);
	context->vc_handles = NULL;
	HashTable_Free(context->vc_ids);
	context->vc_ids = NULL;
	return FALSE;
}

/* Proxy context free callback */
static void client_to_proxy_context_free(freerdp_peer* client, pServerContext* context)
{
	proxyServer* server;

	if (!client || !context)
		return;

	server = (proxyServer*)client->ContextExtra;

	WTSCloseServer((HANDLE)context->vcm);

	if (context->dynvcReady)
	{
		CloseHandle(context->dynvcReady);
		context->dynvcReady = NULL;
	}

	HashTable_Free(context->vc_ids);
	free(context->vc_handles);
}

BOOL pf_context_init_server_context(freerdp_peer* client)
{
	client->ContextSize = sizeof(pServerContext);
	client->ContextNew = (psPeerContextNew)client_to_proxy_context_new;
	client->ContextFree = (psPeerContextFree)client_to_proxy_context_free;

	return freerdp_peer_context_new(client);
}

/*
 * pf_context_copy_settings copies settings from `src` to `dst`.
 * when using this function, is_dst_server must be set to TRUE if the destination
 * settings are server's settings. otherwise, they must be set to FALSE.
 */
BOOL pf_context_copy_settings(rdpSettings* dst, const rdpSettings* src)
{
	rdpSettings* before_copy = freerdp_settings_clone(dst);

	if (!before_copy)
		return FALSE;

	if (!freerdp_settings_copy(dst, src))
	{
		freerdp_settings_free(before_copy);
		return FALSE;
	}

	free(dst->ConfigPath);
	free(dst->PrivateKeyContent);
	free(dst->RdpKeyContent);
	free(dst->RdpKeyFile);
	free(dst->PrivateKeyFile);
	free(dst->CertificateFile);
	free(dst->CertificateName);
	free(dst->CertificateContent);

	/* adjust pointer to instance pointer */
	dst->ServerMode = before_copy->ServerMode;

	/* revert some values that must not be changed */
	dst->ConfigPath = _strdup(before_copy->ConfigPath);
	dst->PrivateKeyContent = _strdup(before_copy->PrivateKeyContent);
	dst->RdpKeyContent = _strdup(before_copy->RdpKeyContent);
	dst->RdpKeyFile = _strdup(before_copy->RdpKeyFile);
	dst->PrivateKeyFile = _strdup(before_copy->PrivateKeyFile);
	dst->CertificateFile = _strdup(before_copy->CertificateFile);
	dst->CertificateName = _strdup(before_copy->CertificateName);
	dst->CertificateContent = _strdup(before_copy->CertificateContent);

	if (!dst->ServerMode)
	{
		/* adjust instance pointer for client's context */
		dst->instance = before_copy->instance;

		/*
		 * RdpServerRsaKey must be set to NULL if `dst` is client's context
		 * it must be freed before setting it to NULL to avoid a memory leak!
		 */

		free(dst->RdpServerRsaKey->Modulus);
		free(dst->RdpServerRsaKey->PrivateExponent);
		free(dst->RdpServerRsaKey);
		dst->RdpServerRsaKey = NULL;
	}

	freerdp_settings_free(before_copy);
	return TRUE;
}

pClientContext* pf_context_create_client_context(rdpSettings* clientSettings)
{
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints;
	pClientContext* pc;
	rdpContext* context;
	RdpClientEntry(&clientEntryPoints);
	context = freerdp_client_context_new(&clientEntryPoints);

	if (!context)
		return NULL;

	pc = (pClientContext*)context;

	if (!pf_context_copy_settings(context->settings, clientSettings))
		goto error;

	pc->vc_ids = create_channel_ids_map();
	if (!pc->vc_ids)
		goto error;

	return pc;
error:
	freerdp_client_context_free(context);
	return NULL;
}

proxyData* proxy_data_new(void)
{
	BYTE temp[16];
	proxyData* pdata = calloc(1, sizeof(proxyData));

	if (pdata == NULL)
	{
		return NULL;
	}

	if (!(pdata->abort_event = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		proxy_data_free(pdata);
		return NULL;
	}

	if (!(pdata->gfx_server_ready = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		proxy_data_free(pdata);
		return NULL;
	}

	winpr_RAND((BYTE*)&temp, 16);
	if (!(pdata->session_id = winpr_BinToHexString(temp, 16, FALSE)))
	{
		proxy_data_free(pdata);
		return NULL;
	}

	if (!(pdata->modules_info = HashTable_New(FALSE)))
	{
		proxy_data_free(pdata);
		return NULL;
	}

	/* modules_info maps between plugin name to custom data */
	pdata->modules_info->hash = HashTable_StringHash;
	pdata->modules_info->keyCompare = HashTable_StringCompare;
	pdata->modules_info->keyClone = HashTable_StringClone;
	pdata->modules_info->keyFree = HashTable_StringFree;
	return pdata;
}

/* updates circular pointers between proxyData and pClientContext instances */
void proxy_data_set_client_context(proxyData* pdata, pClientContext* context)
{
	pdata->pc = context;
	context->pdata = pdata;
}

/* updates circular pointers between proxyData and pServerContext instances */
void proxy_data_set_server_context(proxyData* pdata, pServerContext* context)
{
	pdata->ps = context;
	context->pdata = pdata;
}

void proxy_data_free(proxyData* pdata)
{
	if (pdata->abort_event)
	{
		CloseHandle(pdata->abort_event);
		pdata->abort_event = NULL;
	}

	if (pdata->client_thread)
	{
		CloseHandle(pdata->client_thread);
		pdata->client_thread = NULL;
	}

	if (pdata->gfx_server_ready)
	{
		CloseHandle(pdata->gfx_server_ready);
		pdata->gfx_server_ready = NULL;
	}

	if (pdata->session_id)
		free(pdata->session_id);

	if (pdata->modules_info)
		HashTable_Free(pdata->modules_info);

	free(pdata);
}

void proxy_data_abort_connect(proxyData* pdata)
{
	SetEvent(pdata->abort_event);
}

BOOL proxy_data_shall_disconnect(proxyData* pdata)
{
	return WaitForSingleObject(pdata->abort_event, 0) == WAIT_OBJECT_0;
}
