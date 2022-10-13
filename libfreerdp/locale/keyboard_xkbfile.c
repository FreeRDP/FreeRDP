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

#include <freerdp/config.h>

#include "keyboard_xkbfile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/input.h>

#include <freerdp/locale/keyboard.h>

#include "keyboard_x11.h"
#include "xkb_layout_ids.h"
#include "liblocale.h"

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBfile.h>
#include <X11/extensions/XKBrules.h>

typedef struct
{
	const char* xkb_keyname; /* XKB keyname */
	DWORD rdp_scancode;
} XKB_KEY_NAME_SCANCODE;

static const XKB_KEY_NAME_SCANCODE XKB_KEY_NAME_SCANCODE_TABLE[] = {
	{ "AB00", RDP_SCANCODE_LSHIFT },
	{ "AB01", RDP_SCANCODE_KEY_Z },      // evdev 52
	{ "AB02", RDP_SCANCODE_KEY_X },      // evdev 53
	{ "AB03", RDP_SCANCODE_KEY_C },      // evdev 54
	{ "AB04", RDP_SCANCODE_KEY_V },      // evdev 55
	{ "AB05", RDP_SCANCODE_KEY_B },      // evdev 56
	{ "AB06", RDP_SCANCODE_KEY_N },      // evdev 57
	{ "AB07", RDP_SCANCODE_KEY_M },      // evdev 58
	{ "AB08", RDP_SCANCODE_OEM_COMMA },  // evdev 59
	{ "AB09", RDP_SCANCODE_OEM_PERIOD }, // evdev 60
	{ "AB10", RDP_SCANCODE_OEM_2 },      // evdev 61. Not KP, not RDP_SCANCODE_DIVIDE
	{ "AB11", RDP_SCANCODE_ABNT_C1 },    // evdev 97.  Brazil backslash/underscore.
	{ "AC01", RDP_SCANCODE_KEY_A },      // evdev 38
	{ "AC02", RDP_SCANCODE_KEY_S },      // evdev 39
	{ "AC03", RDP_SCANCODE_KEY_D },      // evdev 40
	{ "AC04", RDP_SCANCODE_KEY_F },      // evdev 41
	{ "AC05", RDP_SCANCODE_KEY_G },      // evdev 42
	{ "AC06", RDP_SCANCODE_KEY_H },      // evdev 43
	{ "AC07", RDP_SCANCODE_KEY_J },      // evdev 44
	{ "AC08", RDP_SCANCODE_KEY_K },      // evdev 45
	{ "AC09", RDP_SCANCODE_KEY_L },      // evdev 46
	{ "AC10", RDP_SCANCODE_OEM_1 },      // evdev 47
	{ "AC11", RDP_SCANCODE_OEM_7 },      // evdev 48
	{ "AC12", RDP_SCANCODE_OEM_5 },      // alias of evdev 51 backslash
	{ "AD01", RDP_SCANCODE_KEY_Q },      // evdev 24
	{ "AD02", RDP_SCANCODE_KEY_W },      // evdev 25
	{ "AD03", RDP_SCANCODE_KEY_E },      // evdev 26
	{ "AD04", RDP_SCANCODE_KEY_R },      // evdev 27
	{ "AD05", RDP_SCANCODE_KEY_T },      // evdev 28
	{ "AD06", RDP_SCANCODE_KEY_Y },      // evdev 29
	{ "AD07", RDP_SCANCODE_KEY_U },      // evdev 30
	{ "AD08", RDP_SCANCODE_KEY_I },      // evdev 31
	{ "AD09", RDP_SCANCODE_KEY_O },      // evdev 32
	{ "AD10", RDP_SCANCODE_KEY_P },      // evdev 33
	{ "AD11", RDP_SCANCODE_OEM_4 },      // evdev 34
	{ "AD12", RDP_SCANCODE_OEM_6 },      // evdev 35
	{ "AE00", RDP_SCANCODE_OEM_3 },
	{ "AE01", RDP_SCANCODE_KEY_1 },        // evdev 10
	{ "AE02", RDP_SCANCODE_KEY_2 },        // evdev 11
	{ "AE03", RDP_SCANCODE_KEY_3 },        // evdev 12
	{ "AE04", RDP_SCANCODE_KEY_4 },        // evdev 13
	{ "AE05", RDP_SCANCODE_KEY_5 },        // evdev 14
	{ "AE06", RDP_SCANCODE_KEY_6 },        // evdev 15
	{ "AE07", RDP_SCANCODE_KEY_7 },        // evdev 16
	{ "AE08", RDP_SCANCODE_KEY_8 },        // evdev 17
	{ "AE09", RDP_SCANCODE_KEY_9 },        // evdev 18
	{ "AE10", RDP_SCANCODE_KEY_0 },        // evdev 19
	{ "AE11", RDP_SCANCODE_OEM_MINUS },    // evdev 20
	{ "AE12", RDP_SCANCODE_OEM_PLUS },     // evdev 21
	{ "AE13", RDP_SCANCODE_BACKSLASH_JP }, // JP 132 Yen next to backspace
	// { "AGAI", RDP_SCANCODE_ },	// evdev 137
	{ "ALGR", RDP_SCANCODE_RMENU },     // alias of evdev 108 RALT
	{ "ALT", RDP_SCANCODE_LMENU },      // evdev 204, fake keycode for virtual key
	{ "BKSL", RDP_SCANCODE_OEM_5 },     // evdev 51
	{ "BKSP", RDP_SCANCODE_BACKSPACE }, // evdev 22
	// { "BRK",  RDP_SCANCODE_ },	// evdev 419
	{ "CAPS", RDP_SCANCODE_CAPSLOCK }, // evdev 66
	{ "COMP", RDP_SCANCODE_APPS },     // evdev 135
	// { "COPY", RDP_SCANCODE_ },	// evdev 141
	// { "CUT",  RDP_SCANCODE_ },	// evdev 145
	{ "DELE", RDP_SCANCODE_DELETE }, // evdev 119
	{ "DOWN", RDP_SCANCODE_DOWN },   // evdev 116
	{ "END", RDP_SCANCODE_END },     // evdev 115
	{ "ESC", RDP_SCANCODE_ESCAPE },  // evdev 9
	// { "FIND", RDP_SCANCODE_ },	// evdev 144
	{ "FK01", RDP_SCANCODE_F1 },  // evdev 67
	{ "FK02", RDP_SCANCODE_F2 },  // evdev 68
	{ "FK03", RDP_SCANCODE_F3 },  // evdev 69
	{ "FK04", RDP_SCANCODE_F4 },  // evdev 70
	{ "FK05", RDP_SCANCODE_F5 },  // evdev 71
	{ "FK06", RDP_SCANCODE_F6 },  // evdev 72
	{ "FK07", RDP_SCANCODE_F7 },  // evdev 73
	{ "FK08", RDP_SCANCODE_F8 },  // evdev 74
	{ "FK09", RDP_SCANCODE_F9 },  // evdev 75
	{ "FK10", RDP_SCANCODE_F10 }, // evdev 76
	{ "FK11", RDP_SCANCODE_F11 }, // evdev 95
	{ "FK12", RDP_SCANCODE_F12 }, // evdev 96
	{ "FK13", RDP_SCANCODE_F13 }, // evdev 191
	{ "FK14", RDP_SCANCODE_F14 }, // evdev 192
	{ "FK15", RDP_SCANCODE_F15 }, // evdev 193
	{ "FK16", RDP_SCANCODE_F16 }, // evdev 194
	{ "FK17", RDP_SCANCODE_F17 }, // evdev 195
	{ "FK18", RDP_SCANCODE_F18 }, // evdev 196
	{ "FK19", RDP_SCANCODE_F19 }, // evdev 197
	{ "FK20", RDP_SCANCODE_F20 }, // evdev 198
	{ "FK21", RDP_SCANCODE_F21 }, // evdev 199
	{ "FK22", RDP_SCANCODE_F22 }, // evdev 200
	{ "FK23", RDP_SCANCODE_F23 }, // evdev 201
	{ "FK24", RDP_SCANCODE_F24 }, // evdev 202
	// { "FRNT", RDP_SCANCODE_ },	// evdev 140
	{ "HANJ", RDP_SCANCODE_HANJA },
	{ "HELP", RDP_SCANCODE_HELP },       // evdev 146
	{ "HENK", RDP_SCANCODE_CONVERT_JP }, // JP evdev 100 Henkan
	{ "HIRA", RDP_SCANCODE_HIRAGANA },   // JP evdev  99	Hiragana
	{ "HJCV", RDP_SCANCODE_HANJA },      // KR evdev 131 Hangul->Hanja
	{ "HKTG", RDP_SCANCODE_HIRAGANA },   // JP evdev 101 Hiragana/Katakana toggle
	{ "HNGL", RDP_SCANCODE_HANGUL },     // KR evdev 130 Hangul/Latin toggle
	{ "HOME", RDP_SCANCODE_HOME },       // evdev 110
	{ "HYPR", RDP_SCANCODE_LWIN },       // evdev 207, fake keycode for virtual key
	{ "HZTG", RDP_SCANCODE_OEM_3 },      // JP alias of evdev 49
	// { "I120", RDP_SCANCODE_ },	// evdev 120 KEY_MACRO
	// { "I126", RDP_SCANCODE_ },	// evdev 126 KEY_KPPLUSMINUS
	// { "I128", RDP_SCANCODE_ },	// evdev 128 KEY_SCALE
	{ "I129", RDP_SCANCODE_ABNT_C2 }, // evdev 129 KEY_KPCOMMA Brazil
	// { "I147", RDP_SCANCODE_ },	// evdev 147 KEY_MENU
	// { "I148", RDP_SCANCODE_ },	// evdev 148 KEY_CALC
	// { "I149", RDP_SCANCODE_ },	// evdev 149 KEY_SETUP
	{ "I150", RDP_SCANCODE_SLEEP }, // evdev 150 KEY_SLEEP
	// { "I151", RDP_SCANCODE_ },	// evdev 151 KEY_WAKEUP
	// { "I152", RDP_SCANCODE_ },	// evdev 152 KEY_FILE
	// { "I153", RDP_SCANCODE_ },	// evdev 153 KEY_SENDFILE
	// { "I154", RDP_SCANCODE_ },	// evdev 154 KEY_DELETEFILE
	// { "I155", RDP_SCANCODE_ },	// evdev 155 KEY_XFER
	// { "I156", RDP_SCANCODE_ },	// evdev 156 KEY_PROG1 VK_LAUNCH_APP1
	// { "I157", RDP_SCANCODE_ },	// evdev 157 KEY_PROG2 VK_LAUNCH_APP2
	// { "I158", RDP_SCANCODE_ },	// evdev 158 KEY_WWW
	// { "I159", RDP_SCANCODE_ },	// evdev 159 KEY_MSDOS
	// { "I160", RDP_SCANCODE_ },	// evdev 160 KEY_COFFEE
	// { "I161", RDP_SCANCODE_ },	// evdev 161 KEY_DIRECTION
	// { "I162", RDP_SCANCODE_ },	// evdev 162 KEY_CYCLEWINDOWS
	{ "I163", RDP_SCANCODE_LAUNCH_MAIL },       // evdev 163 KEY_MAIL
	{ "I164", RDP_SCANCODE_BROWSER_FAVORITES }, // evdev 164 KEY_BOOKMARKS
	// { "I165", RDP_SCANCODE_ },	// evdev 165 KEY_COMPUTER
	{ "I166", RDP_SCANCODE_BROWSER_BACK },    // evdev 166 KEY_BACK
	{ "I167", RDP_SCANCODE_BROWSER_FORWARD }, // evdev 167 KEY_FORWARD
	// { "I168", RDP_SCANCODE_ },	// evdev 168 KEY_CLOSECD
	// { "I169", RDP_SCANCODE_ },	// evdev 169 KEY_EJECTCD
	// { "I170", RDP_SCANCODE_ },	// evdev 170 KEY_EJECTCLOSECD
	{ "I171", RDP_SCANCODE_MEDIA_NEXT_TRACK }, // evdev 171 KEY_NEXTSONG
	{ "I172", RDP_SCANCODE_MEDIA_PLAY_PAUSE }, // evdev 172 KEY_PLAYPAUSE
	{ "I173", RDP_SCANCODE_MEDIA_PREV_TRACK }, // evdev 173 KEY_PREVIOUSSONG
	{ "I174", RDP_SCANCODE_MEDIA_STOP },       // evdev 174 KEY_STOPCD
	// { "I175", RDP_SCANCODE_ },	// evdev 175 KEY_RECORD              167
	// { "I176", RDP_SCANCODE_ },	// evdev 176 KEY_REWIND
	// { "I177", RDP_SCANCODE_ },	// evdev 177 KEY_PHONE
	// { "I178", RDP_SCANCODE_ },	// evdev 178 KEY_ISO
	// { "I179", RDP_SCANCODE_ },	// evdev 179 KEY_CONFIG
	{ "I180", RDP_SCANCODE_BROWSER_HOME },    // evdev 180 KEY_HOMEPAGE
	{ "I181", RDP_SCANCODE_BROWSER_REFRESH }, // evdev 181 KEY_REFRESH
	// { "I182", RDP_SCANCODE_ },	// evdev 182 KEY_EXIT
	// { "I183", RDP_SCANCODE_ },	// evdev 183 KEY_MOVE
	// { "I184", RDP_SCANCODE_ },	// evdev 184 KEY_EDIT
	// { "I185", RDP_SCANCODE_ },	// evdev 185 KEY_SCROLLUP
	// { "I186", RDP_SCANCODE_ },	// evdev 186 KEY_SCROLLDOWN
	// { "I187", RDP_SCANCODE_ },	// evdev 187 KEY_KPLEFTPAREN
	// { "I188", RDP_SCANCODE_ },	// evdev 188 KEY_KPRIGHTPAREN
	// { "I189", RDP_SCANCODE_ },	// evdev 189 KEY_NEW
	// { "I190", RDP_SCANCODE_ },	// evdev 190 KEY_REDO
	// { "I208", RDP_SCANCODE_ },	// evdev 208 KEY_PLAYCD
	// { "I209", RDP_SCANCODE_ },	// evdev 209 KEY_PAUSECD
	// { "I210", RDP_SCANCODE_ },	// evdev 210 KEY_PROG3
	// { "I211", RDP_SCANCODE_ },	// evdev 211 KEY_PROG4
	// { "I212", RDP_SCANCODE_ },	// evdev 212 KEY_DASHBOARD
	// { "I213", RDP_SCANCODE_ },	// evdev 213 KEY_SUSPEND
	// { "I214", RDP_SCANCODE_ },	// evdev 214 KEY_CLOSE
	// { "I215", RDP_SCANCODE_ },	// evdev 215 KEY_PLAY
	// { "I216", RDP_SCANCODE_ },	// evdev 216 KEY_FASTFORWARD
	// { "I217", RDP_SCANCODE_ },	// evdev 217 KEY_BASSBOOST
	// { "I218", RDP_SCANCODE_ },	// evdev 218 KEY_PRINT
	// { "I219", RDP_SCANCODE_ },	// evdev 219 KEY_HP
	// { "I220", RDP_SCANCODE_ },	// evdev 220 KEY_CAMERA
	// { "I221", RDP_SCANCODE_ },	// evdev 221 KEY_SOUND
	// { "I222", RDP_SCANCODE_ },	// evdev 222 KEY_QUESTION
	// { "I223", RDP_SCANCODE_ },	// evdev 223 KEY_EMAIL
	// { "I224", RDP_SCANCODE_ },	// evdev 224 KEY_CHAT
	{ "I225", RDP_SCANCODE_BROWSER_SEARCH }, // evdev 225 KEY_SEARCH
	// { "I226", RDP_SCANCODE_ },	// evdev 226 KEY_CONNECT
	// { "I227", RDP_SCANCODE_ },	// evdev 227 KEY_FINANCE
	// { "I228", RDP_SCANCODE_ },	// evdev 228 KEY_SPORT
	// { "I229", RDP_SCANCODE_ },	// evdev 229 KEY_SHOP
	// { "I230", RDP_SCANCODE_ },	// evdev 230 KEY_ALTERASE
	// { "I231", RDP_SCANCODE_ },	// evdev 231 KEY_CANCEL
	// { "I232", RDP_SCANCODE_ },	// evdev 232 KEY_BRIGHTNESSDOWN
	// { "I233", RDP_SCANCODE_ },	// evdev 233 KEY_BRIGHTNESSUP
	// { "I234", RDP_SCANCODE_ },	// evdev 234 KEY_MEDIA
	// { "I235", RDP_SCANCODE_ },	// evdev 235 KEY_SWITCHVIDEOMODE
	// { "I236", RDP_SCANCODE_ },	// evdev 236 KEY_KBDILLUMTOGGLE
	// { "I237", RDP_SCANCODE_ },	// evdev 237 KEY_KBDILLUMDOWN
	// { "I238", RDP_SCANCODE_ },	// evdev 238 KEY_KBDILLUMUP
	// { "I239", RDP_SCANCODE_ },	// evdev 239 KEY_SEND
	// { "I240", RDP_SCANCODE_ },	// evdev 240 KEY_REPLY
	// { "I241", RDP_SCANCODE_ },	// evdev 241 KEY_FORWARDMAIL
	// { "I242", RDP_SCANCODE_ },	// evdev 242 KEY_SAVE
	// { "I243", RDP_SCANCODE_ },	// evdev 243 KEY_DOCUMENTS
	// { "I244", RDP_SCANCODE_ },	// evdev 244 KEY_BATTERY
	// { "I245", RDP_SCANCODE_ },	// evdev 245 KEY_BLUETOOTH
	// { "I246", RDP_SCANCODE_ },	// evdev 246 KEY_WLAN
	// { "I247", RDP_SCANCODE_ },	// evdev 247 KEY_UWB
	// { "I248", RDP_SCANCODE_ },	// evdev 248 KEY_UNKNOWN
	// { "I249", RDP_SCANCODE_ },	// evdev 249 KEY_VIDEO_NEXT
	// { "I250", RDP_SCANCODE_ },	// evdev 250 KEY_VIDEO_PREV
	// { "I251", RDP_SCANCODE_ },	// evdev 251 KEY_BRIGHTNESS_CYCLE
	// { "I252", RDP_SCANCODE_ },	// evdev 252 KEY_BRIGHTNESS_ZERO
	// { "I253", RDP_SCANCODE_ },	// evdev 253 KEY_DISPLAY_OFF
	{ "INS", RDP_SCANCODE_INSERT }, // evdev 118
	// { "JPCM", RDP_SCANCODE_ },	// evdev 103 KPJPComma
	// { "KATA", RDP_SCANCODE_ },	// evdev  98 Katakana VK_DBE_KATAKANA
	{ "KP0", RDP_SCANCODE_NUMPAD0 },    // evdev 90
	{ "KP1", RDP_SCANCODE_NUMPAD1 },    // evdev 87
	{ "KP2", RDP_SCANCODE_NUMPAD2 },    // evdev 88
	{ "KP3", RDP_SCANCODE_NUMPAD3 },    // evdev 89
	{ "KP4", RDP_SCANCODE_NUMPAD4 },    // evdev 83
	{ "KP5", RDP_SCANCODE_NUMPAD5 },    // evdev 84
	{ "KP6", RDP_SCANCODE_NUMPAD6 },    // evdev 85
	{ "KP7", RDP_SCANCODE_NUMPAD7 },    // evdev 79
	{ "KP8", RDP_SCANCODE_NUMPAD8 },    // evdev 80
	{ "KP9", RDP_SCANCODE_NUMPAD9 },    // evdev 81
	{ "KPAD", RDP_SCANCODE_ADD },       // evdev 86
	{ "KPDL", RDP_SCANCODE_DECIMAL },   // evdev 91
	{ "KPDV", RDP_SCANCODE_DIVIDE },    // evdev 106
	{ "KPEN", RDP_SCANCODE_RETURN_KP }, // evdev 104 KP!
	// { "KPEQ", RDP_SCANCODE_ },	// evdev 125
	{ "KPMU", RDP_SCANCODE_MULTIPLY }, // evdev 63
	{ "KPPT", RDP_SCANCODE_ABNT_C2 },  // BR alias of evdev 129
	{ "KPSU", RDP_SCANCODE_SUBTRACT }, // evdev 82
	{ "LALT", RDP_SCANCODE_LMENU },    // evdev 64
	{ "LCTL", RDP_SCANCODE_LCONTROL }, // evdev 37
	{ "LEFT", RDP_SCANCODE_LEFT },     // evdev 113
	{ "LFSH", RDP_SCANCODE_LSHIFT },   // evdev 50
	{ "LMTA", RDP_SCANCODE_LWIN },     // alias of evdev 133 LWIN
	// { "LNFD", RDP_SCANCODE_ },	// evdev 109 KEY_LINEFEED
	{ "LSGT", RDP_SCANCODE_OEM_102 },       // evdev 94
	{ "LVL3", RDP_SCANCODE_RMENU },         // evdev 92, fake keycode for virtual key
	{ "LWIN", RDP_SCANCODE_LWIN },          // evdev 133
	{ "MDSW", RDP_SCANCODE_RMENU },         // evdev 203, fake keycode for virtual key
	{ "MENU", RDP_SCANCODE_APPS },          // alias of evdev 135 COMP
	{ "META", RDP_SCANCODE_LMENU },         // evdev 205, fake keycode for virtual key
	{ "MUHE", RDP_SCANCODE_NONCONVERT_JP }, // JP evdev 102 Muhenkan
	{ "MUTE", RDP_SCANCODE_VOLUME_MUTE },   // evdev 121
	{ "NFER", RDP_SCANCODE_NONCONVERT_JP }, // JP alias of evdev 102 Muhenkan
	{ "NMLK", RDP_SCANCODE_NUMLOCK },       // evdev 77
	// { "OPEN", RDP_SCANCODE_ },	// evdev 142
	// { "PAST", RDP_SCANCODE_ },	// evdev 143
	{ "PAUS", RDP_SCANCODE_PAUSE }, // evdev 127
	{ "PGDN", RDP_SCANCODE_NEXT },  // evdev 117
	{ "PGUP", RDP_SCANCODE_PRIOR }, // evdev 112
	// { "POWR", RDP_SCANCODE_ },	// evdev 124
	// { "PROP", RDP_SCANCODE_ },	// evdev 138
	{ "PRSC", RDP_SCANCODE_PRINTSCREEN }, // evdev 107
	{ "RALT", RDP_SCANCODE_RMENU },       // evdev 108 RALT
	{ "RCTL", RDP_SCANCODE_RCONTROL },    // evdev 105
	{ "RGHT", RDP_SCANCODE_RIGHT },       // evdev 114
	{ "RMTA", RDP_SCANCODE_RWIN },        // alias of evdev 134 RWIN
	// { "RO",   RDP_SCANCODE_ },	// JP evdev  97	Romaji
	{ "RTRN", RDP_SCANCODE_RETURN },       // not KP, evdev 36
	{ "RTSH", RDP_SCANCODE_RSHIFT },       // evdev 62
	{ "RWIN", RDP_SCANCODE_RWIN },         // evdev 134
	{ "SCLK", RDP_SCANCODE_SCROLLLOCK },   // evdev 78
	{ "SPCE", RDP_SCANCODE_SPACE },        // evdev 65
	{ "STOP", RDP_SCANCODE_BROWSER_STOP }, // evdev 136
	{ "SUPR", RDP_SCANCODE_LWIN },         // evdev 206, fake keycode for virtual key
	{ "SYRQ", RDP_SCANCODE_SYSREQ },       // evdev 107
	{ "TAB", RDP_SCANCODE_TAB },           // evdev 23
	{ "TLDE", RDP_SCANCODE_OEM_3 },        // evdev 49
	// { "UNDO", RDP_SCANCODE_ },	// evdev 139
	{ "UP", RDP_SCANCODE_UP },            // evdev 111
	{ "VOL-", RDP_SCANCODE_VOLUME_DOWN }, // evdev 122
	{ "VOL+", RDP_SCANCODE_VOLUME_UP },   // evdev 123
	{ "XFER", RDP_SCANCODE_CONVERT_JP },  // JP alias of evdev 100 Henkan
};

