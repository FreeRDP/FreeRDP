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
	0, /* 9 */
	0, /* 10 */
	0, /* 11 */
	0, /* 12 */
	0, /* 13 */
	0, /* 14 */
	0, /* 15 */
	0, /* 16 */
	0, /* 17 */
	0, /* 18 */
	0, /* 19 */
	0, /* 20 */
	0, /* 21 */
	0, /* 22 */
	0, /* 23 */
	0, /* 24 */
	0, /* 25 */
	0, /* 26 */
	0, /* 27 */
	0, /* 28 */
	0, /* 29 */
	0, /* 30 */
	0, /* 31 */
	0, /* 32 */
	0, /* 33 */
	0, /* 34 */
	0, /* 35 */
	0, /* 36 */
	0, /* 37 */
	0, /* 38 */
	0, /* 39 */
	0, /* 40 */
	0, /* 41 */
	0, /* 42 */
	0, /* 43 */
	0, /* 44 */
	0, /* 45 */
	0, /* 46 */
	0, /* 47 */
	0, /* 48 */
	0, /* 49 */
	0, /* 50 */
	0, /* 51 */
	0, /* 52 */
	0, /* 53 */
	0, /* 54 */
	0, /* 55 */
	0, /* 56 */
	0, /* 57 */
	0, /* 58 */
	0, /* 59 */
	0, /* 60 */
	0, /* 61 */
	0, /* 62 */
	0, /* 63 */
	0, /* 64 */
	0, /* 65 */
	0, /* 66 */
	0, /* 67 */
	0, /* 68 */
	0, /* 69 */
	0, /* 70 */
	0, /* 71 */
	0, /* 72 */
	0, /* 73 */
	0, /* 74 */
	0, /* 75 */
	0, /* 76 */
	0, /* 77 */
	0, /* 78 */
	0, /* 79 */
	0, /* 80 */
	0, /* 81 */
	0, /* 82 */
	0, /* 83 */
	0, /* 84 */
	0, /* 85 */
	0, /* 86 */
	0, /* 87 */
	0, /* 88 */
	0, /* 89 */
	0, /* 90 */
	0, /* 91 */
	0, /* 92 */
	0, /* 93 */
	0, /* 94 */
	0, /* 95 */
	0, /* 96 */
	0, /* 97 */
	0, /* 98 */
	0, /* 99 */
	0, /* 100 */
	0, /* 101 */
	0, /* 102 */
	0, /* 103 */
	0, /* 104 */
	0, /* 105 */
	0, /* 106 */
	0, /* 107 */
	0, /* 108 */
	0, /* 109 */
	0, /* 110 */
	0, /* 111 */
	0, /* 112 */
	0, /* 113 */
	0, /* 114 */
	0, /* 115 */
	0, /* 116 */
	0, /* 117 */
	0, /* 118 */
	0, /* 119 */
	0, /* 120 */
	0, /* 121 */
	0, /* 122 */
	0, /* 123 */
	0, /* 124 */
	0, /* 125 */
	0, /* 126 */
	0, /* 127 */
	0, /* 128 */
	0, /* 129 */
	0, /* 130 */
	0, /* 131 */
	0, /* 132 */
	0, /* 133 */
	0, /* 134 */
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
