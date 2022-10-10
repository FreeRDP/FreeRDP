/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X.224 Transport Protocol Data Units (TPDUs)
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

#ifndef FREERDP_LIB_CORE_TPDU_H
#define FREERDP_LIB_CORE_TPDU_H

#include <winpr/stream.h>
#include <freerdp/api.h>

#include "tpkt.h"

enum X224_TPDU_TYPE
{
	X224_TPDU_CONNECTION_REQUEST = 0xE0,
	X224_TPDU_CONNECTION_CONFIRM = 0xD0,
	X224_TPDU_DISCONNECT_REQUEST = 0x80,
	X224_TPDU_DATA = 0xF0,
	X224_TPDU_ERROR = 0x70
};

#define TPDU_DATA_HEADER_LENGTH 3
#define TPDU_CONNECTION_REQUEST_HEADER_LENGTH 7
#define TPDU_CONNECTION_CONFIRM_HEADER_LENGTH 7
#define TPDU_DISCONNECT_REQUEST_HEADER_LENGTH 7

#define TPDU_DATA_LENGTH (TPKT_HEADER_LENGTH + TPDU_DATA_HEADER_LENGTH)
#define TPDU_CONNECTION_REQUEST_LENGTH (TPKT_HEADER_LENGTH + TPDU_CONNECTION_REQUEST_HEADER_LENGTH)
#define TPDU_CONNECTION_CONFIRM_LENGTH (TPKT_HEADER_LENGTH + TPDU_CONNECTION_CONFIRM_HEADER_LENGTH)
#define TPDU_DISCONNECT_REQUEST_LENGTH (TPKT_HEADER_LENGTH + TPDU_DISCONNECT_REQUEST_HEADER_LENGTH)

const char* tpdu_type_to_string(int type);
FREERDP_LOCAL BOOL tpdu_read_header(wStream* s, BYTE* code, BYTE* li, UINT16 tpktlength);
FREERDP_LOCAL BOOL tpdu_read_connection_request(wStream* s, BYTE* li, UINT16 tpktlength);
FREERDP_LOCAL BOOL tpdu_write_connection_request(wStream* s, UINT16 length);
FREERDP_LOCAL BOOL tpdu_read_connection_confirm(wStream* s, BYTE* li, UINT16 tpktlength);
FREERDP_LOCAL BOOL tpdu_write_connection_confirm(wStream* s, UINT16 length);
FREERDP_LOCAL BOOL tpdu_write_disconnect_request(wStream* s, UINT16 length);
FREERDP_LOCAL BOOL tpdu_read_data(wStream* s, UINT16* li, UINT16 tpktlength);
FREERDP_LOCAL BOOL tpdu_write_data(wStream* s);

#endif /* FREERDP_LIB_CORE_TPDU_H */
