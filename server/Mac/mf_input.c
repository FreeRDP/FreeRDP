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
#include <Carbon/Carbon.h>

#include <winpr/windows.h>

#include "mf_input.h"
#include "mf_info.h"


CGKeyCode test = kVK_ANSI_A;

/*static const CGKeyCode keymap[256] =
 {
 0xFF, //{ 0, "" },
 0xFF, //{ VK_LBUTTON, "VK_LBUTTON" },
 0xFF, //{ VK_RBUTTON, "VK_RBUTTON" },
 0xFF, //{ VK_CANCEL, "VK_CANCEL" },
 0xFF, //{ VK_MBUTTON, "VK_MBUTTON" },
 0xFF, //{ VK_XBUTTON1, "VK_XBUTTON1" },
 0xFF, //{ VK_XBUTTON2, "VK_XBUTTON2" },
 0xFF, //{ 0, "" },
 0xFF, //{ VK_BACK, "VK_BACK" },
 0xFF, //{ VK_TAB, "VK_TAB" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ VK_CLEAR, "VK_CLEAR" },
 0xFF, //{ VK_RETURN, "VK_RETURN" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ VK_SHIFT, "VK_SHIFT" },
 0xFF, //{ VK_CONTROL, "VK_CONTROL" },
 0xFF, //{ VK_MENU, "VK_MENU" },
 0xFF, //{ VK_PAUSE, "VK_PAUSE" },
 0xFF, //{ VK_CAPITAL, "VK_CAPITAL" },
 0xFF, //{ VK_KANA, "VK_KANA" }0xFF, // also VK_HANGUL
 0xFF, //{ 0, "" },
 0xFF, //{ VK_JUNJA, "VK_JUNJA" },
 0xFF, //{ VK_FINAL, "VK_FINAL" },
 0xFF, //{ VK_KANJI, "VK_KANJI" }0xFF, // also VK_HANJA
 0xFF, //{ 0, "" },
 0xFF, //{ VK_ESCAPE, "VK_ESCAPE" },
 0xFF, //{ VK_CONVERT, "VK_CONVERT" },
 0xFF, //{ VK_NONCONVERT, "VK_NONCONVERT" },
 0xFF, //{ VK_ACCEPT, "VK_ACCEPT" },
 0xFF, //{ VK_MODECHANGE, "VK_MODECHANGE" },
 0xFF, //{ VK_SPACE, "VK_SPACE" },
 0xFF, //{ VK_PRIOR, "VK_PRIOR" },
 0xFF, //{ VK_NEXT, "VK_NEXT" },
 0xFF, //{ VK_END, "VK_END" },
 0xFF, //{ VK_HOME, "VK_HOME" },
 0xFF, //{ VK_LEFT, "VK_LEFT" },
 0xFF, //{ VK_UP, "VK_UP" },
 0xFF, //{ VK_RIGHT, "VK_RIGHT" },
 0xFF, //{ VK_DOWN, "VK_DOWN" },
 0xFF, //{ VK_SELECT, "VK_SELECT" },
 0xFF, //{ VK_PRINT, "VK_PRINT" },
 0xFF, //{ VK_EXECUTE, "VK_EXECUTE" },
 0xFF, //{ VK_SNAPSHOT, "VK_SNAPSHOT" },
 0xFF, //{ VK_INSERT, "VK_INSERT" },
 0xFF, //{ VK_DELETE, "VK_DELETE" },
 0xFF, //{ VK_HELP, "VK_HELP" },
 0x0B, //{ VK_KEY_0, "VK_KEY_0" },
 0x02, //{ VK_KEY_1, "VK_KEY_1" },
 0x03, //{ VK_KEY_2, "VK_KEY_2" },
 0x04, //{ VK_KEY_3, "VK_KEY_3" },
 0x05, //{ VK_KEY_4, "VK_KEY_4" },
 0x06, //{ VK_KEY_5, "VK_KEY_5" },
 0x07, //{ VK_KEY_6, "VK_KEY_6" },
 0x08, //{ VK_KEY_7, "VK_KEY_7" },
 0x09, //{ VK_KEY_8, "VK_KEY_8" },
 0x0A, //{ VK_KEY_9, "VK_KEY_9" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0x1E, //{ VK_KEY_A, "VK_KEY_A" },
 0xFF, //{ VK_KEY_B, "VK_KEY_B" },
 0xFF, //{ VK_KEY_C, "VK_KEY_C" },
 0x20, //{ VK_KEY_D, "VK_KEY_D" },
 0xFF, //{ VK_KEY_E, "VK_KEY_E" },
 0xFF, //{ VK_KEY_F, "VK_KEY_F" },
 0xFF, //{ VK_KEY_G, "VK_KEY_G" },
 0xFF, //{ VK_KEY_H, "VK_KEY_H" },
 0xFF, //{ VK_KEY_I, "VK_KEY_I" },
 0xFF, //{ VK_KEY_J, "VK_KEY_J" },
 0xFF, //{ VK_KEY_K, "VK_KEY_K" },
 0xFF, //{ VK_KEY_L, "VK_KEY_L" },
 0xFF, //{ VK_KEY_M, "VK_KEY_M" },
 0xFF, //{ VK_KEY_N, "VK_KEY_N" },
 0xFF, //{ VK_KEY_O, "VK_KEY_O" },
 0xFF, //{ VK_KEY_P, "VK_KEY_P" },
 0xFF, //{ VK_KEY_Q, "VK_KEY_Q" },
 0xFF, //{ VK_KEY_R, "VK_KEY_R" },
 0x1F, //{ VK_KEY_S, "VK_KEY_S" },
 0xFF, //{ VK_KEY_T, "VK_KEY_T" },
 0xFF, //{ VK_KEY_U, "VK_KEY_U" },
 0xFF, //{ VK_KEY_V, "VK_KEY_V" },
 0xFF, //{ VK_KEY_W, "VK_KEY_W" },
 0xFF, //{ VK_KEY_X, "VK_KEY_X" },
 0xFF, //{ VK_KEY_Y, "VK_KEY_Y" },
 0xFF, //{ VK_KEY_Z, "VK_KEY_Z" },
 0xFF, //{ VK_LWIN, "VK_LWIN" },
 0xFF, //{ VK_RWIN, "VK_RWIN" },
 0xFF, //{ VK_APPS, "VK_APPS" },
 0xFF, //{ 0, "" },
 0xFF, //{ VK_SLEEP, "VK_SLEEP" },
 0xFF, //{ VK_NUMPAD0, "VK_NUMPAD0" },
 0xFF, //{ VK_NUMPAD1, "VK_NUMPAD1" },
 0xFF, //{ VK_NUMPAD2, "VK_NUMPAD2" },
 0xFF, //{ VK_NUMPAD3, "VK_NUMPAD3" },
 0xFF, //{ VK_NUMPAD4, "VK_NUMPAD4" },
 0xFF, //{ VK_NUMPAD5, "VK_NUMPAD5" },
 0xFF, //{ VK_NUMPAD6, "VK_NUMPAD6" },
 0xFF, //{ VK_NUMPAD7, "VK_NUMPAD7" },
 0xFF, //{ VK_NUMPAD8, "VK_NUMPAD8" },
 0xFF, //{ VK_NUMPAD9, "VK_NUMPAD9" },
 0xFF, //{ VK_MULTIPLY, "VK_MULTIPLY" },
 0xFF, //{ VK_ADD, "VK_ADD" },
 0xFF, //{ VK_SEPARATOR, "VK_SEPARATOR" },
 0xFF, //{ VK_SUBTRACT, "VK_SUBTRACT" },
 0xFF, //{ VK_DECIMAL, "VK_DECIMAL" },
 0xFF, //{ VK_DIVIDE, "VK_DIVIDE" },
 0xFF, //{ VK_F1, "VK_F1" },
 0xFF, //{ VK_F2, "VK_F2" },
 0xFF, //{ VK_F3, "VK_F3" },
 0xFF, //{ VK_F4, "VK_F4" },
 0xFF, //{ VK_F5, "VK_F5" },
 0xFF, //{ VK_F6, "VK_F6" },
 0xFF, //{ VK_F7, "VK_F7" },
 0xFF, //{ VK_F8, "VK_F8" },
 0xFF, //{ VK_F9, "VK_F9" },
 0xFF, //{ VK_F10, "VK_F10" },
 0xFF, //{ VK_F11, "VK_F11" },
 0xFF, //{ VK_F12, "VK_F12" },
 0xFF, //{ VK_F13, "VK_F13" },
 0xFF, //{ VK_F14, "VK_F14" },
 0xFF, //{ VK_F15, "VK_F15" },
 0xFF, //{ VK_F16, "VK_F16" },
 0xFF, //{ VK_F17, "VK_F17" },
 0xFF, //{ VK_F18, "VK_F18" },
 0xFF, //{ VK_F19, "VK_F19" },
 0xFF, //{ VK_F20, "VK_F20" },
 0xFF, //{ VK_F21, "VK_F21" },
 0xFF, //{ VK_F22, "VK_F22" },
 0xFF, //{ VK_F23, "VK_F23" },
 0xFF, //{ VK_F24, "VK_F24" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ VK_NUMLOCK, "VK_NUMLOCK" },
 0xFF, //{ VK_SCROLL, "VK_SCROLL" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ VK_LSHIFT, "VK_LSHIFT" },
 0xFF, //{ VK_RSHIFT, "VK_RSHIFT" },
 0xFF, //{ VK_LCONTROL, "VK_LCONTROL" },
 0xFF, //{ VK_RCONTROL, "VK_RCONTROL" },
 0xFF, //{ VK_LMENU, "VK_LMENU" },
 0xFF, //{ VK_RMENU, "VK_RMENU" },
 0xFF, //{ VK_BROWSER_BACK, "VK_BROWSER_BACK" },
 0xFF, //{ VK_BROWSER_FORWARD, "VK_BROWSER_FORWARD" },
 0xFF, //{ VK_BROWSER_REFRESH, "VK_BROWSER_REFRESH" },
 0xFF, //{ VK_BROWSER_STOP, "VK_BROWSER_STOP" },
 0xFF, //{ VK_BROWSER_SEARCH, "VK_BROWSER_SEARCH" },
 0xFF, //{ VK_BROWSER_FAVORITES, "VK_BROWSER_FAVORITES" },
 0xFF, //{ VK_BROWSER_HOME, "VK_BROWSER_HOME" },
 0xFF, //{ VK_VOLUME_MUTE, "VK_VOLUME_MUTE" },
 0xFF, //{ VK_VOLUME_DOWN, "VK_VOLUME_DOWN" },
 0xFF, //{ VK_VOLUME_UP, "VK_VOLUME_UP" },
 0xFF, //{ VK_MEDIA_NEXT_TRACK, "VK_MEDIA_NEXT_TRACK" },
 0xFF, //{ VK_MEDIA_PREV_TRACK, "VK_MEDIA_PREV_TRACK" },
 0xFF, //{ VK_MEDIA_STOP, "VK_MEDIA_STOP" },
 0xFF, //{ VK_MEDIA_PLAY_PAUSE, "VK_MEDIA_PLAY_PAUSE" },
 0xFF, //{ VK_LAUNCH_MAIL, "VK_LAUNCH_MAIL" },
 0xFF, //{ VK_MEDIA_SELECT, "VK_MEDIA_SELECT" },
 0xFF, //{ VK_LAUNCH_APP1, "VK_LAUNCH_APP1" },
 0xFF, //{ VK_LAUNCH_APP2, "VK_LAUNCH_APP2" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ VK_OEM_1, "VK_OEM_1" },
 0xFF, //{ VK_OEM_PLUS, "VK_OEM_PLUS" },
 0xFF, //{ VK_OEM_COMMA, "VK_OEM_COMMA" },
 0xFF, //{ VK_OEM_MINUS, "VK_OEM_MINUS" },
 0xFF, //{ VK_OEM_PERIOD, "VK_OEM_PERIOD" },
 0xFF, //{ VK_OEM_2, "VK_OEM_2" },
 0xFF, //{ VK_OEM_3, "VK_OEM_3" },
 0xFF, //{ VK_ABNT_C1, "VK_ABNT_C1" },
 0xFF, //{ VK_ABNT_C2, "VK_ABNT_C2" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ VK_OEM_4, "VK_OEM_4" },
 0xFF, //{ VK_OEM_5, "VK_OEM_5" },
 0xFF, //{ VK_OEM_6, "VK_OEM_6" },
 0xFF, //{ VK_OEM_7, "VK_OEM_7" },
 0xFF, //{ VK_OEM_8, "VK_OEM_8" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ VK_OEM_102, "VK_OEM_102" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ VK_PROCESSKEY, "VK_PROCESSKEY" },
 0xFF, //{ 0, "" },
 0xFF, //{ VK_PACKET, "VK_PACKET" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ 0, "" },
 0xFF, //{ VK_ATTN, "VK_ATTN" },
 0xFF, //{ VK_CRSEL, "VK_CRSEL" },
 0xFF, //{ VK_EXSEL, "VK_EXSEL" },
 0xFF, //{ VK_EREOF, "VK_EREOF" },
 0xFF, //{ VK_PLAY, "VK_PLAY" },
 0xFF, //{ VK_ZOOM, "VK_ZOOM" },
 0xFF, //{ VK_NONAME, "VK_NONAME" },
 0xFF, //{ VK_PA1, "VK_PA1" },
 0xFF, //{ VK_OEM_CLEAR, "VK_OEM_CLEAR" },
 0xFF //{ 0, "" }
 };
 
 */

