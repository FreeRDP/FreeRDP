#include <cassert>
#include <winpr/cast.h>
#include "sdl_select_list.hpp"
#include "../sdl_utils.hpp"

static const Uint32 vpadding = 5;

SdlSelectList::SdlSelectList(const std::string& title, const std::vector<std::string>& labels)
{
	const size_t widget_height = 50;
	const size_t widget_width = 600;

	const size_t total_height = labels.size() * (widget_height + vpadding) + vpadding;
	const size_t height = total_height + widget_height;
	if (reset(title, widget_width, height))
	{
		SDL_FRect rect = { 0, 0, widget_width, widget_height };
		for (auto& label : labels)
		{
			_list.emplace_back(_renderer, label, rect);
			rect.y += widget_height + vpadding;
		}

		const std::vector<int> buttonids = { INPUT_BUTTON_ACCEPT, INPUT_BUTTON_CANCEL };
		const std::vector<std::string> buttonlabels = { "accept", "cancel" };
		_buttons.populate(_renderer, buttonlabels, buttonids, widget_width,
		                  static_cast<Sint32>(total_height), static_cast<Sint32>(widget_width / 2),
		                  static_cast<Sint32>(widget_height));
		_buttons.set_highlight(0);
	}
}

SdlSelectList::~SdlSelectList() = default;

int SdlSelectList::run()
{
	int res = -2;
	ssize_t CurrentActiveTextInput = 0;
	bool running = true;

	if (!_window || !_renderer)
		return -2;
	try
	{
		while (running)
		{
			if (!update())
				throw;

			SDL_Event event = {};
			if (!SDL_WaitEvent(&event))
				throw;
			do
			{
				switch (event.type)
				{
					case SDL_EVENT_KEY_DOWN:
						switch (event.key.key)
						{
							case SDLK_UP:
							case SDLK_BACKSPACE:
								if (CurrentActiveTextInput > 0)
									CurrentActiveTextInput--;
								else if (_list.empty())
									CurrentActiveTextInput = 0;
								else
								{
									auto s = _list.size();
									CurrentActiveTextInput =
									    WINPR_ASSERTING_INT_CAST(ssize_t, s) - 1;
								}
								break;
							case SDLK_DOWN:
							case SDLK_TAB:
								if ((CurrentActiveTextInput < 0) || _list.empty())
									CurrentActiveTextInput = 0;
								else
								{
									auto s = _list.size();
									CurrentActiveTextInput++;
									if (s > 0)
									{
										CurrentActiveTextInput =
										    CurrentActiveTextInput %
										    WINPR_ASSERTING_INT_CAST(ssize_t, s);
									}
								}
								break;
							case SDLK_RETURN:
							case SDLK_RETURN2:
							case SDLK_KP_ENTER:
								running = false;
								res = static_cast<int>(CurrentActiveTextInput);
								break;
							case SDLK_ESCAPE:
								running = false;
								res = INPUT_BUTTON_CANCEL;
								break;
							default:
								break;
						}
						break;
					case SDL_EVENT_MOUSE_MOTION:
					{
						auto TextInputIndex = get_index(event.button);
						reset_mouseover();
						if (TextInputIndex >= 0)
						{
							auto& cur = _list[WINPR_ASSERTING_INT_CAST(size_t, TextInputIndex)];
							if (!cur.mouseover(true))
								throw;
						}

						_buttons.set_mouseover(event.button.x, event.button.y);
					}
					break;
					case SDL_EVENT_MOUSE_BUTTON_DOWN:
					{
						auto button = _buttons.get_selected(event.button);
						if (button)
						{
							running = false;
							if (button->id() == INPUT_BUTTON_CANCEL)
								res = INPUT_BUTTON_CANCEL;
							else
								res = static_cast<int>(CurrentActiveTextInput);
						}
						else
						{
							CurrentActiveTextInput = get_index(event.button);
						}
					}
					break;
					case SDL_EVENT_QUIT:
						res = INPUT_BUTTON_CANCEL;
						running = false;
						break;
					default:
						break;
				}
			} while (SDL_PollEvent(&event));
			reset_highlight();
			if (CurrentActiveTextInput >= 0)
			{
				auto& cur = _list[WINPR_ASSERTING_INT_CAST(size_t, CurrentActiveTextInput)];
				if (!cur.highlight(true))
					throw;
			}

			update();
		}
	}
	catch (...)
	{
		return -1;
	}
	return res;
}

bool SdlSelectList::updateInternal()
{
	for (auto& cur : _list)
	{
		if (!cur.update())
			return false;
	}
	return true;
}

ssize_t SdlSelectList::get_index(const SDL_MouseButtonEvent& button)
{
	const auto x = button.x;
	const auto y = button.y;
	for (size_t i = 0; i < _list.size(); i++)
	{
		auto& cur = _list[i];
		auto r = cur.rect();

		if ((x >= r.x) && (x <= r.x + r.w) && (y >= r.y) && (y <= r.y + r.h))
			return WINPR_ASSERTING_INT_CAST(ssize_t, i);
	}
	return -1;
}

void SdlSelectList::reset_mouseover()
{
	for (auto& cur : _list)
	{
		cur.mouseover(false);
	}
}

void SdlSelectList::reset_highlight()
{
	for (auto& cur : _list)
	{
		cur.highlight(false);
	}
}
