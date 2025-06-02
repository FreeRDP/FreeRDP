#pragma once

#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "sdl_widget_list.hpp"
#include "sdl_select.hpp"
#include "sdl_button.hpp"
#include "sdl_buttons.hpp"

class SdlSelectList : public SdlWidgetList
{
  public:
	SdlSelectList(const std::string& title, const std::vector<std::string>& labels);
	SdlSelectList(const SdlSelectList& other) = delete;
	SdlSelectList(SdlSelectList&& other) = delete;
	~SdlSelectList() override;

	SdlSelectList& operator=(const SdlSelectList& other) = delete;
	SdlSelectList& operator=(SdlSelectList&& other) = delete;

	int run();

  protected:
	bool updateInternal() override;

  private:
	enum
	{
		INPUT_BUTTON_ACCEPT = 0,
		INPUT_BUTTON_CANCEL = -2
	};

	ssize_t get_index(const SDL_MouseButtonEvent& button);
	void reset_mouseover();
	void reset_highlight();

	std::vector<SdlSelectWidget> _list;
};
