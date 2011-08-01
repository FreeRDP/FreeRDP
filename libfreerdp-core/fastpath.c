/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Fast Path
 *
 * Copyright 2011 Vic Lee
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/utils/stream.h>

#include "fastpath.h"

/**
 * Fast-Path packet format is defined in [MS-RDPBCGR] 2.2.9.1.2, which revises
 * server output packets from the first byte with the goal of improving
 * bandwidth.
 * 
 * Slow-Path packet always starts with TPKT header, which has the first
 * byte 0x03, while Fast-Path packet starts with 2 zero bits in the first
 * two less significant bits of the first byte.
 */

/**
 * Read a Fast-Path packet header.\n
 * @param s stream
 * @param encryptionFlags
 * @return length
 */

uint16 fastpath_read_header(STREAM* s, uint8* encryptionFlags)
{
	uint8 header;
	uint16 length;
	uint8 t;

	stream_read_uint8(s, header);
	if (encryptionFlags != NULL)
		*encryptionFlags = (header & 0xC0) >> 6;

	stream_read_uint8(s, length); /* length1 */
	/* If most significant bit is not set, length2 is not presented. */
	if ((length & 0x80))
	{
		length &= 0x7F;
		length <<= 8;
		stream_read_uint8(s, t);
		length += t;
	}

	return length;
}
