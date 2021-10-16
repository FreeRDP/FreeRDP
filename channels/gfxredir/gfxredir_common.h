/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDPXXXX Remote App Graphics Redirection Virtual Channel Extension
 *
 * Copyright 2020 Microsoft
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

#ifndef FREERDP_CHANNEL_GFXREDIR_COMMON_H
#define FREERDP_CHANNEL_GFXREDIR_COMMON_H

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/channels/gfxredir.h>
#include <freerdp/api.h>

FREERDP_LOCAL UINT gfxredir_read_header(wStream* s, GFXREDIR_HEADER* header);
FREERDP_LOCAL UINT gfxredir_write_header(wStream* s, const GFXREDIR_HEADER* header);

#endif /* FREERDP_CHANNEL_GFXREDIR_COMMON_H */
