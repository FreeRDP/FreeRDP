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

#include <freerdp/log.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/region.h>

#include "win_shadow.h"

#define TAG SERVER_TAG("shadow.win")

void win_shadow_input_synchronize_event(winShadowSubsystem* subsystem, rdpShadowClient* client, UINT32 flags)
{

}

void win_shadow_input_keyboard_event(winShadowSubsystem* subsystem, rdpShadowClient* client, UINT16 flags, UINT16 code)
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

void win_shadow_input_unicode_keyboard_event(winShadowSubsystem* subsystem, rdpShadowClient* client, UINT16 flags, UINT16 code)
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

void win_shadow_input_mouse_event(winShadowSubsystem* subsystem, rdpShadowClient* client, UINT16 flags, UINT16 x, UINT16 y)
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

void win_shadow_input_extended_mouse_event(winShadowSubsystem* subsystem, rdpShadowClient* client, UINT16 flags, UINT16 x, UINT16 y)
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


int win_shadow_invalidate_region(winShadowSubsystem* subsystem, int x, int y, int width, int height)
{
	rdpShadowServer* server;
	rdpShadowSurface* surface;
	RECTANGLE_16 invalidRect;

	server = subsystem->server;
	surface = server->surface;

	invalidRect.left = x;
	invalidRect.top = y;
	invalidRect.right = x + width;
	invalidRect.bottom = y + height;

	EnterCriticalSection(&(surface->lock));
	region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion), &invalidRect);
	LeaveCriticalSection(&(surface->lock));

	return 1;
}

int win_shadow_surface_copy(winShadowSubsystem* subsystem)
{
	int x, y;
	int width;
	int height;
	int count;
	int status = 1;
	int nDstStep = 0;
	BYTE* pDstData = NULL;
	rdpShadowServer* server;
	rdpShadowSurface* surface;
	RECTANGLE_16 surfaceRect;
	RECTANGLE_16 invalidRect;
	const RECTANGLE_16* extents;

	server = subsystem->server;
	surface = server->surface;

	if (ArrayList_Count(server->clients) < 1)
	{
		region16_clear(&(subsystem->invalidRegion));
		return 1;
	}

	surfaceRect.left = surface->x;
	surfaceRect.top = surface->y;
	surfaceRect.right = surface->x + surface->width;
	surfaceRect.bottom = surface->y + surface->height;

	region16_intersect_rect(&(subsystem->invalidRegion), &(subsystem->invalidRegion), &surfaceRect);

	if (region16_is_empty(&(subsystem->invalidRegion)))
		return 1;

	extents = region16_extents(&(subsystem->invalidRegion));
	CopyMemory(&invalidRect, extents, sizeof(RECTANGLE_16));

	shadow_capture_align_clip_rect(&invalidRect, &surfaceRect);

	x = invalidRect.left;
	y = invalidRect.top;
	width = invalidRect.right - invalidRect.left;
	height = invalidRect.bottom - invalidRect.top;

	if (0)
	{
		x = 0;
		y = 0;
		width = surface->width;
		height = surface->height;
	}

	WLog_INFO(TAG, "SurfaceCopy x: %d y: %d width: %d height: %d right: %d bottom: %d",
		x, y, width, height, x + width, y + height);

#if defined(WITH_WDS_API)
	{
		rdpGdi* gdi;
		shwContext* shw;
		rdpContext* context;

		shw = subsystem->shw;
		context = (rdpContext*) shw;
		gdi = context->gdi;

		pDstData = gdi->primary_buffer;
		nDstStep = gdi->width * 4;
	}
#elif defined(WITH_DXGI_1_2)
	status = win_shadow_dxgi_fetch_frame_data(subsystem, &pDstData, &nDstStep, x, y, width, height);
#endif

	if (status <= 0)
		return status;

	freerdp_image_copy(surface->data, PIXEL_FORMAT_XRGB32,
			surface->scanline, x - surface->x, y - surface->y, width, height,
			pDstData, PIXEL_FORMAT_XRGB32, nDstStep, 0, 0, NULL);

	ArrayList_Lock(server->clients);

	count = ArrayList_Count(server->clients);

	shadow_subsystem_frame_update((rdpShadowSubsystem *)subsystem);

	ArrayList_Unlock(server->clients);

	region16_clear(&(subsystem->invalidRegion));

	return 1;
}

#if defined(WITH_WDS_API)

void* win_shadow_subsystem_thread(winShadowSubsystem* subsystem)
{
	DWORD status;
	DWORD nCount;
	HANDLE events[32];
	HANDLE StopEvent;

	StopEvent = subsystem->server->StopEvent;

	nCount = 0;
	events[nCount++] = StopEvent;
	events[nCount++] = subsystem->RdpUpdateEnterEvent;

	while (1)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (WaitForSingleObject(StopEvent, 0) == WAIT_OBJECT_0)
		{
			break;
		}

		if (WaitForSingleObject(subsystem->RdpUpdateEnterEvent, 0) == WAIT_OBJECT_0)
		{
			win_shadow_surface_copy(subsystem);
			ResetEvent(subsystem->RdpUpdateEnterEvent);
			SetEvent(subsystem->RdpUpdateLeaveEvent);
		}
	}

	ExitThread(0);
	return NULL;
}

#elif defined(WITH_DXGI_1_2)

