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

#ifdef WITH_SHADOW_X11
extern rdpShadowSubsystem* X11_ShadowCreateSubsystem(rdpShadowServer* server);
#endif

#ifdef WITH_SHADOW_MAC
extern rdpShadowSubsystem* Mac_ShadowCreateSubsystem(rdpShadowServer* server);
#endif

#ifdef WITH_SHADOW_WIN
extern rdpShadowSubsystem* Win_ShadowCreateSubsystem(rdpShadowServer* server);
#endif

rdpShadowSubsystem* shadow_subsystem_new(UINT32 flags)
{
	rdpShadowSubsystem* subsystem = NULL;
	pfnShadowCreateSubsystem CreateSubsystem = NULL;

#ifdef WITH_SHADOW_X11
	CreateSubsystem = X11_ShadowCreateSubsystem;
#endif

#ifdef WITH_SHADOW_MAC
	CreateSubsystem = Mac_ShadowCreateSubsystem;
#endif

#ifdef WITH_SHADOW_WIN
	CreateSubsystem = Win_ShadowCreateSubsystem;
#endif

	if (CreateSubsystem)
		subsystem = CreateSubsystem(NULL);

	return subsystem;
}

void shadow_subsystem_free(rdpShadowSubsystem* subsystem)
{
	if (subsystem->Free)
		subsystem->Free(subsystem);
}

int shadow_subsystem_init(rdpShadowSubsystem* subsystem, rdpShadowServer* server)
{
	int status;

	subsystem->server = server;
	subsystem->selectedMonitor = server->selectedMonitor;

	subsystem->updateEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	subsystem->MsgPipe = MessagePipe_New();
	region16_init(&(subsystem->invalidRegion));

	if (!subsystem->Init)
		return -1;

	if (subsystem->Init)
		status = subsystem->Init(subsystem);

	return status;
}

void shadow_subsystem_uninit(rdpShadowSubsystem* subsystem)
{
	if (subsystem->Uninit)
		subsystem->Uninit(subsystem);

	if (subsystem->updateEvent)
	{
		CloseHandle(subsystem->updateEvent);
		subsystem->updateEvent = NULL;
	}

	if (subsystem->MsgPipe)
	{
		MessagePipe_Free(subsystem->MsgPipe);
		subsystem->MsgPipe = NULL;
	}

	if (subsystem->invalidRegion.data)
	{
		region16_uninit(&(subsystem->invalidRegion));
	}
}
