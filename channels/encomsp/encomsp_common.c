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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/print.h>

#include "encomsp_common.h"

int encomsp_read_header(wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	if (Stream_GetRemainingLength(s) < ENCOMSP_ORDER_HEADER_SIZE)
		return -1;

	Stream_Read_UINT16(s, header->Type); /* Type (2 bytes) */
	Stream_Read_UINT16(s, header->Length); /* Length (2 bytes) */

	return 1;
}

int encomsp_write_header(wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	Stream_Write_UINT16(s, header->Type); /* Type (2 bytes) */
	Stream_Write_UINT16(s, header->Length); /* Length (2 bytes) */

	return 1;
}

int encomsp_read_unicode_string(wStream* s, ENCOMSP_UNICODE_STRING* str)
{
	ZeroMemory(str, sizeof(ENCOMSP_UNICODE_STRING));

	if (Stream_GetRemainingLength(s) < 2)
		return -1;

	Stream_Read_UINT16(s, str->cchString); /* cchString (2 bytes) */

	if (str->cchString > 1024)
		return -1;

	if (Stream_GetRemainingLength(s) < (str->cchString * 2))
		return -1;

	Stream_Read(s, &(str->wString), (str->cchString * 2)); /* String (variable) */

	return 1;
}
