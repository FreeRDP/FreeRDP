/**
 * FreeRDP: A Remote Desktop Protocol Client
 * XKB-based Keyboard Mapping to Microsoft Keyboard System
 *
 * Copyright 2009 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <stdlib.h>
#include <string.h>
#include <freerdp/types.h>
#include <freerdp/kbd/kbd.h>

#include "libkbd.h"

#include <freerdp/kbd/locales.h>
#include <freerdp/kbd/vkcodes.h>
#include <freerdp/kbd/layouts.h>
#include "layouts_xkb.h"

/*
 * The actual mapping from X keycodes to RDP keycodes, initialized from xkb keycodes or similar.
 * Used directly by freerdp_kbd_get_scancode_by_keycode. The mapping is a global variable,
 * but it only depends on which keycodes the X servers keyboard driver uses and is thus very static.
 */

RdpScancodes x_keycode_to_rdp_scancode;

uint8 rdp_scancode_to_x_keycode[256][2];

#ifndef WITH_XKBFILE

static unsigned int detect_keyboard(void* dpy, unsigned int keyboardLayoutID, char* xkbfile, size_t xkbfilelength)
{
	xkbfile[0] = '\0';

	if (keyboardLayoutID != 0)
		DEBUG_KBD("keyboard layout configuration: %X", keyboardLayoutID);

#if defined(sun)
	if (keyboardLayoutID == 0)
	{
		keyboardLayoutID = detect_keyboard_type_and_layout_sunos(xkbfile, xkbfilelength);
		DEBUG_KBD("detect_keyboard_type_and_layout_sunos: %X %s", keyboardLayoutID, xkbfile);
	}
#endif

	if (keyboardLayoutID == 0)
	{
		keyboardLayoutID = detect_keyboard_layout_from_locale();
		DEBUG_KBD("detect_keyboard_layout_from_locale: %X", keyboardLayoutID);
	}

	if (keyboardLayoutID == 0)
	{
		keyboardLayoutID = 0x0409;
		DEBUG_KBD("using default keyboard layout: %X", keyboardLayoutID);
	}

	if (xkbfile[0] == '\0')
	{
		strncpy(xkbfile, "base", xkbfilelength);
		DEBUG_KBD("using default keyboard layout: %s", xkbfile);
	}

	return keyboardLayoutID;
}

#endif

/*
 * Initialize global keyboard mapping and return the suggested server side layout.
 * dpy must be a X Display* or NULL.
 */

unsigned int freerdp_kbd_init(void* dpy, unsigned int keyboard_layout_id)
{
	memset(x_keycode_to_rdp_scancode, 0, sizeof(x_keycode_to_rdp_scancode));
	memset(rdp_scancode_to_x_keycode, '\0', sizeof(rdp_scancode_to_x_keycode));

#ifdef WITH_XKBFILE
	if (!init_xkb(dpy))
	{
		DEBUG_KBD("Error initializing xkb");
		return 0;
	}
	if (keyboard_layout_id == 0)
	{
		keyboard_layout_id = detect_keyboard_layout_from_xkb(dpy);
		DEBUG_KBD("detect_keyboard_layout_from_xkb: %X", keyboard_layout_id);
	}
	init_keycodes_from_xkb(dpy, x_keycode_to_rdp_scancode, rdp_scancode_to_x_keycode);
#else
	int vkcode;
	int keycode;
	char xkbfile[256];
	KeycodeToVkcode keycodeToVkcode;

	if (keyboard_layout_id == 0)
		keyboard_layout_id = detect_keyboard(dpy, keyboard_layout_id, xkbfile, sizeof(xkbfile));

	DEBUG_KBD("Using keyboard layout 0x%X with xkb name %s and xkbfile %s",
			keyboard_layout_id, get_layout_name(keyboard_layout_id), xkbfile);

	load_keyboard_map(keycodeToVkcode, xkbfile);

	for (keycode = 0; keycode < 256; keycode++)
	{
		vkcode = keycodeToVkcode[keycode];

		DEBUG_KBD("X keycode %3d VK %3d %-19s-> RDP scancode %d/%d",
				keycode, vkcode, virtualKeyboard[vkcode].name,
				virtualKeyboard[vkcode].extended, virtualKeyboard[vkcode].scancode);

		x_keycode_to_rdp_scancode[keycode].keycode = virtualKeyboard[vkcode].scancode;
		x_keycode_to_rdp_scancode[keycode].extended = virtualKeyboard[vkcode].extended;
		x_keycode_to_rdp_scancode[keycode].keyname = virtualKeyboard[vkcode].name;

		if (x_keycode_to_rdp_scancode[keycode].extended)
			rdp_scancode_to_x_keycode[virtualKeyboard[vkcode].scancode][1] = keycode;
		else
			rdp_scancode_to_x_keycode[virtualKeyboard[vkcode].scancode][0] = keycode;
	}
#endif

	return keyboard_layout_id;
}

rdpKeyboardLayout* freerdp_kbd_get_layouts(int types)
{
	return get_keyboard_layouts(types);
}

uint8 freerdp_kbd_get_scancode_by_keycode(uint8 keycode, boolean* extended)
{
	DEBUG_KBD("%2x %4s -> %d/%d", keycode, x_keycode_to_rdp_scancode[keycode].keyname,
			x_keycode_to_rdp_scancode[keycode].extended, x_keycode_to_rdp_scancode[keycode].keycode);

	*extended = x_keycode_to_rdp_scancode[keycode].extended;

	return x_keycode_to_rdp_scancode[keycode].keycode;
}

uint8 freerdp_kbd_get_keycode_by_scancode(uint8 scancode, boolean extended)
{
	if (extended)
		return rdp_scancode_to_x_keycode[scancode][1];
	else
		return rdp_scancode_to_x_keycode[scancode][0];
}

uint8 freerdp_kbd_get_scancode_by_virtualkey(int vkcode, boolean* extended)
{
	*extended = virtualKeyboard[vkcode].extended;
	return virtualKeyboard[vkcode].scancode;
}
