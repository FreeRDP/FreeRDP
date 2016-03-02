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

#ifndef FREERDP_CHANNEL_RDPGFX_CLIENT_COMMON_H
#define FREERDP_CHANNEL_RDPGFX_CLIENT_COMMON_H

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/channels/rdpgfx.h>

const char* rdpgfx_get_cmd_id_string(UINT16 cmdId);
const char* rdpgfx_get_codec_id_string(UINT16 codecId);

UINT rdpgfx_read_header(wStream* s, RDPGFX_HEADER* header);
UINT rdpgfx_write_header(wStream* s, RDPGFX_HEADER* header);

UINT rdpgfx_read_point16(wStream* s, RDPGFX_POINT16* pt16);
UINT rdpgfx_write_point16(wStream* s, RDPGFX_POINT16* point16);

UINT rdpgfx_read_rect16(wStream* s, RECTANGLE_16* rect16);
UINT rdpgfx_write_rect16(wStream* s, RECTANGLE_16* rect16);

UINT rdpgfx_read_color32(wStream* s, RDPGFX_COLOR32* color32);
UINT rdpgfx_write_color32(wStream* s, RDPGFX_COLOR32* color32);

#endif /* FREERDP_CHANNEL_RDPGFX_CLIENT_COMMON_H */

