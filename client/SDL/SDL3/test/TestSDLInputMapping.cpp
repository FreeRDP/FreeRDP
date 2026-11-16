/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client captured pointer mapping tests
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

#include "../sdl_input_mapping.hpp"

#include <cmath>
#include <iostream>

static bool expect(const char* name, const SDL_FPoint& pos, const SDL_Rect& source,
                   const SDL_Rect& target, std::optional<SDL_FPoint> expected)
{
	const auto result = sdl_pointer_in_window(pos, source, target);
	if (result.has_value() == expected.has_value() &&
	    (!result || (std::fabs(result->x - expected->x) < 0.001f &&
	                 std::fabs(result->y - expected->y) < 0.001f)))
		return true;
	std::cerr << name << " failed\n";
	return false;
}

int main()
{
	const SDL_Rect dell{ 0, -1080, 1920, 1080 };
	const SDL_Rect m27{ 1920, -1080, 1920, 1080 };
	const SDL_Rect below{ 0, 0, 1470, 956 };
	bool ok = true;

	// The measured captured positions must become destination-local points
	// BEFORE the renderer applies Dell's 1x or M27UP's 2x pixel scale.
	ok &= expect("M27UP to Dell", { -820, 269 }, m27, dell, SDL_FPoint{ 1100, 269 });
	ok &= expect("Dell to M27UP", { 2552, 269 }, dell, m27, SDL_FPoint{ 632, 269 });
	ok &= expect("same window", { 632, 269 }, m27, m27, SDL_FPoint{ 632, 269 });
	ok &= expect("fractional point", { -0.25f, 269.5f }, m27, dell, SDL_FPoint{ 1919.75f, 269.5f });
	ok &= expect("seam belongs to right window", { 1920, 269 }, dell, m27, SDL_FPoint{ 0, 269 });
	ok &= expect("seam outside left window", { 1920, 269 }, dell, dell, std::nullopt);
	ok &= expect("bottom edge", { 632, 1080 }, dell, below, SDL_FPoint{ 632, 0 });
	ok &= expect("below to upper right", { 2552, -811 }, below, m27, SDL_FPoint{ 632, 269 });
	ok &= expect("outside selected surface", { 632, 1200 }, dell, m27, std::nullopt);
	ok &= expect("invalid source geometry", { 0, 0 }, {}, dell, std::nullopt);
	ok &= expect("invalid target geometry", { 0, 0 }, dell, {}, std::nullopt);

	// Moving a window changes the next event's rebasing without cached origins.
	ok &= expect("moved source", { -820, 269 }, { 1920, -980, 1920, 1080 }, dell,
	             SDL_FPoint{ 1100, 369 });
	ok &= expect("negative horizontal origin", { -820, 269 }, { 0, 0, 1920, 1080 },
	             { -1920, 0, 1920, 1080 }, SDL_FPoint{ 1100, 269 });
	return ok ? 0 : 1;
}
