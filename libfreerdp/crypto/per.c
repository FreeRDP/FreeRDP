/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ASN.1 Packed Encoding Rules (BER)
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/assert.h>
#include <winpr/print.h>

#include <freerdp/config.h>
#include <freerdp/crypto/per.h>

#include <freerdp/log.h>
#define TAG FREERDP_TAG("crypto.per")

BOOL freerdp_per_read_length(wStream* s, UINT16* length)
{
	BYTE byte = 0;

	WINPR_ASSERT(length);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	Stream_Read_UINT8(s, byte);

	if (byte & 0x80)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
			return FALSE;

		byte &= ~(0x80);
		*length = (byte << 8);
		Stream_Read_UINT8(s, byte);
		*length += byte;
	}
	else
	{
		*length = byte;
	}

	return TRUE;
}

BOOL freerdp_per_write_length(wStream* s, UINT16 length)
{
	if (length > 0x7F)
	{
		if (!Stream_EnsureRemainingCapacity(s, 2))
			return FALSE;
		Stream_Write_UINT16_BE(s, (length | 0x8000));
	}
	else
	{
		if (!Stream_EnsureRemainingCapacity(s, 1))
			return FALSE;
		Stream_Write_UINT8(s, (UINT8)length);
	}
	return TRUE;
}

BOOL freerdp_per_read_choice(wStream* s, BYTE* choice)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	Stream_Read_UINT8(s, *choice);
	return TRUE;
}

BOOL freerdp_per_write_choice(wStream* s, BYTE choice)
{
	if (!Stream_EnsureRemainingCapacity(s, 1))
		return FALSE;
	Stream_Write_UINT8(s, choice);
	return TRUE;
}

BOOL freerdp_per_read_selection(wStream* s, BYTE* selection)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	WINPR_ASSERT(selection);
	Stream_Read_UINT8(s, *selection);
	return TRUE;
}

BOOL freerdp_per_write_selection(wStream* s, BYTE selection)
{
	if (!Stream_EnsureRemainingCapacity(s, 1))
		return FALSE;
	Stream_Write_UINT8(s, selection);
	return TRUE;
}

BOOL freerdp_per_read_number_of_sets(wStream* s, BYTE* number)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	WINPR_ASSERT(number);
	Stream_Read_UINT8(s, *number);
	return TRUE;
}

BOOL freerdp_per_write_number_of_sets(wStream* s, BYTE number)
{
	if (!Stream_EnsureRemainingCapacity(s, 1))
		return FALSE;
	Stream_Write_UINT8(s, number);
	return TRUE;
}

BOOL freerdp_per_read_padding(wStream* s, UINT16 length)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, length))
		return FALSE;

	Stream_Seek(s, length);
	return TRUE;
}

BOOL freerdp_per_write_padding(wStream* s, UINT16 length)
{
	if (!Stream_EnsureRemainingCapacity(s, length))
		return FALSE;
	Stream_Zero(s, length);
	return TRUE;
}

BOOL freerdp_per_read_integer(wStream* s, UINT32* integer)
{
	UINT16 length = 0;

	WINPR_ASSERT(integer);

	if (!freerdp_per_read_length(s, &length))
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, length))
		return FALSE;

	if (length == 0)
		*integer = 0;
	else if (length == 1)
		Stream_Read_UINT8(s, *integer);
	else if (length == 2)
		Stream_Read_UINT16_BE(s, *integer);
	else
		return FALSE;

	return TRUE;
}

BOOL freerdp_per_write_integer(wStream* s, UINT32 integer)
{
	if (integer <= UINT8_MAX)
	{
		if (!freerdp_per_write_length(s, 1))
			return FALSE;
		if (!Stream_EnsureRemainingCapacity(s, 1))
			return FALSE;
		Stream_Write_UINT8(s, integer);
	}
	else if (integer <= UINT16_MAX)
	{
		if (!freerdp_per_write_length(s, 2))
			return FALSE;
		if (!Stream_EnsureRemainingCapacity(s, 2))
			return FALSE;
		Stream_Write_UINT16_BE(s, integer);
	}
	else if (integer <= UINT32_MAX)
	{
		if (!freerdp_per_write_length(s, 4))
			return FALSE;
		if (!Stream_EnsureRemainingCapacity(s, 4))
			return FALSE;
		Stream_Write_UINT32_BE(s, integer);
	}
	return TRUE;
}

BOOL freerdp_per_read_integer16(wStream* s, UINT16* integer, UINT16 min)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
		return FALSE;

	Stream_Read_UINT16_BE(s, *integer);

	if (*integer > UINT16_MAX - min)
	{
		WLog_WARN(TAG, "PER uint16 invalid value %" PRIu16 " > %" PRIu16, *integer,
		          UINT16_MAX - min);
		return FALSE;
	}

	*integer += min;

	return TRUE;
}

