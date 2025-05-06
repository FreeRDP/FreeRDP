#include <cassert>
#include <algorithm>

#include "sdl_buttons.hpp"

static const Uint32 hpadding = 10;

SdlButtonList::~SdlButtonList() = default;

bool SdlButtonList::populate(std::shared_ptr<SDL_Renderer>& renderer,
                             const std::vector<std::string>& labels, const std::vector<int>& ids,
                             Sint32 total_width, Sint32 offsetY, Sint32 width, Sint32 height)
{
	assert(renderer);
	assert(width >= 0);
	assert(height >= 0);
	assert(labels.size() == ids.size());

	_list.clear();
	size_t button_width = ids.size() * (static_cast<size_t>(width) + hpadding) + hpadding;
	size_t offsetX = static_cast<size_t>(total_width) -
	                 std::min<size_t>(static_cast<size_t>(total_width), button_width);
	for (size_t x = 0; x < ids.size(); x++)
	{
		const size_t curOffsetX = offsetX + x * (static_cast<size_t>(width) + hpadding);
		const SDL_FRect rect = { static_cast<float>(curOffsetX), static_cast<float>(offsetY),
			                     static_cast<float>(width), static_cast<float>(height) };
		std::shared_ptr<SdlButton> button(new SdlButton(renderer, labels[x], ids[x], rect));
		_list.emplace_back(button);
	}
	return true;
}

std::shared_ptr<SdlButton> SdlButtonList::get_selected(const SDL_MouseButtonEvent& button)
{
	const auto x = button.x;
	const auto y = button.y;

	return get_selected(x, y);
}

std::shared_ptr<SdlButton> SdlButtonList::get_selected(float x, float y)
{
	for (auto& btn : _list)
	{
		auto r = btn->rect();
		if ((x >= r.x) && (x <= r.x + r.w) && (y >= r.y) && (y <= r.y + r.h))
			return btn;
	}
	return nullptr;
}

bool SdlButtonList::set_highlight_next(bool reset)
{
	if (reset)
		_highlighted = nullptr;
	else
	{
		auto next = _highlight_index++;
		_highlight_index %= _list.size();
		auto& element = _list[next];
		_highlighted = element;
	}
	return true;
}

bool SdlButtonList::set_highlight(size_t index)
{
	if (index >= _list.size())
	{
		_highlighted = nullptr;
		return false;
	}
	auto& element = _list[index];
	_highlighted = element;
	_highlight_index = ++index % _list.size();
	return true;
}

bool SdlButtonList::set_mouseover(float x, float y)
{
	_mouseover = get_selected(x, y);
	return _mouseover != nullptr;
}

void SdlButtonList::clear()
{
	_list.clear();
	_mouseover = nullptr;
	_highlighted = nullptr;
	_highlight_index = 0;
}

bool SdlButtonList::update()
{
	for (auto& btn : _list)
	{
		btn->highlight(btn == _highlighted);
		btn->mouseover(btn == _mouseover);

		if (!btn->update())
			return false;
	}

	return true;
}
