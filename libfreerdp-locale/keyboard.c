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

#ifdef WITH_X11
#include "keyboard_x11.h"
#endif

#ifdef WITH_XKB
#include "keyboard_xkb.h"
#endif

#ifdef WITH_SUN
#include "keyboard_sun.h"
#endif

#include <freerdp/locale/locale.h>
#include <freerdp/locale/keyboard.h>

#include "keyboard.h"

extern const VIRTUAL_KEY virtualKeyboard[258];

/*
 * The actual mapping from X keycodes to RDP keycodes, initialized from xkb keycodes or similar.
 * Used directly by freerdp_kbd_get_scancode_by_keycode. The mapping is a global variable,
 * but it only depends on which keycodes the X servers keyboard driver uses and is thus very static.
 */

RdpScancodes x_keycode_to_rdp_scancode;

uint8 rdp_scancode_to_x_keycode[256][2];

uint32 freerdp_detect_keyboard(uint32 keyboardLayoutID)
{
	if (keyboardLayoutID != 0)
		DEBUG_KBD("keyboard layout configuration: %X", keyboardLayoutID);

	if (keyboardLayoutID == 0)
	{
		keyboardLayoutID = freerdp_detect_keyboard_layout_from_locale();
		DEBUG_KBD("detect_keyboard_layout_from_locale: %X", keyboardLayoutID);
	}

	if (keyboardLayoutID == 0)
	{
		keyboardLayoutID = 0x0409;
		DEBUG_KBD("using default keyboard layout: %X", keyboardLayoutID);
	}

	return keyboardLayoutID;
}

uint32 freerdp_keyboard_init_xkb(uint32 keyboardLayoutId)
{
	void* display;
	memset(x_keycode_to_rdp_scancode, 0, sizeof(x_keycode_to_rdp_scancode));
	memset(rdp_scancode_to_x_keycode, '\0', sizeof(rdp_scancode_to_x_keycode));

	display = freerdp_keyboard_xkb_init();

	if (!display)
	{
		DEBUG_KBD("Error initializing xkb");
		return 0;
	}

	if (keyboardLayoutId == 0)
	{
		keyboardLayoutId = detect_keyboard_layout_from_xkb(display);
		DEBUG_KBD("detect_keyboard_layout_from_xkb: %X", keyboardLayoutId);
	}

	freerdp_keyboard_load_map_from_xkb(display, x_keycode_to_rdp_scancode, rdp_scancode_to_x_keycode);

	return keyboardLayoutId;
}

uint32 freerdp_keyboard_init_x11(uint32 keyboardLayoutId)
{
	uint32 vkcode;
	uint32 keycode;
	KeycodeToVkcode keycodeToVkcode;

	memset(x_keycode_to_rdp_scancode, 0, sizeof(x_keycode_to_rdp_scancode));
	memset(rdp_scancode_to_x_keycode, '\0', sizeof(rdp_scancode_to_x_keycode));

	if (keyboardLayoutId == 0)
	{
		keyboardLayoutId = freerdp_detect_keyboard_layout_from_locale();
		DEBUG_KBD("using keyboard layout: %X", keyboardLayoutID);
	}

	if (keyboardLayoutId == 0)
	{
		keyboardLayoutId = 0x0409;
		DEBUG_KBD("using default keyboard layout: %X", keyboardLayoutID);
	}

#ifdef __APPLE__
	/* Apple X11 breaks XKB detection */
	freerdp_keyboard_load_map(keycodeToVkcode, "macosx(macosx)");
#endif

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

	return keyboardLayoutId;
}

uint32 freerdp_keyboard_init(uint32 keyboardLayoutId)
{
#ifdef WITH_XKB
	keyboardLayoutId = freerdp_keyboard_init_xkb(keyboardLayoutId);

	if (keyboardLayoutId == 0)
		keyboardLayoutId = freerdp_keyboard_init_x11(keyboardLayoutId);
#else

#ifdef WITH_X11
	keyboardLayoutId = freerdp_keyboard_init_x11(keyboardLayoutId);
#endif

#endif
	return keyboardLayoutId;
}

static int freerdp_keyboard_load_map(KeycodeToVkcode map, char* name)
{
	FILE* fp;
	char* pch;
	char* beg;
	char* end;
	int i = 0;
	int kbdFound = 0;
	char* keymap_path;
	uint32 keycode = 0;
	char buffer[1024] = "";
	char keymap_name[256] = "";
	char keymap_include[256] = "";
	char keymap_filename[256] = "";
	char keycodeString[32] = "";
	char vkcodeName[128] = "";

	beg = name;

	/* Extract file name and keymap name */
	if ((end = strrchr(name, '(')) != NULL)
	{
		strncpy(keymap_filename, &name[beg - name], end - beg);

		beg = end + 1;
		if ((end = strrchr(name, ')')) != NULL)
		{
			strncpy(keymap_name, &name[beg - name], end - beg);
			keymap_name[end - beg] = '\0';
		}
	}
	else
	{
		/* The keyboard name is the same as the file name */
		strcpy(keymap_filename, name);
		strcpy(keymap_name, name);
	}

	keymap_path = freerdp_construct_path(FREERDP_KEYMAP_PATH, keymap_filename);

	DEBUG_KBD("Loading keymap %s, first trying %s", name, keymap_path);

	if ((fp = fopen(keymap_path, "r")) == NULL)
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
				for(i = 0; i < sizeof(virtualKeyboard) / sizeof(VIRTUAL_KEY); i++)
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

				strncpy(keymap_include, beg, end - beg);
				keymap_include[end - beg] = '\0';

				freerdp_keyboard_load_map(map, keymap_include); /* Load included keymap */
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
			if (strncmp(keymap_name, pch, strlen(keymap_name)) == 0)
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

	DEBUG_KBD("loaded %d keymaps", keymapLoaded);

	if (keymapLoaded <= 0)
		printf("error: no keyboard mapping available!\n");
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
