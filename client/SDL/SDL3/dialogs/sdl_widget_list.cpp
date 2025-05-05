#include "sdl_widget_list.hpp"

bool SdlWidgetList::reset(const std::string& title, size_t width, size_t height)
{
	auto w = WINPR_ASSERTING_INT_CAST(int, width);
	auto h = WINPR_ASSERTING_INT_CAST(int, height);
	SDL_Renderer* renderer = nullptr;
	SDL_Window* window = nullptr;
	auto rc = SDL_CreateWindowAndRenderer(title.c_str(), w, h,
	                                      SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_MOUSE_FOCUS |
	                                          SDL_WINDOW_INPUT_FOCUS,
	                                      &window, &renderer);
	_renderer = std::shared_ptr<SDL_Renderer>(renderer, SDL_DestroyRenderer);
	_window = std::shared_ptr<SDL_Window>(window, SDL_DestroyWindow);
	if (!rc)
		widget_log_error(rc, "SDL_CreateWindowAndRenderer");
	return rc;
}