BOOL freerdp_per_write_integer16(wStream* s, UINT16 integer, UINT16 min)
{
	if (!Stream_EnsureRemainingCapacity(s, 2))
		return FALSE;
	Stream_Write_UINT16_BE(s, integer - min);
	return TRUE;
}

BOOL freerdp_per_read_enumerated(wStream* s, BYTE* enumerated, BYTE count)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	WINPR_ASSERT(enumerated);
	Stream_Read_UINT8(s, *enumerated);

	/* check that enumerated value falls within expected range */
	if (*enumerated + 1 > count)
	{
		WLog_WARN(TAG, "PER invalid data, expected %" PRIu8 " < %" PRIu8, *enumerated, count);
		return FALSE;
	}

	return TRUE;
}

BOOL freerdp_per_write_enumerated(wStream* s, BYTE enumerated, BYTE count)
{
	if (!Stream_EnsureRemainingCapacity(s, 1))
		return FALSE;
	Stream_Write_UINT8(s, enumerated);
	return TRUE;
}

static BOOL freerdp_per_check_oid_and_log_mismatch(const BYTE* got, const BYTE* expect,
                                                   size_t length)
{
	if (memcmp(got, expect, length) == 0)
	{
		return TRUE;
	}
	else
	{
		char* got_str = winpr_BinToHexString(got, length, TRUE);
		char* expect_str = winpr_BinToHexString(expect, length, TRUE);

		WLog_WARN(TAG, "PER OID mismatch, got %s, expected %s", got_str, expect_str);
		free(got_str);
		free(expect_str);
		return FALSE;
	}
}

BOOL freerdp_per_read_object_identifier(wStream* s, const BYTE oid[6])
{
	BYTE t12 = 0;
	UINT16 length = 0;
	BYTE a_oid[6] = { 0 };

	if (!freerdp_per_read_length(s, &length))
		return FALSE;

	if (length != 5)
	{
		WLog_WARN(TAG, "PER length, got %" PRIu16 ", expected 5", length);
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, length))
		return FALSE;

	Stream_Read_UINT8(s, t12); /* first two tuples */
	a_oid[0] = t12 / 40;
	a_oid[1] = t12 % 40;

	Stream_Read_UINT8(s, a_oid[2]); /* tuple 3 */
	Stream_Read_UINT8(s, a_oid[3]); /* tuple 4 */
	Stream_Read_UINT8(s, a_oid[4]); /* tuple 5 */
	Stream_Read_UINT8(s, a_oid[5]); /* tuple 6 */

	return freerdp_per_check_oid_and_log_mismatch(a_oid, oid, sizeof(a_oid));
}

BOOL freerdp_per_write_object_identifier(wStream* s, const BYTE oid[6])
{
	BYTE t12 = oid[0] * 40 + oid[1];
	if (!Stream_EnsureRemainingCapacity(s, 6))
		return FALSE;
	Stream_Write_UINT8(s, 5);      /* length */
	Stream_Write_UINT8(s, t12);    /* first two tuples */
	Stream_Write_UINT8(s, oid[2]); /* tuple 3 */
	Stream_Write_UINT8(s, oid[3]); /* tuple 4 */
	Stream_Write_UINT8(s, oid[4]); /* tuple 5 */
	Stream_Write_UINT8(s, oid[5]); /* tuple 6 */
	return TRUE;
}

static void freerdp_per_write_string(wStream* s, BYTE* str, int length)
{
	for (int i = 0; i < length; i++)
		Stream_Write_UINT8(s, str[i]);
}

BOOL freerdp_per_read_octet_string(wStream* s, const BYTE* oct_str, UINT16 length, UINT16 min)
{
	UINT16 mlength = 0;

	if (!freerdp_per_read_length(s, &mlength))
		return FALSE;

	if (mlength + min != length)
	{
		WLog_ERR(TAG, "length mismatch: %" PRIu16 "!= %" PRIu16, mlength + min, length);
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, length))
		return FALSE;

	const BYTE* a_oct_str = Stream_ConstPointer(s);
	Stream_Seek(s, length);

	return freerdp_per_check_oid_and_log_mismatch(a_oct_str, oct_str, length);
}

BOOL freerdp_per_write_octet_string(wStream* s, const BYTE* oct_str, UINT16 length, UINT16 min)
{
	UINT16 mlength = 0;

	mlength = (length >= min) ? length - min : min;

	if (!freerdp_per_write_length(s, mlength))
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, length))
		return FALSE;
	for (UINT16 i = 0; i < length; i++)
		Stream_Write_UINT8(s, oct_str[i]);
	return TRUE;
}

