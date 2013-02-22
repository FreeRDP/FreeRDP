/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Mac OS X Server (Input)
 *
 * Copyright 2013 Corey Clayton <can.of.tuna@gmail.com>
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

#include <ApplicationServices/ApplicationServices.h>

#include <winpr/windows.h>

#include "mf_input.h"
#include "mf_info.h"

void mf_input_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	/*
	INPUT keyboard_event;
	
	keyboard_event.type = INPUT_KEYBOARD;
	keyboard_event.ki.wVk = 0;
	keyboard_event.ki.wScan = code;
	keyboard_event.ki.dwFlags = KEYEVENTF_SCANCODE;
	keyboard_event.ki.dwExtraInfo = 0;
	keyboard_event.ki.time = 0;
	
	if (flags & KBD_FLAGS_RELEASE)
		keyboard_event.ki.dwFlags |= KEYEVENTF_KEYUP;
	
	if (flags & KBD_FLAGS_EXTENDED)
		keyboard_event.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
	
	SendInput(1, &keyboard_event, sizeof(INPUT));
	 */
}

void mf_input_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	/*
	INPUT keyboard_event;
	
	keyboard_event.type = INPUT_KEYBOARD;
	keyboard_event.ki.wVk = 0;
	keyboard_event.ki.wScan = code;
	keyboard_event.ki.dwFlags = KEYEVENTF_UNICODE;
	keyboard_event.ki.dwExtraInfo = 0;
	keyboard_event.ki.time = 0;
	
	if (flags & KBD_FLAGS_RELEASE)
		keyboard_event.ki.dwFlags |= KEYEVENTF_KEYUP;
	
	SendInput(1, &keyboard_event, sizeof(INPUT));
	 */
}

void mf_input_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	float width, height;
	
	if (flags & PTR_FLAGS_WHEEL)
	{
		/*
		mouse_event.mi.dwFlags = MOUSEEVENTF_WHEEL;
		mouse_event.mi.mouseData = flags & WheelRotationMask;
		
		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
			mouse_event.mi.mouseData *= -1;
		
		SendInput(1, &mouse_event, sizeof(INPUT));
		 */
	}
	else
	{
		
		mfInfo * mfi;
		CGEventType mouseType = kCGEventNull;
		CGMouseButton mouseButton = kCGMouseButtonLeft;
		
		
		mfi = mf_info_get_instance();
		
		//width and height of primary screen (even in multimon setups
		width = (float) mfi->servscreen_width;
		height = (float) mfi->servscreen_height;
		
		x += mfi->servscreen_xoffset;
		y += mfi->servscreen_yoffset;
		
		if (flags & PTR_FLAGS_MOVE)
		{
			if (mfi->mouse_down_left == TRUE)
			{
				mouseType = kCGEventLeftMouseDragged;
			}
			else if (mfi->mouse_down_right == TRUE)
			{
				mouseType = kCGEventRightMouseDragged;
			}
			else if (mfi->mouse_down_other == TRUE)
			{
				mouseType = kCGEventOtherMouseDragged;
			}
			else
			{
				mouseType = kCGEventMouseMoved;
			}
			
			CGEventRef move = CGEventCreateMouseEvent(NULL,
								   mouseType,
								   CGPointMake(x, y),
								   mouseButton // ignored for just movement
								   );
			
			CGEventPost(kCGHIDEventTap, move);
			
			CFRelease(move);
		}
				
		if (flags & PTR_FLAGS_BUTTON1)
		{
			mouseButton = kCGMouseButtonLeft;
			if (flags & PTR_FLAGS_DOWN)
			{
				mouseType = kCGEventLeftMouseDown;
				mfi->mouse_down = TRUE;
			}
			else
			{
				mouseType = kCGEventLeftMouseUp;
				mfi->mouse_down = FALSE;
			}
		}
		else if (flags & PTR_FLAGS_BUTTON2)
		{
			mouseButton = kCGMouseButtonRight;
			if (flags & PTR_FLAGS_DOWN)
				mouseType = kCGEventRightMouseDown;
			else
				mouseType = kCGEventRightMouseUp;

		}
		else if (flags & PTR_FLAGS_BUTTON3)
		{
			mouseButton = kCGMouseButtonCenter;
			if (flags & PTR_FLAGS_DOWN)
				mouseType = kCGEventOtherMouseDown;
			else
				mouseType = kCGEventOtherMouseUp;

		}
		
		
		CGEventRef mouseEvent = CGEventCreateMouseEvent(NULL,
								 mouseType,
								 CGPointMake(x, y),
								 mouseButton
								 );
		CGEventPost(kCGHIDEventTap, mouseEvent);
		
		CFRelease(mouseEvent);
	}
}

void mf_input_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	/*
	if ((flags & PTR_XFLAGS_BUTTON1) || (flags & PTR_XFLAGS_BUTTON2))
	{
		INPUT mouse_event;
		ZeroMemory(&mouse_event, sizeof(INPUT));
		
		mouse_event.type = INPUT_MOUSE;
		
		if (flags & PTR_FLAGS_MOVE)
		{
			float width, height;
			wfInfo * wfi;
			
			wfi = wf_info_get_instance();
			//width and height of primary screen (even in multimon setups
			width = (float) GetSystemMetrics(SM_CXSCREEN);
			height = (float) GetSystemMetrics(SM_CYSCREEN);
			
			x += wfi->servscreen_xoffset;
			y += wfi->servscreen_yoffset;
			
			//mouse_event.mi.dx = x * (0xFFFF / width);
			//mouse_event.mi.dy = y * (0xFFFF / height);
			mouse_event.mi.dx = (LONG) ((float) x * (65535.0f / width));
			mouse_event.mi.dy = (LONG) ((float) y * (65535.0f / height));
			mouse_event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
			
			SendInput(1, &mouse_event, sizeof(INPUT));
		}
		
		mouse_event.mi.dx = mouse_event.mi.dy = mouse_event.mi.dwFlags = 0;
		
		if (flags & PTR_XFLAGS_DOWN)
			mouse_event.mi.dwFlags |= MOUSEEVENTF_XDOWN;
		else
			mouse_event.mi.dwFlags |= MOUSEEVENTF_XUP;
		
		if (flags & PTR_XFLAGS_BUTTON1)
			mouse_event.mi.mouseData = XBUTTON1;
		else if (flags & PTR_XFLAGS_BUTTON2)
			mouse_event.mi.mouseData = XBUTTON2;
		
		SendInput(1, &mouse_event, sizeof(INPUT));
	}
	else
	{
		mf_input_mouse_event(input, flags, x, y);
	}
	 */
}


void mf_input_keyboard_event_dummy(rdpInput* input, UINT16 flags, UINT16 code)
{
}

void mf_input_unicode_keyboard_event_dummy(rdpInput* input, UINT16 flags, UINT16 code)
{
}

void mf_input_mouse_event_dummy(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
}

void mf_input_extended_mouse_event_dummy(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
}