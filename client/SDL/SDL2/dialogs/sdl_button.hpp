#pragma once

#include <string>

#include "sdl_widget.hpp"

class SdlButton : public SdlWidget
{
  public:
	SdlButton(SDL_Renderer* renderer, std::string label, int id, SDL_Rect rect);
	SdlButton(const SdlButton& other) = delete;
	SdlButton(SdlButton&& other) noexcept;
	~SdlButton() override;

	SdlButton& operator=(const SdlButton& other) = delete;
	SdlButton& operator=(SdlButton&& other) = delete;

	bool highlight(SDL_Renderer* renderer);
	bool mouseover(SDL_Renderer* renderer);
	bool update(SDL_Renderer* renderer);

	[[nodiscard]] int id() const;

  private:
	std::string _name;
	int _id;
};
