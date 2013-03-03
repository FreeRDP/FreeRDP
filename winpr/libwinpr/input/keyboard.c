/**
 * WinPR: Windows Portable Runtime
 * Keyboard Input
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>

#include <winpr/input.h>

/**
 * Virtual Key Codes
 */

struct _VIRTUAL_KEY_CODE
{
	DWORD code; /* Windows Virtual Key Code */
	const char* name; /* Virtual Key Code Name */
};
typedef struct _VIRTUAL_KEY_CODE VIRTUAL_KEY_CODE;

static const VIRTUAL_KEY_CODE VIRTUAL_KEY_CODE_TABLE[256] =
{
	{ 0, NULL },
	{ VK_LBUTTON, "VK_LBUTTON" },
	{ VK_RBUTTON, "VK_RBUTTON" },
	{ VK_CANCEL, "VK_CANCEL" },
	{ VK_MBUTTON, "VK_MBUTTON" },
	{ VK_XBUTTON1, "VK_XBUTTON1" },
	{ VK_XBUTTON2, "VK_XBUTTON2" },
	{ 0, NULL },
	{ VK_BACK, "VK_BACK" },
	{ VK_TAB, "VK_TAB" },
	{ 0, NULL },
	{ 0, NULL },
	{ VK_CLEAR, "VK_CLEAR" },
	{ VK_RETURN, "VK_RETURN" },
	{ 0, NULL },
	{ 0, NULL },
	{ VK_SHIFT, "VK_SHIFT" },
	{ VK_CONTROL, "VK_CONTROL" },
	{ VK_MENU, "VK_MENU" },
	{ VK_PAUSE, "VK_PAUSE" },
	{ VK_CAPITAL, "VK_CAPITAL" },
	{ VK_KANA, "VK_KANA" }, /* also VK_HANGUL */
	{ 0, NULL },
	{ VK_JUNJA, "VK_JUNJA" },
	{ VK_FINAL, "VK_FINAL" },
	{ VK_KANJI, "VK_KANJI" }, /* also VK_HANJA */
	{ 0, NULL },
	{ VK_ESCAPE, "VK_ESCAPE" },
	{ VK_CONVERT, "VK_CONVERT" },
	{ VK_NONCONVERT, "VK_NONCONVERT" },
	{ VK_ACCEPT, "VK_ACCEPT" },
	{ VK_MODECHANGE, "VK_MODECHANGE" },
	{ VK_SPACE, "VK_SPACE" },
	{ VK_PRIOR, "VK_PRIOR" },
	{ VK_NEXT, "VK_NEXT" },
	{ VK_END, "VK_END" },
	{ VK_HOME, "VK_HOME" },
	{ VK_LEFT, "VK_LEFT" },
	{ VK_UP, "VK_UP" },
	{ VK_RIGHT, "VK_RIGHT" },
	{ VK_DOWN, "VK_DOWN" },
	{ VK_SELECT, "VK_SELECT" },
	{ VK_PRINT, "VK_PRINT" },
	{ VK_EXECUTE, "VK_EXECUTE" },
	{ VK_SNAPSHOT, "VK_SNAPSHOT" },
	{ VK_INSERT, "VK_INSERT" },
	{ VK_DELETE, "VK_DELETE" },
	{ VK_HELP, "VK_HELP" },
	{ VK_KEY_0, "VK_KEY_0" },
	{ VK_KEY_1, "VK_KEY_1" },
	{ VK_KEY_2, "VK_KEY_2" },
	{ VK_KEY_3, "VK_KEY_3" },
	{ VK_KEY_4, "VK_KEY_4" },
	{ VK_KEY_5, "VK_KEY_5" },
	{ VK_KEY_6, "VK_KEY_6" },
	{ VK_KEY_7, "VK_KEY_7" },
	{ VK_KEY_8, "VK_KEY_8" },
	{ VK_KEY_9, "VK_KEY_9" },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ VK_KEY_A, "VK_KEY_A" },
	{ VK_KEY_B, "VK_KEY_B" },
	{ VK_KEY_C, "VK_KEY_C" },
	{ VK_KEY_D, "VK_KEY_D" },
	{ VK_KEY_E, "VK_KEY_E" },
	{ VK_KEY_F, "VK_KEY_F" },
	{ VK_KEY_G, "VK_KEY_G" },
	{ VK_KEY_H, "VK_KEY_H" },
	{ VK_KEY_I, "VK_KEY_I" },
	{ VK_KEY_J, "VK_KEY_J" },
	{ VK_KEY_K, "VK_KEY_K" },
	{ VK_KEY_L, "VK_KEY_L" },
	{ VK_KEY_M, "VK_KEY_M" },
	{ VK_KEY_N, "VK_KEY_N" },
	{ VK_KEY_O, "VK_KEY_O" },
	{ VK_KEY_P, "VK_KEY_P" },
	{ VK_KEY_Q, "VK_KEY_Q" },
	{ VK_KEY_R, "VK_KEY_R" },
	{ VK_KEY_S, "VK_KEY_S" },
	{ VK_KEY_T, "VK_KEY_T" },
	{ VK_KEY_U, "VK_KEY_U" },
	{ VK_KEY_V, "VK_KEY_V" },
	{ VK_KEY_W, "VK_KEY_W" },
	{ VK_KEY_X, "VK_KEY_X" },
	{ VK_KEY_Y, "VK_KEY_Y" },
	{ VK_KEY_Z, "VK_KEY_Z" },
	{ VK_LWIN, "VK_LWIN" },
	{ VK_RWIN, "VK_RWIN" },
	{ VK_APPS, "VK_APPS" },
	{ 0, NULL },
	{ VK_SLEEP, "VK_SLEEP" },
	{ VK_NUMPAD0, "VK_NUMPAD0" },
	{ VK_NUMPAD1, "VK_NUMPAD1" },
	{ VK_NUMPAD2, "VK_NUMPAD2" },
	{ VK_NUMPAD3, "VK_NUMPAD3" },
	{ VK_NUMPAD4, "VK_NUMPAD4" },
	{ VK_NUMPAD5, "VK_NUMPAD5" },
	{ VK_NUMPAD6, "VK_NUMPAD6" },
	{ VK_NUMPAD7, "VK_NUMPAD7" },
	{ VK_NUMPAD8, "VK_NUMPAD8" },
	{ VK_NUMPAD9, "VK_NUMPAD9" },
	{ VK_MULTIPLY, "VK_MULTIPLY" },
	{ VK_ADD, "VK_ADD" },
	{ VK_SEPARATOR, "VK_SEPARATOR" },
	{ VK_SUBTRACT, "VK_SUBTRACT" },
	{ VK_DECIMAL, "VK_DECIMAL" },
	{ VK_DIVIDE, "VK_DIVIDE" },
	{ VK_F1, "VK_F1" },
	{ VK_F2, "VK_F2" },
	{ VK_F3, "VK_F3" },
	{ VK_F4, "VK_F4" },
	{ VK_F5, "VK_F5" },
	{ VK_F6, "VK_F6" },
	{ VK_F7, "VK_F7" },
	{ VK_F8, "VK_F8" },
	{ VK_F9, "VK_F9" },
	{ VK_F10, "VK_F10" },
	{ VK_F11, "VK_F11" },
	{ VK_F12, "VK_F12" },
	{ VK_F13, "VK_F13" },
	{ VK_F14, "VK_F14" },
	{ VK_F15, "VK_F15" },
	{ VK_F16, "VK_F16" },
	{ VK_F17, "VK_F17" },
	{ VK_F18, "VK_F18" },
	{ VK_F19, "VK_F19" },
	{ VK_F20, "VK_F20" },
	{ VK_F21, "VK_F21" },
	{ VK_F22, "VK_F22" },
	{ VK_F23, "VK_F23" },
	{ VK_F24, "VK_F24" },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ VK_NUMLOCK, "VK_NUMLOCK" },
	{ VK_SCROLL, "VK_SCROLL" },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ VK_LSHIFT, "VK_LSHIFT" },
	{ VK_RSHIFT, "VK_RSHIFT" },
	{ VK_LCONTROL, "VK_LCONTROL" },
	{ VK_RCONTROL, "VK_RCONTROL" },
	{ VK_LMENU, "VK_LMENU" },
	{ VK_RMENU, "VK_RMENU" },
	{ VK_BROWSER_BACK, "VK_BROWSER_BACK" },
	{ VK_BROWSER_FORWARD, "VK_BROWSER_FORWARD" },
	{ VK_BROWSER_REFRESH, "VK_BROWSER_REFRESH" },
	{ VK_BROWSER_STOP, "VK_BROWSER_STOP" },
	{ VK_BROWSER_SEARCH, "VK_BROWSER_SEARCH" },
	{ VK_BROWSER_FAVORITES, "VK_BROWSER_FAVORITES" },
	{ VK_BROWSER_HOME, "VK_BROWSER_HOME" },
	{ VK_VOLUME_MUTE, "VK_VOLUME_MUTE" },
	{ VK_VOLUME_DOWN, "VK_VOLUME_DOWN" },
	{ VK_VOLUME_UP, "VK_VOLUME_UP" },
	{ VK_MEDIA_NEXT_TRACK, "VK_MEDIA_NEXT_TRACK" },
	{ VK_MEDIA_PREV_TRACK, "VK_MEDIA_PREV_TRACK" },
	{ VK_MEDIA_STOP, "VK_MEDIA_STOP" },
	{ VK_MEDIA_PLAY_PAUSE, "VK_MEDIA_PLAY_PAUSE" },
	{ VK_LAUNCH_MAIL, "VK_LAUNCH_MAIL" },
	{ VK_MEDIA_SELECT, "VK_MEDIA_SELECT" },
	{ VK_LAUNCH_APP1, "VK_LAUNCH_APP1" },
	{ VK_LAUNCH_APP2, "VK_LAUNCH_APP2" },
	{ 0, NULL },
	{ 0, NULL },
	{ VK_OEM_1, "VK_OEM_1" },
	{ VK_OEM_PLUS, "VK_OEM_PLUS" },
	{ VK_OEM_COMMA, "VK_OEM_COMMA" },
	{ VK_OEM_MINUS, "VK_OEM_MINUS" },
	{ VK_OEM_PERIOD, "VK_OEM_PERIOD" },
	{ VK_OEM_2, "VK_OEM_2" },
	{ VK_OEM_3, "VK_OEM_3" },
	{ VK_ABNT_C1, "VK_ABNT_C1" },
	{ VK_ABNT_C2, "VK_ABNT_C2" },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ VK_OEM_4, "VK_OEM_4" },
	{ VK_OEM_5, "VK_OEM_5" },
	{ VK_OEM_6, "VK_OEM_6" },
	{ VK_OEM_7, "VK_OEM_7" },
	{ VK_OEM_8, "VK_OEM_8" },
	{ 0, NULL },
	{ 0, NULL },
	{ VK_OEM_102, "VK_OEM_102" },
	{ 0, NULL },
	{ 0, NULL },
	{ VK_PROCESSKEY, "VK_PROCESSKEY" },
	{ 0, NULL },
	{ VK_PACKET, "VK_PACKET" },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ VK_ATTN, "VK_ATTN" },
	{ VK_CRSEL, "VK_CRSEL" },
	{ VK_EXSEL, "VK_EXSEL" },
	{ VK_EREOF, "VK_EREOF" },
	{ VK_PLAY, "VK_PLAY" },
	{ VK_ZOOM, "VK_ZOOM" },
	{ VK_NONAME, "VK_NONAME" },
	{ VK_PA1, "VK_PA1" },
	{ VK_OEM_CLEAR, "VK_OEM_CLEAR" },
	{ 0, NULL }
};

