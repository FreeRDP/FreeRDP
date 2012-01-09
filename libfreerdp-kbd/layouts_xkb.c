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

#ifdef WITH_XKBFILE
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBfile.h>
#include <X11/extensions/XKBrules.h>
#endif

#include "libkbd.h"
#include <freerdp/kbd/vkcodes.h>
#include "x_layout_id_table.h"

#include "layouts_xkb.h"

#ifndef KEYMAP_PATH
#define KEYMAP_PATH	"/usr/local/freerdp/keymaps"
#endif

#ifdef WITH_XKBFILE

int init_xkb(void* dpy)
{
	return XkbQueryExtension(dpy, NULL, NULL, NULL, NULL, NULL);
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

unsigned int detect_keyboard_layout_from_xkb(void* dpy)
{
	char *layout, *variant;
	unsigned int keyboard_layout = 0, group = 0;
	XkbRF_VarDefsRec rules_names;
	XKeyboardState coreKbdState;
	XkbStateRec state;

	DEBUG_KBD("display: %p", dpy);

	if (dpy && XkbRF_GetNamesProp(dpy, NULL, &rules_names))
	{
		DEBUG_KBD("layouts: %s", rules_names.layout ? rules_names.layout : "");
		DEBUG_KBD("variants: %s", rules_names.variant ? rules_names.variant : "");

		XGetKeyboardControl(dpy, &coreKbdState);

		if (XkbGetState(dpy, XkbUseCoreKbd, &state) == Success)
			group = state.group;

		DEBUG_KBD("group: %d", state.group);

		layout = comma_substring(rules_names.layout, group);
		variant = comma_substring(rules_names.variant, group);

		DEBUG_KBD("layout: %s", layout ? layout : "");
		DEBUG_KBD("variant: %s", variant ? variant : "");

		keyboard_layout = find_keyboard_layout_in_xorg_rules(layout, variant);

		free(rules_names.model);
		free(rules_names.layout);
		free(rules_names.variant);
		free(rules_names.options);
	}

	return keyboard_layout;
}

int init_keycodes_from_xkb(void* dpy, RdpScancodes x_keycode_to_rdp_scancode, uint8 rdp_scancode_to_x_keycode[256][2])
{
	int ret = 0;
	XkbDescPtr xkb;

	if (dpy && (xkb = XkbGetMap(dpy, 0, XkbUseCoreKbd)))
	{
		if (XkbGetNames(dpy, XkbKeyNamesMask, xkb) == Success)
		{
			int i, j;
			char buf[5] = {42, 42, 42, 42, 0}; /* end-of-string at pos 5 */

			for (i = xkb->min_key_code; i <= xkb->max_key_code; i++)
			{
				memcpy(buf, xkb->names->keys[i].name, 4);

				/* TODO: Use more efficient search ... but it is so fast that it doesn't matter */
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
			ret = 1;
		}
		XkbFreeKeyboard(xkb, 0, 1);
	}
	return ret;
}

#else

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

static int load_xkb_keyboard(KeycodeToVkcode map, char* kbd)
{
	char* pch;
	char *beg, *end;
	char* home;
	char buffer[1024] = "";
	char xkbfile[256] = "";
	char xkbfilepath[512] = "";
	char xkbmap[256] = "";
	char xkbinc[256] = "";

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
	snprintf(xkbfilepath, sizeof(xkbfilepath), "keymaps/%s", xkbfile);
	DEBUG_KBD("Loading keymap %s, first trying %s", kbd, xkbfilepath);

	/*
	 *  Open the file for reading only
	 * It can happen that the same file is opened twice at the same time
	 * in order to load multiple keyboard maps from the same file, but
	 * it does not matter: files can be opened as many times as we want
	 * when it is for reading only.
	 */

	if ((fp = fopen(xkbfilepath, "r")) == NULL)
	{
		/* Look first in path given at compile time (install path) */
		snprintf(xkbfilepath, sizeof(xkbfilepath), "%s/%s", KEYMAP_PATH, xkbfile);

		if ((fp = fopen(xkbfilepath, "r")) == NULL)
		{
			/* If ran from the root of the source tree */
			snprintf(xkbfilepath, sizeof(xkbfilepath), "./keymaps/%s", xkbfile);

			/* If ran from the client directory */
			if((fp = fopen(xkbfilepath, "r")) == NULL)
				snprintf(xkbfilepath, sizeof(xkbfilepath), "../../keymaps/%s", xkbfile);

			if ((fp = fopen(xkbfilepath, "r")) == NULL)
			{
				/* File wasn't found in the source tree, try ~/.freerdp/ folder */
				if ((home = getenv("HOME")) == NULL)
					return 0;

				/* Get path to file in ~/.freerdp/ folder */
				snprintf(xkbfilepath, sizeof(xkbfilepath), "%s/.freerdp/keymaps/%s", home, xkbfile);

				if ((fp = fopen(xkbfilepath, "r")) == NULL)
				{
					/* Try /usr/share/freerdp folder */
					snprintf(xkbfilepath, sizeof(xkbfilepath), "/usr/share/freerdp/keymaps/%s", xkbfile);

					if ((fp = fopen(xkbfilepath, "r")) == NULL)
					{
						/* Try /usr/local/share/freerdp folder */
						snprintf(xkbfilepath, sizeof(xkbfilepath), "/usr/local/share/freerdp/keymaps/%s", xkbfile);

						if ((fp = fopen(xkbfilepath, "r")) == NULL)
						{
							/* Error: Could not find keymap */
							DEBUG_KBD("keymaps for %s not found", xkbfile);
							return 0;
						}
					}
				}
			}
		}
	}

	DEBUG_KBD("xkbfilepath: %s", xkbfilepath);

	while(fgets(buffer, sizeof(buffer), fp) != NULL)
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

				load_xkb_keyboard(map, xkbinc); /* Load included keymap */
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

void load_keyboard_map(KeycodeToVkcode keycodeToVkcode, char *xkbfile)
{
	char* kbd;
	char* xkbfileEnd;
	int keymapLoaded = 0;

	memset(keycodeToVkcode, 0, sizeof(keycodeToVkcode));

	kbd = xkbfile;
	xkbfileEnd = xkbfile + strlen(xkbfile);

#ifdef __APPLE__
	/* Apple X11 breaks XKB detection */
	keymapLoaded += load_xkb_keyboard(keycodeToVkcode, "macosx(macosx)");
#else
	do
	{
		/* Multiple maps are separated by '+' */
		int kbdlen = strcspn(kbd + 1, "+") + 1;
		kbd[kbdlen] = '\0';

		/* Load keyboard map */
		keymapLoaded += load_xkb_keyboard(keycodeToVkcode, kbd);

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

#endif
