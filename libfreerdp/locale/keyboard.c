/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Keyboard Localization
 *
 * Copyright 2009-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/config.h>

#include <stdio.h>
#include <string.h>

#include <winpr/crt.h>

#include <freerdp/types.h>
#include <freerdp/locale/keyboard.h>
#include <freerdp/locale/locale.h>

#include <freerdp/log.h>

#include "liblocale.h"

#if defined(__MACOSX__)
#include "keyboard_apple.h"
#endif

#define TAG FREERDP_TAG("locale.keyboard")

#ifdef WITH_X11

#include "keyboard_x11.h"

#ifdef WITH_XKBFILE
#include "keyboard_xkbfile.h"
#endif

#endif

static DWORD VIRTUAL_SCANCODE_TO_X11_KEYCODE[256][2] = { 0 };
static DWORD X11_KEYCODE_TO_VIRTUAL_SCANCODE[256] = { 0 };
static DWORD REMAPPING_TABLE[0x10000] = { 0 };

struct scancode_map_entry
{
	DWORD scancode;
	const char* name;
};

static const struct scancode_map_entry RDP_SCANCODE_MAP[] = {
	{ RDP_SCANCODE_ESCAPE, "VK_ESCAPE" },
	{ RDP_SCANCODE_KEY_1, "VK_KEY_1" },
	{ RDP_SCANCODE_KEY_2, "VK_KEY_2" },
	{ RDP_SCANCODE_KEY_3, "VK_KEY_3" },
	{ RDP_SCANCODE_KEY_4, "VK_KEY_4" },
	{ RDP_SCANCODE_KEY_5, "VK_KEY_5" },
	{ RDP_SCANCODE_KEY_6, "VK_KEY_6" },
	{ RDP_SCANCODE_KEY_7, "VK_KEY_7" },
	{ RDP_SCANCODE_KEY_8, "VK_KEY_8" },
	{ RDP_SCANCODE_KEY_9, "VK_KEY_9" },
	{ RDP_SCANCODE_KEY_0, "VK_KEY_0" },
	{ RDP_SCANCODE_OEM_MINUS, "VK_OEM_MINUS" },
	{ RDP_SCANCODE_OEM_PLUS, "VK_OEM_PLUS" },
	{ RDP_SCANCODE_BACKSPACE, "VK_BACK Backspace" },
	{ RDP_SCANCODE_TAB, "VK_TAB" },
	{ RDP_SCANCODE_KEY_Q, "VK_KEY_Q" },
	{ RDP_SCANCODE_KEY_W, "VK_KEY_W" },
	{ RDP_SCANCODE_KEY_E, "VK_KEY_E" },
	{ RDP_SCANCODE_KEY_R, "VK_KEY_R" },
	{ RDP_SCANCODE_KEY_T, "VK_KEY_T" },
	{ RDP_SCANCODE_KEY_Y, "VK_KEY_Y" },
	{ RDP_SCANCODE_KEY_U, "VK_KEY_U" },
	{ RDP_SCANCODE_KEY_I, "VK_KEY_I" },
	{ RDP_SCANCODE_KEY_O, "VK_KEY_O" },
	{ RDP_SCANCODE_KEY_P, "VK_KEY_P" },
	{ RDP_SCANCODE_OEM_4, "VK_OEM_4 '[' on US" },
	{ RDP_SCANCODE_OEM_6, "VK_OEM_6 ']' on US" },
	{ RDP_SCANCODE_RETURN, "VK_RETURN Normal Enter" },
	{ RDP_SCANCODE_LCONTROL, "VK_LCONTROL" },
	{ RDP_SCANCODE_KEY_A, "VK_KEY_A" },
	{ RDP_SCANCODE_KEY_S, "VK_KEY_S" },
	{ RDP_SCANCODE_KEY_D, "VK_KEY_D" },
	{ RDP_SCANCODE_KEY_F, "VK_KEY_F" },
	{ RDP_SCANCODE_KEY_G, "VK_KEY_G" },
	{ RDP_SCANCODE_KEY_H, "VK_KEY_H" },
	{ RDP_SCANCODE_KEY_J, "VK_KEY_J" },
	{ RDP_SCANCODE_KEY_K, "VK_KEY_K" },
	{ RDP_SCANCODE_KEY_L, "VK_KEY_L" },
	{ RDP_SCANCODE_OEM_1, "VK_OEM_1 ';' on US" },
	{ RDP_SCANCODE_OEM_7, "VK_OEM_7 on US" },
	{ RDP_SCANCODE_OEM_3, "VK_OEM_3 Top left, '`' on US, JP DBE_SBCSCHAR" },
	{ RDP_SCANCODE_LSHIFT, "VK_LSHIFT" },
	{ RDP_SCANCODE_OEM_5, "VK_OEM_5 Next to Enter, '\' on US" },
	{ RDP_SCANCODE_KEY_Z, "VK_KEY_Z" },
	{ RDP_SCANCODE_KEY_X, "VK_KEY_X" },
	{ RDP_SCANCODE_KEY_C, "VK_KEY_C" },
	{ RDP_SCANCODE_KEY_V, "VK_KEY_V" },
	{ RDP_SCANCODE_KEY_B, "VK_KEY_B" },
	{ RDP_SCANCODE_KEY_N, "VK_KEY_N" },
	{ RDP_SCANCODE_KEY_M, "VK_KEY_M" },
	{ RDP_SCANCODE_OEM_COMMA, "VK_OEM_COMMA" },
	{ RDP_SCANCODE_OEM_PERIOD, "VK_OEM_PERIOD" },
	{ RDP_SCANCODE_OEM_2, "VK_OEM_2 '/' on US" },
	{ RDP_SCANCODE_RSHIFT, "VK_RSHIFT" },
	{ RDP_SCANCODE_MULTIPLY, "VK_MULTIPLY Numerical" },
	{ RDP_SCANCODE_LMENU, "VK_LMENU Left 'Alt' key" },
	{ RDP_SCANCODE_SPACE, "VK_SPACE" },
	{ RDP_SCANCODE_CAPSLOCK, "VK_CAPITAL 'Caps Lock', JP DBE_ALPHANUMERIC" },
	{ RDP_SCANCODE_F1, "VK_F1" },
	{ RDP_SCANCODE_F2, "VK_F2" },
	{ RDP_SCANCODE_F3, "VK_F3" },
	{ RDP_SCANCODE_F4, "VK_F4" },
	{ RDP_SCANCODE_F5, "VK_F5" },
	{ RDP_SCANCODE_F6, "VK_F6" },
	{ RDP_SCANCODE_F7, "VK_F7" },
	{ RDP_SCANCODE_F8, "VK_F8" },
	{ RDP_SCANCODE_F9, "VK_F9" },
	{ RDP_SCANCODE_F10, "VK_F10" },
	{ RDP_SCANCODE_NUMLOCK, "VK_NUMLOCK" },
	{ RDP_SCANCODE_SCROLLLOCK, "VK_SCROLL 'Scroll Lock', JP OEM_SCROLL" },
	{ RDP_SCANCODE_NUMPAD7, "VK_NUMPAD7" },
	{ RDP_SCANCODE_NUMPAD8, "VK_NUMPAD8" },
	{ RDP_SCANCODE_NUMPAD9, "VK_NUMPAD9" },
	{ RDP_SCANCODE_SUBTRACT, "VK_SUBTRACT" },
	{ RDP_SCANCODE_NUMPAD4, "VK_NUMPAD4" },
	{ RDP_SCANCODE_NUMPAD5, "VK_NUMPAD5" },
	{ RDP_SCANCODE_NUMPAD6, "VK_NUMPAD6" },
	{ RDP_SCANCODE_ADD, "VK_ADD" },
	{ RDP_SCANCODE_NUMPAD1, "VK_NUMPAD1" },
	{ RDP_SCANCODE_NUMPAD2, "VK_NUMPAD2" },
	{ RDP_SCANCODE_NUMPAD3, "VK_NUMPAD3" },
	{ RDP_SCANCODE_NUMPAD0, "VK_NUMPAD0" },
	{ RDP_SCANCODE_DECIMAL, "VK_DECIMAL Numerical, '.' on US" },
	{ RDP_SCANCODE_SYSREQ, "Sys Req" },
	{ RDP_SCANCODE_OEM_102, "VK_OEM_102 Lower left '\' on US" },
	{ RDP_SCANCODE_F11, "VK_F11" },
	{ RDP_SCANCODE_F12, "VK_F12" },
	{ RDP_SCANCODE_SLEEP, "VK_SLEEP OEM_8 on FR (undocumented?)" },
	{ RDP_SCANCODE_ZOOM, "VK_ZOOM (undocumented?)" },
	{ RDP_SCANCODE_HELP, "VK_HELP (undocumented?)" },
	{ RDP_SCANCODE_F13, "VK_F13" },
	{ RDP_SCANCODE_F14, "VK_F14" },
	{ RDP_SCANCODE_F15, "VK_F15" },
	{ RDP_SCANCODE_F16, "VK_F16" },
	{ RDP_SCANCODE_F17, "VK_F17" },
	{ RDP_SCANCODE_F18, "VK_F18" },
	{ RDP_SCANCODE_F19, "VK_F19" },
	{ RDP_SCANCODE_F20, "VK_F20" },
	{ RDP_SCANCODE_F21, "VK_F21" },
	{ RDP_SCANCODE_F22, "VK_F22" },
	{ RDP_SCANCODE_F23, "VK_F23" },
	{ RDP_SCANCODE_F24, "VK_F24" },
	{ RDP_SCANCODE_HIRAGANA, "JP DBE_HIRAGANA" },
	{ RDP_SCANCODE_HANJA_KANJI, "VK_HANJA / VK_KANJI (undocumented?)" },
	{ RDP_SCANCODE_KANA_HANGUL, "VK_KANA / VK_HANGUL (undocumented?)" },
	{ RDP_SCANCODE_ABNT_C1, "VK_ABNT_C1 JP OEM_102" },
	{ RDP_SCANCODE_F24_JP, "JP F24" },
	{ RDP_SCANCODE_CONVERT_JP, "JP VK_CONVERT" },
	{ RDP_SCANCODE_NONCONVERT_JP, "JP VK_NONCONVERT" },
	{ RDP_SCANCODE_TAB_JP, "JP TAB" },
	{ RDP_SCANCODE_BACKSLASH_JP, "JP OEM_5 ('\')" },
	{ RDP_SCANCODE_ABNT_C2, "VK_ABNT_C2, JP" },
	{ RDP_SCANCODE_HANJA, "KR VK_HANJA" },
	{ RDP_SCANCODE_HANGUL, "KR VK_HANGUL" },
	{ RDP_SCANCODE_RETURN_KP, "not RDP_SCANCODE_RETURN Numerical Enter" },
	{ RDP_SCANCODE_RCONTROL, "VK_RCONTROL" },
	{ RDP_SCANCODE_DIVIDE, "VK_DIVIDE Numerical" },
	{ RDP_SCANCODE_PRINTSCREEN, "VK_EXECUTE/VK_PRINT/VK_SNAPSHOT Print Screen" },
	{ RDP_SCANCODE_RMENU, "VK_RMENU Right 'Alt' / 'Alt Gr'" },
	{ RDP_SCANCODE_PAUSE, "VK_PAUSE Pause / Break (Slightly special handling)" },
	{ RDP_SCANCODE_HOME, "VK_HOME" },
	{ RDP_SCANCODE_UP, "VK_UP" },
	{ RDP_SCANCODE_PRIOR, "VK_PRIOR 'Page Up'" },
	{ RDP_SCANCODE_LEFT, "VK_LEFT" },
	{ RDP_SCANCODE_RIGHT, "VK_RIGHT" },
	{ RDP_SCANCODE_END, "VK_END" },
	{ RDP_SCANCODE_DOWN, "VK_DOWN" },
	{ RDP_SCANCODE_NEXT, "VK_NEXT 'Page Down'" },
	{ RDP_SCANCODE_INSERT, "VK_INSERT" },
	{ RDP_SCANCODE_DELETE, "VK_DELETE" },
	{ RDP_SCANCODE_NULL, "<00>" },
	{ RDP_SCANCODE_HELP2, "Help - documented, different from VK_HELP" },
	{ RDP_SCANCODE_LWIN, "VK_LWIN" },
	{ RDP_SCANCODE_RWIN, "VK_RWIN" },
	{ RDP_SCANCODE_APPS, "VK_APPS Application" },
	{ RDP_SCANCODE_POWER_JP, "JP POWER" },
	{ RDP_SCANCODE_SLEEP_JP, "JP SLEEP" },
	{ RDP_SCANCODE_NUMLOCK_EXTENDED, "should be RDP_SCANCODE_NUMLOCK" },
	{ RDP_SCANCODE_RSHIFT_EXTENDED, "should be RDP_SCANCODE_RSHIFT" },
	{ RDP_SCANCODE_VOLUME_MUTE, "VK_VOLUME_MUTE" },
	{ RDP_SCANCODE_VOLUME_DOWN, "VK_VOLUME_DOWN" },
	{ RDP_SCANCODE_VOLUME_UP, "VK_VOLUME_UP" },
	{ RDP_SCANCODE_MEDIA_NEXT_TRACK, "VK_MEDIA_NEXT_TRACK" },
	{ RDP_SCANCODE_MEDIA_PREV_TRACK, "VK_MEDIA_PREV_TRACK" },
	{ RDP_SCANCODE_MEDIA_STOP, "VK_MEDIA_MEDIA_STOP" },
	{ RDP_SCANCODE_MEDIA_PLAY_PAUSE, "VK_MEDIA_MEDIA_PLAY_PAUSE" },
	{ RDP_SCANCODE_BROWSER_BACK, "VK_BROWSER_BACK" },
	{ RDP_SCANCODE_BROWSER_FORWARD, "VK_BROWSER_FORWARD" },
	{ RDP_SCANCODE_BROWSER_REFRESH, "VK_BROWSER_REFRESH" },
	{ RDP_SCANCODE_BROWSER_STOP, "VK_BROWSER_STOP" },
	{ RDP_SCANCODE_BROWSER_SEARCH, "VK_BROWSER_SEARCH" },
	{ RDP_SCANCODE_BROWSER_FAVORITES, "VK_BROWSER_FAVORITES" },
	{ RDP_SCANCODE_BROWSER_HOME, "VK_BROWSER_HOME" },
	{ RDP_SCANCODE_LAUNCH_MAIL, "VK_LAUNCH_MAIL" },
	{ RDP_SCANCODE_LAUNCH_MEDIA_SELECT, "VK_LAUNCH_MEDIA_SELECT" },
	{ RDP_SCANCODE_LAUNCH_APP1, "VK_LAUNCH_APP1" },
	{ RDP_SCANCODE_LAUNCH_APP2, "VK_LAUNCH_APP2" },
};

