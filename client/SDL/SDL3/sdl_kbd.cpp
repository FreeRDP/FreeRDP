/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP SDL keyboard helper
 *
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
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

#include "sdl_kbd.hpp"
#include "sdl_disp.hpp"
#include "sdl_freerdp.hpp"
#include "sdl_utils.hpp"
#include "sdl_prefs.hpp"

#include <SDL3/SDL_oldnames.h>

#include <map>

#include <freerdp/utils/string.h>
#include <freerdp/scancode.h>

#include <freerdp/log.h>
#define TAG CLIENT_TAG("SDL.kbd")

typedef struct
{
	Uint32 sdl;
	const char* sdl_name;
	UINT32 rdp;
	const char* rdp_name;
} scancode_entry_t;

#define STR(x) #x
#define ENTRY(x, y)      \
	{                    \
		x, STR(x), y, #y \
	}
static const scancode_entry_t map[] = {
	ENTRY(SDL_SCANCODE_UNKNOWN, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_A, RDP_SCANCODE_KEY_A),
	ENTRY(SDL_SCANCODE_B, RDP_SCANCODE_KEY_B),
	ENTRY(SDL_SCANCODE_C, RDP_SCANCODE_KEY_C),
	ENTRY(SDL_SCANCODE_D, RDP_SCANCODE_KEY_D),
	ENTRY(SDL_SCANCODE_E, RDP_SCANCODE_KEY_E),
	ENTRY(SDL_SCANCODE_F, RDP_SCANCODE_KEY_F),
	ENTRY(SDL_SCANCODE_G, RDP_SCANCODE_KEY_G),
	ENTRY(SDL_SCANCODE_H, RDP_SCANCODE_KEY_H),
	ENTRY(SDL_SCANCODE_I, RDP_SCANCODE_KEY_I),
	ENTRY(SDL_SCANCODE_J, RDP_SCANCODE_KEY_J),
	ENTRY(SDL_SCANCODE_K, RDP_SCANCODE_KEY_K),
	ENTRY(SDL_SCANCODE_L, RDP_SCANCODE_KEY_L),
	ENTRY(SDL_SCANCODE_M, RDP_SCANCODE_KEY_M),
	ENTRY(SDL_SCANCODE_N, RDP_SCANCODE_KEY_N),
	ENTRY(SDL_SCANCODE_O, RDP_SCANCODE_KEY_O),
	ENTRY(SDL_SCANCODE_P, RDP_SCANCODE_KEY_P),
	ENTRY(SDL_SCANCODE_Q, RDP_SCANCODE_KEY_Q),
	ENTRY(SDL_SCANCODE_R, RDP_SCANCODE_KEY_R),
	ENTRY(SDL_SCANCODE_S, RDP_SCANCODE_KEY_S),
	ENTRY(SDL_SCANCODE_T, RDP_SCANCODE_KEY_T),
	ENTRY(SDL_SCANCODE_U, RDP_SCANCODE_KEY_U),
	ENTRY(SDL_SCANCODE_V, RDP_SCANCODE_KEY_V),
	ENTRY(SDL_SCANCODE_W, RDP_SCANCODE_KEY_W),
	ENTRY(SDL_SCANCODE_X, RDP_SCANCODE_KEY_X),
	ENTRY(SDL_SCANCODE_Y, RDP_SCANCODE_KEY_Y),
	ENTRY(SDL_SCANCODE_Z, RDP_SCANCODE_KEY_Z),
	ENTRY(SDL_SCANCODE_1, RDP_SCANCODE_KEY_1),
	ENTRY(SDL_SCANCODE_2, RDP_SCANCODE_KEY_2),
	ENTRY(SDL_SCANCODE_3, RDP_SCANCODE_KEY_3),
	ENTRY(SDL_SCANCODE_4, RDP_SCANCODE_KEY_4),
	ENTRY(SDL_SCANCODE_5, RDP_SCANCODE_KEY_5),
	ENTRY(SDL_SCANCODE_6, RDP_SCANCODE_KEY_6),
	ENTRY(SDL_SCANCODE_7, RDP_SCANCODE_KEY_7),
	ENTRY(SDL_SCANCODE_8, RDP_SCANCODE_KEY_8),
	ENTRY(SDL_SCANCODE_9, RDP_SCANCODE_KEY_9),
	ENTRY(SDL_SCANCODE_0, RDP_SCANCODE_KEY_0),
	ENTRY(SDL_SCANCODE_RETURN, RDP_SCANCODE_RETURN),
	ENTRY(SDL_SCANCODE_ESCAPE, RDP_SCANCODE_ESCAPE),
	ENTRY(SDL_SCANCODE_BACKSPACE, RDP_SCANCODE_BACKSPACE),
	ENTRY(SDL_SCANCODE_TAB, RDP_SCANCODE_TAB),
	ENTRY(SDL_SCANCODE_SPACE, RDP_SCANCODE_SPACE),
	ENTRY(SDL_SCANCODE_MINUS, RDP_SCANCODE_OEM_MINUS),
	ENTRY(SDL_SCANCODE_CAPSLOCK, RDP_SCANCODE_CAPSLOCK),
	ENTRY(SDL_SCANCODE_F1, RDP_SCANCODE_F1),
	ENTRY(SDL_SCANCODE_F2, RDP_SCANCODE_F2),
	ENTRY(SDL_SCANCODE_F3, RDP_SCANCODE_F3),
	ENTRY(SDL_SCANCODE_F4, RDP_SCANCODE_F4),
	ENTRY(SDL_SCANCODE_F5, RDP_SCANCODE_F5),
	ENTRY(SDL_SCANCODE_F6, RDP_SCANCODE_F6),
	ENTRY(SDL_SCANCODE_F7, RDP_SCANCODE_F7),
	ENTRY(SDL_SCANCODE_F8, RDP_SCANCODE_F8),
	ENTRY(SDL_SCANCODE_F9, RDP_SCANCODE_F9),
	ENTRY(SDL_SCANCODE_F10, RDP_SCANCODE_F10),
	ENTRY(SDL_SCANCODE_F11, RDP_SCANCODE_F11),
	ENTRY(SDL_SCANCODE_F12, RDP_SCANCODE_F12),
	ENTRY(SDL_SCANCODE_F13, RDP_SCANCODE_F13),
	ENTRY(SDL_SCANCODE_F14, RDP_SCANCODE_F14),
	ENTRY(SDL_SCANCODE_F15, RDP_SCANCODE_F15),
	ENTRY(SDL_SCANCODE_F16, RDP_SCANCODE_F16),
	ENTRY(SDL_SCANCODE_F17, RDP_SCANCODE_F17),
	ENTRY(SDL_SCANCODE_F18, RDP_SCANCODE_F18),
	ENTRY(SDL_SCANCODE_F19, RDP_SCANCODE_F19),
	ENTRY(SDL_SCANCODE_F20, RDP_SCANCODE_F20),
	ENTRY(SDL_SCANCODE_F21, RDP_SCANCODE_F21),
	ENTRY(SDL_SCANCODE_F22, RDP_SCANCODE_F22),
	ENTRY(SDL_SCANCODE_F23, RDP_SCANCODE_F23),
	ENTRY(SDL_SCANCODE_F24, RDP_SCANCODE_F24),
	ENTRY(SDL_SCANCODE_NUMLOCKCLEAR, RDP_SCANCODE_NUMLOCK),
	ENTRY(SDL_SCANCODE_KP_DIVIDE, RDP_SCANCODE_DIVIDE),
	ENTRY(SDL_SCANCODE_KP_MULTIPLY, RDP_SCANCODE_MULTIPLY),
	ENTRY(SDL_SCANCODE_KP_MINUS, RDP_SCANCODE_SUBTRACT),
	ENTRY(SDL_SCANCODE_KP_PLUS, RDP_SCANCODE_ADD),
	ENTRY(SDL_SCANCODE_KP_ENTER, RDP_SCANCODE_RETURN_KP),
	ENTRY(SDL_SCANCODE_KP_1, RDP_SCANCODE_NUMPAD1),
	ENTRY(SDL_SCANCODE_KP_2, RDP_SCANCODE_NUMPAD2),
	ENTRY(SDL_SCANCODE_KP_3, RDP_SCANCODE_NUMPAD3),
	ENTRY(SDL_SCANCODE_KP_4, RDP_SCANCODE_NUMPAD4),
	ENTRY(SDL_SCANCODE_KP_5, RDP_SCANCODE_NUMPAD5),
	ENTRY(SDL_SCANCODE_KP_6, RDP_SCANCODE_NUMPAD6),
	ENTRY(SDL_SCANCODE_KP_7, RDP_SCANCODE_NUMPAD7),
	ENTRY(SDL_SCANCODE_KP_8, RDP_SCANCODE_NUMPAD8),
	ENTRY(SDL_SCANCODE_KP_9, RDP_SCANCODE_NUMPAD9),
	ENTRY(SDL_SCANCODE_KP_0, RDP_SCANCODE_NUMPAD0),
	ENTRY(SDL_SCANCODE_KP_PERIOD, RDP_SCANCODE_OEM_PERIOD),
	ENTRY(SDL_SCANCODE_LCTRL, RDP_SCANCODE_LCONTROL),
	ENTRY(SDL_SCANCODE_LSHIFT, RDP_SCANCODE_LSHIFT),
	ENTRY(SDL_SCANCODE_LALT, RDP_SCANCODE_LMENU),
	ENTRY(SDL_SCANCODE_LGUI, RDP_SCANCODE_LWIN),
	ENTRY(SDL_SCANCODE_RCTRL, RDP_SCANCODE_RCONTROL),
	ENTRY(SDL_SCANCODE_RSHIFT, RDP_SCANCODE_RSHIFT),
	ENTRY(SDL_SCANCODE_RALT, RDP_SCANCODE_RMENU),
	ENTRY(SDL_SCANCODE_RGUI, RDP_SCANCODE_RWIN),
	ENTRY(SDL_SCANCODE_MODE, RDP_SCANCODE_APPS),
	ENTRY(SDL_SCANCODE_MUTE, RDP_SCANCODE_VOLUME_MUTE),
	ENTRY(SDL_SCANCODE_VOLUMEUP, RDP_SCANCODE_VOLUME_UP),
	ENTRY(SDL_SCANCODE_VOLUMEDOWN, RDP_SCANCODE_VOLUME_DOWN),
	ENTRY(SDL_SCANCODE_GRAVE, RDP_SCANCODE_OEM_3),
	ENTRY(SDL_SCANCODE_COMMA, RDP_SCANCODE_OEM_COMMA),
	ENTRY(SDL_SCANCODE_PERIOD, RDP_SCANCODE_OEM_PERIOD),
	ENTRY(SDL_SCANCODE_SLASH, RDP_SCANCODE_OEM_2),
	ENTRY(SDL_SCANCODE_BACKSLASH, RDP_SCANCODE_OEM_5),
	ENTRY(SDL_SCANCODE_SCROLLLOCK, RDP_SCANCODE_SCROLLLOCK),
	ENTRY(SDL_SCANCODE_INSERT, RDP_SCANCODE_INSERT),
	ENTRY(SDL_SCANCODE_PRINTSCREEN, RDP_SCANCODE_PRINTSCREEN),
	ENTRY(SDL_SCANCODE_HOME, RDP_SCANCODE_HOME),
	ENTRY(SDL_SCANCODE_DELETE, RDP_SCANCODE_DELETE),
	ENTRY(SDL_SCANCODE_RIGHT, RDP_SCANCODE_RIGHT),
	ENTRY(SDL_SCANCODE_LEFT, RDP_SCANCODE_LEFT),
	ENTRY(SDL_SCANCODE_DOWN, RDP_SCANCODE_DOWN),
	ENTRY(SDL_SCANCODE_UP, RDP_SCANCODE_UP),
	ENTRY(SDL_SCANCODE_SEMICOLON, RDP_SCANCODE_OEM_1),
	ENTRY(SDL_SCANCODE_PAUSE, RDP_SCANCODE_PAUSE),
	ENTRY(SDL_SCANCODE_PAGEUP, RDP_SCANCODE_PRIOR),
	ENTRY(SDL_SCANCODE_END, RDP_SCANCODE_END),
	ENTRY(SDL_SCANCODE_PAGEDOWN, RDP_SCANCODE_NEXT),
	ENTRY(SDL_SCANCODE_MEDIA_NEXT_TRACK, RDP_SCANCODE_MEDIA_NEXT_TRACK),
	ENTRY(SDL_SCANCODE_MEDIA_PREVIOUS_TRACK, RDP_SCANCODE_MEDIA_PREV_TRACK),
	ENTRY(SDL_SCANCODE_MEDIA_STOP, RDP_SCANCODE_MEDIA_STOP),
	ENTRY(SDL_SCANCODE_MEDIA_PLAY, RDP_SCANCODE_MEDIA_PLAY_PAUSE),
	ENTRY(SDL_SCANCODE_MUTE, RDP_SCANCODE_VOLUME_MUTE),
	ENTRY(SDL_SCANCODE_MEDIA_SELECT, RDP_SCANCODE_LAUNCH_MEDIA_SELECT),
	ENTRY(SDL_SCANCODE_SYSREQ, RDP_SCANCODE_SYSREQ),
	ENTRY(SDL_SCANCODE_LEFTBRACKET, RDP_SCANCODE_OEM_4),
	ENTRY(SDL_SCANCODE_RIGHTBRACKET, RDP_SCANCODE_OEM_6),
	ENTRY(SDL_SCANCODE_APOSTROPHE, RDP_SCANCODE_OEM_7),
	ENTRY(SDL_SCANCODE_NONUSBACKSLASH, RDP_SCANCODE_OEM_102),
	ENTRY(SDL_SCANCODE_SLEEP, RDP_SCANCODE_SLEEP),
	ENTRY(SDL_SCANCODE_EQUALS, RDP_SCANCODE_OEM_PLUS),
	ENTRY(SDL_SCANCODE_KP_COMMA, RDP_SCANCODE_DECIMAL),
	ENTRY(SDL_SCANCODE_FIND, RDP_SCANCODE_BROWSER_SEARCH),
	ENTRY(SDL_SCANCODE_RETURN2, RDP_SCANCODE_RETURN_KP),
	ENTRY(SDL_SCANCODE_AC_SEARCH, RDP_SCANCODE_BROWSER_SEARCH),
	ENTRY(SDL_SCANCODE_AC_HOME, RDP_SCANCODE_BROWSER_HOME),
	ENTRY(SDL_SCANCODE_AC_BACK, RDP_SCANCODE_BROWSER_BACK),
	ENTRY(SDL_SCANCODE_AC_FORWARD, RDP_SCANCODE_BROWSER_FORWARD),
	ENTRY(SDL_SCANCODE_AC_STOP, RDP_SCANCODE_BROWSER_STOP),

#if 1 // TODO: unmapped
	ENTRY(SDL_SCANCODE_NONUSHASH, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_APPLICATION, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_POWER, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_EQUALS, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_EXECUTE, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_HELP, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_MENU, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_SELECT, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_STOP, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_AGAIN, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_UNDO, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_CUT, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_COPY, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_PASTE, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_EQUALSAS400, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_INTERNATIONAL1, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_INTERNATIONAL2, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_INTERNATIONAL3, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_INTERNATIONAL4, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_INTERNATIONAL5, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_INTERNATIONAL6, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_INTERNATIONAL7, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_INTERNATIONAL8, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_INTERNATIONAL9, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_LANG1, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_LANG2, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_LANG3, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_LANG4, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_LANG5, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_LANG6, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_LANG7, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_LANG8, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_LANG9, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_ALTERASE, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_CANCEL, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_CLEAR, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_PRIOR, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_SEPARATOR, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_OUT, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_OPER, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_CLEARAGAIN, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_CRSEL, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_EXSEL, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_00, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_000, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_THOUSANDSSEPARATOR, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_DECIMALSEPARATOR, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_CURRENCYUNIT, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_CURRENCYSUBUNIT, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_LEFTPAREN, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_RIGHTPAREN, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_LEFTBRACE, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_RIGHTBRACE, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_TAB, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_BACKSPACE, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_A, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_B, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_C, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_D, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_E, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_F, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_XOR, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_POWER, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_PERCENT, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_LESS, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_GREATER, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_AMPERSAND, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_DBLAMPERSAND, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_VERTICALBAR, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_DBLVERTICALBAR, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_COLON, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_HASH, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_SPACE, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_AT, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_EXCLAM, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_MEMSTORE, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_MEMRECALL, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_MEMCLEAR, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_MEMADD, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_MEMSUBTRACT, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_MEMMULTIPLY, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_MEMDIVIDE, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_PLUSMINUS, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_CLEAR, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_CLEARENTRY, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_BINARY, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_OCTAL, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_DECIMAL, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_KP_HEXADECIMAL, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_AC_REFRESH, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_AC_BOOKMARKS, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_MEDIA_EJECT, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_MEDIA_REWIND, RDP_SCANCODE_UNKNOWN),
	ENTRY(SDL_SCANCODE_MEDIA_FAST_FORWARD, RDP_SCANCODE_UNKNOWN)
#endif
};

