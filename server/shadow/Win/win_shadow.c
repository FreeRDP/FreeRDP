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
#include <freerdp/server/server-common.h>

#include "win_shadow.h"
#include "win_sas.h"

#define TAG SERVER_TAG("shadow.win")

/* https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-mouse_event
 * does not mention this flag is only supported if building for _WIN32_WINNT >= 0x0600
 */
#ifndef MOUSEEVENTF_HWHEEL
#define MOUSEEVENTF_HWHEEL 0x1000
#endif

/* The desktop this thread opened to follow the input desktop, if any */
static WINPR_TLS HDESK win_shadow_thread_desktop = nullptr;

/* Moves the calling thread to the input desktop when that changed: SendInput
 * and DXGI desktop duplication only work there. The input desktop is the
 * user's (Default) or, as SYSTEM on the console session (the service mode),
 * the secure one (Winlogon: the logon and lock screens, UAC prompts). A user
 * process cannot open the secure desktop and stays where it is. Returns TRUE
 * when the thread moved. */
static BOOL win_shadow_follow_input_desktop(void)
{
	WCHAR inputName[64] = WINPR_C_ARRAY_INIT;
	WCHAR threadName[64] = WINPR_C_ARRAY_INIT;
	HDESK input = OpenInputDesktop(0, FALSE, GENERIC_ALL);

	if (!input)
		return FALSE;

	(void)GetUserObjectInformationW(input, UOI_NAME, inputName, sizeof(inputName), nullptr);
	(void)GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()), UOI_NAME, threadName,
	                                sizeof(threadName), nullptr);

	if (_wcsicmp(inputName, threadName) == 0)
	{
		(void)CloseDesktop(input);
		return FALSE;
	}

	if (!SetThreadDesktop(input))
	{
		WLog_WARN(TAG, "SetThreadDesktop(%ls) failed: 0x%08lX", inputName, GetLastError());
		(void)CloseDesktop(input);
		return FALSE;
	}

	/* the desktop GetThreadDesktop() gave first is not ours to close */
	if (win_shadow_thread_desktop)
		(void)CloseDesktop(win_shadow_thread_desktop);
	win_shadow_thread_desktop = input;
	WLog_INFO(TAG, "thread %lu follows the input desktop from %ls to %ls", GetCurrentThreadId(),
	          threadName, inputName);
	return TRUE;
}

/* SendInput, without ending the session when Windows refuses the event: it
 * does while the input desktop switches (the secure desktop comes or goes)
 * and for a window above our integrity level, and FreeRDP drops the client
 * when an input handler fails */
static void win_shadow_send_input(INPUT* event)
{
	if (SendInput(1, event, sizeof(INPUT)) == 0)
		WLog_WARN(TAG, "SendInput refused the event (0x%08lX), it is dropped", GetLastError());
}

/* Ctrl+Alt+Del: asks freerdp-shadow-service for the secure attention
 * sequence (win_sas.h) */
static void win_shadow_send_sas(void)
{
	HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, WIN_SHADOW_SAS_EVENT);

	if (!event)
	{
		WLog_WARN(TAG, "Ctrl+Alt+Del: no freerdp-shadow-service to send it (0x%08lX)",
		          GetLastError());
		return;
	}

	(void)SetEvent(event);
	(void)CloseHandle(event);
	WLog_INFO(TAG, "Ctrl+Alt+Del: asked freerdp-shadow-service for the secure attention sequence");
}

static BOOL win_shadow_input_synchronize_event(rdpShadowSubsystem* subsystem,
                                               rdpShadowClient* client, UINT32 flags)
{
	WLog_WARN(TAG, "TODO: Implement!");
	return TRUE;
}