char* GetVirtualKeyName(DWORD vkcode)
{
	char* vkname;

	vkname = (char*) VIRTUAL_KEY_CODE_TABLE[vkcode].name;

	if (!vkname)
		vkname = "VK_NONE";

	return vkname;
}

DWORD GetVirtualKeyCodeFromName(const char* vkname)
{
	int i;

	for (i = 0; i < ARRAYSIZE(VIRTUAL_KEY_CODE_TABLE); i++)
	{
		if (VIRTUAL_KEY_CODE_TABLE[i].name)
		{
			if (strcmp(vkname, VIRTUAL_KEY_CODE_TABLE[i].name) == 0)
				return VIRTUAL_KEY_CODE_TABLE[i].code;
		}
	}

	return VK_NONE;
}

/**
 * Virtual Scan Codes
 */

/**
 * Keyboard Type 4
 */

DWORD KBD4T[128] =
{
	KBD4_T00,
	KBD4_T01,
	KBD4_T02,
	KBD4_T03,
	KBD4_T04,
	KBD4_T05,
	KBD4_T06,
	KBD4_T07,
	KBD4_T08,
	KBD4_T09,
	KBD4_T0A,
	KBD4_T0B,
	KBD4_T0C,
	KBD4_T0D,
	KBD4_T0E,
	KBD4_T0F,
	KBD4_T10,
	KBD4_T11,
	KBD4_T12,
	KBD4_T13,
	KBD4_T14,
	KBD4_T15,
	KBD4_T16,
	KBD4_T17,
	KBD4_T18,
	KBD4_T19,
	KBD4_T1A,
	KBD4_T1B,
	KBD4_T1C,
	KBD4_T1D,
	KBD4_T1E,
	KBD4_T1F,
	KBD4_T20,
	KBD4_T21,
	KBD4_T22,
	KBD4_T23,
	KBD4_T24,
	KBD4_T25,
	KBD4_T26,
	KBD4_T27,
	KBD4_T28,
	KBD4_T29,
	KBD4_T2A,
	KBD4_T2B,
	KBD4_T2C,
	KBD4_T2D,
	KBD4_T2E,
	KBD4_T2F,
	KBD4_T30,
	KBD4_T31,
	KBD4_T32,
	KBD4_T33,
	KBD4_T34,
	KBD4_T35,
	KBD4_T36,
	KBD4_T37,
	KBD4_T38,
	KBD4_T39,
	KBD4_T3A,
	KBD4_T3B,
	KBD4_T3C,
	KBD4_T3D,
	KBD4_T3E,
	KBD4_T3F,
	KBD4_T40,
	KBD4_T41,
	KBD4_T42,
	KBD4_T43,
	KBD4_T44,
	KBD4_T45,
	KBD4_T46,
	KBD4_T47,
	KBD4_T48,
	KBD4_T49,
	KBD4_T4A,
	KBD4_T4B,
	KBD4_T4C,
	KBD4_T4D,
	KBD4_T4E,
	KBD4_T4F,
	KBD4_T50,
	KBD4_T51,
	KBD4_T52,
	KBD4_T53,
	KBD4_T54,
	KBD4_T55,
	KBD4_T56,
	KBD4_T57,
	KBD4_T58,
	KBD4_T59,
	KBD4_T5A,
	KBD4_T5B,
	KBD4_T5C,
	KBD4_T5D,
	KBD4_T5E,
	KBD4_T5F,
	KBD4_T60,
	KBD4_T61,
	KBD4_T62,
	KBD4_T63,
	KBD4_T64,
	KBD4_T65,
	KBD4_T66,
	KBD4_T67,
	KBD4_T68,
	KBD4_T69,
	KBD4_T6A,
	KBD4_T6B,
	KBD4_T6C,
	KBD4_T6D,
	KBD4_T6E,
	KBD4_T6F,
	KBD4_T70,
	KBD4_T71,
	KBD4_T72,
	KBD4_T73,
	KBD4_T74,
	KBD4_T75,
	KBD4_T76,
	KBD4_T77,
	KBD4_T78,
	KBD4_T79,
	KBD4_T7A,
	KBD4_T7B,
	KBD4_T7C,
	KBD4_T7D,
	KBD4_T7E,
	KBD4_T7F
};

