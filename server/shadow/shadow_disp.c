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

#include <winpr/assert.h>
#include <winpr/synch.h>
#include <winpr/sysinfo.h>
#include <winpr/thread.h>

#include <freerdp/log.h>
#include "shadow.h"

#include "shadow_disp.h"

#define TAG SERVER_TAG("shadow.disp")

#if defined(CHANNEL_DISP_SERVER)

/* Limits of the client's layout PDU, not of what the subsystem then does with it */
#define SHADOW_DISP_MAX_MONITORS 16
#define SHADOW_DISP_MAX_SIDE 8192

/* A client that is resized by hand sends a layout every few hundred milliseconds. Each one
 * the subsystem follows is a new screen size and a resize of every client, so the layouts
 * are applied one at a time: the next only once the client shows the size of the last one
 * (at most SHADOW_DISP_SETTLE_MAX_MS), and SHADOW_DISP_GAP_MS after it. What came in
 * meanwhile is reduced to the newest layout. */
#define SHADOW_DISP_SETTLE_MAX_MS 2000
#define SHADOW_DISP_GAP_MS 300
#define SHADOW_DISP_CHECK_MS 20

typedef struct
{
	rdpShadowClient* client;
	CRITICAL_SECTION lock;
	HANDLE event; /* a layout is pending, manual reset */
	HANDLE stop;
	HANDLE thread;
	BOOL pending;
	UINT32 width;
	UINT32 height;
	UINT64 received;
	UINT64 applied;
} ShadowDisp;

/* the client's desktop size; only a hint for when to apply the next layout, read from
 * another thread */
static void shadow_disp_client_size(const ShadowDisp* sd, UINT32* width, UINT32* height)
{
	WINPR_ASSERT(sd);
	const rdpSettings* settings = sd->client->context.settings;
	*width = freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth);
	*height = freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight);
}

/* the client took the layout: it is active with the size asked for, or with another size
 * than before (the subsystem may round, or arrange several monitors) */
static BOOL shadow_disp_client_settled(const ShadowDisp* sd, UINT32 width, UINT32 height,
                                       UINT32 oldWidth, UINT32 oldHeight)
{
	UINT32 w = 0;
	UINT32 h = 0;
	shadow_disp_client_size(sd, &w, &h);
	if (!sd->client->activated)
		return FALSE;
	return ((w == width) && (h == height)) || (w != oldWidth) || (h != oldHeight);
}

/* @return FALSE when stopped */
static BOOL shadow_disp_wait(ShadowDisp* sd, DWORD ms)
{
	return WaitForSingleObject(sd->stop, ms) != WAIT_OBJECT_0;
}

static DWORD WINAPI shadow_disp_thread(LPVOID arg)
{
	ShadowDisp* sd = arg;
	WINPR_ASSERT(sd);
	rdpShadowClient* client = sd->client;
	WINPR_ASSERT(client);
	rdpShadowSubsystem* subsystem = client->subsystem;
	WINPR_ASSERT(subsystem);

	HANDLE events[] = { sd->stop, sd->event };
	while (WaitForMultipleObjects(ARRAYSIZE(events), events, FALSE, INFINITE) == WAIT_OBJECT_0 + 1)
	{
		EnterCriticalSection(&sd->lock);
		(void)ResetEvent(sd->event);
		const BOOL pending = sd->pending;
		const UINT32 width = sd->width;
		const UINT32 height = sd->height;
		sd->pending = FALSE;
		LeaveCriticalSection(&sd->lock);

		if (!pending)
			continue;

		UINT32 oldWidth = 0;
		UINT32 oldHeight = 0;
		shadow_disp_client_size(sd, &oldWidth, &oldHeight);

		if (!subsystem->ClientResize(subsystem, client, width, height))
			WLog_WARN(TAG, "cannot follow the client to %" PRIu32 "x%" PRIu32, width, height);

		const UINT64 start = GetTickCount64();
		while (!shadow_disp_client_settled(sd, width, height, oldWidth, oldHeight) &&
		       (GetTickCount64() - start < SHADOW_DISP_SETTLE_MAX_MS))
		{
			if (!shadow_disp_wait(sd, SHADOW_DISP_CHECK_MS))
				return 0;
		}
		if (!shadow_disp_wait(sd, SHADOW_DISP_GAP_MS))
			return 0;

		EnterCriticalSection(&sd->lock);
		sd->applied++;
		if (sd->pending)
			WLog_DBG(TAG, "%" PRIu64 " layouts received, %" PRIu64 " applied", sd->received,
			         sd->applied);
		LeaveCriticalSection(&sd->lock);
	}
	return 0;
}

WINPR_ATTR_NODISCARD
static UINT shadow_disp_monitor_layout(DispServerContext* context,
                                       const DISPLAY_CONTROL_MONITOR_LAYOUT_PDU* pdu)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(pdu);

	ShadowDisp* sd = context->custom;
	WINPR_ASSERT(sd);
	rdpShadowClient* client = sd->client;
	WINPR_ASSERT(client);

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

	/* the newest layout replaces one that was not applied yet */
	EnterCriticalSection(&sd->lock);
	sd->pending = TRUE;
	sd->width = monitor->Width;
	sd->height = monitor->Height;
	sd->received++;
	(void)SetEvent(sd->event);
	LeaveCriticalSection(&sd->lock);

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

	ShadowDisp* sd = calloc(1, sizeof(ShadowDisp));
	if (!sd)
		return FALSE;
	sd->client = client;
	if (!InitializeCriticalSectionAndSpinCount(&sd->lock, 4000))
	{
		free(sd);
		return FALSE;
	}
	sd->event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	sd->stop = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (sd->event && sd->stop)
		sd->thread = CreateThread(nullptr, 0, shadow_disp_thread, sd, 0, nullptr);

	DispServerContext* disp = sd->thread ? disp_server_context_new(client->vcm) : nullptr;
	if (!disp)
	{
		if (sd->thread)
		{
			(void)SetEvent(sd->stop);
			(void)WaitForSingleObject(sd->thread, INFINITE);
			(void)CloseHandle(sd->thread);
		}
		if (sd->event)
			(void)CloseHandle(sd->event);
		if (sd->stop)
			(void)CloseHandle(sd->stop);
		DeleteCriticalSection(&sd->lock);
		free(sd);
		return FALSE;
	}
	client->disp = disp;

	disp->custom = sd;
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
	DispServerContext* disp = client->disp;
	if (!disp)
		return;
	ShadowDisp* sd = disp->custom;

	/* no layout callback after this */
	disp_server_context_free(disp);
	client->disp = nullptr;

	if (sd)
	{
		(void)SetEvent(sd->stop);
		(void)WaitForSingleObject(sd->thread, INFINITE);
		(void)CloseHandle(sd->thread);
		(void)CloseHandle(sd->event);
		(void)CloseHandle(sd->stop);
		DeleteCriticalSection(&sd->lock);
		free(sd);
	}
#endif
}