static int freerdp_detect_keyboard(DWORD* keyboardLayoutId)
{
#if defined(_WIN32)
	CHAR name[KL_NAMELENGTH + 1] = { 0 };
	if (GetKeyboardLayoutNameA(name))
	{
		ULONG rc;

		errno = 0;
		rc = strtoul(name, NULL, 16);
		if (errno == 0)
			*keyboardLayoutId = rc;
	}

	if (*keyboardLayoutId == 0)
		*keyboardLayoutId = ((DWORD)GetKeyboardLayout(0) >> 16) & 0x0000FFFF;
#endif

#if defined(__MACOSX__)
	if (*keyboardLayoutId == 0)
		freerdp_detect_keyboard_layout_from_cf(keyboardLayoutId);
#endif

#ifdef WITH_X11
	if (*keyboardLayoutId == 0)
		freerdp_detect_keyboard_layout_from_xkb(keyboardLayoutId);
#endif

	if (*keyboardLayoutId == 0)
		freerdp_detect_keyboard_layout_from_system_locale(keyboardLayoutId);

	if (*keyboardLayoutId == 0)
		*keyboardLayoutId = ENGLISH_UNITED_STATES;

	return 0;
}

static int freerdp_keyboard_init_apple(DWORD* keyboardLayoutId,
                                       DWORD x11_keycode_to_rdp_scancode[256])
{
	DWORD vkcode;
	DWORD keycode;
	DWORD keycode_to_vkcode[256];

	ZeroMemory(keycode_to_vkcode, sizeof(keycode_to_vkcode));

	for (keycode = 0; keycode < 256; keycode++)
	{
		vkcode = keycode_to_vkcode[keycode] =
		    GetVirtualKeyCodeFromKeycode(keycode, KEYCODE_TYPE_APPLE);
		x11_keycode_to_rdp_scancode[keycode] = GetVirtualScanCodeFromVirtualKeyCode(vkcode, 4);
	}

	return 0;
}

