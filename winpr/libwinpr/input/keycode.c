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

#include <winpr/config.h>

#include <winpr/crt.h>

#include <winpr/input.h>

/**
 * X11 Keycodes
 */

/**
 * Mac OS X
 */

static DWORD KEYCODE_TO_VKCODE_APPLE[256] = {
	VK_KEY_A,             /* APPLE_VK_ANSI_A (0x00) */
	VK_KEY_S,             /* APPLE_VK_ANSI_S (0x01) */
	VK_KEY_D,             /* APPLE_VK_ANSI_D (0x02) */
	VK_KEY_F,             /* APPLE_VK_ANSI_F (0x03) */
	VK_KEY_H,             /* APPLE_VK_ANSI_H (0x04) */
	VK_KEY_G,             /* APPLE_VK_ANSI_G (0x05) */
	VK_KEY_Z,             /* APPLE_VK_ANSI_Z (0x06) */
	VK_KEY_X,             /* APPLE_VK_ANSI_X (0x07) */
	VK_KEY_C,             /* APPLE_VK_ANSI_C (0x08) */
	VK_KEY_V,             /* APPLE_VK_ANSI_V (0x09) */
	VK_OEM_102,           /* APPLE_VK_ISO_Section (0x0A) */
	VK_KEY_B,             /* APPLE_VK_ANSI_B (0x0B) */
	VK_KEY_Q,             /* APPLE_VK_ANSI_Q (0x0C) */
	VK_KEY_W,             /* APPLE_VK_ANSI_W (0x0D) */
	VK_KEY_E,             /* APPLE_VK_ANSI_E (0x0E) */
	VK_KEY_R,             /* APPLE_VK_ANSI_R (0x0F) */
	VK_KEY_Y,             /* APPLE_VK_ANSI_Y (0x10) */
	VK_KEY_T,             /* APPLE_VK_ANSI_T (0x11) */
	VK_KEY_1,             /* APPLE_VK_ANSI_1 (0x12) */
	VK_KEY_2,             /* APPLE_VK_ANSI_2 (0x13) */
	VK_KEY_3,             /* APPLE_VK_ANSI_3 (0x14) */
	VK_KEY_4,             /* APPLE_VK_ANSI_4 (0x15) */
	VK_KEY_6,             /* APPLE_VK_ANSI_6 (0x16) */
	VK_KEY_5,             /* APPLE_VK_ANSI_5 (0x17) */
	VK_OEM_PLUS,          /* APPLE_VK_ANSI_Equal (0x18) */
	VK_KEY_9,             /* APPLE_VK_ANSI_9 (0x19) */
	VK_KEY_7,             /* APPLE_VK_ANSI_7 (0x1A) */
	VK_OEM_MINUS,         /* APPLE_VK_ANSI_Minus (0x1B) */
	VK_KEY_8,             /* APPLE_VK_ANSI_8 (0x1C) */
	VK_KEY_0,             /* APPLE_VK_ANSI_0 (0x1D) */
	VK_OEM_6,             /* APPLE_VK_ANSI_RightBracket (0x1E) */
	VK_KEY_O,             /* APPLE_VK_ANSI_O (0x1F) */
	VK_KEY_U,             /* APPLE_VK_ANSI_U (0x20) */
	VK_OEM_4,             /* APPLE_VK_ANSI_LeftBracket (0x21) */
	VK_KEY_I,             /* APPLE_VK_ANSI_I (0x22) */
	VK_KEY_P,             /* APPLE_VK_ANSI_P (0x23) */
	VK_RETURN,            /* APPLE_VK_Return (0x24) */
	VK_KEY_L,             /* APPLE_VK_ANSI_L (0x25) */
	VK_KEY_J,             /* APPLE_VK_ANSI_J (0x26) */
	VK_OEM_7,             /* APPLE_VK_ANSI_Quote (0x27) */
	VK_KEY_K,             /* APPLE_VK_ANSI_K (0x28) */
	VK_OEM_1,             /* APPLE_VK_ANSI_Semicolon (0x29) */
	VK_OEM_5,             /* APPLE_VK_ANSI_Backslash (0x2A) */
	VK_OEM_COMMA,         /* APPLE_VK_ANSI_Comma (0x2B) */
	VK_OEM_2,             /* APPLE_VK_ANSI_Slash (0x2C) */
	VK_KEY_N,             /* APPLE_VK_ANSI_N (0x2D) */
	VK_KEY_M,             /* APPLE_VK_ANSI_M (0x2E) */
	VK_OEM_PERIOD,        /* APPLE_VK_ANSI_Period (0x2F) */
	VK_TAB,               /* APPLE_VK_Tab (0x30) */
	VK_SPACE,             /* APPLE_VK_Space (0x31) */
	VK_OEM_3,             /* APPLE_VK_ANSI_Grave (0x32) */
	VK_BACK,              /* APPLE_VK_Delete (0x33) */
	VK_NONE,              /* APPLE_VK_0x34 (0x34) */
	VK_ESCAPE,            /* APPLE_VK_Escape (0x35) */
	VK_RWIN | KBDEXT,     /* APPLE_VK_RightCommand (0x36) */
	VK_LWIN | KBDEXT,     /* APPLE_VK_Command (0x37) */
	VK_LSHIFT,            /* APPLE_VK_Shift (0x38) */
	VK_CAPITAL,           /* APPLE_VK_CapsLock (0x39) */
	VK_LMENU,             /* APPLE_VK_Option (0x3A) */
	VK_LCONTROL,          /* APPLE_VK_Control (0x3B) */
	VK_RSHIFT,            /* APPLE_VK_RightShift (0x3C) */
	VK_RMENU | KBDEXT,    /* APPLE_VK_RightOption (0x3D) */
	VK_RWIN | KBDEXT,     /* APPLE_VK_RightControl (0x3E) */
	VK_RWIN | KBDEXT,     /* APPLE_VK_Function (0x3F) */
	VK_F17,               /* APPLE_VK_F17 (0x40) */
	VK_DECIMAL,           /* APPLE_VK_ANSI_KeypadDecimal (0x41) */
	VK_NONE,              /* APPLE_VK_0x42 (0x42) */
	VK_MULTIPLY,          /* APPLE_VK_ANSI_KeypadMultiply (0x43) */
	VK_NONE,              /* APPLE_VK_0x44 (0x44) */
	VK_ADD,               /* APPLE_VK_ANSI_KeypadPlus (0x45) */
	VK_NONE,              /* APPLE_VK_0x46 (0x46) */
	VK_NUMLOCK,           /* APPLE_VK_ANSI_KeypadClear (0x47) */
	VK_VOLUME_UP,         /* APPLE_VK_VolumeUp (0x48) */
	VK_VOLUME_DOWN,       /* APPLE_VK_VolumeDown (0x49) */
	VK_VOLUME_MUTE,       /* APPLE_VK_Mute (0x4A) */
	VK_DIVIDE | KBDEXT,   /* APPLE_VK_ANSI_KeypadDivide (0x4B) */
	VK_RETURN | KBDEXT,   /* APPLE_VK_ANSI_KeypadEnter (0x4C) */
	VK_NONE,              /* APPLE_VK_0x4D (0x4D) */
	VK_SUBTRACT,          /* APPLE_VK_ANSI_KeypadMinus (0x4E) */
	VK_F18,               /* APPLE_VK_F18 (0x4F) */
	VK_F19,               /* APPLE_VK_F19 (0x50) */
	VK_CLEAR | KBDEXT,    /* APPLE_VK_ANSI_KeypadEquals (0x51) */
	VK_NUMPAD0,           /* APPLE_VK_ANSI_Keypad0 (0x52) */
	VK_NUMPAD1,           /* APPLE_VK_ANSI_Keypad1 (0x53) */
	VK_NUMPAD2,           /* APPLE_VK_ANSI_Keypad2 (0x54) */
	VK_NUMPAD3,           /* APPLE_VK_ANSI_Keypad3 (0x55) */
	VK_NUMPAD4,           /* APPLE_VK_ANSI_Keypad4 (0x56) */
	VK_NUMPAD5,           /* APPLE_VK_ANSI_Keypad5 (0x57) */
	VK_NUMPAD6,           /* APPLE_VK_ANSI_Keypad6 (0x58) */
	VK_NUMPAD7,           /* APPLE_VK_ANSI_Keypad7 (0x59) */
	VK_F20,               /* APPLE_VK_F20 (0x5A) */
	VK_NUMPAD8,           /* APPLE_VK_ANSI_Keypad8 (0x5B) */
	VK_NUMPAD9,           /* APPLE_VK_ANSI_Keypad9 (0x5C) */
	VK_NONE,              /* APPLE_VK_JIS_Yen (0x5D) */
	VK_NONE,              /* APPLE_VK_JIS_Underscore (0x5E) */
	VK_DECIMAL,           /* APPLE_VK_JIS_KeypadComma (0x5F) */
	VK_F5,                /* APPLE_VK_F5 (0x60) */
	VK_F6,                /* APPLE_VK_F6 (0x61) */
	VK_F7,                /* APPLE_VK_F7 (0x62) */
	VK_F3,                /* APPLE_VK_F3 (0x63) */
	VK_F8,                /* APPLE_VK_F8 (0x64) */
	VK_F9,                /* APPLE_VK_F9 (0x65) */
	VK_NONE,              /* APPLE_VK_JIS_Eisu (0x66) */
	VK_F11,               /* APPLE_VK_F11 (0x67) */
	VK_NONE,              /* APPLE_VK_JIS_Kana (0x68) */
	VK_SNAPSHOT | KBDEXT, /* APPLE_VK_F13 (0x69) */
	VK_F16,               /* APPLE_VK_F16 (0x6A) */
	VK_F14,               /* APPLE_VK_F14 (0x6B) */
	VK_NONE,              /* APPLE_VK_0x6C (0x6C) */
	VK_F10,               /* APPLE_VK_F10 (0x6D) */
	VK_NONE,              /* APPLE_VK_0x6E (0x6E) */
	VK_F12,               /* APPLE_VK_F12 (0x6F) */
	VK_NONE,              /* APPLE_VK_0x70 (0x70) */
	VK_PAUSE | KBDEXT,    /* APPLE_VK_F15 (0x71) */
	VK_INSERT | KBDEXT,   /* APPLE_VK_Help (0x72) */
	VK_HOME | KBDEXT,     /* APPLE_VK_Home (0x73) */
	VK_PRIOR | KBDEXT,    /* APPLE_VK_PageUp (0x74) */
	VK_DELETE | KBDEXT,   /* APPLE_VK_ForwardDelete (0x75) */
	VK_F4,                /* APPLE_VK_F4 (0x76) */
	VK_END | KBDEXT,      /* APPLE_VK_End (0x77) */
	VK_F2,                /* APPLE_VK_F2 (0x78) */
	VK_NEXT | KBDEXT,     /* APPLE_VK_PageDown (0x79) */
	VK_F1,                /* APPLE_VK_F1 (0x7A) */
	VK_LEFT | KBDEXT,     /* APPLE_VK_LeftArrow (0x7B) */
	VK_RIGHT | KBDEXT,    /* APPLE_VK_RightArrow (0x7C) */
	VK_DOWN | KBDEXT,     /* APPLE_VK_DownArrow (0x7D) */
	VK_UP | KBDEXT,       /* APPLE_VK_UpArrow (0x7E) */
	VK_NONE,              /* 127 */
	VK_NONE,              /* 128 */
	VK_NONE,              /* 129 */
	VK_NONE,              /* 130 */
	VK_NONE,              /* 131 */
	VK_NONE,              /* 132 */
	VK_NONE,              /* 133 */
	VK_NONE,              /* 134 */
	VK_NONE,              /* 135 */
	VK_NONE,              /* 136 */
	VK_NONE,              /* 137 */
	VK_NONE,              /* 138 */
	VK_NONE,              /* 139 */
	VK_NONE,              /* 140 */
	VK_NONE,              /* 141 */
	VK_NONE,              /* 142 */
	VK_NONE,              /* 143 */
	VK_NONE,              /* 144 */
	VK_NONE,              /* 145 */
	VK_NONE,              /* 146 */
	VK_NONE,              /* 147 */
	VK_NONE,              /* 148 */
	VK_NONE,              /* 149 */
	VK_NONE,              /* 150 */
	VK_NONE,              /* 151 */
	VK_NONE,              /* 152 */
	VK_NONE,              /* 153 */
	VK_NONE,              /* 154 */
	VK_NONE,              /* 155 */
	VK_NONE,              /* 156 */
	VK_NONE,              /* 157 */
	VK_NONE,              /* 158 */
	VK_NONE,              /* 159 */
	VK_NONE,              /* 160 */
	VK_NONE,              /* 161 */
	VK_NONE,              /* 162 */
	VK_NONE,              /* 163 */
	VK_NONE,              /* 164 */
	VK_NONE,              /* 165 */
	VK_NONE,              /* 166 */
	VK_NONE,              /* 167 */
	VK_NONE,              /* 168 */
	VK_NONE,              /* 169 */
	VK_NONE,              /* 170 */
	VK_NONE,              /* 171 */
	VK_NONE,              /* 172 */
	VK_NONE,              /* 173 */
	VK_NONE,              /* 174 */
	VK_NONE,              /* 175 */
	VK_NONE,              /* 176 */
	VK_NONE,              /* 177 */
	VK_NONE,              /* 178 */
	VK_NONE,              /* 179 */
	VK_NONE,              /* 180 */
	VK_NONE,              /* 181 */
	VK_NONE,              /* 182 */
	VK_NONE,              /* 183 */
	VK_NONE,              /* 184 */
	VK_NONE,              /* 185 */
	VK_NONE,              /* 186 */
	VK_NONE,              /* 187 */
	VK_NONE,              /* 188 */
	VK_NONE,              /* 189 */
	VK_NONE,              /* 190 */
	VK_NONE,              /* 191 */
	VK_NONE,              /* 192 */
	VK_NONE,              /* 193 */
	VK_NONE,              /* 194 */
	VK_NONE,              /* 195 */
	VK_NONE,              /* 196 */
	VK_NONE,              /* 197 */
	VK_NONE,              /* 198 */
	VK_NONE,              /* 199 */
	VK_NONE,              /* 200 */
	VK_NONE,              /* 201 */
	VK_NONE,              /* 202 */
	VK_NONE,              /* 203 */
	VK_NONE,              /* 204 */
	VK_NONE,              /* 205 */
	VK_NONE,              /* 206 */
	VK_NONE,              /* 207 */
	VK_NONE,              /* 208 */
	VK_NONE,              /* 209 */
	VK_NONE,              /* 210 */
	VK_NONE,              /* 211 */
	VK_NONE,              /* 212 */
	VK_NONE,              /* 213 */
	VK_NONE,              /* 214 */
	VK_NONE,              /* 215 */
	VK_NONE,              /* 216 */
	VK_NONE,              /* 217 */
	VK_NONE,              /* 218 */
	VK_NONE,              /* 219 */
	VK_NONE,              /* 220 */
	VK_NONE,              /* 221 */
	VK_NONE,              /* 222 */
	VK_NONE,              /* 223 */
	VK_NONE,              /* 224 */
	VK_NONE,              /* 225 */
	VK_NONE,              /* 226 */
	VK_NONE,              /* 227 */
	VK_NONE,              /* 228 */
	VK_NONE,              /* 229 */
	VK_NONE,              /* 230 */
	VK_NONE,              /* 231 */
	VK_NONE,              /* 232 */
	VK_NONE,              /* 233 */
	VK_NONE,              /* 234 */
	VK_NONE,              /* 235 */
	VK_NONE,              /* 236 */
	VK_NONE,              /* 237 */
	VK_NONE,              /* 238 */
	VK_NONE,              /* 239 */
	VK_NONE,              /* 240 */
	VK_NONE,              /* 241 */
	VK_NONE,              /* 242 */
	VK_NONE,              /* 243 */
	VK_NONE,              /* 244 */
	VK_NONE,              /* 245 */
	VK_NONE,              /* 246 */
	VK_NONE,              /* 247 */
	VK_NONE,              /* 248 */
	VK_NONE,              /* 249 */
	VK_NONE,              /* 250 */
	VK_NONE,              /* 251 */
	VK_NONE,              /* 252 */
	VK_NONE,              /* 253 */
	VK_NONE,              /* 254 */
	VK_NONE               /* 255 */
};

