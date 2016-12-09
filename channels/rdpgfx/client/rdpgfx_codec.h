/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Graphics Pipeline Extension
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CHANNEL_RDPGFX_CLIENT_CODEC_H
#define FREERDP_CHANNEL_RDPGFX_CLIENT_CODEC_H

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/channels/rdpgfx.h>
#include <freerdp/api.h>

#include "rdpgfx_main.h"

FREERDP_LOCAL UINT rdpgfx_decode(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd);

#endif /* FREERDP_CHANNEL_RDPGFX_CLIENT_CODEC_H */
