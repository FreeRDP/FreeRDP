/**
 * WinPR: Windows Portable Runtime
 * Keyboard Input
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>

#include <winpr/input.h>

/**
 * X11 Keycodes
 */

/**
 * Mac OS X
 */

DWORD KEYCODE_TO_VKCODE_APPLE[256] =
{
	0, /* 0 */
	0, /* 1 */
	0, /* 2 */
	0, /* 3 */
	0, /* 4 */
	0, /* 5 */
	0, /* 6 */
	0, /* 7 */
	VK_KEY_A, /* APPLE_VK_ANSI_A (0x00) */
	VK_KEY_S, /* APPLE_VK_ANSI_S (0x01) */
	VK_KEY_D, /* APPLE_VK_ANSI_D (0x02) */
	VK_KEY_F, /* APPLE_VK_ANSI_F (0x03) */
	VK_KEY_H, /* APPLE_VK_ANSI_H (0x04) */
	VK_KEY_G, /* APPLE_VK_ANSI_G (0x05) */
	VK_KEY_Z, /* APPLE_VK_ANSI_Z (0x06) */
	VK_KEY_X, /* APPLE_VK_ANSI_X (0x07) */
	VK_KEY_C, /* APPLE_VK_ANSI_C (0x08) */
	VK_KEY_V, /* APPLE_VK_ANSI_V (0x09) */
	VK_OEM_102, /* APPLE_VK_ISO_Section (0x0A) */
	VK_KEY_B, /* APPLE_VK_ANSI_B (0x0B) */
	VK_KEY_Q, /* APPLE_VK_ANSI_Q (0x0C) */
	VK_KEY_W, /* APPLE_VK_ANSI_W (0x0D) */
	VK_KEY_E, /* APPLE_VK_ANSI_E (0x0E) */
	VK_KEY_R, /* APPLE_VK_ANSI_R (0x0F) */
	VK_KEY_Y, /* APPLE_VK_ANSI_Y (0x10) */
	VK_KEY_T, /* APPLE_VK_ANSI_T (0x11) */
	VK_KEY_1, /* APPLE_VK_ANSI_1 (0x12) */
	VK_KEY_2, /* APPLE_VK_ANSI_2 (0x13) */
	VK_KEY_3, /* APPLE_VK_ANSI_3 (0x14) */
	VK_KEY_4, /* APPLE_VK_ANSI_4 (0x15) */
	VK_KEY_6, /* APPLE_VK_ANSI_6 (0x16) */
	VK_KEY_5, /* APPLE_VK_ANSI_5 (0x17) */
	VK_OEM_PLUS, /* APPLE_VK_ANSI_Equal (0x18) */
	VK_KEY_9, /* APPLE_VK_ANSI_9 (0x19) */
	VK_KEY_7, /* APPLE_VK_ANSI_7 (0x1A) */
	VK_OEM_MINUS, /* APPLE_VK_ANSI_Minus (0x1B) */
	VK_KEY_8, /* APPLE_VK_ANSI_8 (0x1C) */
	VK_KEY_0, /* APPLE_VK_ANSI_0 (0x1D) */
	VK_OEM_6, /* APPLE_VK_ANSI_RightBracket (0x1E) */
	VK_KEY_O, /* APPLE_VK_ANSI_O (0x1F) */
	VK_KEY_U, /* APPLE_VK_ANSI_U (0x20) */
	VK_OEM_4, /* APPLE_VK_ANSI_LeftBracket (0x21) */
	VK_KEY_I, /* APPLE_VK_ANSI_I (0x22) */
	VK_KEY_P, /* APPLE_VK_ANSI_P (0x23) */
	VK_RETURN, /* APPLE_VK_Return (0x24) */
	VK_KEY_L, /* APPLE_VK_ANSI_L (0x25) */
	VK_KEY_J, /* APPLE_VK_ANSI_J (0x26) */
	VK_OEM_7, /* APPLE_VK_ANSI_Quote (0x27) */
	VK_KEY_K, /* APPLE_VK_ANSI_K (0x28) */
	VK_OEM_1, /* APPLE_VK_ANSI_Semicolon (0x29) */
	VK_OEM_5, /* APPLE_VK_ANSI_Backslash (0x2A) */
	VK_OEM_COMMA, /* APPLE_VK_ANSI_Comma (0x2B) */
	VK_OEM_2, /* APPLE_VK_ANSI_Slash (0x2C) */
	VK_KEY_N, /* APPLE_VK_ANSI_N (0x2D) */
	VK_KEY_M, /* APPLE_VK_ANSI_M (0x2E) */
	VK_OEM_PERIOD, /* APPLE_VK_ANSI_Period (0x2F) */
	VK_TAB, /* APPLE_VK_Tab (0x30) */
	VK_SPACE, /* APPLE_VK_Space (0x31) */
	VK_OEM_3, /* APPLE_VK_ANSI_Grave (0x32) */
	VK_BACK, /* APPLE_VK_Delete (0x33) */
	0, /* APPLE_VK_0x34 (0x34) */
	VK_ESCAPE, /* APPLE_VK_Escape (0x35) */
	0, /* APPLE_VK_0x36 (0x36) */
	VK_LWIN, /* APPLE_VK_Command (0x37) */
	VK_LSHIFT, /* APPLE_VK_Shift (0x38) */
	VK_CAPITAL, /* APPLE_VK_CapsLock (0x39) */
	VK_LMENU, /* APPLE_VK_Option (0x3A) */
	VK_LCONTROL, /* APPLE_VK_Control (0x3B) */
	VK_RSHIFT, /* APPLE_VK_RightShift (0x3C) */
	VK_RMENU, /* APPLE_VK_RightOption (0x3D) */
	0, /* APPLE_VK_RightControl (0x3E) */
	VK_RWIN, /* APPLE_VK_Function (0x3F) */
	0, /* APPLE_VK_F17 (0x40) */
	VK_DECIMAL, /* APPLE_VK_ANSI_KeypadDecimal (0x41) */
	0, /* APPLE_VK_0x42 (0x42) */
	VK_MULTIPLY, /* APPLE_VK_ANSI_KeypadMultiply (0x43) */
	0, /* APPLE_VK_0x44 (0x44) */
	VK_ADD, /* APPLE_VK_ANSI_KeypadPlus (0x45) */
	0, /* APPLE_VK_0x46 (0x46) */
	VK_NUMLOCK, /* APPLE_VK_ANSI_KeypadClear (0x47) */
	0, /* APPLE_VK_VolumeUp (0x48) */
	0, /* APPLE_VK_VolumeDown (0x49) */
	0, /* APPLE_VK_Mute (0x4A) */
	VK_DIVIDE, /* APPLE_VK_ANSI_KeypadDivide (0x4B) */
	VK_RETURN, /* APPLE_VK_ANSI_KeypadEnter (0x4C) */
	0, /* APPLE_VK_0x4D (0x4D) */
	VK_SUBTRACT, /* APPLE_VK_ANSI_KeypadMinus (0x4E) */
	0, /* APPLE_VK_F18 (0x4F) */
	0, /* APPLE_VK_F19 (0x50) */
	0, /* APPLE_VK_ANSI_KeypadEquals (0x51) */
	VK_NUMPAD0, /* APPLE_VK_ANSI_Keypad0 (0x52) */
	VK_NUMPAD1, /* APPLE_VK_ANSI_Keypad1 (0x53) */
	VK_NUMPAD2, /* APPLE_VK_ANSI_Keypad2 (0x54) */
	VK_NUMPAD3, /* APPLE_VK_ANSI_Keypad3 (0x55) */
	VK_NUMPAD4, /* APPLE_VK_ANSI_Keypad4 (0x56) */
	VK_NUMPAD5, /* APPLE_VK_ANSI_Keypad5 (0x57) */
	VK_NUMPAD6, /* APPLE_VK_ANSI_Keypad6 (0x58) */
	VK_NUMPAD7, /* APPLE_VK_ANSI_Keypad7 (0x59) */
	0, /* APPLE_VK_F20 (0x5A) */
	VK_NUMPAD8, /* APPLE_VK_ANSI_Keypad8 (0x5B) */
	VK_NUMPAD9, /* APPLE_VK_ANSI_Keypad9 (0x5C) */
	0, /* APPLE_VK_JIS_Yen (0x5D) */
	0, /* APPLE_VK_JIS_Underscore (0x5E) */
	0, /* APPLE_VK_JIS_KeypadComma (0x5F) */
	VK_F5, /* APPLE_VK_F5 (0x60) */
	VK_F6, /* APPLE_VK_F6 (0x61) */
	VK_F7, /* APPLE_VK_F7 (0x62) */
	VK_F3, /* APPLE_VK_F3 (0x63) */
	VK_F8, /* APPLE_VK_F8 (0x64) */
	VK_F9, /* APPLE_VK_F9 (0x65) */
	0, /* APPLE_VK_JIS_Eisu (0x66) */
	VK_F11, /* APPLE_VK_F11 (0x67) */
	0, /* APPLE_VK_JIS_Kana (0x68) */
	VK_SNAPSHOT, /* APPLE_VK_F13 (0x69) */
	0, /* APPLE_VK_F16 (0x6A) */
	VK_SCROLL, /* APPLE_VK_F14 (0x6B) */
	0, /* APPLE_VK_0x6C (0x6C) */
	VK_F10, /* APPLE_VK_F10 (0x6D) */
	0, /* APPLE_VK_0x6E (0x6E) */
	VK_F12, /* APPLE_VK_F12 (0x6F) */
	0, /* APPLE_VK_0x70 (0x70) */
	VK_PAUSE, /* APPLE_VK_F15 (0x71) */
	VK_INSERT, /* APPLE_VK_Help (0x72) */
	VK_HOME, /* APPLE_VK_Home (0x73) */
	VK_PRIOR, /* APPLE_VK_PageUp (0x74) */
	VK_DELETE, /* APPLE_VK_ForwardDelete (0x75) */
	VK_F4, /* APPLE_VK_F4 (0x76) */
	VK_END, /* APPLE_VK_End (0x77) */
	VK_F2, /* APPLE_VK_F2 (0x78) */
	VK_NEXT, /* APPLE_VK_PageDown (0x79) */
	VK_F1, /* APPLE_VK_F1 (0x7A) */
	VK_LEFT, /* APPLE_VK_LeftArrow (0x7B) */
	VK_RIGHT, /* APPLE_VK_RightArrow (0x7C) */
	VK_DOWN, /* APPLE_VK_DownArrow (0x7D) */
	VK_UP, /* APPLE_VK_UpArrow (0x7E) */
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

/**
 * evdev (Linux)
 *
 * Refer to X Keyboard Configuration Database:
 * http://www.freedesktop.org/wiki/Software/XKeyboardConfig
 */

DWORD KEYCODE_TO_VKCODE_EVDEV[256] =
{
	0, /* 0 */
	0, /* 1 */
	0, /* 2 */
	0, /* 3 */
	0, /* 4 */
	0, /* 5 */
	0, /* 6 */
	0, /* 7 */
	0, /* 8 */
	0, /* <ESC> 9 */
	0, /* <AE01> 10 */
	0, /* <AE02> 11 */
	0, /* <AE03> 12 */
	0, /* <AE04> 13 */
	0, /* <AE05> 14 */
	0, /* <AE06> 15 */
	0, /* <AE07> 16 */
	0, /* <AE08> 17 */
	0, /* <AE09> 18 */
	0, /* <AE10> 19 */
	0, /* <AE11> 20 */
	0, /* <AE12> 21 */
	0, /* <BKSP> 22 */
	0, /* <TAB> 23 */
	0, /* <AD01> 24 */
	0, /* <AD02> 25 */
	0, /* <AD03> 26 */
	0, /* <AD04> 27 */
	0, /* <AD05> 28 */
	0, /* <AD06> 29 */
	0, /* <AD07> 30 */
	0, /* <AD08> 31 */
	0, /* <AD09> 32 */
	0, /* <AD10> 33 */
	0, /* <AD11> 34 */
	0, /* <AD12> 35 */
	0, /* <RTRN> 36 */
	0, /* <LCTL> 37 */
	0, /* <AC01> 38 */
	0, /* <AC02> 39 */
	0, /* <AC03> 40 */
	0, /* <AC04> 41 */
	0, /* <AC05> 42 */
	0, /* <AC06> 43 */
	0, /* <AC07> 44 */
	0, /* <AC08> 45 */
	0, /* <AC09> 46 */
	0, /* <AC10> 47 */
	0, /* <AC11> 48 */
	0, /* <TLDE> 49 */
	0, /* <LFSH> 50 */
	0, /* <BKSL> <AC12> 51 */
	0, /* <AB01> 52 */
	0, /* <AB02> 53 */
	0, /* <AB03> 54 */
	0, /* <AB04> 55 */
	0, /* <AB05> 56 */
	0, /* <AB06> 57 */
	0, /* <AB07> 58 */
	0, /* <AB08> 59 */
	0, /* <AB09> 60 */
	0, /* <AB10> 61 */
	0, /* <RTSH> 62 */
	0, /* <KPMU> 63 */
	0, /* <LALT> 64 */
	0, /* <SPCE> 65 */
	0, /* <CAPS> 66 */
	0, /* <FK01> 67 */
	0, /* <FK02> 68 */
	0, /* <FK03> 69 */
	0, /* <FK04> 70 */
	0, /* <FK05> 71 */
	0, /* <FK06> 72 */
	0, /* <FK07> 73 */
	0, /* <FK08> 74 */
	0, /* <FK09> 75 */
	0, /* <FK10> 76 */
	0, /* <NMLK> 77 */
	0, /* <SCLK> 78 */
	0, /* <KP7> 79 */
	0, /* <KP8> 80 */
	0, /* <KP9> 81 */
	0, /* <KPSU> 82 */
	0, /* <KP4> 83 */
	0, /* <KP5> 84 */
	0, /* <KP6> 85 */
	0, /* <KPAD> 86 */
	0, /* <KP1> 87 */
	0, /* <KP2> 88 */
	0, /* <KP3> 89 */
	0, /* <KP0> 90 */
	0, /* <KPDL> 91 */
	0, /* <LVL3> 92 */
	0, /* 93 */
	0, /* <LSGT> 94 */
	0, /* <FK11> 95 */
	0, /* <FK12> 96 */
	0, /* <AB11> 97 */
	0, /* <KATA> 98 */
	0, /* <HIRA> 99 */
	0, /* <HENK> 100 */
	0, /* <HKTG> 101 */
	0, /* <MUHE> 102 */
	0, /* <JPCM> 103 */
	0, /* <KPEN> 104 */
	0, /* <RCTL> 105 */
	0, /* <KPDV> 106 */
	0, /* <PRSC> 107 */
	0, /* <RALT> <ALGR> 108 */
	0, /* <LNFD> KEY_LINEFEED 109 */
	0, /* <HOME> 110 */
	0, /* <UP> 111 */
	0, /* <PGUP> 112 */
	0, /* <LEFT> 113 */
	0, /* <RGHT> 114 */
	0, /* <END> 115 */
	0, /* <DOWN> 116 */
	0, /* <PGDN> 117 */
	0, /* <INS> 118 */
	0, /* <DELE> 119 */
	0, /* <I120> KEY_MACRO 120 */
	0, /* <MUTE> 121 */
	0, /* <VOL-> 122 */
	0, /* <VOL+> 123 */
	0, /* <POWR> 124 */
	0, /* <KPEQ> 125 */
	0, /* <I126> KEY_KPPLUSMINUS 126 */
	0, /* <PAUS> 127 */
	0, /* <I128> KEY_SCALE 128 */
	0, /* <I129> <KPPT> KEY_KPCOMMA 129 */
	0, /* <HNGL> 130 */
	0, /* <HJCV> 131 */
	0, /* <AE13> 132 */
	0, /* <LWIN> <LMTA> 133 */
	0, /* <RWIN> <RMTA> 134 */
	0, /* <COMP> <MENU> 135 */
	0, /* <STOP> 136 */
	0, /* <AGAI> 137 */
	0, /* <PROP> 138 */
	0, /* <UNDO> 139 */
	0, /* <FRNT> 140 */
	0, /* <COPY> 141 */
	0, /* <OPEN> 142 */
	0, /* <PAST> 143 */
	0, /* <FIND> 144 */
	0, /* <CUT> 145 */
	0, /* <HELP> 146 */
	0, /* <I147> KEY_MENU 147 */
	0, /* <I148> KEY_CALC 148 */
	0, /* <I149> KEY_SETUP 149 */
	0, /* <I150> KEY_SLEEP 150 */
	0, /* <I151> KEY_WAKEUP 151 */
	0, /* <I152> KEY_FILE 152 */
	0, /* <I153> KEY_SEND 153 */
	0, /* <I154> KEY_DELETEFILE 154 */
	0, /* <I155> KEY_XFER 155 */
	0, /* <I156> KEY_PROG1 156 */
	0, /* <I157> KEY_PROG2 157 */
	0, /* <I158> KEY_WWW 158 */
	0, /* <I159> KEY_MSDOS 159 */
	0, /* <I160> KEY_COFFEE 160 */
	0, /* <I161> KEY_DIRECTION 161 */
	0, /* <I162> KEY_CYCLEWINDOWS 162 */
	0, /* <I163> KEY_MAIL 163 */
	0, /* <I164> KEY_BOOKMARKS 164 */
	0, /* <I165> KEY_COMPUTER 165 */
	0, /* <I166> KEY_BACK 166 */
	0, /* <I167> KEY_FORWARD 167 */
	0, /* <I168> KEY_CLOSECD 168 */
	0, /* <I169> KEY_EJECTCD 169 */
	0, /* <I170> KEY_EJECTCLOSECD 170 */
	0, /* <I171> KEY_NEXTSONG 171 */
	0, /* <I172> KEY_PLAYPAUSE 172 */
	0, /* <I173> KEY_PREVIOUSSONG 173 */
	0, /* <I174> KEY_STOPCD 174 */
	0, /* <I175> KEY_RECORD 175 */
	0, /* <I176> KEY_REWIND 176 */
	0, /* <I177> KEY_PHONE 177 */
	0, /* <I178> KEY_ISO 178 */
	0, /* <I179> KEY_CONFIG 179 */
	0, /* <I180> KEY_HOMEPAGE 180 */
	0, /* <I181> KEY_REFRESH 181 */
	0, /* <I182> KEY_EXIT 182 */
	0, /* <I183> KEY_MOVE 183 */
	0, /* <I184> KEY_EDIT 184 */
	0, /* <I185> KEY_SCROLLUP 185 */
	0, /* <I186> KEY_SCROLLDOWN 186 */
	0, /* <I187> KEY_KPLEFTPAREN 187 */
	0, /* <I188> KEY_KPRIGHTPAREN 188 */
	0, /* <I189> KEY_NEW 189 */
	0, /* <I190> KEY_REDO 190 */
	0, /* <FK13> 191 */
	0, /* <FK14> 192 */
	0, /* <FK15> 193 */
	0, /* <FK16> 194 */
	0, /* <FK17> 195 */
	0, /* <FK18> 196 */
	0, /* <FK19> 197 */
	0, /* <FK20> 198 */
	0, /* <FK21> 199 */
	0, /* <FK22> 200 */
	0, /* <FK23> 201 */
	0, /* <FK24> 202 */
	0, /* <MDSW> 203 */
	0, /* <ALT> 204 */
	0, /* <META> 205 */
	0, /* <SUPR> 206 */
	0, /* <HYPR> 207 */
	0, /* <I208> KEY_PLAYCD 208 */
	0, /* <I209> KEY_PAUSECD 209 */
	0, /* <I210> KEY_PROG3 210 */
	0, /* <I211> KEY_PROG4 211 */
	0, /* <I212> KEY_DASHBOARD 212 */
	0, /* <I213> KEY_SUSPEND 213 */
	0, /* <I214> KEY_CLOSE 214 */
	0, /* <I215> KEY_PLAY 215 */
	0, /* <I216> KEY_FASTFORWARD 216 */
	0, /* <I217> KEY_BASSBOOST 217 */
	0, /* <I218> KEY_PRINT 218 */
	0, /* <I219> KEY_HP 219 */
	0, /* <I220> KEY_CAMERA 220 */
	0, /* <I221> KEY_SOUND 221 */
	0, /* <I222> KEY_QUESTION 222 */
	0, /* <I223> KEY_EMAIL 223 */
	0, /* <I224> KEY_CHAT 224 */
	0, /* <I225> KEY_SEARCH 225 */
	0, /* <I226> KEY_CONNECT 226 */
	0, /* <I227> KEY_FINANCE 227 */
	0, /* <I228> KEY_SPORT 228 */
	0, /* <I229> KEY_SHOP 229 */
	0, /* <I230> KEY_ALTERASE 230 */
	0, /* <I231> KEY_CANCEL 231 */
	0, /* <I232> KEY_BRIGHTNESSDOWN 232 */
	0, /* <I233> KEY_BRIGHTNESSUP 233 */
	0, /* <I234> KEY_MEDIA 234 */
	0, /* <I235> KEY_SWITCHVIDEOMODE 235 */
	0, /* <I236> KEY_KBDILLUMTOGGLE 236 */
	0, /* <I237> KEY_KBDILLUMDOWN 237 */
	0, /* <I238> KEY_KBDILLUMUP 238 */
	0, /* <I239> KEY_SEND 239 */
	0, /* <I240> KEY_REPLY 240 */
	0, /* <I241> KEY_FORWARDMAIL 241 */
	0, /* <I242> KEY_SAVE 242 */
	0, /* <I243> KEY_DOCUMENTS 243 */
	0, /* <I244> KEY_BATTERY 244 */
	0, /* <I245> KEY_BLUETOOTH 245 */
	0, /* <I246> KEY_WLAN 246 */
	0, /* <I247> KEY_UWB 247 */
	0, /* <I248> KEY_UNKNOWN 248 */
	0, /* <I249> KEY_VIDEO_NEXT 249 */
	0, /* <I250> KEY_VIDEO_PREV 250 */
	0, /* <I251> KEY_BRIGHTNESS_CYCLE 251 */
	0, /* <I252> KEY_BRIGHTNESS_ZERO 252 */
	0, /* <I253> KEY_DISPLAY_OFF 253 */
	0, /* 254 */
	0 /* 255 */
};

DWORD GetVirtualKeyCodeFromKeycode(DWORD keycode, DWORD dwFlags)
{
	DWORD vkcode;

	vkcode = VK_NONE;

	if (dwFlags & KEYCODE_TYPE_APPLE)
	{
		if (keycode < 0xFF)
			vkcode = KEYCODE_TO_VKCODE_APPLE[keycode & 0xFF];
	}

	if (!vkcode)
		vkcode = VK_NONE;

	return vkcode;
}
