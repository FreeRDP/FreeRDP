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

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <winpr/json.h>

class SdlPref
{
  public:
	static std::shared_ptr<SdlPref> instance(const std::string& name = SdlPref::get_default_file());

	std::string get_pref_file();

	std::string get_string(const std::string& key, const std::string& fallback = "");
	int64_t get_int(const std::string& key, int64_t fallback = 0);
	bool get_bool(const std::string& key, bool fallback = false);
	std::vector<std::string> get_array(const std::string& key,
	                                   const std::vector<std::string>& fallback = {});

	static void print_config_file_help(int version);

  private:
	using WINPR_JSONPtr = std::unique_ptr<WINPR_JSON, decltype(&WINPR_JSON_Delete)>;

	std::string _name;
	WINPR_JSONPtr _config;

	explicit SdlPref(std::string file);

	WINPR_JSON* get_item(const std::string& key);
	WINPR_JSONPtr get();

	static std::string get_pref_dir();
	static std::string get_default_file();
	static std::string item_to_str(WINPR_JSON* item, const std::string& fallback = "");
};
