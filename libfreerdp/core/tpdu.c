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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <winpr/print.h>

#include <freerdp/log.h>

#include "tpdu.h"

#define TAG FREERDP_TAG("core")

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

/**
 * Read TPDU header.
 * @param s stream
 * @param code variable pointer to receive TPDU code
 * @return TPDU length indicator (LI)
 */

BOOL tpdu_read_header(wStream* s, BYTE* code, BYTE* li)
{
	if (Stream_GetRemainingLength(s) < 3)
		return FALSE;

	Stream_Read_UINT8(s, *li); /* LI */
	Stream_Read_UINT8(s, *code); /* Code */

	if (*code == X224_TPDU_DATA)
	{
		/* EOT (1 byte) */
		Stream_Seek(s, 1);
	}
	else
	{
		/* DST-REF (2 bytes) */
		/* SRC-REF (2 bytes) */
		/* Class 0 (1 byte) */
		return Stream_SafeSeek(s, 5);
	}

	return TRUE;
}

/**
 * Write TDPU header.
 * @param s stream
 * @param length length
 * @param code TPDU code
 */

void tpdu_write_header(wStream* s, UINT16 length, BYTE code)
{
	Stream_Write_UINT8(s, length); /* LI */
	Stream_Write_UINT8(s, code); /* code */

	if (code == X224_TPDU_DATA)
	{
		Stream_Write_UINT8(s, 0x80); /* EOT */
	}
	else
	{
		Stream_Write_UINT16(s, 0); /* DST-REF */
		Stream_Write_UINT16(s, 0); /* SRC-REF */
		Stream_Write_UINT8(s, 0); /* Class 0 */
	}
}

/**
 * Read Connection Request TPDU
 * @param s stream
 * @return length indicator (LI)
 */

BOOL tpdu_read_connection_request(wStream* s, BYTE* li)
{
	BYTE code;

	if (!tpdu_read_header(s, &code, li))
		return FALSE;

	if (code != X224_TPDU_CONNECTION_REQUEST)
	{
		WLog_ERR(TAG,  "Error: expected X224_TPDU_CONNECTION_REQUEST");
		return FALSE;
	}

	return TRUE;
}

/**
 * Write Connection Request TPDU.
 * @param s stream
 * @param length TPDU length
 */

void tpdu_write_connection_request(wStream* s, UINT16 length)
{
	tpdu_write_header(s, length, X224_TPDU_CONNECTION_REQUEST);
}

/**
 * Read Connection Confirm TPDU.
 * @param s stream
 * @return length indicator (LI)
 */

BOOL tpdu_read_connection_confirm(wStream* s, BYTE* li)
{
	BYTE code;
	size_t position;
	size_t bytes_read = 0;

	/* save the position to determine the number of bytes read */
	position = Stream_GetPosition(s);

	if (!tpdu_read_header(s, &code, li))
		return FALSE;

	if (code != X224_TPDU_CONNECTION_CONFIRM)
	{
		WLog_ERR(TAG,  "Error: expected X224_TPDU_CONNECTION_CONFIRM");
		return FALSE;
	}
	/*
	 * To ensure that there are enough bytes remaining for processing
	 * check against the length indicator (li). Already read bytes need
	 * to be taken into account.
	 * The -1 is because li was read but isn't included in the TPDU size.
	 * For reference see ITU-T Rec. X.224 - 13.2.1
	 */
	bytes_read = (Stream_GetPosition(s) - position) - 1;

	return (Stream_GetRemainingLength(s) >= (size_t) (*li - bytes_read));
}

/**
 * Write Connection Confirm TPDU.
 * @param s stream
 * @param length TPDU length
 */

void tpdu_write_connection_confirm(wStream* s, UINT16 length)
{
	tpdu_write_header(s, length, X224_TPDU_CONNECTION_CONFIRM);
}

/**
 * Write Disconnect Request TPDU.
 * @param s stream
 * @param length TPDU length
 */

void tpdu_write_disconnect_request(wStream* s, UINT16 length)
{
	tpdu_write_header(s, length, X224_TPDU_DISCONNECT_REQUEST);
}

/**
 * Write Data TPDU.
 * @param s stream
 */

void tpdu_write_data(wStream* s)
{
	tpdu_write_header(s, 2, X224_TPDU_DATA);
}

/**
 * Read Data TPDU.
 * @param s stream
 */

BOOL tpdu_read_data(wStream* s, UINT16* LI)
{
	BYTE code;
	BYTE li;

	if (!tpdu_read_header(s, &code, &li))
		return FALSE;

	if (code != X224_TPDU_DATA)
		return FALSE;

	*LI = li;

	return TRUE;
}
