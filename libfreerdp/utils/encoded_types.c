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
	BYTE byte = 0;

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
			*value = (((INT32)si.val1) << 8) | ((INT32)si.val2);
			break;
		case THREE_BYTE_VAL:
			Stream_Read_UINT8(s, si.val2);
			Stream_Read_UINT8(s, si.val3);
			*value = (((INT32)si.val1) << 16) | (((INT32)si.val2) << 8) | ((INT32)si.val3);
			break;
		case FOUR_BYTE_VAL:
			Stream_Read_UINT8(s, si.val2);
			Stream_Read_UINT8(s, si.val3);
			Stream_Read_UINT8(s, si.val4);
			*value = (((INT32)si.val1) << 24) | (((INT32)si.val2) << 16) | (((INT32)si.val3) << 8) |
			         ((INT32)si.val4);
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

BOOL freerdp_write_four_byte_signed_integer(wStream* s, INT32 value)
{
	FOUR_BYTE_SIGNED_INTEGER si = { 0 };

	WINPR_ASSERT(s);
	if (value > FREERDP_FOUR_BYTE_SIGNED_INT_MAX)
		return FALSE;
	if (value < FREERDP_FOUR_BYTE_SIGNED_INT_MIN)
		return FALSE;

	if (value < 0)
	{
		si.s = NEGATIVE_VAL;
		value = -value;
	}

	if (value <= 0x1F)
	{
		si.c = ONE_BYTE_VAL;
		si.val1 = value & 0x1f;
	}
	else if (value <= 0x1FFF)
	{
		si.c = TWO_BYTE_VAL;
		si.val1 = (value >> 8) & 0x1f;
		si.val2 = value & 0xff;
	}
	else if (value <= 0x1FFFFF)
	{
		si.c = THREE_BYTE_VAL;
		si.val1 = (value >> 16) & 0x1f;
		si.val2 = (value >> 8) & 0xff;
		si.val3 = value & 0xff;
	}
	else if (value <= 0x1FFFFFFF)
	{
		si.c = FOUR_BYTE_VAL;
		si.val1 = (value >> 24) & 0x1f;
		si.val2 = (value >> 16) & 0xff;
		si.val3 = (value >> 8) & 0xff;
		si.val4 = value & 0xff;
	}
	else
	{
		WLog_ERR(TAG, "Invalid byte count for value %" PRId32, value);
		return FALSE;
	}

	if (!Stream_EnsureRemainingCapacity(s, si.c + 1))
		return FALSE;

	const BYTE byte = ((si.c << 6) & 0xC0) | ((si.s << 5) & 0x20) | (si.val1 & 0x1F);
	Stream_Write_UINT8(s, byte);

	switch (si.c)
	{
		case ONE_BYTE_VAL:
			break;
		case TWO_BYTE_VAL:
			Stream_Write_UINT8(s, si.val2);
			break;
		case THREE_BYTE_VAL:
			Stream_Write_UINT8(s, si.val2);
			Stream_Write_UINT8(s, si.val3);
			break;
		case FOUR_BYTE_VAL:
			Stream_Write_UINT8(s, si.val2);
			Stream_Write_UINT8(s, si.val3);
			Stream_Write_UINT8(s, si.val4);
			break;
		case FIVE_BYTE_VAL:
		case SIX_BYTE_VAL:
		case SEVEN_BYTE_VAL:
		case EIGHT_BYTE_VAL:
		default:
			WLog_ERR(TAG, "Invalid byte count value in si.c: %u", si.c);
			return FALSE;
	}

	return TRUE;
}

BOOL freerdp_read_four_byte_float(wStream* s, double* value)
{
	return freerdp_read_four_byte_float_exp(s, value, NULL);
}

BOOL freerdp_read_four_byte_float_exp(wStream* s, double* value, BYTE* exp)
{
	FOUR_BYTE_FLOAT f = { 0 };
	UINT32 base = 0;
	BYTE byte = 0;

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
			base = (((UINT32)f.val1) << 8) | ((UINT32)f.val2);
			break;
		case THREE_BYTE_VAL:
			Stream_Read_UINT8(s, f.val2);
			Stream_Read_UINT8(s, f.val3);
			base = (((UINT32)f.val1) << 16) | (((UINT32)f.val2) << 8) | ((UINT32)f.val3);
			break;
		case FOUR_BYTE_VAL:
			Stream_Read_UINT8(s, f.val2);
			Stream_Read_UINT8(s, f.val3);
			Stream_Read_UINT8(s, f.val4);
			base = (((UINT32)f.val1) << 24) | (((UINT32)f.val2) << 16) | (((UINT32)f.val3) << 8) |
			       ((UINT32)f.val4);
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

	if (exp)
		*exp = f.e;

	return TRUE;
}

