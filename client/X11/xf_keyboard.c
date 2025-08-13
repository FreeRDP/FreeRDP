/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Keyboard Handling
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/assert.h>
#include <winpr/cast.h>
#include <winpr/collections.h>

#include <freerdp/utils/string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>

#include <freerdp/locale/keyboard.h>
#include <freerdp/locale/locale.h>

#include "xf_event.h"

#include "xf_keyboard.h"

#include "xf_utils.h"
#include "keyboard_x11.h"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("x11")

typedef struct
{
	BOOL Shift;
	BOOL LeftShift;
	BOOL RightShift;
	BOOL Alt;
	BOOL LeftAlt;
	BOOL RightAlt;
	BOOL Ctrl;
	BOOL LeftCtrl;
	BOOL RightCtrl;
	BOOL Super;
	BOOL LeftSuper;
	BOOL RightSuper;
} XF_MODIFIER_KEYS;

struct x11_key_scancode_t
{
	const char* name;
	DWORD sc;
};

static const struct x11_key_scancode_t XKB_KEY_NAME_SCANCODE_TABLE[] = {
	{ "", RDP_SCANCODE_UNKNOWN },                 /* 008:  [(null)] */
	{ "ESC", RDP_SCANCODE_ESCAPE },               /* 009: ESC [Escape] */
	{ "AE01", RDP_SCANCODE_KEY_1 },               /* 010: AE01 [1] */
	{ "AE02", RDP_SCANCODE_KEY_2 },               /* 011: AE02 [2] */
	{ "AE03", RDP_SCANCODE_KEY_3 },               /* 012: AE03 [3] */
	{ "AE04", RDP_SCANCODE_KEY_4 },               /* 013: AE04 [4] */
	{ "AE05", RDP_SCANCODE_KEY_5 },               /* 014: AE05 [5] */
	{ "AE06", RDP_SCANCODE_KEY_6 },               /* 015: AE06 [6] */
	{ "AE07", RDP_SCANCODE_KEY_7 },               /* 016: AE07 [7] */
	{ "AE08", RDP_SCANCODE_KEY_8 },               /* 017: AE08 [8] */
	{ "AE09", RDP_SCANCODE_KEY_9 },               /* 018: AE09 [9] */
	{ "AE10", RDP_SCANCODE_KEY_0 },               /* 019: AE10 [0] */
	{ "AE11", RDP_SCANCODE_OEM_MINUS },           /* 020: AE11 [minus] */
	{ "AE12", RDP_SCANCODE_OEM_PLUS },            /* 021: AE12 [equal] */
	{ "BKSP", RDP_SCANCODE_BACKSPACE },           /* 022: BKSP [BackSpace] */
	{ "TAB", RDP_SCANCODE_TAB },                  /* 023: TAB [Tab] */
	{ "AD01", RDP_SCANCODE_KEY_Q },               /* 024: AD01 [q] */
	{ "AD02", RDP_SCANCODE_KEY_W },               /* 025: AD02 [w] */
	{ "AD03", RDP_SCANCODE_KEY_E },               /* 026: AD03 [e] */
	{ "AD04", RDP_SCANCODE_KEY_R },               /* 027: AD04 [r] */
	{ "AD05", RDP_SCANCODE_KEY_T },               /* 028: AD05 [t] */
	{ "AD06", RDP_SCANCODE_KEY_Y },               /* 029: AD06 [y] */
	{ "AD07", RDP_SCANCODE_KEY_U },               /* 030: AD07 [u] */
	{ "AD08", RDP_SCANCODE_KEY_I },               /* 031: AD08 [i] */
	{ "AD09", RDP_SCANCODE_KEY_O },               /* 032: AD09 [o] */
	{ "AD10", RDP_SCANCODE_KEY_P },               /* 033: AD10 [p] */
	{ "AD11", RDP_SCANCODE_OEM_4 },               /* 034: AD11 [bracketleft] */
	{ "AD12", RDP_SCANCODE_OEM_6 },               /* 035: AD12 [bracketright] */
	{ "RTRN", RDP_SCANCODE_RETURN },              /* 036: RTRN [Return] */
	{ "LCTL", RDP_SCANCODE_LCONTROL },            /* 037: LCTL [Control_L] */
	{ "AC01", RDP_SCANCODE_KEY_A },               /* 038: AC01 [a] */
	{ "AC02", RDP_SCANCODE_KEY_S },               /* 039: AC02 [s] */
	{ "AC03", RDP_SCANCODE_KEY_D },               /* 040: AC03 [d] */
	{ "AC04", RDP_SCANCODE_KEY_F },               /* 041: AC04 [f] */
	{ "AC05", RDP_SCANCODE_KEY_G },               /* 042: AC05 [g] */
	{ "AC06", RDP_SCANCODE_KEY_H },               /* 043: AC06 [h] */
	{ "AC07", RDP_SCANCODE_KEY_J },               /* 044: AC07 [j] */
	{ "AC08", RDP_SCANCODE_KEY_K },               /* 045: AC08 [k] */
	{ "AC09", RDP_SCANCODE_KEY_L },               /* 046: AC09 [l] */
	{ "AC10", RDP_SCANCODE_OEM_1 },               /* 047: AC10 [semicolon] */
	{ "AC11", RDP_SCANCODE_OEM_7 },               /* 048: AC11 [dead_acute] */
	{ "TLDE", RDP_SCANCODE_OEM_3 },               /* 049: TLDE [dead_grave] */
	{ "LFSH", RDP_SCANCODE_LSHIFT },              /* 050: LFSH [Shift_L] */
	{ "BKSL", RDP_SCANCODE_OEM_5 },               /* 051: BKSL [backslash] */
	{ "AB01", RDP_SCANCODE_KEY_Z },               /* 052: AB01 [z] */
	{ "AB02", RDP_SCANCODE_KEY_X },               /* 053: AB02 [x] */
	{ "AB03", RDP_SCANCODE_KEY_C },               /* 054: AB03 [c] */
	{ "AB04", RDP_SCANCODE_KEY_V },               /* 055: AB04 [v] */
	{ "AB05", RDP_SCANCODE_KEY_B },               /* 056: AB05 [b] */
	{ "AB06", RDP_SCANCODE_KEY_N },               /* 057: AB06 [n] */
	{ "AB07", RDP_SCANCODE_KEY_M },               /* 058: AB07 [m] */
	{ "AB08", RDP_SCANCODE_OEM_COMMA },           /* 059: AB08 [comma] */
	{ "AB09", RDP_SCANCODE_OEM_PERIOD },          /* 060: AB09 [period] */
	{ "AB10", RDP_SCANCODE_OEM_2 },               /* 061: AB10 [slash] */
	{ "RTSH", RDP_SCANCODE_RSHIFT },              /* 062: RTSH [Shift_R] */
	{ "KPMU", RDP_SCANCODE_MULTIPLY },            /* 063: KPMU [KP_Multiply] */
	{ "LALT", RDP_SCANCODE_LMENU },               /* 064: LALT [Alt_L] */
	{ "SPCE", RDP_SCANCODE_SPACE },               /* 065: SPCE [space] */
	{ "CAPS", RDP_SCANCODE_CAPSLOCK },            /* 066: CAPS [Caps_Lock] */
	{ "FK01", RDP_SCANCODE_F1 },                  /* 067: FK01 [F1] */
	{ "FK02", RDP_SCANCODE_F2 },                  /* 068: FK02 [F2] */
	{ "FK03", RDP_SCANCODE_F3 },                  /* 069: FK03 [F3] */
	{ "FK04", RDP_SCANCODE_F4 },                  /* 070: FK04 [F4] */
	{ "FK05", RDP_SCANCODE_F5 },                  /* 071: FK05 [F5] */
	{ "FK06", RDP_SCANCODE_F6 },                  /* 072: FK06 [F6] */
	{ "FK07", RDP_SCANCODE_F7 },                  /* 073: FK07 [F7] */
	{ "FK08", RDP_SCANCODE_F8 },                  /* 074: FK08 [F8] */
	{ "FK09", RDP_SCANCODE_F9 },                  /* 075: FK09 [F9] */
	{ "FK10", RDP_SCANCODE_F10 },                 /* 076: FK10 [F10] */
	{ "NMLK", RDP_SCANCODE_NUMLOCK },             /* 077: NMLK [Num_Lock] */
	{ "SCLK", RDP_SCANCODE_SCROLLLOCK },          /* 078: SCLK [Multi_key] */
	{ "KP7", RDP_SCANCODE_NUMPAD7 },              /* 079: KP7 [KP_Home] */
	{ "KP8", RDP_SCANCODE_NUMPAD8 },              /* 080: KP8 [KP_Up] */
	{ "KP9", RDP_SCANCODE_NUMPAD9 },              /* 081: KP9 [KP_Prior] */
	{ "KPSU", RDP_SCANCODE_SUBTRACT },            /* 082: KPSU [KP_Subtract] */
	{ "KP4", RDP_SCANCODE_NUMPAD4 },              /* 083: KP4 [KP_Left] */
	{ "KP5", RDP_SCANCODE_NUMPAD5 },              /* 084: KP5 [KP_Begin] */
	{ "KP6", RDP_SCANCODE_NUMPAD6 },              /* 085: KP6 [KP_Right] */
	{ "KPAD", RDP_SCANCODE_ADD },                 /* 086: KPAD [KP_Add] */
	{ "KP1", RDP_SCANCODE_NUMPAD1 },              /* 087: KP1 [KP_End] */
	{ "KP2", RDP_SCANCODE_NUMPAD2 },              /* 088: KP2 [KP_Down] */
	{ "KP3", RDP_SCANCODE_NUMPAD3 },              /* 089: KP3 [KP_Next] */
	{ "KP0", RDP_SCANCODE_NUMPAD0 },              /* 090: KP0 [KP_Insert] */
	{ "KPDL", RDP_SCANCODE_DECIMAL },             /* 091: KPDL [KP_Delete] */
	{ "LVL3", RDP_SCANCODE_RMENU },               /* 092: LVL3 [ISO_Level3_Shift] */
	{ "", RDP_SCANCODE_UNKNOWN },                 /* 093:  [(null)] */
	{ "LSGT", RDP_SCANCODE_OEM_102 },             /* 094: LSGT [backslash] */
	{ "FK11", RDP_SCANCODE_F11 },                 /* 095: FK11 [F11] */
	{ "FK12", RDP_SCANCODE_F12 },                 /* 096: FK12 [F12] */
	{ "AB11", RDP_SCANCODE_ABNT_C1 },             /* 097: AB11 [(null)] */
	{ "KATA", RDP_SCANCODE_KANA_HANGUL },         /* 098: KATA [Katakana] */
	{ "HIRA", RDP_SCANCODE_HIRAGANA },            /* 099: HIRA [Hiragana] */
	{ "HENK", RDP_SCANCODE_CONVERT_JP },          /* 100: HENK [Henkan_Mode] */
	{ "HKTG", RDP_SCANCODE_HIRAGANA },            /* 101: HKTG [Hiragana_Katakana] */
	{ "MUHE", RDP_SCANCODE_NONCONVERT_JP },       /* 102: MUHE [Muhenkan] */
	{ "JPCM", RDP_SCANCODE_UNKNOWN },             /* 103: JPCM [(null)] */
	{ "KPEN", RDP_SCANCODE_RETURN_KP },           /* 104: KPEN [KP_Enter] */
	{ "RCTL", RDP_SCANCODE_RCONTROL },            /* 105: RCTL [Control_R] */
	{ "KPDV", RDP_SCANCODE_DIVIDE },              /* 106: KPDV [KP_Divide] */
	{ "PRSC", RDP_SCANCODE_PRINTSCREEN },         /* 107: PRSC [Print] */
	{ "RALT", RDP_SCANCODE_RMENU },               /* 108: RALT [ISO_Level3_Shift] */
	{ "LNFD", RDP_SCANCODE_UNKNOWN },             /* 109: LNFD [Linefeed] */
	{ "HOME", RDP_SCANCODE_HOME },                /* 110: HOME [Home] */
	{ "UP", RDP_SCANCODE_UP },                    /* 111: UP [Up] */
	{ "PGUP", RDP_SCANCODE_PRIOR },               /* 112: PGUP [Prior] */
	{ "LEFT", RDP_SCANCODE_LEFT },                /* 113: LEFT [Left] */
	{ "RGHT", RDP_SCANCODE_RIGHT },               /* 114: RGHT [Right] */
	{ "END", RDP_SCANCODE_END },                  /* 115: END [End] */
	{ "DOWN", RDP_SCANCODE_DOWN },                /* 116: DOWN [Down] */
	{ "PGDN", RDP_SCANCODE_NEXT },                /* 117: PGDN [Next] */
	{ "INS", RDP_SCANCODE_INSERT },               /* 118: INS [Insert] */
	{ "DELE", RDP_SCANCODE_DELETE },              /* 119: DELE [Delete] */
	{ "I120", RDP_SCANCODE_UNKNOWN },             /* 120: I120 [(null)] */
	{ "MUTE", RDP_SCANCODE_VOLUME_MUTE },         /* 121: MUTE [XF86AudioMute] */
	{ "VOL-", RDP_SCANCODE_VOLUME_DOWN },         /* 122: VOL- [XF86AudioLowerVolume] */
	{ "VOL+", RDP_SCANCODE_VOLUME_UP },           /* 123: VOL+ [XF86AudioRaiseVolume] */
	{ "POWR", RDP_SCANCODE_UNKNOWN },             /* 124: POWR [XF86PowerOff] */
	{ "KPEQ", RDP_SCANCODE_UNKNOWN },             /* 125: KPEQ [KP_Equal] */
	{ "I126", RDP_SCANCODE_UNKNOWN },             /* 126: I126 [plusminus] */
	{ "PAUS", RDP_SCANCODE_PAUSE },               /* 127: PAUS [Pause] */
	{ "I128", RDP_SCANCODE_LAUNCH_MEDIA_SELECT }, /* 128: I128 [XF86LaunchA] */
	{ "I129", RDP_SCANCODE_ABNT_C2 },             /* 129: I129 [KP_Decimal] */
	{ "HNGL", RDP_SCANCODE_HANGUL },              /* 130: HNGL [Hangul] */
	{ "HJCV", RDP_SCANCODE_HANJA },               /* 131: HJCV [Hangul_Hanja] */
	{ "AE13", RDP_SCANCODE_BACKSLASH_JP },        /* 132: AE13 [(null)] */
	{ "LWIN", RDP_SCANCODE_LWIN },                /* 133: LWIN [Super_L] */
	{ "RWIN", RDP_SCANCODE_RWIN },                /* 134: RWIN [Super_R] */
	{ "COMP", RDP_SCANCODE_APPS },                /* 135: COMP [Menu] */
	{ "STOP", RDP_SCANCODE_BROWSER_STOP },        /* 136: STOP [Cancel] */
	{ "AGAI", RDP_SCANCODE_UNKNOWN },             /* 137: AGAI [Redo] */
	{ "PROP", RDP_SCANCODE_UNKNOWN },             /* 138: PROP [SunProps] */
	{ "UNDO", RDP_SCANCODE_UNKNOWN },             /* 139: UNDO [Undo] */
	{ "FRNT", RDP_SCANCODE_UNKNOWN },             /* 140: FRNT [SunFront] */
	{ "COPY", RDP_SCANCODE_UNKNOWN },             /* 141: COPY [XF86Copy] */
	{ "OPEN", RDP_SCANCODE_UNKNOWN },             /* 142: OPEN [XF86Open] */
	{ "PAST", RDP_SCANCODE_UNKNOWN },             /* 143: PAST [XF86Paste] */
	{ "FIND", RDP_SCANCODE_UNKNOWN },             /* 144: FIND [Find] */
	{ "CUT", RDP_SCANCODE_UNKNOWN },              /* 145: CUT [XF86Cut] */
	{ "HELP", RDP_SCANCODE_HELP },                /* 146: HELP [Help] */
	{ "I147", RDP_SCANCODE_UNKNOWN },             /* 147: I147 [XF86MenuKB] */
	{ "I148", RDP_SCANCODE_UNKNOWN },             /* 148: I148 [XF86Calculator] */
	{ "I149", RDP_SCANCODE_UNKNOWN },             /* 149: I149 [(null)] */
	{ "I150", RDP_SCANCODE_SLEEP },               /* 150: I150 [XF86Sleep] */
	{ "I151", RDP_SCANCODE_UNKNOWN },             /* 151: I151 [XF86WakeUp] */
	{ "I152", RDP_SCANCODE_UNKNOWN },             /* 152: I152 [XF86Explorer] */
	{ "I153", RDP_SCANCODE_UNKNOWN },             /* 153: I153 [XF86Send] */
	{ "I154", RDP_SCANCODE_UNKNOWN },             /* 154: I154 [(null)] */
	{ "I155", RDP_SCANCODE_UNKNOWN },             /* 155: I155 [XF86Xfer] */
	{ "I156", RDP_SCANCODE_LAUNCH_APP1 },         /* 156: I156 [XF86Launch1] */
	{ "I157", RDP_SCANCODE_LAUNCH_APP2 },         /* 157: I157 [XF86Launch2] */
	{ "I158", RDP_SCANCODE_BROWSER_HOME },        /* 158: I158 [XF86WWW] */
	{ "I159", RDP_SCANCODE_UNKNOWN },             /* 159: I159 [XF86DOS] */
	{ "I160", RDP_SCANCODE_UNKNOWN },             /* 160: I160 [XF86ScreenSaver] */
	{ "I161", RDP_SCANCODE_UNKNOWN },             /* 161: I161 [XF86RotateWindows] */
	{ "I162", RDP_SCANCODE_UNKNOWN },             /* 162: I162 [XF86TaskPane] */
	{ "I163", RDP_SCANCODE_LAUNCH_MAIL },         /* 163: I163 [XF86Mail] */
	{ "I164", RDP_SCANCODE_BROWSER_FAVORITES },   /* 164: I164 [XF86Favorites] */
	{ "I165", RDP_SCANCODE_UNKNOWN },             /* 165: I165 [XF86MyComputer] */
	{ "I166", RDP_SCANCODE_BROWSER_BACK },        /* 166: I166 [XF86Back] */
	{ "I167", RDP_SCANCODE_BROWSER_FORWARD },     /* 167: I167 [XF86Forward] */
	{ "I168", RDP_SCANCODE_UNKNOWN },             /* 168: I168 [(null)] */
	{ "I169", RDP_SCANCODE_UNKNOWN },             /* 169: I169 [XF86Eject] */
	{ "I170", RDP_SCANCODE_UNKNOWN },             /* 170: I170 [XF86Eject] */
	{ "I171", RDP_SCANCODE_MEDIA_NEXT_TRACK },    /* 171: I171 [XF86AudioNext] */
	{ "I172", RDP_SCANCODE_MEDIA_PLAY_PAUSE },    /* 172: I172 [XF86AudioPlay] */
	{ "I173", RDP_SCANCODE_MEDIA_PREV_TRACK },    /* 173: I173 [XF86AudioPrev] */
	{ "I174", RDP_SCANCODE_MEDIA_STOP },          /* 174: I174 [XF86AudioStop] */
	{ "I175", RDP_SCANCODE_UNKNOWN },             /* 175: I175 [XF86AudioRecord] */
	{ "I176", RDP_SCANCODE_UNKNOWN },             /* 176: I176 [XF86AudioRewind] */
	{ "I177", RDP_SCANCODE_UNKNOWN },             /* 177: I177 [XF86Phone] */
	{ "I178", RDP_SCANCODE_UNKNOWN },             /* 178: I178 [(null)] */
	{ "I179", RDP_SCANCODE_UNKNOWN },             /* 179: I179 [XF86Tools] */
	{ "I180", RDP_SCANCODE_BROWSER_HOME },        /* 180: I180 [XF86HomePage] */
	{ "I181", RDP_SCANCODE_BROWSER_REFRESH },     /* 181: I181 [XF86Reload] */
	{ "I182", RDP_SCANCODE_UNKNOWN },             /* 182: I182 [XF86Close] */
	{ "I183", RDP_SCANCODE_UNKNOWN },             /* 183: I183 [(null)] */
	{ "I184", RDP_SCANCODE_UNKNOWN },             /* 184: I184 [(null)] */
	{ "I185", RDP_SCANCODE_UNKNOWN },             /* 185: I185 [XF86ScrollUp] */
	{ "I186", RDP_SCANCODE_UNKNOWN },             /* 186: I186 [XF86ScrollDown] */
	{ "I187", RDP_SCANCODE_UNKNOWN },             /* 187: I187 [parenleft] */
	{ "I188", RDP_SCANCODE_UNKNOWN },             /* 188: I188 [parenright] */
	{ "I189", RDP_SCANCODE_UNKNOWN },             /* 189: I189 [XF86New] */
	{ "I190", RDP_SCANCODE_UNKNOWN },             /* 190: I190 [Redo] */
	{ "FK13", RDP_SCANCODE_F13 },                 /* 191: FK13 [XF86Tools] */
	{ "FK14", RDP_SCANCODE_F14 },                 /* 192: FK14 [XF86Launch5] */
	{ "FK15", RDP_SCANCODE_F15 },                 /* 193: FK15 [XF86Launch6] */
	{ "FK16", RDP_SCANCODE_F16 },                 /* 194: FK16 [XF86Launch7] */
	{ "FK17", RDP_SCANCODE_F17 },                 /* 195: FK17 [XF86Launch8] */
	{ "FK18", RDP_SCANCODE_F18 },                 /* 196: FK18 [XF86Launch9] */
	{ "FK19", RDP_SCANCODE_F19 },                 /* 197: FK19 [(null)] */
	{ "FK20", RDP_SCANCODE_F20 },                 /* 198: FK20 [XF86AudioMicMute] */
	{ "FK21", RDP_SCANCODE_F21 },                 /* 199: FK21 [XF86TouchpadToggle] */
	{ "FK22", RDP_SCANCODE_F22 },                 /* 200: FK22 [XF86TouchpadOn] */
	{ "FK23", RDP_SCANCODE_F23 },                 /* 201: FK23 [XF86TouchpadOff] */
	{ "FK24", RDP_SCANCODE_F24 },                 /* 202: FK24 [(null)] */
	{ "LVL5", RDP_SCANCODE_UNKNOWN },             /* 203: LVL5 [ISO_Level5_Shift] */
	{ "ALT", RDP_SCANCODE_LMENU },                /* 204: ALT [(null)] */
	{ "META", RDP_SCANCODE_LMENU },               /* 205: META [(null)] */
	{ "SUPR", RDP_SCANCODE_LWIN },                /* 206: SUPR [(null)] */
	{ "HYPR", RDP_SCANCODE_LWIN },                /* 207: HYPR [(null)] */
	{ "I208", RDP_SCANCODE_MEDIA_PLAY_PAUSE },    /* 208: I208 [XF86AudioPlay] */
	{ "I209", RDP_SCANCODE_MEDIA_PLAY_PAUSE },    /* 209: I209 [XF86AudioPause] */
	{ "I210", RDP_SCANCODE_UNKNOWN },             /* 210: I210 [XF86Launch3] */
	{ "I211", RDP_SCANCODE_UNKNOWN },             /* 211: I211 [XF86Launch4] */
	{ "I212", RDP_SCANCODE_UNKNOWN },             /* 212: I212 [XF86LaunchB] */
	{ "I213", RDP_SCANCODE_UNKNOWN },             /* 213: I213 [XF86Suspend] */
	{ "I214", RDP_SCANCODE_UNKNOWN },             /* 214: I214 [XF86Close] */
	{ "I215", RDP_SCANCODE_MEDIA_PLAY_PAUSE },    /* 215: I215 [XF86AudioPlay] */
	{ "I216", RDP_SCANCODE_MEDIA_NEXT_TRACK },    /* 216: I216 [XF86AudioForward] */
	{ "I217", RDP_SCANCODE_UNKNOWN },             /* 217: I217 [(null)] */
	{ "I218", RDP_SCANCODE_UNKNOWN },             /* 218: I218 [Print] */
	{ "I219", RDP_SCANCODE_UNKNOWN },             /* 219: I219 [(null)] */
	{ "I220", RDP_SCANCODE_UNKNOWN },             /* 220: I220 [XF86WebCam] */
	{ "I221", RDP_SCANCODE_UNKNOWN },             /* 221: I221 [XF86AudioPreset] */
	{ "I222", RDP_SCANCODE_UNKNOWN },             /* 222: I222 [(null)] */
	{ "I223", RDP_SCANCODE_LAUNCH_MAIL },         /* 223: I223 [XF86Mail] */
	{ "I224", RDP_SCANCODE_UNKNOWN },             /* 224: I224 [XF86Messenger] */
	{ "I225", RDP_SCANCODE_BROWSER_SEARCH },      /* 225: I225 [XF86Search] */
	{ "I226", RDP_SCANCODE_UNKNOWN },             /* 226: I226 [XF86Go] */
	{ "I227", RDP_SCANCODE_UNKNOWN },             /* 227: I227 [XF86Finance] */
	{ "I228", RDP_SCANCODE_UNKNOWN },             /* 228: I228 [XF86Game] */
	{ "I229", RDP_SCANCODE_UNKNOWN },             /* 229: I229 [XF86Shop] */
	{ "I230", RDP_SCANCODE_UNKNOWN },             /* 230: I230 [(null)] */
	{ "I231", RDP_SCANCODE_UNKNOWN },             /* 231: I231 [Cancel] */
	{ "I232", RDP_SCANCODE_UNKNOWN },             /* 232: I232 [XF86MonBrightnessDown] */
	{ "I233", RDP_SCANCODE_UNKNOWN },             /* 233: I233 [XF86MonBrightnessUp] */
	{ "I234", RDP_SCANCODE_LAUNCH_MEDIA_SELECT }, /* 234: I234 [XF86AudioMedia] */
	{ "I235", RDP_SCANCODE_UNKNOWN },             /* 235: I235 [XF86Display] */
	{ "I236", RDP_SCANCODE_UNKNOWN },             /* 236: I236 [XF86KbdLightOnOff] */
	{ "I237", RDP_SCANCODE_UNKNOWN },             /* 237: I237 [XF86KbdBrightnessDown] */
	{ "I238", RDP_SCANCODE_UNKNOWN },             /* 238: I238 [XF86KbdBrightnessUp] */
	{ "I239", RDP_SCANCODE_UNKNOWN },             /* 239: I239 [XF86Send] */
	{ "I240", RDP_SCANCODE_UNKNOWN },             /* 240: I240 [XF86Reply] */
	{ "I241", RDP_SCANCODE_UNKNOWN },             /* 241: I241 [XF86MailForward] */
	{ "I242", RDP_SCANCODE_UNKNOWN },             /* 242: I242 [XF86Save] */
	{ "I243", RDP_SCANCODE_UNKNOWN },             /* 243: I243 [XF86Documents] */
	{ "I244", RDP_SCANCODE_UNKNOWN },             /* 244: I244 [XF86Battery] */
	{ "I245", RDP_SCANCODE_UNKNOWN },             /* 245: I245 [XF86Bluetooth] */
	{ "I246", RDP_SCANCODE_UNKNOWN },             /* 246: I246 [XF86WLAN] */
	{ "I247", RDP_SCANCODE_UNKNOWN },             /* 247: I247 [XF86UWB] */
	{ "I248", RDP_SCANCODE_UNKNOWN },             /* 248: I248 [(null)] */
	{ "I249", RDP_SCANCODE_UNKNOWN },             /* 249: I249 [XF86Next_VMode] */
	{ "I250", RDP_SCANCODE_UNKNOWN },             /* 250: I250 [XF86Prev_VMode] */
	{ "I251", RDP_SCANCODE_UNKNOWN },             /* 251: I251 [XF86MonBrightnessCycle] */
	{ "I252", RDP_SCANCODE_UNKNOWN },             /* 252: I252 [XF86BrightnessAuto] */
	{ "I253", RDP_SCANCODE_UNKNOWN },             /* 253: I253 [XF86DisplayOff] */
	{ "I254", RDP_SCANCODE_UNKNOWN },             /* 254: I254 [XF86WWAN] */
	{ "I255", RDP_SCANCODE_UNKNOWN }              /* 255: I255 [XF86RFKill] */
};

