/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "wf_channels.h"

#include "wf_rail.h"
#include "wf_cliprdr.h"

#include <freerdp/gdi/gfx.h>

#include <freerdp/log.h>
#define TAG CLIENT_TAG("windows")

void wf_OnChannelConnectedEventHandler(void* context,
                                       ChannelConnectedEventArgs* e)
{
	wfContext* wfc = (wfContext*) context;
	rdpSettings* settings = wfc->context.settings;

	if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		if (!settings->SoftwareGdi)
			WLog_WARN(TAG,
			          "Channel "RDPGFX_DVC_CHANNEL_NAME" does not support hardware acceleration, using fallback.");

		gdi_graphics_pipeline_init(wfc->context.gdi, (RdpgfxClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
		wf_rail_init(wfc, (RailClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		wf_cliprdr_init(wfc, (CliprdrClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
	}
}

void wf_OnChannelDisconnectedEventHandler(void* context,
        ChannelDisconnectedEventArgs* e)
{
	wfContext* wfc = (wfContext*) context;
	rdpSettings* settings = wfc->context.settings;

	if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		gdi_graphics_pipeline_uninit(wfc->context.gdi,
		                             (RdpgfxClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
		wf_rail_uninit(wfc, (RailClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		wf_cliprdr_uninit(wfc, (CliprdrClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
	}
}