static BOOL win_shadow_input_keyboard_event(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
                                            UINT16 flags, UINT8 code)
{
	winShadowSubsystem* win = (winShadowSubsystem*)subsystem;
	const BOOL down = (flags & KBD_FLAGS_RELEASE) == 0;

	(void)win_shadow_follow_input_desktop();

	/* scan codes: Ctrl 0x1D, Alt 0x38 (the right ones extended), Delete
	 * 0x53 (the keypad's not extended); Windows takes either Delete */
	if (code == 0x1D)
		win->ctrlDown = down;
	else if (code == 0x38)
		win->altDown = down;
	else if ((code == 0x53) && win->ctrlDown && win->altDown)
	{
		if (down)
			win_shadow_send_sas();
		return TRUE; /* never typed into the desktop */
	}
	INPUT event = WINPR_C_ARRAY_INIT;
	event.type = INPUT_KEYBOARD;
	KEYBDINPUT* ki =
#if defined(NONAMELESSUNION)
	    &event.u.ki;
#else
	    &event.ki;
#endif

	ki->wVk = 0;
	ki->wScan = code;
	ki->dwFlags = KEYEVENTF_SCANCODE;
	ki->dwExtraInfo = 0;
	ki->time = 0;

	if (flags & KBD_FLAGS_RELEASE)
		ki->dwFlags |= KEYEVENTF_KEYUP;

	if (flags & KBD_FLAGS_EXTENDED)
		ki->dwFlags |= KEYEVENTF_EXTENDEDKEY;

	win_shadow_send_input(&event);
	return TRUE;
}

static BOOL win_shadow_input_unicode_keyboard_event(rdpShadowSubsystem* subsystem,
                                                    rdpShadowClient* client, UINT16 flags,
                                                    UINT16 code)
{
	(void)win_shadow_follow_input_desktop();
	INPUT event = WINPR_C_ARRAY_INIT;
	event.type = INPUT_KEYBOARD;
	KEYBDINPUT* ki =
#if defined(NONAMELESSUNION)
	    &event.u.ki;
#else
	    &event.ki;
#endif
	ki->wVk = 0;
	ki->wScan = code;
	ki->dwFlags = KEYEVENTF_UNICODE;
	ki->dwExtraInfo = 0;
	ki->time = 0;

	if (flags & KBD_FLAGS_RELEASE)
		ki->dwFlags |= KEYEVENTF_KEYUP;

	win_shadow_send_input(&event);
	return TRUE;
}

static BOOL win_shadow_input_mouse_event(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
                                         UINT16 flags, UINT16 x, UINT16 y)
{
	(void)win_shadow_follow_input_desktop();
	INPUT event = WINPR_C_ARRAY_INIT;
	float width;
	float height;
	MOUSEINPUT* mi =
#if defined(NONAMELESSUNION)
	    &event.u.mi;
#else
	    &event.mi;
#endif

	event.type = INPUT_MOUSE;

	if (flags & (PTR_FLAGS_WHEEL | PTR_FLAGS_HWHEEL))
	{
		if (flags & PTR_FLAGS_WHEEL)
			mi->dwFlags = MOUSEEVENTF_WHEEL;
		else
			mi->dwFlags = MOUSEEVENTF_HWHEEL;
		mi->mouseData = flags & WheelRotationMask;

		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
			mi->mouseData *= -1;

		win_shadow_send_input(&event);
	}
	else
	{
		width = (float)GetSystemMetrics(SM_CXSCREEN);
		height = (float)GetSystemMetrics(SM_CYSCREEN);
		mi->dx = (LONG)((float)x * (65535.0f / width));
		mi->dy = (LONG)((float)y * (65535.0f / height));
		mi->dwFlags = MOUSEEVENTF_ABSOLUTE;

		if (flags & PTR_FLAGS_MOVE)
		{
			mi->dwFlags |= MOUSEEVENTF_MOVE;
			win_shadow_send_input(&event);
		}

		mi->dwFlags = MOUSEEVENTF_ABSOLUTE;

		if (flags & PTR_FLAGS_BUTTON1)
		{
			if (flags & PTR_FLAGS_DOWN)
				mi->dwFlags |= MOUSEEVENTF_LEFTDOWN;
			else
				mi->dwFlags |= MOUSEEVENTF_LEFTUP;

			win_shadow_send_input(&event);
		}
		else if (flags & PTR_FLAGS_BUTTON2)
		{
			if (flags & PTR_FLAGS_DOWN)
				mi->dwFlags |= MOUSEEVENTF_RIGHTDOWN;
			else
				mi->dwFlags |= MOUSEEVENTF_RIGHTUP;

			win_shadow_send_input(&event);
		}
		else if (flags & PTR_FLAGS_BUTTON3)
		{
			if (flags & PTR_FLAGS_DOWN)
				mi->dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
			else
				mi->dwFlags |= MOUSEEVENTF_MIDDLEUP;

			win_shadow_send_input(&event);
		}
	}

	return TRUE;
}

