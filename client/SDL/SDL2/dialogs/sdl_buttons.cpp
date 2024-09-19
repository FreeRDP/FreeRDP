#include <cassert>
#include <algorithm>

#include "sdl_buttons.hpp"

static const Uint32 hpadding = 10;

SdlButtonList::~SdlButtonList() = default;

bool SdlButtonList::populate(SDL_Renderer* renderer, const std::vector<std::string>& labels,
                             const std::vector<int>& ids, Sint32 total_width, Sint32 offsetY,
                             Sint32 width, Sint32 height)
{
	assert(renderer);
	assert(width >= 0);
	assert(height >= 0);
	assert(labels.size() == ids.size());

	_list.clear();
	size_t button_width = ids.size() * (width + hpadding) + hpadding;
	size_t offsetX = total_width - std::min<size_t>(total_width, button_width);
	for (size_t x = 0; x < ids.size(); x++)
	{
		const size_t curOffsetX = offsetX + x * (static_cast<size_t>(width) + hpadding);
		const SDL_Rect rect = { static_cast<int>(curOffsetX), offsetY, width, height };
		_list.emplace_back(renderer, labels[x], ids[x], rect);
	}
	return true;
}

SdlButton* SdlButtonList::get_selected(const SDL_MouseButtonEvent& button)
{
	const Sint32 x = button.x;
	const Sint32 y = button.y;

	return get_selected(x, y);
}

SdlButton* SdlButtonList::get_selected(Sint32 x, Sint32 y)
{
	for (auto& btn : _list)
	{
		auto r = btn.rect();
		if ((x >= r.x) && (x <= r.x + r.w) && (y >= r.y) && (y <= r.y + r.h))
			return &btn;
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
		_highlighted = &element;
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
	_highlighted = &element;
	_highlight_index = ++index % _list.size();
	return true;
}

bool SdlButtonList::set_mouseover(Sint32 x, Sint32 y)
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

bool SdlButtonList::update(SDL_Renderer* renderer)
{
	assert(renderer);

	for (auto& btn : _list)
	{
		if (!btn.update(renderer))
			return false;
	}

	if (_highlighted)
		_highlighted->highlight(renderer);

	if (_mouseover)
		_mouseover->mouseover(renderer);
	return true;
}
