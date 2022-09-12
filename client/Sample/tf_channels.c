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

#include <freerdp/config.h>

#include <winpr/assert.h>
#include <freerdp/gdi/gfx.h>

#include <freerdp/client/rdpei.h>
#include <freerdp/client/rail.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/client/encomsp.h>

#include "tf_channels.h"
#include "tf_freerdp.h"

static UINT tf_update_surfaces(RdpgfxClientContext* context)
{
	WINPR_UNUSED(context);
	return CHANNEL_RC_OK;
}

void tf_OnChannelConnectedEventHandler(void* context, const ChannelConnectedEventArgs* e)
{
	tfContext* tf = (tfContext*)context;

	WINPR_ASSERT(tf);
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
	else
		freerdp_client_OnChannelConnectedEventHandler(context, e);
}

void tf_OnChannelDisconnectedEventHandler(void* context, const ChannelDisconnectedEventArgs* e)
{
	tfContext* tf = (tfContext*)context;

	WINPR_ASSERT(tf);
	WINPR_ASSERT(e);

	if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		CliprdrClientContext* clip = (CliprdrClientContext*)e->pInterface;
		WINPR_ASSERT(clip);
		clip->custom = NULL;
	}
	else
		freerdp_client_OnChannelDisconnectedEventHandler(context, e);
}
