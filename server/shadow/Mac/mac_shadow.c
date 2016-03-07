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
#include <winpr/input.h>
#include <winpr/sysinfo.h>

#include <freerdp/codec/color.h>
#include <freerdp/codec/region.h>
#include <freerdp/log.h>

#include "mac_shadow.h"

#define TAG SERVER_TAG("shadow.mac")


static macShadowSubsystem* g_Subsystem = NULL;

void mac_shadow_input_synchronize_event(macShadowSubsystem* subsystem, rdpShadowClient* client, UINT32 flags)
{

}

void mac_shadow_input_keyboard_event(macShadowSubsystem* subsystem, rdpShadowClient* client, UINT16 flags, UINT16 code)
{
	DWORD vkcode;
	DWORD keycode;
	BOOL extended;
	CGEventRef kbdEvent;
	CGEventSourceRef source;
	
	extended = (flags & KBD_FLAGS_EXTENDED) ? TRUE : FALSE;
	
	if (extended)
		code |= KBDEXT;
	
	vkcode = GetVirtualKeyCodeFromVirtualScanCode(code, 4);
	
	if (extended)
		vkcode |= KBDEXT;
	
	keycode = GetKeycodeFromVirtualKeyCode(vkcode, KEYCODE_TYPE_APPLE);
	
	if (keycode < 8)
		return;
	
	keycode -= 8;

	source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
	
	if (flags & KBD_FLAGS_DOWN)
	{
		kbdEvent = CGEventCreateKeyboardEvent(source, (CGKeyCode) keycode, TRUE);
		CGEventPost(kCGHIDEventTap, kbdEvent);
		CFRelease(kbdEvent);
	}
	else if (flags & KBD_FLAGS_RELEASE)
	{
		kbdEvent = CGEventCreateKeyboardEvent(source, (CGKeyCode) keycode, FALSE);
		CGEventPost(kCGHIDEventTap, kbdEvent);
		CFRelease(kbdEvent);
	}
	
	CFRelease(source);
}

void mac_shadow_input_unicode_keyboard_event(macShadowSubsystem* subsystem, rdpShadowClient* client, UINT16 flags, UINT16 code)
{

}

void mac_shadow_input_mouse_event(macShadowSubsystem* subsystem, rdpShadowClient* client, UINT16 flags, UINT16 x, UINT16 y)
{
	UINT32 scrollX = 0;
	UINT32 scrollY = 0;
	CGWheelCount wheelCount = 2;
	
	if (flags & PTR_FLAGS_WHEEL)
	{
		scrollY = flags & WheelRotationMask;
		
		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
		{
			scrollY = -(flags & WheelRotationMask) / 392;
		}
		else
		{
			scrollY = (flags & WheelRotationMask) / 120;
		}
		
		CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
		CGEventRef scroll = CGEventCreateScrollWheelEvent(source, kCGScrollEventUnitLine,
								  wheelCount, scrollY, scrollX);
		CGEventPost(kCGHIDEventTap, scroll);
		
		CFRelease(scroll);
		CFRelease(source);
	}
	else
	{
		CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
		CGEventType mouseType = kCGEventNull;
		CGMouseButton mouseButton = kCGMouseButtonLeft;
		
		if (flags & PTR_FLAGS_MOVE)
		{
			if (subsystem->mouseDownLeft)
				mouseType = kCGEventLeftMouseDragged;
			else if (subsystem->mouseDownRight)
				mouseType = kCGEventRightMouseDragged;
			else if (subsystem->mouseDownOther)
				mouseType = kCGEventOtherMouseDragged;
			else
				mouseType = kCGEventMouseMoved;
			
			CGEventRef move = CGEventCreateMouseEvent(source, mouseType, CGPointMake(x, y), mouseButton);
			CGEventPost(kCGHIDEventTap, move);
			CFRelease(move);
		}
		
		if (flags & PTR_FLAGS_BUTTON1)
		{
			mouseButton = kCGMouseButtonLeft;
			
			if (flags & PTR_FLAGS_DOWN)
			{
				mouseType = kCGEventLeftMouseDown;
				subsystem->mouseDownLeft = TRUE;
			}
			else
			{
				mouseType = kCGEventLeftMouseUp;
				subsystem->mouseDownLeft = FALSE;
			}
		}
		else if (flags & PTR_FLAGS_BUTTON2)
		{
			mouseButton = kCGMouseButtonRight;
			
			if (flags & PTR_FLAGS_DOWN)
			{
				mouseType = kCGEventRightMouseDown;
				subsystem->mouseDownRight = TRUE;
			}
			else
			{
				mouseType = kCGEventRightMouseUp;
				subsystem->mouseDownRight = FALSE;
			}
			
		}
		else if (flags & PTR_FLAGS_BUTTON3)
		{
			mouseButton = kCGMouseButtonCenter;
			
			if (flags & PTR_FLAGS_DOWN)
			{
				mouseType = kCGEventOtherMouseDown;
				subsystem->mouseDownOther = TRUE;
			}
			else
			{
				mouseType = kCGEventOtherMouseUp;
				subsystem->mouseDownOther = FALSE;
			}
		}
		
		CGEventRef mouseEvent = CGEventCreateMouseEvent(source, mouseType, CGPointMake(x, y), mouseButton);
		CGEventPost(kCGHIDEventTap, mouseEvent);
		
		CFRelease(mouseEvent);
		CFRelease(source);
	}
}

