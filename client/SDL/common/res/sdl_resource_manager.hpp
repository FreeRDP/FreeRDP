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

class SDLResourceManager
{
	friend class SDLResourceFile;

  public:
	SDLResourceManager() = delete;
	SDLResourceManager(const SDLResourceManager& other) = delete;
	SDLResourceManager(const SDLResourceManager&& other) = delete;
	~SDLResourceManager() = delete;
	SDLResourceManager operator=(const SDLResourceManager& other) = delete;
	SDLResourceManager& operator=(SDLResourceManager&& other) = delete;

	static std::string typeFonts();
	static std::string typeImages();

  protected:
	static void insert(const std::string& type, const std::string& id,
	                   const std::vector<unsigned char>& data);

	static const std::vector<unsigned char>* data(const std::string& type, const std::string& id);
	static std::string filename(const std::string& type, const std::string& id);

	static bool useCompiledResources();

  private:
	static std::map<std::string, std::vector<unsigned char>>& resources();
#if defined(SDL_USE_COMPILED_RESOURCES)
	static void init(); // implemented in generated file
#endif
};
