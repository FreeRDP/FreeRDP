/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include "tpdu.h"

/**
 * TPDUs are defined in:
 *
 * http://www.itu.int/rec/T-REC-X.224-199511-I/
 * X.224: Information technology - Open Systems Interconnection - Protocol for providing the connection-mode transport service
 *
 * RDP uses only TPDUs of class 0, the "simple class" defined in section 8 of X.224
 *
 *       TPDU Header
 *  ____________________   byte
 * |                    |
 * |         LI         |   1
 * |____________________|
 * |                    |
 * |        Code        |   2
 * |____________________|
 * |                    |
 * |                    |   3
 * |_______DST-REF______|
 * |                    |
 * |                    |   4
 * |____________________|
 * |                    |
 * |                    |   5
 * |_______SRC-REF______|
 * |                    |
 * |                    |   6
 * |____________________|
 * |                    |
 * |        Class       |   7
 * |____________________|
 * |         ...        |
 */

uint8
tpdu_read_header(STREAM* s, uint16 length)
{
	uint8 li;
	uint8 code;

	stream_read_uint8(s, li); /* LI */
	stream_read_uint8(s, code); /* Code */

	if (code == X224_TPDU_DATA)
	{
		/* EOT (1 byte) */
		stream_seek(s, 1);
	}
	else
	{
		/* DST-REF (2 bytes) */
		/* SRC-REF (2 bytes) */
		/* Class 0 (1 byte) */
		stream_seek(s, 5);
	}

	return code;
}

void
tpdu_write_header(STREAM* s, uint16 length, uint8 code)
{
	stream_write_uint8(s, length); /* LI */
	stream_write_uint8(s, code); /* code */

	if (code == X224_TPDU_DATA)
	{
		stream_write_uint8(s, 0x80); /* EOT */
	}
	else
	{
		stream_write_uint16(s, 0); /* DST-REF */
		stream_write_uint16(s, 0); /* SRC-REF */
		stream_write_uint8(s, 0); /* Class 0 */
	}
}

void
tpdu_write_connection_request(STREAM* s, uint16 length)
{
	tpdu_write_header(s, length, X224_TPDU_CONNECTION_REQUEST);
}

void
tpdu_write_disconnect_request(STREAM* s, uint16 length)
{
	tpdu_write_header(s, length, X224_TPDU_DISCONNECT_REQUEST);
}

void
tpdu_write_data(STREAM* s, uint16 length)
{
	tpdu_write_header(s, length, X224_TPDU_DATA);
}