static int detect_keyboard_layout_from_xkbfile(void* display, DWORD* keyboardLayoutId);
static int freerdp_keyboard_load_map_from_xkbfile(void* display,
                                                  DWORD x11_keycode_to_rdp_scancode[256]);

static void* freerdp_keyboard_xkb_init(void)
{
	int status;

	Display* display = XOpenDisplay(NULL);

	if (!display)
		return NULL;

	status = XkbQueryExtension(display, NULL, NULL, NULL, NULL, NULL);

	if (!status)
		return NULL;

	return (void*)display;
}

int freerdp_keyboard_init_xkbfile(DWORD* keyboardLayoutId, DWORD x11_keycode_to_rdp_scancode[256])
{
	void* display;

	WINPR_ASSERT(keyboardLayoutId);
	WINPR_ASSERT(x11_keycode_to_rdp_scancode);
	ZeroMemory(x11_keycode_to_rdp_scancode, sizeof(DWORD) * 256);

	display = freerdp_keyboard_xkb_init();

	if (!display)
	{
		DEBUG_KBD("Error initializing xkb");
		return -1;
	}

	if (*keyboardLayoutId == 0)
	{
		detect_keyboard_layout_from_xkbfile(display, keyboardLayoutId);
		DEBUG_KBD("detect_keyboard_layout_from_xkb: %" PRIu32 " (0x%08X" PRIX32 ")",
		          *keyboardLayoutId, *keyboardLayoutId);
	}

	freerdp_keyboard_load_map_from_xkbfile(display, x11_keycode_to_rdp_scancode);

	XCloseDisplay(display);

	return 0;
}

