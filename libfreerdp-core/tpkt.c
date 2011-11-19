/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include "tpdu.h"

#include "tpkt.h"

/**
 * TPKTs are defined in:
 *
 * http://tools.ietf.org/html/rfc1006/
 * RFC 1006 - ISO Transport Service on top of the TCP
 *
 * http://www.itu.int/rec/T-REC-T.123/
 * ITU-T T.123 (01/2007) - Network-specific data protocol stacks for multimedia conferencing
 *
 *       TPKT Header
 *  ____________________   byte
 * |                    |
 * |     3 (version)    |   1
 * |____________________|
 * |                    |
 * |      Reserved      |   2
 * |____________________|
 * |                    |
 * |    Length (MSB)    |   3
 * |____________________|
 * |                    |
 * |    Length (LSB)    |   4
 * |____________________|
 * |                    |
 * |     X.224 TPDU     |   5 - ?
 *          ....
 *
 * A TPKT header is of fixed length 4, and the following X.224 TPDU is at least three bytes long.
 * Therefore, the minimum TPKT length is 7, and the maximum TPKT length is 65535. Because the TPKT
 * length includes the TPKT header (4 bytes), the maximum X.224 TPDU length is 65531.
 */

/**
 * Verify if a packet has valid TPKT header.\n
 * @param s
 * @return boolean
 */

boolean tpkt_verify_header(STREAM* s)
{
	uint8 version;

	stream_peek_uint8(s, version);
	if (version == 3)
		return true;
	else
		return false;
}

/**
 * Read a TPKT header.\n
 * @param s
 * @return length
 */

uint16 tpkt_read_header(STREAM* s)
{
	uint8 version;
	uint16 length;

	stream_peek_uint8(s, version);

	if (version == 3)
	{
		stream_seek(s, 2);
		stream_read_uint16_be(s, length);
	}
	else
	{
		/* not a TPKT header */
		length = 0;
	}

	return length;
}

/**
 * Write a TPKT header.\n
 * @param s
 * @param length
 */

void tpkt_write_header(STREAM* s, uint16 length)
{
	stream_write_uint8(s, 3); /* version */
	stream_write_uint8(s, 0); /* reserved */
	stream_write_uint16_be(s, length); /* length */
}
