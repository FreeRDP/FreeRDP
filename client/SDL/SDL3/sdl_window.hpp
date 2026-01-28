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
#include <vector>

#include <SDL3/SDL.h>

#include <freerdp/settings_types.h>

class SdlWindow
{
  public:
	[[nodiscard]] static SdlWindow create(SDL_DisplayID id, const std::string& title, Uint32 flags,
	                                      Uint32 width = 0, Uint32 height = 0);

	SdlWindow(SdlWindow&& other) noexcept;
	SdlWindow(const SdlWindow& other) = delete;
	virtual ~SdlWindow();

	SdlWindow& operator=(const SdlWindow& other) = delete;
	SdlWindow& operator=(SdlWindow&& other) = delete;

	[[nodiscard]] SDL_WindowID id() const;
	[[nodiscard]] SDL_DisplayID displayIndex() const;
	[[nodiscard]] SDL_Rect rect() const;
	[[nodiscard]] SDL_Rect bounds() const;
	[[nodiscard]] SDL_Window* window() const;

	[[nodiscard]] Sint32 offsetX() const;
	void setOffsetX(Sint32 x);

	void setOffsetY(Sint32 y);
	[[nodiscard]] Sint32 offsetY() const;

	[[nodiscard]] rdpMonitor monitor(bool isPrimary) const;

	[[nodiscard]] float scale() const;
	[[nodiscard]] SDL_DisplayOrientation orientation() const;

	[[nodiscard]] bool grabKeyboard(bool enable);
	[[nodiscard]] bool grabMouse(bool enable);
	void setBordered(bool bordered);
	void raise();
	void resizeable(bool use);
	void fullscreen(bool enter);
	void minimize();

	[[nodiscard]] bool resize(const SDL_Point& size);

	[[nodiscard]] bool drawRect(SDL_Surface* surface, SDL_Point offset, const SDL_Rect& srcRect);
	[[nodiscard]] bool drawRects(SDL_Surface* surface, SDL_Point offset,
	                             const std::vector<SDL_Rect>& rects = {});
	[[nodiscard]] bool drawScaledRect(SDL_Surface* surface, const SDL_FPoint& scale,
	                                  const SDL_Rect& srcRect);

	[[nodiscard]] bool drawScaledRects(SDL_Surface* surface, const SDL_FPoint& scale,
	                                   const std::vector<SDL_Rect>& rects = {});

	[[nodiscard]] bool fill(Uint8 r = 0x00, Uint8 g = 0x00, Uint8 b = 0x00, Uint8 a = 0xff);
	[[nodiscard]] bool blit(SDL_Surface* surface, const SDL_Rect& src, SDL_Rect& dst);
	void updateSurface();

  protected:
	SdlWindow(const std::string& title, const SDL_Rect& rect, Uint32 flags);

  private:
	SDL_Window* _window = nullptr;
	Sint32 _offset_x = 0;
	Sint32 _offset_y = 0;
};
