/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client Channels
 *
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
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

#include <freerdp/client/rail.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/disp.h>

#include "sdl_channels.h"
#include "sdl_freerdp.h"
#include "sdl_disp.h"

void sdl_OnChannelConnectedEventHandler(void* context, const ChannelConnectedEventArgs* e)
{
	sdlContext* sdl = (sdlContext*)context;

	WINPR_ASSERT(sdl);
	WINPR_ASSERT(e);

	if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		CliprdrClientContext* clip = (CliprdrClientContext*)e->pInterface;
		WINPR_ASSERT(clip);
		clip->custom = context;
	}
	else if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0)
	{
		sdl_disp_init(sdl->disp, (DispClientContext*)e->pInterface);
	}
	else
		freerdp_client_OnChannelConnectedEventHandler(context, e);
}

void sdl_OnChannelDisconnectedEventHandler(void* context, const ChannelDisconnectedEventArgs* e)
{
	sdlContext* sdl = (sdlContext*)context;

	WINPR_ASSERT(sdl);
	WINPR_ASSERT(e);

	// TODO: Set resizeable depending on disp channel and /dynamic-resolution
	if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		CliprdrClientContext* clip = (CliprdrClientContext*)e->pInterface;
		WINPR_ASSERT(clip);
		clip->custom = NULL;
	}
	else if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0)
	{
		sdl_disp_uninit(sdl->disp, (DispClientContext*)e->pInterface);
	}
	else
		freerdp_client_OnChannelDisconnectedEventHandler(context, e);
}
