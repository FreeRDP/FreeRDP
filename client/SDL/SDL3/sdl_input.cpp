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

#include "sdl_input.hpp"
#include "sdl_disp.hpp"
#include "sdl_context.hpp"
#include "sdl_utils.hpp"
#include "sdl_prefs.hpp"
#include "sdl_touch.hpp"
#include "sdl_scancode_map.hpp"

#include <SDL3/SDL_oldnames.h>

#include <array>
#include <map>
#include <tuple>

#include <freerdp/utils/string.h>
#include <freerdp/scancode.h>
#include <freerdp/locale/keyboard.h>
#include <freerdp/locale/locale.h>

#include <freerdp/log.h>

[[nodiscard]] static UINT32 sdl_get_kbd_flags()
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

[[nodiscard]] static FREERDP_CLIENT_KBD_MOD sdl_get_kbd_modmask(SDL_Keymod mod)
{
	static const std::array<std::pair<SDL_Keymod, uint32_t>, 8> masks = {
		{ { SDL_KMOD_LSHIFT, FREERDP_CLIENT_KBD_MOD_LSHIFT },
		  { SDL_KMOD_RSHIFT, FREERDP_CLIENT_KBD_MOD_RSHIFT },
		  { SDL_KMOD_LALT, FREERDP_CLIENT_KBD_MOD_LMENU },
		  { SDL_KMOD_RALT, FREERDP_CLIENT_KBD_MOD_RMENU },
		  { SDL_KMOD_LCTRL, FREERDP_CLIENT_KBD_MOD_LCONTROL },
		  { SDL_KMOD_RCTRL, FREERDP_CLIENT_KBD_MOD_RCONTROL },
		  { SDL_KMOD_LGUI, FREERDP_CLIENT_KBD_MOD_LWIN },
		  { SDL_KMOD_RGUI, FREERDP_CLIENT_KBD_MOD_RWIN } }
	};

	uint32_t modmask = FREERDP_CLIENT_KBD_MOD_NONE;
	for (const auto& [sdl, rdp] : masks)
	{
		if ((mod & sdl) != 0)
			modmask |= rdp;
	}
	return static_cast<FREERDP_CLIENT_KBD_MOD>(modmask);
}

bool sdlInput::keyboard_sync_state()
{
	const UINT32 syncFlags = sdl_get_kbd_flags();
	return freerdp_input_send_synchronize_event(_sdl->context()->input, syncFlags);
}

bool sdlInput::keyboard_focus_in()
{
	auto input = _sdl->context()->input;
	WINPR_ASSERT(input);

	auto syncFlags = sdl_get_kbd_flags();
	if (!freerdp_input_send_focus_in_event(input, WINPR_ASSERTING_INT_CAST(uint16_t, syncFlags)))
		return false;

	/* finish with a mouse pointer position like mstsc.exe if required */
	// TODO: fullscreen/remote app
	float fx = 0.0f;
	float fy = 0.0f;
	SDL_GetMouseState(&fx, &fy);

	auto w = SDL_GetMouseFocus();
	const auto& pos = _sdl->screenToPixel(SDL_GetWindowID(w), SDL_FPoint{ fx, fy });

	return freerdp_client_send_button_event(_sdl->common(), FALSE, PTR_FLAGS_MOVE,
	                                        static_cast<Sint32>(pos.x), static_cast<Sint32>(pos.y));
}

/* This function is called to update the keyboard indicator LED */
BOOL sdlInput::keyboard_set_indicators(rdpContext* context, UINT16 led_flags)
{
	WINPR_UNUSED(context);

	SDL_Keymod state = SDL_KMOD_NONE;

	if ((led_flags & KBD_SYNC_NUM_LOCK) != 0)
		state |= SDL_KMOD_NUM;
	if ((led_flags & KBD_SYNC_CAPS_LOCK) != 0)
		state |= SDL_KMOD_CAPS;
	if ((led_flags & KBD_SYNC_SCROLL_LOCK) != 0)
		state |= SDL_KMOD_SCROLL;
	if ((led_flags & KBD_SYNC_KANA_LOCK) != 0)
		state |= SDL_KMOD_LEVEL5;

	SDL_SetModState(state);

	return TRUE;
}

