/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Graphics Pipeline Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#ifndef FREERDP_CHANNEL_RDPGFX_COMMON_H
#define FREERDP_CHANNEL_RDPGFX_COMMON_H

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/config.h>
#include <freerdp/channels/rdpgfx.h>
#include <freerdp/api.h>
#include <freerdp/utils/gfx.h>

FREERDP_LOCAL UINT rdpgfx_read_header(wStream* s, RDPGFX_HEADER* header);
FREERDP_LOCAL UINT rdpgfx_write_header(wStream* s, const RDPGFX_HEADER* header);

FREERDP_LOCAL UINT rdpgfx_read_point16(wStream* s, RDPGFX_POINT16* pt16);
FREERDP_LOCAL UINT rdpgfx_write_point16(wStream* s, const RDPGFX_POINT16* point16);

FREERDP_LOCAL UINT rdpgfx_read_rect16(wStream* s, RECTANGLE_16* rect16);
FREERDP_LOCAL UINT rdpgfx_write_rect16(wStream* s, const RECTANGLE_16* rect16);

FREERDP_LOCAL UINT rdpgfx_read_color32(wStream* s, RDPGFX_COLOR32* color32);
FREERDP_LOCAL UINT rdpgfx_write_color32(wStream* s, const RDPGFX_COLOR32* color32);

#ifdef WITH_DEBUG_RDPGFX
#define DEBUG_RDPGFX(_LOGGER, ...) WLog_Print(_LOGGER, WLOG_DEBUG, __VA_ARGS__)
#else
#define DEBUG_RDPGFX(_LOGGER, ...) \
	do                             \
	{                              \
	} while (0)
#endif

#endif /* FREERDP_CHANNEL_RDPGFX_COMMON_H */