DWORD KBD4X[128] =
{
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	KBD4_X10,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	KBD4_X19,
	VK_NONE,
	VK_NONE,
	KBD4_X1C,
	KBD4_X1D,
	VK_NONE,
	VK_NONE,
	KBD4_X20,
	KBD4_X21,
	KBD4_X22,
	VK_NONE,
	KBD4_X24,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	KBD4_X2E,
	VK_NONE,
	KBD4_X30,
	VK_NONE,
	KBD4_X32,
	VK_NONE,
	VK_NONE,
	KBD4_X35,
	VK_NONE,
	KBD4_X37,
	KBD4_X38,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	KBD4_X46,
	KBD4_X47,
	KBD4_X48,
	KBD4_X49,
	VK_NONE,
	KBD4_X4B,
	VK_NONE,
	KBD4_X4D,
	VK_NONE,
	KBD4_X4F,
	KBD4_X50,
	KBD4_X51,
	KBD4_X52,
	KBD4_X53,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	KBD4_X5B,
	KBD4_X5C,
	KBD4_X5D,
	KBD4_X5E,
	KBD4_X5F,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	KBD4_X65,
	KBD4_X66,
	KBD4_X67,
	KBD4_X68,
	KBD4_X69,
	KBD4_X6A,
	KBD4_X6B,
	KBD4_X6C,
	KBD4_X6D,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE
};

