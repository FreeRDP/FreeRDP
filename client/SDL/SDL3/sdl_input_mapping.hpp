/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client pointer coordinate mapping
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

#include <optional>
#include <SDL3/SDL_rect.h>

/* Captured coordinates are relative to the source window even when the pointer
 * is over another window. Rebase in logical desktop units, before applying the
 * destination renderer's pixel scale. Bounds are half-open at shared edges. */
[[nodiscard]] inline std::optional<SDL_FPoint>
sdl_pointer_in_window(const SDL_FPoint& pos, const SDL_Rect& source, const SDL_Rect& target)
{
	if (source.w <= 0 || source.h <= 0 || target.w <= 0 || target.h <= 0)
		return std::nullopt;

	const SDL_FPoint local{ pos.x + static_cast<float>(source.x) - static_cast<float>(target.x),
		                    pos.y + static_cast<float>(source.y) - static_cast<float>(target.y) };
	if (local.x >= 0 && local.y >= 0 && local.x < static_cast<float>(target.w) &&
	    local.y < static_cast<float>(target.h))
		return local;
	return std::nullopt;
}