/* This function is called to set the IME state */
BOOL sdlInput::keyboard_set_ime_status(rdpContext* context, UINT16 imeId, UINT32 imeState,
                                       UINT32 imeConvMode)
{
	auto sdl = reinterpret_cast<SdlContext*>(context);
	if (!context)
		return FALSE;

	WLog_Print(sdl->getWLog(), WLOG_WARN,
	           "KeyboardSetImeStatus(unitId=%04" PRIx16 ", imeState=%08" PRIx32
	           ", imeConvMode=%08" PRIx32 ") ignored",
	           imeId, imeState, imeConvMode);
	return TRUE;
}

[[nodiscard]] static const std::map<std::string, uint32_t>& getSdlMap()
{
	static std::map<std::string, uint32_t> s_map = {
		{ "KMOD_LSHIFT", SDL_KMOD_LSHIFT },     { "KMOD_RSHIFT", SDL_KMOD_RSHIFT },
		{ "KMOD_LCTRL", SDL_KMOD_LCTRL },       { "KMOD_RCTRL", SDL_KMOD_RCTRL },
		{ "KMOD_LALT", SDL_KMOD_LALT },         { "KMOD_RALT", SDL_KMOD_RALT },
		{ "KMOD_LGUI", SDL_KMOD_LGUI },         { "KMOD_RGUI", SDL_KMOD_RGUI },
		{ "KMOD_NUM", SDL_KMOD_NUM },           { "KMOD_CAPS", SDL_KMOD_CAPS },
		{ "KMOD_MODE", SDL_KMOD_MODE },         { "KMOD_SCROLL", SDL_KMOD_SCROLL },
		{ "KMOD_CTRL", SDL_KMOD_CTRL },         { "KMOD_SHIFT", SDL_KMOD_SHIFT },
		{ "KMOD_ALT", SDL_KMOD_ALT },           { "KMOD_GUI", SDL_KMOD_GUI },
		{ "KMOD_NONE", SDL_KMOD_NONE },         { "SDL_KMOD_LSHIFT", SDL_KMOD_LSHIFT },
		{ "SDL_KMOD_RSHIFT", SDL_KMOD_RSHIFT }, { "SDL_KMOD_LCTRL", SDL_KMOD_LCTRL },
		{ "SDL_KMOD_RCTRL", SDL_KMOD_RCTRL },   { "SDL_KMOD_LALT", SDL_KMOD_LALT },
		{ "SDL_KMOD_RALT", SDL_KMOD_RALT },     { "SDL_KMOD_LGUI", SDL_KMOD_LGUI },
		{ "SDL_KMOD_RGUI", SDL_KMOD_RGUI },     { "SDL_KMOD_NUM", SDL_KMOD_NUM },
		{ "SDL_KMOD_CAPS", SDL_KMOD_CAPS },     { "SDL_KMOD_MODE", SDL_KMOD_MODE },
		{ "SDL_KMOD_SCROLL", SDL_KMOD_SCROLL }, { "SDL_KMOD_CTRL", SDL_KMOD_CTRL },
		{ "SDL_KMOD_SHIFT", SDL_KMOD_SHIFT },   { "SDL_KMOD_ALT", SDL_KMOD_ALT },
		{ "SDL_KMOD_GUI", SDL_KMOD_GUI },       { "SDL_KMOD_NONE", SDL_KMOD_NONE },
		{ "SDL_KMOD_MODE", SDL_KMOD_MODE },     { "SDL_KMOD_LEVEL5", SDL_KMOD_LEVEL5 }
	};

	return s_map;
}

[[nodiscard]] static std::string modbyvalue(uint32_t val)
{
	for (const auto& v : getSdlMap())
	{
		if (v.second == val)
		{
			return v.first;
		}
	}
	return "KMOD_UNKNONW";
}