/**
 * Keyboard Type 7
 */

DWORD KBD7T[128] =
{
	KBD7_T00,
	KBD7_T01,
	KBD7_T02,
	KBD7_T03,
	KBD7_T04,
	KBD7_T05,
	KBD7_T06,
	KBD7_T07,
	KBD7_T08,
	KBD7_T09,
	KBD7_T0A,
	KBD7_T0B,
	KBD7_T0C,
	KBD7_T0D,
	KBD7_T0E,
	KBD7_T0F,
	KBD7_T10,
	KBD7_T11,
	KBD7_T12,
	KBD7_T13,
	KBD7_T14,
	KBD7_T15,
	KBD7_T16,
	KBD7_T17,
	KBD7_T18,
	KBD7_T19,
	KBD7_T1A,
	KBD7_T1B,
	KBD7_T1C,
	KBD7_T1D,
	KBD7_T1E,
	KBD7_T1F,
	KBD7_T20,
	KBD7_T21,
	KBD7_T22,
	KBD7_T23,
	KBD7_T24,
	KBD7_T25,
	KBD7_T26,
	KBD7_T27,
	KBD7_T28,
	KBD7_T29,
	KBD7_T2A,
	KBD7_T2B,
	KBD7_T2C,
	KBD7_T2D,
	KBD7_T2E,
	KBD7_T2F,
	KBD7_T30,
	KBD7_T31,
	KBD7_T32,
	KBD7_T33,
	KBD7_T34,
	KBD7_T35,
	KBD7_T36,
	KBD7_T37,
	KBD7_T38,
	KBD7_T39,
	KBD7_T3A,
	KBD7_T3B,
	KBD7_T3C,
	KBD7_T3D,
	KBD7_T3E,
	KBD7_T3F,
	KBD7_T40,
	KBD7_T41,
	KBD7_T42,
	KBD7_T43,
	KBD7_T44,
	KBD7_T45,
	KBD7_T46,
	KBD7_T47,
	KBD7_T48,
	KBD7_T49,
	KBD7_T4A,
	KBD7_T4B,
	KBD7_T4C,
	KBD7_T4D,
	KBD7_T4E,
	KBD7_T4F,
	KBD7_T50,
	KBD7_T51,
	KBD7_T52,
	KBD7_T53,
	KBD7_T54,
	KBD7_T55,
	KBD7_T56,
	KBD7_T57,
	KBD7_T58,
	KBD7_T59,
	KBD7_T5A,
	KBD7_T5B,
	KBD7_T5C,
	KBD7_T5D,
	KBD7_T5E,
	KBD7_T5F,
	KBD7_T60,
	KBD7_T61,
	KBD7_T62,
	KBD7_T63,
	KBD7_T64,
	KBD7_T65,
	KBD7_T66,
	KBD7_T67,
	KBD7_T68,
	KBD7_T69,
	KBD7_T6A,
	KBD7_T6B,
	KBD7_T6C,
	KBD7_T6D,
	KBD7_T6E,
	KBD7_T6F,
	KBD7_T70,
	KBD7_T71,
	KBD7_T72,
	KBD7_T73,
	KBD7_T74,
	KBD7_T75,
	KBD7_T76,
	KBD7_T77,
	KBD7_T78,
	KBD7_T79,
	KBD7_T7A,
	KBD7_T7B,
	KBD7_T7C,
	KBD7_T7D,
	KBD7_T7E,
	KBD7_T7F
};

