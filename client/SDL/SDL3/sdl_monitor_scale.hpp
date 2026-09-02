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

#include <map>
#include <string>

#include <SDL3/SDL.h>

#include <freerdp/settings_types.h>

struct SdlMonitorScaleOverride
{
	SDL_DisplayID displayId = 0;
	UINT32 desktopScaleFactor = 100;
	UINT32 deviceScaleFactor = 100;
};

using SdlMonitorScaleOverrides = std::map<SDL_DisplayID, SdlMonitorScaleOverride>;

/**
 * Parse a /monitor-scale value.
 *
 * The output map is only changed after the complete value has been validated.
 * On failure, error contains a user-facing description of the invalid input.
 */
[[nodiscard]] bool sdl_parse_monitor_scale_overrides(const char* value,
                                                     SdlMonitorScaleOverrides& overrides,
                                                     std::string& error);

/** Apply an ID-matched override to an rdpMonitor. */
[[nodiscard]] bool sdl_apply_monitor_scale_override(const SdlMonitorScaleOverrides& overrides,
                                                    rdpMonitor& monitor);
