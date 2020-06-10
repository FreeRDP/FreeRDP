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

BOOL pf_context_copy_settings(rdpSettings* dst, const rdpSettings* src)
{
	BOOL rc = FALSE;
	rdpSettings* before_copy;

	if (!dst || !src)
		return FALSE;

	before_copy = freerdp_settings_clone(dst);
	if (!before_copy)
		return FALSE;

#define REVERT_STR_VALUE(name)                                          \
	free(dst->name);                                                    \
	dst->name = NULL;                                                   \
	if (before_copy->name && !(dst->name = _strdup(before_copy->name))) \
	goto out_fail

	if (!freerdp_settings_copy(dst, src))
	{
		freerdp_settings_free(before_copy);
		return FALSE;
	}

	/* keep original ServerMode value */
	dst->ServerMode = before_copy->ServerMode;

	/* revert some values that must not be changed */
	REVERT_STR_VALUE(ConfigPath);
	REVERT_STR_VALUE(PrivateKeyContent);
	REVERT_STR_VALUE(RdpKeyContent);
	REVERT_STR_VALUE(RdpKeyFile);
	REVERT_STR_VALUE(PrivateKeyFile);
	REVERT_STR_VALUE(CertificateFile);
	REVERT_STR_VALUE(CertificateName);
	REVERT_STR_VALUE(CertificateContent);

	if (!dst->ServerMode)
	{
		/* adjust instance pointer */
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

	rc = TRUE;

out_fail:
	freerdp_settings_free(before_copy);
	return rc;
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
	char* hex;
	proxyData* pdata;

	pdata = calloc(1, sizeof(proxyData));
	if (!pdata)
		return NULL;

	if (!(pdata->abort_event = CreateEvent(NULL, TRUE, FALSE, NULL)))
		goto error;

	if (!(pdata->gfx_server_ready = CreateEvent(NULL, TRUE, FALSE, NULL)))
		goto error;

	winpr_RAND((BYTE*)&temp, 16);
	hex = winpr_BinToHexString(temp, 16, FALSE);
	if (!hex)
		goto error;

	CopyMemory(pdata->session_id, hex, PROXY_SESSION_ID_LENGTH);
	pdata->session_id[PROXY_SESSION_ID_LENGTH] = '\0';
	free(hex);

	if (!(pdata->modules_info = HashTable_New(FALSE)))
		goto error;

	/* modules_info maps between plugin name to custom data */
	pdata->modules_info->hash = HashTable_StringHash;
	pdata->modules_info->keyCompare = HashTable_StringCompare;
	pdata->modules_info->keyClone = HashTable_StringClone;
	pdata->modules_info->keyFree = HashTable_StringFree;

	return pdata;
error:
	proxy_data_free(pdata);
	return NULL;
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
