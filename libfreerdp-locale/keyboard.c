/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Localization Services
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
#include <freerdp/utils/file.h>
#include <freerdp/utils/memory.h>
#include <freerdp/locale/keyboard.h>

#include "liblocale.h"

#ifdef WITH_XKB
#include "keyboard_xkb.h"
#endif

#include <freerdp/locale/locales.h>
#include <freerdp/locale/vkcodes.h>
#include <freerdp/locale/layouts.h>

#include "keyboard.h"

extern const virtualKey virtualKeyboard[258];

/*
 * The actual mapping from X keycodes to RDP keycodes, initialized from xkb keycodes or similar.
 * Used directly by freerdp_kbd_get_scancode_by_keycode. The mapping is a global variable,
 * but it only depends on which keycodes the X servers keyboard driver uses and is thus very static.
 */

RdpScancodes x_keycode_to_rdp_scancode;

uint8 rdp_scancode_to_x_keycode[256][2];

uint32 freerdp_detect_keyboard(void* display, uint32 keyboardLayoutID, char* xkbfile, size_t xkbfile_length)
{
	xkbfile[0] = '\0';

	if (keyboardLayoutID != 0)
		DEBUG_KBD("keyboard layout configuration: %X", keyboardLayoutID);

#if defined(sun)
	if (keyboardLayoutID == 0)
	{
		keyboardLayoutID = detect_keyboard_type_and_layout_sunos(xkbfile, xkbfile_length);
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
		strncpy(xkbfile, "base", xkbfile_length);
		DEBUG_KBD("using default keyboard layout: %s", xkbfile);
	}

	return keyboardLayoutID;
}

/*
 * Initialize global keyboard mapping and return the suggested server side layout.
 * display must be a X Display* or NULL.
 */