static UINT32 xf_keyboard_get_toggle_keys_state(xfContext* xfc);
static BOOL xf_keyboard_handle_special_keys(xfContext* xfc, KeySym keysym);
static void xf_keyboard_handle_special_keys_release(xfContext* xfc, KeySym keysym);

static void xf_keyboard_modifier_map_free(xfContext* xfc)
{
	WINPR_ASSERT(xfc);
	if (xfc->modifierMap)
	{
		XFreeModifiermap(xfc->modifierMap);
		xfc->modifierMap = NULL;
	}
}

BOOL xf_keyboard_update_modifier_map(xfContext* xfc)
{
	WINPR_ASSERT(xfc);
	xf_keyboard_modifier_map_free(xfc);
	xfc->modifierMap = XGetModifierMapping(xfc->display);
	return xfc->modifierMap != NULL;
}

static void xf_keyboard_send_key(xfContext* xfc, BOOL down, BOOL repeat, const XKeyEvent* ev);

static BOOL xf_sync_kbd_state(xfContext* xfc)
{
	const UINT32 syncFlags = xf_keyboard_get_toggle_keys_state(xfc);

	WINPR_ASSERT(xfc);
	return freerdp_input_send_synchronize_event(xfc->common.context.input, syncFlags);
}

static void xf_keyboard_clear(xfContext* xfc)
{
	WINPR_ASSERT(xfc);
	ZeroMemory(xfc->KeyboardState, sizeof(xfc->KeyboardState));
}

