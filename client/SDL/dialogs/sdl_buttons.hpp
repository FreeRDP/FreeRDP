#pragma once

#include <vector>
#include <stdint.h>

#include "sdl_button.hpp"

class SdlButtonList
{
  public:
	SdlButtonList();
	virtual ~SdlButtonList();

	bool populate(SDL_Renderer* renderer, const std::vector<std::string>& labels,
	              const std::vector<int>& ids, Sint32 total_width, Sint32 offsetY, Sint32 width,
	              Sint32 heigth);

	bool update(SDL_Renderer* renderer);
	SdlButton* get_selected(const SDL_MouseButtonEvent& button);
	SdlButton* get_selected(Sint32 x, Sint32 y);

	bool set_highlight_next(bool reset = false);
	bool set_highlight(size_t index);
	bool set_mouseover(Sint32 x, Sint32 y);

	void clear();

  private:
	SdlButtonList(const SdlButtonList& other) = delete;
	SdlButtonList(SdlButtonList&& other) = delete;

  private:
	std::vector<SdlButton> _list;
	SdlButton* _highlighted = nullptr;
	size_t _highlight_index = 0;
	SdlButton* _mouseover = nullptr;
};