/**
 * evdev (Linux)
 *
 * Refer to linux/input-event-codes.h
 */

static DWORD KEYCODE_TO_VKCODE_EVDEV[256] = {
	VK_NONE,                       /* KEY_RESERVED (0) */
	VK_ESCAPE,                     /* KEY_ESC (1) */
	VK_KEY_1,                      /* KEY_1 (2) */
	VK_KEY_2,                      /* KEY_2 (3) */
	VK_KEY_3,                      /* KEY_3 (4) */
	VK_KEY_4,                      /* KEY_4 (5) */
	VK_KEY_5,                      /* KEY_5 (6) */
	VK_KEY_6,                      /* KEY_6 (7) */
	VK_KEY_7,                      /* KEY_7 (8) */
	VK_KEY_8,                      /* KEY_8 (9) */
	VK_KEY_9,                      /* KEY_9 (10) */
	VK_KEY_0,                      /* KEY_0 (11) */
	VK_OEM_MINUS,                  /* KEY_MINUS (12) */
	VK_OEM_PLUS,                   /* KEY_EQUAL (13) */
	VK_BACK,                       /* KEY_BACKSPACE (14) */
	VK_TAB,                        /* KEY_TAB (15) */
	VK_KEY_Q,                      /* KEY_Q (16) */
	VK_KEY_W,                      /* KEY_W (17) */
	VK_KEY_E,                      /* KEY_E (18) */
	VK_KEY_R,                      /* KEY_R (19) */
	VK_KEY_T,                      /* KEY_T (20) */
	VK_KEY_Y,                      /* KEY_Y (21) */
	VK_KEY_U,                      /* KEY_U (22) */
	VK_KEY_I,                      /* KEY_I (23) */
	VK_KEY_O,                      /* KEY_O (24) */
	VK_KEY_P,                      /* KEY_P (25) */
	VK_OEM_4,                      /* KEY_LEFTBRACE (26) */
	VK_OEM_6,                      /* KEY_RIGHTBRACE (27) */
	VK_RETURN,                     /* KEY_ENTER (28) */
	VK_LCONTROL,                   /* KEY_LEFTCTRL (29) */
	VK_KEY_A,                      /* KEY_A (30) */
	VK_KEY_S,                      /* KEY_S (31) */
	VK_KEY_D,                      /* KEY_D (32) */
	VK_KEY_F,                      /* KEY_F (33) */
	VK_KEY_G,                      /* KEY_G (34) */
	VK_KEY_H,                      /* KEY_H (35) */
	VK_KEY_J,                      /* KEY_J (36) */
	VK_KEY_K,                      /* KEY_K (37) */
	VK_KEY_L,                      /* KEY_L (38) */
	VK_OEM_1,                      /* KEY_SEMICOLON (39) */
	VK_OEM_7,                      /* KEY_APOSTROPHE (40) */
	VK_OEM_3,                      /* KEY_GRAVE (41) */
	VK_LSHIFT,                     /* KEY_LEFTSHIFT (42) */
	VK_OEM_5,                      /* KEY_BACKSLASH (43) */
	VK_KEY_Z,                      /* KEY_Z (44) */
	VK_KEY_X,                      /* KEY_X (45) */
	VK_KEY_C,                      /* KEY_C (46) */
	VK_KEY_V,                      /* KEY_V (47) */
	VK_KEY_B,                      /* KEY_B (48) */
	VK_KEY_N,                      /* KEY_N (49) */
	VK_KEY_M,                      /* KEY_M (50) */
	VK_OEM_COMMA,                  /* KEY_COMMA (51) */
	VK_OEM_PERIOD,                 /* KEY_DOT (52) */
	VK_OEM_2,                      /* KEY_SLASH (53) */
	VK_RSHIFT,                     /* KEY_RIGHTSHIFT (54) */
	VK_MULTIPLY,                   /* KEY_KPASTERISK (55) */
	VK_LMENU,                      /* KEY_LEFTALT (56) */
	VK_SPACE,                      /* KEY_SPACE (57) */
	VK_CAPITAL,                    /* KEY_CAPSLOCK (58) */
	VK_F1,                         /* KEY_F1 (59) */
	VK_F2,                         /* KEY_F2 (60) */
	VK_F3,                         /* KEY_F3 (61) */
	VK_F4,                         /* KEY_F4 (62) */
	VK_F5,                         /* KEY_F5 (63) */
	VK_F6,                         /* KEY_F6 (64) */
	VK_F7,                         /* KEY_F7 (65) */
	VK_F8,                         /* KEY_F8 (66) */
	VK_F9,                         /* KEY_F9 (67) */
	VK_F10,                        /* KEY_F10 (68) */
	VK_NUMLOCK,                    /* KEY_NUMLOCK (69) */
	VK_SCROLL,                     /* KEY_SCROLLLOCK (70) */
	VK_NUMPAD7,                    /* KEY_KP7 (71) */
	VK_NUMPAD8,                    /* KEY_KP8 (72) */
	VK_NUMPAD9,                    /* KEY_KP9 (73) */
	VK_SUBTRACT,                   /* KEY_KPMINUS (74) */
	VK_NUMPAD4,                    /* KEY_KP4 (75) */
	VK_NUMPAD5,                    /* KEY_KP5 (76) */
	VK_NUMPAD6,                    /* KEY_KP6 (77) */
	VK_ADD,                        /* KEY_KPPLUS (78) */
	VK_NUMPAD1,                    /* KEY_KP1 (79) */
	VK_NUMPAD2,                    /* KEY_KP2 (80) */
	VK_NUMPAD3,                    /* KEY_KP3 (81) */
	VK_NUMPAD0,                    /* KEY_KP0 (82) */
	VK_DECIMAL,                    /* KEY_KPDOT (83) */
	VK_NONE,                       /* (84) */
	VK_NONE,                       /* KEY_ZENKAKUHANKAKU (85) */
	VK_OEM_102,                    /* KEY_102ND (86) */
	VK_F11,                        /* KEY_F11 (87) */
	VK_F12,                        /* KEY_F12 (88) */
	VK_ABNT_C1,                    /* KEY_RO (89) */
	VK_DBE_KATAKANA,               /* KEY_KATAKANA (90) */
	VK_DBE_HIRAGANA,               /* KEY_HIRAGANA (91) */
	VK_CONVERT,                    /* KEY_HENKAN (92) */
	VK_HKTG,                       /* KEY_KATAKANAHIRAGANA (93) */
	VK_NONCONVERT,                 /* KEY_MUHENKAN (94) */
	VK_NONE,                       /* KEY_KPJPCOMMA (95) */
	VK_RETURN | KBDEXT,            /* KEY_KPENTER (96) */
	VK_RCONTROL | KBDEXT,          /* KEY_RIGHTCTRL (97) */
	VK_DIVIDE | KBDEXT,            /* KEY_KPSLASH (98) */
	VK_SNAPSHOT | KBDEXT,          /* KEY_SYSRQ (99) */
	VK_RMENU | KBDEXT,             /* KEY_RIGHTALT (100) */
	VK_NONE,                       /* KEY_LINEFEED (101) */
	VK_HOME | KBDEXT,              /* KEY_HOME (102) */
	VK_UP | KBDEXT,                /* KEY_UP (103) */
	VK_PRIOR | KBDEXT,             /* KEY_PAGEUP (104) */
	VK_LEFT | KBDEXT,              /* KEY_LEFT (105) */
	VK_RIGHT | KBDEXT,             /* KEY_RIGHT (106) */
	VK_END | KBDEXT,               /* KEY_END (107) */
	VK_DOWN | KBDEXT,              /* KEY_DOWN (108) */
	VK_NEXT | KBDEXT,              /* KEY_PAGEDOWN (109) */
	VK_INSERT | KBDEXT,            /* KEY_INSERT (110) */
	VK_DELETE | KBDEXT,            /* KEY_DELETE (111) */
	VK_NONE,                       /* KEY_MACRO (112) */
	VK_VOLUME_MUTE | KBDEXT,       /* KEY_MUTE (113) */
	VK_VOLUME_DOWN | KBDEXT,       /* KEY_VOLUMEDOWN (114) */
	VK_VOLUME_UP | KBDEXT,         /* KEY_VOLUMEUP (115) */
	VK_NONE,                       /* KEY_POWER (SC System Power Down) (116) */
	VK_NONE,                       /* KEY_KPEQUAL (117) */
	VK_NONE,                       /* KEY_KPPLUSMINUS (118) */
	VK_PAUSE | KBDEXT,             /* KEY_PAUSE (119) */
	VK_NONE,                       /* KEY_SCALE (AL Compiz Scale (Expose)) (120) */
	VK_ABNT_C2,                    /* KEY_KPCOMMA (121) */
	VK_HANGUL,                     /* KEY_HANGEUL, KEY_HANGUEL (122) */
	VK_HANJA,                      /* KEY_HANJA (123) */
	VK_OEM_8,                      /* KEY_YEN (124) */
	VK_LWIN | KBDEXT,              /* KEY_LEFTMETA (125) */
	VK_RWIN | KBDEXT,              /* KEY_RIGHTMETA (126) */
	VK_NONE,                       /* KEY_COMPOSE (127) */
	VK_NONE,                       /* KEY_STOP (AC Stop) (128) */
	VK_NONE,                       /* KEY_AGAIN (AC Properties) (129) */
	VK_NONE,                       /* KEY_PROPS (AC Undo) (130) */
	VK_NONE,                       /* KEY_UNDO (131) */
	VK_NONE,                       /* KEY_FRONT (132) */
	VK_NONE,                       /* KEY_COPY (AC Copy) (133) */
	VK_NONE,                       /* KEY_OPEN (AC Open) (134) */
	VK_NONE,                       /* KEY_PASTE (AC Paste) (135) */
	VK_NONE,                       /* KEY_FIND (AC Search) (136) */
	VK_NONE,                       /* KEY_CUT (AC Cut) (137) */
	VK_HELP,                       /* KEY_HELP (AL Integrated Help Center) (138) */
	VK_APPS | KBDEXT,              /* KEY_MENU (Menu (show menu)) (139) */
	VK_NONE,                       /* KEY_CALC (AL Calculator) (140) */
	VK_NONE,                       /* KEY_SETUP (141) */
	VK_SLEEP,                      /* KEY_SLEEP (SC System Sleep) (142) */
	VK_NONE,                       /* KEY_WAKEUP (System Wake Up) (143) */
	VK_NONE,                       /* KEY_FILE (AL Local Machine Browser) (144) */
	VK_NONE,                       /* KEY_SENDFILE (145) */
	VK_NONE,                       /* KEY_DELETEFILE (146) */
	VK_CONVERT,                    /* KEY_XFER (147) */
	VK_LAUNCH_APP1,                /* KEY_PROG1 (148) */
	VK_LAUNCH_APP2,                /* KEY_PROG2 (149) */
	VK_NONE,                       /* KEY_WWW (AL Internet Browser) (150) */
	VK_NONE,                       /* KEY_MSDOS (151) */
	VK_NONE,                       /* KEY_COFFEE, KEY_SCREENLOCK
	                                * (AL Terminal Lock/Screensaver) (152) */
	VK_NONE,                       /* KEY_ROTATE_DISPLAY, KEY_DIRECTION
	                                * (Display orientation for e.g. tablets) (153) */
	VK_NONE,                       /* KEY_CYCLEWINDOWS (154) */
	VK_LAUNCH_MAIL | KBDEXT,       /* KEY_MAIL (155) */
	VK_BROWSER_FAVORITES | KBDEXT, /* KEY_BOOKMARKS (AC Bookmarks) (156) */
	VK_NONE,                       /* KEY_COMPUTER (157) */
	VK_BROWSER_BACK | KBDEXT,      /* KEY_BACK (AC Back) (158) */
	VK_BROWSER_FORWARD | KBDEXT,   /* KEY_FORWARD (AC Forward) (159) */
	VK_NONE,                       /* KEY_CLOSECD (160) */
	VK_NONE,                       /* KEY_EJECTCD (161) */
	VK_NONE,                       /* KEY_EJECTCLOSECD (162) */
	VK_MEDIA_NEXT_TRACK | KBDEXT,  /* KEY_NEXTSONG (163) */
	VK_MEDIA_PLAY_PAUSE | KBDEXT,  /* KEY_PLAYPAUSE (164) */
	VK_MEDIA_PREV_TRACK | KBDEXT,  /* KEY_PREVIOUSSONG (165) */
	VK_MEDIA_STOP | KBDEXT,        /* KEY_STOPCD (166) */
	VK_NONE,                       /* KEY_RECORD (167) */
	VK_NONE,                       /* KEY_REWIND (168) */
	VK_NONE,                       /* KEY_PHONE (Media Select Telephone) (169) */
	VK_NONE,                       /* KEY_ISO (170) */
	VK_NONE,                       /* KEY_CONFIG (AL Consumer Control Configuration) (171) */
	VK_BROWSER_HOME | KBDEXT,      /* KEY_HOMEPAGE (AC Home) (172) */
	VK_BROWSER_REFRESH | KBDEXT,   /* KEY_REFRESH (AC Refresh) (173) */
	VK_NONE,                       /* KEY_EXIT (AC Exit) (174) */
	VK_NONE,                       /* KEY_MOVE (175) */
	VK_NONE,                       /* KEY_EDIT (176) */
	VK_NONE,                       /* KEY_SCROLLUP (177) */
	VK_NONE,                       /* KEY_SCROLLDOWN (178) */
	VK_NONE,                       /* KEY_KPLEFTPAREN (179) */
	VK_NONE,                       /* KEY_KPRIGHTPAREN (180) */
	VK_NONE,                       /* KEY_NEW (AC New) (181) */
	VK_NONE,                       /* KEY_REDO (AC Redo/Repeat) (182) */
	VK_F13,                        /* KEY_F13 (183) */
	VK_F14,                        /* KEY_F14 (184) */
	VK_F15,                        /* KEY_F15 (185) */
	VK_F16,                        /* KEY_F16 (186) */
	VK_F17,                        /* KEY_F17 (187) */
	VK_F18,                        /* KEY_F18 (188) */
	VK_F19,                        /* KEY_F19 (189) */
	VK_F20,                        /* KEY_F20 (190) */
	VK_F21,                        /* KEY_F21 (191) */
	VK_F22,                        /* KEY_F22 (192) */
	VK_F23,                        /* KEY_F23 (193) */
	VK_F24,                        /* KEY_F24 (194) */
	VK_NONE,                       /* (195) */
	VK_NONE,                       /* (196) */
	VK_NONE,                       /* (197) */
	VK_NONE,                       /* (198) */
	VK_NONE,                       /* (199) */
	VK_PLAY,                       /* KEY_PLAYCD (200) */
	VK_NONE,                       /* KEY_PAUSECD (201) */
	VK_NONE,                       /* KEY_PROG3 (202) */
	VK_NONE,                       /* KEY_PROG4 (203) */
	VK_NONE,                       /* KEY_ALL_APPLICATIONS, KEY_DASHBOARD
	                                * (AC Desktop Show All Applications) (204) */
	VK_NONE,                       /* KEY_SUSPEND (205) */
	VK_NONE,                       /* KEY_CLOSE (AC Close) (206) */
	VK_PLAY,                       /* KEY_PLAY (207) */
	VK_NONE,                       /* KEY_FASTFORWARD (208) */
	VK_NONE,                       /* KEY_BASSBOOST (209) */
	VK_PRINT | KBDEXT,             /* KEY_PRINT (AC Print) (210) */
	VK_NONE,                       /* KEY_HP (211) */
	VK_NONE,                       /* KEY_CAMERA (212) */
	VK_NONE,                       /* KEY_SOUND (213) */
	VK_NONE,                       /* KEY_QUESTION (214) */
	VK_NONE,                       /* KEY_EMAIL (215) */
	VK_NONE,                       /* KEY_CHAT (216) */
	VK_BROWSER_SEARCH | KBDEXT,    /* KEY_SEARCH (217) */
	VK_NONE,                       /* KEY_CONNECT (218) */
	VK_NONE,                       /* KEY_FINANCE (AL Checkbook/Finance) (219) */
	VK_NONE,                       /* KEY_SPORT (220) */
	VK_NONE,                       /* KEY_SHOP (221) */
	VK_NONE,                       /* KEY_ALTERASE (222) */
	VK_NONE,                       /* KEY_CANCEL (AC Cancel) (223) */
	VK_NONE,                       /* KEY_BRIGHTNESSDOWN (224) */
	VK_NONE,                       /* KEY_BRIGHTNESSUP (225) */
	VK_NONE,                       /* KEY_MEDIA (226) */
	VK_NONE,                       /* KEY_SWITCHVIDEOMODE
	                                * (Cycle between available video outputs
	                                *  (Monitor/LCD/TV-out/etc)) (227) */
	VK_NONE,                       /* KEY_KBDILLUMTOGGLE (228) */
	VK_NONE,                       /* KEY_KBDILLUMDOWN (229) */
	VK_NONE,                       /* KEY_KBDILLUMUP (230) */
	VK_NONE,                       /* KEY_SEND (AC Send) (231) */
	VK_NONE,                       /* KEY_REPLY (AC Reply) (232) */
	VK_NONE,                       /* KEY_FORWARDMAIL (AC Forward Msg) (233) */
	VK_NONE,                       /* KEY_SAVE (AC Save) (234) */
	VK_NONE,                       /* KEY_DOCUMENTS (235) */
	VK_NONE,                       /* KEY_BATTERY (236) */
	VK_NONE,                       /* KEY_BLUETOOTH (237) */
	VK_NONE,                       /* KEY_WLAN (238) */
	VK_NONE,                       /* KEY_UWB (239) */
	VK_NONE,                       /* KEY_UNKNOWN (240) */
	VK_NONE,                       /* KEY_VIDEO_NEXT (drive next video source) (241) */
	VK_NONE,                       /* KEY_VIDEO_PREV (drive previous video source) (242) */
	VK_NONE,                       /* KEY_BRIGHTNESS_CYCLE
	                                * (brightness up, after max is min) (243) */
	VK_NONE,                       /* KEY_BRIGHTNESS_AUTO, KEY_BRIGHTNESS_ZERO
	                                * (Set Auto Brightness: manual brightness control is off,
	                                *  rely on ambient) (244) */
	VK_NONE,                       /* KEY_DISPLAY_OFF (display device to off state) (245) */
	VK_NONE,                       /* KEY_WWAN, KEY_WIMAX
	                                * (Wireless WAN (LTE, UMTS, GSM, etc.)) (246) */
	VK_NONE,                       /* KEY_RFKILL (Key that controls all radios) (247) */
	VK_NONE,                       /* KEY_MICMUTE (Mute / unmute the microphone) (248) */
	VK_NONE,                       /* (249) */
	VK_NONE,                       /* (250) */
	VK_NONE,                       /* (251) */
	VK_NONE,                       /* (252) */
	VK_NONE,                       /* (253) */
	VK_NONE,                       /* (254) */
	VK_NONE,                       /* (255) */
};