BOOL freerdp_per_read_numeric_string(wStream* s, UINT16 min)
{
	size_t length = 0;
	UINT16 mlength = 0;

	if (!freerdp_per_read_length(s, &mlength))
		return FALSE;

	length = (mlength + min + 1) / 2;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, length))
		return FALSE;

	Stream_Seek(s, length);
	return TRUE;
}

BOOL freerdp_per_write_numeric_string(wStream* s, const BYTE* num_str, UINT16 length, UINT16 min)
{
	WINPR_ASSERT(num_str || (length == 0));

	const UINT16 mlength = (length >= min) ? length - min : min;

	if (!freerdp_per_write_length(s, mlength))
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, length))
		return FALSE;
	for (UINT16 i = 0; i < length; i += 2)
	{
		BYTE c1 = num_str[i];
		BYTE c2 = ((i + 1) < length) ? num_str[i + 1] : 0x30;

		if ((c1 < 0x30) || (c2 < 0x30))
			return FALSE;

		c1 = (c1 - 0x30) % 10;
		c2 = (c2 - 0x30) % 10;
		const BYTE num = (c1 << 4) | c2;

		Stream_Write_UINT8(s, num); /* string */
	}
	return TRUE;
}

#if defined(WITH_FREERDP_3x_DEPRECATED)
BOOL per_read_length(wStream* s, UINT16* length)
{
	return freerdp_per_read_length(s, length);
}

BOOL per_write_length(wStream* s, UINT16 length)
{
	return freerdp_per_write_length(s, length);
}

BOOL per_read_choice(wStream* s, BYTE* choice)
{
	return freerdp_per_read_choice(s, choice);
}

BOOL per_write_choice(wStream* s, BYTE choice)
{
	return freerdp_per_write_choice(s, choice);
}

BOOL per_read_selection(wStream* s, BYTE* selection)
{
	return freerdp_per_read_selection(s, selection);
}

BOOL per_write_selection(wStream* s, BYTE selection)
{
	return freerdp_per_write_selection(s, selection);
}

BOOL per_read_number_of_sets(wStream* s, BYTE* number)
{
	return freerdp_per_read_number_of_sets(s, number);
}

BOOL per_write_number_of_sets(wStream* s, BYTE number)
{
	return freerdp_per_write_number_of_sets(s, number);
}

BOOL per_read_padding(wStream* s, UINT16 length)
{
	return freerdp_per_read_padding(s, length);
}

BOOL per_write_padding(wStream* s, UINT16 length)
{
	return freerdp_per_write_padding(s, length);
}

BOOL per_read_integer(wStream* s, UINT32* integer)
{
	return freerdp_per_read_integer(s, integer);
}

BOOL per_read_integer16(wStream* s, UINT16* integer, UINT16 min)
{
	return freerdp_per_read_integer16(s, integer, min);
}

BOOL per_write_integer(wStream* s, UINT32 integer)
{
	return freerdp_per_write_integer(s, integer);
}

BOOL per_write_integer16(wStream* s, UINT16 integer, UINT16 min)
{
	return freerdp_per_write_integer16(s, integer, min);
}

BOOL per_read_enumerated(wStream* s, BYTE* enumerated, BYTE count)
{
	return freerdp_per_read_enumerated(s, enumerated, count);
}

BOOL per_write_enumerated(wStream* s, BYTE enumerated, BYTE count)
{
	return freerdp_per_write_enumerated(s, enumerated, count);
}

BOOL per_write_object_identifier(wStream* s, const BYTE oid[6])
{
	return freerdp_per_write_object_identifier(s, oid);
}

BOOL per_read_object_identifier(wStream* s, const BYTE oid[6])
{
	return freerdp_per_read_object_identifier(s, oid);
}

BOOL per_read_octet_string(wStream* s, const BYTE* oct_str, UINT16 length, UINT16 min)
{
	return freerdp_per_read_octet_string(s, oct_str, length, min);
}

BOOL per_write_octet_string(wStream* s, const BYTE* oct_str, UINT16 length, UINT16 min)
{
	return freerdp_per_write_octet_string(s, oct_str, length, min);
}

BOOL per_read_numeric_string(wStream* s, UINT16 min)
{
	return freerdp_per_read_numeric_string(s, min);
}

BOOL per_write_numeric_string(wStream* s, const BYTE* num_str, UINT16 length, UINT16 min)
{
	return freerdp_per_write_numeric_string(s, num_str, length, min);
}
#endif
