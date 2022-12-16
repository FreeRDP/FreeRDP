/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * GFX Utils - Helper functions converting something to string
 *
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
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

#ifndef FREERDP_UTILS_GFX_H
#define FREERDP_UTILS_GFX_H

#include <winpr/wtypes.h>
#include <freerdp/api.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API const char* rdpgfx_get_cmd_id_string(UINT16 cmdId);

	FREERDP_API const char* rdpgfx_get_codec_id_string(UINT16 codecId);

#ifdef __cplusplus
}
#endif

#endif
