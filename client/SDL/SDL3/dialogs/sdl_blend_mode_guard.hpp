/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client helper dialogs
 *
 * Copyright 2025 Armin Novak <armin.novak@thincast.com>
 * Copyright 2025 Thincast Technologies GmbH
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

#include <memory>

#include <SDL3/SDL.h>

class SdlBlendModeGuard
{
  public:
	explicit SdlBlendModeGuard(const std::shared_ptr<SDL_Renderer>& renderer,
	                           SDL_BlendMode mode = SDL_BLENDMODE_NONE);
	~SdlBlendModeGuard();

	SdlBlendModeGuard(SdlBlendModeGuard&& other) noexcept;
	SdlBlendModeGuard(const SdlBlendModeGuard& other) = delete;

	SdlBlendModeGuard& operator=(const SdlBlendModeGuard& other) = delete;
	SdlBlendModeGuard& operator=(SdlBlendModeGuard&& other) = delete;

	bool update(SDL_BlendMode mode);

  private:
	SDL_BlendMode _restore_mode = SDL_BLENDMODE_INVALID;
	SDL_BlendMode _current_mode = SDL_BLENDMODE_INVALID;
	std::shared_ptr<SDL_Renderer> _renderer;
};
