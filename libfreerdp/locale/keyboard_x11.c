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

#ifdef WITH_SUN
#include "keyboard_sun.h"
#endif

UINT32 freerdp_detect_keyboard_layout_from_xkb(char** xkb_layout, char** xkb_variant)
{
	char* pch;
	char* beg;
	char* end;
	FILE* xprop;
	char buffer[1024];
	char* layout = NULL;
	char* variant = NULL;
	UINT32 keyboardLayoutId = 0;

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

	while (fgets(buffer, sizeof(buffer), xprop) != NULL)
	{
		if ((pch = strstr(buffer, "_XKB_RULES_NAMES_BACKUP(STRING) = ")) != NULL)
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
		*xkb_layout = _strdup(layout);
		*xkb_variant = _strdup(variant);
		return keyboardLayoutId;
	}

	/* Check _XKB_RULES_NAMES if _XKB_RULES_NAMES_BACKUP fails */

	xprop = popen("xprop -root _XKB_RULES_NAMES", "r");

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
	keyboardLayoutId = find_keyboard_layout_in_xorg_rules(layout, variant);

	if (keyboardLayoutId > 0)
	{
		*xkb_layout = _strdup(layout);
		*xkb_variant = _strdup(variant);
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
				keymap = (char*) malloc(length + 1);
				strncpy(keymap, beg, length);
				keymap[length] = '\0';

				break;
			}
		}
	}

	pclose(setxkbmap);

	return keymap;
}

#ifdef __APPLE__

#define APPLE_VK_ANSI_A			0x00
#define APPLE_VK_ANSI_S			0x01
#define APPLE_VK_ANSI_D			0x02
#define APPLE_VK_ANSI_F			0x03
#define APPLE_VK_ANSI_H			0x04
#define APPLE_VK_ANSI_G			0x05
#define APPLE_VK_ANSI_Z			0x06
#define APPLE_VK_ANSI_X			0x07
#define APPLE_VK_ANSI_C			0x08
#define APPLE_VK_ANSI_V			0x09
#define APPLE_VK_ANSI_B			0x0B
#define APPLE_VK_ANSI_Q			0x0C
#define APPLE_VK_ANSI_W			0x0D
#define APPLE_VK_ANSI_E			0x0E
#define APPLE_VK_ANSI_R			0x0F
#define APPLE_VK_ANSI_Y			0x10
#define APPLE_VK_ANSI_T			0x11
#define APPLE_VK_ANSI_1			0x12
#define APPLE_VK_ANSI_2			0x13
#define APPLE_VK_ANSI_3			0x14
#define APPLE_VK_ANSI_4			0x15
#define APPLE_VK_ANSI_6			0x16
#define APPLE_VK_ANSI_5			0x17
#define APPLE_VK_ANSI_Equal		0x18
#define APPLE_VK_ANSI_9			0x19
#define APPLE_VK_ANSI_7			0x1A
#define APPLE_VK_ANSI_Minus		0x1B
#define APPLE_VK_ANSI_8			0x1C
#define APPLE_VK_ANSI_0			0x1D
#define APPLE_VK_ANSI_RightBracket	0x1E
#define APPLE_VK_ANSI_O			0x1F
#define APPLE_VK_ANSI_U			0x20
#define APPLE_VK_ANSI_LeftBracket	0x21
#define APPLE_VK_ANSI_I			0x22
#define APPLE_VK_ANSI_P			0x23
#define APPLE_VK_ANSI_L			0x25
#define APPLE_VK_ANSI_J			0x26
#define APPLE_VK_ANSI_Quote		0x27
#define APPLE_VK_ANSI_K			0x28
#define APPLE_VK_ANSI_Semicolon		0x29
#define APPLE_VK_ANSI_Backslash		0x2A
#define APPLE_VK_ANSI_Comma		0x2B
#define APPLE_VK_ANSI_Slash		0x2C
#define APPLE_VK_ANSI_N			0x2D
#define APPLE_VK_ANSI_M			0x2E
#define APPLE_VK_ANSI_Period		0x2F
#define APPLE_VK_ANSI_Grave		0x32
#define APPLE_VK_ANSI_KeypadDecimal	0x41
#define APPLE_VK_ANSI_KeypadMultiply	0x43
#define APPLE_VK_ANSI_KeypadPlus	0x45
#define APPLE_VK_ANSI_KeypadClear	0x47
#define APPLE_VK_ANSI_KeypadDivide	0x4B
#define APPLE_VK_ANSI_KeypadEnter	0x4C
#define APPLE_VK_ANSI_KeypadMinus	0x4E
#define APPLE_VK_ANSI_KeypadEquals	0x51
#define APPLE_VK_ANSI_Keypad0		0x52
#define APPLE_VK_ANSI_Keypad1		0x53
#define APPLE_VK_ANSI_Keypad2		0x54
#define APPLE_VK_ANSI_Keypad3		0x55
#define APPLE_VK_ANSI_Keypad4		0x56
#define APPLE_VK_ANSI_Keypad5		0x57
#define APPLE_VK_ANSI_Keypad6		0x58
#define APPLE_VK_ANSI_Keypad7		0x59
#define APPLE_VK_ANSI_Keypad8		0x5B
#define APPLE_VK_ANSI_Keypad9		0x5C