DWORD KBD7X[128] =
{
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	KBD7_X10,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	KBD7_X19,
	VK_NONE,
	VK_NONE,
	KBD7_X1C,
	KBD7_X1D,
	VK_NONE,
	VK_NONE,
	KBD7_X20,
	KBD7_X21,
	KBD7_X22,
	VK_NONE,
	KBD7_X24,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	KBD7_X2E,
	VK_NONE,
	KBD7_X30,
	VK_NONE,
	KBD7_X32,
	KBD7_X33,
	VK_NONE,
	KBD7_X35,
	VK_NONE,
	KBD7_X37,
	KBD7_X38,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	KBD7_X42,
	KBD7_X43,
	KBD7_X44,
	VK_NONE,
	KBD7_X46,
	KBD7_X47,
	KBD7_X48,
	KBD7_X49,
	VK_NONE,
	KBD7_X4B,
	VK_NONE,
	KBD7_X4D,
	VK_NONE,
	KBD7_X4F,
	KBD7_X50,
	KBD7_X51,
	KBD7_X52,
	KBD7_X53,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	KBD7_X5B,
	KBD7_X5C,
	KBD7_X5D,
	KBD7_X5E,
	KBD7_X5F,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	KBD7_X65,
	KBD7_X66,
	KBD7_X67,
	KBD7_X68,
	KBD7_X69,
	KBD7_X6A,
	KBD7_X6B,
	KBD7_X6C,
	KBD7_X6D,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE,
	VK_NONE
};

