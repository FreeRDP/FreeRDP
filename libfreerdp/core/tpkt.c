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

#include <freerdp/config.h>

#include "tpdu.h"

#include "tpkt.h"

#include <winpr/wlog.h>

#define TAG FREERDP_TAG("core.tpkt")

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
 * @return BOOL
 */

BOOL tpkt_verify_header(wStream* s)
{
	BYTE version;

	WINPR_ASSERT(s);

	Stream_Peek_UINT8(s, version);

	if (version == 3)
		return TRUE;
	else
		return FALSE;
}

/**
 * Read a TPKT header.\n
 * @param s
 * @param length
 * @return success
 */

BOOL tpkt_read_header(wStream* s, UINT16* length)
{
	BYTE version;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	Stream_Peek_UINT8(s, version);

	if (version == 3)
	{
		UINT16 len;
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
			return FALSE;

		Stream_Seek(s, 2);
		Stream_Read_UINT16_BE(s, len);

		/* ITU-T Rec. T.123 8 Packet header to delimit data units in an octet stream */
		if (len < 7)
		{
			WLog_ERR(TAG, "TPKT header too short, require minimum of 7 bytes, got %" PRId16, len);
			return FALSE;
		}

		if (!Stream_CheckAndLogRequiredLength(TAG, s, len - 4))
		{
			WLog_ERR(TAG, "TPKT header length %" PRIu16 ", but received less", len);
			return FALSE;
		}
		*length = len;
	}
	else
	{
		/* not a TPKT header */
		*length = 0;
	}

	return TRUE;
}

BOOL tpkt_ensure_stream_consumed_(wStream* s, UINT16 length, const char* fkt)
{
	size_t rem = Stream_GetRemainingLength(s);
	if (rem > 0)
	{
		WLog_ERR(TAG,
		         "[%s] Received invalid TPKT header length %" PRIu16 ", %" PRIdz " bytes too long!",
		         fkt, length, rem);
		return FALSE;
	}
	return TRUE;
}

/**
 * Write a TPKT header.\n
 * @param s
 * @param length
 */

BOOL tpkt_write_header(wStream* s, UINT16 length)
{
	if (Stream_GetRemainingCapacity(s) < 4)
		return FALSE;
	Stream_Write_UINT8(s, 3);          /* version */
	Stream_Write_UINT8(s, 0);          /* reserved */
	Stream_Write_UINT16_BE(s, length); /* length */
	return TRUE;
}
