/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDPEDISP Virtual Channel Extension
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

#ifndef FREERDP_CHANNEL_DISP_COMMON_H
#define FREERDP_CHANNEL_DISP_COMMON_H

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/channels/disp.h>
#include <freerdp/api.h>

FREERDP_LOCAL UINT disp_read_header(wStream* s, DISPLAY_CONTROL_HEADER* header);
FREERDP_LOCAL UINT disp_write_header(wStream* s, const DISPLAY_CONTROL_HEADER* header);

#endif /* FREERDP_CHANNEL_DISP_COMMON_H */
