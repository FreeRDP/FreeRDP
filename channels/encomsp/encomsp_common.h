/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Multiparty Virtual Channel
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CHANNEL_ENCOMSP_COMMON_H
#define FREERDP_CHANNEL_ENCOMSP_COMMON_H

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/channels/encomsp.h>

int encomsp_read_header(wStream* s, ENCOMSP_ORDER_HEADER* header);
int encomsp_write_header(wStream* s, ENCOMSP_ORDER_HEADER* header);

int encomsp_read_unicode_string(wStream* s, ENCOMSP_UNICODE_STRING* str);

#endif /* FREERDP_CHANNEL_ENCOMSP_COMMON_H */
