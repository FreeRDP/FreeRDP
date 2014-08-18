/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2011-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/sysinfo.h>

#include <freerdp/codec/color.h>
#include <freerdp/codec/region.h>

#include "../shadow_screen.h"
#include "../shadow_surface.h"

#include "mac_shadow.h"

void mac_shadow_input_synchronize_event(macShadowSubsystem* subsystem, UINT32 flags)
{

}

void mac_shadow_input_keyboard_event(macShadowSubsystem* subsystem, UINT16 flags, UINT16 code)
{

}

void mac_shadow_input_unicode_keyboard_event(macShadowSubsystem* subsystem, UINT16 flags, UINT16 code)
{

}

void mac_shadow_input_mouse_event(macShadowSubsystem* subsystem, UINT16 flags, UINT16 x, UINT16 y)
{

}

void mac_shadow_input_extended_mouse_event(macShadowSubsystem* subsystem, UINT16 flags, UINT16 x, UINT16 y)
{

}

int mac_shadow_surface_copy(macShadowSubsystem* subsystem)
{
	return 1;
}

void* mac_shadow_subsystem_thread(macShadowSubsystem* subsystem)
{
	DWORD status;
	DWORD nCount;
	HANDLE events[32];
	HANDLE StopEvent;

	StopEvent = subsystem->server->StopEvent;

	nCount = 0;
	events[nCount++] = StopEvent;

	while (1)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (WaitForSingleObject(StopEvent, 0) == WAIT_OBJECT_0)
		{
			break;
		}
	}

	ExitThread(0);
	return NULL;
}

int mac_shadow_subsystem_init(macShadowSubsystem* subsystem)
{
	return 1;
}

int mac_shadow_subsystem_uninit(macShadowSubsystem* subsystem)
{
	if (!subsystem)
		return -1;

	return 1;
}

int mac_shadow_subsystem_start(macShadowSubsystem* subsystem)
{
	HANDLE thread;

	if (!subsystem)
		return -1;

	thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) mac_shadow_subsystem_thread,
			(void*) subsystem, 0, NULL);

	return 1;
}

int mac_shadow_subsystem_stop(macShadowSubsystem* subsystem)
{
	if (!subsystem)
		return -1;

	return 1;
}

void mac_shadow_subsystem_free(macShadowSubsystem* subsystem)
{
	if (!subsystem)
		return;

	mac_shadow_subsystem_uninit(subsystem);

	free(subsystem);
}

macShadowSubsystem* mac_shadow_subsystem_new(rdpShadowServer* server)
{
	macShadowSubsystem* subsystem;

	subsystem = (macShadowSubsystem*) calloc(1, sizeof(macShadowSubsystem));

	if (!subsystem)
		return NULL;

	subsystem->server = server;

	subsystem->Init = (pfnShadowSubsystemInit) mac_shadow_subsystem_init;
	subsystem->Uninit = (pfnShadowSubsystemInit) mac_shadow_subsystem_uninit;
	subsystem->Start = (pfnShadowSubsystemStart) mac_shadow_subsystem_start;
	subsystem->Stop = (pfnShadowSubsystemStop) mac_shadow_subsystem_stop;
	subsystem->Free = (pfnShadowSubsystemFree) mac_shadow_subsystem_free;

	subsystem->SurfaceCopy = (pfnShadowSurfaceCopy) mac_shadow_surface_copy;

	subsystem->SynchronizeEvent = (pfnShadowSynchronizeEvent) mac_shadow_input_synchronize_event;
	subsystem->KeyboardEvent = (pfnShadowKeyboardEvent) mac_shadow_input_keyboard_event;
	subsystem->UnicodeKeyboardEvent = (pfnShadowUnicodeKeyboardEvent) mac_shadow_input_unicode_keyboard_event;
	subsystem->MouseEvent = (pfnShadowMouseEvent) mac_shadow_input_mouse_event;
	subsystem->ExtendedMouseEvent = (pfnShadowExtendedMouseEvent) mac_shadow_input_extended_mouse_event;

	return subsystem;
}

rdpShadowSubsystem* Mac_ShadowCreateSubsystem(rdpShadowServer* server)
{
	return (rdpShadowSubsystem*) mac_shadow_subsystem_new(server);
}
