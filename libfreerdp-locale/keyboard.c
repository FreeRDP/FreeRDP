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

#include <stdio.h>
#include <string.h>

#include <freerdp/types.h>
#include <freerdp/utils/memory.h>
#include <freerdp/locale/keyboard.h>
#include <freerdp/locale/locale.h>

#include "keyboard_keymap.h"
#include "liblocale.h"

#ifdef WITH_X11
#include "keyboard_x11.h"

#ifdef WITH_XKBFILE
#include "keyboard_xkbfile.h"
#endif

#endif

uint32 RDP_SCANCODE_TO_X11_KEYCODE[256][2];
RDP_SCANCODE X11_KEYCODE_TO_RDP_SCANCODE[256];

extern const RDP_SCANCODE VIRTUAL_KEY_CODE_TO_DEFAULT_RDP_SCANCODE_TABLE[256];

uint32 freerdp_detect_keyboard(uint32 keyboardLayoutID)
{
	if (keyboardLayoutID != 0)
		DEBUG_KBD("keyboard layout configuration: %X", keyboardLayoutID);

	if (keyboardLayoutID == 0)
	{
		keyboardLayoutID = freerdp_detect_keyboard_layout_from_system_locale();
		DEBUG_KBD("detect_keyboard_layout_from_locale: %X", keyboardLayoutID);
	}

	if (keyboardLayoutID == 0)
	{
		keyboardLayoutID = 0x0409;
		DEBUG_KBD("using default keyboard layout: %X", keyboardLayoutID);
	}

	return keyboardLayoutID;
}

uint32 freerdp_keyboard_init(uint32 keyboardLayoutId)
{
	uint32 keycode;

#ifdef WITH_X11

#ifdef WITH_XKBFILE
	keyboardLayoutId = freerdp_keyboard_init_xkbfile(keyboardLayoutId, X11_KEYCODE_TO_RDP_SCANCODE);
#else
	keyboardLayoutId = freerdp_keyboard_init_x11(keyboardLayoutId, X11_KEYCODE_TO_RDP_SCANCODE);
#endif

#endif
	keyboardLayoutId = freerdp_detect_keyboard(keyboardLayoutId);

	memset(RDP_SCANCODE_TO_X11_KEYCODE, 0, sizeof(RDP_SCANCODE_TO_X11_KEYCODE));
	for (keycode=0; keycode < ARRAY_SIZE(RDP_SCANCODE_TO_X11_KEYCODE); keycode++)
		RDP_SCANCODE_TO_X11_KEYCODE
			[rdp_scancode_code(X11_KEYCODE_TO_RDP_SCANCODE[keycode])]
			[rdp_scancode_extended(X11_KEYCODE_TO_RDP_SCANCODE[keycode]) ? 1 : 0] = keycode;

	return keyboardLayoutId;
}

RDP_SCANCODE freerdp_keyboard_get_rdp_scancode_from_x11_keycode(uint32 keycode)
{
	DEBUG_KBD("x11 keycode: %02X -> rdp code: %02X%s", keycode,
		rdp_scancode_code(X11_KEYCODE_TO_RDP_SCANCODE[keycode]),
		rdp_scancode_extended(X11_KEYCODE_TO_RDP_SCANCODE[keycode]) ? " extended" : "");

	return X11_KEYCODE_TO_RDP_SCANCODE[keycode];
}

uint32 freerdp_keyboard_get_x11_keycode_from_rdp_scancode(uint32 scancode, boolean extended)
{
	if (extended)
		return RDP_SCANCODE_TO_X11_KEYCODE[scancode][1];
	else
		return RDP_SCANCODE_TO_X11_KEYCODE[scancode][0];
}

RDP_SCANCODE freerdp_keyboard_get_rdp_scancode_from_virtual_key_code(uint32 vkcode)
{
	return VIRTUAL_KEY_CODE_TO_DEFAULT_RDP_SCANCODE_TABLE[vkcode];
}
