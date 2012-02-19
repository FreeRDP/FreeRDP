/**
 * FreeRDP: A Remote Desktop Protocol Client
 * XKB-based Keyboard Mapping to Microsoft Keyboard System
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

#include "liblocale.h"

#include "keyboard_xkb.h"
#include "keyboard_x11.h"
#include <freerdp/locale/vkcodes.h>

extern const virtualKey virtualKeyboard[258];

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBfile.h>
#include <X11/extensions/XKBrules.h>

#include <freerdp/utils/memory.h>

void* freerdp_keyboard_xkb_init()
{
	int status;

	Display* display = XOpenDisplay(NULL);

	if (display == NULL)
		return NULL;

	status = XkbQueryExtension(display, NULL, NULL, NULL, NULL, NULL);

	if (!status)
		return NULL;

	return (void*) display;
}

/* return substring starting after nth comma, ending at following comma */
static char* comma_substring(char* s, int n)
{
	char *p;

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

uint32 detect_keyboard_layout_from_xkb(void* display)
{
	char* layout;
	char* variant;
	uint32 group = 0;
	uint32 keyboard_layout = 0;
	XkbRF_VarDefsRec rules_names;
	XKeyboardState coreKbdState;
	XkbStateRec state;

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

		keyboard_layout = find_keyboard_layout_in_xorg_rules(layout, variant);

		xfree(rules_names.model);
		xfree(rules_names.layout);
		xfree(rules_names.variant);
		xfree(rules_names.options);
	}

	return keyboard_layout;
}

int freerdp_keyboard_load_map_from_xkb(void* display, RdpScancodes x_keycode_to_rdp_scancode, uint8 rdp_scancode_to_x_keycode[256][2])
{
	int status = 0;
	XkbDescPtr xkb;

	if (display && (xkb = XkbGetMap(display, 0, XkbUseCoreKbd)))
	{
		if (XkbGetNames(display, XkbKeyNamesMask, xkb) == Success)
		{
			int i, j;
			char buf[5] = {42, 42, 42, 42, 0}; /* end-of-string at pos 5 */

			for (i = xkb->min_key_code; i <= xkb->max_key_code; i++)
			{
				memcpy(buf, xkb->names->keys[i].name, 4);

				j = sizeof(virtualKeyboard) / sizeof(virtualKeyboard[0]) - 1;

				while (j >= 0)
				{
					if (virtualKeyboard[j].x_keyname && !strcmp(buf, virtualKeyboard[j].x_keyname))
						break;
					j--;
				}

				if (j >= 0)
				{
					DEBUG_KBD("X keycode %3d has keyname %-4s -> RDP scancode %d/%d",
							i, buf, virtualKeyboard[j].extended, virtualKeyboard[j].scancode);

					x_keycode_to_rdp_scancode[i].extended = virtualKeyboard[j].extended;
					x_keycode_to_rdp_scancode[i].keycode = virtualKeyboard[j].scancode;
					x_keycode_to_rdp_scancode[i].keyname = virtualKeyboard[j].x_keyname;

					if (x_keycode_to_rdp_scancode[i].extended)
						rdp_scancode_to_x_keycode[virtualKeyboard[j].scancode][1] = i;
					else
						rdp_scancode_to_x_keycode[virtualKeyboard[j].scancode][0] = i;
				}
				else
				{
					DEBUG_KBD("X key code %3d has keyname %-4s -> ??? - not found", i, buf);
				}
			}
			status = 1;
		}

		XkbFreeKeyboard(xkb, 0, 1);
	}

	return status;
}