[[nodiscard]] static std::string masktostr(uint32_t mask)
{
	if (mask == 0)
		return "<NONE>";

	std::string str = "<";
	for (uint32_t x = 0; x < 32; x++)
	{
		uint32_t cmask = 1u << x;
		if ((mask & cmask) != 0)
		{
			auto s = modbyvalue(cmask);
			if (str.size() > 1)
			{
				str += ">+<";
			}
			str += s;
		}
	}
	str += ">";
	return str;
}

bool sdlInput::prefToEnabled()
{
	bool enabled = true;
	const auto& m = getSdlMap();
	for (const auto& val : SdlPref::instance()->get_array("SDL_KeyModMask", { "KMOD_RSHIFT" }))
	{
		auto it = m.find(val);
		if (it != m.end())
		{
			if (it->second == SDL_KMOD_NONE)
				enabled = false;
		}
		else
		{
			WLog_Print(_sdl->getWLog(), WLOG_WARN,
			           "Invalid config::SDL_KeyModMask entry value '%s', disabling hotkeys",
			           val.c_str());
			enabled = false;
		}
	}
	return enabled;
}

uint32_t sdlInput::prefToMask()
{
	const auto& m = getSdlMap();
	uint32_t mod = SDL_KMOD_NONE;
	for (const auto& val : SdlPref::instance()->get_array("SDL_KeyModMask", { "KMOD_RSHIFT" }))
	{
		auto it = m.find(val);
		if (it != m.end())
			mod |= it->second;
	}
	return mod;
}

[[nodiscard]] static const char* sdl_scancode_name(Uint32 scancode)
{
	for (const auto& cur : sdl_scancode_map)
	{
		if (cur.sdl == scancode)
			return cur.sdl_name;
	}

	return "SDL_SCANCODE_UNKNOWN";
}

[[nodiscard]] static Uint32 sdl_scancode_val(const char* scancodeName)
{
	for (const auto& cur : sdl_scancode_map)
	{
		if (strcmp(cur.sdl_name, scancodeName) == 0)
			return cur.sdl;
	}

	return SDL_SCANCODE_UNKNOWN;
}

#if defined(WITH_DEBUG_SDL_KBD_EVENTS)
[[nodiscard]] static const char* sdl_rdp_scancode_name(UINT32 scancode)
{
	for (const auto& cur : sdl_scancode_map)
	{
		if (cur.rdp == scancode)
			return cur.rdp_name;
	}

	return "RDP_SCANCODE_UNKNOWN";
}
#endif

UINT32 sdlInput::scancode_to_rdp(Uint32 scancode)
{
	UINT32 rdp = RDP_SCANCODE_UNKNOWN;

	for (const auto& cur : sdl_scancode_map)
	{
		if (cur.sdl == scancode)
		{
			rdp = cur.rdp;
			break;
		}
	}

#if defined(WITH_DEBUG_SDL_KBD_EVENTS)
	auto code = static_cast<SDL_Scancode>(scancode);
	WLog_Print(_sdl->getWLog(), WLOG_DEBUG, "got %s [%s] -> [%s]", SDL_GetScancodeName(code),
	           sdl_scancode_name(scancode), sdl_rdp_scancode_name(rdp));
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
		list.push_back(std::move(token));
	}
	auto token = data.substr(lastpos);
	list.push_back(std::move(token));
	return list;
}

bool sdlInput::extract(const std::string& token, uint32_t& key, uint32_t& value)
{
	return freerdp_extract_key_value(token.c_str(), &key, &value);
}

