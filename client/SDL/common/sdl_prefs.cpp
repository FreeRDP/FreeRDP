/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Prefs
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

#include <iostream>
#include <fstream>
#if __has_include(<filesystem>)
#include <filesystem>
#include <utility>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#error Could not find system header "<filesystem>" or "<experimental/filesystem>"
#endif

#include "sdl_prefs.hpp"

#include <winpr/path.h>
#include <winpr/config.h>
#include <winpr/json.h>

#include <freerdp/version.h>
#include <freerdp/settings.h>
#include <freerdp/utils/helpers.h>

SdlPref::WINPR_JSONPtr SdlPref::get(bool systemConfigOnly) const
{
	auto config = get_pref_file(systemConfigOnly);
	return { WINPR_JSON_ParseFromFile(config.c_str()), WINPR_JSON_Delete };
}

WINPR_JSON* SdlPref::get_item(const std::string& key, bool systemConfigOnly) const
{
	/* If we request a system setting or user settings are disabled */
	if (systemConfigOnly || !is_user_config_enabled())
		return get_item(_system_config, key);

	/* Get the user setting */
	auto res = get_item(_config, key);

	/* User setting does not exist, fall back to system setting */
	if (!res)
		res = get_item(_system_config, key);
	return res;
}

WINPR_JSON* SdlPref::get_item(const WINPR_JSONPtr& config, const std::string& key) const
{
	if (!config)
		return nullptr;
	return WINPR_JSON_GetObjectItemCaseSensitive(config.get(), key.c_str());
}

bool SdlPref::get_bool(const WINPR_JSONPtr& config, const std::string& key, bool fallback) const
{
	auto item = get_item(config, key);
	if (!item || !WINPR_JSON_IsBool(item))
		return fallback;
	return WINPR_JSON_IsTrue(item);
}

bool SdlPref::is_user_config_enabled() const
{
	auto& config = _system_config;
	if (!config)
		return true;
	return get_bool(config, "isUserConfigEnabled", true);
}

std::string SdlPref::item_to_str(WINPR_JSON* item, const std::string& fallback)
{
	if (!item || !WINPR_JSON_IsString(item))
		return fallback;
	auto str = WINPR_JSON_GetStringValue(item);
	if (!str)
		return {};
	return str;
}

std::string SdlPref::get_string(const std::string& key, const std::string& fallback,
                                bool systemConfigOnly) const
{
	auto item = get_item(key, systemConfigOnly);
	return item_to_str(item, fallback);
}

bool SdlPref::get_bool(const std::string& key, bool fallback, bool systemConfigOnly) const
{
	auto& config = systemConfigOnly ? _system_config : _config;
	return get_bool(config, key, fallback);
}

int64_t SdlPref::get_int(const std::string& key, int64_t fallback, bool systemConfigOnly) const
{
	auto item = get_item(key, systemConfigOnly);
	if (!item || !WINPR_JSON_IsNumber(item))
		return fallback;
	auto val = WINPR_JSON_GetNumberValue(item);
	return static_cast<int64_t>(val);
}

std::vector<std::string> SdlPref::get_array(const std::string& key,
                                            const std::vector<std::string>& fallback,
                                            bool systemConfigOnly) const
{
	auto item = get_item(key, systemConfigOnly);
	if (!item || !WINPR_JSON_IsArray(item))
		return fallback;

	std::vector<std::string> values;
	for (size_t x = 0; x < WINPR_JSON_GetArraySize(item); x++)
	{
		auto cur = WINPR_JSON_GetArrayItem(item, x);
		values.push_back(item_to_str(cur));
	}

	return values;
}