static BOOL xf_action_script_append(xfContext* xfc, const char* buffer, size_t size,
                                    WINPR_ATTR_UNUSED void* user, const char* what, const char* arg)
{
	WINPR_ASSERT(xfc);
	WINPR_UNUSED(what);
	WINPR_UNUSED(arg);

	if (!buffer || (size == 0))
		return TRUE;
	return ArrayList_Append(xfc->keyCombinations, buffer);
}

static void xf_keyboard_action_script_free(xfContext* xfc)
{
	xf_event_action_script_free(xfc);

	if (xfc->keyCombinations)
	{
		ArrayList_Free(xfc->keyCombinations);
		xfc->keyCombinations = NULL;
		xfc->actionScriptExists = FALSE;
	}
}

BOOL xf_keyboard_action_script_init(xfContext* xfc)
{
	WINPR_ASSERT(xfc);

	xf_keyboard_action_script_free(xfc);
	xfc->keyCombinations = ArrayList_New(TRUE);

	if (!xfc->keyCombinations)
		return FALSE;

	wObject* obj = ArrayList_Object(xfc->keyCombinations);
	WINPR_ASSERT(obj);
	obj->fnObjectNew = winpr_ObjectStringClone;
	obj->fnObjectFree = winpr_ObjectStringFree;

	if (!run_action_script(xfc, "key", NULL, xf_action_script_append, NULL))
		return FALSE;

	return xf_event_action_script_init(xfc);
}

