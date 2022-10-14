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

#include <windows.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/sysinfo.h>

#include <freerdp/log.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/region.h>

#include "win_shadow.h"

#define TAG SERVER_TAG("shadow.win")

/* https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-mouse_event
 * does not mention this flag is only supported if building for _WIN32_WINNT >= 0x0600
 */
#ifndef MOUSEEVENTF_HWHEEL
#define MOUSEEVENTF_HWHEEL 0x1000
#endif

static BOOL win_shadow_input_synchronize_event(rdpShadowSubsystem* subsystem,
                                               rdpShadowClient* client, UINT32 flags)
{
	WLog_WARN(TAG, "%s: TODO: Implement!", __FUNCTION__);
	return TRUE;
}

static BOOL win_shadow_input_keyboard_event(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
                                            UINT16 flags, UINT8 code)
{
	UINT rc;
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

	rc = SendInput(1, &event, sizeof(INPUT));
	if (rc == 0)
		return FALSE;
	return TRUE;
}

static BOOL win_shadow_input_unicode_keyboard_event(rdpShadowSubsystem* subsystem,
                                                    rdpShadowClient* client, UINT16 flags,
                                                    UINT16 code)
{
	UINT rc;
	INPUT event;
	event.type = INPUT_KEYBOARD;
	event.ki.wVk = 0;
	event.ki.wScan = code;
	event.ki.dwFlags = KEYEVENTF_UNICODE;
	event.ki.dwExtraInfo = 0;
	event.ki.time = 0;

	if (flags & KBD_FLAGS_RELEASE)
		event.ki.dwFlags |= KEYEVENTF_KEYUP;

	rc = SendInput(1, &event, sizeof(INPUT));
	if (rc == 0)
		return FALSE;
	return TRUE;
}

static BOOL win_shadow_input_mouse_event(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
                                         UINT16 flags, UINT16 x, UINT16 y)
{
	UINT rc = 1;
	INPUT event = { 0 };
	float width;
	float height;

	event.type = INPUT_MOUSE;

	if (flags & (PTR_FLAGS_WHEEL | PTR_FLAGS_HWHEEL))
	{
		if (flags & PTR_FLAGS_WHEEL)
			event.mi.dwFlags = MOUSEEVENTF_WHEEL;
		else
			event.mi.dwFlags = MOUSEEVENTF_HWHEEL;
		event.mi.mouseData = flags & WheelRotationMask;

		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
			event.mi.mouseData *= -1;

		rc = SendInput(1, &event, sizeof(INPUT));

		/* The build target is a system that did not support MOUSEEVENTF_HWHEEL
		 * but it may run on newer systems supporting it.
		 * Ignore the return value in these cases.
		 */
#if (_WIN32_WINNT < 0x0600)
		if (flags & PTR_FLAGS_HWHEEL)
			rc = 1;
#endif
	}
	else
	{
		width = (float)GetSystemMetrics(SM_CXSCREEN);
		height = (float)GetSystemMetrics(SM_CYSCREEN);
		event.mi.dx = (LONG)((float)x * (65535.0f / width));
		event.mi.dy = (LONG)((float)y * (65535.0f / height));
		event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE;

		if (flags & PTR_FLAGS_MOVE)
		{
			event.mi.dwFlags |= MOUSEEVENTF_MOVE;
			rc = SendInput(1, &event, sizeof(INPUT));
			if (rc == 0)
				return FALSE;
		}

		event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE;

		if (flags & PTR_FLAGS_BUTTON1)
		{
			if (flags & PTR_FLAGS_DOWN)
				event.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
			else
				event.mi.dwFlags |= MOUSEEVENTF_LEFTUP;

			rc = SendInput(1, &event, sizeof(INPUT));
		}
		else if (flags & PTR_FLAGS_BUTTON2)
		{
			if (flags & PTR_FLAGS_DOWN)
				event.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
			else
				event.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;

			rc = SendInput(1, &event, sizeof(INPUT));
		}
		else if (flags & PTR_FLAGS_BUTTON3)
		{
			if (flags & PTR_FLAGS_DOWN)
				event.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
			else
				event.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;

			rc = SendInput(1, &event, sizeof(INPUT));
		}
	}

	if (rc == 0)
		return FALSE;
	return TRUE;
}

