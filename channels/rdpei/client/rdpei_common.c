/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Input Virtual Channel Extension
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

#include "rdpei_common.h"

BOOL rdpei_read_2byte_unsigned(wStream* s, UINT32* value)
{
	BYTE byte;

	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, byte);

	if (byte & 0x80)
	{
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		*value = (byte & 0x7F) << 8;
		Stream_Read_UINT8(s, byte);
		*value |= byte;
	}
	else
	{
		*value = (byte & 0x7F);
	}

	return TRUE;
}

BOOL rdpei_write_2byte_unsigned(wStream* s, UINT32 value)
{
	BYTE byte;

	if (value > 0x7FFF)
		return FALSE;

	if (value >= 0x7F)
	{
		byte = ((value & 0x7F00) >> 8);
		Stream_Write_UINT8(s, byte | 0x80);
		byte = (value & 0xFF);
		Stream_Write_UINT8(s, byte);
	}
	else
	{
		byte = (value & 0x7F);
		Stream_Write_UINT8(s, byte);
	}

	return TRUE;
}

BOOL rdpei_read_2byte_signed(wStream* s, INT32* value)
{
	BYTE byte;
	BOOL negative;

	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, byte);

	negative = (byte & 0x40) ? TRUE : FALSE;

	*value = (byte & 0x3F);

	if (byte & 0x80)
	{
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, byte);
		*value = (*value << 8) | byte;
	}

	if (negative)
		*value *= -1;

	return TRUE;
}

BOOL rdpei_write_2byte_signed(wStream* s, INT32 value)
{
	BYTE byte;
	BOOL negative = FALSE;

	if (value < 0)
	{
		negative = TRUE;
		value *= -1;
	}

	if (value > 0x3FFF)
		return FALSE;

	if (value >= 0x3F)
	{
		byte = ((value & 0x3F00) >> 8);

		if (negative)
			byte |= 0x40;

		Stream_Write_UINT8(s, byte | 0x80);
		byte = (value & 0xFF);
		Stream_Write_UINT8(s, byte);
	}
	else
	{
		byte = (value & 0x3F);

		if (negative)
			byte |= 0x40;

		Stream_Write_UINT8(s, byte);
	}

	return TRUE;
}

BOOL rdpei_read_4byte_unsigned(wStream* s, UINT32* value)
{
	BYTE byte;
	BYTE count;

	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, byte);

	count = (byte & 0xC0) >> 6;

	if (Stream_GetRemainingLength(s) < count)
		return FALSE;

	switch (count)
	{
		case 0:
			*value = (byte & 0x3F);
			break;

		case 1:
			*value = (byte & 0x3F) << 8;
			Stream_Read_UINT8(s, byte);
			*value |= byte;
			break;

		case 2:
			*value = (byte & 0x3F) << 16;
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 8);
			Stream_Read_UINT8(s, byte);
			*value |= byte;
			break;

		case 3:
			*value = (byte & 0x3F) << 24;
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 16);
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 8);
			Stream_Read_UINT8(s, byte);
			*value |= byte;
			break;

		default:
			break;
	}

	return TRUE;
}

BOOL rdpei_write_4byte_unsigned(wStream* s, UINT32 value)
{
	BYTE byte;

	if (value <= 0x3F)
	{
		Stream_Write_UINT8(s, value);
	}
	else if (value <= 0x3FFF)
	{
		byte = (value >> 8) & 0x3F;
		Stream_Write_UINT8(s, byte | 0x40);
		byte = (value & 0xFF);
		Stream_Write_UINT8(s, byte);
	}
	else if (value <= 0x3FFFFF)
	{
		byte = (value >> 16) & 0x3F;
		Stream_Write_UINT8(s, byte | 0x80);
		byte = (value >> 8) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (value & 0xFF);
		Stream_Write_UINT8(s, byte);
	}
	else if (value <= 0x3FFFFF)
	{
		byte = (value >> 24) & 0x3F;
		Stream_Write_UINT8(s, byte | 0xC0);
		byte = (value >> 16) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (value >> 8) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (value & 0xFF);
		Stream_Write_UINT8(s, byte);
	}
	else
	{
		return FALSE;
	}

	return TRUE;
}

