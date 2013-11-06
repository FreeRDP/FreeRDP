/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RAIL common functions
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2011 Roman Barabanov <romanbarabanov@gmail.com>
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
#include "rail_common.h"

#include <winpr/crt.h>

const char* const RAIL_ORDER_TYPE_STRINGS[] =
{
	"",
	"Execute",
	"Activate",
	"System Parameters Update",
	"System Command",
	"Handshake",
	"Notify Event",
	"",
	"Window Move",
	"Local Move/Size",
	"Min Max Info",
	"Client Status",
	"System Menu",
	"Language Bar Info",
	"Get Application ID Request",
	"Get Application ID Response",
	"Execute Result",
	"",
	"",
	"",
	"",
	"",
	""
};

void rail_string_to_unicode_string(char* string, RAIL_UNICODE_STRING* unicode_string)
{
	WCHAR* buffer = NULL;
	int length = 0;

	if (unicode_string->string != NULL)
		free(unicode_string->string);

	unicode_string->string = NULL;
	unicode_string->length = 0;

	if (!string || strlen(string) < 1)
		return;

	length = ConvertToUnicode(CP_UTF8, 0, string, -1, &buffer, 0) * 2;

	unicode_string->string = (BYTE*) buffer;
	unicode_string->length = (UINT16) length;
}

BOOL rail_read_pdu_header(wStream* s, UINT16* orderType, UINT16* orderLength)
{
	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT16(s, *orderType); /* orderType (2 bytes) */
	Stream_Read_UINT16(s, *orderLength); /* orderLength (2 bytes) */

	return TRUE;
}

void rail_write_pdu_header(wStream* s, UINT16 orderType, UINT16 orderLength)
{
	Stream_Write_UINT16(s, orderType); /* orderType (2 bytes) */
	Stream_Write_UINT16(s, orderLength); /* orderLength (2 bytes) */
}

wStream* rail_pdu_init(int length)
{
	wStream* s;
	s = Stream_New(NULL, length + RAIL_PDU_HEADER_LENGTH);
	Stream_Seek(s, RAIL_PDU_HEADER_LENGTH);
	return s;
}

BOOL rail_read_handshake_order(wStream* s, RAIL_HANDSHAKE_ORDER* handshake)
{
	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT32(s, handshake->buildNumber); /* buildNumber (4 bytes) */

	return TRUE;
}

void rail_write_handshake_order(wStream* s, RAIL_HANDSHAKE_ORDER* handshake)
{
	Stream_Write_UINT32(s, handshake->buildNumber); /* buildNumber (4 bytes) */
}

BOOL rail_read_handshake_ex_order(wStream* s, RAIL_HANDSHAKE_EX_ORDER* handshakeEx)
{
	if (Stream_GetRemainingLength(s) < 8)
		return FALSE;

	Stream_Read_UINT32(s, handshakeEx->buildNumber); /* buildNumber (4 bytes) */
	Stream_Read_UINT32(s, handshakeEx->railHandshakeFlags); /* railHandshakeFlags (4 bytes) */

	return TRUE;
}

void rail_write_handshake_ex_order(wStream* s, RAIL_HANDSHAKE_EX_ORDER* handshakeEx)
{
	Stream_Write_UINT32(s, handshakeEx->buildNumber); /* buildNumber (4 bytes) */
	Stream_Write_UINT32(s, handshakeEx->railHandshakeFlags); /* railHandshakeFlags (4 bytes) */
}
