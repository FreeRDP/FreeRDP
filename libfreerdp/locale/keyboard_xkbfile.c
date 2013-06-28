/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * XKB Keyboard Mapping
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

#include "keyboard_xkbfile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/input.h>

#include <freerdp/locale/keyboard.h>

#include "keyboard_x11.h"
#include "xkb_layout_ids.h"
#include "liblocale.h"

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBfile.h>
#include <X11/extensions/XKBrules.h>

struct _XKB_KEY_NAME_SCANCODE
{
	const char* xkb_keyname; /* XKB keyname */
	DWORD rdp_scancode;
};
typedef struct _XKB_KEY_NAME_SCANCODE XKB_KEY_NAME_SCANCODE;

XKB_KEY_NAME_SCANCODE XKB_KEY_NAME_SCANCODE_TABLE[] =
{
	{ "BKSP",	RDP_SCANCODE_BACKSPACE},
	{ "TAB",	RDP_SCANCODE_TAB},
	{ "RTRN",	RDP_SCANCODE_RETURN}, // not KP
	{ "LFSH",	RDP_SCANCODE_LSHIFT},
	{ "LALT",	RDP_SCANCODE_LMENU},
	{ "CAPS",	RDP_SCANCODE_CAPSLOCK},
	{ "ESC",	RDP_SCANCODE_ESCAPE},
	{ "SPCE",	RDP_SCANCODE_SPACE},
	{ "AE10",	RDP_SCANCODE_KEY_0},
	{ "AE01",	RDP_SCANCODE_KEY_1},
	{ "AE02",	RDP_SCANCODE_KEY_2},
	{ "AE03",	RDP_SCANCODE_KEY_3},
	{ "AE04",	RDP_SCANCODE_KEY_4},
	{ "AE05",	RDP_SCANCODE_KEY_5},
	{ "AE06",	RDP_SCANCODE_KEY_6},
	{ "AE07",	RDP_SCANCODE_KEY_7},
	{ "AE08",	RDP_SCANCODE_KEY_8},
	{ "AE09",	RDP_SCANCODE_KEY_9},
	{ "AC01",	RDP_SCANCODE_KEY_A},
	{ "AB05",	RDP_SCANCODE_KEY_B},
	{ "AB03",	RDP_SCANCODE_KEY_C},
	{ "AC03",	RDP_SCANCODE_KEY_D},
	{ "AD03",	RDP_SCANCODE_KEY_E},
	{ "AC04",	RDP_SCANCODE_KEY_F},
	{ "AC05",	RDP_SCANCODE_KEY_G},
	{ "AC06",	RDP_SCANCODE_KEY_H},
	{ "AD08",	RDP_SCANCODE_KEY_I},
	{ "AC07",	RDP_SCANCODE_KEY_J},
	{ "AC08",	RDP_SCANCODE_KEY_K},
	{ "AC09",	RDP_SCANCODE_KEY_L},
	{ "AB07",	RDP_SCANCODE_KEY_M},
	{ "AB06",	RDP_SCANCODE_KEY_N},
	{ "AD09",	RDP_SCANCODE_KEY_O},
	{ "AD10",	RDP_SCANCODE_KEY_P},
	{ "AD01",	RDP_SCANCODE_KEY_Q},
	{ "AD04",	RDP_SCANCODE_KEY_R},
	{ "AC02",	RDP_SCANCODE_KEY_S},
	{ "AD05",	RDP_SCANCODE_KEY_T},
	{ "AD07",	RDP_SCANCODE_KEY_U},
	{ "AB04",	RDP_SCANCODE_KEY_V},
	{ "AD02",	RDP_SCANCODE_KEY_W},
	{ "AB02",	RDP_SCANCODE_KEY_X},
	{ "AD06",	RDP_SCANCODE_KEY_Y},
	{ "AB01",	RDP_SCANCODE_KEY_Z},
	{ "KP0",	RDP_SCANCODE_NUMPAD0},
	{ "KP1",	RDP_SCANCODE_NUMPAD1},
	{ "KP2",	RDP_SCANCODE_NUMPAD2},
	{ "KP3",	RDP_SCANCODE_NUMPAD3},
	{ "KP4",	RDP_SCANCODE_NUMPAD4},
	{ "KP5",	RDP_SCANCODE_NUMPAD5},
	{ "KP6",	RDP_SCANCODE_NUMPAD6},
	{ "KP7",	RDP_SCANCODE_NUMPAD7},
	{ "KP8",	RDP_SCANCODE_NUMPAD8},
	{ "KP9",	RDP_SCANCODE_NUMPAD9},
	{ "KPMU",	RDP_SCANCODE_MULTIPLY},
	{ "KPAD",	RDP_SCANCODE_ADD},
	{ "KPSU",	RDP_SCANCODE_SUBTRACT},
	{ "KPDL",	RDP_SCANCODE_DECIMAL},
	{ "AB10",	RDP_SCANCODE_OEM_2}, // not KP, not RDP_SCANCODE_DIVIDE
	{ "FK01",	RDP_SCANCODE_F1},
	{ "FK02",	RDP_SCANCODE_F2},
	{ "FK03",	RDP_SCANCODE_F3},
	{ "FK04",	RDP_SCANCODE_F4},
	{ "FK05",	RDP_SCANCODE_F5},
	{ "FK06",	RDP_SCANCODE_F6},
	{ "FK07",	RDP_SCANCODE_F7},
	{ "FK08",	RDP_SCANCODE_F8},
	{ "FK09",	RDP_SCANCODE_F9},
	{ "FK10",	RDP_SCANCODE_F10},
	{ "FK11",	RDP_SCANCODE_F11},
	{ "FK12",	RDP_SCANCODE_F12},
	{ "NMLK",	RDP_SCANCODE_NUMLOCK},
	{ "SCLK",	RDP_SCANCODE_SCROLLLOCK},
	{ "RTSH",	RDP_SCANCODE_RSHIFT},
	{ "LCTL",	RDP_SCANCODE_LCONTROL},
	{ "AC10",	RDP_SCANCODE_OEM_1},
	{ "AE12",	RDP_SCANCODE_OEM_PLUS},
	{ "AB08",	RDP_SCANCODE_OEM_COMMA},
	{ "AE11",	RDP_SCANCODE_OEM_MINUS},
	{ "AB09",	RDP_SCANCODE_OEM_PERIOD},
	{ "TLDE",	RDP_SCANCODE_OEM_3},
	{ "AB11",	RDP_SCANCODE_ABNT_C1},
	{ "I129",	RDP_SCANCODE_ABNT_C2},
	{ "AD11",	RDP_SCANCODE_OEM_4},
	{ "BKSL",	RDP_SCANCODE_OEM_5},
	{ "AD12",	RDP_SCANCODE_OEM_6},
	{ "AC11",	RDP_SCANCODE_OEM_7},
	{ "LSGT",	RDP_SCANCODE_OEM_102},
	{ "KPEN",	RDP_SCANCODE_RETURN_KP}, // KP!
	{ "PAUS",	RDP_SCANCODE_PAUSE},
	{ "PGUP",	RDP_SCANCODE_PRIOR},
	{ "PGDN",	RDP_SCANCODE_NEXT},
	{ "END",	RDP_SCANCODE_END},
	{ "HOME",	RDP_SCANCODE_HOME},
	{ "LEFT",	RDP_SCANCODE_LEFT},
	{ "UP",		RDP_SCANCODE_UP},
	{ "RGHT",	RDP_SCANCODE_RIGHT},
	{ "DOWN",	RDP_SCANCODE_DOWN},
	{ "PRSC",	RDP_SCANCODE_PRINTSCREEN},
	{ "INS",	RDP_SCANCODE_INSERT},
	{ "DELE",	RDP_SCANCODE_DELETE},
	{ "LWIN",	RDP_SCANCODE_LWIN},
	{ "RWIN",	RDP_SCANCODE_RWIN},
	{ "COMP",	RDP_SCANCODE_APPS},
	{ "MENU",	RDP_SCANCODE_APPS}, // WinMenu in xfree86 layout
	{ "KPDV",	RDP_SCANCODE_DIVIDE}, // KP!
	{ "RCTL",	RDP_SCANCODE_RCONTROL},
	{ "RALT",	RDP_SCANCODE_RMENU},
	{ "AE13",	RDP_SCANCODE_BACKSLASH_JP}, // JP
	{ "HKTG",       RDP_SCANCODE_HIRAGANA}, // JP
	{ "HENK",       RDP_SCANCODE_CONVERT_JP}, // JP
	{ "MUHE",       RDP_SCANCODE_NONCONVERT_JP} // JP
/*	{ "LVL3",	0x54} */
};