#define APPLE_VK_Return			0x24
#define APPLE_VK_Tab			0x30
#define APPLE_VK_Space			0x31
#define APPLE_VK_Delete			0x33
#define APPLE_VK_Escape			0x35
#define APPLE_VK_Command		0x37
#define APPLE_VK_Shift			0x38
#define APPLE_VK_CapsLock		0x39
#define APPLE_VK_Option			0x3A
#define APPLE_VK_Control		0x3B
#define APPLE_VK_RightShift		0x3C
#define APPLE_VK_RightOption		0x3D
#define APPLE_VK_RightControl		0x3E
#define APPLE_VK_Function		0x3F
#define APPLE_VK_F17			0x40
#define APPLE_VK_VolumeUp		0x48
#define APPLE_VK_VolumeDown		0x49
#define APPLE_VK_Mute			0x4A
#define APPLE_VK_F18			0x4F
#define APPLE_VK_F19			0x50
#define APPLE_VK_F20			0x5A
#define APPLE_VK_F5			0x60
#define APPLE_VK_F6			0x61
#define APPLE_VK_F7			0x62
#define APPLE_VK_F3			0x63
#define APPLE_VK_F8			0x64
#define APPLE_VK_F9			0x65
#define APPLE_VK_F11			0x67
#define APPLE_VK_F13			0x69
#define APPLE_VK_F16			0x6A
#define APPLE_VK_F14			0x6B
#define APPLE_VK_F10			0x6D
#define APPLE_VK_F12			0x6F
#define APPLE_VK_F15			0x71
#define APPLE_VK_Help			0x72
#define APPLE_VK_Home			0x73
#define APPLE_VK_PageUp			0x74
#define APPLE_VK_ForwardDelete		0x75
#define APPLE_VK_F4			0x76
#define APPLE_VK_End			0x77
#define APPLE_VK_F2			0x78
#define APPLE_VK_PageDown		0x79
#define APPLE_VK_F1			0x7A
#define APPLE_VK_LeftArrow		0x7B
#define APPLE_VK_RightArrow		0x7C
#define APPLE_VK_DownArrow		0x7D
#define APPLE_VK_UpArrow		0x7E

#define APPLE_VK_ISO_Section		0x0A

#define APPLE_VK_JIS_Yen		0x5D
#define APPLE_VK_JIS_Underscore		0x5E
#define APPLE_VK_JIS_KeypadComma	0x5F
#define APPLE_VK_JIS_Eisu		0x66
#define APPLE_VK_JIS_Kana		0x68