static int freerdp_keyboard_init_x11_evdev(DWORD* keyboardLayoutId,
                                           DWORD x11_keycode_to_rdp_scancode[256])
{
	DWORD vkcode;
	DWORD keycode;
	DWORD keycode_to_vkcode[256];

	ZeroMemory(keycode_to_vkcode, sizeof(keycode_to_vkcode));

	for (keycode = 0; keycode < 256; keycode++)
	{
		vkcode = keycode_to_vkcode[keycode] =
		    GetVirtualKeyCodeFromKeycode(keycode, KEYCODE_TYPE_EVDEV);
		x11_keycode_to_rdp_scancode[keycode] = GetVirtualScanCodeFromVirtualKeyCode(vkcode, 4);
	}

	return 0;
}

DWORD freerdp_keyboard_init(DWORD keyboardLayoutId)
{
	DWORD keycode;
	int status = -1;

#ifdef __APPLE__
	if (status < 0)
		status = freerdp_keyboard_init_apple(&keyboardLayoutId, X11_KEYCODE_TO_VIRTUAL_SCANCODE);
#endif

#if defined(WITH_X11) || defined(WITH_WAYLAND)

#ifdef WITH_XKBFILE
	if (status < 0)
		status = freerdp_keyboard_init_xkbfile(&keyboardLayoutId, X11_KEYCODE_TO_VIRTUAL_SCANCODE);
#endif

	if (status < 0)
		status =
		    freerdp_keyboard_init_x11_evdev(&keyboardLayoutId, X11_KEYCODE_TO_VIRTUAL_SCANCODE);

#endif

	if (status < 0)
		WLog_DBG(TAG, "Platform keyboard detection failed, trying autodetection");

	freerdp_detect_keyboard(&keyboardLayoutId);

	ZeroMemory(VIRTUAL_SCANCODE_TO_X11_KEYCODE, sizeof(VIRTUAL_SCANCODE_TO_X11_KEYCODE));

	for (keycode = 0; keycode < ARRAYSIZE(VIRTUAL_SCANCODE_TO_X11_KEYCODE); keycode++)
	{
		VIRTUAL_SCANCODE_TO_X11_KEYCODE
		[RDP_SCANCODE_CODE(X11_KEYCODE_TO_VIRTUAL_SCANCODE[keycode])]
		    [RDP_SCANCODE_EXTENDED(X11_KEYCODE_TO_VIRTUAL_SCANCODE[keycode]) ? 1 : 0] = keycode;
	}

	return keyboardLayoutId;
}

