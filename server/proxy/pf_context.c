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

/* Proxy context initialization callback */
static BOOL client_to_proxy_context_new(freerdp_peer* client, pServerContext* context)
{
	context->dynvcReady = NULL;
	context->modules_info = NULL;

	context->modules_info = HashTable_New(TRUE);
	if (!context->modules_info)
		return FALSE;

	context->vcm = WTSOpenServerA((LPSTR)client->context);

	if (!context->vcm || context->vcm == INVALID_HANDLE_VALUE)
		goto error;

	if (!(context->dynvcReady = CreateEvent(NULL, TRUE, FALSE, NULL)))
		goto error;

	return TRUE;

error:
	HashTable_Free(context->modules_info);
	WTSCloseServer((HANDLE)context->vcm);
	context->vcm = NULL;

	if (context->dynvcReady)
	{
		CloseHandle(context->dynvcReady);
		context->dynvcReady = NULL;
	}

	return FALSE;
}

/* Proxy context free callback */
static void client_to_proxy_context_free(freerdp_peer* client, pServerContext* context)
{
	WINPR_UNUSED(client);

	if (!context)
		return;

	WTSCloseServer((HANDLE)context->vcm);

	if (context->dynvcReady)
	{
		CloseHandle(context->dynvcReady);
		context->dynvcReady = NULL;
	}

	HashTable_Free(context->modules_info);
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

		/* RdpServerRsaKey must be set to NULL if `dst` is client's context */
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

	return pc;
error:
	freerdp_client_context_free(context);
	return NULL;
}

proxyData* proxy_data_new(void)
{
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

	return pdata;
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