/**
 * XKB
 *
 * Refer to X Keyboard Configuration Database:
 * http://www.freedesktop.org/wiki/Software/XKeyboardConfig
 */

/* TODO: Finish Japanese Keyboard */

static DWORD KEYCODE_TO_VKCODE_XKB[256] = {
	VK_NONE,       /* 0 */
	VK_NONE,       /* 1 */
	VK_NONE,       /* 2 */
	VK_NONE,       /* 3 */
	VK_NONE,       /* 4 */
	VK_NONE,       /* 5 */
	VK_NONE,       /* 6 */
	VK_NONE,       /* 7 */
	VK_NONE,       /* 8 */
	VK_ESCAPE,     /* <ESC> 9 */
	VK_KEY_1,      /* <AE01> 10 */
	VK_KEY_2,      /* <AE02> 11 */
	VK_KEY_3,      /* <AE03> 12 */
	VK_KEY_4,      /* <AE04> 13 */
	VK_KEY_5,      /* <AE05> 14 */
	VK_KEY_6,      /* <AE06> 15 */
	VK_KEY_7,      /* <AE07> 16 */
	VK_KEY_8,      /* <AE08> 17 */
	VK_KEY_9,      /* <AE09> 18 */
	VK_KEY_0,      /* <AE10> 19 */
	VK_OEM_MINUS,  /* <AE11> 20 */
	VK_OEM_PLUS,   /* <AE12> 21 */
	VK_BACK,       /* <BKSP> 22 */
	VK_TAB,        /* <TAB> 23 */
	VK_KEY_Q,      /* <AD01> 24 */
	VK_KEY_W,      /* <AD02> 25 */
	VK_KEY_E,      /* <AD03> 26 */
	VK_KEY_R,      /* <AD04> 27 */
	VK_KEY_T,      /* <AD05> 28 */
	VK_KEY_Y,      /* <AD06> 29 */
	VK_KEY_U,      /* <AD07> 30 */
	VK_KEY_I,      /* <AD08> 31 */
	VK_KEY_O,      /* <AD09> 32 */
	VK_KEY_P,      /* <AD10> 33 */
	VK_OEM_4,      /* <AD11> 34 */
	VK_OEM_6,      /* <AD12> 35 */
	VK_RETURN,     /* <RTRN> 36 */
	VK_LCONTROL,   /* <LCTL> 37 */
	VK_KEY_A,      /* <AC01> 38 */
	VK_KEY_S,      /* <AC02> 39 */
	VK_KEY_D,      /* <AC03> 40 */
	VK_KEY_F,      /* <AC04> 41 */
	VK_KEY_G,      /* <AC05> 42 */
	VK_KEY_H,      /* <AC06> 43 */
	VK_KEY_J,      /* <AC07> 44 */
	VK_KEY_K,      /* <AC08> 45 */
	VK_KEY_L,      /* <AC09> 46 */
	VK_OEM_1,      /* <AC10> 47 */
	VK_OEM_7,      /* <AC11> 48 */
	VK_OEM_3,      /* <TLDE> 49 */
	VK_LSHIFT,     /* <LFSH> 50 */
	VK_OEM_5,      /* <BKSL> <AC12> 51 */
	VK_KEY_Z,      /* <AB01> 52 */
	VK_KEY_X,      /* <AB02> 53 */
	VK_KEY_C,      /* <AB03> 54 */
	VK_KEY_V,      /* <AB04> 55 */
	VK_KEY_B,      /* <AB05> 56 */
	VK_KEY_N,      /* <AB06> 57 */
	VK_KEY_M,      /* <AB07> 58 */
	VK_OEM_COMMA,  /* <AB08> 59 */
	VK_OEM_PERIOD, /* <AB09> 60 */
	VK_OEM_2,      /* <AB10> 61 */
	VK_RSHIFT,     /* <RTSH> 62 */
	VK_MULTIPLY,   /* <KPMU> 63 */
	VK_LMENU,      /* <LALT> 64 */
	VK_SPACE,      /* <SPCE> 65 */
	VK_CAPITAL,    /* <CAPS> 66 */
	VK_F1,         /* <FK01> 67 */
	VK_F2,         /* <FK02> 68 */
	VK_F3,         /* <FK03> 69 */
	VK_F4,         /* <FK04> 70 */
	VK_F5,         /* <FK05> 71 */
	VK_F6,         /* <FK06> 72 */
	VK_F7,         /* <FK07> 73 */
	VK_F8,         /* <FK08> 74 */
	VK_F9,         /* <FK09> 75 */
	VK_F10,        /* <FK10> 76 */
	VK_NUMLOCK,    /* <NMLK> 77 */
	VK_SCROLL,     /* <SCLK> 78 */
	VK_NUMPAD7,    /* <KP7> 79 */
	VK_NUMPAD8,    /* <KP8> 80 */
	VK_NUMPAD9,    /* <KP9> 81 */
	VK_SUBTRACT,   /* <KPSU> 82 */
	VK_NUMPAD4,    /* <KP4> 83 */
	VK_NUMPAD5,    /* <KP5> 84 */
	VK_NUMPAD6,    /* <KP6> 85 */
	VK_ADD,        /* <KPAD> 86 */
	VK_NUMPAD1,    /* <KP1> 87 */
	VK_NUMPAD2,    /* <KP2> 88 */
	VK_NUMPAD3,    /* <KP3> 89 */
	VK_NUMPAD0,    /* <KP0> 90 */
	VK_DECIMAL,    /* <KPDL> 91 */
	VK_NONE,       /* <LVL3> 92 */
	VK_NONE,       /* 93 */
	VK_OEM_102,    /* <LSGT> 94 */
	VK_F11,        /* <FK11> 95 */
	VK_F12,        /* <FK12> 96 */
#ifdef __sun
	VK_HOME | KBDEXT,   /* <HOME> 97 */
	VK_UP | KBDEXT,     /* <UP> 98 */
	VK_PRIOR | KBDEXT,  /* <PGUP> 99 */
	VK_LEFT | KBDEXT,   /* <LEFT> 100 */
	VK_HKTG,            /* <HKTG> 101 */
	VK_RIGHT | KBDEXT,  /* <RGHT> 102 */
	VK_END | KBDEXT,    /* <END> 103 */
	VK_DOWN | KBDEXT,   /* <DOWN> 104 */
	VK_NEXT | KBDEXT,   /* <PGDN> 105 */
	VK_INSERT | KBDEXT, /* <INS> 106 */
	VK_DELETE | KBDEXT, /* <DELE> 107 */
	VK_RETURN | KBDEXT, /* <KPEN> 108 */
#else
	VK_ABNT_C1,           /* <AB11> 97 */
	VK_DBE_KATAKANA,      /* <KATA> 98 */
	VK_DBE_HIRAGANA,      /* <HIRA> 99 */
	VK_CONVERT,           /* <HENK> 100 */
	VK_HKTG,              /* <HKTG> 101 */
	VK_NONCONVERT,        /* <MUHE> 102 */
	VK_NONE,              /* <JPCM> 103 */
	VK_RETURN | KBDEXT,   /* <KPEN> 104 */
	VK_RCONTROL | KBDEXT, /* <RCTL> 105 */
	VK_DIVIDE | KBDEXT,   /* <KPDV> 106 */
	VK_SNAPSHOT | KBDEXT, /* <PRSC> 107 */
	VK_RMENU | KBDEXT,    /* <RALT> <ALGR> 108 */
#endif
	VK_NONE,                 /* <LNFD> KEY_LINEFEED 109 */
	VK_HOME | KBDEXT,        /* <HOME> 110 */
	VK_UP | KBDEXT,          /* <UP> 111 */
	VK_PRIOR | KBDEXT,       /* <PGUP> 112 */
	VK_LEFT | KBDEXT,        /* <LEFT> 113 */
	VK_RIGHT | KBDEXT,       /* <RGHT> 114 */
	VK_END | KBDEXT,         /* <END> 115 */
	VK_DOWN | KBDEXT,        /* <DOWN> 116 */
	VK_NEXT | KBDEXT,        /* <PGDN> 117 */
	VK_INSERT | KBDEXT,      /* <INS> 118 */
	VK_DELETE | KBDEXT,      /* <DELE> 119 */
	VK_NONE,                 /* <I120> KEY_MACRO 120 */
	VK_VOLUME_MUTE | KBDEXT, /* <MUTE> 121 */
	VK_VOLUME_DOWN | KBDEXT, /* <VOL-> 122 */
	VK_VOLUME_UP | KBDEXT,   /* <VOL+> 123 */
	VK_NONE,                 /* <POWR> 124 */
	VK_NONE,                 /* <KPEQ> 125 */
	VK_NONE,                 /* <I126> KEY_KPPLUSMINUS 126 */
	VK_PAUSE | KBDEXT,       /* <PAUS> 127 */
	VK_NONE,                 /* <I128> KEY_SCALE 128 */
	VK_ABNT_C2,              /* <I129> <KPPT> KEY_KPCOMMA 129 */
	VK_HANGUL,               /* <HNGL> 130 */
	VK_HANJA,                /* <HJCV> 131 */
	VK_OEM_8,                /* <AE13> 132 */
	VK_LWIN | KBDEXT,        /* <LWIN> <LMTA> 133 */
	VK_RWIN | KBDEXT,        /* <RWIN> <RMTA> 134 */
	VK_APPS | KBDEXT,        /* <COMP> <MENU> 135 */
	VK_NONE,                 /* <STOP> 136 */
	VK_NONE,
	/* <AGAI> 137 */               // codespell:ignore
	VK_NONE,                       /* <PROP> 138 */
	VK_NONE,                       /* <UNDO> 139 */
	VK_NONE,                       /* <FRNT> 140 */
	VK_NONE,                       /* <COPY> 141 */
	VK_NONE,                       /* <OPEN> 142 */
	VK_NONE,                       /* <PAST> 143 */
	VK_NONE,                       /* <FIND> 144 */
	VK_NONE,                       /* <CUT> 145 */
	VK_HELP,                       /* <HELP> 146 */
	VK_APPS | KBDEXT,              /* <I147> KEY_MENU 147 */
	VK_NONE,                       /* <I148> KEY_CALC 148 */
	VK_NONE,                       /* <I149> KEY_SETUP 149 */
	VK_SLEEP,                      /* <I150> KEY_SLEEP 150 */
	VK_NONE,                       /* <I151> KEY_WAKEUP 151 */
	VK_NONE,                       /* <I152> KEY_FILE 152 */
	VK_NONE,                       /* <I153> KEY_SEND 153 */
	VK_NONE,                       /* <I154> KEY_DELETEFILE 154 */
	VK_CONVERT,                    /* <I155> KEY_XFER 155 */
	VK_LAUNCH_APP1,                /* <I156> KEY_PROG1 156 */
	VK_LAUNCH_APP2,                /* <I157> KEY_PROG2 157 */
	VK_NONE,                       /* <I158> KEY_WWW 158 */
	VK_NONE,                       /* <I159> KEY_MSDOS 159 */
	VK_NONE,                       /* <I160> KEY_COFFEE 160 */
	VK_NONE,                       /* <I161> KEY_DIRECTION 161 */
	VK_NONE,                       /* <I162> KEY_CYCLEWINDOWS 162 */
	VK_LAUNCH_MAIL | KBDEXT,       /* <I163> KEY_MAIL 163 */
	VK_BROWSER_FAVORITES | KBDEXT, /* <I164> KEY_BOOKMARKS 164 */
	VK_NONE,                       /* <I165> KEY_COMPUTER 165 */
	VK_BROWSER_BACK | KBDEXT,      /* <I166> KEY_BACK 166 */
	VK_BROWSER_FORWARD | KBDEXT,   /* <I167> KEY_FORWARD 167 */
	VK_NONE,                       /* <I168> KEY_CLOSECD 168 */
	VK_NONE,                       /* <I169> KEY_EJECTCD 169 */
	VK_NONE,                       /* <I170> KEY_EJECTCLOSECD 170 */
	VK_MEDIA_NEXT_TRACK | KBDEXT,  /* <I171> KEY_NEXTSONG 171 */
	VK_MEDIA_PLAY_PAUSE | KBDEXT,  /* <I172> KEY_PLAYPAUSE 172 */
	VK_MEDIA_PREV_TRACK | KBDEXT,  /* <I173> KEY_PREVIOUSSONG 173 */
	VK_MEDIA_STOP | KBDEXT,        /* <I174> KEY_STOPCD 174 */
	VK_NONE,                       /* <I175> KEY_RECORD 175 */
	VK_NONE,                       /* <I176> KEY_REWIND 176 */
	VK_NONE,                       /* <I177> KEY_PHONE 177 */
	VK_NONE,                       /* <I178> KEY_ISO 178 */
	VK_NONE,                       /* <I179> KEY_CONFIG 179 */
	VK_BROWSER_HOME | KBDEXT,      /* <I180> KEY_HOMEPAGE 180 */
	VK_BROWSER_REFRESH | KBDEXT,   /* <I181> KEY_REFRESH 181 */
	VK_NONE,                       /* <I182> KEY_EXIT 182 */
	VK_NONE,                       /* <I183> KEY_MOVE 183 */
	VK_NONE,                       /* <I184> KEY_EDIT 184 */
	VK_NONE,                       /* <I185> KEY_SCROLLUP 185 */
	VK_NONE,                       /* <I186> KEY_SCROLLDOWN 186 */
	VK_NONE,                       /* <I187> KEY_KPLEFTPAREN 187 */
	VK_NONE,                       /* <I188> KEY_KPRIGHTPAREN 188 */
	VK_NONE,                       /* <I189> KEY_NEW 189 */
	VK_NONE,                       /* <I190> KEY_REDO 190 */
	VK_F13,                        /* <FK13> 191 */
	VK_F14,                        /* <FK14> 192 */
	VK_F15,                        /* <FK15> 193 */
	VK_F16,                        /* <FK16> 194 */
	VK_F17,                        /* <FK17> 195 */
	VK_F18,                        /* <FK18> 196 */
	VK_F19,                        /* <FK19> 197 */
	VK_F20,                        /* <FK20> 198 */
	VK_F21,                        /* <FK21> 199 */
	VK_F22,                        /* <FK22> 200 */
	VK_F23,                        /* <FK23> 201 */
	VK_F24,                        /* <FK24> 202 */
	VK_NONE,                       /* <MDSW> 203 */
	VK_NONE,                       /* <ALT> 204 */
	VK_NONE,                       /* <META> 205 */
	VK_LWIN,                       /* <SUPR> 206 */
	VK_NONE,                       /* <HYPR> 207 */
	VK_PLAY,                       /* <I208> KEY_PLAYCD 208 */
	VK_PAUSE,                      /* <I209> KEY_PAUSECD 209 */
	VK_NONE,                       /* <I210> KEY_PROG3 210 */
	VK_NONE,                       /* <I211> KEY_PROG4 211 */
	VK_NONE,                       /* <I212> KEY_DASHBOARD 212 */
	VK_NONE,                       /* <I213> KEY_SUSPEND 213 */
	VK_NONE,                       /* <I214> KEY_CLOSE 214 */
	VK_PLAY,                       /* <I215> KEY_PLAY 215 */
	VK_NONE,                       /* <I216> KEY_FASTFORWARD 216 */
	VK_NONE,                       /* <I217> KEY_BASSBOOST 217 */
	VK_PRINT | KBDEXT,             /* <I218> KEY_PRINT 218 */
	VK_NONE,                       /* <I219> KEY_HP 219 */
	VK_NONE,                       /* <I220> KEY_CAMERA 220 */
	VK_NONE,                       /* <I221> KEY_SOUND 221 */
	VK_NONE,                       /* <I222> KEY_QUESTION 222 */
	VK_NONE,                       /* <I223> KEY_EMAIL 223 */
	VK_NONE,                       /* <I224> KEY_CHAT 224 */
	VK_BROWSER_SEARCH | KBDEXT,    /* <I225> KEY_SEARCH 225 */
	VK_NONE,                       /* <I226> KEY_CONNECT 226 */
	VK_NONE,                       /* <I227> KEY_FINANCE 227 */
	VK_NONE,                       /* <I228> KEY_SPORT 228 */
	VK_NONE,                       /* <I229> KEY_SHOP 229 */
	VK_NONE,                       /* <I230> KEY_ALTERASE 230 */
	VK_NONE,                       /* <I231> KEY_CANCEL 231 */
	VK_NONE,                       /* <I232> KEY_BRIGHTNESSDOWN 232 */
	VK_NONE,                       /* <I233> KEY_BRIGHTNESSUP 233 */
	VK_NONE,                       /* <I234> KEY_MEDIA 234 */
	VK_NONE,                       /* <I235> KEY_SWITCHVIDEOMODE 235 */
	VK_NONE,                       /* <I236> KEY_KBDILLUMTOGGLE 236 */
	VK_NONE,                       /* <I237> KEY_KBDILLUMDOWN 237 */
	VK_NONE,                       /* <I238> KEY_KBDILLUMUP 238 */
	VK_NONE,                       /* <I239> KEY_SEND 239 */
	VK_NONE,                       /* <I240> KEY_REPLY 240 */
	VK_NONE,                       /* <I241> KEY_FORWARDMAIL 241 */
	VK_NONE,                       /* <I242> KEY_SAVE 242 */
	VK_NONE,                       /* <I243> KEY_DOCUMENTS 243 */
	VK_NONE,                       /* <I244> KEY_BATTERY 244 */
	VK_NONE,                       /* <I245> KEY_BLUETOOTH 245 */
	VK_NONE,                       /* <I246> KEY_WLAN 246 */
	VK_NONE,                       /* <I247> KEY_UWB 247 */
	VK_NONE,                       /* <I248> KEY_UNKNOWN 248 */
	VK_NONE,                       /* <I249> KEY_VIDEO_NEXT 249 */
	VK_NONE,                       /* <I250> KEY_VIDEO_PREV 250 */
	VK_NONE,                       /* <I251> KEY_BRIGHTNESS_CYCLE 251 */
	VK_NONE,                       /* <I252> KEY_BRIGHTNESS_ZERO 252 */
	VK_NONE,                       /* <I253> KEY_DISPLAY_OFF 253 */
	VK_NONE,                       /* 254 */
	VK_NONE                        /* 255 */
};

