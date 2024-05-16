#pragma once

#include <string>

#include "sdl_widget.hpp"

class SdlButton : public SdlWidget
{
  public:
	SdlButton(SDL_Renderer* renderer, const std::string& label, int id, const SDL_FRect& rect);
	SdlButton(SdlButton&& other) noexcept;
	~SdlButton() override = default;

	bool highlight(SDL_Renderer* renderer);
	bool mouseover(SDL_Renderer* renderer);
	bool update(SDL_Renderer* renderer);

	[[nodiscard]] int id() const;

  private:
	SdlButton(const SdlButton& other) = delete;

  private:
	std::string _name;
	int _id;
};
