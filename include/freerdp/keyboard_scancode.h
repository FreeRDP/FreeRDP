/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP protocol "scancodes"
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

#ifndef __FREERDP_LOCALE_KEYBOARD_RDP_SCANCODE_H
#define __FREERDP_LOCALE_KEYBOARD_RDP_SCANCODE_H

/* @msdn{cc240584} says:
 * "... (a scancode is an 8-bit value specifying a key location on the keyboard).
 * The server accepts a scancode value and translates it into the correct character depending on the language locale and keyboard layout used in the session."
 * The 8-bit value is later called "keyCode"
 * The extended flag is for all practical an important 9th bit with a strange encoding - not just a modifier.
 */

typedef uint32 RDP_SCANCODE;	/* Our own representation of a RDP protocol scancode */
#define rdp_scancode_code(_rdp_scancode) ((uint8)(_rdp_scancode & 0xff))
#define rdp_scancode_extended(_rdp_scancode) (((_rdp_scancode) & 0x100) ? true : false)
#define mk_rdp_scancode(_code, _extended) (((_code) & 0xff) | ((_extended) ? 0x100 : 0))


/* Defines for known RDP_SCANCODE protocol values.
 * Mostly the same as the undocumented PKBDLLHOOKSTRUCT scanCode, "A hardware scan code for the key", @msdn{ms644967} */