void* win_shadow_subsystem_thread(winShadowSubsystem* subsystem)
{
	int fps;
	DWORD status;
	DWORD nCount;
	UINT64 cTime;
	DWORD dwTimeout;
	DWORD dwInterval;
	UINT64 frameTime;
	HANDLE events[32];
	HANDLE StopEvent;

	StopEvent = subsystem->server->StopEvent;

	nCount = 0;
	events[nCount++] = StopEvent;

	fps = 16;
	dwInterval = 1000 / fps;
	frameTime = GetTickCount64() + dwInterval;

	while (1)
	{
		dwTimeout = INFINITE;

		cTime = GetTickCount64();
		dwTimeout = (DWORD) ((cTime > frameTime) ? 0 : frameTime - cTime);

		status = WaitForMultipleObjects(nCount, events, FALSE, dwTimeout);

		if (WaitForSingleObject(StopEvent, 0) == WAIT_OBJECT_0)
		{
			break;
		}

		if ((status == WAIT_TIMEOUT) || (GetTickCount64() > frameTime))
		{
			int dxgi_status;

			dxgi_status = win_shadow_dxgi_get_next_frame(subsystem);
			
			if (dxgi_status > 0)
				dxgi_status = win_shadow_dxgi_get_invalid_region(subsystem);

			if (dxgi_status > 0)
				win_shadow_surface_copy(subsystem);

			dwInterval = 1000 / fps;
			frameTime += dwInterval;
		}
	}

	ExitThread(0);
	return NULL;
}

#endif

int win_shadow_enum_monitors(MONITOR_DEF* monitors, int maxMonitors)
{
	HDC hdc;
	int index;
	int desktopWidth;
	int desktopHeight;
	DWORD iDevNum = 0;
	int numMonitors = 0;
	MONITOR_DEF* monitor;
	DISPLAY_DEVICE displayDevice;

	ZeroMemory(&displayDevice, sizeof(DISPLAY_DEVICE));
	displayDevice.cb = sizeof(DISPLAY_DEVICE);

	if (EnumDisplayDevices(NULL, iDevNum, &displayDevice, 0))
	{
		hdc = CreateDC(displayDevice.DeviceName, NULL, NULL, NULL);

		desktopWidth = GetDeviceCaps(hdc, HORZRES);
		desktopHeight = GetDeviceCaps(hdc, VERTRES);

		index = 0;
		numMonitors = 1;

		monitor = &monitors[index];

		monitor->left = 0;
		monitor->top = 0;
		monitor->right = desktopWidth;
		monitor->bottom = desktopHeight;
		monitor->flags = 1;

		DeleteDC(hdc);
	}

	return numMonitors;
}

int win_shadow_subsystem_init(winShadowSubsystem* subsystem)
{
	int status;
	MONITOR_DEF* virtualScreen;

	subsystem->numMonitors = win_shadow_enum_monitors(subsystem->monitors, 16);

#if defined(WITH_WDS_API)
	status = win_shadow_wds_init(subsystem);
#elif defined(WITH_DXGI_1_2)
	status = win_shadow_dxgi_init(subsystem);
#endif

	virtualScreen = &(subsystem->virtualScreen);

	virtualScreen->left = 0;
	virtualScreen->top = 0;
	virtualScreen->right = subsystem->width;
	virtualScreen->bottom = subsystem->height;
	virtualScreen->flags = 1;

	WLog_INFO(TAG, "width: %d height: %d", subsystem->width, subsystem->height);

	return 1;
}

int win_shadow_subsystem_uninit(winShadowSubsystem* subsystem)
{
	if (!subsystem)
		return -1;

#if defined(WITH_WDS_API)
	win_shadow_wds_uninit(subsystem);
#elif defined(WITH_DXGI_1_2)
	win_shadow_dxgi_uninit(subsystem);
#endif

	return 1;
}

int win_shadow_subsystem_start(winShadowSubsystem* subsystem)
{
	HANDLE thread;

	if (!subsystem)
		return -1;

	if (!(thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) win_shadow_subsystem_thread,
			(void*) subsystem, 0, NULL)))
	{
		WLog_ERR(TAG, "Failed to create thread");
		return -1;
	}

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

winShadowSubsystem* win_shadow_subsystem_new()
{
	winShadowSubsystem* subsystem;

	subsystem = (winShadowSubsystem*) calloc(1, sizeof(winShadowSubsystem));

	if (!subsystem)
		return NULL;

	subsystem->SynchronizeEvent = (pfnShadowSynchronizeEvent) win_shadow_input_synchronize_event;
	subsystem->KeyboardEvent = (pfnShadowKeyboardEvent) win_shadow_input_keyboard_event;
	subsystem->UnicodeKeyboardEvent = (pfnShadowUnicodeKeyboardEvent) win_shadow_input_unicode_keyboard_event;
	subsystem->MouseEvent = (pfnShadowMouseEvent) win_shadow_input_mouse_event;
	subsystem->ExtendedMouseEvent = (pfnShadowExtendedMouseEvent) win_shadow_input_extended_mouse_event;

	return subsystem;
}

FREERDP_API int Win_ShadowSubsystemEntry(RDP_SHADOW_ENTRY_POINTS* pEntryPoints)
{
	pEntryPoints->New = (pfnShadowSubsystemNew) win_shadow_subsystem_new;
	pEntryPoints->Free = (pfnShadowSubsystemFree) win_shadow_subsystem_free;

	pEntryPoints->Init = (pfnShadowSubsystemInit) win_shadow_subsystem_init;
	pEntryPoints->Uninit = (pfnShadowSubsystemInit) win_shadow_subsystem_uninit;

	pEntryPoints->Start = (pfnShadowSubsystemStart) win_shadow_subsystem_start;
	pEntryPoints->Stop = (pfnShadowSubsystemStop) win_shadow_subsystem_stop;

	pEntryPoints->EnumMonitors = (pfnShadowEnumMonitors) win_shadow_enum_monitors;

	return 1;
}