BOOL rdpei_read_4byte_signed(wStream* s, INT32* value)
{
	BYTE byte;
	BYTE count;
	BOOL negative;

	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, byte);

	count = (byte & 0xC0) >> 6;
	negative = (byte & 0x20);

	if (Stream_GetRemainingLength(s) < count)
		return FALSE;

	switch (count)
	{
		case 0:
			*value = (byte & 0x1F);
			break;

		case 1:
			*value = (byte & 0x1F) << 8;
			Stream_Read_UINT8(s, byte);
			*value |= byte;
			break;

		case 2:
			*value = (byte & 0x1F) << 16;
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 8);
			Stream_Read_UINT8(s, byte);
			*value |= byte;
			break;

		case 3:
			*value = (byte & 0x1F) << 24;
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 16);
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 8);
			Stream_Read_UINT8(s, byte);
			*value |= byte;
			break;

		default:
			break;
	}

	if (negative)
		*value *= -1;

	return TRUE;
}

BOOL rdpei_write_4byte_signed(wStream* s, INT32 value)
{
	BYTE byte;
	BOOL negative = FALSE;

	if (value < 0)
	{
		negative = TRUE;
		value *= -1;
	}

	if (value <= 0x1F)
	{
		byte = value & 0x1F;

		if (negative)
			byte |= 0x20;

		Stream_Write_UINT8(s, value);
	}
	else if (value <= 0x1FFF)
	{
		byte = (value >> 8) & 0x1F;

		if (negative)
			byte |= 0x20;

		Stream_Write_UINT8(s, byte | 0x40);
		byte = (value & 0xFF);
		Stream_Write_UINT8(s, byte);
	}
	else if (value <= 0x1FFFFF)
	{
		byte = (value >> 16) & 0x1F;

		if (negative)
			byte |= 0x20;

		Stream_Write_UINT8(s, byte | 0x80);
		byte = (value >> 8) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (value & 0xFF);
		Stream_Write_UINT8(s, byte);
	}
	else if (value <= 0x1FFFFF)
	{
		byte = (value >> 24) & 0x1F;

		if (negative)
			byte |= 0x20;

		Stream_Write_UINT8(s, byte | 0xC0);
		byte = (value >> 16) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (value >> 8) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (value & 0xFF);
		Stream_Write_UINT8(s, byte);
	}
	else
	{
		return FALSE;
	}

	return TRUE;
}

BOOL rdpei_read_8byte_unsigned(wStream* s, UINT64* value)
{
	BYTE byte;
	BYTE count;

	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, byte);

	count = (byte & 0xE0) >> 5;

	if (Stream_GetRemainingLength(s) < count)
		return FALSE;

	switch (count)
	{
		case 0:
			*value = (byte & 0x1F);
			break;

		case 1:
			*value = (byte & 0x1F) << 8;
			Stream_Read_UINT8(s, byte);
			*value |= byte;
			break;

		case 2:
			*value = (byte & 0x1F) << 16;
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 8);
			Stream_Read_UINT8(s, byte);
			*value |= byte;
			break;

		case 3:
			*value = (byte & 0x1F) << 24;
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 16);
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 8);
			Stream_Read_UINT8(s, byte);
			*value |= byte;
			break;

		case 4:
			*value = ((UINT64) (byte & 0x1F)) << 32;
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 24);
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 16);
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 8);
			Stream_Read_UINT8(s, byte);
			*value |= byte;
			break;

		case 5:
			*value = ((UINT64) (byte & 0x1F)) << 40;
			Stream_Read_UINT8(s, byte);
			*value |= (((UINT64) byte) << 32);
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 24);
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 16);
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 8);
			Stream_Read_UINT8(s, byte);
			*value |= byte;
			break;

		case 6:
			*value = ((UINT64) (byte & 0x1F)) << 48;
			Stream_Read_UINT8(s, byte);
			*value |= (((UINT64) byte) << 40);
			Stream_Read_UINT8(s, byte);
			*value |= (((UINT64) byte) << 32);
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 24);
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 16);
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 8);
			Stream_Read_UINT8(s, byte);
			*value |= byte;
			break;

		case 7:
			*value = ((UINT64) (byte & 0x1F)) << 56;
			Stream_Read_UINT8(s, byte);
			*value |= (((UINT64) byte) << 48);
			Stream_Read_UINT8(s, byte);
			*value |= (((UINT64) byte) << 40);
			Stream_Read_UINT8(s, byte);
			*value |= (((UINT64) byte) << 32);
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 24);
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 16);
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 8);
			Stream_Read_UINT8(s, byte);
			*value |= byte;
			break;

		default:
			break;
	}

	return TRUE;
}