uint32 freerdp_keyboard_init(uint32 keyboard_layout_id)
{
	void* display;
	memset(x_keycode_to_rdp_scancode, 0, sizeof(x_keycode_to_rdp_scancode));
	memset(rdp_scancode_to_x_keycode, '\0', sizeof(rdp_scancode_to_x_keycode));

#ifdef WITH_XKB
	display = freerdp_keyboard_xkb_init();

	if (!display)
	{
		DEBUG_KBD("Error initializing xkb");
		return 0;
	}

	if (keyboard_layout_id == 0)
	{
		keyboard_layout_id = detect_keyboard_layout_from_xkb(display);
		DEBUG_KBD("detect_keyboard_layout_from_xkb: %X", keyboard_layout_id);
	}

	freerdp_keyboard_load_map_from_xkb(display, x_keycode_to_rdp_scancode, rdp_scancode_to_x_keycode);
#else
	int vkcode;
	int keycode;
	char xkbfile[256];
	KeycodeToVkcode keycodeToVkcode;

	if (keyboard_layout_id == 0)
		keyboard_layout_id = freerdp_detect_keyboard(display, keyboard_layout_id, xkbfile, sizeof(xkbfile));

	DEBUG_KBD("Using keyboard layout 0x%X with xkb name %s and xkbfile %s",
			keyboard_layout_id, get_layout_name(keyboard_layout_id), xkbfile);

	freerdp_keyboard_load_maps(keycodeToVkcode, xkbfile);

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

/* Default built-in keymap */
static const KeycodeToVkcode defaultKeycodeToVkcode =
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1B, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
	0x37, 0x38, 0x39, 0x30, 0xBD, 0xBB, 0x08, 0x09, 0x51, 0x57, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49,
	0x4F, 0x50, 0xDB, 0xDD, 0x0D, 0xA2, 0x41, 0x53, 0x44, 0x46, 0x47, 0x48, 0x4A, 0x4B, 0x4C, 0xBA,
	0xDE, 0xC0, 0xA0, 0x00, 0x5A, 0x58, 0x43, 0x56, 0x42, 0x4E, 0x4D, 0xBC, 0xBE, 0xBF, 0xA1, 0x6A,
	0x12, 0x20, 0x14, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x90, 0x91, 0x67,
	0x68, 0x69, 0x6D, 0x64, 0x65, 0x66, 0x6B, 0x61, 0x62, 0x63, 0x60, 0x6E, 0x00, 0x00, 0x00, 0x7A,
	0x7B, 0x24, 0x26, 0x21, 0x25, 0x00, 0x27, 0x23, 0x28, 0x22, 0x2D, 0x2E, 0x0D, 0xA3, 0x13, 0x2C,
	0x6F, 0x12, 0x00, 0x5B, 0x5C, 0x5D, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static int freerdp_keyboard_load_map(KeycodeToVkcode map, char* kbd)
{
	char* pch;
	char* beg;
	char* end;
	char* xkbfile_path;
	char xkbmap[256] = "";
	char xkbinc[256] = "";
	char buffer[1024] = "";
	char xkbfile[256] = "";

	FILE* fp;
	int kbdFound = 0;

	int i = 0;
	int keycode = 0;
	char keycodeString[32] = "";
	char vkcodeName[128] = "";

	beg = kbd;

	/* Extract file name and keymap name */
	if ((end = strrchr(kbd, '(')) != NULL)
	{
		strncpy(xkbfile, &kbd[beg - kbd], end - beg);

		beg = end + 1;
		if ((end = strrchr(kbd, ')')) != NULL)
		{
			strncpy(xkbmap, &kbd[beg - kbd], end - beg);
			xkbmap[end - beg] = '\0';
		}
	}
	else
	{
		/* The keyboard name is the same as the file name */
		strcpy(xkbfile, kbd);
		strcpy(xkbmap, kbd);
	}

	/* Get path to file relative to freerdp's directory */
	xkbfile_path = freerdp_construct_path(FREERDP_KEYMAP_PATH, xkbfile);

	DEBUG_KBD("Loading keymap %s, first trying %s", kbd, xkbfile_path);

	if ((fp = fopen(xkbfile_path, "r")) == NULL)
	{
		return 0;
	}

	while (fgets(buffer, sizeof(buffer), fp) != NULL)
	{
		if (buffer[0] == '#')
		{
			continue; /* Skip comments */
		}

		if (kbdFound)
		{
			/* Closing curly bracket and semicolon */
			if ((pch = strstr(buffer, "};")) != NULL)
			{
				break;
			}
			else if ((pch = strstr(buffer, "VK_")) != NULL)
			{
				/* The end is delimited by the first white space */
				end = strcspn(pch, " \t\n\0") + pch;

				/* We copy the virtual key code name in a string */
				beg = pch;
				strncpy(vkcodeName, beg, end - beg);
				vkcodeName[end - beg] = '\0';

				/* Now we want to extract the virtual key code itself which is in between '<' and '>' */
				if ((beg = strchr(pch + 3, '<')) == NULL)
					break;
				else
					beg++;

				if ((end = strchr(beg, '>')) == NULL)
					break;

				/* We copy the string representing the number in a string */
				strncpy(keycodeString, beg, end - beg);
				keycodeString[end - beg] = '\0';

				/* Convert the string representing the code to an integer */
				keycode = atoi(keycodeString);

				/* Make sure it is a valid keycode */
				if (keycode < 0 || keycode > 255)
					break;

				/* Load this key mapping in the keyboard mapping */
				for(i = 0; i < sizeof(virtualKeyboard) / sizeof(virtualKey); i++)
				{
					if (strcmp(vkcodeName, virtualKeyboard[i].name) == 0)
					{
						map[keycode] = i;
					}
				}
			}
			else if ((pch = strstr(buffer, ": extends")) != NULL)
			{
				/*
				 * This map extends another keymap We extract its name
				 * and we recursively load the keymap we need to include.
				 */

				if ((beg = strchr(pch + sizeof(": extends"), '"')) == NULL)
					break;
				beg++;

				if ((end = strchr(beg, '"')) == NULL)
					break;

				strncpy(xkbinc, beg, end - beg);
				xkbinc[end - beg] = '\0';

				freerdp_keyboard_load_map(map, xkbinc); /* Load included keymap */
			}
		}
		else if ((pch = strstr(buffer, "keyboard")) != NULL)
		{
			/* Keyboard map identifier */
			if ((beg = strchr(pch + sizeof("keyboard"), '"')) == NULL)
				break;
			beg++;

			if ((end = strchr(beg, '"')) == NULL)
				break;

			pch = beg;
			buffer[end - beg] = '\0';

			/* Does it match our keymap name? */
			if (strncmp(xkbmap, pch, strlen(xkbmap)) == 0)
				kbdFound = 1;
		}
	}

	fclose(fp); /* Don't forget to close file */

	return 1;
}

void freerdp_keyboard_load_maps(KeycodeToVkcode keycodeToVkcode, char* xkbfile)
{
	char* kbd;
	char* xkbfileEnd;
	int keymapLoaded = 0;

	memset(keycodeToVkcode, 0, sizeof(keycodeToVkcode));

	kbd = xkbfile;
	xkbfileEnd = xkbfile + strlen(xkbfile);

#ifdef __APPLE__
	/* Apple X11 breaks XKB detection */
	keymapLoaded += freerdp_keyboard_load_map(keycodeToVkcode, "macosx(macosx)");
#else
	do
	{
		/* Multiple maps are separated by '+' */
		int kbdlen = strcspn(kbd + 1, "+") + 1;
		kbd[kbdlen] = '\0';

		/* Load keyboard map */
		keymapLoaded += freerdp_keyboard_load_map(keycodeToVkcode, kbd);

		kbd += kbdlen + 1;
	}
	while (kbd < xkbfileEnd);
#endif

	DEBUG_KBD("loaded %d keymaps", keymapLoaded);
	if (keymapLoaded <= 0)
	{
		/* No keymap was loaded, load default hard-coded keymap */
		DEBUG_KBD("using default keymap");
		memcpy(keycodeToVkcode, defaultKeycodeToVkcode, sizeof(keycodeToVkcode));
	}
}

rdpKeyboardLayout* freerdp_keyboard_get_layouts(uint32 types)
{
	return get_keyboard_layouts(types);
}

uint32 freerdp_keyboard_get_scancode_from_keycode(uint32 keycode, boolean* extended)
{
	DEBUG_KBD("%2x %4s -> %d/%d", keycode, x_keycode_to_rdp_scancode[keycode].keyname,
			x_keycode_to_rdp_scancode[keycode].extended, x_keycode_to_rdp_scancode[keycode].keycode);

	*extended = x_keycode_to_rdp_scancode[keycode].extended;

	return x_keycode_to_rdp_scancode[keycode].keycode;
}

uint32 freerdp_keyboard_get_keycode_from_scancode(uint32 scancode, boolean extended)
{
	if (extended)
		return rdp_scancode_to_x_keycode[scancode][1];
	else
		return rdp_scancode_to_x_keycode[scancode][0];
}

uint32 freerdp_keyboard_get_scancode_from_vkcode(uint32 vkcode, boolean* extended)
{
	*extended = virtualKeyboard[vkcode].extended;
	return virtualKeyboard[vkcode].scancode;
}
