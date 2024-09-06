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

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

#include "sdl_types.hpp"

int sdl_list_monitors(SdlContext* sdl);
BOOL sdl_detect_monitors(SdlContext* sdl, UINT32* pMaxWidth, UINT32* ppMaxHeight);
INT64 sdl_monitor_id_for_index(SdlContext* sdl, UINT32 index);
