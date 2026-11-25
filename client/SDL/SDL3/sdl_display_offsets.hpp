/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client
 *
 * Copyright 2026 FreeRDP contributors
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

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>

/* Pixel origins of the local displays, derived from their logical (point) bounds and their pixel
 * sizes. Each entry maps a display to its logical bounds and its pixel rectangle; only the pixel
 * origin is computed. */
namespace sdl_display_offsets
{
	using Offsets = std::map<SDL_DisplayID, std::pair<SDL_Rect, SDL_Rect>>;

	inline bool alignX(const SDL_Rect& a, const SDL_Rect& b)
	{
		if (a.x + a.w == b.x)
			return true;
		if (b.x + b.w == a.x)
			return true;
		return false;
	}

	inline bool alignY(const SDL_Rect& a, const SDL_Rect& b)
	{
		if (a.y + a.h == b.y)
			return true;
		if (b.y + b.h == a.y)
			return true;
		return false;
	}

	inline std::vector<SDL_DisplayID>
	updateDisplayOffsetsForNeighbours(Offsets& offsets, SDL_DisplayID id,
	                                  const std::vector<SDL_DisplayID>& ignore = {})
	{
		auto first = offsets.at(id);
		std::vector<SDL_DisplayID> neighbours;

		for (auto& entry : offsets)
		{
			if (entry.first == id)
				continue;
			if (std::find(ignore.begin(), ignore.end(), entry.first) != ignore.end())
				continue;

			bool neighbor = false;
			const bool ax = alignX(entry.second.first, first.first);
			const bool ay = alignY(entry.second.first, first.first);

			/* Offset along the axis that does not touch, converted from logical to pixel
			 * units of the reference display. Without this, a monitor that is adjacent on
			 * one axis but shifted on the other (e.g. a wider monitor above another, or a
			 * rotated monitor that is not top aligned) keeps offset 0 on that axis, the
			 * layout sent to the server no longer matches the local one and pointer input
			 * lands offset by that difference. */
			auto crossOffset = [](int ePos, int fPos, int fLogical, int fPixel) -> int
			{
				if (fLogical <= 0)
					return ePos - fPos;
				return static_cast<int>(
				    std::lround(static_cast<double>(ePos - fPos) * fPixel / fLogical));
			};

			if (ax)
			{
				if (entry.second.first.x < first.first.x)
					entry.second.second.x = first.second.x - entry.second.second.w;
				else
					entry.second.second.x = first.second.x + first.second.w;
				if (!ay)
					entry.second.second.y =
					    first.second.y + crossOffset(entry.second.first.y, first.first.y,
					                                 first.first.h, first.second.h);
				neighbor = true;
			}
			if (ay)
			{
				if (entry.second.first.y < first.first.y)
					entry.second.second.y = first.second.y - entry.second.second.h;
				else
					entry.second.second.y = first.second.y + first.second.h;
				if (!ax)
					entry.second.second.x =
					    first.second.x + crossOffset(entry.second.first.x, first.first.x,
					                                 first.first.w, first.second.w);
				neighbor = true;
			}

			if (neighbor)
				neighbours.push_back(entry.first);
		}
		return neighbours;
	}

	/* Places the primary display at its origin, then walks outwards over its neighbours. With
	 * mixed scales this can leave gaps or overlaps: displays whose edges are merely in line count
	 * as neighbours, and a display can be placed again from a later neighbour. update() uses it
	 * only when placeOnce() gives no valid layout. */
	inline void walk(Offsets& offsets, SDL_DisplayID primary)
	{
		/* 1. Find primary and update all neighbors
		 * 2. For each neighbor update all neighbors
		 * 3. repeat until all displays updated.
		 */
		/* Align display offsets to the primary display origin. */
		auto it = offsets.find(primary);
		if (it != offsets.end())
		{
			it->second.second.x = it->second.first.x;
			it->second.second.y = it->second.first.y;
		}
		std::vector<SDL_DisplayID> handled;
		handled.push_back(primary);

		auto neighbors = updateDisplayOffsetsForNeighbours(offsets, primary);
		while (!neighbors.empty())
		{
			auto neighbor = neighbors.front();
			neighbors.erase(neighbors.begin());

			if (std::find(handled.begin(), handled.end(), neighbor) != handled.end())
				continue;
			handled.push_back(neighbor);

			auto next = updateDisplayOffsetsForNeighbours(offsets, neighbor, handled);
			neighbors.insert(neighbors.end(), next.begin(), next.end());
		}
	}
	inline bool spans(int a, int alen, int b, int blen)
	{
		return (a < b + blen) && (b < a + alen);
	}

	/* The two displays share an edge segment of positive length beside each other (touchX) or
	 * above each other (touchY); edges that are merely in line do not count. */
	inline bool touchX(const SDL_Rect& a, const SDL_Rect& b)
	{
		return ((a.x + a.w == b.x) || (b.x + b.w == a.x)) && spans(a.y, a.h, b.y, b.h);
	}

	inline bool touchY(const SDL_Rect& a, const SDL_Rect& b)
	{
		return ((a.y + a.h == b.y) || (b.y + b.h == a.y)) && spans(a.x, a.w, b.x, b.w);
	}

	inline bool overlap(const SDL_Rect& a, const SDL_Rect& b)
	{
		return spans(a.x, a.w, b.x, b.w) && spans(a.y, a.h, b.y, b.h);
	}

