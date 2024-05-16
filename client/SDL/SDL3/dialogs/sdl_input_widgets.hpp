#pragma once

#include <string>
#include <vector>
#include <SDL3/SDL.h>

#include "sdl_input.hpp"
#include "sdl_buttons.hpp"

class SdlInputWidgetList
{
  public:
	SdlInputWidgetList(const std::string& title, const std::vector<std::string>& labels,
	                   const std::vector<std::string>& initial, const std::vector<Uint32>& flags);
	virtual ~SdlInputWidgetList();

	int run(std::vector<std::string>& result);

  protected:
	bool update(SDL_Renderer* renderer);
	ssize_t get_index(const SDL_MouseButtonEvent& button);

  private:
	SdlInputWidgetList(const SdlInputWidgetList& other) = delete;
	SdlInputWidgetList(SdlInputWidgetList&& other) = delete;

  private:
	enum
	{
		INPUT_BUTTON_ACCEPT = 1,
		INPUT_BUTTON_CANCEL = -2
	};

  private:
	ssize_t next(ssize_t current);
	[[nodiscard]] bool valid(ssize_t current) const;
	SdlInputWidget* get(ssize_t index);

  private:
	SDL_Window* _window;
	SDL_Renderer* _renderer;
	std::vector<SdlInputWidget> _list;
	SdlButtonList _buttons;
};
