#pragma once

#include <memory>

#include <SDL3/SDL.h>
#include <winpr/assert.h>

#include "sdl_buttons.hpp"

class SdlWidgetList
{
  public:
	SdlWidgetList() = default;

	SdlWidgetList(const SdlWidgetList& other) = delete;
	SdlWidgetList(SdlWidgetList&& other) = delete;
	SdlWidgetList& operator=(const SdlWidgetList& other) = delete;
	SdlWidgetList& operator=(SdlWidgetList&& other) = delete;

	bool reset(const std::string& title, size_t width, size_t height);

  protected:
	std::shared_ptr<SDL_Window> _window{};
	std::shared_ptr<SDL_Renderer> _renderer{};
	SdlButtonList _buttons;
};