void* freerdp_keyboard_xkb_init()
{
	int status;

	Display* display = XOpenDisplay(NULL);

	if (!display)
		return NULL;

	status = XkbQueryExtension(display, NULL, NULL, NULL, NULL, NULL);

	if (!status)
		return NULL;

	return (void*) display;
}

int freerdp_keyboard_init_xkbfile(DWORD* keyboardLayoutId, DWORD x11_keycode_to_rdp_scancode[256])
{
	void* display;

	ZeroMemory(x11_keycode_to_rdp_scancode, sizeof(DWORD) * 256);

	display = freerdp_keyboard_xkb_init();

	if (!display)
	{
		DEBUG_KBD("Error initializing xkb");
		return -1;
	}

	if (*keyboardLayoutId == 0)
	{
		detect_keyboard_layout_from_xkbfile(display, keyboardLayoutId);
		DEBUG_KBD("detect_keyboard_layout_from_xkb: %X", keyboardLayoutId);
	}

	freerdp_keyboard_load_map_from_xkbfile(display, x11_keycode_to_rdp_scancode);

	XCloseDisplay(display);

	return 0;
}

/* return substring starting after nth comma, ending at following comma */
static char* comma_substring(char* s, int n)
{
	char* p;

	if (!s)
		return "";

	while (n-- > 0)
	{
		if (!(p = strchr(s, ',')))
			break;

		s = p + 1;
	}

	if ((p = strchr(s, ',')))
		*p = 0;

	return s;
}