const DWORD KEYCODE_TO_VKCODE_MACOSX[256] =
{
	0, /* 0 */
	0, /* 1 */
	0, /* 2 */
	0, /* 3 */
	0, /* 4 */
	0, /* 5 */
	0, /* 6 */
	0, /* 7 */
	VK_KEY_A, /* 8 */
	VK_KEY_S, /* 9 */
	VK_KEY_D, /* 10 */
	VK_KEY_F, /* 11 */
	VK_KEY_H, /* 12 */
	VK_KEY_G, /* 13 */
	VK_KEY_Z, /* 14 */
	VK_KEY_X, /* 15 */
	VK_KEY_C, /* 16 */
	VK_KEY_V, /* 17 */
	VK_OEM_102, /* 18 */
	VK_KEY_B, /* 19 */
	VK_KEY_Q, /* 20 */
	VK_KEY_W, /* 21 */
	VK_KEY_E, /* 22 */
	VK_KEY_R, /* 23 */
	VK_KEY_Y, /* 24 */
	VK_KEY_T, /* 25 */
	VK_KEY_1, /* 26 */
	VK_KEY_2, /* 27 */
	VK_KEY_3, /* 28 */
	VK_KEY_4, /* 29 */
	VK_KEY_6, /* 30 */
	VK_KEY_5, /* 31 */
	VK_OEM_PLUS, /* 32 */
	VK_KEY_9, /* 33 */
	VK_KEY_7, /* 34 */
	VK_OEM_MINUS, /* 35 */
	VK_KEY_8, /* 36 */
	VK_KEY_0, /* 37 */
	VK_OEM_6, /* 38 */
	VK_KEY_O, /* 39 */
	VK_KEY_U, /* 40 */
	VK_OEM_4, /* 41 */
	VK_KEY_I, /* 42 */
	VK_KEY_P, /* 43 */
	VK_RETURN, /* 44 */
	VK_KEY_L, /* 45 */
	VK_KEY_J, /* 46 */
	VK_OEM_7, /* 47 */
	VK_KEY_K, /* 48 */
	VK_OEM_1, /* 49 */
	VK_OEM_5, /* 50 */
	VK_OEM_COMMA, /* 51 */
	VK_OEM_2, /* 52 */
	VK_KEY_N, /* 53 */
	VK_KEY_M, /* 54 */
	VK_OEM_PERIOD, /* 55 */
	VK_TAB, /* 56 */
	VK_SPACE, /* 57 */
	VK_OEM_3, /* 58 */
	VK_BACK, /* 59 */
	0, /* 60 */
	VK_ESCAPE, /* 61 */
	0, /* 62 */
	VK_LWIN, /* 63 */
	VK_LSHIFT, /* 64 */
	VK_CAPITAL, /* 65 */
	VK_LMENU, /* 66 */
	VK_LCONTROL, /* 67 */
	VK_RSHIFT, /* 68 */
	VK_RMENU, /* 69 */
	0, /* 70 */
	VK_RWIN, /* 71 */
	0, /* 72 */
	VK_DECIMAL, /* 73 */
	0, /* 74 */
	VK_MULTIPLY, /* 75 */
	0, /* 76 */
	VK_ADD, /* 77 */
	0, /* 78 */
	VK_NUMLOCK, /* 79 */
	0, /* 80 */
	0, /* 81 */
	0, /* 82 */
	VK_DIVIDE, /* 83 */
	VK_RETURN, /* 84 */
	0, /* 85 */
	VK_SUBTRACT, /* 86 */
	0, /* 87 */
	0, /* 88 */
	0, /* 89 */
	VK_NUMPAD0, /* 90 */
	VK_NUMPAD1, /* 91 */
	VK_NUMPAD2, /* 92 */
	VK_NUMPAD3, /* 93 */
	VK_NUMPAD4, /* 94 */
	VK_NUMPAD5, /* 95 */
	VK_NUMPAD6, /* 96 */
	VK_NUMPAD7, /* 97 */
	0, /* 98 */
	VK_NUMPAD8, /* 99 */
	VK_NUMPAD9, /* 100 */
	0, /* 101 */
	0, /* 102 */
	0, /* 103 */
	VK_F5, /* 104 */
	VK_F6, /* 105 */
	VK_F7, /* 106 */
	VK_F3, /* 107 */
	VK_F8, /* 108 */
	VK_F9, /* 109 */
	0, /* 110 */
	VK_F11, /* 111 */
	0, /* 112 */
	VK_SNAPSHOT, /* 113 */
	0, /* 114 */
	VK_SCROLL, /* 115 */
	0, /* 116 */
	VK_F10, /* 117 */
	0, /* 118 */
	VK_F12, /* 119 */
	0, /* 120 */
	VK_PAUSE, /* 121 */
	VK_INSERT, /* 122 */
	VK_HOME, /* 123 */
	VK_PRIOR, /* 124 */
	VK_DELETE, /* 125 */
	VK_F4, /* 126 */
	VK_END, /* 127 */
	VK_F2, /* 128 */
	VK_NEXT, /* 129 */
	VK_F1, /* 130 */
	VK_LEFT, /* 131 */
	VK_RIGHT, /* 132 */
	VK_DOWN, /* 133 */
	VK_UP, /* 134 */
	0, /* 135 */
	0, /* 136 */
	0, /* 137 */
	0, /* 138 */
	0, /* 139 */
	0, /* 140 */
	0, /* 141 */
	0, /* 142 */
	0, /* 143 */
	0, /* 144 */
	0, /* 145 */
	0, /* 146 */
	0, /* 147 */
	0, /* 148 */
	0, /* 149 */
	0, /* 150 */
	0, /* 151 */
	0, /* 152 */
	0, /* 153 */
	0, /* 154 */
	0, /* 155 */
	0, /* 156 */
	0, /* 157 */
	0, /* 158 */
	0, /* 159 */
	0, /* 160 */
	0, /* 161 */
	0, /* 162 */
	0, /* 163 */
	0, /* 164 */
	0, /* 165 */
	0, /* 166 */
	0, /* 167 */
	0, /* 168 */
	0, /* 169 */
	0, /* 170 */
	0, /* 171 */
	0, /* 172 */
	0, /* 173 */
	0, /* 174 */
	0, /* 175 */
	0, /* 176 */
	0, /* 177 */
	0, /* 178 */
	0, /* 179 */
	0, /* 180 */
	0, /* 181 */
	0, /* 182 */
	0, /* 183 */
	0, /* 184 */
	0, /* 185 */
	0, /* 186 */
	0, /* 187 */
	0, /* 188 */
	0, /* 189 */
	0, /* 190 */
	0, /* 191 */
	0, /* 192 */
	0, /* 193 */
	0, /* 194 */
	0, /* 195 */
	0, /* 196 */
	0, /* 197 */
	0, /* 198 */
	0, /* 199 */
	0, /* 200 */
	0, /* 201 */
	0, /* 202 */
	0, /* 203 */
	0, /* 204 */
	0, /* 205 */
	0, /* 206 */
	0, /* 207 */
	0, /* 208 */
	0, /* 209 */
	0, /* 210 */
	0, /* 211 */
	0, /* 212 */
	0, /* 213 */
	0, /* 214 */
	0, /* 215 */
	0, /* 216 */
	0, /* 217 */
	0, /* 218 */
	0, /* 219 */
	0, /* 220 */
	0, /* 221 */
	0, /* 222 */
	0, /* 223 */
	0, /* 224 */
	0, /* 225 */
	0, /* 226 */
	0, /* 227 */
	0, /* 228 */
	0, /* 229 */
	0, /* 230 */
	0, /* 231 */
	0, /* 232 */
	0, /* 233 */
	0, /* 234 */
	0, /* 235 */
	0, /* 236 */
	0, /* 237 */
	0, /* 238 */
	0, /* 239 */
	0, /* 240 */
	0, /* 241 */
	0, /* 242 */
	0, /* 243 */
	0, /* 244 */
	0, /* 245 */
	0, /* 246 */
	0, /* 247 */
	0, /* 248 */
	0, /* 249 */
	0, /* 250 */
	0, /* 251 */
	0, /* 252 */
	0, /* 253 */
	0, /* 254 */
	0 /* 255 */
};