static int xkb_cmp(const void* pva, const void* pvb)
{
	const struct x11_key_scancode_t* a = pva;
	const struct x11_key_scancode_t* b = pvb;

	if (!a && !b)
		return 0;
	if (!a)
		return 1;
	if (!b)
		return -1;
	return strcmp(a->name, b->name);
}

static BOOL try_add(xfContext* xfc, size_t offset, const char* xkb_keyname)
{
	WINPR_ASSERT(xfc);

	static BOOL initialized = FALSE;
	static struct x11_key_scancode_t copy[ARRAYSIZE(XKB_KEY_NAME_SCANCODE_TABLE)] = { 0 };
	if (!initialized)
	{
		// TODO: Here we can do some magic:
		// depending on input keyboard type use different mapping! (e.g. IBM, Apple, ...)
		memcpy(copy, XKB_KEY_NAME_SCANCODE_TABLE, sizeof(copy));
		qsort(copy, ARRAYSIZE(copy), sizeof(struct x11_key_scancode_t), xkb_cmp);
		initialized = TRUE;
	}

	struct x11_key_scancode_t key = { .name = xkb_keyname,
		                              .sc = WINPR_ASSERTING_INT_CAST(uint32_t, offset) };

	struct x11_key_scancode_t* found =
	    bsearch(&key, copy, ARRAYSIZE(copy), sizeof(struct x11_key_scancode_t), xkb_cmp);
	if (found)
	{
		WLog_Print(xfc->log, WLOG_DEBUG,
		           "%4s: keycode: 0x%02" PRIuz " -> rdp scancode: 0x%08" PRIx32 "", xkb_keyname,
		           offset, found->sc);
		xfc->X11_KEYCODE_TO_VIRTUAL_SCANCODE[offset] = found->sc;
		return TRUE;
	}

	return FALSE;
}