/* return substring starting after nth comma, ending at following comma */
static char* comma_substring(char* s, int n)
{
	char* p;

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

int detect_keyboard_layout_from_xkbfile(void* display, DWORD* keyboardLayoutId)
{
	char* layout = NULL;
	char* variant = NULL;
	DWORD group = 0;
	XkbStateRec state = { 0 };
	XKeyboardState coreKbdState;
	XkbRF_VarDefsRec rules_names = { 0 };

	DEBUG_KBD("display: %p", display);

	if (display && XkbRF_GetNamesProp(display, NULL, &rules_names))
	{
		DEBUG_KBD("layouts: %s", rules_names.layout ? rules_names.layout : "");
		DEBUG_KBD("variants: %s", rules_names.variant ? rules_names.variant : "");

		XGetKeyboardControl(display, &coreKbdState);

		if (XkbGetState(display, XkbUseCoreKbd, &state) == Success)
			group = state.group;

		DEBUG_KBD("group: %u", state.group);

		layout = comma_substring(rules_names.layout, group);
		variant = comma_substring(rules_names.variant, group);

		DEBUG_KBD("layout: %s", layout ? layout : "");
		DEBUG_KBD("variant: %s", variant ? variant : "");

		*keyboardLayoutId = find_keyboard_layout_in_xorg_rules(layout, variant);

		free(rules_names.model);
		free(rules_names.layout);
		free(rules_names.variant);
		free(rules_names.options);
	}

	return 0;
}

int freerdp_keyboard_load_map_from_xkbfile(void* display, DWORD x11_keycode_to_rdp_scancode[256])
{
	size_t i, j;
	BOOL found;
	XkbDescPtr xkb;
	BOOL status = FALSE;

	if (display && (xkb = XkbGetMap(display, 0, XkbUseCoreKbd)))
	{
		if (XkbGetNames(display, XkbKeyNamesMask, xkb) == Success)
		{
			char xkb_keyname[5] = { 42, 42, 42, 42, 0 }; /* end-of-string at index 5 */

			for (i = xkb->min_key_code; i <= xkb->max_key_code; i++)
			{
				found = FALSE;
				CopyMemory(xkb_keyname, xkb->names->keys[i].name, 4);

				if (strnlen(xkb_keyname, sizeof(xkb_keyname)) < 1)
					continue;

				for (j = 0; j < ARRAYSIZE(XKB_KEY_NAME_SCANCODE_TABLE); j++)
				{
					if (!strcmp(xkb_keyname, XKB_KEY_NAME_SCANCODE_TABLE[j].xkb_keyname))
					{
						DEBUG_KBD("%4s: keycode: 0x%02X -> rdp scancode: 0x%08" PRIX32 "",
						          xkb_keyname, i, XKB_KEY_NAME_SCANCODE_TABLE[j].rdp_scancode);

						if (found)
						{
							DEBUG_KBD("Internal error! duplicate key %s!", xkb_keyname);
						}

						x11_keycode_to_rdp_scancode[i] =
						    XKB_KEY_NAME_SCANCODE_TABLE[j].rdp_scancode;
						found = TRUE;
					}
				}

				if (!found)
				{
					DEBUG_KBD("%4s: keycode: 0x%02X -> no RDP scancode found", xkb_keyname, i);
				}
			}

			status = TRUE;
		}

		XkbFreeKeyboard(xkb, 0, 1);
	}

	return status;
}
