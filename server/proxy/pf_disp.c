/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

#include <freerdp/server/disp.h>

#include "pf_disp.h"
#include "pf_log.h"

#define TAG PROXY_TAG("disp")

BOOL pf_server_disp_init(pServerContext* ps)
{
	DispServerContext* disp;
	disp = ps->disp = disp_server_context_new(ps->vcm);

	if (!disp)
	{
		return FALSE;
	}

	disp->rdpcontext = (rdpContext*)ps;
	return TRUE;
}

static UINT pf_disp_monitor_layout(DispServerContext* context,
                                   const DISPLAY_CONTROL_MONITOR_LAYOUT_PDU* pdu)
{
	proxyData* pdata = (proxyData*)context->custom;
	DispClientContext* client = (DispClientContext*)pdata->pc->disp;
	WLog_DBG(TAG, __FUNCTION__);
	return client->SendMonitorLayout(client, pdu->NumMonitors, pdu->Monitors);
}

static UINT pf_disp_on_caps_control(DispClientContext* context, UINT32 MaxNumMonitors,
                                    UINT32 MaxMonitorAreaFactorA, UINT32 MaxMonitorAreaFactorB)
{
	proxyData* pdata = (proxyData*)context->custom;
	DispServerContext* server = (DispServerContext*)pdata->ps->disp;
	WLog_DBG(TAG, __FUNCTION__);
	/* Update caps of proxy's disp server */
	server->MaxMonitorAreaFactorA = MaxMonitorAreaFactorA;
	server->MaxMonitorAreaFactorB = MaxMonitorAreaFactorB;
	server->MaxNumMonitors = MaxNumMonitors;
	/* Send CapsControl to client */
	return server->DisplayControlCaps(server);
}

void pf_disp_register_callbacks(DispClientContext* client, DispServerContext* server,
                                proxyData* pdata)
{
	client->custom = (void*)pdata;
	server->custom = (void*)pdata;
	/* client receives from server, forward using disp server to original client */
	client->DisplayControlCaps = pf_disp_on_caps_control;
	/* server receives from client, forward to target server using disp client */
	server->DispMonitorLayout = pf_disp_monitor_layout;
}
