/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
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
#include <freerdp/client/tsmf.h>
#include <freerdp/client/rail.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/rdpgfx.h>

#include "pf_channels.h"
#include "pf_client.h"
#include "pf_context.h"
#include "pf_rdpgfx.h"
#include "pf_log.h"

#define TAG PROXY_TAG("channels")

void pf_OnChannelConnectedEventHandler(void* context,
                                       ChannelConnectedEventArgs* e)
{
	pClientContext* pc = (pClientContext*) context;
	pServerContext* ps = pc->pdata->ps;
	RdpgfxClientContext* gfx;
	RdpgfxServerContext* server;
	WLog_DBG(TAG, "Channel connected: %s", e->name);

	if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
		pc->rdpei = (RdpeiClientContext*) e->pInterface;
	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		gfx = (RdpgfxClientContext*) e->pInterface;
		pc->gfx = gfx;
		server = ps->gfx;
		pf_rdpgfx_pipeline_init(gfx, server, pc->pdata);
	}
}

void pf_OnChannelDisconnectedEventHandler(void* context,
        ChannelDisconnectedEventArgs* e)
{
	pClientContext* pc = (pClientContext*) context;
	rdpSettings* settings;
	settings = ((rdpContext*)pc)->settings;

	if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
		pc->rdpei = NULL;
	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		gdi_graphics_pipeline_uninit(((rdpContext*)pc)->gdi,
		                             (RdpgfxClientContext*) e->pInterface);
	}
}
