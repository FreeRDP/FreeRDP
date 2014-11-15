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

#include "xf_channels.h"

#include "xf_client.h"
#include "xfreerdp.h"

#include "xf_gfx.h"
#include "xf_tsmf.h"
#include "xf_rail.h"
#include "xf_cliprdr.h"

void xf_OnChannelConnectedEventHandler(rdpContext* context, ChannelConnectedEventArgs* e)
{
	xfContext* xfc = (xfContext*) context;
	rdpSettings* settings = context->settings;

	if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
		xfc->rdpei = (RdpeiClientContext*) e->pInterface;
	}
	else if (strcmp(e->name, TSMF_DVC_CHANNEL_NAME) == 0)
	{
		xf_tsmf_init(xfc, (TsmfClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		if (settings->SoftwareGdi)
			gdi_graphics_pipeline_init(context->gdi, (RdpgfxClientContext*) e->pInterface);
		else
			xf_graphics_pipeline_init(xfc, (RdpgfxClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
		xf_rail_init(xfc, (RailClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		xf_cliprdr_init(xfc, (CliprdrClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
		xf_encomsp_init(xfc, (EncomspClientContext*) e->pInterface);
	}
}

void xf_OnChannelDisconnectedEventHandler(rdpContext* context, ChannelDisconnectedEventArgs* e)
{
	xfContext* xfc = (xfContext*) context;
	rdpSettings* settings = context->settings;

	if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
		xfc->rdpei = NULL;
	}
	else if (strcmp(e->name, TSMF_DVC_CHANNEL_NAME) == 0)
	{
		xf_tsmf_uninit(xfc, (TsmfClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		if (settings->SoftwareGdi)
			gdi_graphics_pipeline_uninit(context->gdi, (RdpgfxClientContext*) e->pInterface);
		else
			xf_graphics_pipeline_uninit(xfc, (RdpgfxClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
		xf_rail_uninit(xfc, (RailClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		xf_cliprdr_uninit(xfc, (CliprdrClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
		xf_encomsp_uninit(xfc, (EncomspClientContext*) e->pInterface);
	}
}
