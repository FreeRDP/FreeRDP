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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include <winpr/crt.h>

#include <freerdp/types.h>
#include <freerdp/locale/keyboard.h>
#include <freerdp/locale/locale.h>

#include "liblocale.h"

#ifdef WITH_X11

#include "keyboard_x11.h"

#ifdef WITH_XKBFILE
#include "keyboard_xkbfile.h"
#endif

#endif

DWORD VIRTUAL_SCANCODE_TO_X11_KEYCODE[256][2];
DWORD X11_KEYCODE_TO_VIRTUAL_SCANCODE[256];

int freerdp_detect_keyboard(DWORD* keyboardLayoutId)
{
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

int freerdp_keyboard_init_apple(DWORD* keyboardLayoutId, DWORD x11_keycode_to_rdp_scancode[256])
{
	DWORD vkcode;
	DWORD keycode;
	DWORD keycode_to_vkcode[256];

	ZeroMemory(keycode_to_vkcode, sizeof(keycode_to_vkcode));

	for (keycode = 0; keycode < 256; keycode++)
	{
		vkcode = keycode_to_vkcode[keycode] = GetVirtualKeyCodeFromKeycode(keycode, KEYCODE_TYPE_APPLE);
		x11_keycode_to_rdp_scancode[keycode] = GetVirtualScanCodeFromVirtualKeyCode(vkcode, 4);
	}

	return 0;
}

int freerdp_keyboard_init_x11_evdev(DWORD* keyboardLayoutId, DWORD x11_keycode_to_rdp_scancode[256])
{
	DWORD vkcode;
	DWORD keycode;
	DWORD keycode_to_vkcode[256];

	ZeroMemory(keycode_to_vkcode, sizeof(keycode_to_vkcode));

	for (keycode = 0; keycode < 256; keycode++)
	{
		vkcode = keycode_to_vkcode[keycode] = GetVirtualKeyCodeFromKeycode(keycode, KEYCODE_TYPE_EVDEV);
		x11_keycode_to_rdp_scancode[keycode] = GetVirtualScanCodeFromVirtualKeyCode(vkcode, 4);
	}

	return 0;
}

DWORD freerdp_keyboard_init(DWORD keyboardLayoutId)
{
	DWORD keycode;
#if defined(__APPLE__) || defined(WITH_X11) || defined(WITH_WAYLAND)
	int status = -1;
#endif

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
		status = freerdp_keyboard_init_x11_evdev(&keyboardLayoutId, X11_KEYCODE_TO_VIRTUAL_SCANCODE);

#endif

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

DWORD freerdp_keyboard_get_rdp_scancode_from_x11_keycode(DWORD keycode)
{
	DEBUG_KBD("x11 keycode: %02"PRIX32" -> rdp code: %02"PRIX8"%s", keycode,
		RDP_SCANCODE_CODE(X11_KEYCODE_TO_VIRTUAL_SCANCODE[keycode]),
		RDP_SCANCODE_EXTENDED(X11_KEYCODE_TO_VIRTUAL_SCANCODE[keycode]) ? " extended" : "");

	return X11_KEYCODE_TO_VIRTUAL_SCANCODE[keycode];
}

DWORD freerdp_keyboard_get_x11_keycode_from_rdp_scancode(DWORD scancode, BOOL extended)
{
	if (extended)
		return VIRTUAL_SCANCODE_TO_X11_KEYCODE[scancode][1];
	else
		return VIRTUAL_SCANCODE_TO_X11_KEYCODE[scancode][0];
}
