/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client helper dialogs
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

#include "../sdl_types.hpp"

class SDLConnectionDialogHider
{
  public:
	explicit SDLConnectionDialogHider(SdlContext* sdl);

	SDLConnectionDialogHider(const SDLConnectionDialogHider& other) = delete;
	SDLConnectionDialogHider(SDLConnectionDialogHider&& other) = delete;
	SDLConnectionDialogHider& operator=(const SDLConnectionDialogHider& other) = delete;
	SDLConnectionDialogHider& operator=(SDLConnectionDialogHider&& other) = delete;

	~SDLConnectionDialogHider();

  private:
	SdlContext* _sdl = nullptr;
	bool _visible = false;
};
