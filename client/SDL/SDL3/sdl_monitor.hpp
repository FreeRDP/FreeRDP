/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Monitor Handling
 *
 * Copyright 2023 Armin Novak <anovak@thincast.com>
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

#include <vector>

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

#include "sdl_types.hpp"

[[nodiscard]] int sdl_list_monitors(SdlContext* sdl);
[[nodiscard]] BOOL sdl_detect_monitors(SdlContext* sdl, UINT32* pMaxWidth, UINT32* ppMaxHeight);

/** @brief parses a /vmonitors:<w>x<h>@<x>x<y>[,<w>x<h>@<x>x<y>...] value into a list of virtual
 *         monitor definitions. Returns false and logs an error on any malformed entry. */
[[nodiscard]] bool sdl_parse_vmonitors(const char* value, std::vector<rdpMonitor>& monitors);

/** @brief true if every monitor is reachable from the others via touching/overlapping edges or
 *         corners, i.e. the layout forms a single connected region with no isolated islands. */
[[nodiscard]] bool monitorsAreContiguous(const std::vector<rdpMonitor>& monitors);

/** @brief true if any two monitors' rectangles have a positive-area intersection; on true, `a`
 *         and `b` are set to the offending pair's indices. */
[[nodiscard]] bool monitorsHaveOverlap(const std::vector<rdpMonitor>& monitors, size_t& a,
                                       size_t& b);
