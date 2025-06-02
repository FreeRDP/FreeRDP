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
	virtual ~SdlWidgetList();

	virtual bool reset(const std::string& title, size_t width, size_t height);

	[[nodiscard]] virtual bool visible() const;

  protected:
	bool update();
	virtual bool clearWindow();
	virtual bool updateInternal() = 0;

	std::shared_ptr<SDL_Window> _window;
	std::shared_ptr<SDL_Renderer> _renderer;
	SdlButtonList _buttons;
	SDL_Color _backgroundcolor{ 0x38, 0x36, 0x35, 0xff };
};
