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

#include "liblocale.h"

#include "keyboard_xkb.h"
#include "keyboard_x11.h"
#include <freerdp/locale/keyboard.h>

extern uint32 RDP_SCANCODE_TO_X11_KEYCODE[256][2];
extern RDP_SCANCODE X11_KEYCODE_TO_RDP_SCANCODE[256];
extern const RDP_SCANCODE VIRTUAL_KEY_CODE_TO_RDP_SCANCODE_TABLE[];

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBfile.h>
#include <X11/extensions/XKBrules.h>

#include <freerdp/utils/memory.h>

VIRTUAL_KEY_CODE_TO_XKB_KEY_NAME VIRTUAL_KEY_CODE_TO_XKB_KEY_NAME_TABLE[] =
{
	{ 0, "" },
	{ VK_LBUTTON, "" },
	{ VK_RBUTTON, "" },
	{ VK_CANCEL, "" },
	{ VK_MBUTTON, "" },
	{ VK_XBUTTON1, "" },
	{ VK_XBUTTON2, "" },
	{ 0, "" },
	{ VK_BACK, "BKSP" },
	{ VK_TAB, "TAB" },
	{ 0, "" },
	{ 0, "" },
	{ VK_CLEAR, "" },
	{ VK_RETURN, "RTRN" },
	{ 0, "" },
	{ 0, "" },
	{ VK_SHIFT, "LFSH" },
	{ VK_CONTROL, "" },
	{ VK_MENU, "LALT" },
	{ VK_PAUSE, "PAUS" },
	{ VK_CAPITAL, "CAPS" },
	{ VK_KANA / VK_HANGUL, "" },
	{ 0, "" },
	{ VK_JUNJA, "" },
	{ VK_FINAL, "" },
	{ VK_HANJA / VK_KANJI, "" },
	{ 0, "" },
	{ VK_ESCAPE, "ESC" },
	{ VK_CONVERT, "" },
	{ VK_NONCONVERT, "" },
	{ VK_ACCEPT, "" },
	{ VK_MODECHANGE, "" },
	{ VK_SPACE, "SPCE" },
	{ VK_PRIOR, "PGUP" },
	{ VK_NEXT, "PGDN" },
	{ VK_END, "END" },
	{ VK_HOME, "HOME" },
	{ VK_LEFT, "LEFT" },
	{ VK_UP, "UP" },
	{ VK_RIGHT, "RGHT" },
	{ VK_DOWN, "DOWN" },
	{ VK_SELECT, "" },
	{ VK_PRINT, "PRSC" },
	{ VK_EXECUTE, "" },
	{ VK_SNAPSHOT, "" },
	{ VK_INSERT, "INS" },
	{ VK_DELETE, "DELE" },
	{ VK_HELP, "" },
	{ VK_KEY_0, "AE10" },
	{ VK_KEY_1, "AE01" },
	{ VK_KEY_2, "AE02" },
	{ VK_KEY_3, "AE03" },
	{ VK_KEY_4, "AE04" },
	{ VK_KEY_5, "AE05" },
	{ VK_KEY_6, "AE06" },
	{ VK_KEY_7, "AE07" },
	{ VK_KEY_8, "AE08" },
	{ VK_KEY_9, "AE09" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ VK_KEY_A, "AC01" },
	{ VK_KEY_B, "AB05" },
	{ VK_KEY_C, "AB03" },
	{ VK_KEY_D, "AC03" },
	{ VK_KEY_E, "AD03" },
	{ VK_KEY_F, "AC04" },
	{ VK_KEY_G, "AC05" },
	{ VK_KEY_H, "AC06" },
	{ VK_KEY_I, "AD08" },
	{ VK_KEY_J, "AC07" },
	{ VK_KEY_K, "AC08" },
	{ VK_KEY_L, "AC09" },
	{ VK_KEY_M, "AB07" },
	{ VK_KEY_N, "AB06" },
	{ VK_KEY_O, "AD09" },
	{ VK_KEY_P, "AD10" },
	{ VK_KEY_Q, "AD01" },
	{ VK_KEY_R, "AD04" },
	{ VK_KEY_S, "AC02" },
	{ VK_KEY_T, "AD05" },
	{ VK_KEY_U, "AD07" },
	{ VK_KEY_V, "AB04" },
	{ VK_KEY_W, "AD02" },
	{ VK_KEY_X, "AB02" },
	{ VK_KEY_Y, "AD06" },
	{ VK_KEY_Z, "AB01" },
	{ VK_LWIN, "LWIN" },
	{ VK_RWIN, "RWIN" },
	{ VK_APPS, "COMP" },
	{ 0, "" },
	{ VK_SLEEP, "" },
	{ VK_NUMPAD0, "KP0" },
	{ VK_NUMPAD1, "KP1" },
	{ VK_NUMPAD2, "KP2" },
	{ VK_NUMPAD3, "KP3" },
	{ VK_NUMPAD4, "KP4" },
	{ VK_NUMPAD5, "KP5" },
	{ VK_NUMPAD6, "KP6" },
	{ VK_NUMPAD7, "KP7" },
	{ VK_NUMPAD8, "KP8" },
	{ VK_NUMPAD9, "KP9" },
	{ VK_MULTIPLY, "KPMU" },
	{ VK_ADD, "KPAD" },
	{ VK_SEPARATOR, "" },
	{ VK_SUBTRACT, "KPSU" },
	{ VK_DECIMAL, "KPDL" },
	{ VK_DIVIDE, "KPDV" },
	{ VK_F1, "FK01" },
	{ VK_F2, "FK02" },
	{ VK_F3, "FK03" },
	{ VK_F4, "FK04" },
	{ VK_F5, "FK05" },
	{ VK_F6, "FK06" },
	{ VK_F7, "FK07" },
	{ VK_F8, "FK08" },
	{ VK_F9, "FK09" },
	{ VK_F10, "FK10" },
	{ VK_F11, "FK11" },
	{ VK_F12, "FK12" },
	{ VK_F13, "" },
	{ VK_F14, "" },
	{ VK_F15, "" },
	{ VK_F16, "" },
	{ VK_F17, "" },
	{ VK_F18, "" },
	{ VK_F19, "" },
	{ VK_F20, "" },
	{ VK_F21, "" },
	{ VK_F22, "" },
	{ VK_F23, "" },
	{ VK_F24, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ VK_NUMLOCK, "NMLK" },
	{ VK_SCROLL, "SCLK" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ VK_LSHIFT, "" },
	{ VK_RSHIFT, "RTSH" },
	{ VK_LCONTROL, "LCTL" },
	{ VK_RCONTROL, "RCTL" },
	{ VK_LMENU, "" },
	{ VK_RMENU, "RALT" },
	{ VK_BROWSER_BACK, "" },
	{ VK_BROWSER_FORWARD, "" },
	{ VK_BROWSER_REFRESH, "" },
	{ VK_BROWSER_STOP, "" },
	{ VK_BROWSER_SEARCH, "" },
	{ VK_BROWSER_FAVORITES, "" },
	{ VK_BROWSER_HOME, "" },
	{ VK_VOLUME_MUTE, "" },
	{ VK_VOLUME_DOWN, "" },
	{ VK_VOLUME_UP, "" },
	{ VK_MEDIA_NEXT_TRACK, "" },
	{ VK_MEDIA_PREV_TRACK, "" },
	{ VK_MEDIA_STOP, "" },
	{ VK_MEDIA_PLAY_PAUSE, "" },
	{ VK_LAUNCH_MAIL, "" },
	{ VK_MEDIA_SELECT, "" },
	{ VK_LAUNCH_APP1, "" },
	{ VK_LAUNCH_APP2, "" },
	{ 0, "" },
	{ 0, "" },
	{ VK_OEM_1, "AC10" },
	{ VK_OEM_PLUS, "AE12" },
	{ VK_OEM_COMMA, "AB08" },
	{ VK_OEM_MINUS, "AE11" },
	{ VK_OEM_PERIOD, "AB09" },
	{ VK_OEM_2, "AB10" },
	{ VK_OEM_3, "TLDE" },
	{ VK_ABNT_C1, "AB11" },
	{ VK_ABNT_C2, "I129" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ VK_OEM_4, "AD11" },
	{ VK_OEM_5, "BKSL" },
	{ VK_OEM_6, "AD12" },
	{ VK_OEM_7, "AC11" },
	{ VK_OEM_8, "" },
	{ 0, "" },
	{ 0, "" },
	{ VK_OEM_102, "LSGT" },
	{ 0, "" },
	{ 0, "" },
	{ VK_PROCESSKEY, "" },
	{ 0, "" },
	{ VK_PACKET, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ VK_ATTN, "" },
	{ VK_CRSEL, "" },
	{ VK_EXSEL, "" },
	{ VK_EREOF, "" },
	{ VK_PLAY, "" },
	{ VK_ZOOM, "" },
	{ VK_NONAME, "" },
	{ VK_PA1, "" },
	{ VK_OEM_CLEAR, "" },
	{ 0, "" }
};