#endif

UINT32 freerdp_keyboard_init_x11(UINT32 keyboardLayoutId, RDP_SCANCODE x11_keycode_to_rdp_scancode[256])
{
	DWORD vkcode;
	DWORD keycode;
	DWORD keycode_to_vkcode[256];

	ZeroMemory(keycode_to_vkcode, sizeof(keycode_to_vkcode));
	ZeroMemory(x11_keycode_to_rdp_scancode, sizeof(RDP_SCANCODE) * 256);

#ifdef __APPLE__
	/* Apple X11 breaks XKB detection */

	CopyMemory(keycode_to_vkcode, KEYCODE_TO_VKCODE_MACOSX, sizeof(keycode_to_vkcode));

#elif defined(WITH_SUN)
	{
		char sunkeymap[32];

		freerdp_detect_keyboard_type_and_layout_solaris(sunkeymap, sizeof(sunkeymap));
		freerdp_keyboard_load_map(keycode_to_vkcode, sunkeymap);
	}
#else
	{
		char* keymap;
		char* xkb_layout = 0;
		char* xkb_variant = 0;

		if (keyboardLayoutId == 0)
		{
			keyboardLayoutId = freerdp_detect_keyboard_layout_from_xkb(&xkb_layout, &xkb_variant);

			if (xkb_layout)
				free(xkb_layout);

			if (xkb_variant)
				free(xkb_variant);
		}

		keymap = freerdp_detect_keymap_from_xkb();

		if (keymap)
		{
			freerdp_keyboard_load_maps(keycode_to_vkcode, keymap);
			free(keymap);
		}
	}
#endif

	for (keycode = 0; keycode < 256; keycode++)
	{
		vkcode = keycode_to_vkcode[keycode];
		
		if (vkcode >= 0xFF)
			continue;
		
		x11_keycode_to_rdp_scancode[keycode] = GetVirtualScanCodeFromVirtualKeyCode(vkcode, 4);
	}
	
	return keyboardLayoutId;
}