void mac_shadow_input_extended_mouse_event(macShadowSubsystem* subsystem, rdpShadowClient* client, UINT16 flags, UINT16 x, UINT16 y)
{

}

int mac_shadow_detect_monitors(macShadowSubsystem* subsystem)
{
	size_t wide, high;
	MONITOR_DEF* monitor;
	CGDirectDisplayID displayId;
	
	displayId = CGMainDisplayID();
	
	CGDisplayModeRef mode = CGDisplayCopyDisplayMode(displayId);
	
	subsystem->pixelWidth = CGDisplayModeGetPixelWidth(mode);
	subsystem->pixelHeight = CGDisplayModeGetPixelHeight(mode);
	
	wide = CGDisplayPixelsWide(displayId);
	high = CGDisplayPixelsHigh(displayId);
	
	CGDisplayModeRelease(mode);
	
	subsystem->retina = ((subsystem->pixelWidth / wide) == 2) ? TRUE : FALSE;
	
	if (subsystem->retina)
	{
		subsystem->width = wide;
		subsystem->height = high;
	}
	else
	{
		subsystem->width = subsystem->pixelWidth;
		subsystem->height = subsystem->pixelHeight;
	}
	
	subsystem->numMonitors = 1;
	
	monitor = &(subsystem->monitors[0]);
	
	monitor->left = 0;
	monitor->top = 0;
	monitor->right = subsystem->width;
	monitor->bottom = subsystem->height;
	monitor->flags = 1;
	
	return 1;
}

int mac_shadow_capture_start(macShadowSubsystem* subsystem)
{
	CGError err;
	
	err = CGDisplayStreamStart(subsystem->stream);
	
	if (err != kCGErrorSuccess)
		return -1;
	
	return 1;
}

int mac_shadow_capture_stop(macShadowSubsystem* subsystem)
{
	CGError err;
	
	err = CGDisplayStreamStop(subsystem->stream);
	
	if (err != kCGErrorSuccess)
		return -1;
	
	return 1;
}

int mac_shadow_capture_get_dirty_region(macShadowSubsystem* subsystem)
{
	size_t index;
	size_t numRects;
	const CGRect* rects;
	RECTANGLE_16 invalidRect;
	
	rects = CGDisplayStreamUpdateGetRects(subsystem->lastUpdate, kCGDisplayStreamUpdateDirtyRects, &numRects);
	
	if (!numRects)
		return -1;
	
	for (index = 0; index < numRects; index++)
	{
		invalidRect.left = (UINT16) rects[index].origin.x;
		invalidRect.top = (UINT16) rects[index].origin.y;
		invalidRect.right = invalidRect.left + (UINT16) rects[index].size.width;
		invalidRect.bottom = invalidRect.top + (UINT16) rects[index].size.height;
		
		if (subsystem->retina)
		{
			/* scale invalid rect */
			invalidRect.left /= 2;
			invalidRect.top /= 2;
			invalidRect.right /= 2;
			invalidRect.bottom /= 2;
		}
		
		region16_union_rect(&(subsystem->invalidRegion), &(subsystem->invalidRegion), &invalidRect);
	}
	
	return 0;
}