static int load_map_from_xkbfile(xfContext* xfc)
{
	WINPR_ASSERT(xfc);
	int status = -1;

	if (!xfc->display)
		return -2;

	XkbDescPtr xkb = XkbGetMap(xfc->display, 0, XkbUseCoreKbd);
	if (!xkb)
	{
		WLog_Print(xfc->log, WLOG_WARN, "XkbGetMap() == NULL");
		return -3;
	}

	const int rc = XkbGetNames(xfc->display, XkbKeyNamesMask, xkb);
	if (rc != Success)
	{
		char buffer[64] = { 0 };
		WLog_Print(xfc->log, WLOG_WARN, "XkbGetNames() != Success: [%s]",
		           x11_error_to_string(xfc, rc, buffer, sizeof(buffer)));
	}
	else
	{
		char xkb_keyname[XkbKeyNameLength + 1] = { 42, 42, 42, 42,
			                                       0 }; /* end-of-string at index 5 */

		WLog_Print(xfc->log, WLOG_TRACE, "XkbGetNames() == Success, min=%" PRIu8 ", max=%" PRIu8,
		           xkb->min_key_code, xkb->max_key_code);
		for (size_t i = xkb->min_key_code; i < xkb->max_key_code; i++)
		{
			BOOL found = FALSE;
			strncpy(xkb_keyname, xkb->names->keys[i].name, XkbKeyNameLength);

			WLog_Print(xfc->log, WLOG_TRACE, "KeyCode %" PRIuz " -> %s", i, xkb_keyname);
			if (strnlen(xkb_keyname, ARRAYSIZE(xkb_keyname)) >= 1)
				found = try_add(xfc, i, xkb_keyname);

			if (!found)
			{
#if defined(__APPLE__)
				const DWORD vkcode =
				    GetVirtualKeyCodeFromKeycode((UINT32)i - 8u, WINPR_KEYCODE_TYPE_APPLE);
				xfc->X11_KEYCODE_TO_VIRTUAL_SCANCODE[i] =
				    GetVirtualScanCodeFromVirtualKeyCode(vkcode, WINPR_KBD_TYPE_IBM_ENHANCED);
				found = TRUE;
#endif
			}
			if (!found)
			{
				WLog_Print(xfc->log, WLOG_WARN,
				           "%4s: keycode: 0x%02" PRIx32 " -> no RDP scancode found", xkb_keyname,
				           WINPR_ASSERTING_INT_CAST(UINT32, i));
			}
			else
				status = 0;
		}
	}

	XkbFreeKeyboard(xkb, 0, 1);

	return status;
}

BOOL xf_keyboard_init(xfContext* xfc)
{
	rdpSettings* settings = NULL;

	WINPR_ASSERT(xfc);

	settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

	xf_keyboard_clear(xfc);

	/* Layout detection algorithm:
	 *
	 * 1. If one was supplied, use that one
	 * 2. Try to determine current X11 keyboard layout
	 * 3. Try to determine default layout for current system language
	 * 4. Fall back to ENGLISH_UNITED_STATES
	 */
	UINT32 KeyboardLayout = freerdp_settings_get_uint32(settings, FreeRDP_KeyboardLayout);
	if (KeyboardLayout == 0)
	{
		xf_detect_keyboard_layout_from_xkb(xfc->log, &KeyboardLayout);
		if (KeyboardLayout == 0)
			freerdp_detect_keyboard_layout_from_system_locale(&KeyboardLayout);
		if (KeyboardLayout == 0)
			KeyboardLayout = ENGLISH_UNITED_STATES;
		if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardLayout, KeyboardLayout))
			return FALSE;
	}

	const int rc = load_map_from_xkbfile(xfc);
	if (rc != 0)
		return FALSE;

	return xf_keyboard_update_modifier_map(xfc);
}

void xf_keyboard_free(xfContext* xfc)
{
	xf_keyboard_modifier_map_free(xfc);
	xf_keyboard_action_script_free(xfc);
}

void xf_keyboard_key_press(xfContext* xfc, const XKeyEvent* event, KeySym keysym)
{
	BOOL last = 0;

	WINPR_ASSERT(xfc);
	WINPR_ASSERT(event);
	WINPR_ASSERT(event->keycode < ARRAYSIZE(xfc->KeyboardState));

	last = xfc->KeyboardState[event->keycode];
	xfc->KeyboardState[event->keycode] = TRUE;

	if (xf_keyboard_handle_special_keys(xfc, keysym))
		return;

	xf_keyboard_send_key(xfc, TRUE, last, event);
}

void xf_keyboard_key_release(xfContext* xfc, const XKeyEvent* event, KeySym keysym)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(event);
	WINPR_ASSERT(event->keycode < ARRAYSIZE(xfc->KeyboardState));

	BOOL last = xfc->KeyboardState[event->keycode];
	xfc->KeyboardState[event->keycode] = FALSE;
	xf_keyboard_handle_special_keys_release(xfc, keysym);
	xf_keyboard_send_key(xfc, FALSE, last, event);
}