static UINT32 sdl_get_kbd_flags()
{
	UINT32 flags = 0;

	SDL_Keymod mod = SDL_GetModState();
	if ((mod & SDL_KMOD_NUM) != 0)
		flags |= KBD_SYNC_NUM_LOCK;
	if ((mod & SDL_KMOD_CAPS) != 0)
		flags |= KBD_SYNC_CAPS_LOCK;
	if ((mod & SDL_KMOD_SCROLL) != 0)
		flags |= KBD_SYNC_SCROLL_LOCK;

	// TODO: KBD_SYNC_KANA_LOCK

	return flags;
}

BOOL sdlInput::keyboard_sync_state()
{
	const UINT32 syncFlags = sdl_get_kbd_flags();
	return freerdp_input_send_synchronize_event(_sdl->context()->input, syncFlags);
}

BOOL sdlInput::keyboard_focus_in()
{
	auto input = _sdl->context()->input;
	WINPR_ASSERT(input);

	auto syncFlags = sdl_get_kbd_flags();
	freerdp_input_send_focus_in_event(input, syncFlags);

	/* finish with a mouse pointer position like mstsc.exe if required */
#if 0
    if (xfc->remote_app)
        return;

    if (XQueryPointer(xfc->display, xfc->window->handle, &w, &w, &d, &d, &x, &y, &state))
    {
        if ((x >= 0) && (x < xfc->window->width) && (y >= 0) && (y < xfc->window->height))
        {
            xf_event_adjust_coordinates(xfc, &x, &y);
            freerdp_client_send_button_event(&xfc->common, FALSE, PTR_FLAGS_MOVE, x, y);
        }
    }
#endif
	return TRUE;
}