void (^mac_capture_stream_handler)(CGDisplayStreamFrameStatus, uint64_t, IOSurfaceRef, CGDisplayStreamUpdateRef) =
	^(CGDisplayStreamFrameStatus status, uint64_t displayTime, IOSurfaceRef frameSurface, CGDisplayStreamUpdateRef updateRef)
{
	int x, y;
	int count;
	int width;
	int height;
	int nSrcStep;
	BYTE* pSrcData;
	RECTANGLE_16 surfaceRect;
	const RECTANGLE_16* extents;
	macShadowSubsystem* subsystem = g_Subsystem;
	rdpShadowServer* server = subsystem->server;
	rdpShadowSurface* surface = server->surface;
	
	count = ArrayList_Count(server->clients);
	
	if (count < 1)
		return;
	
	if ((count == 1) && subsystem->suppressOutput)
		return;
	
	mac_shadow_capture_get_dirty_region(subsystem);
		
	surfaceRect.left = 0;
	surfaceRect.top = 0;
	surfaceRect.right = surface->width;
	surfaceRect.bottom = surface->height;
		
	region16_intersect_rect(&(subsystem->invalidRegion), &(subsystem->invalidRegion), &surfaceRect);
	
	if (!region16_is_empty(&(subsystem->invalidRegion)))
	{
		extents = region16_extents(&(subsystem->invalidRegion));

		x = extents->left;
		y = extents->top;
		width = extents->right - extents->left;
		height = extents->bottom - extents->top;
			
		IOSurfaceLock(frameSurface, kIOSurfaceLockReadOnly, NULL);
		
		pSrcData = (BYTE*) IOSurfaceGetBaseAddress(frameSurface);
		nSrcStep = (int) IOSurfaceGetBytesPerRow(frameSurface);

		if (subsystem->retina)
		{
			freerdp_image_copy_from_retina(surface->data, PIXEL_FORMAT_XRGB32, surface->scanline,
					       x, y, width, height, pSrcData, nSrcStep, x, y);
		}
		else
		{
			freerdp_image_copy(surface->data, PIXEL_FORMAT_XRGB32, surface->scanline,
						x, y, width, height, pSrcData, PIXEL_FORMAT_XRGB32, nSrcStep, x, y, NULL);
		}
		
		IOSurfaceUnlock(frameSurface, kIOSurfaceLockReadOnly, NULL);
			
		ArrayList_Lock(server->clients);
			
		count = ArrayList_Count(server->clients);
			
		shadow_subsystem_frame_update((rdpShadowSubsystem *)subsystem);
		
		if (count == 1)
		{
			rdpShadowClient* client;
			
			client = (rdpShadowClient*) ArrayList_GetItem(server->clients, 0);
			
			if (client)
			{
				subsystem->captureFrameRate = shadow_encoder_preferred_fps(client->encoder);
			}
		}
		
		ArrayList_Unlock(server->clients);
			
		region16_clear(&(subsystem->invalidRegion));
	}
	
	if (status != kCGDisplayStreamFrameStatusFrameComplete)
	{
		switch (status)
		{
			case kCGDisplayStreamFrameStatusFrameIdle:
				break;
				
			case kCGDisplayStreamFrameStatusStopped:
				break;
				
			case kCGDisplayStreamFrameStatusFrameBlank:
				break;
				
			default:
				break;
		}
	}
	else if (!subsystem->lastUpdate)
	{
		CFRetain(updateRef);
		subsystem->lastUpdate = updateRef;
	}
	else
	{
		CGDisplayStreamUpdateRef tmpRef = subsystem->lastUpdate;
		subsystem->lastUpdate = CGDisplayStreamUpdateCreateMergedUpdate(tmpRef, updateRef);
		CFRelease(tmpRef);
	}
};