int detect_keyboard_layout_from_xkbfile(void* display, DWORD* keyboardLayoutId)
{
	char* layout;
	char* variant;
	DWORD group = 0;
	XkbStateRec state;
	XKeyboardState coreKbdState;
	XkbRF_VarDefsRec rules_names;

	DEBUG_KBD("display: %p", display);

	if (display && XkbRF_GetNamesProp(display, NULL, &rules_names))
	{
		DEBUG_KBD("layouts: %s", rules_names.layout ? rules_names.layout : "");
		DEBUG_KBD("variants: %s", rules_names.variant ? rules_names.variant : "");

		XGetKeyboardControl(display, &coreKbdState);

		if (XkbGetState(display, XkbUseCoreKbd, &state) == Success)
			group = state.group;

		DEBUG_KBD("group: %d", state.group);

		layout = comma_substring(rules_names.layout, group);
		variant = comma_substring(rules_names.variant, group);

		DEBUG_KBD("layout: %s", layout ? layout : "");
		DEBUG_KBD("variant: %s", variant ? variant : "");

		*keyboardLayoutId = find_keyboard_layout_in_xorg_rules(layout, variant);

		free(rules_names.model);
		free(rules_names.layout);
		free(rules_names.variant);
		free(rules_names.options);
	}

	return 0;
}

int freerdp_keyboard_load_map_from_xkbfile(void* display, DWORD x11_keycode_to_rdp_scancode[256])
{
	int i, j;
	BOOL found;
	XkbDescPtr xkb;
	BOOL status = FALSE;

	if (display && (xkb = XkbGetMap(display, 0, XkbUseCoreKbd)))
	{
		if (XkbGetNames(display, XkbKeyNamesMask, xkb) == Success)
		{
			char xkb_keyname[5] = { 42, 42, 42, 42, 0 }; /* end-of-string at index 5 */

			for (i = xkb->min_key_code; i <= xkb->max_key_code; i++)
			{
				found = FALSE;
				CopyMemory(xkb_keyname, xkb->names->keys[i].name, 4);

				if (strlen(xkb_keyname) < 1)
					continue;

				for (j = 0; j < ARRAYSIZE(XKB_KEY_NAME_SCANCODE_TABLE); j++)
				{
					if (!strcmp(xkb_keyname, XKB_KEY_NAME_SCANCODE_TABLE[j].xkb_keyname))
					{
						DEBUG_KBD("%4s: keycode: 0x%02X -> rdp scancode: 0x%04X",
								xkb_keyname, i, XKB_KEY_NAME_SCANCODE_TABLE[j].rdp_scancode);

						if (found)
						{
							DEBUG_KBD("Internal error! duplicate key %s!", xkb_keyname);
						}

						x11_keycode_to_rdp_scancode[i] = XKB_KEY_NAME_SCANCODE_TABLE[j].rdp_scancode;
						found = TRUE;
					}
				}

				if (!found)
				{
					DEBUG_KBD("%4s: keycode: 0x%02X -> no RDP scancode found", xkb_keyname, i);
				}
			}

			status = TRUE;
		}

		XkbFreeKeyboard(xkb, 0, 1);
	}

	return status;
}
