/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client: IWTSBitmapRenderService
 *
 * Copyright 2026 David Fort <contact@hardening-consulting.com>
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

#include <freerdp/wts_services.h>

class SdlContext;

/** @brief rdpContext::bitmapRenderService for `sdl` (one reference for the caller): each
 *  GetMappedRenderer() is a renderer SdlContext draws where the mapping's geometry says */
[[nodiscard]] IWTSBitmapRenderService* sdl_render_service_new(SdlContext* sdl);

/** @brief before `sdl` goes: the service and its renderers, which channels may keep, answer
 *  E_UNEXPECTED from then on */
void sdl_render_service_detach(IWTSBitmapRenderService* service);
