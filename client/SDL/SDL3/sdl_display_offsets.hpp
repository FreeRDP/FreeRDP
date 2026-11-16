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

	/* Places the primary display at its origin, then walks outwards over its neighbours. */
	inline void update(Offsets& offsets, SDL_DisplayID primary)
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
} // namespace sdl_display_offsets