static DWORD get_rdp_scancode_from_x11_keycode(xfContext* xfc, DWORD keycode)
{
	WINPR_ASSERT(xfc);
	if (keycode >= ARRAYSIZE(xfc->X11_KEYCODE_TO_VIRTUAL_SCANCODE))
	{
		WLog_ERR(TAG, "KeyCode %" PRIu32 " exceeds allowed value range [0,%" PRIuz "]", keycode,
		         ARRAYSIZE(xfc->X11_KEYCODE_TO_VIRTUAL_SCANCODE));
		return 0;
	}

	const DWORD scancode = xfc->X11_KEYCODE_TO_VIRTUAL_SCANCODE[keycode];
	const DWORD remapped = freerdp_keyboard_remap_key(xfc->remap_table, scancode);

#if defined(WITH_DEBUG_KBD)
	{
		const BOOL ex = RDP_SCANCODE_EXTENDED(scancode);
		const DWORD sc = RDP_SCANCODE_CODE(scancode);
		WLog_DBG(TAG, "x11 keycode: %02" PRIX32 " -> rdp code: [%04" PRIx16 "] %02" PRIX8 "%s",
		         keycode, scancode, sc, ex ? " extended" : "");
	}
#endif

	if (remapped != 0)
	{
#if defined(WITH_DEBUG_KBD)
		{
			const BOOL ex = RDP_SCANCODE_EXTENDED(remapped);
			const DWORD sc = RDP_SCANCODE_CODE(remapped);
			WLog_DBG(TAG,
			         "x11 keycode: %02" PRIX32 " -> remapped rdp code: [%04" PRIx16 "] %02" PRIX8
			         "%s",
			         keycode, remapped, sc, ex ? " extended" : "");
		}
#endif
		return remapped;
	}

	return scancode;
}

void xf_keyboard_release_all_keypress(xfContext* xfc)
{
	WINPR_ASSERT(xfc);

	WINPR_STATIC_ASSERT(ARRAYSIZE(xfc->KeyboardState) <= UINT32_MAX);
	for (size_t keycode = 0; keycode < ARRAYSIZE(xfc->KeyboardState); keycode++)
	{
		if (xfc->KeyboardState[keycode])
		{
			const DWORD rdp_scancode =
			    get_rdp_scancode_from_x11_keycode(xfc, WINPR_ASSERTING_INT_CAST(UINT32, keycode));

			// release tab before releasing the windows key.
			// this stops the start menu from opening on unfocus event.
			if (rdp_scancode == RDP_SCANCODE_LWIN)
				freerdp_input_send_keyboard_event_ex(xfc->common.context.input, FALSE, FALSE,
				                                     RDP_SCANCODE_TAB);

			freerdp_input_send_keyboard_event_ex(xfc->common.context.input, FALSE, FALSE,
			                                     rdp_scancode);
			xfc->KeyboardState[keycode] = FALSE;
		}
	}
	xf_sync_kbd_state(xfc);
}

static BOOL xf_keyboard_key_pressed(xfContext* xfc, KeySym keysym)
{
	KeyCode keycode = XKeysymToKeycode(xfc->display, keysym);
	WINPR_ASSERT(keycode < ARRAYSIZE(xfc->KeyboardState));
	return xfc->KeyboardState[keycode];
}

void xf_keyboard_send_key(xfContext* xfc, BOOL down, BOOL repeat, const XKeyEvent* event)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(event);

	rdpInput* input = xfc->common.context.input;
	WINPR_ASSERT(input);

	const DWORD rdp_scancode = get_rdp_scancode_from_x11_keycode(xfc, event->keycode);
	if (rdp_scancode == RDP_SCANCODE_PAUSE && !xf_keyboard_key_pressed(xfc, XK_Control_L) &&
	    !xf_keyboard_key_pressed(xfc, XK_Control_R))
	{
		/* Pause without Ctrl has to be sent as a series of keycodes
		 * in a single input PDU.  Pause only happens on "press";
		 * no code is sent on "release".
		 */
		if (down)
		{
			(void)freerdp_input_send_keyboard_pause_event(input);
		}
	}
	else
	{
		if (freerdp_settings_get_bool(xfc->common.context.settings, FreeRDP_UnicodeInput))
		{
			wchar_t buffer[32] = { 0 };
			int xwc = -1;

			switch (rdp_scancode)
			{
				case RDP_SCANCODE_RETURN:
					break;
				default:
				{
					XIM xim = XOpenIM(xfc->display, 0, 0, 0);
					if (!xim)
					{
						WLog_WARN(TAG, "Failed to XOpenIM");
					}
					else
					{
						XIC xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
						                    NULL);
						if (!xic)
						{
							WLog_WARN(TAG, "XCreateIC failed");
						}
						else
						{
							KeySym ignore = { 0 };
							Status return_status = 0;
							XKeyEvent ev = *event;
							ev.type = KeyPress;
							xwc = XwcLookupString(xic, &ev, buffer, ARRAYSIZE(buffer), &ignore,
							                      &return_status);
							XDestroyIC(xic);
						}
						XCloseIM(xim);
					}
				}
				break;
			}

			if (xwc < 1)
			{
				if (rdp_scancode == RDP_SCANCODE_UNKNOWN)
					WLog_ERR(TAG, "Unknown key with X keycode 0x%02" PRIx8 "", event->keycode);
				else
					(void)freerdp_input_send_keyboard_event_ex(input, down, repeat, rdp_scancode);
			}
			else
			{
				char str[3 * ARRAYSIZE(buffer)] = { 0 };
				// NOLINTNEXTLINE(concurrency-mt-unsafe)
				const size_t rc = wcstombs(str, buffer, ARRAYSIZE(buffer));

				WCHAR wbuffer[ARRAYSIZE(buffer)] = { 0 };
				(void)ConvertUtf8ToWChar(str, wbuffer, rc);
				(void)freerdp_input_send_unicode_keyboard_event(input, down ? 0 : KBD_FLAGS_RELEASE,
				                                                wbuffer[0]);
			}
		}
		else if (rdp_scancode == RDP_SCANCODE_UNKNOWN)
			WLog_ERR(TAG, "Unknown key with X keycode 0x%02" PRIx8 "", event->keycode);
		else
			(void)freerdp_input_send_keyboard_event_ex(input, down, repeat, rdp_scancode);
	}
}

static int xf_keyboard_read_keyboard_state(xfContext* xfc)
{
	int dummy = 0;
	Window wdummy = 0;
	UINT32 state = 0;

	if (!xfc->remote_app && xfc->window)
	{
		XQueryPointer(xfc->display, xfc->window->handle, &wdummy, &wdummy, &dummy, &dummy, &dummy,
		              &dummy, &state);
	}
	else
	{
		XQueryPointer(xfc->display, DefaultRootWindow(xfc->display), &wdummy, &wdummy, &dummy,
		              &dummy, &dummy, &dummy, &state);
	}

	return WINPR_ASSERTING_INT_CAST(int, state);
}

static int xf_keyboard_get_keymask(xfContext* xfc, KeySym keysym)
{
	int keysymMask = 0;
	KeyCode keycode = XKeysymToKeycode(xfc->display, keysym);

	if (keycode == NoSymbol)
		return 0;

	WINPR_ASSERT(xfc->modifierMap);
	for (int modifierpos = 0; modifierpos < 8; modifierpos++)
	{
		int offset = xfc->modifierMap->max_keypermod * modifierpos;

		for (int key = 0; key < xfc->modifierMap->max_keypermod; key++)
		{
			if (xfc->modifierMap->modifiermap[offset + key] == keycode)
			{
				keysymMask |= 1 << modifierpos;
			}
		}
	}

	return keysymMask;
}

static BOOL xf_keyboard_get_key_state(xfContext* xfc, int state, KeySym keysym)
{
	int keysymMask = xf_keyboard_get_keymask(xfc, keysym);

	if (!keysymMask)
		return FALSE;

	return (state & keysymMask) ? TRUE : FALSE;
}