/* This function is called to update the keyboard indicator LED */
BOOL sdlInput::keyboard_set_indicators(rdpContext* context, UINT16 led_flags)
{
	WINPR_UNUSED(context);

	int state = SDL_KMOD_NONE;

	if ((led_flags & KBD_SYNC_NUM_LOCK) != 0)
		state |= SDL_KMOD_NUM;
	if ((led_flags & KBD_SYNC_CAPS_LOCK) != 0)
		state |= SDL_KMOD_CAPS;
	if ((led_flags & KBD_SYNC_SCROLL_LOCK) != 0)
		state |= SDL_KMOD_SCROLL;

	// TODO: KBD_SYNC_KANA_LOCK

	SDL_SetModState(static_cast<SDL_Keymod>(state));

	return TRUE;
}

/* This function is called to set the IME state */
BOOL sdlInput::keyboard_set_ime_status(rdpContext* context, UINT16 imeId, UINT32 imeState,
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

uint32_t sdlInput::prefToMask()
{
	const std::map<std::string, uint32_t> mapping = {
		{ "KMOD_LSHIFT", SDL_KMOD_LSHIFT }, { "KMOD_RSHIFT", SDL_KMOD_RSHIFT },
		{ "KMOD_LCTRL", SDL_KMOD_LCTRL },   { "KMOD_RCTRL", SDL_KMOD_RCTRL },
		{ "KMOD_LALT", SDL_KMOD_LALT },     { "KMOD_RALT", SDL_KMOD_RALT },
		{ "KMOD_LGUI", SDL_KMOD_LGUI },     { "KMOD_RGUI", SDL_KMOD_RGUI },
		{ "KMOD_NUM", SDL_KMOD_NUM },       { "KMOD_CAPS", SDL_KMOD_CAPS },
		{ "KMOD_MODE", SDL_KMOD_MODE },     { "KMOD_SCROLL", SDL_KMOD_SCROLL },
		{ "KMOD_CTRL", SDL_KMOD_CTRL },     { "KMOD_SHIFT", SDL_KMOD_SHIFT },
		{ "KMOD_ALT", SDL_KMOD_ALT },       { "KMOD_GUI", SDL_KMOD_GUI }
	};
	uint32_t mod = SDL_KMOD_NONE;
	for (const auto& val : SdlPref::instance()->get_array("SDL_KeyModMask", { "KMOD_RSHIFT" }))
	{
		auto it = mapping.find(val);
		if (it != mapping.end())
		{
			mod |= it->second;
		}
	}
	return mod;
}

static const char* sdl_scancode_name(Uint32 scancode)
{
	for (const auto& cur : map)
	{
		if (cur.sdl == scancode)
			return cur.sdl_name;
	}

	return "SDL_SCANCODE_UNKNOWN";
}

static Uint32 sdl_scancode_val(const char* scancodeName)
{
	for (const auto& cur : map)
	{
		if (strcmp(cur.sdl_name, scancodeName) == 0)
			return cur.sdl;
	}

	return SDL_SCANCODE_UNKNOWN;
}

static const char* sdl_rdp_scancode_name(UINT32 scancode)
{
	for (const auto& cur : map)
	{
		if (cur.rdp == scancode)
			return cur.rdp_name;
	}

	return "RDP_SCANCODE_UNKNOWN";
}

static UINT32 sdl_rdp_scancode_val(const char* scancodeName)
{
	for (const auto& cur : map)
	{
		if (strcmp(cur.rdp_name, scancodeName) == 0)
			return cur.rdp;
	}

	return RDP_SCANCODE_UNKNOWN;
}

static UINT32 sdl_scancode_to_rdp(Uint32 scancode)
{
	UINT32 rdp = RDP_SCANCODE_UNKNOWN;

	for (const auto& cur : map)
	{
		if (cur.sdl == scancode)
		{
			rdp = cur.rdp;
			break;
		}
	}

#if defined(WITH_DEBUG_SDL_KBD_EVENTS)
	auto code = static_cast<SDL_Scancode>(scancode);
	WLog_DBG(TAG, "got %s [%s] -> [%s]", SDL_GetScancodeName(code), sdl_scancode_name(scancode),
	         sdl_rdp_scancode_name(rdp));
#endif
	return rdp;
}

uint32_t sdlInput::prefKeyValue(const std::string& key, uint32_t fallback)
{
	auto item = SdlPref::instance()->get_string(key);
	if (item.empty())
		return fallback;
	auto val = sdl_scancode_val(item.c_str());
	if (val == SDL_SCANCODE_UNKNOWN)
		return fallback;
	return val;
}

std::list<std::string> sdlInput::tokenize(const std::string& data, const std::string& delimiter)
{
	size_t lastpos = 0;
	size_t pos = 0;
	std::list<std::string> list;
	while ((pos = data.find(delimiter, lastpos)) != std::string::npos)
	{
		auto token = data.substr(lastpos, pos);
		lastpos = pos + 1;
		list.push_back(token);
	}
	auto token = data.substr(lastpos);
	list.push_back(token);
	return list;
}

bool sdlInput::extract(const std::string& token, uint32_t& key, uint32_t& value)
{
	return freerdp_extract_key_value(token.c_str(), &key, &value);
}

uint32_t sdlInput::remapScancode(uint32_t scancode)
{
	if (!_remapInitialized.exchange(true))
		remapInitialize();
	auto it = _remapList.find(scancode);
	if (it != _remapList.end())
		return it->second;
	return scancode;
}

void sdlInput::remapInitialize()
{
	WINPR_ASSERT(_sdl);

	auto context = _sdl->context();
	WINPR_ASSERT(context);
	auto KeyboardRemappingList =
	    freerdp_settings_get_string(context->settings, FreeRDP_KeyboardRemappingList);
	if (!KeyboardRemappingList)
		return;

	auto list = tokenize(KeyboardRemappingList);
	for (auto& token : list)
	{
		uint32_t key = 0;
		uint32_t value = 0;
		if (!extract(token, key, value))
			continue;
		_remapList.emplace(key, value);
	}
}

BOOL sdlInput::keyboard_handle_event(const SDL_KeyboardEvent* ev)
{
	WINPR_ASSERT(ev);
	const UINT32 rdp_scancode = sdl_scancode_to_rdp(ev->scancode);
	const SDL_Keymod mods = SDL_GetModState();

	if ((mods & _hotkeyModmask) == _hotkeyModmask)
	{
		if (ev->type == SDL_EVENT_KEY_DOWN)
		{
			if (ev->scancode == _hotkeyFullscreen)
			{
				_sdl->update_fullscreen(!_sdl->fullscreen);
				return TRUE;
			}
			if (ev->scancode == _hotkeyResizable)
			{
				_sdl->update_resizeable(!_sdl->resizeable);
				return TRUE;
			}

			if (ev->scancode == _hotkeyGrab)
			{
				_sdl->grab_kbd_enabled = !_sdl->grab_kbd_enabled;
				keyboard_grab(ev->windowID, _sdl->grab_kbd);
				return TRUE;
			}
			if (ev->scancode == _hotkeyDisconnect)
			{
				freerdp_abort_connect_context(_sdl->context());
				return TRUE;
			}
			if (ev->scancode == _hotkeyMinimize)
			{
				_sdl->update_minimize();
				return TRUE;
			}
		}
	}

	auto scancode = remapScancode(rdp_scancode);
	return freerdp_input_send_keyboard_event_ex(
	    _sdl->context()->input, ev->type == SDL_EVENT_KEY_DOWN, ev->repeat, scancode);
}

BOOL sdlInput::keyboard_grab(Uint32 windowID, bool enable)
{
	auto it = _sdl->windows.find(windowID);
	if (it == _sdl->windows.end())
		return FALSE;
	auto status = enable && _sdl->grab_kbd_enabled;
	_sdl->grab_kbd = status;
	return it->second.grabKeyboard(status);
}

BOOL sdlInput::mouse_focus(Uint32 windowID)
{
	if (_lastWindowID != windowID)
	{
		_lastWindowID = windowID;
		auto it = _sdl->windows.find(windowID);
		if (it == _sdl->windows.end())
			return FALSE;

		it->second.raise();
	}
	return TRUE;
}

BOOL sdlInput::mouse_grab(Uint32 windowID, bool enable)
{
	auto it = _sdl->windows.find(windowID);
	if (it == _sdl->windows.end())
		return FALSE;
	_sdl->grab_mouse = enable;
	return it->second.grabMouse(enable);
}

sdlInput::sdlInput(SdlContext* sdl)
    : _sdl(sdl), _lastWindowID(UINT32_MAX), _hotkeyModmask(prefToMask())
{

	_hotkeyFullscreen = prefKeyValue("SDL_Fullscreen", SDL_SCANCODE_RETURN);
	_hotkeyResizable = prefKeyValue("SDL_Resizeable", SDL_SCANCODE_R);
	_hotkeyGrab = prefKeyValue("SDL_Grab", SDL_SCANCODE_G);
	_hotkeyDisconnect = prefKeyValue("SDL_Disconnect", SDL_SCANCODE_D);
	_hotkeyMinimize = prefKeyValue("SDL_Minimize", SDL_SCANCODE_M);
}
