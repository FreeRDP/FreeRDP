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
#include "sdl_resource_manager.hpp"
#include <filesystem>
#include <iostream>

std::map<std::string, std::vector<unsigned char>> SDLResourceManager::_resources;

SDL_RWops* SDLResourceManager::get(const std::string& type, const std::string& id)
{
	std::string uuid = type + "/" + id;

#if defined(SDL_USE_COMPILED_RESOURCES)
	auto val = _resources.find(uuid);
	if (val == _resources.end())
		return nullptr;

	auto& v = _resources[uuid];
	return SDL_RWFromConstMem(v.data(), v.size());
#else
	std::filesystem::path path(SDL_RESOURCE_ROOT);
	path /= type;
	path /= id;

	if (!std::filesystem::exists(path))
	{
		std::cerr << "sdl-freerdp expects resource '" << uuid << "' at location "
		          << std::filesystem::absolute(path) << std::endl;
		std::cerr << "file not found, application will fail" << std::endl;
	}
	return SDL_RWFromFile(path.native().c_str(), "rb");
#endif
}

const std::string SDLResourceManager::typeFonts()
{
	return "fonts";
}

const std::string SDLResourceManager::typeImages()
{
	return "images";
}

void SDLResourceManager::insert(const std::string& type, const std::string& id,
                                const std::vector<unsigned char>& data)
{
	std::string uuid = type + "/" + id;
	_resources.emplace(uuid, data);
}