DWORD freerdp_keyboard_init_ex(DWORD keyboardLayoutId, const char* keyboardRemappingList)
{
	DWORD res = freerdp_keyboard_init(keyboardLayoutId);

	memset(REMAPPING_TABLE, 0, sizeof(REMAPPING_TABLE));
	if (keyboardRemappingList)
	{
		char* copy = _strdup(keyboardRemappingList);
		char* context = NULL;
		char* token;
		if (!copy)
			goto fail;
		token = strtok_s(copy, ",", &context);
		while (token)
		{
			DWORD key, value;
			int rc = sscanf(token, "%" PRIu32 "=%" PRIu32, &key, &value);
			if (rc != 2)
				rc = sscanf(token, "%" PRIx32 "=%" PRIx32 "", &key, &value);
			if (rc != 2)
				rc = sscanf(token, "%" PRIu32 "=%" PRIx32, &key, &value);
			if (rc != 2)
				rc = sscanf(token, "%" PRIx32 "=%" PRIu32, &key, &value);
			if (rc != 2)
				goto fail;
			if (key >= ARRAYSIZE(REMAPPING_TABLE))
				goto fail;
			REMAPPING_TABLE[key] = value;
			token = strtok_s(NULL, ",", &context);
		}
	fail:
		free(copy);
	}
	return res;
}

