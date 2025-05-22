/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client helper dialogs
 *
 * Copyright 2025 Armin Novak <armin.novak@thincast.com>
 * Copyright 2025 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <cassert>
#include <algorithm>

#include <winpr/cast.h>

#include "sdl_widget_list.hpp"
#include "sdl_input_widget_pair_list.hpp"

static const Uint32 vpadding = 5;

SdlInputWidgetPairList::SdlInputWidgetPairList(const std::string& title,
                                               const std::vector<std::string>& labels,
                                               const std::vector<std::string>& initial,
                                               const std::vector<Uint32>& flags)
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

	if (reset(title, total_width, total_height))
	{
		for (size_t x = 0; x < labels.size(); x++)
		{
			std::shared_ptr<SdlInputWidgetPair> widget(new SdlInputWidgetPair(
			    _renderer, labels[x], initial[x], flags[x], x, widget_width, widget_heigth));
			_list.emplace_back(widget);
		}

		_buttons.populate(_renderer, buttonlabels, buttonids, total_width,
		                  static_cast<Sint32>(input_height), static_cast<Sint32>(widget_width),
		                  static_cast<Sint32>(widget_heigth));
		_buttons.set_highlight(0);
	}
}

ssize_t SdlInputWidgetPairList::next(ssize_t current)
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

bool SdlInputWidgetPairList::valid(ssize_t current) const
{
	if (current < 0)
		return false;
	auto s = static_cast<size_t>(current);
	if (s >= _list.size())
		return false;
	return !_list[s]->readonly();
}

std::shared_ptr<SdlInputWidgetPair> SdlInputWidgetPairList::get(ssize_t index)
{
	if (index < 0)
		return nullptr;
	auto s = static_cast<size_t>(index);
	if (s >= _list.size())
		return nullptr;
	return _list[s];
}

SdlInputWidgetPairList::~SdlInputWidgetPairList()
{
	_list.clear();
	_buttons.clear();
}

bool SdlInputWidgetPairList::updateInternal()
{
	for (auto& btn : _list)
	{
		if (!btn->update())
			return false;
		if (!btn->update())
			return false;
	}

	return true;
}

ssize_t SdlInputWidgetPairList::get_index(const SDL_MouseButtonEvent& button)
{
	const auto x = button.x;
	const auto y = button.y;
	for (size_t i = 0; i < _list.size(); i++)
	{
		auto& cur = _list[i];
		auto r = cur->input_rect();

		if ((x >= r.x) && (x <= r.x + r.w) && (y >= r.y) && (y <= r.y + r.h))
			return WINPR_ASSERTING_INT_CAST(ssize_t, i);
	}
	return -1;
}

int SdlInputWidgetPairList::run(std::vector<std::string>& result)
{
	int res = -1;
	ssize_t LastActiveTextInput = -1;
	ssize_t CurrentActiveTextInput = next(-1);

	if (!_window || !_renderer)
		return -2;

	if (!SDL_StartTextInput(_window.get()))
		return -3;

	try
	{
		bool running = true;
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
					case SDL_EVENT_KEY_UP:
					{
						switch (event.key.key)
						{
							case SDLK_BACKSPACE:
							{
								auto cur = get(CurrentActiveTextInput);
								if (cur)
								{
									if ((event.key.mod & SDL_KMOD_CTRL) != 0)
									{
										if (!cur->set_str(""))
											throw;
									}
									else
									{
										if (!cur->remove_str(1))
											throw;
									}
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
							case SDLK_V:
								if ((event.key.mod & SDL_KMOD_CTRL) != 0)
								{
									auto cur = get(CurrentActiveTextInput);
									if (cur)
									{
										auto text = SDL_GetClipboardText();
										cur->set_str(text);
									}
								}
								break;
							default:
								break;
						}
					}
					break;
					case SDL_EVENT_TEXT_INPUT:
					{
						auto cur = get(CurrentActiveTextInput);
						if (cur)
						{
							if (!cur->append_str(event.text.text))
								throw;
						}
					}
					break;
					case SDL_EVENT_MOUSE_MOTION:
					{
						auto TextInputIndex = get_index(event.button);
						for (auto& cur : _list)
						{
							if (!cur->set_mouseover(false))
								throw;
						}
						if (TextInputIndex >= 0)
						{
							auto& cur = _list[static_cast<size_t>(TextInputIndex)];
							if (!cur->set_mouseover(true))
								throw;
						}

						_buttons.set_mouseover(event.button.x, event.button.y);
					}
					break;
					case SDL_EVENT_MOUSE_BUTTON_DOWN:
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
					case SDL_EVENT_QUIT:
						res = INPUT_BUTTON_CANCEL;
						running = false;
						break;
					default:
						break;
				}
			} while (SDL_PollEvent(&event));

			if (LastActiveTextInput != CurrentActiveTextInput)
			{
				LastActiveTextInput = CurrentActiveTextInput;
			}

			for (auto& cur : _list)
			{
				if (!cur->set_highlight(false))
					throw;
			}
			auto cur = get(CurrentActiveTextInput);
			if (cur)
			{
				if (!cur->set_highlight(true))
					throw;
			}

			auto rc = SDL_RenderPresent(_renderer.get());
			if (!rc)
			{
				SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[%s] SDL_RenderPresent failed with %s",
				            __func__, SDL_GetError());
			}
		}

		for (auto& cur : _list)
			result.push_back(cur->value());
	}
	catch (...)
	{
		res = -2;
	}
	if (!SDL_StopTextInput(_window.get()))
		return -4;

	return res;
}
