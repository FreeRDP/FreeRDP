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

#include <fstream>
#if __has_include(<filesystem>)
#include <filesystem>
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
#include <freerdp/version.h>
#include <winpr/json.h>
#include <freerdp/settings.h>

SdlPref::WINPR_JSONPtr SdlPref::get()
{
	auto config = get_pref_file();

	std::ifstream ifs(config);
	std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
	return { WINPR_JSON_ParseWithLength(content.c_str(), content.size()), WINPR_JSON_Delete };
}

WINPR_JSON* SdlPref::get_item(const std::string& key)
{
	if (!_config)
		return nullptr;
	return WINPR_JSON_GetObjectItem(_config.get(), key.c_str());
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

std::string SdlPref::get_string(const std::string& key, const std::string& fallback)
{
	auto item = get_item(key);
	return item_to_str(item, fallback);
}

bool SdlPref::get_bool(const std::string& key, bool fallback)
{
	auto item = get_item(key);
	if (!item || !WINPR_JSON_IsBool(item))
		return fallback;
	return WINPR_JSON_IsTrue(item);
}

int64_t SdlPref::get_int(const std::string& key, int64_t fallback)
{
	auto item = get_item(key);
	if (!item || !WINPR_JSON_IsNumber(item))
		return fallback;
	auto val = WINPR_JSON_GetNumberValue(item);
	return static_cast<int64_t>(val);
}

std::vector<std::string> SdlPref::get_array(const std::string& key,
                                            const std::vector<std::string>& fallback)
{
	auto item = get_item(key);
	if (!item || !WINPR_JSON_IsArray(item))
		return fallback;

	std::vector<std::string> values;
	for (int x = 0; x < WINPR_JSON_GetArraySize(item); x++)
	{
		auto cur = WINPR_JSON_GetArrayItem(item, x);
		values.push_back(item_to_str(cur));
	}

	return values;
}

SdlPref::SdlPref(const std::string& file) : _name(file), _config(get())
{
}

std::string SdlPref::get_pref_dir()
{
	using CStringPtr = std::unique_ptr<char, decltype(&free)>;
	CStringPtr path(freerdp_settings_get_config_path(), free);
	if (!path)
		return {};

	fs::path config{ path.get() };
	return config.string();
}

std::string SdlPref::get_default_file()
{
	fs::path config{ SdlPref::get_pref_dir() };
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

std::string SdlPref::get_pref_file()
{
	return _name;
}
