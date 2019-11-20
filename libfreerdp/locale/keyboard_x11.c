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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/input.h>

#include "liblocale.h"

#include <freerdp/locale/locale.h>
#include <freerdp/locale/keyboard.h>

#include "keyboard_x11.h"
#include "xkb_layout_ids.h"

int freerdp_detect_keyboard_layout_from_xkb(DWORD* keyboardLayoutId)
{
	char* pch;
	char* beg;
	char* end;
	FILE* xprop;
	char buffer[1024];
	char* layout = NULL;
	char* variant = NULL;

	/* We start by looking for _XKB_RULES_NAMES_BACKUP which appears to be used by libxklavier */

	if (!(xprop = popen("xprop -root _XKB_RULES_NAMES_BACKUP", "r")))
		return 0;

	/* Sample output for "Canadian Multilingual Standard"
	 *
	 * _XKB_RULES_NAMES_BACKUP(STRING) = "xorg", "pc105", "ca", "multix", ""
	 * Where "xorg" is the set of rules
	 * "pc105" the keyboard type
	 * "ca" the keyboard layout
	 * "multi" the keyboard layout variant
	 */

	while (fgets(buffer, sizeof(buffer), xprop) != NULL)
	{
		if ((pch = strstr(buffer, "_XKB_RULES_NAMES_BACKUP(STRING) = ")) != NULL)
		{
			/* "rules" */
			pch = strchr(&buffer[34], ','); /* We assume it is xorg */
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
	*keyboardLayoutId = find_keyboard_layout_in_xorg_rules(layout, variant);

	if (*keyboardLayoutId > 0)
		return 0;

	/* Check _XKB_RULES_NAMES if _XKB_RULES_NAMES_BACKUP fails */

	if (!(xprop = popen("xprop -root _XKB_RULES_NAMES", "r")))
		return 0;

	while (fgets(buffer, sizeof(buffer), xprop) != NULL)
	{
		if ((pch = strstr(buffer, "_XKB_RULES_NAMES(STRING) = ")) != NULL)
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
	*keyboardLayoutId = find_keyboard_layout_in_xorg_rules(layout, variant);

	if (*keyboardLayoutId > 0)
		return *keyboardLayoutId;

	return 0;
}

static char* freerdp_detect_keymap_from_xkb(void)
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

	if (!setxkbmap)
		return NULL;

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
				keymap = (char*)malloc(length + 1);
				if (keymap)
				{
					strncpy(keymap, beg, length);
					keymap[length] = '\0';
				}

				break;
			}
		}
	}

	pclose(setxkbmap);

	return keymap;
}