int mac_shadow_capture_init(macShadowSubsystem* subsystem)
{
	void* keys[2];
	void* values[2];
	CFDictionaryRef opts;
	CGDirectDisplayID displayId;
	
	displayId = CGMainDisplayID();
	
	subsystem->updateBuffer = (BYTE*) malloc(subsystem->pixelWidth * subsystem->pixelHeight * 4);
	
	if (!subsystem->updateBuffer)
		return -1;
	
	subsystem->captureQueue = dispatch_queue_create("mac.shadow.capture", NULL);
	
	keys[0] = (void*) kCGDisplayStreamShowCursor;
	values[0] = (void*) kCFBooleanFalse;
	
	opts = CFDictionaryCreate(kCFAllocatorDefault, (const void**) keys, (const void**) values, 1, NULL, NULL);
	
	subsystem->stream = CGDisplayStreamCreateWithDispatchQueue(displayId, subsystem->pixelWidth, subsystem->pixelHeight,
							'BGRA', opts, subsystem->captureQueue, mac_capture_stream_handler);
	
	CFRelease(opts);
	
	return 1;
}

int mac_shadow_screen_grab(macShadowSubsystem* subsystem)
{
	return 1;
}

int mac_shadow_subsystem_process_message(macShadowSubsystem* subsystem, wMessage* message)
{
	switch(message->id)
	{
		case SHADOW_MSG_IN_REFRESH_OUTPUT_ID:
		{
			UINT32 index;
			SHADOW_MSG_IN_REFRESH_OUTPUT* msg = (SHADOW_MSG_IN_REFRESH_OUTPUT*) message->wParam;

			if (msg->numRects)
			{
				for (index = 0; index < msg->numRects; index++)
				{
					region16_union_rect(&(subsystem->invalidRegion),
							&(subsystem->invalidRegion), &msg->rects[index]);
				}
			}
			else
			{
				RECTANGLE_16 refreshRect;

				refreshRect.left = 0;
				refreshRect.top = 0;
				refreshRect.right = subsystem->width;
				refreshRect.bottom = subsystem->height;

				region16_union_rect(&(subsystem->invalidRegion),
							&(subsystem->invalidRegion), &refreshRect);
			}
			break;
		}
		case SHADOW_MSG_IN_SUPPRESS_OUTPUT_ID:
		{
			SHADOW_MSG_IN_SUPPRESS_OUTPUT* msg = (SHADOW_MSG_IN_SUPPRESS_OUTPUT*) message->wParam;

			subsystem->suppressOutput = (msg->allow) ? FALSE : TRUE;

			if (msg->allow)
			{
				region16_union_rect(&(subsystem->invalidRegion),
							&(subsystem->invalidRegion), &(msg->rect));
			}
			break;
		}
		default:
			WLog_ERR(TAG, "Unknown message id: %u", message->id);
			break;
	}

	if (message->Free)
		message->Free(message);

	return 1;
}

void* mac_shadow_subsystem_thread(macShadowSubsystem* subsystem)
{
	DWORD status;
	DWORD nCount;
	UINT64 cTime;
	DWORD dwTimeout;
	DWORD dwInterval;
	UINT64 frameTime;
	HANDLE events[32];
	wMessage message;
	wMessagePipe* MsgPipe;
	
	MsgPipe = subsystem->MsgPipe;
	
	nCount = 0;
	events[nCount++] = MessageQueue_Event(MsgPipe->In);
	
	subsystem->captureFrameRate = 16;
	dwInterval = 1000 / subsystem->captureFrameRate;
	frameTime = GetTickCount64() + dwInterval;
	
	while (1)
	{
		cTime = GetTickCount64();
		dwTimeout = (cTime > frameTime) ? 0 : frameTime - cTime;
		
		status = WaitForMultipleObjects(nCount, events, FALSE, dwTimeout);
		
		if (WaitForSingleObject(MessageQueue_Event(MsgPipe->In), 0) == WAIT_OBJECT_0)
		{
			if (MessageQueue_Peek(MsgPipe->In, &message, TRUE))
			{
				if (message.id == WMQ_QUIT)
					break;
				
				mac_shadow_subsystem_process_message(subsystem, &message);
			}
		}
		
		if ((status == WAIT_TIMEOUT) || (GetTickCount64() > frameTime))
		{
			mac_shadow_screen_grab(subsystem);
			
			dwInterval = 1000 / subsystem->captureFrameRate;
			frameTime += dwInterval;
		}
	}
	
	ExitThread(0);
	return NULL;
}