#define RDP_SCANCODE_UNKNOWN           mk_rdp_scancode(0x00, false)
/* #define RDP_SCANCODE_LBUTTON                  VK_LBUTTON */
/* #define RDP_SCANCODE_RBUTTON                  VK_RBUTTON */
/* #define RDP_SCANCODE_CANCEL                   VK_CANCEL */
/* #define RDP_SCANCODE_MBUTTON                  VK_MBUTTON */
/* #define RDP_SCANCODE_XBUTTON1                 VK_XBUTTON1 */
/* #define RDP_SCANCODE_XBUTTON2                 VK_XBUTTON2 */
#define RDP_SCANCODE_BACK              mk_rdp_scancode(0x0E, false) /* VK_BACK */
#define RDP_SCANCODE_TAB               mk_rdp_scancode(0x0F, false) /* VK_TAB */
/* #define RDP_SCANCODE_CLEAR                    VK_CLEAR */
#define RDP_SCANCODE_RETURN            mk_rdp_scancode(0x1C, false) /* VK_RETURN */
#define RDP_SCANCODE_SHIFT             mk_rdp_scancode(0x2A, false) /* VK_SHIFT */
/* #define RDP_SCANCODE_CONTROL                  VK_CONTROL */
#define RDP_SCANCODE_MENU              mk_rdp_scancode(0x38, false) /* VK_MENU */
#define RDP_SCANCODE_PAUSE             mk_rdp_scancode(0x46, true)  /* VK_PAUSE */
#define RDP_SCANCODE_CAPITAL           mk_rdp_scancode(0x3A, false) /* VK_CAPITAL */
#define RDP_SCANCODE_KANA / VK_HANGUL  mk_rdp_scancode(0x72, false) /* VK_KANA / VK_HANGUL */
/* #define RDP_SCANCODE_JUNJA                    VK_JUNJA */
/* #define RDP_SCANCODE_FINAL                    VK_FINAL */
#define RDP_SCANCODE_HANJA / VK_KANJI  mk_rdp_scancode(0x71, false) /* VK_HANJA / VK_KANJI */
#define RDP_SCANCODE_ESCAPE            mk_rdp_scancode(0x01, false) /* VK_ESCAPE */
/* #define RDP_SCANCODE_CONVERT                  VK_CONVERT */
/* #define RDP_SCANCODE_NONCONVERT               VK_NONCONVERT */
/* #define RDP_SCANCODE_ACCEPT                   VK_ACCEPT */
/* #define RDP_SCANCODE_MODECHANGE               VK_MODECHANGE */
#define RDP_SCANCODE_SPACE             mk_rdp_scancode(0x39, false) /* VK_SPACE */
#define RDP_SCANCODE_PRIOR             mk_rdp_scancode(0x49, true)  /* VK_PRIOR */
#define RDP_SCANCODE_NEXT              mk_rdp_scancode(0x51, true)  /* VK_NEXT */
#define RDP_SCANCODE_END               mk_rdp_scancode(0x4F, true)  /* VK_END */
#define RDP_SCANCODE_HOME              mk_rdp_scancode(0x47, true)  /* VK_HOME */
#define RDP_SCANCODE_LEFT              mk_rdp_scancode(0x4B, true)  /* VK_LEFT */
#define RDP_SCANCODE_UP                mk_rdp_scancode(0x48, true)  /* VK_UP */
#define RDP_SCANCODE_RIGHT             mk_rdp_scancode(0x4D, true)  /* VK_RIGHT */
#define RDP_SCANCODE_DOWN              mk_rdp_scancode(0x50, true)  /* VK_DOWN */
/* #define RDP_SCANCODE_SELECT                   VK_SELECT */
#define RDP_SCANCODE_PRINT             mk_rdp_scancode(0x37, true)  /* VK_PRINT */
#define RDP_SCANCODE_EXECUTE           mk_rdp_scancode(0x37, true)  /* VK_EXECUTE */
#define RDP_SCANCODE_SNAPSHOT          mk_rdp_scancode(0x37, true)  /* VK_SNAPSHOT */
#define RDP_SCANCODE_INSERT            mk_rdp_scancode(0x52, true)  /* VK_INSERT */
#define RDP_SCANCODE_DELETE            mk_rdp_scancode(0x53, true)  /* VK_DELETE */
#define RDP_SCANCODE_HELP              mk_rdp_scancode(0x63, false) /* VK_HELP */
#define RDP_SCANCODE_KEY_0             mk_rdp_scancode(0x0B, false) /* VK_KEY_0 */
#define RDP_SCANCODE_KEY_1             mk_rdp_scancode(0x02, false) /* VK_KEY_1 */
#define RDP_SCANCODE_KEY_2             mk_rdp_scancode(0x03, false) /* VK_KEY_2 */
#define RDP_SCANCODE_KEY_3             mk_rdp_scancode(0x04, false) /* VK_KEY_3 */
#define RDP_SCANCODE_KEY_4             mk_rdp_scancode(0x05, false) /* VK_KEY_4 */
#define RDP_SCANCODE_KEY_5             mk_rdp_scancode(0x06, false) /* VK_KEY_5 */
#define RDP_SCANCODE_KEY_6             mk_rdp_scancode(0x07, false) /* VK_KEY_6 */
#define RDP_SCANCODE_KEY_7             mk_rdp_scancode(0x08, false) /* VK_KEY_7 */
#define RDP_SCANCODE_KEY_8             mk_rdp_scancode(0x09, false) /* VK_KEY_8 */
#define RDP_SCANCODE_KEY_9             mk_rdp_scancode(0x0A, false) /* VK_KEY_9 */
#define RDP_SCANCODE_KEY_A             mk_rdp_scancode(0x1E, false) /* VK_KEY_A */
#define RDP_SCANCODE_KEY_B             mk_rdp_scancode(0x30, false) /* VK_KEY_B */
#define RDP_SCANCODE_KEY_C             mk_rdp_scancode(0x2E, false) /* VK_KEY_C */
#define RDP_SCANCODE_KEY_D             mk_rdp_scancode(0x20, false) /* VK_KEY_D */
#define RDP_SCANCODE_KEY_E             mk_rdp_scancode(0x12, false) /* VK_KEY_E */
#define RDP_SCANCODE_KEY_F             mk_rdp_scancode(0x21, false) /* VK_KEY_F */
#define RDP_SCANCODE_KEY_G             mk_rdp_scancode(0x22, false) /* VK_KEY_G */
#define RDP_SCANCODE_KEY_H             mk_rdp_scancode(0x23, false) /* VK_KEY_H */
#define RDP_SCANCODE_KEY_I             mk_rdp_scancode(0x17, false) /* VK_KEY_I */
#define RDP_SCANCODE_KEY_J             mk_rdp_scancode(0x24, false) /* VK_KEY_J */
#define RDP_SCANCODE_KEY_K             mk_rdp_scancode(0x25, false) /* VK_KEY_K */
#define RDP_SCANCODE_KEY_L             mk_rdp_scancode(0x26, false) /* VK_KEY_L */
#define RDP_SCANCODE_KEY_M             mk_rdp_scancode(0x32, false) /* VK_KEY_M */
#define RDP_SCANCODE_KEY_N             mk_rdp_scancode(0x31, false) /* VK_KEY_N */
#define RDP_SCANCODE_KEY_O             mk_rdp_scancode(0x18, false) /* VK_KEY_O */
#define RDP_SCANCODE_KEY_P             mk_rdp_scancode(0x19, false) /* VK_KEY_P */
#define RDP_SCANCODE_KEY_Q             mk_rdp_scancode(0x10, false) /* VK_KEY_Q */
#define RDP_SCANCODE_KEY_R             mk_rdp_scancode(0x13, false) /* VK_KEY_R */
#define RDP_SCANCODE_KEY_S             mk_rdp_scancode(0x1F, false) /* VK_KEY_S */
#define RDP_SCANCODE_KEY_T             mk_rdp_scancode(0x14, false) /* VK_KEY_T */
#define RDP_SCANCODE_KEY_U             mk_rdp_scancode(0x16, false) /* VK_KEY_U */
#define RDP_SCANCODE_KEY_V             mk_rdp_scancode(0x2F, false) /* VK_KEY_V */
#define RDP_SCANCODE_KEY_W             mk_rdp_scancode(0x11, false) /* VK_KEY_W */
#define RDP_SCANCODE_KEY_X             mk_rdp_scancode(0x2D, false) /* VK_KEY_X */
#define RDP_SCANCODE_KEY_Y             mk_rdp_scancode(0x15, false) /* VK_KEY_Y */
#define RDP_SCANCODE_KEY_Z             mk_rdp_scancode(0x2C, false) /* VK_KEY_Z */
#define RDP_SCANCODE_LWIN              mk_rdp_scancode(0x5B, true)  /* VK_LWIN */
#define RDP_SCANCODE_RWIN              mk_rdp_scancode(0x5C, true)  /* VK_RWIN */
#define RDP_SCANCODE_APPS              mk_rdp_scancode(0x5D, true)  /* VK_APPS */
#define RDP_SCANCODE_SLEEP             mk_rdp_scancode(0x5F, false) /* VK_SLEEP */
#define RDP_SCANCODE_NUMPAD0           mk_rdp_scancode(0x52, false) /* VK_NUMPAD0 */
#define RDP_SCANCODE_NUMPAD1           mk_rdp_scancode(0x4F, false) /* VK_NUMPAD1 */
#define RDP_SCANCODE_NUMPAD2           mk_rdp_scancode(0x50, false) /* VK_NUMPAD2 */
#define RDP_SCANCODE_NUMPAD3           mk_rdp_scancode(0x51, false) /* VK_NUMPAD3 */
#define RDP_SCANCODE_NUMPAD4           mk_rdp_scancode(0x4B, false) /* VK_NUMPAD4 */
#define RDP_SCANCODE_NUMPAD5           mk_rdp_scancode(0x4C, false) /* VK_NUMPAD5 */
#define RDP_SCANCODE_NUMPAD6           mk_rdp_scancode(0x4D, false) /* VK_NUMPAD6 */
#define RDP_SCANCODE_NUMPAD7           mk_rdp_scancode(0x47, false) /* VK_NUMPAD7 */
#define RDP_SCANCODE_NUMPAD8           mk_rdp_scancode(0x48, false) /* VK_NUMPAD8 */
#define RDP_SCANCODE_NUMPAD9           mk_rdp_scancode(0x49, false) /* VK_NUMPAD9 */
#define RDP_SCANCODE_MULTIPLY          mk_rdp_scancode(0x37, false) /* VK_MULTIPLY */
#define RDP_SCANCODE_ADD               mk_rdp_scancode(0x4E, false) /* VK_ADD */
/* #define RDP_SCANCODE_SEPARATOR                VK_SEPARATOR */
#define RDP_SCANCODE_SUBTRACT          mk_rdp_scancode(0x4A, false) /* VK_SUBTRACT */
#define RDP_SCANCODE_DECIMAL           mk_rdp_scancode(0x53, false) /* VK_DECIMAL */
#define RDP_SCANCODE_DIVIDE            mk_rdp_scancode(0x35, true)  /* VK_DIVIDE */
#define RDP_SCANCODE_F1                mk_rdp_scancode(0x3B, false) /* VK_F1 */
#define RDP_SCANCODE_F2                mk_rdp_scancode(0x3C, false) /* VK_F2 */
#define RDP_SCANCODE_F3                mk_rdp_scancode(0x3D, false) /* VK_F3 */
#define RDP_SCANCODE_F4                mk_rdp_scancode(0x3E, false) /* VK_F4 */
#define RDP_SCANCODE_F5                mk_rdp_scancode(0x3F, false) /* VK_F5 */
#define RDP_SCANCODE_F6                mk_rdp_scancode(0x40, false) /* VK_F6 */
#define RDP_SCANCODE_F7                mk_rdp_scancode(0x41, false) /* VK_F7 */
#define RDP_SCANCODE_F8                mk_rdp_scancode(0x42, false) /* VK_F8 */
#define RDP_SCANCODE_F9                mk_rdp_scancode(0x43, false) /* VK_F9 */
#define RDP_SCANCODE_F10               mk_rdp_scancode(0x44, false) /* VK_F10 */
#define RDP_SCANCODE_F11               mk_rdp_scancode(0x57, false) /* VK_F11 */
#define RDP_SCANCODE_F12               mk_rdp_scancode(0x58, false) /* VK_F12 */
#define RDP_SCANCODE_F13               mk_rdp_scancode(0x64, false) /* VK_F13 */
#define RDP_SCANCODE_F14               mk_rdp_scancode(0x65, false) /* VK_F14 */
#define RDP_SCANCODE_F15               mk_rdp_scancode(0x66, false) /* VK_F15 */
#define RDP_SCANCODE_F16               mk_rdp_scancode(0x67, false) /* VK_F16 */
#define RDP_SCANCODE_F17               mk_rdp_scancode(0x68, false) /* VK_F17 */
#define RDP_SCANCODE_F18               mk_rdp_scancode(0x69, false) /* VK_F18 */
#define RDP_SCANCODE_F19               mk_rdp_scancode(0x6A, false) /* VK_F19 */
#define RDP_SCANCODE_F20               mk_rdp_scancode(0x6B, false) /* VK_F20 */
#define RDP_SCANCODE_F21               mk_rdp_scancode(0x6C, false) /* VK_F21 */
#define RDP_SCANCODE_F22               mk_rdp_scancode(0x6D, false) /* VK_F22 */
#define RDP_SCANCODE_F23               mk_rdp_scancode(0x6E, false) /* VK_F23 */
#define RDP_SCANCODE_F24               mk_rdp_scancode(0x6F, false) /* VK_F24 */
#define RDP_SCANCODE_NUMLOCK           mk_rdp_scancode(0x45, false) /* VK_NUMLOCK */ /* Note: when this seems to appear in PKBDLLHOOKSTRUCT it means Pause which must be sent as Ctrl + NumLock */
#define RDP_SCANCODE_SCROLL            mk_rdp_scancode(0x46, false) /* VK_SCROLL */
#define RDP_SCANCODE_LSHIFT            mk_rdp_scancode(0x2A, false) /* VK_LSHIFT */
#define RDP_SCANCODE_RSHIFT            mk_rdp_scancode(0x36, false) /* VK_RSHIFT */
#define RDP_SCANCODE_LCONTROL          mk_rdp_scancode(0x1D, false) /* VK_LCONTROL */
#define RDP_SCANCODE_RCONTROL          mk_rdp_scancode(0x1D, true)  /* VK_RCONTROL */
#define RDP_SCANCODE_LMENU             mk_rdp_scancode(0x38, false) /* VK_LMENU */
#define RDP_SCANCODE_RMENU             mk_rdp_scancode(0x38, true)  /* VK_RMENU */
/* #define RDP_SCANCODE_BROWSER_BACK             VK_BROWSER_BACK */
/* #define RDP_SCANCODE_BROWSER_FORWARD          VK_BROWSER_FORWARD */
/* #define RDP_SCANCODE_BROWSER_REFRESH          VK_BROWSER_REFRESH */
/* #define RDP_SCANCODE_BROWSER_STOP             VK_BROWSER_STOP */
/* #define RDP_SCANCODE_BROWSER_SEARCH           VK_BROWSER_SEARCH */
/* #define RDP_SCANCODE_BROWSER_FAVORITES        VK_BROWSER_FAVORITES */
/* #define RDP_SCANCODE_BROWSER_HOME             VK_BROWSER_HOME */
/* #define RDP_SCANCODE_VOLUME_MUTE              VK_VOLUME_MUTE */
/* #define RDP_SCANCODE_VOLUME_DOWN              VK_VOLUME_DOWN */
/* #define RDP_SCANCODE_VOLUME_UP                VK_VOLUME_UP */
/* #define RDP_SCANCODE_MEDIA_NEXT_TRACK         VK_MEDIA_NEXT_TRACK */
/* #define RDP_SCANCODE_MEDIA_PREV_TRACK         VK_MEDIA_PREV_TRACK */
/* #define RDP_SCANCODE_MEDIA_STOP               VK_MEDIA_STOP */
/* #define RDP_SCANCODE_MEDIA_PLAY_PAUSE         VK_MEDIA_PLAY_PAUSE */
/* #define RDP_SCANCODE_LAUNCH_MAIL              VK_LAUNCH_MAIL */
/* #define RDP_SCANCODE_MEDIA_SELECT             VK_MEDIA_SELECT */
/* #define RDP_SCANCODE_LAUNCH_APP1              VK_LAUNCH_APP1 */
/* #define RDP_SCANCODE_LAUNCH_APP2              VK_LAUNCH_APP2 */
#define RDP_SCANCODE_OEM_1             mk_rdp_scancode(0x27, false) /* VK_OEM_1 */
#define RDP_SCANCODE_OEM_PLUS          mk_rdp_scancode(0x0D, false) /* VK_OEM_PLUS */
#define RDP_SCANCODE_OEM_COMMA         mk_rdp_scancode(0x33, false) /* VK_OEM_COMMA */
#define RDP_SCANCODE_OEM_MINUS         mk_rdp_scancode(0x0C, false) /* VK_OEM_MINUS */
#define RDP_SCANCODE_OEM_PERIOD        mk_rdp_scancode(0x34, false) /* VK_OEM_PERIOD */
#define RDP_SCANCODE_OEM_2             mk_rdp_scancode(0x35, false) /* VK_OEM_2 */
#define RDP_SCANCODE_OEM_3             mk_rdp_scancode(0x29, false) /* VK_OEM_3 */
#define RDP_SCANCODE_ABNT_C1           mk_rdp_scancode(0x73, false) /* VK_ABNT_C1 */
#define RDP_SCANCODE_ABNT_C2           mk_rdp_scancode(0x7E, false) /* VK_ABNT_C2 */
#define RDP_SCANCODE_OEM_4             mk_rdp_scancode(0x1A, false) /* VK_OEM_4 */
#define RDP_SCANCODE_OEM_5             mk_rdp_scancode(0x2B, false) /* VK_OEM_5 */
#define RDP_SCANCODE_OEM_6             mk_rdp_scancode(0x1B, false) /* VK_OEM_6 */
#define RDP_SCANCODE_OEM_7             mk_rdp_scancode(0x28, false) /* VK_OEM_7 */
#define RDP_SCANCODE_OEM_8             mk_rdp_scancode(0x1D, false) /* VK_OEM_8 */
#define RDP_SCANCODE_OEM_102           mk_rdp_scancode(0x56, false) /* VK_OEM_102 */
/* #define RDP_SCANCODE_PROCESSKEY               VK_PROCESSKEY */
/* #define RDP_SCANCODE_PACKET                   VK_PACKET */
/* #define RDP_SCANCODE_ATTN                     VK_ATTN */
/* #define RDP_SCANCODE_CRSEL                    VK_CRSEL */
/* #define RDP_SCANCODE_EXSEL                    VK_EXSEL */
/* #define RDP_SCANCODE_EREOF                    VK_EREOF */
/* #define RDP_SCANCODE_PLAY                     VK_PLAY */
#define RDP_SCANCODE_ZOOM              mk_rdp_scancode(0x62, false) /* VK_ZOOM */
/* #define RDP_SCANCODE_NONAME                   VK_NONAME */
/* #define RDP_SCANCODE_PA1                      VK_PA1 */
/* #define RDP_SCANCODE_OEM_CLEAR                VK_OEM_CLEAR */

/* _not_ valid scancode, but this is what a windows PKBDLLHOOKSTRUCT for NumLock contains */
#define RDP_SCANCODE_NUMLOCK_EXTENDED  mk_rdp_scancode(0x45, true)  /* should be RDP_SCANCODE_NUMLOCK */
#define RDP_SCANCODE_RSHIFT_EXTENDED   mk_rdp_scancode(0x36, true)  /* should be RDP_SCANCODE_RSHIFT */

#endif /* __FREERDP_LOCALE_KEYBOARD_RDP_SCANCODE_H */
