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

#pragma once

#include <string>
#include <vector>

std::string sdl_get_pref_dir();
std::string sdl_get_pref_file();

std::string sdl_get_pref_string(const std::string& key, const std::string& fallback = "");
int64_t sdl_get_pref_int(const std::string& key, int64_t fallback = 0);
bool sdl_get_pref_bool(const std::string& key, bool fallback = false);
std::vector<std::string> sdl_get_pref_array(const std::string& key,
                                            const std::vector<std::string>& fallback = {});