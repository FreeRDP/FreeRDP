/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2015 Jiang Zihao <zihao.jiang@yahoo.com>
 * Copyright 2023 Pascal Nowack <Pascal.Nowack@gmx.de>
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

#include <freerdp/log.h>
#include "shadow.h"

#include "shadow_audin.h"
#include <freerdp/server/server-common.h>

#if defined(CHANNEL_AUDIN_SERVER)
#include <freerdp/server/audin.h>
#endif

#if defined(CHANNEL_AUDIN_SERVER)

static UINT AudinServerData(audin_server_context* audin, const SNDIN_DATA* data)
{
	rdpShadowClient* client = NULL;
	rdpShadowSubsystem* subsystem = NULL;

	WINPR_ASSERT(audin);
	WINPR_ASSERT(data);

	client = audin->userdata;
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->server);
	subsystem = client->server->subsystem;
	WINPR_ASSERT(subsystem);

	if (!client->mayInteract)
		return CHANNEL_RC_OK;

	if (!IFCALLRESULT(TRUE, subsystem->AudinServerReceiveSamples, subsystem, client,
	                  audin_server_get_negotiated_format(client->audin), data->Data))
		return ERROR_INTERNAL_ERROR;

	return CHANNEL_RC_OK;
}

#endif

BOOL shadow_client_audin_init(rdpShadowClient* client)
{
	WINPR_ASSERT(client);

#if defined(CHANNEL_AUDIN_SERVER)
	audin_server_context* audin = client->audin = audin_server_context_new(client->vcm);

	if (!audin)
		return FALSE;

	audin->userdata = client;

	audin->Data = AudinServerData;

	if (client->subsystem->audinFormats)
	{
		if (client->subsystem->nAudinFormats > SSIZE_MAX)
			goto fail;

		if (!audin_server_set_formats(client->audin, (SSIZE_T)client->subsystem->nAudinFormats,
		                              client->subsystem->audinFormats))
			goto fail;
	}
	else
	{
		if (!audin_server_set_formats(client->audin, -1, NULL))
			goto fail;
	}

	return TRUE;
fail:
	audin_server_context_free(audin);
	client->audin = NULL;
#endif
	return FALSE;
}

void shadow_client_audin_uninit(rdpShadowClient* client)
{
	WINPR_ASSERT(client);

#if defined(CHANNEL_AUDIN_SERVER)
	audin_server_context_free(client->audin);
	client->audin = NULL;
#endif
}
