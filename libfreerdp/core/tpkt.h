/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Transport Packets (TPKTs)
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_LIB_CORE_TPKT_H
#define FREERDP_LIB_CORE_TPKT_H

#include "tpdu.h"
#include "transport.h"

#include <winpr/stream.h>
#include <freerdp/api.h>

#define TPKT_HEADER_LENGTH 4

FREERDP_LOCAL BOOL tpkt_verify_header(wStream* s);
FREERDP_LOCAL BOOL tpkt_read_header(wStream* s, UINT16* length);
FREERDP_LOCAL BOOL tpkt_write_header(wStream* s, UINT16 length);
#define tpkt_ensure_stream_consumed(s, length) \
	tpkt_ensure_stream_consumed_((s), (length), __FUNCTION__)
FREERDP_LOCAL BOOL tpkt_ensure_stream_consumed_(wStream* s, UINT16 length, const char* fkt);

#endif /* FREERDP_LIB_CORE_TPKT_H */
