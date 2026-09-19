/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client
 *
 * Copyright 2026 Daniel Nylander <1206564+yeager@users.noreply.github.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include <SDL3/SDL.h>

class SdlFloatbar
{
  public:
	enum class Action
	{
		None,
		Minimize,
		ToggleFullscreen,
		Disconnect
	};

	SdlFloatbar() = default;
	SdlFloatbar(const SdlFloatbar&) = delete;
	SdlFloatbar(SdlFloatbar&&) = delete;
	~SdlFloatbar();

	SdlFloatbar& operator=(const SdlFloatbar&) = delete;
	SdlFloatbar& operator=(SdlFloatbar&&) = delete;

	[[nodiscard]] bool show(SDL_Window* parent);
	void hide();
	[[nodiscard]] bool owns(SDL_WindowID windowId) const;
	[[nodiscard]] Action handleEvent(const SDL_MouseButtonEvent& ev) const;

  private:
	SDL_Window* _window = nullptr;
	SDL_Renderer* _renderer = nullptr;
};
