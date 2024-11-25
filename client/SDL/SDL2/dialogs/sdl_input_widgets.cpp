#include <cassert>
#include <limits>
#include <algorithm>
#include <cinttypes>

#include "sdl_input_widgets.hpp"

static const Uint32 vpadding = 5;

SdlInputWidgetList::SdlInputWidgetList(const std::string& title,
                                       const std::vector<std::string>& labels,
                                       const std::vector<std::string>& initial,
                                       const std::vector<Uint32>& flags)
    : _window(nullptr), _renderer(nullptr)
{
	assert(labels.size() == initial.size());
	assert(labels.size() == flags.size());
	const std::vector<int> buttonids = { INPUT_BUTTON_ACCEPT, INPUT_BUTTON_CANCEL };
	const std::vector<std::string> buttonlabels = { "accept", "cancel" };

	const size_t widget_width = 300;
	const size_t widget_heigth = 50;

	const size_t total_width = widget_width + widget_width;
	const size_t input_height = labels.size() * (widget_heigth + vpadding) + vpadding;
	const size_t total_height = input_height + widget_heigth;

	assert(total_width <= INT32_MAX);
	assert(total_height <= INT32_MAX);
	Uint32 wflags = SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_INPUT_FOCUS;
	auto rc =
	    SDL_CreateWindowAndRenderer(static_cast<int>(total_width), static_cast<int>(total_height),
	                                wflags, &_window, &_renderer);
	if (rc != 0)
		widget_log_error(rc, "SDL_CreateWindowAndRenderer");
	else
	{
		SDL_SetWindowTitle(_window, title.c_str());
		for (size_t x = 0; x < labels.size(); x++)
			_list.emplace_back(_renderer, labels[x], initial[x], flags[x], x, widget_width,
			                   widget_heigth);

		_buttons.populate(_renderer, buttonlabels, buttonids, total_width,
		                  static_cast<Sint32>(input_height), static_cast<Sint32>(widget_width),
		                  static_cast<Sint32>(widget_heigth));
		_buttons.set_highlight(0);
	}
}

ssize_t SdlInputWidgetList::next(ssize_t current)
{
	size_t iteration = 0;
	auto val = static_cast<size_t>(current);

	do
	{
		if (iteration >= _list.size())
			return -1;

		if (iteration == 0)
		{
			if (current < 0)
				val = 0;
			else
				val++;
		}
		else
			val++;
		iteration++;
		val %= _list.size();
	} while (!valid(static_cast<ssize_t>(val)));
	return static_cast<ssize_t>(val);
}

bool SdlInputWidgetList::valid(ssize_t current) const
{
	if (current < 0)
		return false;
	auto s = static_cast<size_t>(current);
	if (s >= _list.size())
		return false;
	return !_list[s].readonly();
}

SdlInputWidget* SdlInputWidgetList::get(ssize_t index)
{
	if (index < 0)
		return nullptr;
	auto s = static_cast<size_t>(index);
	if (s >= _list.size())
		return nullptr;
	return &_list[s];
}

SdlInputWidgetList::~SdlInputWidgetList()
{
	_list.clear();
	_buttons.clear();
	SDL_DestroyRenderer(_renderer);
	SDL_DestroyWindow(_window);
}

bool SdlInputWidgetList::update(SDL_Renderer* renderer)
{
	for (auto& btn : _list)
	{
		if (!btn.update_label(renderer))
			return false;
		if (!btn.update_input(renderer))
			return false;
	}

	return _buttons.update(renderer);
}

ssize_t SdlInputWidgetList::get_index(const SDL_MouseButtonEvent& button)
{
	const Sint32 x = button.x;
	const Sint32 y = button.y;

	assert(_list.size() <= std::numeric_limits<ssize_t>::max());
	for (size_t i = 0; i < _list.size(); i++)
	{
		auto& cur = _list[i];
		auto r = cur.input_rect();

		if ((x >= r.x) && (x <= r.x + r.w) && (y >= r.y) && (y <= r.y + r.h))
			return static_cast<ssize_t>(i);
	}
	return -1;
}

