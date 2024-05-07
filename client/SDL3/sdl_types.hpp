/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client
 *
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
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

#include <freerdp/freerdp.h>

class SdlContext;

typedef struct
{
	rdpClientContext common;
	SdlContext* sdl;
} sdl_rdp_context;

static inline SdlContext* get_context(void* ctx)
{
	if (!ctx)
		return nullptr;
	auto sdl = static_cast<sdl_rdp_context*>(ctx);
	return sdl->sdl;
}

static inline SdlContext* get_context(rdpContext* ctx)
{
	if (!ctx)
		return nullptr;
	auto sdl = reinterpret_cast<sdl_rdp_context*>(ctx);
	return sdl->sdl;
}
