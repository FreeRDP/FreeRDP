#pragma once

#include <string>
#include <vector>

#include <SDL.h>

#include "sdl_select.hpp"
#include "sdl_button.hpp"
#include "sdl_buttons.hpp"

class SdlSelectList
{
  public:
	SdlSelectList(const std::string& title, const std::vector<std::string>& labels);
	virtual ~SdlSelectList();

	int run();

	SdlSelectList(const SdlSelectList& other) = delete;
	SdlSelectList(SdlSelectList&& other) = delete;
	SdlSelectList& operator=(const SdlSelectList& other) = delete;
	SdlSelectList& operator=(SdlSelectList&& other) = delete;

  private:
	enum
	{
		INPUT_BUTTON_ACCEPT = 0,
		INPUT_BUTTON_CANCEL = -2
	};

	ssize_t get_index(const SDL_MouseButtonEvent& button);
	bool update_text();
	void reset_mouseover();
	void reset_highlight();

	SDL_Window* _window;
	SDL_Renderer* _renderer;
	std::vector<SdlSelectWidget> _list;
	SdlButtonList _buttons;
};
