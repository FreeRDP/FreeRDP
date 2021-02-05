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

#if defined(__MACOSX__)
#include "keyboard_apple.h"
#endif

#ifdef WITH_X11

#include "keyboard_x11.h"

#ifdef WITH_XKBFILE
#include "keyboard_xkbfile.h"
#endif

#endif

static DWORD VIRTUAL_SCANCODE_TO_X11_KEYCODE[256][2] = { 0 };
static DWORD X11_KEYCODE_TO_VIRTUAL_SCANCODE[256] = { 0 };
static DWORD REMAPPING_TABLE[0x10000] = { 0 };

int freerdp_detect_keyboard(DWORD* keyboardLayoutId)
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
		status =
		    freerdp_keyboard_init_x11_evdev(&keyboardLayoutId, X11_KEYCODE_TO_VIRTUAL_SCANCODE);

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

DWORD freerdp_keyboard_init_ex(DWORD keyboardLayoutId, const char* keyboardRemappingList)
{
	DWORD rc = freerdp_keyboard_init(keyboardLayoutId);

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
	return rc;
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
