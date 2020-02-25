/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Sample Client Channels
 *
 * Copyright 2018 Armin Novak <armin.novak@thincast.com>
 * Copyright 2018 Thincast Technologies GmbH
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

#include <freerdp/gdi/gfx.h>

#include <freerdp/client/rdpei.h>
#include <freerdp/client/rail.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/client/encomsp.h>

#include "tf_channels.h"
#include "tf_freerdp.h"

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
tf_encomsp_participant_created(EncomspClientContext* context,
                               const ENCOMSP_PARTICIPANT_CREATED_PDU* participantCreated)
{
	WINPR_UNUSED(context);
	WINPR_UNUSED(participantCreated);
	return CHANNEL_RC_OK;
}

static void tf_encomsp_init(tfContext* tf, EncomspClientContext* encomsp)
{
	tf->encomsp = encomsp;
	encomsp->custom = (void*)tf;
	encomsp->ParticipantCreated = tf_encomsp_participant_created;
}

static void tf_encomsp_uninit(tfContext* tf, EncomspClientContext* encomsp)
{
	if (encomsp)
	{
		encomsp->custom = NULL;
		encomsp->ParticipantCreated = NULL;
	}

	if (tf)
		tf->encomsp = NULL;
}

void tf_OnChannelConnectedEventHandler(void* context, ChannelConnectedEventArgs* e)
{
	tfContext* tf = (tfContext*)context;

	if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
		tf->rdpei = (RdpeiClientContext*)e->pInterface;
	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		gdi_graphics_pipeline_init(tf->context.gdi, (RdpgfxClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
		tf_encomsp_init(tf, (EncomspClientContext*)e->pInterface);
	}
}

void tf_OnChannelDisconnectedEventHandler(void* context, ChannelDisconnectedEventArgs* e)
{
	tfContext* tf = (tfContext*)context;

	if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
		tf->rdpei = NULL;
	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		gdi_graphics_pipeline_uninit(tf->context.gdi, (RdpgfxClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
		tf_encomsp_uninit(tf, (EncomspClientContext*)e->pInterface);
	}
}