static BOOL win_shadow_input_extended_mouse_event(rdpShadowSubsystem* subsystem,
                                                  rdpShadowClient* client, UINT16 flags, UINT16 x,
                                                  UINT16 y)
{
	UINT rc = 1;
	INPUT event = { 0 };
	float width;
	float height;

	if ((flags & PTR_XFLAGS_BUTTON1) || (flags & PTR_XFLAGS_BUTTON2))
	{
		event.type = INPUT_MOUSE;

		if (flags & PTR_FLAGS_MOVE)
		{
			width = (float)GetSystemMetrics(SM_CXSCREEN);
			height = (float)GetSystemMetrics(SM_CYSCREEN);
			event.mi.dx = (LONG)((float)x * (65535.0f / width));
			event.mi.dy = (LONG)((float)y * (65535.0f / height));
			event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
			rc = SendInput(1, &event, sizeof(INPUT));
			if (rc == 0)
				return FALSE;
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

		rc = SendInput(1, &event, sizeof(INPUT));
	}

	if (rc == 0)
		return FALSE;
	return TRUE;
}

static int win_shadow_invalidate_region(winShadowSubsystem* subsystem, int x, int y, int width,
                                        int height)
{
	rdpShadowServer* server;
	rdpShadowSurface* surface;
	RECTANGLE_16 invalidRect;
	server = subsystem->base.server;
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

static int win_shadow_surface_copy(winShadowSubsystem* subsystem)
{
	int x, y;
	int width;
	int height;
	int count;
	int status = 1;
	int nDstStep = 0;
	DWORD DstFormat;
	BYTE* pDstData = NULL;
	rdpShadowServer* server;
	rdpShadowSurface* surface;
	RECTANGLE_16 surfaceRect;
	RECTANGLE_16 invalidRect;
	const RECTANGLE_16* extents;
	server = subsystem->base.server;
	surface = server->surface;

	if (ArrayList_Count(server->clients) < 1)
		return 1;

	surfaceRect.left = surface->x;
	surfaceRect.top = surface->y;
	surfaceRect.right = surface->x + surface->width;
	surfaceRect.bottom = surface->y + surface->height;
	region16_intersect_rect(&(surface->invalidRegion), &(surface->invalidRegion), &surfaceRect);

	if (region16_is_empty(&(surface->invalidRegion)))
		return 1;

	extents = region16_extents(&(surface->invalidRegion));
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

	WLog_INFO(TAG, "SurfaceCopy x: %d y: %d width: %d height: %d right: %d bottom: %d", x, y, width,
	          height, x + width, y + height);
#if defined(WITH_WDS_API)
	{
		rdpGdi* gdi;
		shwContext* shw;
		rdpContext* context;

		WINPR_ASSERT(subsystem);
		shw = subsystem->shw;
		WINPR_ASSERT(shw);

		context = &shw->common.context;
		WINPR_ASSERT(context);

		gdi = context->gdi;
		WINPR_ASSERT(gdi);

		pDstData = gdi->primary_buffer;
		nDstStep = gdi->width * 4;
		DstFormat = gdi->dstFormat;
	}
#elif defined(WITH_DXGI_1_2)
	DstFormat = PIXEL_FORMAT_BGRX32;
	status = win_shadow_dxgi_fetch_frame_data(subsystem, &pDstData, &nDstStep, x, y, width, height);
#endif

	if (status <= 0)
		return status;

	if (!freerdp_image_copy(surface->data, surface->format, surface->scanline, x, y, width, height,
	                        pDstData, DstFormat, nDstStep, x, y, NULL, FREERDP_FLIP_NONE))
		return ERROR_INTERNAL_ERROR;

	ArrayList_Lock(server->clients);
	count = ArrayList_Count(server->clients);
	shadow_subsystem_frame_update(&subsystem->base);
	ArrayList_Unlock(server->clients);
	region16_clear(&(surface->invalidRegion));
	return 1;
}

#if defined(WITH_WDS_API)

static DWORD WINAPI win_shadow_subsystem_thread(LPVOID arg)
{
	winShadowSubsystem* subsystem = (winShadowSubsystem*)arg;
	DWORD status;
	DWORD nCount;
	HANDLE events[32];
	HANDLE StopEvent;
	StopEvent = subsystem->base.server->StopEvent;
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
	return 0;
}

#elif defined(WITH_DXGI_1_2)

static DWORD WINAPI win_shadow_subsystem_thread(LPVOID arg)
{
	winShadowSubsystem* subsystem = (winShadowSubsystem*)arg;
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
		dwTimeout = (DWORD)((cTime > frameTime) ? 0 : frameTime - cTime);
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
	return 0;
}

#endif

static UINT32 win_shadow_enum_monitors(MONITOR_DEF* monitors, UINT32 maxMonitors)
{
	HDC hdc;
	int index;
	int desktopWidth;
	int desktopHeight;
	DWORD iDevNum = 0;
	int numMonitors = 0;
	MONITOR_DEF* monitor;
	DISPLAY_DEVICE displayDevice = { 0 };

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

static int win_shadow_subsystem_init(rdpShadowSubsystem* arg)
{
	winShadowSubsystem* subsystem = (winShadowSubsystem*)arg;
	int status;
	MONITOR_DEF* virtualScreen;
	subsystem->base.numMonitors = win_shadow_enum_monitors(subsystem->base.monitors, 16);
#if defined(WITH_WDS_API)
	status = win_shadow_wds_init(subsystem);
#elif defined(WITH_DXGI_1_2)
	status = win_shadow_dxgi_init(subsystem);
#endif
	virtualScreen = &(subsystem->base.virtualScreen);
	virtualScreen->left = 0;
	virtualScreen->top = 0;
	virtualScreen->right = subsystem->width;
	virtualScreen->bottom = subsystem->height;
	virtualScreen->flags = 1;
	WLog_INFO(TAG, "width: %d height: %d", subsystem->width, subsystem->height);
	return 1;
}

static int win_shadow_subsystem_uninit(rdpShadowSubsystem* arg)
{
	winShadowSubsystem* subsystem = (winShadowSubsystem*)arg;

	if (!subsystem)
		return -1;

#if defined(WITH_WDS_API)
	win_shadow_wds_uninit(subsystem);
#elif defined(WITH_DXGI_1_2)
	win_shadow_dxgi_uninit(subsystem);
#endif
	return 1;
}

static int win_shadow_subsystem_start(rdpShadowSubsystem* arg)
{
	winShadowSubsystem* subsystem = (winShadowSubsystem*)arg;
	HANDLE thread;

	if (!subsystem)
		return -1;

	if (!(thread = CreateThread(NULL, 0, win_shadow_subsystem_thread, (void*)subsystem, 0, NULL)))
	{
		WLog_ERR(TAG, "Failed to create thread");
		return -1;
	}

	return 1;
}

static int win_shadow_subsystem_stop(rdpShadowSubsystem* arg)
{
	winShadowSubsystem* subsystem = (winShadowSubsystem*)arg;

	if (!subsystem)
		return -1;

	return 1;
}

static void win_shadow_subsystem_free(rdpShadowSubsystem* arg)
{
	winShadowSubsystem* subsystem = (winShadowSubsystem*)arg;

	if (!subsystem)
		return;

	win_shadow_subsystem_uninit(arg);
	free(subsystem);
}

static rdpShadowSubsystem* win_shadow_subsystem_new(void)
{
	winShadowSubsystem* subsystem;
	subsystem = (winShadowSubsystem*)calloc(1, sizeof(winShadowSubsystem));

	if (!subsystem)
		return NULL;

	subsystem->base.SynchronizeEvent = win_shadow_input_synchronize_event;
	subsystem->base.KeyboardEvent = win_shadow_input_keyboard_event;
	subsystem->base.UnicodeKeyboardEvent = win_shadow_input_unicode_keyboard_event;
	subsystem->base.MouseEvent = win_shadow_input_mouse_event;
	subsystem->base.ExtendedMouseEvent = win_shadow_input_extended_mouse_event;
	return &subsystem->base;
}

FREERDP_API int Win_ShadowSubsystemEntry(RDP_SHADOW_ENTRY_POINTS* pEntryPoints)
{
	pEntryPoints->New = win_shadow_subsystem_new;
	pEntryPoints->Free = win_shadow_subsystem_free;
	pEntryPoints->Init = win_shadow_subsystem_init;
	pEntryPoints->Uninit = win_shadow_subsystem_uninit;
	pEntryPoints->Start = win_shadow_subsystem_start;
	pEntryPoints->Stop = win_shadow_subsystem_stop;
	pEntryPoints->EnumMonitors = win_shadow_enum_monitors;
	return 1;
}
