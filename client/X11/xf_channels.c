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

#include <freerdp/config.h>

#include <winpr/assert.h>
#include <freerdp/gdi/video.h>
#include "xf_channels.h"

#include "xf_client.h"
#include "xfreerdp.h"

#include "xf_gfx.h"
#if defined(CHANNEL_TSMF_CLIENT)
#include "xf_tsmf.h"
#endif
#include "xf_rail.h"
#include "xf_cliprdr.h"
#include "xf_disp.h"
#include "xf_video.h"

void xf_OnChannelConnectedEventHandler(void* context, const ChannelConnectedEventArgs* e)
{
	xfContext* xfc = (xfContext*)context;
	rdpSettings* settings;

	WINPR_ASSERT(xfc);
	WINPR_ASSERT(e);
	WINPR_ASSERT(e->name);

	settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

	if (FALSE)
	{
	}
#if defined(CHANNEL_TSMF_CLIENT)
	else if (strcmp(e->name, TSMF_DVC_CHANNEL_NAME) == 0)
	{
		xf_tsmf_init(xfc, (TsmfClientContext*)e->pInterface);
	}
#endif
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		xf_graphics_pipeline_init(xfc, (RdpgfxClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
		xf_rail_init(xfc, (RailClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		xf_cliprdr_init(xfc, (CliprdrClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
		xf_encomsp_init(xfc, (EncomspClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0)
	{
		xf_disp_init(xfc->xfDisp, (DispClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, VIDEO_CONTROL_DVC_CHANNEL_NAME) == 0)
	{
		if (settings->SoftwareGdi)
			gdi_video_control_init(xfc->common.context.gdi, (VideoClientContext*)e->pInterface);
		else
			xf_video_control_init(xfc, (VideoClientContext*)e->pInterface);
	}
	else
		freerdp_client_OnChannelConnectedEventHandler(context, e);
}

void xf_OnChannelDisconnectedEventHandler(void* context, const ChannelDisconnectedEventArgs* e)
{
	xfContext* xfc = (xfContext*)context;
	rdpSettings* settings;

	WINPR_ASSERT(xfc);
	WINPR_ASSERT(e);
	WINPR_ASSERT(e->name);

	settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

	if (FALSE)
	{
	}
	else if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0)
	{
		xf_disp_uninit(xfc->xfDisp, (DispClientContext*)e->pInterface);
	}
#if defined(CHANNEL_TSMF_CLIENT)
	else if (strcmp(e->name, TSMF_DVC_CHANNEL_NAME) == 0)
	{
		xf_tsmf_uninit(xfc, (TsmfClientContext*)e->pInterface);
	}
#endif
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		xf_graphics_pipeline_uninit(xfc, (RdpgfxClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
		xf_rail_uninit(xfc, (RailClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		xf_cliprdr_uninit(xfc, (CliprdrClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
		xf_encomsp_uninit(xfc, (EncomspClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, VIDEO_CONTROL_DVC_CHANNEL_NAME) == 0)
	{
		if (settings->SoftwareGdi)
			gdi_video_control_uninit(xfc->common.context.gdi, (VideoClientContext*)e->pInterface);
		else
			xf_video_control_uninit(xfc, (VideoClientContext*)e->pInterface);
	}
	else
		freerdp_client_OnChannelDisconnectedEventHandler(context, e);
}
