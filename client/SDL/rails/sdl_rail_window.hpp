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

#pragma once

#include <stdint.h>

#include <string>

#include <freerdp/types.h>
#include <SDL.h>

#include "sdl_rail_icon.hpp"

class SdlRailWindow
{
  public:
	enum localmove_state
	{
		LMS_NOT_ACTIVE,
		LMS_STARTING,
		LMS_ACTIVE,
		LMS_TERMINATING
	};

	struct localmove
	{
		int root_x;
		int root_y;
		int window_x;
		int window_y;
		enum localmove_state state;
		int direction;
	};

	struct Point
	{
		int32_t x;
		int32_t y;

		Point() : x(0), y(0)
		{
		}
		Point(int32_t x, int32_t y) : x(x), y(y)
		{
		}
	};

	struct Size
	{
		int32_t w;
		int32_t h;

		Size() : w(0), h(0)
		{
		}
		Size(int32_t w, int32_t h) : w(w), h(h)
		{
		}
	};

  public:
	SdlRailWindow(uint64_t id, uint32_t surfaceId, const SDL_Rect& rect);
	SdlRailWindow(const SdlRailWindow& other) = delete;
	SdlRailWindow(SdlRailWindow&& other) noexcept;
	virtual ~SdlRailWindow();

	bool setIcon(const SdlRailIcon& icon, bool replace);
	bool applyStyle(bool cached = true);
	bool setStyle(uint32_t dwStyle, uint32_t dwExStyle);
	bool setRailState(uint32_t state);
	uint32_t railState() const;

	int showState() const;
	bool setShowState(int state);

	bool create();

	bool update(const SDL_Rect& rect);
	bool move(const SDL_Rect& rect);
	bool updateWindowRect(const SDL_Rect& rect);
	bool updateMargins(const SDL_Rect& rect);
	bool updateVisibilityRects(const RECTANGLE_16* rects, size_t count);
	bool updateWindowRects(const RECTANGLE_16* rects, size_t count);

	uint64_t id() const;
	uint64_t owner() const;
	bool setOwner(uint64_t id);

	bool setTitle(const char16_t* str, size_t len);
	bool setTitle(const std::string& title);
	const std::string& title() const;
	const SDL_Rect& rect() const;
	const SDL_Rect& margins() const;
	const SDL_Rect& windowRect() const;
	bool mapped() const;

	enum localmove_state localMoveState() const;
	bool updateLocalMoveState(enum localmove_state state);

	Point clientOffset() const;
	bool setClientOffset(const Point& point);

	Size clientArea() const;
	bool setClientArea(const Size& point);

	Point visibleOffset() const;
	bool setVisibleOffset(const Point& point);

	Point clientDelta() const;
	bool setClientDelta(const Point& point);

	int localMoveType() const;
	bool setLocalMoveType(int type);

	void StartLocalMoveSize(int direction, int x, int y);
	void EndLocalMoveSize();

	bool handleEvent(const SDL_Event& event);

  protected:
	bool setLocalMoveStartPosition(const Point& point);
	const Point& localMoveStartPosition() const;

  private:
	void conditional_update(SDL_Rect& dst, const SDL_Rect& src);
	SDL_Window* get() const;

  private:
	uint64_t _id;
	uint64_t _owner_id;
	uint32_t _surfaceId;
	uint32_t _style;
	uint32_t _ex_style;
	uint32_t _rail_state;
	int _show_state;
	Point _client_offset;
	Size _client_area;
	Point _client_delta;
	Point _visible_offset;
	Point _local_move_start;
	SDL_Rect _rect;
	SDL_Rect _margins;
	SDL_Rect _window_rect;
	std::vector<SDL_Rect> _visibility_rects;
	std::vector<SDL_Rect> _window_rects;
	std::string _title;

	enum localmove_state _local_move_state;
	int _local_move_type;
	bool _use_cached_style = false;
};