int mac_shadow_enum_monitors(MONITOR_DEF* monitors, int maxMonitors)
{
	int index;
	size_t wide, high;
	int numMonitors = 0;
	MONITOR_DEF* monitor;
	CGDirectDisplayID displayId;

	displayId = CGMainDisplayID();
	CGDisplayModeRef mode = CGDisplayCopyDisplayMode(displayId);

	wide = CGDisplayPixelsWide(displayId);
	high = CGDisplayPixelsHigh(displayId);

	CGDisplayModeRelease(mode);

	index = 0;
	numMonitors = 1;

	monitor = &monitors[index];

	monitor->left = 0;
	monitor->top = 0;
	monitor->right = (int) wide;
	monitor->bottom = (int) high;
	monitor->flags = 1;

	return numMonitors;
}

int mac_shadow_subsystem_init(macShadowSubsystem* subsystem)
{
	g_Subsystem = subsystem;
	
	mac_shadow_detect_monitors(subsystem);
	
	mac_shadow_capture_init(subsystem);
	
	return 1;
}

int mac_shadow_subsystem_uninit(macShadowSubsystem* subsystem)
{
	if (!subsystem)
		return -1;
	
	if (subsystem->lastUpdate)
	{
		CFRelease(subsystem->lastUpdate);
		subsystem->lastUpdate = NULL;
	}

	return 1;
}

int mac_shadow_subsystem_start(macShadowSubsystem* subsystem)
{
	HANDLE thread;

	if (!subsystem)
		return -1;

	mac_shadow_capture_start(subsystem);
	
	if (!(thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) mac_shadow_subsystem_thread,
			(void*) subsystem, 0, NULL)))
	{
		WLog_ERR(TAG, "Failed to create thread");
		return -1;
	}

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

macShadowSubsystem* mac_shadow_subsystem_new()
{
	macShadowSubsystem* subsystem;

	subsystem = (macShadowSubsystem*) calloc(1, sizeof(macShadowSubsystem));

	if (!subsystem)
		return NULL;

	subsystem->SynchronizeEvent = (pfnShadowSynchronizeEvent) mac_shadow_input_synchronize_event;
	subsystem->KeyboardEvent = (pfnShadowKeyboardEvent) mac_shadow_input_keyboard_event;
	subsystem->UnicodeKeyboardEvent = (pfnShadowUnicodeKeyboardEvent) mac_shadow_input_unicode_keyboard_event;
	subsystem->MouseEvent = (pfnShadowMouseEvent) mac_shadow_input_mouse_event;
	subsystem->ExtendedMouseEvent = (pfnShadowExtendedMouseEvent) mac_shadow_input_extended_mouse_event;

	return subsystem;
}

FREERDP_API int Mac_ShadowSubsystemEntry(RDP_SHADOW_ENTRY_POINTS* pEntryPoints)
{
	pEntryPoints->New = (pfnShadowSubsystemNew) mac_shadow_subsystem_new;
	pEntryPoints->Free = (pfnShadowSubsystemFree) mac_shadow_subsystem_free;

	pEntryPoints->Init = (pfnShadowSubsystemInit) mac_shadow_subsystem_init;
	pEntryPoints->Uninit = (pfnShadowSubsystemInit) mac_shadow_subsystem_uninit;

	pEntryPoints->Start = (pfnShadowSubsystemStart) mac_shadow_subsystem_start;
	pEntryPoints->Stop = (pfnShadowSubsystemStop) mac_shadow_subsystem_stop;

	pEntryPoints->EnumMonitors = (pfnShadowEnumMonitors) mac_shadow_enum_monitors;

	return 1;
}