BOOL freerdp_write_four_byte_float(wStream* s, double value)
{
	FOUR_BYTE_FLOAT si = { 0 };

	WINPR_ASSERT(s);

	if (value > FREERDP_FOUR_BYTE_FLOAT_MAX)
		return FALSE;
	if (value < FREERDP_FOUR_BYTE_FLOAT_MIN)
		return FALSE;

	if (value < 0)
	{
		si.s = NEGATIVE_VAL;
		value = -value;
	}

	int exp = 0;
	double ival = FP_NAN;
	const double aval = fabs(value);
	const double frac = modf(aval, &ival);
	if (frac != 0.0)
	{
		const double maxfrac = frac * 10000000.0;
		if (maxfrac <= 1.0)
			exp = 0;
		else if (maxfrac <= 10.0)
			exp = 1;
		else if (maxfrac <= 100.0)
			exp = 2;
		else if (maxfrac <= 1000.0)
			exp = 3;
		else if (maxfrac <= 10000.0)
			exp = 4;
		else if (maxfrac <= 100000.0)
			exp = 5;
		else if (maxfrac <= 1000000.0)
			exp = 6;
		else
			exp = 7;
	}

	UINT64 base = (UINT64)llround(aval);
	while (exp >= 0)
	{
		const double div = pow(10.0, exp);
		const double dval = (aval * div);
		base = (UINT64)dval;
		if (base <= 0x03ffffff)
			break;
		exp--;
	}

	if (exp < 0)
		return FALSE;

	si.e = (BYTE)exp;
	if (base <= 0x03)
	{
		si.c = ONE_BYTE_VAL;
		si.val1 = base & 0x03;
	}
	else if (base <= 0x03ff)
	{
		si.c = TWO_BYTE_VAL;
		si.val1 = (base >> 8) & 0x03;
		si.val2 = base & 0xff;
	}
	else if (base <= 0x03ffff)
	{
		si.c = THREE_BYTE_VAL;
		si.val1 = (base >> 16) & 0x03;
		si.val2 = (base >> 8) & 0xff;
		si.val3 = base & 0xff;
	}
	else if (base <= 0x03ffffff)
	{
		si.c = FOUR_BYTE_VAL;
		si.val1 = (base >> 24) & 0x03;
		si.val2 = (base >> 16) & 0xff;
		si.val3 = (base >> 8) & 0xff;
		si.val4 = base & 0xff;
	}
	else
	{
		WLog_ERR(TAG, "Invalid byte count for value %ld", value);
		return FALSE;
	}

	if (!Stream_EnsureRemainingCapacity(s, si.c + 1))
		return FALSE;

	const BYTE byte =
	    ((si.c << 6) & 0xC0) | ((si.s << 5) & 0x20) | ((si.e << 2) & 0x1c) | (si.val1 & 0x03);
	Stream_Write_UINT8(s, byte);

	switch (si.c)
	{
		case ONE_BYTE_VAL:
			break;
		case TWO_BYTE_VAL:
			Stream_Write_UINT8(s, si.val2);
			break;
		case THREE_BYTE_VAL:
			Stream_Write_UINT8(s, si.val2);
			Stream_Write_UINT8(s, si.val3);
			break;
		case FOUR_BYTE_VAL:
			Stream_Write_UINT8(s, si.val2);
			Stream_Write_UINT8(s, si.val3);
			Stream_Write_UINT8(s, si.val4);
			break;
		case FIVE_BYTE_VAL:
		case SIX_BYTE_VAL:
		case SEVEN_BYTE_VAL:
		case EIGHT_BYTE_VAL:
		default:
			WLog_ERR(TAG, "Invalid byte count value in si.c: %u", si.c);
			return FALSE;
	}

	return TRUE;
}
