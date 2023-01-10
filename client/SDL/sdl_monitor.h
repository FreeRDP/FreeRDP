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

#ifndef FREERDP_CLIENT_SDL_MONITOR_H
#define FREERDP_CLIENT_SDL_MONITOR_H

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

#include "sdl_freerdp.h"

int sdl_list_monitors(sdlContext* sdl);
BOOL sdl_detect_monitors(sdlContext* sdl, UINT32* pWidth, UINT32* pHeight);

#endif /* FREERDP_CLIENT_SDL_MONITOR_H */
