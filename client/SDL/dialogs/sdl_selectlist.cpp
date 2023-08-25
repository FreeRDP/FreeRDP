#include "sdl_selectlist.hpp"

static const Uint32 vpadding = 5;

SdlSelectList::SdlSelectList(const std::string& title, const std::vector<std::string>& labels)
    : _window(nullptr), _renderer(nullptr)
{
	TTF_Init();

	const size_t widget_height = 50;
	const size_t widget_width = 600;

	const size_t total_height = labels.size() * (widget_height + vpadding) + vpadding;
	_window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
	                           widget_width, total_height + widget_height, 0);
	if (_window == nullptr)
	{
		widget_log_error(-1, "SDL_CreateWindow");
	}
	else
	{
		_renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED);
		if (_renderer == nullptr)
		{
			widget_log_error(-1, "SDL_CreateRenderer");
		}
		else
		{
			SDL_Rect rect = { 0, 0, widget_width, widget_height };
			for (auto& label : labels)
			{
				_list.push_back({ _renderer, label, rect });
				rect.y += widget_height + vpadding;
			}

			const std::vector<int> buttonids = { INPUT_BUTTON_ACCEPT, INPUT_BUTTON_CANCEL };
			const std::vector<std::string> buttonlabels = { "accept", "cancel" };
			_buttons.populate(_renderer, buttonlabels, buttonids, static_cast<Sint32>(total_height),
			                  static_cast<Sint32>(widget_width / 2),
			                  static_cast<Sint32>(widget_height));
		}
	}
}

SdlSelectList::~SdlSelectList()
{
	_list.clear();
	_buttons.clear();
	SDL_DestroyRenderer(_renderer);
	SDL_DestroyWindow(_window);

	TTF_Quit();
}

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
			if (!clear_window(_renderer))
				throw;

			if (!update_text())
				throw;

			if (!_buttons.update(_renderer))
				throw;

			SDL_Event event = { 0 };
			SDL_WaitEvent(&event);
			switch (event.type)
			{
				case SDL_KEYDOWN:
					switch (event.key.keysym.sym)
					{
						case SDLK_UP:
						case SDLK_BACKSPACE:
							if (CurrentActiveTextInput > 0)
								CurrentActiveTextInput--;
							else
								CurrentActiveTextInput = _list.size() - 1;
							break;
						case SDLK_DOWN:
						case SDLK_TAB:
							if (CurrentActiveTextInput < 0)
								CurrentActiveTextInput = 0;
							else
								CurrentActiveTextInput++;
							CurrentActiveTextInput = CurrentActiveTextInput % _list.size();
							break;
						case SDLK_RETURN:
						case SDLK_RETURN2:
						case SDLK_KP_ENTER:
							running = false;
							res = CurrentActiveTextInput;
							break;
						case SDLK_ESCAPE:
							running = false;
							res = INPUT_BUTTON_CANCEL;
							break;
						default:
							break;
					}
					break;
				case SDL_MOUSEMOTION:
				{
					ssize_t TextInputIndex = get_index(event.button);
					reset_mouseover();
					if (TextInputIndex >= 0)
					{
						auto& cur = _list[TextInputIndex];
						if (!cur.set_mouseover(_renderer, true))
							throw;
					}

					auto button = _buttons.get_selected(event.button);
					if (button)
					{
						if (!button->highlight(_renderer))
							throw;
					}
				}
				break;
				case SDL_MOUSEBUTTONDOWN:
				{
					auto button = _buttons.get_selected(event.button);
					if (button)
					{
						running = false;
						if (button->id() == INPUT_BUTTON_CANCEL)
							res = INPUT_BUTTON_CANCEL;
						else
							res = CurrentActiveTextInput;
					}
					else
					{
						CurrentActiveTextInput = get_index(event.button);
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

			reset_highlight();
			if (CurrentActiveTextInput >= 0)
			{
				auto& cur = _list[CurrentActiveTextInput];
				if (!cur.set_highlight(_renderer, true))
					throw;
			}

			SDL_RenderPresent(_renderer);
		}
	}
	catch (...)
	{
		return -1;
	}
	return res;
}

ssize_t SdlSelectList::get_index(const SDL_MouseButtonEvent& button)
{
	const Sint32 x = button.x;
	const Sint32 y = button.y;
	for (size_t i = 0; i < _list.size(); i++)
	{
		auto& cur = _list[i];
		auto r = cur.rect();

		if ((x >= r.x) && (x <= r.x + r.w) && (y >= r.y) && (y <= r.y + r.h))
			return i;
	}
	return -1;
}

bool SdlSelectList::update_text()
{
	for (auto& cur : _list)
	{
		if (!cur.update_text(_renderer))
			return false;
	}

	return true;
}

void SdlSelectList::reset_mouseover()
{
	for (auto& cur : _list)
	{
		cur.set_mouseover(_renderer, false);
	}
}

void SdlSelectList::reset_highlight()
{
	for (auto& cur : _list)
	{
		cur.set_highlight(_renderer, false);
	}
}