static BOOL win_shadow_input_extended_mouse_event(rdpShadowSubsystem* subsystem,
                                                  rdpShadowClient* client, UINT16 flags, UINT16 x,
                                                  UINT16 y)
{
	(void)win_shadow_follow_input_desktop();
	INPUT event = WINPR_C_ARRAY_INIT;
	float width;
	float height;
	MOUSEINPUT* mi =
#if defined(NONAMELESSUNION)
	    &event.u.mi;
#else
	    &event.mi;
#endif

	if ((flags & PTR_XFLAGS_BUTTON1) || (flags & PTR_XFLAGS_BUTTON2))
	{
		event.type = INPUT_MOUSE;

		if (flags & PTR_FLAGS_MOVE)
		{
			width = (float)GetSystemMetrics(SM_CXSCREEN);
			height = (float)GetSystemMetrics(SM_CYSCREEN);
			mi->dx = (LONG)((float)x * (65535.0f / width));
			mi->dy = (LONG)((float)y * (65535.0f / height));
			mi->dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
			win_shadow_send_input(&event);
		}

		mi->dx = mi->dy = mi->dwFlags = 0;

		if (flags & PTR_XFLAGS_DOWN)
			mi->dwFlags |= MOUSEEVENTF_XDOWN;
		else
			mi->dwFlags |= MOUSEEVENTF_XUP;

		if (flags & PTR_XFLAGS_BUTTON1)
			mi->mouseData = XBUTTON1;
		else if (flags & PTR_XFLAGS_BUTTON2)
			mi->mouseData = XBUTTON2;

		win_shadow_send_input(&event);
	}

	return TRUE;
}

static int win_shadow_invalidate_region(winShadowSubsystem* subsystem, int x, int y, int width,
                                        int height)
{
	rdpShadowServer* server = subsystem->base.server;
	rdpShadowSurface* surface = server->surface;
	RECTANGLE_16 invalidRect = { .left = x, .top = y, .right = x + width, .bottom = y + height };
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
	int status = 1;
	int nDstStep = 0;
	DWORD DstFormat;
	BYTE* pDstData = nullptr;
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

#if defined(WITH_WDS_API)
	/* WDS: the GDI primary buffer spans the whole desktop */
	const int srcX = x;
	const int srcY = y;
#elif defined(WITH_DXGI_1_2)
	/* DXGI: fetch_frame_data() copies the region to the staging texture
	 * origin, so the source offset is always the origin */
	const int srcX = 0;
	const int srcY = 0;
#else
	/* no capture backend compiled in: keep the previous behavior */
	const int srcX = x;
	const int srcY = y;
#endif
	if (!freerdp_image_copy_no_overlap(surface->data, surface->format, surface->scanline, x, y,
	                                   width, height, pDstData, DstFormat, nDstStep, srcX, srcY,
	                                   nullptr, FREERDP_FLIP_NONE))
		return ERROR_INTERNAL_ERROR;

	ArrayList_Lock(server->clients);
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
			(void)ResetEvent(subsystem->RdpUpdateEnterEvent);
			(void)SetEvent(subsystem->RdpUpdateLeaveEvent);
		}
	}

	ExitThread(0);
	return 0;
}

#elif defined(WITH_DXGI_1_2)

