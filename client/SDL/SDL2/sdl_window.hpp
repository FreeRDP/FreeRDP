/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client
 *
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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
#include <SDL.h>

class SdlWindow
{
  public:
	SdlWindow(const std::string& title, Sint32 startupX, Sint32 startupY, Sint32 width,
	          Sint32 height, Uint32 flags);
	SdlWindow(const SdlWindow& other) = delete;
	SdlWindow(SdlWindow&& other) noexcept;
	~SdlWindow();

	SdlWindow& operator=(const SdlWindow& other) = delete;
	SdlWindow& operator=(SdlWindow&& other) = delete;

	[[nodiscard]] Uint32 id() const;
	[[nodiscard]] int displayIndex() const;
	[[nodiscard]] SDL_Rect rect() const;
	[[nodiscard]] SDL_Window* window() const;

	[[nodiscard]] Sint32 offsetX() const;
	void setOffsetX(Sint32 x);

	void setOffsetY(Sint32 y);
	[[nodiscard]] Sint32 offsetY() const;

	bool grabKeyboard(bool enable);
	bool grabMouse(bool enable);
	void setBordered(bool bordered);
	void raise();
	void resizeable(bool use);
	void fullscreen(bool enter);
	void minimize();

	bool fill(Uint8 r = 0x00, Uint8 g = 0x00, Uint8 b = 0x00, Uint8 a = 0xff);
	bool blit(SDL_Surface* surface, SDL_Rect src, SDL_Rect& dst);
	void updateSurface();

  private:
	SDL_Window* _window = nullptr;
	Sint32 _offset_x = 0;
	Sint32 _offset_y = 0;
};
