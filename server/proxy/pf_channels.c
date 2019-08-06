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
#include <freerdp/client/disp.h>

#include "pf_channels.h"
#include "pf_client.h"
#include "pf_context.h"
#include "pf_rdpgfx.h"
#include "pf_log.h"
#include "pf_disp.h"

#define TAG PROXY_TAG("channels")

void pf_OnChannelConnectedEventHandler(void* data,
                                       ChannelConnectedEventArgs* e)
{
	pClientContext* pc = (pClientContext*) data;
	pServerContext* ps = pc->pdata->ps;

	WLog_INFO(TAG, "Channel connected: %s", e->name);

	if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
		pc->rdpei = (RdpeiClientContext*) e->pInterface;
	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		if (!ps->gfx->Open(ps->gfx))
		{
			WLog_ERR(TAG, "failed to open GFX server");
			return;
		}

		pc->gfx = (RdpgfxClientContext*) e->pInterface;
		pf_rdpgfx_pipeline_init(pc->gfx, ps->gfx, pc->pdata);
	}
	else if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0)
	{
		if (ps->disp->Open(ps->disp) != CHANNEL_RC_OK)
		{
			WLog_ERR(TAG, "failed to open disp channel");
			return;
		}

		pc->disp = (DispClientContext*) e->pInterface;
		pf_disp_register_callbacks(pc->disp, ps->disp, pc->pdata);
	}
}

void pf_OnChannelDisconnectedEventHandler(void* data,
        ChannelDisconnectedEventArgs* e)
{
	rdpContext* context = (rdpContext*) data;
	pClientContext* pc = (pClientContext*) context;
	pServerContext* ps = pc->pdata->ps;

	WLog_INFO(TAG, "Channel disconnected: %s", e->name);

	if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
		pc->rdpei = NULL;
	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		if (!ps->gfx->Close(ps->gfx))
			WLog_ERR(TAG, "failed to close gfx server");

		gdi_graphics_pipeline_uninit(context->gdi, (RdpgfxClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0)
	{
		if (ps->disp->Close(ps->disp) != CHANNEL_RC_OK)
			WLog_ERR(TAG, "failed to close disp server");

		pc->disp = NULL;
	}
}

BOOL pf_server_channels_init(pServerContext* ps)
{
	rdpContext* context = (rdpContext*) ps;
	proxyConfig* config = ps->pdata->config;

	if (context->settings->SupportGraphicsPipeline && config->GFX)
	{
		if (!pf_server_rdpgfx_init(ps))
			return FALSE;
	}

	if (config->DisplayControl)
	{
		if (!pf_server_disp_init(ps))
			return FALSE;
	}

	return TRUE;
}

void pf_server_channels_free(pServerContext* ps)
{
	if (ps->gfx)
	{
		rdpgfx_server_context_free(ps->gfx);
		ps->gfx = NULL;
	}

	if (ps->disp)
	{
		disp_server_context_free(ps->disp);
		ps->disp = NULL;
	}
}
