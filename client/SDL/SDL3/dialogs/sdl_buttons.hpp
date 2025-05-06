#pragma once

#include <vector>
#include <cstdint>
#include <memory>

#include "sdl_button.hpp"

class SdlButtonList
{
  public:
	SdlButtonList() = default;
	SdlButtonList(const SdlButtonList& other) = delete;
	SdlButtonList(SdlButtonList&& other) = delete;
	virtual ~SdlButtonList();

	SdlButtonList& operator=(const SdlButtonList& other) = delete;
	SdlButtonList& operator=(SdlButtonList&& other) = delete;

	bool populate(std::shared_ptr<SDL_Renderer>& renderer, const std::vector<std::string>& labels,
	              const std::vector<int>& ids, Sint32 total_width, Sint32 offsetY, Sint32 width,
	              Sint32 height);

	bool update();
	std::shared_ptr<SdlButton> get_selected(const SDL_MouseButtonEvent& button);
	std::shared_ptr<SdlButton> get_selected(float x, float y);

	bool set_highlight_next(bool reset = false);
	bool set_highlight(size_t index);
	bool set_mouseover(float x, float y);

	void clear();

  private:
	std::vector<std::shared_ptr<SdlButton>> _list;
	std::shared_ptr<SdlButton> _highlighted = nullptr;
	size_t _highlight_index = 0;
	std::shared_ptr<SdlButton> _mouseover = nullptr;
};
