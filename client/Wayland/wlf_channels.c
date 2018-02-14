/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Client Channels
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "wlf_channels.h"
#include "wlfreerdp.h"

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT wlf_encomsp_participant_created(EncomspClientContext* context,
        ENCOMSP_PARTICIPANT_CREATED_PDU* participantCreated)
{
	return CHANNEL_RC_OK;
}

static void wlf_encomsp_init(wlfContext* wlf, EncomspClientContext* encomsp)
{
	if (!wlf)
		return;

	wlf->encomsp = encomsp;
	encomsp->custom = (void*) wlf;
	encomsp->ParticipantCreated = wlf_encomsp_participant_created;
}

static void wlf_encomsp_uninit(wlfContext* wlf, EncomspClientContext* encomsp)
{
	if (!wlf)
		return;

	wlf->encomsp = NULL;
}


void wlf_OnChannelConnectedEventHandler(void* context,
                                        ChannelConnectedEventArgs* e)
{
	wlfContext* wlf = (wlfContext*) context;
	rdpSettings* settings = wlf->context.settings;

	if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
		wlf->rdpei = (RdpeiClientContext*) e->pInterface;
	}
	else if (strcmp(e->name, TSMF_DVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		if (settings->SoftwareGdi)
			gdi_graphics_pipeline_init(wlf->context.gdi, (RdpgfxClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
		wlf_encomsp_init(wlf, (EncomspClientContext*) e->pInterface);
	}
}

void wlf_OnChannelDisconnectedEventHandler(void* context,
        ChannelDisconnectedEventArgs* e)
{
	wlfContext* wlf = (wlfContext*) context;
	rdpSettings* settings = wlf->context.settings;

	if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
		wlf->rdpei = NULL;
	}
	else if (strcmp(e->name, TSMF_DVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		if (settings->SoftwareGdi)
			gdi_graphics_pipeline_uninit(wlf->context.gdi,
			                             (RdpgfxClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
		wlf_encomsp_uninit(wlf, (EncomspClientContext*) e->pInterface);
	}
}
