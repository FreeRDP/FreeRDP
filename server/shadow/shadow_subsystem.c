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

struct _RDP_SHADOW_SUBSYSTEM
{
	const char* name;
	pfnShadowSubsystemEntry entry;
};
typedef struct _RDP_SHADOW_SUBSYSTEM RDP_SHADOW_SUBSYSTEM;


#ifdef WITH_SHADOW_X11
extern int X11_ShadowSubsystemEntry(RDP_SHADOW_ENTRY_POINTS* pEntryPoints);
#endif

#ifdef WITH_SHADOW_MAC
extern int Mac_ShadowSubsystemEntry(RDP_SHADOW_ENTRY_POINTS* pEntryPoints);
#endif

#ifdef WITH_SHADOW_WIN
extern int Win_ShadowSubsystemEntry(RDP_SHADOW_ENTRY_POINTS* pEntryPoints);
#endif


static RDP_SHADOW_SUBSYSTEM g_Subsystems[] =
{

#ifdef WITH_SHADOW_X11
	{ "X11", X11_ShadowSubsystemEntry },
#endif

#ifdef WITH_SHADOW_MAC
	{ "Mac", Mac_ShadowSubsystemEntry },
#endif

#ifdef WITH_SHADOW_WIN
	{ "Win", Win_ShadowSubsystemEntry },
#endif

	{ "", NULL }
};

static int g_SubsystemCount = (sizeof(g_Subsystems) / sizeof(g_Subsystems[0]));

pfnShadowSubsystemEntry shadow_subsystem_load_static_entry(const char* name)
{
	int index;

	if (!name)
	{
		for (index = 0; index < g_SubsystemCount; index++)
		{
			if (g_Subsystems[index].name)
				return g_Subsystems[index].entry;
		}
	}

	for (index = 0; index < g_SubsystemCount; index++)
	{
		if (strcmp(name, g_Subsystems[index].name) == 0)
			return g_Subsystems[index].entry;
	}

	return NULL;
}

int shadow_subsystem_load_entry_points(RDP_SHADOW_ENTRY_POINTS* pEntryPoints, const char* name)
{
	pfnShadowSubsystemEntry entry;

	entry = shadow_subsystem_load_static_entry(name);

	ZeroMemory(pEntryPoints, sizeof(RDP_SHADOW_ENTRY_POINTS));

	if (!entry)
		return -1;

	if (entry(pEntryPoints) < 0)
		return -1;

	return 1;
}

rdpShadowSubsystem* shadow_subsystem_new(const char* name)
{
	RDP_SHADOW_ENTRY_POINTS ep;
	rdpShadowSubsystem* subsystem = NULL;

	shadow_subsystem_load_entry_points(&ep, name);

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
	if (subsystem->ep.Free)
		subsystem->ep.Free(subsystem);
}

int shadow_subsystem_init(rdpShadowSubsystem* subsystem, rdpShadowServer* server)
{
	int status;

	subsystem->server = server;
	subsystem->selectedMonitor = server->selectedMonitor;

	subsystem->MsgPipe = MessagePipe_New();
	subsystem->updateEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	region16_init(&(subsystem->invalidRegion));

	if (!subsystem->ep.Init)
		return -1;

	status = subsystem->ep.Init(subsystem);

	return status;
}

void shadow_subsystem_uninit(rdpShadowSubsystem* subsystem)
{
	if (subsystem->ep.Uninit)
		subsystem->ep.Uninit(subsystem);

	if (subsystem->MsgPipe)
	{
		MessagePipe_Free(subsystem->MsgPipe);
		subsystem->MsgPipe = NULL;
	}

	if (subsystem->updateEvent)
	{
		CloseHandle(subsystem->updateEvent);
		subsystem->updateEvent = NULL;
	}

	if (subsystem->invalidRegion.data)
		region16_uninit(&(subsystem->invalidRegion));
}

int shadow_subsystem_start(rdpShadowSubsystem* subsystem)
{
	int status;

	if (!subsystem->ep.Start)
		return -1;

	status = subsystem->ep.Start(subsystem);

	return status;
}

int shadow_subsystem_stop(rdpShadowSubsystem* subsystem)
{
	int status;

	if (!subsystem->ep.Stop)
		return -1;

	status = subsystem->ep.Stop(subsystem);

	return status;
}

int shadow_enum_monitors(MONITOR_DEF* monitors, int maxMonitors, const char* name)
{
	int numMonitors = 0;
	RDP_SHADOW_ENTRY_POINTS ep;

	if (shadow_subsystem_load_entry_points(&ep, name) < 0)
		return -1;

	numMonitors = ep.EnumMonitors(monitors, maxMonitors);

	return numMonitors;
}
