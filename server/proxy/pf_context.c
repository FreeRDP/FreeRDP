/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
 * Copyright 2021 Armin Novak <anovak@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#include <freerdp/server/proxy/proxy_log.h>
#include <freerdp/server/proxy/proxy_server.h>

#include "pf_client.h"
#include "pf_utils.h"
#include <freerdp/server/proxy/proxy_context.h>

#include "channels/pf_channel_rdpdr.h"

#define TAG PROXY_TAG("server")

static UINT32 ChannelId_Hash(const void* key)
{
	const UINT32* v = (const UINT32*)key;
	return *v;
}

static BOOL ChannelId_Compare(const UINT32* v1, const UINT32* v2)
{
	return (*v1 == *v2);
}

pServerStaticChannelContext* StaticChannelContext_new(pServerContext* ps, const char* name,
                                                      UINT32 id)
{
	pServerStaticChannelContext* ret = calloc(1, sizeof(*ret));
	if (!ret)
	{
		PROXY_LOG_ERR(TAG, ps, "error allocating channel context for '%s'", name);
		return NULL;
	}

	ret->front_channel_id = id;
	ret->channel_name = _strdup(name);
	if (!ret->channel_name)
	{
		PROXY_LOG_ERR(TAG, ps, "error allocating name in channel context for '%s'", name);
		free(ret);
		return NULL;
	}

	ret->channelMode = pf_utils_get_channel_mode(ps->pdata->config, name);
	return ret;
}

void StaticChannelContext_free(pServerStaticChannelContext* ctx)
{
	if (!ctx)
		return;

	IFCALL(ctx->contextDtor, ctx->context);

	free(ctx->channel_name);
	free(ctx);
}

/* Proxy context initialization callback */
static void client_to_proxy_context_free(freerdp_peer* client, rdpContext* ctx);
static BOOL client_to_proxy_context_new(freerdp_peer* client, rdpContext* ctx)
{
	wObject* obj;
	pServerContext* context = (pServerContext*)ctx;

	WINPR_ASSERT(client);
	WINPR_ASSERT(context);

	context->dynvcReady = NULL;

	context->vcm = WTSOpenServerA((LPSTR)client->context);

	if (!context->vcm || context->vcm == INVALID_HANDLE_VALUE)
		goto error;

	if (!(context->dynvcReady = CreateEvent(NULL, TRUE, FALSE, NULL)))
		goto error;

	context->interceptContextMap = HashTable_New(FALSE);
	if (!context->interceptContextMap)
		goto error;
	if (!HashTable_SetupForStringData(context->interceptContextMap, FALSE))
		goto error;
	obj = HashTable_ValueObject(context->interceptContextMap);
	WINPR_ASSERT(obj);
	obj->fnObjectFree = intercept_context_entry_free;

	/* channels by ids */
	context->channelsByFrontId = HashTable_New(FALSE);
	if (!context->channelsByFrontId)
		goto error;
	if (!HashTable_SetHashFunction(context->channelsByFrontId, ChannelId_Hash))
		goto error;

	obj = HashTable_KeyObject(context->channelsByFrontId);
	obj->fnObjectEquals = (OBJECT_EQUALS_FN)ChannelId_Compare;

	obj = HashTable_ValueObject(context->channelsByFrontId);
	obj->fnObjectFree = (OBJECT_FREE_FN)StaticChannelContext_free;

	context->channelsByBackId = HashTable_New(FALSE);
	if (!context->channelsByBackId)
		goto error;
	if (!HashTable_SetHashFunction(context->channelsByBackId, ChannelId_Hash))
		goto error;

	obj = HashTable_KeyObject(context->channelsByBackId);
	obj->fnObjectEquals = (OBJECT_EQUALS_FN)ChannelId_Compare;

	return TRUE;

error:
	client_to_proxy_context_free(client, ctx);

	return FALSE;
}

/* Proxy context free callback */
void client_to_proxy_context_free(freerdp_peer* client, rdpContext* ctx)
{
	pServerContext* context = (pServerContext*)ctx;

	WINPR_UNUSED(client);

	if (!context)
		return;

	if (context->dynvcReady)
	{
		CloseHandle(context->dynvcReady);
		context->dynvcReady = NULL;
	}

	HashTable_Free(context->interceptContextMap);
	HashTable_Free(context->channelsByFrontId);
	HashTable_Free(context->channelsByBackId);

	if (context->vcm && (context->vcm != INVALID_HANDLE_VALUE))
		WTSCloseServer((HANDLE)context->vcm);
	context->vcm = NULL;
}