DWORD GetVirtualKeyCodeFromVirtualScanCode(DWORD scancode, DWORD dwKeyboardType)
{
	DWORD vkcode;

	vkcode = VK_NONE;

	if ((dwKeyboardType != 4) && (dwKeyboardType != 7))
		dwKeyboardType = 4;

	if (dwKeyboardType == 4)
	{
		if (scancode & KBDEXT)
			vkcode = KBD4X[scancode & 0x7F];
		else
			vkcode = KBD4T[scancode & 0x7F];
	}
	else if (dwKeyboardType == 7)
	{
		if (scancode & KBDEXT)
			vkcode = KBD7X[scancode & 0x7F];
		else
			vkcode = KBD7T[scancode & 0x7F];
	}

	return vkcode;
}

DWORD GetVirtualScanCodeFromVirtualKeyCode(DWORD vkcode, DWORD dwKeyboardType)
{
	int i;
	DWORD scancode;

	scancode = 0;

	if ((dwKeyboardType != 4) && (dwKeyboardType != 7))
		dwKeyboardType = 4;

	if (dwKeyboardType == 4)
	{
		for (i = 0; i < 128; i++)
		{
			if (KBD4T[i] == vkcode)
			{
				scancode = i;
				break;
			}
		}

		if (!scancode)
		{
			for (i = 0; i < 128; i++)
			{
				if (KBD4X[i] == vkcode)
				{
					scancode = i;
					break;
				}
			}
		}
	}
	else if (dwKeyboardType == 7)
	{
		for (i = 0; i < 128; i++)
		{
			if (KBD7T[i] == vkcode)
			{
				scancode = i;
				break;
			}
		}

		if (!scancode)
		{
			for (i = 0; i < 128; i++)
			{
				if (KBD7X[i] == vkcode)
				{
					scancode = i;
					break;
				}
			}
		}
	}

	return scancode;
}