static const CGKeyCode keymap[256] = {
	0xFF, //0x0
	0xFF, //0x1
	kVK_ANSI_1, //0x2
	kVK_ANSI_2, //0x3
	kVK_ANSI_3, //0x4
	kVK_ANSI_4, //0x5
	kVK_ANSI_5, //0x6
	kVK_ANSI_6, //0x7
	kVK_ANSI_7, //0x8
	kVK_ANSI_8, //0x9
	kVK_ANSI_9, //0xa
	kVK_ANSI_0, //0xb
	0xFF, //0xc
	0xFF, //0xd
	0xFF, //0xe
	0xFF, //0xf
	kVK_ANSI_Q, //0x10
	kVK_ANSI_W, //0x11
	kVK_ANSI_E, //0x12
	kVK_ANSI_R, //0x13
	kVK_ANSI_T, //0x14
	kVK_ANSI_Y, //0x15
	kVK_ANSI_U, //0x16
	kVK_ANSI_I, //0x17
	kVK_ANSI_O, //0x18
	kVK_ANSI_P, //0x19
	kVK_ANSI_LeftBracket, //0x1a
	kVK_ANSI_RightBracket, //0x1b
	0xFF, //0x1c
	0xFF, //0x1d
	kVK_ANSI_A, //0x1e
	kVK_ANSI_S, //0x1f
	kVK_ANSI_D, //0x20
	kVK_ANSI_F, //0x21
	0xFF, //0x22
	0xFF, //0x23
	0xFF, //0x24
	0xFF, //0x25
	0xFF, //0x26
	0xFF, //0x27
	0xFF, //0x28
	0xFF, //0x29
	0xFF, //0x2a
	0xFF, //0x2b
	0xFF, //0x2c
	0xFF, //0x2d
	0xFF, //0x2e
	0xFF, //0x2f
	0xFF, //0x30
	0xFF, //0x31
	0xFF, //0x32
	0xFF, //0x33
	0xFF, //0x34
	0xFF, //0x35
	0xFF, //0x36
	0xFF, //0x37
	0xFF, //0x38
	0xFF, //0x39
	0xFF, //0x3a
	0xFF, //0x3b
	0xFF, //0x3c
	0xFF, //0x3d
	0xFF, //0x3e
	0xFF, //0x3f
	0xFF, //0x40
	0xFF, //0x41
	0xFF, //0x42
	0xFF, //0x43
	0xFF, //0x44
	0xFF, //0x45
	0xFF, //0x46
	0xFF, //0x47
	0xFF, //0x48
	0xFF, //0x49
	0xFF, //0x4a
	0xFF, //0x4b
	0xFF, //0x4c
	0xFF, //0x4d
	0xFF, //0x4e
	0xFF, //0x4f
	0xFF, //0x50
	0xFF, //0x51
	0xFF, //0x52
	0xFF, //0x53
	0xFF, //0x54
	0xFF, //0x55
	0xFF, //0x56
	0xFF, //0x57
	0xFF, //0x58
	0xFF, //0x59
	0xFF, //0x5a
	0xFF, //0x5b
	0xFF, //0x5c
	0xFF, //0x5d
	0xFF, //0x5e
	0xFF, //0x5f
	0xFF, //0x60
	0xFF, //0x61
	0xFF, //0x62
	0xFF, //0x63
	0xFF, //0x64
	0xFF, //0x65
	0xFF, //0x66
	0xFF, //0x67
	0xFF, //0x68
	0xFF, //0x69
	0xFF, //0x6a
	0xFF, //0x6b
	0xFF, //0x6c
	0xFF, //0x6d
	0xFF, //0x6e
	0xFF, //0x6f
	0xFF, //0x70
	0xFF, //0x71
	0xFF, //0x72
	0xFF, //0x73
	0xFF, //0x74
	0xFF, //0x75
	0xFF, //0x76
	0xFF, //0x77
	0xFF, //0x78
	0xFF, //0x79
	0xFF, //0x7a
	0xFF, //0x7b
	0xFF, //0x7c
	0xFF, //0x7d
	0xFF, //0x7e
	0xFF, //0x7f
	0xFF, //0x80
	0xFF, //0x81
	0xFF, //0x82
	0xFF, //0x83
	0xFF, //0x84
	0xFF, //0x85
	0xFF, //0x86
	0xFF, //0x87
	0xFF, //0x88
	0xFF, //0x89
	0xFF, //0x8a
	0xFF, //0x8b
	0xFF, //0x8c
	0xFF, //0x8d
	0xFF, //0x8e
	0xFF, //0x8f
	0xFF, //0x90
	0xFF, //0x91
	0xFF, //0x92
	0xFF, //0x93
	0xFF, //0x94
	0xFF, //0x95
	0xFF, //0x96
	0xFF, //0x97
	0xFF, //0x98
	0xFF, //0x99
	0xFF, //0x9a
	0xFF, //0x9b
	0xFF, //0x9c
	0xFF, //0x9d
	0xFF, //0x9e
	0xFF, //0x9f
	0xFF, //0xa0
	0xFF, //0xa1
	0xFF, //0xa2
	0xFF, //0xa3
	0xFF, //0xa4
	0xFF, //0xa5
	0xFF, //0xa6
	0xFF, //0xa7
	0xFF, //0xa8
	0xFF, //0xa9
	0xFF, //0xaa
	0xFF, //0xab
	0xFF, //0xac
	0xFF, //0xad
	0xFF, //0xae
	0xFF, //0xaf
	0xFF, //0xb0
	0xFF, //0xb1
	0xFF, //0xb2
	0xFF, //0xb3
	0xFF, //0xb4
	0xFF, //0xb5
	0xFF, //0xb6
	0xFF, //0xb7
	0xFF, //0xb8
	0xFF, //0xb9
	0xFF, //0xba
	0xFF, //0xbb
	0xFF, //0xbc
	0xFF, //0xbd
	0xFF, //0xbe
	0xFF, //0xbf
	0xFF, //0xc0
	0xFF, //0xc1
	0xFF, //0xc2
	0xFF, //0xc3
	0xFF, //0xc4
	0xFF, //0xc5
	0xFF, //0xc6
	0xFF, //0xc7
	0xFF, //0xc8
	0xFF, //0xc9
	0xFF, //0xca
	0xFF, //0xcb
	0xFF, //0xcc
	0xFF, //0xcd
	0xFF, //0xce
	0xFF, //0xcf
	0xFF, //0xd0
	0xFF, //0xd1
	0xFF, //0xd2
	0xFF, //0xd3
	0xFF, //0xd4
	0xFF, //0xd5
	0xFF, //0xd6
	0xFF, //0xd7
	0xFF, //0xd8
	0xFF, //0xd9
	0xFF, //0xda
	0xFF, //0xdb
	0xFF, //0xdc
	0xFF, //0xdd
	0xFF, //0xde
	0xFF, //0xdf
	0xFF, //0xe0
	0xFF, //0xe1
	0xFF, //0xe2
	0xFF, //0xe3
	0xFF, //0xe4
	0xFF, //0xe5
	0xFF, //0xe6
	0xFF, //0xe7
	0xFF, //0xe8
	0xFF, //0xe9
	0xFF, //0xea
	0xFF, //0xeb
	0xFF, //0xec
	0xFF, //0xed
	0xFF, //0xee
	0xFF, //0xef
	0xFF, //0xf0
	0xFF, //0xf1
	0xFF, //0xf2
	0xFF, //0xf3
	0xFF, //0xf4
	0xFF, //0xf5
	0xFF, //0xf6
	0xFF, //0xf7
	0xFF, //0xf8
	0xFF, //0xf9
	0xFF, //0xfa
	0xFF, //0xfb
	0xFF, //0xfc
	0xFF, //0xfd
	0xFF, //0xfe
};