BOOL pf_context_init_server_context(freerdp_peer* client)
{
	WINPR_ASSERT(client);

	client->ContextSize = sizeof(pServerContext);
	client->ContextNew = client_to_proxy_context_new;
	client->ContextFree = client_to_proxy_context_free;

	return freerdp_peer_context_new(client);
}

static BOOL pf_context_revert_str_settings(rdpSettings* dst, const rdpSettings* before, size_t nr,
                                           const size_t* ids)
{
	size_t x;
	WINPR_ASSERT(dst);
	WINPR_ASSERT(before);
	WINPR_ASSERT(ids || (nr == 0));

	for (x = 0; x < nr; x++)
	{
		size_t id = ids[x];
		const char* what = freerdp_settings_get_string(before, id);
		if (!freerdp_settings_set_string(dst, id, what))
			return FALSE;
	}

	return TRUE;
}

void intercept_context_entry_free(void* obj)
{
	InterceptContextMapEntry* entry = obj;
	if (!entry)
		return;
	if (!entry->free)
		return;
	entry->free(entry);
}

BOOL pf_context_copy_settings(rdpSettings* dst, const rdpSettings* src)
{
	BOOL rc = FALSE;
	rdpSettings* before_copy;
	const size_t to_revert[] = { FreeRDP_ConfigPath,      FreeRDP_PrivateKeyContent,
		                         FreeRDP_PrivateKeyFile,  FreeRDP_CertificateFile,
		                         FreeRDP_CertificateName, FreeRDP_CertificateContent };

	if (!dst || !src)
		return FALSE;

	before_copy = freerdp_settings_clone(dst);
	if (!before_copy)
		return FALSE;

	if (!freerdp_settings_copy(dst, src))
	{
		freerdp_settings_free(before_copy);
		return FALSE;
	}

	/* keep original ServerMode value */
	dst->ServerMode = before_copy->ServerMode;

	/* revert some values that must not be changed */
	if (!pf_context_revert_str_settings(dst, before_copy, ARRAYSIZE(to_revert), to_revert))
		return FALSE;

	if (!dst->ServerMode)
	{
		/* adjust instance pointer */
		dst->instance = before_copy->instance;

		/*
		 * RdpServerRsaKey must be set to NULL if `dst` is client's context
		 * it must be freed before setting it to NULL to avoid a memory leak!
		 */

		if (!freerdp_settings_set_pointer_len(dst, FreeRDP_RdpServerRsaKey, NULL, 1))
			goto out_fail;
	}

	/* We handle certificate management for this client ourselfes. */
	rc = freerdp_settings_set_bool(dst, FreeRDP_ExternalCertificateManagement, TRUE);

out_fail:
	freerdp_settings_free(before_copy);
	return rc;
}

pClientContext* pf_context_create_client_context(const rdpSettings* clientSettings)
{
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints;
	pClientContext* pc;
	rdpContext* context;

	WINPR_ASSERT(clientSettings);

	RdpClientEntry(&clientEntryPoints);
	context = freerdp_client_context_new(&clientEntryPoints);

	if (!context)
		return NULL;

	pc = (pClientContext*)context;

	if (!pf_context_copy_settings(context->settings, clientSettings))
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
	if (!HashTable_SetupForStringData(pdata->modules_info, FALSE))
		goto error;

	return pdata;
error:
	proxy_data_free(pdata);
	return NULL;
}

/* updates circular pointers between proxyData and pClientContext instances */
void proxy_data_set_client_context(proxyData* pdata, pClientContext* context)
{
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(context);
	pdata->pc = context;
	context->pdata = pdata;
}

/* updates circular pointers between proxyData and pServerContext instances */
void proxy_data_set_server_context(proxyData* pdata, pServerContext* context)
{
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(context);
	pdata->ps = context;
	context->pdata = pdata;
}

void proxy_data_free(proxyData* pdata)
{
	if (!pdata)
		return;

	if (pdata->abort_event)
		CloseHandle(pdata->abort_event);

	if (pdata->client_thread)
		CloseHandle(pdata->client_thread);

	if (pdata->gfx_server_ready)
		CloseHandle(pdata->gfx_server_ready);

	if (pdata->modules_info)
		HashTable_Free(pdata->modules_info);

	if (pdata->pc)
		freerdp_client_context_free(&pdata->pc->context);

	free(pdata);
}

void proxy_data_abort_connect(proxyData* pdata)
{
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(pdata->abort_event);
	SetEvent(pdata->abort_event);
	if (pdata->pc)
		freerdp_abort_connect_context(&pdata->pc->context);
}

BOOL proxy_data_shall_disconnect(proxyData* pdata)
{
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(pdata->abort_event);
	return WaitForSingleObject(pdata->abort_event, 0) == WAIT_OBJECT_0;
}
