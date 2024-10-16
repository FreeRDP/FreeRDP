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

#include <X11/X.h>
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

static int detect_keyboard_layout_from_xkbfile(void* display, DWORD* keyboardLayoutId);
static int freerdp_keyboard_load_map_from_xkbfile(void* display, DWORD* x11_keycode_to_rdp_scancode,
                                                  size_t count);

static void* freerdp_keyboard_xkb_init(void)
{
	int status = 0;

	Display* display = XOpenDisplay(NULL);

	if (!display)
		return NULL;

	status = XkbQueryExtension(display, NULL, NULL, NULL, NULL, NULL);

	if (!status)
		return NULL;

	return (void*)display;
}

int freerdp_keyboard_init_xkbfile(DWORD* keyboardLayoutId, DWORD* x11_keycode_to_rdp_scancode,
                                  size_t count)
{
	WINPR_ASSERT(keyboardLayoutId);
	WINPR_ASSERT(x11_keycode_to_rdp_scancode);
	ZeroMemory(x11_keycode_to_rdp_scancode, sizeof(DWORD) * count);

	void* display = freerdp_keyboard_xkb_init();

	if (!display)
	{
		DEBUG_KBD("Error initializing xkb");
		return -1;
	}

	if (*keyboardLayoutId == 0)
	{
		detect_keyboard_layout_from_xkbfile(display, keyboardLayoutId);
		DEBUG_KBD("detect_keyboard_layout_from_xkb: %" PRIu32 " (0x%08" PRIX32 ")",
		          *keyboardLayoutId, *keyboardLayoutId);
	}

	const int rc =
	    freerdp_keyboard_load_map_from_xkbfile(display, x11_keycode_to_rdp_scancode, count);

	XCloseDisplay(display);

	return rc;
}

/* return substring starting after nth comma, ending at following comma */
static char* comma_substring(char* s, size_t n)
{
	char* p = NULL;

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
	DEBUG_KBD("display: %p", display);
	if (!display)
		return -2;

	char* rules = NULL;
	XkbRF_VarDefsRec rules_names = { 0 };
	const Bool rc = XkbRF_GetNamesProp(display, &rules, &rules_names);
	if (!rc)
	{
		DEBUG_KBD("XkbRF_GetNamesProp == False");
	}
	else
	{
		DEBUG_KBD("rules: %s", rules ? rules : "");
		DEBUG_KBD("model: %s", rules_names.model ? rules_names.model : "");
		DEBUG_KBD("layouts: %s", rules_names.layout ? rules_names.layout : "");
		DEBUG_KBD("variants: %s", rules_names.variant ? rules_names.variant : "");

		DWORD group = 0;
		XkbStateRec state = { 0 };
		XKeyboardState coreKbdState = { 0 };
		XGetKeyboardControl(display, &coreKbdState);

		if (XkbGetState(display, XkbUseCoreKbd, &state) == Success)
			group = state.group;

		DEBUG_KBD("group: %u", state.group);

		const char* layout = comma_substring(rules_names.layout, group);
		const char* variant = comma_substring(rules_names.variant, group);

		DEBUG_KBD("layout: %s", layout ? layout : "");
		DEBUG_KBD("variant: %s", variant ? variant : "");

		*keyboardLayoutId = find_keyboard_layout_in_xorg_rules(layout, variant);
	}
	free(rules_names.model);
	free(rules_names.layout);
	free(rules_names.variant);
	free(rules_names.options);
	free(rules);

	return 0;
}

static int xkb_cmp(const void* pva, const void* pvb)
{
	const XKB_KEY_NAME_SCANCODE* a = pva;
	const XKB_KEY_NAME_SCANCODE* b = pvb;

	if (!a && !b)
		return 0;
	if (!a)
		return 1;
	if (!b)
		return -1;
	return strcmp(a->xkb_keyname, b->xkb_keyname);
}

static BOOL try_add(size_t offset, const char* xkb_keyname, DWORD* x11_keycode_to_rdp_scancode,

                    size_t count)
{
	static BOOL initialized = FALSE;
	static XKB_KEY_NAME_SCANCODE copy[ARRAYSIZE(XKB_KEY_NAME_SCANCODE_TABLE)] = { 0 };
	if (!initialized)
	{
		memcpy(copy, XKB_KEY_NAME_SCANCODE_TABLE, sizeof(copy));
		qsort(copy, ARRAYSIZE(copy), sizeof(XKB_KEY_NAME_SCANCODE), xkb_cmp);
		initialized = TRUE;
	}

	XKB_KEY_NAME_SCANCODE key = { 0 };
	key.xkb_keyname = xkb_keyname;
	XKB_KEY_NAME_SCANCODE* found =
	    bsearch(&key, copy, ARRAYSIZE(copy), sizeof(XKB_KEY_NAME_SCANCODE), xkb_cmp);
	if (found)
	{
		DEBUG_KBD("%4s: keycode: 0x%02" PRIuz " -> rdp scancode: 0x%08" PRIx32 "", xkb_keyname,
		          offset, found->rdp_scancode);
		x11_keycode_to_rdp_scancode[offset] = found->rdp_scancode;
		return TRUE;
	}
	return FALSE;
}

int freerdp_keyboard_load_map_from_xkbfile(void* display, DWORD* x11_keycode_to_rdp_scancode,
                                           size_t count)
{
	int status = -1;

	if (!display)
		return -2;

	XkbDescPtr xkb = XkbGetMap(display, 0, XkbUseCoreKbd);
	if (!xkb)
	{
		DEBUG_KBD("XkbGetMap() == NULL");
		return -3;
	}

	if (XkbGetNames(display, XkbKeyNamesMask, xkb) != Success)
	{
		DEBUG_KBD("XkbGetNames() != Success");
	}
	else
	{
		char xkb_keyname[XkbKeyNameLength + 1] = { 42, 42, 42, 42,
			                                       0 }; /* end-of-string at index 5 */

		DEBUG_KBD("XkbGetNames() == Success, min=%" PRIu8 ", max=%" PRIu8, xkb->min_key_code,
		          xkb->max_key_code);
		for (size_t i = xkb->min_key_code; i <= MIN(xkb->max_key_code, count); i++)
		{
			BOOL found = FALSE;
			strncpy(xkb_keyname, xkb->names->keys[i].name, XkbKeyNameLength);

			DEBUG_KBD("KeyCode %" PRIuz " -> %s", i, xkb_keyname);
			if (strnlen(xkb_keyname, ARRAYSIZE(xkb_keyname)) < 1)
				continue;

			found = try_add(i, xkb_keyname, x11_keycode_to_rdp_scancode, count);

			if (!found)
			{
				DEBUG_KBD("%4s: keycode: 0x%02X -> no RDP scancode found", xkb_keyname, i);
			}
			else
				status = 0;
		}
	}

	XkbFreeKeyboard(xkb, 0, 1);

	return status;
}