static BOOL xf_keyboard_set_key_state(xfContext* xfc, BOOL on, KeySym keysym)
{
	if (!xfc->xkbAvailable)
		return FALSE;

	const int keysymMask = xf_keyboard_get_keymask(xfc, keysym);

	if (!keysymMask)
	{
		return FALSE;
	}

	return XkbLockModifiers(xfc->display, XkbUseCoreKbd,
	                        WINPR_ASSERTING_INT_CAST(uint32_t, keysymMask),
	                        on ? WINPR_ASSERTING_INT_CAST(uint32_t, keysymMask) : 0);
}

UINT32 xf_keyboard_get_toggle_keys_state(xfContext* xfc)
{
	UINT32 toggleKeysState = 0;
	const int state = xf_keyboard_read_keyboard_state(xfc);

	if (xf_keyboard_get_key_state(xfc, state, XK_Scroll_Lock))
		toggleKeysState |= KBD_SYNC_SCROLL_LOCK;

	if (xf_keyboard_get_key_state(xfc, state, XK_Num_Lock))
		toggleKeysState |= KBD_SYNC_NUM_LOCK;

	if (xf_keyboard_get_key_state(xfc, state, XK_Caps_Lock))
		toggleKeysState |= KBD_SYNC_CAPS_LOCK;

	if (xf_keyboard_get_key_state(xfc, state, XK_Kana_Lock))
		toggleKeysState |= KBD_SYNC_KANA_LOCK;

	return toggleKeysState;
}

static void xk_keyboard_update_modifier_keys(xfContext* xfc)
{
	const KeySym keysyms[] = { XK_Shift_L,   XK_Shift_R,   XK_Alt_L,   XK_Alt_R,
		                       XK_Control_L, XK_Control_R, XK_Super_L, XK_Super_R };

	xf_keyboard_clear(xfc);

	const int state = xf_keyboard_read_keyboard_state(xfc);

	for (size_t i = 0; i < ARRAYSIZE(keysyms); i++)
	{
		if (xf_keyboard_get_key_state(xfc, state, keysyms[i]))
		{
			const KeyCode keycode = XKeysymToKeycode(xfc->display, keysyms[i]);
			WINPR_ASSERT(keycode < ARRAYSIZE(xfc->KeyboardState));
			xfc->KeyboardState[keycode] = TRUE;
		}
	}
}

void xf_keyboard_focus_in(xfContext* xfc)
{
	UINT32 state = 0;
	Window w = None;
	int d = 0;
	int x = 0;
	int y = 0;

	WINPR_ASSERT(xfc);
	if (!xfc->display || !xfc->window)
		return;

	rdpInput* input = xfc->common.context.input;
	WINPR_ASSERT(input);

	const UINT32 syncFlags = xf_keyboard_get_toggle_keys_state(xfc);
	freerdp_input_send_focus_in_event(input, WINPR_ASSERTING_INT_CAST(UINT16, syncFlags));
	xk_keyboard_update_modifier_keys(xfc);

	/* finish with a mouse pointer position like mstsc.exe if required */

	if (xfc->remote_app || !xfc->window)
		return;

	if (XQueryPointer(xfc->display, xfc->window->handle, &w, &w, &d, &d, &x, &y, &state))
	{
		if ((x >= 0) && (x < xfc->window->width) && (y >= 0) && (y < xfc->window->height))
		{
			xf_event_adjust_coordinates(xfc, &x, &y);
			freerdp_client_send_button_event(&xfc->common, FALSE, PTR_FLAGS_MOVE, x, y);
		}
	}
}

static BOOL action_script_run(xfContext* xfc, const char* buffer, size_t size, void* user,
                              const char* what, const char* arg)
{
	WINPR_UNUSED(xfc);
	WINPR_UNUSED(what);
	WINPR_UNUSED(arg);
	WINPR_ASSERT(user);
	int* pstatus = user;

	if (size == 0)
	{
		WLog_WARN(TAG, "ActionScript key: script did not return data");
		return FALSE;
	}

	if (strcmp(buffer, "key-local") == 0)
		*pstatus = 0;
	else if (winpr_PathFileExists(buffer))
	{
		FILE* fp = popen(buffer, "w");
		if (!fp)
		{
			WLog_ERR(TAG, "Failed to execute '%s'", buffer);
			return FALSE;
		}

		*pstatus = pclose(fp);
		if (*pstatus < 0)
		{
			WLog_ERR(TAG, "Command '%s' returned %d", buffer, *pstatus);
			return FALSE;
		}
	}
	else
	{
		WLog_WARN(TAG, "ActionScript key: no such file '%s'", buffer);
		return FALSE;
	}
	return TRUE;
}

static int xf_keyboard_execute_action_script(xfContext* xfc, XF_MODIFIER_KEYS* mod, KeySym keysym)
{
	int status = 1;
	BOOL match = FALSE;
	char command[2048] = { 0 };
	char combination[1024] = { 0 };

	if (!xfc->actionScriptExists)
		return 1;

	if ((keysym == XK_Shift_L) || (keysym == XK_Shift_R) || (keysym == XK_Alt_L) ||
	    (keysym == XK_Alt_R) || (keysym == XK_Control_L) || (keysym == XK_Control_R))
	{
		return 1;
	}

	const char* keyStr = XKeysymToString(keysym);

	if (keyStr == 0)
	{
		return 1;
	}

	if (mod->Shift)
		winpr_str_append("Shift", combination, sizeof(combination), "+");

	if (mod->Ctrl)
		winpr_str_append("Ctrl", combination, sizeof(combination), "+");

	if (mod->Alt)
		winpr_str_append("Alt", combination, sizeof(combination), "+");

	if (mod->Super)
		winpr_str_append("Super", combination, sizeof(combination), "+");

	winpr_str_append(keyStr, combination, sizeof(combination), "+");

	for (size_t i = 0; i < strnlen(combination, sizeof(combination)); i++)
		combination[i] = WINPR_ASSERTING_INT_CAST(char, tolower(combination[i]));

	const size_t count = ArrayList_Count(xfc->keyCombinations);

	for (size_t index = 0; index < count; index++)
	{
		const char* keyCombination = (const char*)ArrayList_GetItem(xfc->keyCombinations, index);

		if (_stricmp(keyCombination, combination) == 0)
		{
			match = TRUE;
			break;
		}
	}

	if (!match)
		return 1;

	(void)sprintf_s(command, sizeof(command), "key %s", combination);
	if (!run_action_script(xfc, command, NULL, action_script_run, &status))
		return -1;

	return status;
}

static int xk_keyboard_get_modifier_keys(xfContext* xfc, XF_MODIFIER_KEYS* mod)
{
	mod->LeftShift = xf_keyboard_key_pressed(xfc, XK_Shift_L);
	mod->RightShift = xf_keyboard_key_pressed(xfc, XK_Shift_R);
	mod->Shift = mod->LeftShift || mod->RightShift;
	mod->LeftAlt = xf_keyboard_key_pressed(xfc, XK_Alt_L);
	mod->RightAlt = xf_keyboard_key_pressed(xfc, XK_Alt_R);
	mod->Alt = mod->LeftAlt || mod->RightAlt;
	mod->LeftCtrl = xf_keyboard_key_pressed(xfc, XK_Control_L);
	mod->RightCtrl = xf_keyboard_key_pressed(xfc, XK_Control_R);
	mod->Ctrl = mod->LeftCtrl || mod->RightCtrl;
	mod->LeftSuper = xf_keyboard_key_pressed(xfc, XK_Super_L);
	mod->RightSuper = xf_keyboard_key_pressed(xfc, XK_Super_R);
	mod->Super = mod->LeftSuper || mod->RightSuper;
	return 0;
}