int SdlInputWidgetList::run(std::vector<std::string>& result)
{
	int res = -1;
	ssize_t LastActiveTextInput = -1;
	ssize_t CurrentActiveTextInput = next(-1);

	if (!_window || !_renderer)
		return -2;

	try
	{
		bool running = true;
		std::vector<SDL_Keycode> pressed;
		while (running)
		{
			if (!clear_window(_renderer))
				throw;

			if (!update(_renderer))
				throw;

			if (!_buttons.update(_renderer))
				throw;

			SDL_Event event = {};
			SDL_WaitEvent(&event);
			switch (event.type)
			{
				case SDL_KEYUP:
				{
					auto it = std::remove(pressed.begin(), pressed.end(), event.key.keysym.sym);
					pressed.erase(it, pressed.end());

					switch (event.key.keysym.sym)
					{
						case SDLK_BACKSPACE:
						{
							auto cur = get(CurrentActiveTextInput);
							if (cur)
							{
								if (!cur->remove_str(_renderer, 1))
									throw;
							}
						}
						break;
						case SDLK_TAB:
							CurrentActiveTextInput = next(CurrentActiveTextInput);
							break;
						case SDLK_RETURN:
						case SDLK_RETURN2:
						case SDLK_KP_ENTER:
							running = false;
							res = INPUT_BUTTON_ACCEPT;
							break;
						case SDLK_ESCAPE:
							running = false;
							res = INPUT_BUTTON_CANCEL;
							break;
						case SDLK_v:
							if (pressed.size() == 2)
							{
								if ((pressed[0] == SDLK_LCTRL) || (pressed[0] == SDLK_RCTRL))
								{
									auto cur = get(CurrentActiveTextInput);
									if (cur)
									{
										auto text = SDL_GetClipboardText();
										cur->set_str(_renderer, text);
									}
								}
							}
							break;
						default:
							break;
					}
				}
				break;
				case SDL_KEYDOWN:
					pressed.push_back(event.key.keysym.sym);
					break;
				case SDL_TEXTINPUT:
				{
					auto cur = get(CurrentActiveTextInput);
					if (cur)
					{
						if (!cur->append_str(_renderer, event.text.text))
							throw;
					}
				}
				break;
				case SDL_MOUSEMOTION:
				{
					auto TextInputIndex = get_index(event.button);
					for (auto& cur : _list)
					{
						if (!cur.set_mouseover(_renderer, false))
							throw;
					}
					if (TextInputIndex >= 0)
					{
						auto& cur = _list[static_cast<size_t>(TextInputIndex)];
						if (!cur.set_mouseover(_renderer, true))
							throw;
					}

					_buttons.set_mouseover(event.button.x, event.button.y);
				}
				break;
				case SDL_MOUSEBUTTONDOWN:
				{
					auto val = get_index(event.button);
					if (valid(val))
						CurrentActiveTextInput = val;

					auto button = _buttons.get_selected(event.button);
					if (button)
					{
						running = false;
						if (button->id() == INPUT_BUTTON_CANCEL)
							res = INPUT_BUTTON_CANCEL;
						else
							res = INPUT_BUTTON_ACCEPT;
					}
				}
				break;
				case SDL_QUIT:
					res = INPUT_BUTTON_CANCEL;
					running = false;
					break;
				default:
					break;
			}

			if (LastActiveTextInput != CurrentActiveTextInput)
			{
				if (CurrentActiveTextInput < 0)
					SDL_StopTextInput();
				else
					SDL_StartTextInput();
				LastActiveTextInput = CurrentActiveTextInput;
			}

			for (auto& cur : _list)
			{
				if (!cur.set_highlight(_renderer, false))
					throw;
			}
			auto cur = get(CurrentActiveTextInput);
			if (cur)
			{
				if (!cur->set_highlight(_renderer, true))
					throw;
			}

			SDL_RenderPresent(_renderer);
		}

		for (auto& cur : _list)
			result.push_back(cur.value());
	}
	catch (...)
	{
		res = -2;
	}

	return res;
}
