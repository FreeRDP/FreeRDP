/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "shadow.h"

#include "shadow_subsystem.h"

static pfnShadowSubsystemEntry pSubsystemEntry = NULL;

void shadow_subsystem_set_entry(pfnShadowSubsystemEntry pEntry)
{
	pSubsystemEntry = pEntry;
}

static int shadow_subsystem_load_entry_points(RDP_SHADOW_ENTRY_POINTS* pEntryPoints)
{
	ZeroMemory(pEntryPoints, sizeof(RDP_SHADOW_ENTRY_POINTS));

	if (!pSubsystemEntry)
		return -1;

	if (pSubsystemEntry(pEntryPoints) < 0)
		return -1;

	return 1;
}

rdpShadowSubsystem* shadow_subsystem_new()
{
	RDP_SHADOW_ENTRY_POINTS ep;
	rdpShadowSubsystem* subsystem = NULL;

	shadow_subsystem_load_entry_points(&ep);

	if (!ep.New)
		return NULL;

	subsystem = ep.New();

	if (!subsystem)
		return NULL;

	CopyMemory(&(subsystem->ep), &ep, sizeof(RDP_SHADOW_ENTRY_POINTS));

	return subsystem;
}

void shadow_subsystem_free(rdpShadowSubsystem* subsystem)
{
	if (subsystem && subsystem->ep.Free)
		subsystem->ep.Free(subsystem);
}

int shadow_subsystem_init(rdpShadowSubsystem* subsystem, rdpShadowServer* server)
{
	int status = -1;

	if (!subsystem || !subsystem->ep.Init)
		return -1;

	subsystem->server = server;
	subsystem->selectedMonitor = server->selectedMonitor;

	if (!(subsystem->MsgPipe = MessagePipe_New()))
		goto fail;

	if (!(subsystem->updateEvent = shadow_multiclient_new()))
		goto fail;

	region16_init(&(subsystem->invalidRegion));

	if ((status = subsystem->ep.Init(subsystem)) >= 0)
		return status;

fail:
	if (subsystem->MsgPipe)
	{
		MessagePipe_Free(subsystem->MsgPipe);
		subsystem->MsgPipe = NULL;
	}

	if (subsystem->updateEvent)
	{
		shadow_multiclient_free(subsystem->updateEvent);
		subsystem->updateEvent = NULL;
	}

	return status;
}

static void shadow_subsystem_free_queued_message(void *obj)
{
	wMessage *message = (wMessage*)obj;
	if (message->Free)
	{
		message->Free(message);
		message->Free = NULL;
	}
}

void shadow_subsystem_uninit(rdpShadowSubsystem* subsystem)
{
	if (!subsystem)
		return;

	if (subsystem->ep.Uninit)
		subsystem->ep.Uninit(subsystem);

	if (subsystem->MsgPipe)
	{
		/* Release resource in messages before free */
		subsystem->MsgPipe->In->object.fnObjectFree = shadow_subsystem_free_queued_message;
		MessageQueue_Clear(subsystem->MsgPipe->In);
		subsystem->MsgPipe->Out->object.fnObjectFree = shadow_subsystem_free_queued_message;
		MessageQueue_Clear(subsystem->MsgPipe->Out);
		MessagePipe_Free(subsystem->MsgPipe);
		subsystem->MsgPipe = NULL;
	}

	if (subsystem->updateEvent)
	{
		shadow_multiclient_free(subsystem->updateEvent);
		subsystem->updateEvent = NULL;
	}

	if (subsystem->invalidRegion.data)
		region16_uninit(&(subsystem->invalidRegion));
}

int shadow_subsystem_start(rdpShadowSubsystem* subsystem)
{
	int status;

	if (!subsystem || !subsystem->ep.Start)
		return -1;

	status = subsystem->ep.Start(subsystem);

	return status;
}

int shadow_subsystem_stop(rdpShadowSubsystem* subsystem)
{
	int status;

	if (!subsystem || !subsystem->ep.Stop)
		return -1;

	status = subsystem->ep.Stop(subsystem);

	return status;
}

int shadow_enum_monitors(MONITOR_DEF* monitors, int maxMonitors)
{
	int numMonitors = 0;
	RDP_SHADOW_ENTRY_POINTS ep;

	if (shadow_subsystem_load_entry_points(&ep) < 0)
		return -1;

	numMonitors = ep.EnumMonitors(monitors, maxMonitors);

	return numMonitors;
}
