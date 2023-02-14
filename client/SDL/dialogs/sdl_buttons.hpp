#pragma once

#include <vector>

#include "sdl_button.hpp"

class SdlButtonList
{
  public:
	SdlButtonList();
	virtual ~SdlButtonList();

	bool populate(SDL_Renderer* renderer, const std::vector<std::string>& labels,
	              const std::vector<int>& ids, Sint32 offsetY, Sint32 width, Sint32 heigth);

	bool update(SDL_Renderer* renderer);
	SdlButton* get_selected(const SDL_MouseButtonEvent& button);

	void clear();

  private:
	SdlButtonList(const SdlButtonList& other) = delete;
	SdlButtonList(SdlButtonList&& other) = delete;

  private:
	std::vector<SdlButton> _list;
};