void mf_input_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	CGEventSourceRef source = CGEventSourceCreate (kCGEventSourceStateCombinedSessionState);
	
	BOOL keyDown = TRUE;
	CGEventRef kbEvent;
	
	if (flags & KBD_FLAGS_RELEASE)
	{
		keyDown = FALSE;
	}
	kbEvent = CGEventCreateKeyboardEvent(source, keymap[code], keyDown);
	CGEventPost(kCGSessionEventTap, kbEvent);
	CFRelease(kbEvent);
	CFRelease(source);
	
	printf("keypress: down = %d, SCAN=%#0X, VK=%#0X\n", keyDown, code, keymap[code]);
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
		CGEventSourceRef source = CGEventSourceCreate (kCGEventSourceStateCombinedSessionState);
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
			
			CGEventRef move = CGEventCreateMouseEvent(source,
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
				mfi->mouse_down_left = TRUE;
			}
			else
			{
				mouseType = kCGEventLeftMouseUp;
				mfi->mouse_down_right = FALSE;
			}
		}
		else if (flags & PTR_FLAGS_BUTTON2)
		{
			mouseButton = kCGMouseButtonRight;
			if (flags & PTR_FLAGS_DOWN)
			{
				mouseType = kCGEventRightMouseDown;
				mfi->mouse_down_right = TRUE;
			}
			else
			{
				mouseType = kCGEventRightMouseUp;
				mfi->mouse_down_right = FALSE;
			}
			
		}
		else if (flags & PTR_FLAGS_BUTTON3)
		{
			mouseButton = kCGMouseButtonCenter;
			if (flags & PTR_FLAGS_DOWN)
			{
				mouseType = kCGEventOtherMouseDown;
				mfi->mouse_down_other = TRUE;
			}
			else
			{
				mouseType = kCGEventOtherMouseUp;
				mfi->mouse_down_other = FALSE;
			}
			
		}
		
		
		CGEventRef mouseEvent = CGEventCreateMouseEvent(source,
								mouseType,
								CGPointMake(x, y),
								mouseButton
								);
		CGEventPost(kCGSessionEventTap, mouseEvent);
		
		CFRelease(mouseEvent);
		CFRelease(source);
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