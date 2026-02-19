/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2015 Jiang Zihao <zihao.jiang@yahoo.com>
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

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/cast.h>

#include <freerdp/log.h>
#include <freerdp/codec/dsp.h>
#include <freerdp/server/server-common.h>

#include "shadow.h"

#include "shadow_rdpsnd.h"

#define TAG SERVER_TAG("shadow")

static void rdpsnd_activated(RdpsndServerContext* context)
{
	WINPR_ASSERT(context);
	for (size_t i = 0; i < context->num_client_formats; i++)
	{
		for (size_t j = 0; j < context->num_server_formats; j++)
		{
			if (audio_format_compatible(&context->server_formats[j], &context->client_formats[i]))
			{
				const UINT rc = context->SelectFormat(context, WINPR_ASSERTING_INT_CAST(UINT16, i));
				if (rc != CHANNEL_RC_OK)
					WLog_WARN(TAG, "SelectFormat failed with %" PRIu32, rc);
				return;
			}
		}
	}

	WLog_ERR(TAG, "Could not agree on a audio format with the server\n");
}

int shadow_client_rdpsnd_init(rdpShadowClient* client)
{
	WINPR_ASSERT(client);
	RdpsndServerContext* rdpsnd = client->rdpsnd = rdpsnd_server_context_new(client->vcm);

	if (!rdpsnd)
	{
		return 0;
	}

	rdpsnd->data = client;

	if (client->subsystem->rdpsndFormats)
	{
		rdpsnd->server_formats = client->subsystem->rdpsndFormats;
		rdpsnd->num_server_formats = client->subsystem->nRdpsndFormats;
	}
	else
	{
		rdpsnd->num_server_formats = server_rdpsnd_get_formats(&rdpsnd->server_formats);
	}

	if (rdpsnd->num_server_formats > 0)
		rdpsnd->src_format = &rdpsnd->server_formats[0];

	rdpsnd->Activated = rdpsnd_activated;

	const UINT error = rdpsnd->Initialize(rdpsnd, TRUE);
	if (error != CHANNEL_RC_OK)
		return -1;
	return 1;
}

void shadow_client_rdpsnd_uninit(rdpShadowClient* client)
{
	WINPR_ASSERT(client);
	if (client->rdpsnd)
	{
		client->rdpsnd->Stop(client->rdpsnd);
		rdpsnd_server_context_free(client->rdpsnd);
		client->rdpsnd = NULL;
	}
}
