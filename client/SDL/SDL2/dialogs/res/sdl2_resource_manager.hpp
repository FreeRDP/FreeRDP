/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
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
#include <map>
#include <vector>
#include <SDL.h>

#include <res/sdl_resource_manager.hpp>

class SDL2ResourceManager : public SDLResourceManager
{
  public:
	SDL2ResourceManager() = delete;
	SDL2ResourceManager(const SDL2ResourceManager& other) = delete;
	SDL2ResourceManager(const SDL2ResourceManager&& other) = delete;
	~SDL2ResourceManager() = delete;
	SDL2ResourceManager& operator=(const SDL2ResourceManager& other) = delete;
	SDL2ResourceManager& operator=(SDL2ResourceManager&& other) = delete;

	static SDL_RWops* get(const std::string& type, const std::string& id);
};
