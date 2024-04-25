/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL RAIL
 *
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#include <codecvt>
#include <locale>

#include "sdl_rail_window.hpp"
#include "../sdl_utils.hpp"

SdlRailWindow::SdlRailWindow(uint64_t id, uint32_t surfaceId, const SDL_Rect& rect)
    : _id(id), _surfaceId(surfaceId), _style(0), _ex_style(0), _rect(rect),
      _local_move_state(LMS_NOT_ACTIVE)
{
}

SdlRailWindow::SdlRailWindow(SdlRailWindow&& other) noexcept
    : _id(std::move(other._id)), _owner_id(std::move(other._owner_id)),
      _surfaceId(std::move(other._surfaceId)), _style(std::move(other._style)),
      _ex_style(std::move(other._ex_style)), _rail_state(std::move(other._rail_state)),
      _show_state(std::move(other._show_state)), _client_offset(std::move(other._client_offset)),
      _client_area(std::move(other._client_area)), _client_delta(std::move(other._client_delta)),
      _visible_offset(std::move(other._visible_offset)),
      _local_move_start(std::move(other._local_move_start)), _rect(std::move(other._rect)),
      _margins(std::move(other._margins)), _window_rect(std::move(other._window_rect)),
      _visibility_rects(std::move(other._visibility_rects)),
      _window_rects(std::move(other._window_rects)), _title(std::move(other._title)),
      _local_move_state(std::move(other._local_move_state)),
      _local_move_type(std::move(other._local_move_type))
{
}

SdlRailWindow::~SdlRailWindow()
{
}

bool SdlRailWindow::setIcon(const SdlRailIcon& icon, bool replace)
{
	// TODO
	return true;
}

bool SdlRailWindow::applyStyle(bool cached)
{
	_use_cached_style = cached;
	return sdl_push_user_event(SDL_USEREVENT_RAILS_APPLY_STYLE, this);
}

bool SdlRailWindow::setStyle(uint32_t dwStyle, uint32_t dwExStyle)
{
	_style = dwStyle;
	_ex_style = dwExStyle;
	return true;
}

bool SdlRailWindow::setRailState(uint32_t state)
{
	_rail_state = state;
	return true;
}

uint32_t SdlRailWindow::railState() const
{
	return _rail_state;
}

int SdlRailWindow::showState() const
{
	return _show_state;
}

bool SdlRailWindow::setShowState(int state)
{
	_show_state = state;
	return true;
}

bool SdlRailWindow::create()
{
	// TODO
	return true;
}

bool SdlRailWindow::update(const SDL_Rect& rect)
{
	// TODO
	return true;
}

bool SdlRailWindow::move(const SDL_Rect& rect)
{
	// TODO
	return true;
}

bool SdlRailWindow::updateWindowRect(const SDL_Rect& rect)
{
	conditional_update(_window_rect, rect);
	return true;
}

bool SdlRailWindow::updateMargins(const SDL_Rect& rect)
{
	conditional_update(_margins, rect);
	return true;
}

bool SdlRailWindow::updateVisibilityRects(const RECTANGLE_16* rects, size_t count)
{
	_visibility_rects.clear();
	for (size_t x = 0; x < count; x++)
	{
		const auto& rect = rects[x];
		_visibility_rects.push_back(
		    { rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top });
	}

	return true;
}

bool SdlRailWindow::updateWindowRects(const RECTANGLE_16* rects, size_t count)
{
	_window_rects.clear();
	for (size_t x = 0; x < count; x++)
	{
		const auto& rect = rects[x];
		_window_rects.push_back(
		    { rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top });
	}

	return true;
}

uint64_t SdlRailWindow::id() const
{
	return _id;
}

uint64_t SdlRailWindow::owner() const
{
	return _owner_id;
}

bool SdlRailWindow::setOwner(uint64_t id)
{
	_owner_id = id;
	return true;
}

bool SdlRailWindow::setTitle(const char16_t* str, size_t len)
{
	std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> ucs2conv;
	auto title = ucs2conv.to_bytes(str, str + len / sizeof(char16_t));
	return setTitle(title);
}

bool SdlRailWindow::setTitle(const std::string& title)
{
	_title = title;
	return true;
}

const std::string& SdlRailWindow::title() const
{
	return _title;
}

const SDL_Rect& SdlRailWindow::rect() const
{
	return _rect;
}

const SDL_Rect& SdlRailWindow::margins() const
{
	return _margins;
}

const SDL_Rect& SdlRailWindow::windowRect() const
{
	return _window_rect;
}

bool SdlRailWindow::mapped() const
{
	// TODO
	return false;
}

int SdlRailWindow::localMoveType() const
{
	return _local_move_type;
}

bool SdlRailWindow::setLocalMoveType(int type)
{
	_local_move_type = type;
	return true;
}

void SdlRailWindow::StartLocalMoveSize(int direction, int x, int y)
{
	if (localMoveState() != LMS_NOT_ACTIVE)
		return;

	setLocalMoveStartPosition({ x, y });
	setLocalMoveType(direction);
	updateLocalMoveState(LMS_STARTING);

	// TODO
}

void SdlRailWindow::EndLocalMoveSize()
{
	if (localMoveState() == LMS_NOT_ACTIVE)
		return;

	if (localMoveState() == LMS_STARTING)
	{
		// TODO
	}

	updateLocalMoveState(LMS_NOT_ACTIVE);
}

bool SdlRailWindow::handleEvent(const SDL_Event& event)
{
	auto w = get();
	if (!w)
		return false;

	switch (event.type)
	{
		case SDL_USEREVENT_RAILS_APPLY_STYLE:
			// TODO
			SDL_SetWindowBordered(w, SDL_TRUE);
			return true;
		default:
			return false;
	}
}

bool SdlRailWindow::setLocalMoveStartPosition(const Point& point)
{
	_local_move_start = point;
	return true;
}

const SdlRailWindow::Point& SdlRailWindow::localMoveStartPosition() const
{
	return _local_move_start;
}

void SdlRailWindow::conditional_update(SDL_Rect& dst, const SDL_Rect& src)
{
	if (src.x >= 0)
		dst.x = src.x;
	if (src.y >= 0)
		dst.y = src.y;
	if (src.w >= 0)
		dst.w = src.w;
	if (src.h >= 0)
		dst.h = src.h;
}

SDL_Window* SdlRailWindow::get() const
{
	return SDL_GetWindowFromID(_id);
}

SdlRailWindow::localmove_state SdlRailWindow::localMoveState() const
{
	return _local_move_state;
}

bool SdlRailWindow::updateLocalMoveState(localmove_state state)
{
	_local_move_state = state;
	return true;
}

SdlRailWindow::Point SdlRailWindow::clientOffset() const
{
	return _client_offset;
}

bool SdlRailWindow::setClientOffset(const Point& point)
{
	_client_offset = point;
	return true;
}

SdlRailWindow::Size SdlRailWindow::clientArea() const
{
	return _client_area;
}

bool SdlRailWindow::setClientArea(const Size& point)
{
	_client_area = point;
	return true;
}

SdlRailWindow::Point SdlRailWindow::visibleOffset() const
{
	return _visible_offset;
}

bool SdlRailWindow::setVisibleOffset(const Point& point)
{
	_visible_offset = point;
	return true;
}

SdlRailWindow::Point SdlRailWindow::clientDelta() const
{
	return _client_delta;
}

bool SdlRailWindow::setClientDelta(const Point& point)
{
	_client_delta = point;
	return true;
}