DWORD freerdp_keyboard_get_rdp_scancode_from_x11_keycode(DWORD keycode)
{
	const DWORD scancode = X11_KEYCODE_TO_VIRTUAL_SCANCODE[keycode];
	const DWORD remapped = REMAPPING_TABLE[scancode];
	DEBUG_KBD("x11 keycode: %02" PRIX32 " -> rdp code: [%04" PRIx16 "] %02" PRIX8 "%s", keycode,
	          scancode, RDP_SCANCODE_CODE(scancode),
	          RDP_SCANCODE_EXTENDED(scancode) ? " extended" : "");

	if (remapped != 0)
	{
		DEBUG_KBD("remapped scancode: [%04" PRIx16 "] %02" PRIX8 "[%s] -> [%04" PRIx16 "] %02" PRIX8
		          "[%s]",
		          scancode, RDP_SCANCODE_CODE(scancode),
		          RDP_SCANCODE_EXTENDED(scancode) ? " extended" : "", remapped,
		          RDP_SCANCODE_CODE(remapped), RDP_SCANCODE_EXTENDED(remapped) ? " extended" : "");
		return remapped;
	}
	return scancode;
}

DWORD freerdp_keyboard_get_x11_keycode_from_rdp_scancode(DWORD scancode, BOOL extended)
{
	if (extended)
		return VIRTUAL_SCANCODE_TO_X11_KEYCODE[scancode][1];
	else
		return VIRTUAL_SCANCODE_TO_X11_KEYCODE[scancode][0];
}

const char* freerdp_keyboard_scancode_name(DWORD scancode)
{
	size_t x;

	for (x = 0; x < ARRAYSIZE(RDP_SCANCODE_MAP); x++)
	{
		const struct scancode_map_entry* entry = &RDP_SCANCODE_MAP[x];
		if (entry->scancode == scancode)
			return entry->name;
	}

	return NULL;
}
