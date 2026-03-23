/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Mouse Pointer
 *
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
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

#include <SDL3/SDL.h>

#include "sdl_types.hpp"

#include <freerdp/graphics.h>

[[nodiscard]] bool sdl_register_pointer(rdpGraphics* graphics);

[[nodiscard]] bool sdl_Pointer_Set_Process(SdlContext* sdl);

void sdl_Pointer_FreeCopy(rdpPointer* pointer);

/** @brief creates a copy when the \ref rdpPointer was already created in \ref PointerNew.
 *
 *  The copy only contains the relevant data (coordinates, bitmap data) required for the SDL client
 * to display the cursor. Callbacks and xor/and mask data are not copied.
 */
WINPR_ATTR_MALLOC(sdl_Pointer_FreeCopy, 1)
rdpPointer* sdl_Pointer_Copy(const rdpPointer* pointer);
