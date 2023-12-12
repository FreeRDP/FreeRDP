/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Helper functions to parse encoded types into regular ones
 *
 * Copyright 2023 Pascal Nowack <Pascal.Nowack@gmx.de>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <freerdp/config.h>

#include <freerdp/channels/log.h>
#include <freerdp/utils/encoded_types.h>
#include <math.h>

#define TAG CHANNELS_TAG("encoded_types")

typedef enum
{
	ONE_BYTE_VAL,
	TWO_BYTE_VAL,
	THREE_BYTE_VAL,
	FOUR_BYTE_VAL,
	FIVE_BYTE_VAL,
	SIX_BYTE_VAL,
	SEVEN_BYTE_VAL,
	EIGHT_BYTE_VAL,
} EncodedTypeByteCount;

typedef enum
{
	POSITIVE_VAL,
	NEGATIVE_VAL,
} EncodedTypeSign;

typedef struct
{
	EncodedTypeByteCount c;
	EncodedTypeSign s;
	BYTE val1;
	BYTE val2;
	BYTE val3;
	BYTE val4;
} FOUR_BYTE_SIGNED_INTEGER;

typedef struct
{
	EncodedTypeByteCount c;
	EncodedTypeSign s;
	BYTE e;
	BYTE val1;
	BYTE val2;
	BYTE val3;
	BYTE val4;
} FOUR_BYTE_FLOAT;

BOOL freerdp_read_four_byte_signed_integer(wStream* s, INT32* value)
{
	FOUR_BYTE_SIGNED_INTEGER si = { 0 };
	BYTE byte;

	WINPR_ASSERT(s);
	WINPR_ASSERT(value);

	*value = 0;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	Stream_Read_UINT8(s, byte);

	si.c = (byte & 0xC0) >> 6;
	si.s = (byte & 0x20) >> 5;
	si.val1 = (byte & 0x1F);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, si.c))
		return FALSE;

	switch (si.c)
	{
		case ONE_BYTE_VAL:
			*value = si.val1;
			break;
		case TWO_BYTE_VAL:
			Stream_Read_UINT8(s, si.val2);
			*value = si.val1 << 8 | si.val2;
			break;
		case THREE_BYTE_VAL:
			Stream_Read_UINT8(s, si.val2);
			Stream_Read_UINT8(s, si.val3);
			*value = si.val1 << 16 | si.val2 << 8 | si.val3;
			break;
		case FOUR_BYTE_VAL:
			Stream_Read_UINT8(s, si.val2);
			Stream_Read_UINT8(s, si.val3);
			Stream_Read_UINT8(s, si.val4);
			*value = si.val1 << 24 | si.val2 << 16 | si.val3 << 8 | si.val4;
			break;
		case FIVE_BYTE_VAL:
		case SIX_BYTE_VAL:
		case SEVEN_BYTE_VAL:
		case EIGHT_BYTE_VAL:
		default:
			WLog_ERR(TAG, "Invalid byte count value in si.c: %u", si.c);
			return FALSE;
	}

	if (si.s == NEGATIVE_VAL)
		*value *= -1;

	return TRUE;
}

BOOL freerdp_read_four_byte_float(wStream* s, double* value)
{
	FOUR_BYTE_FLOAT f = { 0 };
	UINT32 base;
	BYTE byte;

	WINPR_ASSERT(s);
	WINPR_ASSERT(value);

	*value = 0.0;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	Stream_Read_UINT8(s, byte);

	f.c = (byte & 0xC0) >> 6;
	f.s = (byte & 0x20) >> 5;
	f.e = (byte & 0x1C) >> 2;
	f.val1 = (byte & 0x03);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, f.c))
		return FALSE;

	switch (f.c)
	{
		case ONE_BYTE_VAL:
			base = f.val1;
			break;
		case TWO_BYTE_VAL:
			Stream_Read_UINT8(s, f.val2);
			base = f.val1 << 8 | f.val2;
			break;
		case THREE_BYTE_VAL:
			Stream_Read_UINT8(s, f.val2);
			Stream_Read_UINT8(s, f.val3);
			base = f.val1 << 16 | f.val2 << 8 | f.val3;
			break;
		case FOUR_BYTE_VAL:
			Stream_Read_UINT8(s, f.val2);
			Stream_Read_UINT8(s, f.val3);
			Stream_Read_UINT8(s, f.val4);
			base = f.val1 << 24 | f.val2 << 16 | f.val3 << 8 | f.val4;
			break;
		case FIVE_BYTE_VAL:
		case SIX_BYTE_VAL:
		case SEVEN_BYTE_VAL:
		case EIGHT_BYTE_VAL:
		default:
			WLog_ERR(TAG, "Invalid byte count value in f.c: %u", f.c);
			return FALSE;
	}

	*value = base;
	*value /= pow(10, f.e);

	if (f.s == NEGATIVE_VAL)
		*value *= -1.0;

	return TRUE;
}
