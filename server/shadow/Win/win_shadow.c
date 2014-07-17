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

#include "win_shadow.h"

void win_shadow_input_synchronize_event(winShadowSubsystem* subsystem, UINT32 flags)
{

}

void win_shadow_input_keyboard_event(winShadowSubsystem* subsystem, UINT16 flags, UINT16 code)
{
	INPUT event;

	event.type = INPUT_KEYBOARD;
	event.ki.wVk = 0;
	event.ki.wScan = code;
	event.ki.dwFlags = KEYEVENTF_SCANCODE;
	event.ki.dwExtraInfo = 0;
	event.ki.time = 0;

	if (flags & KBD_FLAGS_RELEASE)
		event.ki.dwFlags |= KEYEVENTF_KEYUP;

	if (flags & KBD_FLAGS_EXTENDED)
		event.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;

	SendInput(1, &event, sizeof(INPUT));
}

void win_shadow_input_unicode_keyboard_event(winShadowSubsystem* subsystem, UINT16 flags, UINT16 code)
{
	INPUT event;

	event.type = INPUT_KEYBOARD;
	event.ki.wVk = 0;
	event.ki.wScan = code;
	event.ki.dwFlags = KEYEVENTF_UNICODE;
	event.ki.dwExtraInfo = 0;
	event.ki.time = 0;

	if (flags & KBD_FLAGS_RELEASE)
		event.ki.dwFlags |= KEYEVENTF_KEYUP;

	SendInput(1, &event, sizeof(INPUT));
}

void win_shadow_input_mouse_event(winShadowSubsystem* subsystem, UINT16 flags, UINT16 x, UINT16 y)
{
	INPUT event;
	float width;
	float height;

	ZeroMemory(&event, sizeof(INPUT));

	event.type = INPUT_MOUSE;

	if (flags & PTR_FLAGS_WHEEL)
	{
		event.mi.dwFlags = MOUSEEVENTF_WHEEL;
		event.mi.mouseData = flags & WheelRotationMask;

		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
			event.mi.mouseData *= -1;

		SendInput(1, &event, sizeof(INPUT));
	}
	else
	{
		width = (float) GetSystemMetrics(SM_CXSCREEN);
		height = (float) GetSystemMetrics(SM_CYSCREEN);

		event.mi.dx = (LONG) ((float) x * (65535.0f / width));
		event.mi.dy = (LONG) ((float) y * (65535.0f / height));
		event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE;

		if (flags & PTR_FLAGS_MOVE)
		{
			event.mi.dwFlags |= MOUSEEVENTF_MOVE;
			SendInput(1, &event, sizeof(INPUT));
		}

		event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE;

		if (flags & PTR_FLAGS_BUTTON1)
		{
			if (flags & PTR_FLAGS_DOWN)
				event.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
			else
				event.mi.dwFlags |= MOUSEEVENTF_LEFTUP;

			SendInput(1, &event, sizeof(INPUT));
		}
		else if (flags & PTR_FLAGS_BUTTON2)
		{
			if (flags & PTR_FLAGS_DOWN)
				event.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
			else
				event.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;

			SendInput(1, &event, sizeof(INPUT));
		}
		else if (flags & PTR_FLAGS_BUTTON3)
		{
			if (flags & PTR_FLAGS_DOWN)
				event.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
			else
				event.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;

			SendInput(1, &event, sizeof(INPUT));
		}
	}
}

void win_shadow_input_extended_mouse_event(winShadowSubsystem* subsystem, UINT16 flags, UINT16 x, UINT16 y)
{
	INPUT event;
	float width;
	float height;

	ZeroMemory(&event, sizeof(INPUT));

	if ((flags & PTR_XFLAGS_BUTTON1) || (flags & PTR_XFLAGS_BUTTON2))
	{
		event.type = INPUT_MOUSE;

		if (flags & PTR_FLAGS_MOVE)
		{
			width = (float) GetSystemMetrics(SM_CXSCREEN);
			height = (float) GetSystemMetrics(SM_CYSCREEN);

			event.mi.dx = (LONG)((float) x * (65535.0f / width));
			event.mi.dy = (LONG)((float) y * (65535.0f / height));
			event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;

			SendInput(1, &event, sizeof(INPUT));
		}

		event.mi.dx = event.mi.dy = event.mi.dwFlags = 0;

		if (flags & PTR_XFLAGS_DOWN)
			event.mi.dwFlags |= MOUSEEVENTF_XDOWN;
		else
			event.mi.dwFlags |= MOUSEEVENTF_XUP;

		if (flags & PTR_XFLAGS_BUTTON1)
			event.mi.mouseData = XBUTTON1;
		else if (flags & PTR_XFLAGS_BUTTON2)
			event.mi.mouseData = XBUTTON2;

		SendInput(1, &event, sizeof(INPUT));
	}
}

int win_shadow_surface_copy(winShadowSubsystem* subsystem)
{
	return 1;
}

void* win_shadow_subsystem_thread(winShadowSubsystem* subsystem)
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

int win_shadow_subsystem_init(winShadowSubsystem* subsystem)
{
	return 1;
}

int win_shadow_subsystem_uninit(winShadowSubsystem* subsystem)
{
	if (!subsystem)
		return -1;

	return 1;
}

int win_shadow_subsystem_start(winShadowSubsystem* subsystem)
{
	HANDLE thread;

	if (!subsystem)
		return -1;

	thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) win_shadow_subsystem_thread,
			(void*) subsystem, 0, NULL);

	return 1;
}

int win_shadow_subsystem_stop(winShadowSubsystem* subsystem)
{
	if (!subsystem)
		return -1;

	return 1;
}

void win_shadow_subsystem_free(winShadowSubsystem* subsystem)
{
	if (!subsystem)
		return;

	win_shadow_subsystem_uninit(subsystem);

	free(subsystem);
}

winShadowSubsystem* win_shadow_subsystem_new(rdpShadowServer* server)
{
	winShadowSubsystem* subsystem;

	subsystem = (winShadowSubsystem*) calloc(1, sizeof(winShadowSubsystem));

	if (!subsystem)
		return NULL;

	subsystem->server = server;

	subsystem->Init = (pfnShadowSubsystemInit) win_shadow_subsystem_init;
	subsystem->Uninit = (pfnShadowSubsystemInit) win_shadow_subsystem_uninit;
	subsystem->Start = (pfnShadowSubsystemStart) win_shadow_subsystem_start;
	subsystem->Stop = (pfnShadowSubsystemStop) win_shadow_subsystem_stop;
	subsystem->Free = (pfnShadowSubsystemFree) win_shadow_subsystem_free;

	subsystem->SurfaceCopy = (pfnShadowSurfaceCopy) win_shadow_surface_copy;

	subsystem->SynchronizeEvent = (pfnShadowSynchronizeEvent) win_shadow_input_synchronize_event;
	subsystem->KeyboardEvent = (pfnShadowKeyboardEvent) win_shadow_input_keyboard_event;
	subsystem->UnicodeKeyboardEvent = (pfnShadowUnicodeKeyboardEvent) win_shadow_input_unicode_keyboard_event;
	subsystem->MouseEvent = (pfnShadowMouseEvent) win_shadow_input_mouse_event;
	subsystem->ExtendedMouseEvent = (pfnShadowExtendedMouseEvent) win_shadow_input_extended_mouse_event;

	return subsystem;
}

rdpShadowSubsystem* Win_ShadowCreateSubsystem(rdpShadowServer* server)
{
	return (rdpShadowSubsystem*) win_shadow_subsystem_new(server);
}