BOOL rdpei_write_8byte_unsigned(wStream* s, UINT64 value)
{
	BYTE byte;

	if (value <= 0x1F)
	{
		byte = value & 0x1F;
		Stream_Write_UINT8(s, byte);
	}
	else if (value <= 0x1FFF)
	{
		byte = (value >> 8) & 0x1F;
		byte |= (1 << 5);
		Stream_Write_UINT8(s, byte);
		byte = (value & 0xFF);
		Stream_Write_UINT8(s, byte);
	}
	else if (value <= 0x1FFFFF)
	{
		byte = (value >> 16) & 0x1F;
		byte |= (2 << 5);
		Stream_Write_UINT8(s, byte);
		byte = (value >> 8) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (value & 0xFF);
		Stream_Write_UINT8(s, byte);
	}
	else if (value <= 0x1FFFFF)
	{
		byte = (value >> 24) & 0x1F;
		byte |= (3 << 5);
		Stream_Write_UINT8(s, byte);
		byte = (value >> 16) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (value >> 8) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (value & 0xFF);
		Stream_Write_UINT8(s, byte);
	}
	else if (value <= 0x1FFFFFFF)
	{
		byte = (value >> 32) & 0x1F;
		byte |= (4 << 5);
		Stream_Write_UINT8(s, byte);
		byte = (value >> 24) & 0x1F;
		Stream_Write_UINT8(s, byte);
		byte = (value >> 16) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (value >> 8) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (value & 0xFF);
		Stream_Write_UINT8(s, byte);
	}
	else if (value <= 0x1FFFFFFFFF)
	{
		byte = (value >> 40) & 0x1F;
		byte |= (5 << 5);
		Stream_Write_UINT8(s, byte);
		byte = (value >> 32) & 0x1F;
		Stream_Write_UINT8(s, byte);
		byte = (value >> 24) & 0x1F;
		Stream_Write_UINT8(s, byte);
		byte = (value >> 16) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (value >> 8) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (value & 0xFF);
		Stream_Write_UINT8(s, byte);
	}
	else if (value <= 0x1FFFFFFFFFFF)
	{
		byte = (value >> 48) & 0x1F;
		byte |= (6 << 5);
		Stream_Write_UINT8(s, byte);
		byte = (value >> 40) & 0x1F;
		Stream_Write_UINT8(s, byte);
		byte = (value >> 32) & 0x1F;
		Stream_Write_UINT8(s, byte);
		byte = (value >> 24) & 0x1F;
		Stream_Write_UINT8(s, byte);
		byte = (value >> 16) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (value >> 8) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (value & 0xFF);
		Stream_Write_UINT8(s, byte);
	}
	else if (value <= 0x1FFFFFFFFFFFFF)
	{
		byte = (value >> 56) & 0x1F;
		byte |= (7 << 5);
		Stream_Write_UINT8(s, byte);
		byte = (value >> 48) & 0x1F;
		Stream_Write_UINT8(s, byte);
		byte = (value >> 40) & 0x1F;
		Stream_Write_UINT8(s, byte);
		byte = (value >> 32) & 0x1F;
		Stream_Write_UINT8(s, byte);
		byte = (value >> 24) & 0x1F;
		Stream_Write_UINT8(s, byte);
		byte = (value >> 16) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (value >> 8) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (value & 0xFF);
		Stream_Write_UINT8(s, byte);
	}
	else
	{
		return FALSE;
	}

	return TRUE;
}
