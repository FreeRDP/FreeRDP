/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2026 Cong Zhang <congzhangzh@gmail.com>
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

#include <freerdp/log.h>
#include "shadow.h"

#include "shadow_disp.h"

#define TAG SERVER_TAG("shadow.disp")

#if defined(CHANNEL_DISP_SERVER)

/* Limits of the client's layout PDU, not of what the subsystem then does with it */
#define SHADOW_DISP_MAX_MONITORS 16
#define SHADOW_DISP_MAX_SIDE 8192

WINPR_ATTR_NODISCARD
static UINT shadow_disp_monitor_layout(DispServerContext* context,
                                       const DISPLAY_CONTROL_MONITOR_LAYOUT_PDU* pdu)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(pdu);

	rdpShadowClient* client = context->custom;
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->subsystem);

	if (!client->mayInteract || (pdu->NumMonitors == 0))
		return CHANNEL_RC_OK;

	const DISPLAY_CONTROL_MONITOR_LAYOUT* monitor = &pdu->Monitors[0];
	for (UINT32 x = 0; x < pdu->NumMonitors; x++)
	{
		if (pdu->Monitors[x].Flags & DISPLAY_CONTROL_MONITOR_PRIMARY)
		{
			monitor = &pdu->Monitors[x];
			break;
		}
	}

	rdpShadowSubsystem* subsystem = client->subsystem;
	if (!subsystem->ClientResize(subsystem, client, monitor->Width, monitor->Height))
		WLog_WARN(TAG, "cannot follow the client to %" PRIu32 "x%" PRIu32, monitor->Width,
		          monitor->Height);

	return CHANNEL_RC_OK;
}

#endif

BOOL shadow_client_disp_init(rdpShadowClient* client)
{
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->subsystem);

#if defined(CHANNEL_DISP_SERVER)
	if (!client->subsystem->ClientResize)
		return TRUE;

	DispServerContext* disp = client->disp = disp_server_context_new(client->vcm);
	if (!disp)
		return FALSE;

	disp->custom = client;
	disp->rdpcontext = &client->context;
	disp->MaxNumMonitors = SHADOW_DISP_MAX_MONITORS;
	disp->MaxMonitorAreaFactorA = SHADOW_DISP_MAX_SIDE;
	disp->MaxMonitorAreaFactorB = SHADOW_DISP_MAX_SIDE;
	disp->DispMonitorLayout = shadow_disp_monitor_layout;
#endif
	return TRUE;
}

void shadow_client_disp_uninit(rdpShadowClient* client)
{
	WINPR_ASSERT(client);

#if defined(CHANNEL_DISP_SERVER)
	disp_server_context_free(client->disp);
	client->disp = nullptr;
#endif
}