bool sdlInput::handleEvent(const SDL_KeyboardEvent& ev)
{
	UINT32 rdp_scancode = scancode_to_rdp(ev.scancode);
#if defined(_WIN32)
	/* CJK IMEs on Windows may set the extended bit for Right Shift, producing
	 * the invalid scan code E0 36. SDL maps that event to SDL_SCANCODE_UNKNOWN,
	 * so normalize it in the same way as the native Windows client. */
	if ((rdp_scancode == RDP_SCANCODE_UNKNOWN) && (ev.raw == 0xE036))
		rdp_scancode = RDP_SCANCODE_RSHIFT;
#endif
	/* The modifiers held when this key was pressed: SDL_GetModState() is the state after every
	 * event already queued, so a hotkey could be missed, or fire on a plain key typed just
	 * before a modifier. */
	const SDL_Keymod mods = ev.mod;

	if (_hotkeysEnabled && (mods & _hotkeyModmask) == _hotkeyModmask)
	{
		if (ev.type == SDL_EVENT_KEY_DOWN)
		{
			if (ev.scancode == _hotkeyFullscreen)
			{
				WLog_Print(_sdl->getWLog(), WLOG_INFO, "%s+<%s> pressed, toggling fullscreen state",
				           masktostr(_hotkeyModmask).c_str(), sdl_scancode_name(_hotkeyFullscreen));
				if (!keyboard_sync_state())
					return false;
				return _sdl->toggleFullscreen();
			}
			if (ev.scancode == _hotkeyResizable)
			{
				WLog_Print(_sdl->getWLog(), WLOG_INFO, "%s+<%s> pressed, toggling resizeable state",
				           masktostr(_hotkeyModmask).c_str(), sdl_scancode_name(_hotkeyResizable));
				if (!keyboard_sync_state())
					return false;
				return _sdl->toggleResizeable();
			}

			if (ev.scancode == _hotkeyGrab)
			{
				WLog_Print(_sdl->getWLog(), WLOG_INFO, "%s+<%s> pressed, toggling grab state",
				           masktostr(_hotkeyModmask).c_str(), sdl_scancode_name(_hotkeyGrab));
				if (!keyboard_sync_state())
					return false;
				return keyboard_grab(ev.windowID, !_sdl->grabKeyboard());
			}
			if (ev.scancode == _hotkeyDisconnect)
			{
				WLog_Print(_sdl->getWLog(), WLOG_INFO, "%s+<%s> pressed, disconnecting RDP session",
				           masktostr(_hotkeyModmask).c_str(), sdl_scancode_name(_hotkeyDisconnect));
				if (!keyboard_sync_state())
					return false;
				freerdp_abort_connect_context(_sdl->context());
				return true;
			}
			if (ev.scancode == _hotkeyMinimize)
			{
				WLog_Print(_sdl->getWLog(), WLOG_INFO, "%s+<%s> pressed, minimizing client",
				           masktostr(_hotkeyModmask).c_str(), sdl_scancode_name(_hotkeyMinimize));
				if (!keyboard_sync_state())
					return false;
				return _sdl->setMinimized();
			}
		}
	}

#if defined(WITH_DEBUG_KBD)
	{
		const BOOL ex = RDP_SCANCODE_EXTENDED(rdp_scancode);
		const DWORD sc = RDP_SCANCODE_CODE(rdp_scancode);
		WLog_Print(_sdl->getWLog(), WLOG_DEBUG,
		           "SDL keycode: %02" PRIX32 " -> rdp code: [%04" PRIx16 "] %02" PRIX8 "%s",
		           ev.scancode, rdp_scancode, sc, ex ? " extended" : "");
	}
#endif

	auto scancode = freerdp_keyboard_remap_key(_remapTable, rdp_scancode);
#if defined(WITH_DEBUG_KBD)
	{
		const BOOL ex = RDP_SCANCODE_EXTENDED(scancode);
		const DWORD sc = RDP_SCANCODE_CODE(scancode);
		WLog_Print(_sdl->getWLog(), WLOG_DEBUG,
		           "SDL keycode: %02" PRIX32 " -> remapped rdp code: [%04" PRIx16 "] %02" PRIX8
		           "%s",
		           ev.scancode, scancode, sc, ex ? " extended" : "");
	}
#endif
	if (freerdp_settings_get_bool(_sdl->context()->settings, FreeRDP_UnicodeInput))
	{
		/* With key_event false the keycode follows the layout, also for non-Latin ones, where key
		 * events carry the QWERTY letter (SDL_HINT_KEYCODE_OPTIONS). */
		const auto key = SDL_GetKeyFromScancode(ev.scancode, mods, false);
		std::array<char, 5> str = {};
		if ((key >= 0x20) && (key != SDLK_DELETE) && (key <= 0x10FFFF))
			std::ignore = SDL_UCS4ToUTF8(key, str.data());
		return freerdp_client_send_key_as_unicode(
		    _sdl->common(), (ev.type == SDL_EVENT_KEY_DOWN) ? 0 : KBD_FLAGS_RELEASE, str.data(),
		    scancode, sdl_get_kbd_modmask(mods));
	}
	return freerdp_input_send_keyboard_event_ex(_sdl->context()->input,
	                                            ev.type == SDL_EVENT_KEY_DOWN, ev.repeat, scancode);
}

