/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Keyboard Localization - loading of keymap files
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

#include "keyboard_keymap.h"

#include <freerdp/utils/file.h>
#include <freerdp/utils/memory.h>
#include <freerdp/locale/virtual_key_codes.h>
#include <freerdp/locale/keyboard.h>

#include "config.h"
#include "liblocale.h"

#include <stdlib.h>


extern const RDP_SCANCODE VIRTUAL_KEY_CODE_TO_DEFAULT_RDP_SCANCODE_TABLE[256];

int freerdp_keyboard_load_map(uint32 keycode_to_vkcode[256], char* name)
{
	FILE* fp;
	char* pch;
	char* beg;
	char* end;
	uint32 vkcode;
	int kbd_found = 0;
	char* keymap_path;
	uint32 keycode = 0;
	char buffer[1024] = "";
	char keymap_name[256] = "";
	char keymap_include[256] = "";
	char keymap_filename[256] = "";
	char keycode_string[32] = "";
	char vkcode_name[128] = "";

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
		DEBUG_KBD("%s not found", keymap_path);
		xfree(keymap_path);
		return 0;
	}
	xfree(keymap_path);

	while (fgets(buffer, sizeof(buffer), fp) != NULL)
	{
		if (buffer[0] == '#')
		{
			continue; /* Skip comments */
		}

		if (kbd_found)
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
				strncpy(vkcode_name, beg, end - beg);
				vkcode_name[end - beg] = '\0';

				/* Now we want to extract the virtual key code itself which is in between '<' and '>' */
				if ((beg = strchr(pch + 3, '<')) == NULL)
					break;
				else
					beg++;

				if ((end = strchr(beg, '>')) == NULL)
					break;

				/* We copy the string representing the number in a string */
				strncpy(keycode_string, beg, end - beg);
				keycode_string[end - beg] = '\0';

				/* Convert the string representing the code to an integer */
				keycode = atoi(keycode_string);

				/* Make sure it is a valid keycode */
				if (keycode < 0 || keycode > 255)
					break;

				/* Load this key mapping in the keyboard mapping */
				vkcode = freerdp_keyboard_get_virtual_key_code_from_name(vkcode_name);
				keycode_to_vkcode[keycode] = vkcode;
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

				freerdp_keyboard_load_map(keycode_to_vkcode, keymap_include); /* Load included keymap */
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
				kbd_found = 1;
		}
	}

	fclose(fp); /* Don't forget to close file */

	return 1;
}

void freerdp_keyboard_load_maps(uint32 keycode_to_vkcode[256], char* names)
{
	char* kbd;
	char* names_end;
	int keymap_loaded = 0;

	memset(keycode_to_vkcode, 0, sizeof(keycode_to_vkcode));

	kbd = names;
	names_end = names + strlen(names);

	do
	{
		/* multiple maps are separated by '+' */
		int kbd_length = strcspn(kbd + 1, "+") + 1;
		kbd[kbd_length] = '\0';

		/* Load keyboard map */
		keymap_loaded += freerdp_keyboard_load_map(keycode_to_vkcode, kbd);

		kbd += kbd_length + 1;
	}
	while (kbd < names_end);

	DEBUG_KBD("loaded %d keymaps", keymap_loaded);

	if (keymap_loaded <= 0)
		printf("error: no keyboard mapping available!\n");
}