	/* Distance in points, converted to pixels of the display it lies on. */
	inline int scaled(int distance, int logical, int pixel)
	{
		if (logical <= 0)
			return distance;
		return static_cast<int>(std::lround(static_cast<double>(distance) * pixel / logical));
	}

	/* Pixel position of display e along the edge it shares with the placed display f. The
	 * offset between their starts lies on whichever display starts first, so it is converted
	 * by that one. With fromEnd, the offset between their ends is kept instead. */
	inline int along(int ePos, int eLogical, int ePixel, int fPos, int fLogical, int fPixel,
	                 int fPixelPos, bool fromEnd)
	{
		if (!fromEnd)
		{
			const int d = ePos - fPos;
			return fPixelPos +
			       ((d >= 0) ? scaled(d, fLogical, fPixel) : scaled(d, eLogical, ePixel));
		}
		const int d = (ePos + eLogical) - (fPos + fLogical);
		const int end = fPixelPos + fPixel +
		                ((d <= 0) ? scaled(d, fLogical, fPixel) : scaled(d, eLogical, ePixel));
		return end - ePixel;
	}

	inline bool isPlaced(const std::vector<SDL_DisplayID>& placed, SDL_DisplayID id)
	{
		return std::find(placed.begin(), placed.end(), id) != placed.end();
	}

	/* Places each display that shares an edge with display id and is not placed yet next to
	 * it, and returns them. */
	inline std::vector<SDL_DisplayID> placeNeighbours(Offsets& offsets, SDL_DisplayID id,
	                                                  std::vector<SDL_DisplayID>& placed)
	{
		const auto f = offsets.at(id);
		std::vector<SDL_DisplayID> result;
		for (auto& entry : offsets)
		{
			const auto& e = entry.second.first;
			auto& pixel = entry.second.second;
			const bool beside = touchX(e, f.first);
			if (isPlaced(placed, entry.first) || (!beside && !touchY(e, f.first)))
				continue;

			const auto place = [&](bool fromEnd)
			{
				if (beside)
				{
					pixel.x = (e.x < f.first.x) ? f.second.x - pixel.w : f.second.x + f.second.w;
					pixel.y = along(e.y, e.h, pixel.h, f.first.y, f.first.h, f.second.h, f.second.y,
					                fromEnd);
				}
				else
				{
					pixel.y = (e.y < f.first.y) ? f.second.y - pixel.h : f.second.y + f.second.h;
					pixel.x = along(e.x, e.w, pixel.w, f.first.x, f.first.w, f.second.w, f.second.x,
					                fromEnd);
				}
				return std::none_of(placed.begin(), placed.end(), [&](SDL_DisplayID other)
				                    { return overlap(pixel, offsets.at(other).second); });
			};

			/* A 2x display is larger in pixels than in points, so keeping the near edges in
			 * line can overlap a display placed before; keep the far edges in line then. */
			if (!place(false) && !place(true))
				std::ignore = place(false);
			placed.push_back(entry.first);
			result.push_back(entry.first);
		}
		return result;
	}

	/* The walk without its two flaws: only displays sharing an edge segment are neighbours,
	 * and each display is placed once, next to the first placed display it shares an edge
	 * with. Displays not reached keep their position. */
	inline void placeOnce(Offsets& offsets, SDL_DisplayID primary)
	{
		auto& origin = offsets.at(primary);
		origin.second.x = origin.first.x;
		origin.second.y = origin.first.y;
		std::vector<SDL_DisplayID> placed{ primary };
		std::vector<SDL_DisplayID> next{ primary };
		for (size_t i = 0; i < next.size(); i++)
		{
			const auto more = placeNeighbours(offsets, next[i], placed);
			next.insert(next.end(), more.begin(), more.end());
		}
	}

	/* No two selected displays overlap, and they form one area, connected through shared
	 * edges. */
	inline bool valid(const Offsets& offsets, const std::vector<SDL_DisplayID>& selection)
	{
		std::vector<SDL_DisplayID> ids;
		for (const auto& entry : offsets)
		{
			if (selection.empty() || isPlaced(selection, entry.first))
				ids.push_back(entry.first);
		}
		for (size_t i = 0; i < ids.size(); i++)
			for (size_t j = i + 1; j < ids.size(); j++)
				if (overlap(offsets.at(ids[i]).second, offsets.at(ids[j]).second))
					return false;
		if (ids.empty())
			return true;

		std::vector<SDL_DisplayID> reached{ ids.front() };
		for (size_t i = 0; i < reached.size(); i++)
		{
			const auto& a = offsets.at(reached[i]).second;
			for (const auto id : ids)
			{
				const auto& b = offsets.at(id).second;
				if (!isPlaced(reached, id) && (touchX(a, b) || touchY(a, b)))
					reached.push_back(id);
			}
		}
		return reached.size() == ids.size();
	}

	/* Pixel origins of all displays, starting with the primary at its point origin.
	 * selection lists the displays sent to the server (empty: all). The displays are placed
	 * with placeOnce(); if the selected ones then overlap or do not form one area, the walk is
	 * used, as before, and the layout check reports the problem. */
	inline void update(Offsets& offsets, SDL_DisplayID primary,
	                   const std::vector<SDL_DisplayID>& selection = {})
	{
		if (offsets.empty())
			return;
		if (offsets.count(primary) == 0)
			primary = offsets.begin()->first;

		auto placed = offsets;
		placeOnce(placed, primary);
		if (valid(placed, selection))
			offsets = std::move(placed);
		else
			walk(offsets, primary);
	}
} // namespace sdl_display_offsets