/*
{ 0x54, 0, ""                    , "LVL3" },
{ 0x1C, 1, ""                    , "KPEN" }
*/

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

uint32 freerdp_keyboard_init_xkb(uint32 keyboardLayoutId)
{
	void* display;
	memset(X11_KEYCODE_TO_RDP_SCANCODE, 0, sizeof(X11_KEYCODE_TO_RDP_SCANCODE));
	memset(RDP_SCANCODE_TO_X11_KEYCODE, 0, sizeof(RDP_SCANCODE_TO_X11_KEYCODE));

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

	freerdp_keyboard_load_map_from_xkb(display);

	return keyboardLayoutId;
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

int freerdp_keyboard_load_map_from_xkb(void* display)
{
	int i, j;
	boolean found;
	XkbDescPtr xkb;
	boolean status = false;

	if (display && (xkb = XkbGetMap(display, 0, XkbUseCoreKbd)))
	{
		if (XkbGetNames(display, XkbKeyNamesMask, xkb) == Success)
		{
			char xkb_keyname[5] = { 42, 42, 42, 42, 0 }; /* end-of-string at index 5 */

			for (i = xkb->min_key_code; i <= xkb->max_key_code; i++)
			{
				found = false;
				memcpy(xkb_keyname, xkb->names->keys[i].name, 4);

				for (j = 0; j < 256; j++)
				{
					if (VIRTUAL_KEY_CODE_TO_XKB_KEY_NAME_TABLE[j].xkb_keyname)
					{
						if (!strcmp(xkb_keyname, VIRTUAL_KEY_CODE_TO_XKB_KEY_NAME_TABLE[j].xkb_keyname))
						{
							found = true;
							break;
						}
					}
				}

				if (found)
				{
					uint32 vkcode = VIRTUAL_KEY_CODE_TO_XKB_KEY_NAME_TABLE[j].vkcode;

					X11_KEYCODE_TO_RDP_SCANCODE[i].code = VIRTUAL_KEY_CODE_TO_RDP_SCANCODE_TABLE[vkcode].code;
					X11_KEYCODE_TO_RDP_SCANCODE[i].extended = VIRTUAL_KEY_CODE_TO_RDP_SCANCODE_TABLE[vkcode].extended;

					if (X11_KEYCODE_TO_RDP_SCANCODE[i].extended)
						RDP_SCANCODE_TO_X11_KEYCODE[VIRTUAL_KEY_CODE_TO_RDP_SCANCODE_TABLE[vkcode].code][1] = i;
					else
						RDP_SCANCODE_TO_X11_KEYCODE[VIRTUAL_KEY_CODE_TO_RDP_SCANCODE_TABLE[vkcode].code][0] = i;
				}
			}

			status = true;
		}

		XkbFreeKeyboard(xkb, 0, 1);
	}

	return status;
}
