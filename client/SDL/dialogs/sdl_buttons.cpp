#include <assert.h>

#include "sdl_buttons.hpp"

static const Uint32 hpadding = 10;

SdlButtonList::SdlButtonList()
{
}

bool SdlButtonList::populate(SDL_Renderer* renderer, const std::vector<std::string>& labels,
                             const std::vector<int>& ids, Sint32 offsetY, Sint32 width,
                             Sint32 height)
{
	assert(renderer);
	assert(width >= 0);
	assert(height >= 0);
	assert(labels.size() == ids.size());

	_list.clear();
	for (size_t x = 0; x < ids.size(); x++)
	{
		const size_t offsetX = x * (static_cast<size_t>(width) + hpadding);
		const SDL_Rect rect = { static_cast<int>(offsetX), offsetY, width, height };
		_list.push_back({ renderer, labels[x], ids[x], rect });
	}
	return true;
}

SdlButtonList::~SdlButtonList()
{
}

SdlButton* SdlButtonList::get_selected(const SDL_MouseButtonEvent& button)
{
	const Sint32 x = button.x;
	const Sint32 y = button.y;

	for (auto& btn : _list)
	{
		auto r = btn.rect();
		if ((x >= r.x) && (x <= r.x + r.w) && (y >= r.y) && (y <= r.y + r.h))
			return &btn;
	}
	return nullptr;
}

void SdlButtonList::clear()
{
	_list.clear();
}

bool SdlButtonList::update(SDL_Renderer* renderer)
{
	assert(renderer);

	for (auto& btn : _list)
	{
		if (!btn.update(renderer))
			return false;
	}
	return true;
}
