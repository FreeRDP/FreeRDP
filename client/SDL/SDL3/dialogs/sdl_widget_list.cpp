#include "sdl_widget_list.hpp"
#include "sdl_blend_mode_guard.hpp"

SdlWidgetList::~SdlWidgetList() = default;

bool SdlWidgetList::reset(const std::string& title, size_t width, size_t height)
{
	auto w = WINPR_ASSERTING_INT_CAST(int, width);
	auto h = WINPR_ASSERTING_INT_CAST(int, height);
	SDL_Renderer* renderer = nullptr;
	SDL_Window* window = nullptr;
	auto rc = SDL_CreateWindowAndRenderer(
	    title.c_str(), w, h, SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_INPUT_FOCUS, &window, &renderer);
	_renderer = std::shared_ptr<SDL_Renderer>(renderer, SDL_DestroyRenderer);
	_window = std::shared_ptr<SDL_Window>(window, SDL_DestroyWindow);
	if (!rc)
		widget_log_error(rc, "SDL_CreateWindowAndRenderer");
	return rc;
}

bool SdlWidgetList::visible() const
{
	if (!_window || !_renderer)
		return false;

	auto flags = SDL_GetWindowFlags(_window.get());
	return (flags & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)) == 0;
}

bool SdlWidgetList::clearWindow()
{
	if (!_renderer)
		return false;

	SdlBlendModeGuard guard(_renderer, SDL_BLENDMODE_NONE);
	const auto drc = SDL_SetRenderDrawColor(_renderer.get(), _backgroundcolor.r, _backgroundcolor.g,
	                                        _backgroundcolor.b, _backgroundcolor.a);
	if (widget_log_error(drc, "SDL_SetRenderDrawColor"))
		return false;

	const auto rcls = SDL_RenderClear(_renderer.get());
	return !widget_log_error(rcls, "SDL_RenderClear");
}

bool SdlWidgetList::update()
{
	if (!visible())
		return true;

	clearWindow();
	updateInternal();
	if (!_buttons.update())
		return false;
	auto rc = SDL_RenderPresent(_renderer.get());
	if (!rc)
	{
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[%s] SDL_RenderPresent failed with %s", __func__,
		            SDL_GetError());
	}
	return rc;
}
