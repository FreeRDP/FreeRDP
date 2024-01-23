#pragma once

#include <string>

#include "sdl_widget.hpp"

class SdlButton : public SdlWidget
{
  public:
	SdlButton(SDL_Renderer* renderer, const std::string& label, int id, SDL_Rect rect);
	SdlButton(SdlButton&& other) noexcept;
	virtual ~SdlButton() override;

	bool highlight(SDL_Renderer* renderer);
	bool mouseover(SDL_Renderer* renderer);
	bool update(SDL_Renderer* renderer);

	int id() const;

  private:
	SdlButton(const SdlButton& other) = delete;

	std::string _name;
	int _id;
};
