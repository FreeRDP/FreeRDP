/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client
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
#include <freerdp/version.h>
#if defined(WITH_CJSON)
#include <cjson/cJSON.h>
#endif

#if defined(WITH_CJSON)
using cJSONPtr = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;

static cJSONPtr get()
{
	auto config = sdl_get_pref_file();

	std::ifstream ifs(config);
	std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
	return { cJSON_ParseWithLength(content.c_str(), content.size()), cJSON_Delete };
}

static cJSON* get_item(const std::string& key)
{
	static cJSONPtr config{ nullptr, cJSON_Delete };
	if (!config)
		config = get();
	if (!config)
		return nullptr;
	return cJSON_GetObjectItemCaseSensitive(config.get(), key.c_str());
}

static std::string item_to_str(cJSON* item, const std::string& fallback = "")
{
	if (!item || !cJSON_IsString(item))
		return fallback;
	auto str = cJSON_GetStringValue(item);
	if (!str)
		return {};
	return str;
}
#endif

std::string sdl_get_pref_string(const std::string& key, const std::string& fallback)
{
#if defined(WITH_CJSON)
	auto item = get_item(key);
	return item_to_str(item, fallback);
#else
	return fallback;
#endif
}

bool sdl_get_pref_bool(const std::string& key, bool fallback)
{
#if defined(WITH_CJSON)
	auto item = get_item(key);
	if (!item || !cJSON_IsBool(item))
		return fallback;
	return cJSON_IsTrue(item);
#else
	return fallback;
#endif
}

int64_t sdl_get_pref_int(const std::string& key, int64_t fallback)
{
#if defined(WITH_CJSON)
	auto item = get_item(key);
	if (!item || !cJSON_IsNumber(item))
		return fallback;
	auto val = cJSON_GetNumberValue(item);
	return static_cast<int64_t>(val);
#else
	return fallback;
#endif
}

std::vector<std::string> sdl_get_pref_array(const std::string& key,
                                            const std::vector<std::string>& fallback)
{
#if defined(WITH_CJSON)
	auto item = get_item(key);
	if (!item || !cJSON_IsArray(item))
		return fallback;

	std::vector<std::string> values;
	for (int x = 0; x < cJSON_GetArraySize(item); x++)
	{
		auto cur = cJSON_GetArrayItem(item, x);
		values.push_back(item_to_str(cur));
	}

	return values;
#else
	return fallback;
#endif
}

std::string sdl_get_pref_dir()
{
	using CStringPtr = std::unique_ptr<char, decltype(&free)>;
	CStringPtr path(GetKnownPath(KNOWN_PATH_XDG_CONFIG_HOME), free);
	if (!path)
		return {};

	fs::path config{ path.get() };
	config /= FREERDP_VENDOR;
	config /= FREERDP_PRODUCT;
	return config.string();
}

std::string sdl_get_pref_file()
{
	fs::path config{ sdl_get_pref_dir() };
	config /= "sdl-freerdp.json";
	return config.string();
}