static DWORD WINAPI win_shadow_subsystem_thread(LPVOID arg)
{
	winShadowSubsystem* subsystem = (winShadowSubsystem*)arg;
	DWORD status;
	DWORD nCount;
	UINT64 cTime;
	DWORD dwTimeout;
	DWORD dwInterval;
	UINT64 frameTime;
	HANDLE events[32];
	HANDLE StopEvent;
	StopEvent = subsystem->base.server->StopEvent;
	wMessagePipe* MsgPipe = subsystem->base.MsgPipe;
	nCount = 0;
	events[nCount++] = StopEvent;
	events[nCount++] = MessageQueue_Event(MsgPipe->In);
	subsystem->base.captureFrameRate = 120;
	dwInterval = 1000 / subsystem->base.captureFrameRate;
	frameTime = GetTickCount64() + dwInterval;
	BOOL refresh = FALSE;

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

		/* a client asks for the whole screen (its graphics channel is
		 * ready, or it asked to refresh), as the X11 subsystem answers */
		wMessage message = WINPR_C_ARRAY_INIT;
		BOOL quit = FALSE;
		while (!quit && MessageQueue_Peek(MsgPipe->In, &message, TRUE))
		{
			quit = (message.id == WMQ_QUIT);
			if (message.id == SHADOW_MSG_IN_REFRESH_REQUEST_ID)
				refresh = TRUE;
			if (message.Free)
				message.Free(&message);
		}
		if (quit)
			break;

		if ((status == WAIT_TIMEOUT) || (GetTickCount64() > frameTime))
		{
			int dxgi_status = 0;

			/* a desktop switch (logon, lock screen, UAC), a refresh request,
			 * or no duplication yet: duplicate the output again on the input
			 * desktop, and send all of it. DXGI reports changes only: on a
			 * desktop that does not change, a new client would otherwise wait
			 * for its first picture until something moves; a new duplication
			 * starts with the whole desktop image */
			if (win_shadow_follow_input_desktop() || refresh || !subsystem->dxgiOutputDuplication)
			{
				refresh = FALSE;
				if (win_shadow_dxgi_restart_duplication(subsystem) > 0)
					(void)win_shadow_invalidate_region(subsystem, 0, 0, subsystem->width,
					                                   subsystem->height);
			}

			if (subsystem->dxgiOutputDuplication)
				dxgi_status = win_shadow_dxgi_get_next_frame(subsystem);

			/* a frame without dirty rectangles of its own (the first of a
			 * new duplication) still holds the whole desktop image: copy
			 * what is marked invalid already */
			if (dxgi_status > 0)
			{
				const int changed = win_shadow_dxgi_get_invalid_region(subsystem);
				rdpShadowSurface* surface = subsystem->base.server->surface;
				EnterCriticalSection(&(surface->lock));
				const BOOL pending = !region16_is_empty(&(surface->invalidRegion));
				LeaveCriticalSection(&(surface->lock));
				dxgi_status = ((changed > 0) || ((changed == 0) && pending)) ? 1 : changed;
			}

			if (dxgi_status > 0)
				win_shadow_surface_copy(subsystem);

			/* Follow the encoder capture rate suggestion, like the X11 and Mac
			 * subsystems do: shadow_encoder_create_frame_id() lowers the
			 * preferred fps while frames are still in flight (not yet
			 * acknowledged by the client). This subsystem hardcoded the
			 * capture rate and never read the suggestion, so a slow client
			 * could not throttle the capture and in-flight frames (latency)
			 * accumulated without bound. Use the lowest suggestion of all
			 * connected clients. */
			UINT32 captureFrameRate = UINT32_MAX;
			ArrayList_Lock(subsystem->base.server->clients);
			const UINT32 clientCount = ArrayList_Count(subsystem->base.server->clients);
			for (UINT32 index = 0; index < clientCount; index++)
			{
				const rdpShadowClient* client =
				    (rdpShadowClient*)ArrayList_GetItem(subsystem->base.server->clients, index);

				if (client && client->encoder)
				{
					const UINT32 suggestion = shadow_encoder_preferred_fps(client->encoder);
					if (suggestion < captureFrameRate)
						captureFrameRate = suggestion;
				}
			}
			ArrayList_Unlock(subsystem->base.server->clients);

			if ((captureFrameRate == UINT32_MAX) || (captureFrameRate < 1))
				captureFrameRate = subsystem->base.captureFrameRate;
			subsystem->base.captureFrameRate = captureFrameRate;

			dwInterval = 1000 / subsystem->base.captureFrameRate;
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
	DISPLAY_DEVICE displayDevice = WINPR_C_ARRAY_INIT;

	displayDevice.cb = sizeof(DISPLAY_DEVICE);

	/* Skip adapters which are not attached to the desktop: on machines with
	 * a BMC/management GPU (e.g. ASPEED) listed as device 0 while the real GPU
	 * is attached later in the list, CreateDC for the unattached device fails
	 * and the monitor rectangle stays zeroed, which results in a 1x1 surface. */
	{
		DWORD devNum = 0;
		BOOL attached = FALSE;
		DISPLAY_DEVICE probe = WINPR_C_ARRAY_INIT;
		probe.cb = sizeof(DISPLAY_DEVICE);
		while (EnumDisplayDevices(nullptr, devNum, &probe, 0))
		{
			if (probe.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
			{
				attached = TRUE;
				break;
			}
			devNum++;
			probe.cb = sizeof(DISPLAY_DEVICE);
		}
		if (attached && (devNum != 0))
		{
			displayDevice.cb = sizeof(DISPLAY_DEVICE);
			if (EnumDisplayDevices(nullptr, devNum, &displayDevice, 0))
				iDevNum = devNum;
		}
	}

	if (EnumDisplayDevices(nullptr, iDevNum, &displayDevice, 0))
	{
		hdc = CreateDC(displayDevice.DeviceName, nullptr, nullptr, nullptr);
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

	/* Fallback to the primary screen metrics if enumeration did not
	 * yield a usable rectangle, otherwise shadow_screen_new computes a
	 * 1x1 surface and clients are resized to 1x1. */
	if ((numMonitors == 0) || (monitors[0].right <= 0) || (monitors[0].bottom <= 0))
	{
		monitor = &monitors[0];
		monitor->left = 0;
		monitor->top = 0;
		monitor->right = GetSystemMetrics(SM_CXSCREEN);
		monitor->bottom = GetSystemMetrics(SM_CYSCREEN);
		monitor->flags = 1;
		numMonitors = 1;
	}

	return numMonitors;
}

static int win_shadow_subsystem_init(rdpShadowSubsystem* arg)
{
	winShadowSubsystem* subsystem = (winShadowSubsystem*)arg;
	int status;
	MONITOR_DEF* virtualScreen;
	subsystem->base.numMonitors = win_shadow_enum_monitors(subsystem->base.monitors, 16);
	if (subsystem->base.numMonitors > 0)
	{
		subsystem->width = subsystem->base.monitors[0].right;
		subsystem->height = subsystem->base.monitors[0].bottom;
	}

	if ((subsystem->width <= 0) || (subsystem->height <= 0))
	{
		subsystem->width = GetSystemMetrics(SM_CXSCREEN);
		subsystem->height = GetSystemMetrics(SM_CYSCREEN);
	}
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

	if (!(thread =
	          CreateThread(nullptr, 0, win_shadow_subsystem_thread, (void*)subsystem, 0, nullptr)))
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
		return nullptr;

	subsystem->base.SynchronizeEvent = win_shadow_input_synchronize_event;
	subsystem->base.KeyboardEvent = win_shadow_input_keyboard_event;
	subsystem->base.UnicodeKeyboardEvent = win_shadow_input_unicode_keyboard_event;
	subsystem->base.MouseEvent = win_shadow_input_mouse_event;
	subsystem->base.ExtendedMouseEvent = win_shadow_input_extended_mouse_event;
	return &subsystem->base;
}

FREERDP_API const char* ShadowSubsystemName(void)
{
	return "Win";
}

FREERDP_API int ShadowSubsystemEntry(RDP_SHADOW_ENTRY_POINTS* pEntryPoints)
{
	char name[] = "windows shadow subsystem";
	char* arg[] = { name };

	freerdp_server_warn_unmaintained(ARRAYSIZE(arg), arg);
	pEntryPoints->New = win_shadow_subsystem_new;
	pEntryPoints->Free = win_shadow_subsystem_free;
	pEntryPoints->Init = win_shadow_subsystem_init;
	pEntryPoints->Uninit = win_shadow_subsystem_uninit;
	pEntryPoints->Start = win_shadow_subsystem_start;
	pEntryPoints->Stop = win_shadow_subsystem_stop;
	pEntryPoints->EnumMonitors = win_shadow_enum_monitors;
	return 1;
}
