/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2016 Jiang Zihao <zihao.jiang@yahoo.com>
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

#include <freerdp/log.h>
#include "shadow.h"

#include "shadow_rdpgfx.h"

#define TAG SERVER_TAG("shadow")

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_caps_advertise(RdpgfxServerContext* context, RDPGFX_CAPS_ADVERTISE_PDU* capsAdvertise)
{
	UINT16 index;
	RDPGFX_CAPS_CONFIRM_PDU pdu;
	rdpSettings* settings = context->rdpcontext->settings;
	UINT32 flags = 0;

	for (index = 0; index < capsAdvertise->capsSetCount; index++)
	{
		pdu.capsSet = &(capsAdvertise->capsSets[index]);
		if (pdu.capsSet->version == RDPGFX_CAPVERSION_10)
		{
			if (settings)
			{
				flags = pdu.capsSet->flags;
				settings->GfxSmallCache = (flags & RDPGFX_CAPS_FLAG_SMALL_CACHE);
				settings->GfxH264 = !(flags & RDPGFX_CAPS_FLAG_AVC_DISABLED);
			}

			return context->CapsConfirm(context, &pdu);
		}
	}
	for (index = 0; index < capsAdvertise->capsSetCount; index++)
	{
		if (pdu.capsSet->version == RDPGFX_CAPVERSION_81)
		{
			if (settings)
			{
				flags = pdu.capsSet->flags;
				settings->GfxThinClient = (flags & RDPGFX_CAPS_FLAG_THINCLIENT);
				settings->GfxSmallCache = (flags & RDPGFX_CAPS_FLAG_SMALL_CACHE);
				settings->GfxH264 = (flags & RDPGFX_CAPS_FLAG_AVC420_ENABLED);
			}

			return context->CapsConfirm(context, &pdu);
		}
	}
	for (index = 0; index < capsAdvertise->capsSetCount; index++)
	{
		if (pdu.capsSet->version == RDPGFX_CAPVERSION_8)
		{
			if (settings)
			{
				flags = pdu.capsSet->flags;
				settings->GfxThinClient = (flags & RDPGFX_CAPS_FLAG_THINCLIENT);
				settings->GfxSmallCache = (flags & RDPGFX_CAPS_FLAG_SMALL_CACHE);
			}

			return context->CapsConfirm(context, &pdu);
		}
	}

	return CHANNEL_RC_UNSUPPORTED_VERSION;
}

int shadow_client_rdpgfx_init(rdpShadowClient* client)
{
	RdpgfxServerContext* rdpgfx;
	rdpgfx = client->rdpgfx = rdpgfx_server_context_new(client->vcm);
	if (!rdpgfx)
	{
		return 0;
	}

	rdpgfx->rdpcontext = &client->context;

	rdpgfx->custom = client;

	rdpgfx->CapsAdvertise = rdpgfx_caps_advertise;

	return 1;
}

void shadow_client_rdpgfx_uninit(rdpShadowClient* client)
{
	if (client->rdpgfx)
	{
		rdpgfx_server_context_free(client->rdpgfx);
		client->rdpgfx = NULL;
	}
}