bool sdlInput::keyboard_grab(Uint32 windowID, bool enable)
{
	const auto window = _sdl->getWindowForId(windowID);
	if (!window)
		return false;

	auto settings = _sdl->context()->settings;
	auto kbd_enabled = freerdp_settings_get_bool(settings, FreeRDP_GrabKeyboard);
	auto status = enable && kbd_enabled;
	if (!_sdl->setGrabKeyboard(status))
		WLog_Print(_sdl->getWLog(), WLOG_WARN, "Failed to ungrab keyboard");
	return window->grabKeyboard(status);
}

bool sdlInput::mouse_focus(Uint32 windowID)
{
	if (_lastWindowID != windowID)
	{
		_lastWindowID = windowID;
		auto window = _sdl->getWindowForId(windowID);
		if (window)
			window->raise();
		else
			return _sdl->getRailChannelContext().ownsWindow(windowID);
	}
	return true;
}

bool sdlInput::mouse_grab(Uint32 windowID, bool enable)
{
	auto window = _sdl->getWindowForId(windowID);
	if (!window)
		return false;
	if (!_sdl->setGrabMouse(enable))
		WLog_Print(_sdl->getWLog(), WLOG_WARN, "Failed to ungrab mouse");
	return window->grabMouse(enable);
}

sdlInput::sdlInput(SdlContext* sdl)
    : _sdl(sdl), _lastWindowID(UINT32_MAX), _hotkeysEnabled(prefToEnabled()),
      _hotkeyModmask(prefToMask())
{
	_hotkeyFullscreen = prefKeyValue("SDL_Fullscreen", SDL_SCANCODE_RETURN);
	_hotkeyResizable = prefKeyValue("SDL_Resizeable", SDL_SCANCODE_R);
	_hotkeyGrab = prefKeyValue("SDL_Grab", SDL_SCANCODE_G);
	_hotkeyDisconnect = prefKeyValue("SDL_Disconnect", SDL_SCANCODE_D);
	_hotkeyMinimize = prefKeyValue("SDL_Minimize", SDL_SCANCODE_M);
}

sdlInput::~sdlInput()
{
	freerdp_keyboard_remap_free(_remapTable);
}

bool sdlInput::initialize()
{
	auto settings = _sdl->context()->settings;
	WINPR_ASSERT(settings);
	WINPR_ASSERT(!_remapTable);

	{
		auto list = freerdp_settings_get_string(settings, FreeRDP_KeyboardRemappingList);
		_remapTable = freerdp_keyboard_remap_string_to_list(list);
		if (!_remapTable)
			return false;
	}

	if (freerdp_settings_get_uint32(settings, FreeRDP_KeyboardLayout) == 0)
	{
		DWORD KeyboardLayout = 0;

		freerdp_detect_keyboard_layout_from_system_locale(&KeyboardLayout);
		if (KeyboardLayout == 0)
			KeyboardLayout = ENGLISH_UNITED_STATES;

		if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardLayout, KeyboardLayout))
			return false;
	}
	return true;
}
