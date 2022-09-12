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

#include <freerdp/gdi/gfx.h>

#include <freerdp/gdi/video.h>

#include "wlf_channels.h"
#include "wlf_cliprdr.h"
#include "wlf_disp.h"
#include "wlfreerdp.h"

void wlf_OnChannelConnectedEventHandler(void* context, const ChannelConnectedEventArgs* e)
{
	wlfContext* wlf = (wlfContext*)context;

	WINPR_ASSERT(wlf);
	WINPR_ASSERT(e);

	if (FALSE)
	{
	}
	else if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		wlf_cliprdr_init(wlf->clipboard, (CliprdrClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0)
	{
		wlf_disp_init(wlf->disp, (DispClientContext*)e->pInterface);
	}
	else
		freerdp_client_OnChannelConnectedEventHandler(context, e);
}

void wlf_OnChannelDisconnectedEventHandler(void* context, const ChannelDisconnectedEventArgs* e)
{
	wlfContext* wlf = (wlfContext*)context;

	WINPR_ASSERT(wlf);
	WINPR_ASSERT(e);

	if (FALSE)
	{
	}
	else if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		wlf_cliprdr_uninit(wlf->clipboard, (CliprdrClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0)
	{
		wlf_disp_uninit(wlf->disp, (DispClientContext*)e->pInterface);
	}
	else
		freerdp_client_OnChannelDisconnectedEventHandler(context, e);
}
