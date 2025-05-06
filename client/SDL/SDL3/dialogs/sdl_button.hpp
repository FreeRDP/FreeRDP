#pragma once

#include <string>
#include <memory>

#include "sdl_selectable_widget.hpp"

class SdlButton : public SdlSelectableWidget
{
  public:
	SdlButton(std::shared_ptr<SDL_Renderer>& renderer, const std::string& label, int id,
	          const SDL_FRect& rect);
	SdlButton(SdlButton&& other) noexcept;
	SdlButton(const SdlButton& other) = delete;
	~SdlButton() override;

	SdlButton& operator=(const SdlButton& other) = delete;
	SdlButton& operator=(SdlButton&& other) = delete;

	[[nodiscard]] int id() const;

  private:
	int _id;
};
