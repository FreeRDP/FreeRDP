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

	[[nodiscard]] bool show(SDL_Window* parent, bool sticky, bool defaultVisible);
	void hide();
	[[nodiscard]] bool owns(SDL_WindowID windowId) const;
	[[nodiscard]] bool ownsParent(SDL_WindowID windowId) const;
	[[nodiscard]] bool handleParentMotion(const SDL_MouseMotionEvent& ev);
	[[nodiscard]] bool redraw();
	[[nodiscard]] Action handleEvent(const SDL_MouseButtonEvent& ev) const;

  private:
	[[nodiscard]] bool render();
	[[nodiscard]] bool setShown(bool shown);

	SDL_Window* _parent = nullptr;
	SDL_Window* _window = nullptr;
	SDL_Renderer* _renderer = nullptr;
	bool _active = false;
	bool _sticky = false;
};