BOOL xf_keyboard_handle_special_keys(xfContext* xfc, KeySym keysym)
{
	XF_MODIFIER_KEYS mod = { 0 };
	xk_keyboard_get_modifier_keys(xfc, &mod);

	// remember state of RightCtrl to ungrab keyboard if next action is release of RightCtrl
	// do not return anything such that the key could be used by client if ungrab is not the goal
	if (keysym == XK_Control_R)
	{
		if (mod.RightCtrl && !xfc->wasRightCtrlAlreadyPressed)
		{
			// Right Ctrl is pressed, getting ready to ungrab
			xfc->ungrabKeyboardWithRightCtrl = TRUE;
			xfc->wasRightCtrlAlreadyPressed = TRUE;
		}
	}
	else
	{
		// some other key has been pressed, abort ungrabbing
		if (xfc->ungrabKeyboardWithRightCtrl)
			xfc->ungrabKeyboardWithRightCtrl = FALSE;
	}

	const int rc = xf_keyboard_execute_action_script(xfc, &mod, keysym);
	if (rc < 0)
		return FALSE;
	if (rc == 0)
		return TRUE;

	if (!xfc->remote_app && xfc->fullscreen_toggle)
	{
		switch (keysym)
		{
			case XK_Return:
				if (mod.Ctrl && mod.Alt)
				{
					/* Ctrl-Alt-Enter: toggle full screen */
					WLog_INFO(TAG, "<ctrl>+<alt>+<enter> pressed, toggling fullscreen state...");
					xf_sync_kbd_state(xfc);
					xf_toggle_fullscreen(xfc);
					return TRUE;
				}
				break;
			default:
				break;
		}
	}

	if (mod.Ctrl && mod.Alt)
	{
		switch (keysym)
		{
			case XK_m:
			case XK_M:
				WLog_INFO(TAG, "<ctrl>+<alt>+m pressed, minimizing RDP session...");
				xf_sync_kbd_state(xfc);
				xf_minimize(xfc);
				return TRUE;
			case XK_c:
			case XK_C:
				/* Ctrl-Alt-C: toggle control */
				WLog_INFO(TAG, "<ctrl>+<alt>+c pressed, toggle encomps control...");
				if (freerdp_client_encomsp_toggle_control(xfc->common.encomsp))
					return TRUE;
				break;
			case XK_d:
			case XK_D:
				/* <ctrl>+<alt>+d: disconnect session */
				WLog_INFO(TAG, "<ctrl>+<alt>+d pressed, terminating RDP session...");
				xf_sync_kbd_state(xfc);
				return freerdp_abort_connect_context(&xfc->common.context);

			default:
				break;
		}
	}

#if 0 /* set to 1 to enable multi touch gesture simulation via keyboard */
#ifdef WITH_XRENDER

    if (!xfc->remote_app && freerdp_settings_get_bool(xfc->common.context.settings, FreeRDP_MultiTouchGestures))
	{
		rdpContext* ctx = &xfc->common.context;

		if (mod.Ctrl && mod.Alt)
		{
			int pdx = 0;
			int pdy = 0;
			int zdx = 0;
			int zdy = 0;

			switch (keysym)
			{
                case XK_0:	/* Ctrl-Alt-0: Reset scaling and panning */{
                const UINT32 sessionWidth = freerdp_settings_get_uint32(xfc->common.context.settings, FreeRDP_DesktopWidth);
                const UINT32 sessionHeight = freerdp_settings_get_uint32(xfc->common.context.settings, FreeRDP_DesktopHeight);

                    xfc->scaledWidth = sessionWidth;
                    xfc->scaledHeight = sessionHeight;
					xfc->offset_x = 0;
					xfc->offset_y = 0;

                    if (!xfc->fullscreen && (sessionWidth != xfc->window->width ||
                                             sessionHeight != xfc->window->height))
					{
                        xf_ResizeDesktopWindow(xfc, xfc->window, sessionWidth, sessionHeight);
					}

                    xf_draw_screen(xfc, 0, 0, sessionWidth, sessionHeight);
					return TRUE;
}

				case XK_1:	/* Ctrl-Alt-1: Zoom in */
					zdx = zdy = 10;
					break;

				case XK_2:	/* Ctrl-Alt-2: Zoom out */
					zdx = zdy = -10;
					break;

				case XK_3:	/* Ctrl-Alt-3: Pan left */
					pdx = -10;
					break;

				case XK_4:	/* Ctrl-Alt-4: Pan right */
					pdx = 10;
					break;

				case XK_5:	/* Ctrl-Alt-5: Pan up */
					pdy = -10;
					break;

				case XK_6:	/* Ctrl-Alt-6: Pan up */
					pdy = 10;
					break;
			}

			if (pdx != 0 || pdy != 0)
			{
				PanningChangeEventArgs e;
				EventArgsInit(&e, "xfreerdp");
				e.dx = pdx;
				e.dy = pdy;
				PubSub_OnPanningChange(ctx->pubSub, xfc, &e);
				return TRUE;
			}

			if (zdx != 0 || zdy != 0)
			{
				ZoomingChangeEventArgs e;
				EventArgsInit(&e, "xfreerdp");
				e.dx = zdx;
				e.dy = zdy;
				PubSub_OnZoomingChange(ctx->pubSub, xfc, &e);
				return TRUE;
			}
		}
	}

#endif /* WITH_XRENDER defined */
#endif /* pinch/zoom/pan simulation */
	return FALSE;
}

void xf_keyboard_handle_special_keys_release(xfContext* xfc, KeySym keysym)
{
	if (keysym != XK_Control_R)
		return;

	xfc->wasRightCtrlAlreadyPressed = FALSE;

	if (!xfc->ungrabKeyboardWithRightCtrl)
		return;

	// all requirements for ungrab are fulfilled, ungrabbing now
	XF_MODIFIER_KEYS mod = { 0 };
	xk_keyboard_get_modifier_keys(xfc, &mod);

	if (!mod.RightCtrl)
	{
		if (!xfc->fullscreen)
		{
			xf_sync_kbd_state(xfc);
			freerdp_client_encomsp_toggle_control(xfc->common.encomsp);
		}

		xfc->mouse_active = FALSE;
		xf_ungrab(xfc);
	}

	// ungrabbed
	xfc->ungrabKeyboardWithRightCtrl = FALSE;
}

BOOL xf_keyboard_set_indicators(rdpContext* context, UINT16 led_flags)
{
	xfContext* xfc = (xfContext*)context;
	xf_keyboard_set_key_state(xfc, led_flags & KBD_SYNC_SCROLL_LOCK, XK_Scroll_Lock);
	xf_keyboard_set_key_state(xfc, led_flags & KBD_SYNC_NUM_LOCK, XK_Num_Lock);
	xf_keyboard_set_key_state(xfc, led_flags & KBD_SYNC_CAPS_LOCK, XK_Caps_Lock);
	xf_keyboard_set_key_state(xfc, led_flags & KBD_SYNC_KANA_LOCK, XK_Kana_Lock);
	return TRUE;
}

BOOL xf_keyboard_set_ime_status(rdpContext* context, UINT16 imeId, UINT32 imeState,
                                UINT32 imeConvMode)
{
	if (!context)
		return FALSE;

	WLog_WARN(TAG,
	          "KeyboardSetImeStatus(unitId=%04" PRIx16 ", imeState=%08" PRIx32
	          ", imeConvMode=%08" PRIx32 ") ignored",
	          imeId, imeState, imeConvMode);
	return TRUE;
}

BOOL xf_ungrab(xfContext* xfc)
{
	WINPR_ASSERT(xfc);
	XUngrabKeyboard(xfc->display, CurrentTime);
	XUngrabPointer(xfc->display, CurrentTime);
	xfc->common.mouse_grabbed = FALSE;
	return TRUE;
}
