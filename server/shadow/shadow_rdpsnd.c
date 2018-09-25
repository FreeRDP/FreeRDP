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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <freerdp/log.h>
#include <freerdp/codec/dsp.h>
#include <freerdp/server/server-common.h>

#include "shadow.h"

#include "shadow_rdpsnd.h"

#define TAG SERVER_TAG("shadow")

static void rdpsnd_activated(RdpsndServerContext* context)
{
	const AUDIO_FORMAT* agreed_format = NULL;
	UINT16 i = 0, j = 0;

	for (i = 0; i < context->num_client_formats; i++)
	{
		for (j = 0; j < context->num_server_formats; j++)
		{
			if (audio_format_compatible(&context->server_formats[j], &context->client_formats[i]))
			{
				agreed_format = &context->server_formats[j];
				break;
			}
		}

		if (agreed_format != NULL)
			break;
	}

	if (agreed_format == NULL)
	{
		WLog_ERR(TAG, "Could not agree on a audio format with the server\n");
		return;
	}

	context->SelectFormat(context, i);
}

int shadow_client_rdpsnd_init(rdpShadowClient* client)
{
	RdpsndServerContext* rdpsnd;
	rdpsnd = client->rdpsnd = rdpsnd_server_context_new(client->vcm);

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
	rdpsnd->Initialize(rdpsnd, TRUE);
	return 1;
}

void shadow_client_rdpsnd_uninit(rdpShadowClient* client)
{
	if (client->rdpsnd)
	{
		client->rdpsnd->Stop(client->rdpsnd);
		rdpsnd_server_context_free(client->rdpsnd);
		client->rdpsnd = NULL;
	}
}