void SdlPref::print_config_file_help(int version)
{
#if defined(WITH_WINPR_JSON)
	const std::string url = "https://wiki.libsdl.org/SDL" + std::to_string(version);
	std::cout << "GLOBAL CONFIGURATION FILE" << std::endl;
	std::cout << std::endl;
	std::cout << "  The SDL client supports some system defined configuration options."
	          << std::endl;
	std::cout << "  Settings are stored in JSON format" << std::endl;
	std::cout << "  The location is a system configuration file. Location for current machine is "
	          << SdlPref::instance()->get_pref_file(true) << std::endl;
	std::cout << std::endl;
	std::cout << "  The following configuration options are supported:" << std::endl;
	std::cout << std::endl;
	std::cout << "    isUserConfigEnabled" << std::endl;
	std::cout << "      Allows to enable/disable user specific configuration files." << std::endl;
	std::cout << "      Default enabled" << std::endl;
	std::cout << std::endl;
	std::cout << "    All options of the following user configuration file are also supported here."
	          << std::endl;
	std::cout << std::endl;

	std::cout << "CONFIGURATION FILE" << std::endl;
	std::cout << std::endl;
	std::cout << "  The SDL client supports some user defined configuration options." << std::endl;
	std::cout << "  Settings are stored in JSON format" << std::endl;
	std::cout << "  The location is a per user file. Location for current user is "
	          << SdlPref::instance()->get_pref_file() << std::endl;
	std::cout
	    << "  The XDG_CONFIG_HOME environment variable can be used to override the base directory."
	    << std::endl;
	std::cout << std::endl;
	std::cout << "  The following configuration options are supported:" << std::endl;
	std::cout << std::endl;
	std::cout << "    SDL_KeyModMask" << std::endl;
	std::cout << "      Defines the key combination required for SDL client shortcuts."
	          << std::endl;
	std::cout << "      Default KMOD_RSHIFT" << std::endl;
	std::cout << "      An array of SDL_Keymod strings as defined at "
	             ""
	          << url << "/SDL_Keymod" << std::endl;
	std::cout << std::endl;
	std::cout << "    SDL_Fullscreen" << std::endl;
	std::cout << "      Toggles client fullscreen state." << std::endl;
	std::cout << "      Default SDL_SCANCODE_RETURN." << std::endl;
	std::cout << "      A string as "
	             "defined at "
	          << url << "/SDLScancodeLookup" << std::endl;
	std::cout << std::endl;
	std::cout << "    SDL_Minimize" << std::endl;
	std::cout << "      Minimizes client windows." << std::endl;
	std::cout << "      Default SDL_SCANCODE_M." << std::endl;
	std::cout << "      A string as "
	             "defined at "
	          << url << "/SDLScancodeLookup" << std::endl;
	std::cout << std::endl;
	std::cout << "    SDL_Resizeable" << std::endl;
	std::cout << "      Toggles local window resizeable state." << std::endl;
	std::cout << "      Default SDL_SCANCODE_R." << std::endl;
	std::cout << "      A string as "
	             "defined at "
	          << url << "/SDLScancodeLookup" << std::endl;
	std::cout << std::endl;
	std::cout << "    SDL_Grab" << std::endl;
	std::cout << "      Toggles keyboard and mouse grab state." << std::endl;
	std::cout << "      Default SDL_SCANCODE_G." << std::endl;
	std::cout << "      A string as "
	             "defined at "
	          << url << "/SDLScancodeLookup" << std::endl;
	std::cout << std::endl;
	std::cout << "    SDL_Disconnect" << std::endl;
	std::cout << "      Disconnects from the RDP session." << std::endl;
	std::cout << "      Default SDL_SCANCODE_D." << std::endl;
	std::cout << "      A string as defined at " << url << "/SDLScancodeLookup" << std::endl;

#endif
}

SdlPref::SdlPref(std::string file)
    : _name(std::move(file)), _system_name(get_default_file(true)), _config(get(false)),
      _system_config(get(true))
{
}

std::string SdlPref::get_pref_dir(bool systemConfigOnly)
{
	using CStringPtr = std::unique_ptr<char, decltype(&free)>;
	CStringPtr path(freerdp_GetConfigFilePath(systemConfigOnly, ""), free);
	if (!path)
		return {};

	fs::path config{ path.get() };
	return config.string();
}

std::string SdlPref::get_default_file(bool systemConfigOnly)
{
	fs::path config{ SdlPref::get_pref_dir(systemConfigOnly) };
	config /= "sdl-freerdp.json";
	return config.string();
}

std::shared_ptr<SdlPref> SdlPref::instance(const std::string& name)
{
	static std::shared_ptr<SdlPref> _instance;
	if (!_instance || (_instance->get_pref_file() != name))
		_instance.reset(new SdlPref(name));
	return _instance;
}

std::string SdlPref::get_pref_file(bool systemConfigOnly) const
{
	if (systemConfigOnly)
		return _system_name;

	return _name;
}
