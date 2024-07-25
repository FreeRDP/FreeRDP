/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Keyboard Mapping
 *
 * Copyright 2009-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2023 Bernhard Miklautz <bernhard.miklautz@thincast.com>
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

#include <string.h>

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include "liblocale.h"
#include "keyboard_x11.h"
#include "xkb_layout_ids.h"

static BOOL parse_xkb_rule_names(char* xkb_rule, unsigned long num_bytes, char** layout,
                                 char** variant)
{
	/* Sample output for "Canadian Multilingual Standard"
	 *
	 * _XKB_RULES_NAMES_BACKUP(STRING) = "xorg", "pc105", "ca", "multi", "magic"
	 *
	 *  Format: "rules", "model", "layout", "variant", "options"
	 *
	 * Where "xorg" is the set of rules
	 * "pc105" the keyboard model
	 * "ca" the keyboard layout(s) (can also be something like 'us,uk')
	 * "multi" the keyboard layout variant(s)  (in the examples, “,winkeys” - which means first
	 *         layout uses some “default” variant and second uses “winkeys” variant)
	 * "magic" - configuration option (in the examples,
	 * “eurosign:e,lv3:ralt_switch,grp:rctrl_toggle”
	 *         - three options)
	 */
	for (size_t i = 0, index = 0; i < num_bytes; i++, index++)
	{
		char* ptr = xkb_rule + i;

		switch (index)
		{
			case 0: // rules
				break;
			case 1: // model
				break;
			case 2: // layout
			{
				/* If multiple languages are present we just take the first one */
				char* delimiter = strchr(ptr, ',');
				if (delimiter)
					*delimiter = '\0';
				*layout = ptr;
				break;
			}
			case 3: // variant
				*variant = ptr;
				break;
			case 4: // option
				break;
			default:
				break;
		}
		i += strlen(ptr);
	}
	return TRUE;
}

static DWORD kbd_layout_id_from_x_property(Display* display, Window root, char* property_name)
{
	char* layout = NULL;
	char* variant = NULL;
	char* rule = NULL;
	Atom type = None;
	int item_size = 0;
	unsigned long items = 0;
	unsigned long unread_items = 0;
	DWORD layout_id = 0;

	Atom property = XInternAtom(display, property_name, False);
	if (property == None)
		return 0;

	if (XGetWindowProperty(display, root, property, 0, 1024, False, XA_STRING, &type, &item_size,
	                       &items, &unread_items, (unsigned char**)&rule) != Success)
		return 0;

	if (type != XA_STRING || item_size != 8 || unread_items != 0)
	{
		XFree(rule);
		return 0;
	}

	parse_xkb_rule_names(rule, items, &layout, &variant);

	DEBUG_KBD("%s layout: %s, variant: %s", property_name, layout, variant);
	layout_id = find_keyboard_layout_in_xorg_rules(layout, variant);

	XFree(rule);

	return layout_id;
}

int freerdp_detect_keyboard_layout_from_xkb(DWORD* keyboardLayoutId)
{
	Display* display = XOpenDisplay(NULL);

	if (!display)
		return 0;

	Window root = DefaultRootWindow(display);
	if (!root)
		return 0;

	/* We start by looking for _XKB_RULES_NAMES_BACKUP which appears to be used by libxklavier */
	DWORD id = kbd_layout_id_from_x_property(display, root, "_XKB_RULES_NAMES_BACKUP");

	if (0 == id)
		id = kbd_layout_id_from_x_property(display, root, "_XKB_RULES_NAMES");

	if (0 != id)
		*keyboardLayoutId = id;

	XCloseDisplay(display);
	return (int)id;
}
