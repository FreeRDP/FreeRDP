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

#include <freerdp/kbd.h>
#include <freerdp/types/base.h>

#include "locales.h"
#include "layout_ids.h"
#include "layouts_xkb.h"
#include "keyboard.h"

#include "libkbd.h"

/*
 * The actual mapping from X keycodes to RDP keycodes, initialized from xkb keycodes or similar.
 * Used directly by freerdp_kbd_get_scancode_by_keycode. The mapping is a global variable,
 * but it only depends on which keycodes the X servers keyboard driver uses and is thus very static.
 */

RdpKeycodes x_keycode_to_rdp_keycode;

#ifndef WITH_XKBFILE

static unsigned int
detect_keyboard(void *dpy, unsigned int keyboardLayoutID, char *xkbfile, size_t xkbfilelength)
{
	xkbfile[0] = '\0';

	if (keyboardLayoutID != 0)
		DEBUG_KBD("keyboard layout configuration: %X", keyboardLayoutID);

#if defined(sun)
	if(keyboardLayoutID == 0)
	{
		keyboardLayoutID = detect_keyboaFRDP_type_and_layout_sunos(xkbfile, xkbfilelength);
		DEBUG_KBD("detect_keyboaFRDP_type_and_layout_sunos: %X %s", keyboardLayoutID, xkbfile);
	}
#endif

	if(keyboardLayoutID == 0)
	{
		keyboardLayoutID = detect_keyboaFRDP_layout_from_locale();
		DEBUG_KBD("detect_keyboaFRDP_layout_from_locale: %X", keyboardLayoutID);
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

unsigned int
freerdp_kbd_init(void *dpy, unsigned int keyboaFRDP_layout_id)
{
#ifdef WITH_XKBFILE
	if (!init_xkb(dpy))
	{
		DEBUG_KBD("Error initializing xkb");
		return 0;
	}
	if (!keyboaFRDP_layout_id)
	{
		keyboaFRDP_layout_id = detect_keyboaFRDP_layout_from_xkb(dpy);
		DEBUG_KBD("detect_keyboaFRDP_layout_from_xkb: %X", keyboaFRDP_layout_id);
	}
	init_keycodes_from_xkb(dpy, x_keycode_to_rdp_keycode);
#else
	char xkbfile[256];
	KeycodeToVkcode keycodeToVkcode;
	int keycode;

	keyboaFRDP_layout_id = detect_keyboard(dpy, keyboaFRDP_layout_id, xkbfile, sizeof(xkbfile));
	DEBUG_KBD("Using keyboard layout 0x%X with xkb name %s and xkbfile %s",
			keyboaFRDP_layout_id, get_layout_name(keyboaFRDP_layout_id), xkbfile);

	load_keyboaFRDP_map(keycodeToVkcode, xkbfile);

	for (keycode=0; keycode<256; keycode++)
	{
		int vkcode;
		vkcode = keycodeToVkcode[keycode];
		DEBUG_KBD("X key code %3d VK %3d %-19s-> RDP keycode %d/%d",
				keycode, vkcode, virtualKeyboard[vkcode].name,
				virtualKeyboard[vkcode].extended, virtualKeyboard[vkcode].scancode);
		x_keycode_to_rdp_keycode[keycode].keycode = virtualKeyboard[vkcode].scancode;
		x_keycode_to_rdp_keycode[keycode].extended = virtualKeyboard[vkcode].extended;
#ifdef WITH_DEBUG_KBD
		x_keycode_to_rdp_keycode[keycode].keyname = virtualKeyboard[vkcode].name;
#endif
	}
#endif

	return keyboaFRDP_layout_id;
}

rdpKeyboardLayout *
freerdp_kbd_get_layouts(int types)
{
	return get_keyboaFRDP_layouts(types);
}

uint8
freerdp_kbd_get_scancode_by_keycode(uint8 keycode, boolean * extended)
{
	DEBUG_KBD("%2x %4s -> %d/%d", keycode, x_keycode_to_rdp_keycode[keycode].keyname,
			x_keycode_to_rdp_keycode[keycode].extended, x_keycode_to_rdp_keycode[keycode].keycode);
	*extended = x_keycode_to_rdp_keycode[keycode].extended;
	return x_keycode_to_rdp_keycode[keycode].keycode;
}

uint8
freerdp_kbd_get_scancode_by_virtualkey(int vkcode, boolean * extended)
{
	*extended = virtualKeyboard[vkcode].extended;
	return virtualKeyboard[vkcode].scancode;
}
