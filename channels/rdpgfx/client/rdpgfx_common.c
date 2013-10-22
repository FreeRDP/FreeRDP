/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Graphics Pipeline Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <winpr/stream.h>

#include "rdpgfx_common.h"

int rdpgfx_read_point16(wStream* s, RDPGFX_POINT16* point16)
{
	Stream_Read_UINT16(s, point16->x); /* x (2 bytes) */
	Stream_Read_UINT16(s, point16->y); /* y (2 bytes) */

	return 0;
}

int rdpgfx_write_point16(wStream* s, RDPGFX_POINT16* point16)
{
	Stream_Write_UINT16(s, point16->x); /* x (2 bytes) */
	Stream_Write_UINT16(s, point16->y); /* y (2 bytes) */

	return 0;
}

int rdpgfx_read_rect16(wStream* s, RDPGFX_RECT16* rect16)
{
	Stream_Read_UINT16(s, rect16->left); /* left (2 bytes) */
	Stream_Read_UINT16(s, rect16->top); /* top (2 bytes) */
	Stream_Read_UINT16(s, rect16->right); /* right (2 bytes) */
	Stream_Read_UINT16(s, rect16->bottom); /* bottom (2 bytes) */

	return 0;
}

int rdpgfx_write_rect16(wStream* s, RDPGFX_RECT16* rect16)
{
	Stream_Write_UINT16(s, rect16->left); /* left (2 bytes) */
	Stream_Write_UINT16(s, rect16->top); /* top (2 bytes) */
	Stream_Write_UINT16(s, rect16->right); /* right (2 bytes) */
	Stream_Write_UINT16(s, rect16->bottom); /* bottom (2 bytes) */

	return 0;
}

int rdpgfx_read_color32(wStream* s, RDPGFX_COLOR32* color32)
{
	Stream_Read_UINT8(s, color32->B); /* B (1 byte) */
	Stream_Read_UINT8(s, color32->G); /* G (1 byte) */
	Stream_Read_UINT8(s, color32->R); /* R (1 byte) */
	Stream_Read_UINT8(s, color32->XA); /* XA (1 byte) */

	return 0;
}

int rdpgfx_write_color32(wStream* s, RDPGFX_COLOR32* color32)
{
	Stream_Write_UINT8(s, color32->B); /* B (1 byte) */
	Stream_Write_UINT8(s, color32->G); /* G (1 byte) */
	Stream_Write_UINT8(s, color32->R); /* R (1 byte) */
	Stream_Write_UINT8(s, color32->XA); /* XA (1 byte) */

	return 0;
}

int rdpgfx_read_header(wStream* s, RDPGFX_HEADER* header)
{
	Stream_Read_UINT16(s, header->cmdId); /* cmdId (2 bytes) */
	Stream_Read_UINT16(s, header->flags); /* flags (2 bytes) */
	Stream_Read_UINT16(s, header->pduLength); /* pduLength (4 bytes) */

	return 0;
}

int rdpgfx_write_header(wStream* s, RDPGFX_HEADER* header)
{
	Stream_Write_UINT16(s, header->cmdId); /* cmdId (2 bytes) */
	Stream_Write_UINT16(s, header->flags); /* flags (2 bytes) */
	Stream_Write_UINT16(s, header->pduLength); /* pduLength (4 bytes) */

	return 0;
}
