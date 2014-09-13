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
		
		CGEventSourceRef source = CGEventSourceCreate (kCGEventSourceStateHIDSystemState);
		CGEventRef scroll = CGEventCreateScrollWheelEvent(source, kCGScrollEventUnitLine,
								  wheelCount, scrollY, scrollX);
		CGEventPost(kCGHIDEventTap, scroll);
		
		CFRelease(scroll);
		CFRelease(source);
	}
	else
	{
		CGEventSourceRef source = CGEventSourceCreate (kCGEventSourceStateHIDSystemState);
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

void mac_shadow_input_extended_mouse_event(macShadowSubsystem* subsystem, UINT16 flags, UINT16 x, UINT16 y)
{

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
	
	subsystem->monitorCount = 1;
	
	monitor = &(subsystem->monitors[0]);
	
	monitor->left = 0;
	monitor->top = 0;
	monitor->right = subsystem->width;
	monitor->bottom = subsystem->height;
	monitor->flags = 1;
	
	return 1;
}

void (^streamHandler)(CGDisplayStreamFrameStatus, uint64_t, IOSurfaceRef, CGDisplayStreamUpdateRef) =  ^(CGDisplayStreamFrameStatus status, uint64_t displayTime, IOSurfaceRef frameSurface, CGDisplayStreamUpdateRef updateRef)
{
	
};

int mac_shadow_capture_init(macShadowSubsystem* subsystem)
{
	void* keys[2];
	void* values[2];
	CFDictionaryRef opts;
	CGDirectDisplayID displayId;
	
	displayId = CGMainDisplayID();
	
	subsystem->captureQueue = dispatch_queue_create("mac.shadow.capture", NULL);
	
	keys[0] = (void*) kCGDisplayStreamShowCursor;
	values[0] = (void*) kCFBooleanFalse;
	
	opts = CFDictionaryCreate(kCFAllocatorDefault, (const void**) keys, (const void**) values, 1, NULL, NULL);
	
	subsystem->stream = CGDisplayStreamCreateWithDispatchQueue(displayId,
							subsystem->pixelWidth, subsystem->pixelHeight,
							'BGRA', opts, subsystem->captureQueue, streamHandler);
	
	CFRelease(opts);
	
	return 1;
}

int mac_shadow_subsystem_init(macShadowSubsystem* subsystem)
{
	mac_shadow_detect_monitors(subsystem);
	
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
	
	subsystem->updateEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	
	region16_init(&(subsystem->invalidRegion));

	subsystem->Init = (pfnShadowSubsystemInit) mac_shadow_subsystem_init;
	subsystem->Uninit = (pfnShadowSubsystemInit) mac_shadow_subsystem_uninit;
	subsystem->Start = (pfnShadowSubsystemStart) mac_shadow_subsystem_start;
	subsystem->Stop = (pfnShadowSubsystemStop) mac_shadow_subsystem_stop;
	subsystem->Free = (pfnShadowSubsystemFree) mac_shadow_subsystem_free;

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