DWORD GetVirtualKeyCodeFromKeycode(DWORD keycode, WINPR_KEYCODE_TYPE type)
{
	DWORD vkcode = VK_NONE;

	switch (type)
	{
		case WINPR_KEYCODE_TYPE_APPLE:
			if (keycode < 0xFF)
				vkcode = KEYCODE_TO_VKCODE_APPLE[keycode & 0xFF];
			break;
		case WINPR_KEYCODE_TYPE_EVDEV:
			if (keycode < 0xFF)
				vkcode = KEYCODE_TO_VKCODE_EVDEV[keycode & 0xFF];
			break;
		case WINPR_KEYCODE_TYPE_XKB:
			if (keycode < 0xFF)
				vkcode = KEYCODE_TO_VKCODE_XKB[keycode & 0xFF];
			break;
		default:
			break;
	}

	WINPR_ASSERT(vkcode != 0);

	return vkcode;
}

static DWORD GetKeycodeFromVirtualKeyCodeWithOffset(DWORD offset, DWORD keycode,
                                                    WINPR_KEYCODE_TYPE type)
{
	DWORD* targetArray = nullptr;
	size_t targetSize = 0;

	if (keycode == VK_NONE)
		return 0;

	switch (type)
	{
		case WINPR_KEYCODE_TYPE_APPLE:
			targetArray = KEYCODE_TO_VKCODE_APPLE;
			targetSize = ARRAYSIZE(KEYCODE_TO_VKCODE_APPLE);
			break;
		case WINPR_KEYCODE_TYPE_EVDEV:
			targetArray = KEYCODE_TO_VKCODE_EVDEV;
			targetSize = ARRAYSIZE(KEYCODE_TO_VKCODE_EVDEV);
			break;
		case WINPR_KEYCODE_TYPE_XKB:
			targetArray = KEYCODE_TO_VKCODE_XKB;
			targetSize = ARRAYSIZE(KEYCODE_TO_VKCODE_XKB);
			break;
		default:
			return 0;
	}

	for (DWORD index = offset; index < targetSize; index++)
	{
		if (keycode == targetArray[index])
			return index;
	}

	return 0;
}

DWORD GetKeycodeFromVirtualKeyCode(DWORD keycode, WINPR_KEYCODE_TYPE type)
{
	return GetKeycodeFromVirtualKeyCodeWithOffset(0, keycode, type);
}

WINPR_ATTR_NODISCARD
WINPR_API DWORD GetKeycodesFromVirtualKeyCode(DWORD keycode, WINPR_KEYCODE_TYPE type,
                                              DWORD* pdwKeyCodeBuffer, size_t KeyCodeBufferSize)
{
	DWORD count = 0;
	DWORD kc = 0;
	DWORD offset = 0;

	WINPR_ASSERT(pdwKeyCodeBuffer || (KeyCodeBufferSize == 0));

	const BOOL nobuffer = (KeyCodeBufferSize == 0);
	do
	{
		kc = GetKeycodeFromVirtualKeyCodeWithOffset(offset, keycode, type);
		if (kc != 0)
		{
			if (!nobuffer)
			{
				if (count < KeyCodeBufferSize)
					pdwKeyCodeBuffer[count++] = kc;
			}
			else
				count++;

			offset = kc + 1;
		}
	} while ((kc != 0) && ((count < KeyCodeBufferSize) || nobuffer));
	return count;
}
