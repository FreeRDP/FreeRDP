/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Keyboard Mapping
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
#include <stdlib.h>
#include <string.h>

#include "liblocale.h"
#include <freerdp/utils/memory.h>
#include <freerdp/locale/locale.h>
#include <freerdp/locale/keyboard.h>

#include "keyboard_x11.h"
#include "keyboard_keymap.h"
#include "xkb_layout_ids.h"

#ifdef WITH_SUN
#include "keyboard_sun.h"
#endif


extern const RDP_SCANCODE VIRTUAL_KEY_CODE_TO_DEFAULT_RDP_SCANCODE_TABLE[256];


uint32 freerdp_detect_keyboard_layout_from_xkb(char** xkb_layout, char** xkb_variant)
{
	char* pch;
	char* beg;
	char* end;
	FILE* xprop;
	char buffer[1024];
	char* layout = NULL;
	char* variant = NULL;
	uint32 keyboardLayoutId = 0;

	/* We start by looking for _XKB_RULES_NAMES_BACKUP which appears to be used by libxklavier */

        xprop = popen("xprop -root _XKB_RULES_NAMES_BACKUP", "r");

	/* Sample output for "Canadian Multilingual Standard"
	 *
	 * _XKB_RULES_NAMES_BACKUP(STRING) = "xorg", "pc105", "ca", "multix", ""
	 * Where "xorg" is the set of rules
	 * "pc105" the keyboard type
	 * "ca" the keyboard layout
	 * "multi" the keyboard layout variant
	 */

	while(fgets(buffer, sizeof(buffer), xprop) != NULL)
	{
		if((pch = strstr(buffer, "_XKB_RULES_NAMES_BACKUP(STRING) = ")) != NULL)
		{
			/* "rules" */
			pch = strchr(&buffer[34], ','); // We assume it is xorg
			pch += 1;

			/* "type" */
			pch = strchr(pch, ',');

			/* "layout" */
			beg = strchr(pch + 1, '"');
			beg += 1;

			end = strchr(beg, '"');
			*end = '\0';

			layout = beg;

			/* "variant" */
			beg = strchr(end + 1, '"');
			beg += 1;

			end = strchr(beg, '"');
			*end = '\0';

			variant = beg;
		}
	}
	pclose(xprop);

	DEBUG_KBD("_XKB_RULES_NAMES_BACKUP layout: %s, variant: %s", layout, variant);
	keyboardLayoutId = find_keyboard_layout_in_xorg_rules(layout, variant);

	if (keyboardLayoutId > 0)
	{
		*xkb_layout = xstrdup(layout);
		*xkb_variant = xstrdup(variant);
		return keyboardLayoutId;
	}

	/* Check _XKB_RULES_NAMES if _XKB_RULES_NAMES_BACKUP fails */

	xprop = popen("xprop -root _XKB_RULES_NAMES", "r");

	while(fgets(buffer, sizeof(buffer), xprop) != NULL)
	{
		if((pch = strstr(buffer, "_XKB_RULES_NAMES(STRING) = ")) != NULL)
		{
			/* "rules" */
			pch = strchr(&buffer[27], ','); // We assume it is xorg
			pch += 1;

			/* "type" */
			pch = strchr(pch, ',');

			/* "layout" */
			beg = strchr(pch + 1, '"');
			beg += 1;

			end = strchr(beg, '"');
			*end = '\0';

			layout = beg;

			/* "variant" */
			beg = strchr(end + 1, '"');
			beg += 1;

			end = strchr(beg, '"');
			*end = '\0';

			variant = beg;
		}
	}
	pclose(xprop);

	DEBUG_KBD("_XKB_RULES_NAMES layout: %s, variant: %s", layout, variant);
	keyboardLayoutId = find_keyboard_layout_in_xorg_rules(layout, variant);

	if (keyboardLayoutId > 0)
	{
		*xkb_layout = xstrdup(layout);
		*xkb_variant = xstrdup(variant);
		return keyboardLayoutId;
	}

	return 0;
}

char* freerdp_detect_keymap_from_xkb()
{
	char* pch;
	char* beg;
	char* end;
	int length;
	FILE* setxkbmap;
	char buffer[1024];
	char* keymap = NULL;

	/* this tells us about the current XKB configuration, if XKB is available */
	setxkbmap = popen("setxkbmap -print", "r");

	while (fgets(buffer, sizeof(buffer), setxkbmap) != NULL)
	{
		/* the line with xkb_keycodes is what interests us */
		pch = strstr(buffer, "xkb_keycodes");

		if (pch != NULL)
		{
			pch = strstr(pch, "include");

			if (pch != NULL)
			{
				/* check for " " delimiter presence */
				if ((beg = strchr(pch, '"')) == NULL)
					break;
				else
					beg++;

				if ((pch = strchr(beg + 1, '"')) == NULL)
					break;

				end = strcspn(beg + 1, "\"") + beg + 1;
				*end = '\0';

				length = (end - beg);
				keymap = (char*) xmalloc(length + 1);
				strncpy(keymap, beg, length);
				keymap[length] = '\0';

				break;
			}
		}
	}

	pclose(setxkbmap);

	return keymap;
}

uint32 freerdp_keyboard_init_x11(uint32 keyboardLayoutId, RDP_SCANCODE x11_keycode_to_rdp_scancode[256])
{
	uint32 vkcode;
	uint32 keycode;
	uint32 keycode_to_vkcode[256];

	memset(keycode_to_vkcode, 0, sizeof(keycode_to_vkcode));
	memset(x11_keycode_to_rdp_scancode, 0, sizeof(x11_keycode_to_rdp_scancode));

#ifdef __APPLE__
	/* Apple X11 breaks XKB detection */
	freerdp_keyboard_load_map(keycode_to_vkcode, "macosx(macosx)");
#elif defined(WITH_SUN)
	{
		char sunkeymap[32];

		freerdp_detect_keyboard_type_and_layout_solaris(sunkeymap, sizeof(sunkeymap));
		freerdp_keyboard_load_map(keycode_to_vkcode, sunkeymap);
	}
#else
	{
		char* keymap;
		char* xkb_layout;
		char* xkb_variant;

		if (keyboardLayoutId == 0)
		{
			keyboardLayoutId = freerdp_detect_keyboard_layout_from_xkb(&xkb_layout, &xkb_variant);
			xfree(xkb_layout);
			xfree(xkb_variant);
		}

		keymap = freerdp_detect_keymap_from_xkb();

		if (keymap != NULL)
		{
			freerdp_keyboard_load_maps(keycode_to_vkcode, keymap);
			xfree(keymap);
		}
	}
#endif

	for (keycode = 0; keycode < 256; keycode++)
	{
		vkcode = keycode_to_vkcode[keycode];

		if (!(vkcode > 0 && vkcode < 256))
			continue;

		x11_keycode_to_rdp_scancode[keycode] = VIRTUAL_KEY_CODE_TO_DEFAULT_RDP_SCANCODE_TABLE[vkcode];
	}

	return keyboardLayoutId;
}
