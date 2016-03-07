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
	{ VK_HKTG, "VK_HKTG" },
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

struct _XKB_KEYNAME
{
	const char* name;
	DWORD vkcode;
};
typedef struct _XKB_KEYNAME XKB_KEYNAME;

XKB_KEYNAME XKB_KEYNAME_TABLE[] =
{
	{ "BKSP",	VK_BACK },
	{ "TAB",	VK_TAB },
	{ "RTRN",	VK_RETURN },
	{ "LFSH",	VK_LSHIFT },
	{ "LALT",	VK_LMENU },
	{ "CAPS",	VK_CAPITAL },
	{ "ESC",	VK_ESCAPE },
	{ "SPCE",	VK_SPACE },
	{ "AE10",	VK_KEY_0 },
	{ "AE01",	VK_KEY_1 },
	{ "AE02",	VK_KEY_2 },
	{ "AE03",	VK_KEY_3 },
	{ "AE04",	VK_KEY_4 },
	{ "AE05",	VK_KEY_5 },
	{ "AE06",	VK_KEY_6 },
	{ "AE07",	VK_KEY_7 },
	{ "AE08",	VK_KEY_8 },
	{ "AE09",	VK_KEY_9 },
	{ "AC01",	VK_KEY_A },
	{ "AB05",	VK_KEY_B },
	{ "AB03",	VK_KEY_C },
	{ "AC03",	VK_KEY_D },
	{ "AD03",	VK_KEY_E },
	{ "AC04",	VK_KEY_F },
	{ "AC05",	VK_KEY_G },
	{ "AC06",	VK_KEY_H },
	{ "AD08",	VK_KEY_I },
	{ "AC07",	VK_KEY_J },
	{ "AC08",	VK_KEY_K },
	{ "AC09",	VK_KEY_L },
	{ "AB07",	VK_KEY_M },
	{ "AB06",	VK_KEY_N },
	{ "AD09",	VK_KEY_O },
	{ "AD10",	VK_KEY_P },
	{ "AD01",	VK_KEY_Q },
	{ "AD04",	VK_KEY_R },
	{ "AC02",	VK_KEY_S },
	{ "AD05",	VK_KEY_T },
	{ "AD07",	VK_KEY_U },
	{ "AB04",	VK_KEY_V },
	{ "AD02",	VK_KEY_W },
	{ "AB02",	VK_KEY_X },
	{ "AD06",	VK_KEY_Y },
	{ "AB01",	VK_KEY_Z },
	{ "KP0",	VK_NUMPAD0 },
	{ "KP1",	VK_NUMPAD1 },
	{ "KP2",	VK_NUMPAD2 },
	{ "KP3",	VK_NUMPAD3 },
	{ "KP4",	VK_NUMPAD4 },
	{ "KP5",	VK_NUMPAD5 },
	{ "KP6",	VK_NUMPAD6 },
	{ "KP7",	VK_NUMPAD7 },
	{ "KP8",	VK_NUMPAD8 },
	{ "KP9",	VK_NUMPAD9 },
	{ "KPMU",	VK_MULTIPLY },
	{ "KPAD",	VK_ADD },
	{ "KPSU",	VK_SUBTRACT },
	{ "KPDL",	VK_DECIMAL },
	{ "AB10",	VK_OEM_2 },
	{ "FK01",	VK_F1 },
	{ "FK02",	VK_F2 },
	{ "FK03",	VK_F3 },
	{ "FK04",	VK_F4 },
	{ "FK05",	VK_F5 },
	{ "FK06",	VK_F6 },
	{ "FK07",	VK_F7 },
	{ "FK08",	VK_F8 },
	{ "FK09",	VK_F9 },
	{ "FK10",	VK_F10 },
	{ "FK11",	VK_F11 },
	{ "FK12",	VK_F12 },
	{ "NMLK",	VK_NUMLOCK },
	{ "SCLK",	VK_SCROLL },
	{ "RTSH",	VK_RSHIFT },
	{ "LCTL",	VK_LCONTROL },
	{ "AC10",	VK_OEM_1 },
	{ "AE12",	VK_OEM_PLUS },
	{ "AB08",	VK_OEM_COMMA },
	{ "AE11",	VK_OEM_MINUS },
	{ "AB09",	VK_OEM_PERIOD },
	{ "TLDE",	VK_OEM_3 },
	{ "AB11",	VK_ABNT_C1 },
	{ "I129",	VK_ABNT_C2 },
	{ "AD11",	VK_OEM_4 },
	{ "BKSL",	VK_OEM_5 },
	{ "AD12",	VK_OEM_6 },
	{ "AC11",	VK_OEM_7 },
	{ "LSGT",	VK_OEM_102 },
	{ "KPEN",	VK_RETURN | KBDEXT },
	{ "PAUS",	VK_PAUSE | KBDEXT },
	{ "PGUP",	VK_PRIOR | KBDEXT },
	{ "PGDN",	VK_NEXT | KBDEXT },
	{ "END",	VK_END | KBDEXT },
	{ "HOME",	VK_HOME | KBDEXT },
	{ "LEFT",	VK_LEFT | KBDEXT },
	{ "UP",		VK_UP | KBDEXT },
	{ "RGHT",	VK_RIGHT | KBDEXT },
	{ "DOWN",	VK_DOWN | KBDEXT },
	{ "PRSC",	VK_SNAPSHOT | KBDEXT },
	{ "INS",	VK_INSERT | KBDEXT },
	{ "DELE",	VK_DELETE | KBDEXT },
	{ "LWIN",	VK_LWIN | KBDEXT },
	{ "RWIN",	VK_RWIN | KBDEXT },
	{ "COMP",	VK_APPS | KBDEXT },
	{ "KPDV",	VK_DIVIDE | KBDEXT },
	{ "RCTL",	VK_RCONTROL | KBDEXT },
	{ "RALT",	VK_RMENU | KBDEXT },

	/* Japanese */

	{ "HENK",       VK_CONVERT },
	{ "MUHE",       VK_NONCONVERT },
	{ "HKTG",       VK_HKTG },

//	{ "AE13",	VK_BACKSLASH_JP }, // JP
//	{ "LVL3",	0x54}
};

char* GetVirtualKeyName(DWORD vkcode)
{
	char* vkname = NULL;

	if (vkcode < ARRAYSIZE(VIRTUAL_KEY_CODE_TABLE))
		vkname = (char*) VIRTUAL_KEY_CODE_TABLE[vkcode].name;

	if (!vkname)
		vkname = "VK_NONE";

	return vkname;
}

DWORD GetVirtualKeyCodeFromName(const char* vkname)
{
	unsigned long i;

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

DWORD GetVirtualKeyCodeFromXkbKeyName(const char* xkbname)
{
	unsigned long i;

	for (i = 0; i < ARRAYSIZE(XKB_KEYNAME_TABLE); i++)
	{
		if (XKB_KEYNAME_TABLE[i].name)
		{
			if (strcmp(xkbname, XKB_KEYNAME_TABLE[i].name) == 0)
				return XKB_KEYNAME_TABLE[i].vkcode;
		}
	}

	return VK_NONE;
}
