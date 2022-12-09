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

/**
 * Read PER length.
 *
 * @param s stream to read from
 * @param length A pointer to return the length read, must not be NULL
 *
 * @return \b TRUE for success, \b FALSE otherwise.
 */

BOOL per_read_length(wStream* s, UINT16* length)
{
	BYTE byte;

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

/**
 * Write PER length.
 * @param s stream
 * @param length length
 *
 * @return \b TRUE for success, \b FALSE otherwise.
 */

BOOL per_write_length(wStream* s, UINT16 length)
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

/**
 * Read PER choice.
 * @param s stream
 * @param choice choice
 *
 * @return \b TRUE for success, \b FALSE otherwise.
 */

BOOL per_read_choice(wStream* s, BYTE* choice)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	Stream_Read_UINT8(s, *choice);
	return TRUE;
}

/**
 * Write PER CHOICE.
 * @param s stream
 * @param choice index of chosen field
 *
 * @return \b TRUE for success, \b FALSE otherwise.
 */

BOOL per_write_choice(wStream* s, BYTE choice)
{
	if (!Stream_EnsureRemainingCapacity(s, 1))
		return FALSE;
	Stream_Write_UINT8(s, choice);
	return TRUE;
}

/**
 * Read PER selection.
 * @param s stream
 * @param selection selection
 *
 * @return \b TRUE for success, \b FALSE otherwise.
 */

BOOL per_read_selection(wStream* s, BYTE* selection)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	WINPR_ASSERT(selection);
	Stream_Read_UINT8(s, *selection);
	return TRUE;
}

/**
 * Write PER selection for OPTIONAL fields.
 * @param s stream
 * @param selection bit map of selected fields
 *
 * @return \b TRUE for success, \b FALSE otherwise.
 */

BOOL per_write_selection(wStream* s, BYTE selection)
{
	if (!Stream_EnsureRemainingCapacity(s, 1))
		return FALSE;
	Stream_Write_UINT8(s, selection);
	return TRUE;
}

/**
 * Read PER number of sets.
 * @param s stream
 * @param number number of sets
 *
 * @return \b TRUE for success, \b FALSE otherwise.
 */

BOOL per_read_number_of_sets(wStream* s, BYTE* number)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	WINPR_ASSERT(number);
	Stream_Read_UINT8(s, *number);
	return TRUE;
}

/**
 * Write PER number of sets for SET OF.
 *
 * @param s stream
 * @param number number of sets
 *
 * @return \b TRUE for success, \b FALSE otherwise.
 */

BOOL per_write_number_of_sets(wStream* s, BYTE number)
{
	if (!Stream_EnsureRemainingCapacity(s, 1))
		return FALSE;
	Stream_Write_UINT8(s, number);
	return TRUE;
}

/**
 * Read PER padding with zeros.
 *
 * @param s A stream to read from
 * @param length the data to write
 *
 * @return \b TRUE for success, \b FALSE otherwise.
 */

BOOL per_read_padding(wStream* s, UINT16 length)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, length))
		return FALSE;

	Stream_Seek(s, length);
	return TRUE;
}

/**
 * Write PER padding with zeros.
 * @param s A stream to write to
 * @param length the data to write
 *
 * @return \b TRUE for success, \b FALSE otherwise.
 */

BOOL per_write_padding(wStream* s, UINT16 length)
{
	if (!Stream_EnsureRemainingCapacity(s, length))
		return FALSE;
	Stream_Zero(s, length);
	return TRUE;
}

/**
 * Read PER INTEGER.
 * @param s stream
 * @param integer integer
 *
 * @return \b TRUE for success, \b FALSE otherwise.
 */

BOOL per_read_integer(wStream* s, UINT32* integer)
{
	UINT16 length;

	WINPR_ASSERT(integer);

	if (!per_read_length(s, &length))
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

/**
 * Write PER INTEGER.
 * @param s stream
 * @param integer integer
 *
 * @return \b TRUE for success, \b FALSE otherwise.
 */

BOOL per_write_integer(wStream* s, UINT32 integer)
{
	if (integer <= UINT8_MAX)
	{
		if (!per_write_length(s, 1))
			return FALSE;
		if (!Stream_EnsureRemainingCapacity(s, 1))
			return FALSE;
		Stream_Write_UINT8(s, integer);
	}
	else if (integer <= UINT16_MAX)
	{
		if (!per_write_length(s, 2))
			return FALSE;
		if (!Stream_EnsureRemainingCapacity(s, 2))
			return FALSE;
		Stream_Write_UINT16_BE(s, integer);
	}
	else if (integer <= UINT32_MAX)
	{
		if (!per_write_length(s, 4))
			return FALSE;
		if (!Stream_EnsureRemainingCapacity(s, 4))
			return FALSE;
		Stream_Write_UINT32_BE(s, integer);
	}
	return TRUE;
}

/**
 * Read PER INTEGER (UINT16).
 *
 * @param s The stream to read from
 * @param integer The integer result variable pointer, must not be NULL
 * @param min minimum value
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL per_read_integer16(wStream* s, UINT16* integer, UINT16 min)
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

/**
 * Write PER INTEGER (UINT16).
 * @param s stream
 * @param integer integer
 * @param min minimum value
 *
 * @return \b TRUE for success, \b FALSE otherwise.
 */

BOOL per_write_integer16(wStream* s, UINT16 integer, UINT16 min)
{
	if (!Stream_EnsureRemainingCapacity(s, 2))
		return FALSE;
	Stream_Write_UINT16_BE(s, integer - min);
	return TRUE;
}

/**
 * Read PER ENUMERATED.
 *
 * @param s The stream to read from
 * @param enumerated enumerated result variable, must not be NULL
 * @param count enumeration count
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL per_read_enumerated(wStream* s, BYTE* enumerated, BYTE count)
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

/**
 * Write PER ENUMERATED.
 *
 * @param s The stream to write to
 * @param enumerated enumerated
 * @param count enumeration count
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL per_write_enumerated(wStream* s, BYTE enumerated, BYTE count)
{
	if (!Stream_EnsureRemainingCapacity(s, 1))
		return FALSE;
	Stream_Write_UINT8(s, enumerated);
	return TRUE;
}

static BOOL per_check_oid_and_log_mismatch(const BYTE* got, const BYTE* expect, size_t length)
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

/**
 * Read PER OBJECT_IDENTIFIER (OID).
 *
 * @param s The stream to read from
 * @param oid object identifier (OID)
 * @warning It works correctly only for limited set of OIDs.
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL per_read_object_identifier(wStream* s, const BYTE oid[6])
{
	BYTE t12;
	UINT16 length;
	BYTE a_oid[6] = { 0 };

	if (!per_read_length(s, &length))
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

	return per_check_oid_and_log_mismatch(a_oid, oid, sizeof(a_oid));
}

/**
 * Write PER OBJECT_IDENTIFIER (OID)
 * @param s stream
 * @param oid object identifier (oid)
 * @warning It works correctly only for limited set of OIDs.
 *
 * @return \b TRUE for success, \b FALSE otherwise.
 */

BOOL per_write_object_identifier(wStream* s, const BYTE oid[6])
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

/**
 * Write PER string.
 * @param s stream
 * @param str string
 * @param length string length
 */

static void per_write_string(wStream* s, BYTE* str, int length)
{
	int i;

	for (i = 0; i < length; i++)
		Stream_Write_UINT8(s, str[i]);
}

/**
 * Read PER OCTET_STRING.
 *
 * @param s The stream to read from
 * @param oct_str octet string
 * @param length string length
 * @param min minimum length
 *
 * @return \b TRUE for success, \b FALSE otherwise.
 */

BOOL per_read_octet_string(wStream* s, const BYTE* oct_str, UINT16 length, UINT16 min)
{
	UINT16 mlength;
	BYTE* a_oct_str;

	if (!per_read_length(s, &mlength))
		return FALSE;

	if (mlength + min != length)
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, length))
		return FALSE;

	a_oct_str = Stream_Pointer(s);
	Stream_Seek(s, length);

	return per_check_oid_and_log_mismatch(a_oct_str, oct_str, length);
}

/**
 * Write PER OCTET_STRING
 * @param s stream
 * @param oct_str octet string
 * @param length string length
 * @param min minimum string length
 *
 * @return \b TRUE for success, \b FALSE otherwise.
 */

BOOL per_write_octet_string(wStream* s, const BYTE* oct_str, UINT16 length, UINT16 min)
{
	UINT16 i;
	UINT16 mlength;

	mlength = (length >= min) ? length - min : min;

	if (!per_write_length(s, mlength))
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, length))
		return FALSE;
	for (i = 0; i < length; i++)
		Stream_Write_UINT8(s, oct_str[i]);
	return TRUE;
}

/**
 * Read PER NumericString.
 * @param s stream
 * @param min minimum string length
 *
 * @return \b TRUE for success, \b FALSE otherwise.
 */

BOOL per_read_numeric_string(wStream* s, UINT16 min)
{
	size_t length;
	UINT16 mlength;

	if (!per_read_length(s, &mlength))
		return FALSE;

	length = (mlength + min + 1) / 2;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, length))
		return FALSE;

	Stream_Seek(s, length);
	return TRUE;
}

/**
 * Write PER NumericString.
 * @param s stream
 * @param num_str numeric string
 * @param length string length
 * @param min minimum string length
 *
 * @return \b TRUE for success, \b FALSE otherwise.
 */

BOOL per_write_numeric_string(wStream* s, const BYTE* num_str, UINT16 length, UINT16 min)
{
	UINT16 i;
	UINT16 mlength;
	BYTE num, c1, c2;

	mlength = (length >= min) ? length - min : min;

	if (!per_write_length(s, mlength))
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, length))
		return FALSE;
	for (i = 0; i < length; i += 2)
	{
		c1 = num_str[i];
		c2 = ((i + 1) < length) ? num_str[i + 1] : 0x30;

		c1 = (c1 - 0x30) % 10;
		c2 = (c2 - 0x30) % 10;
		num = (c1 << 4) | c2;

		Stream_Write_UINT8(s, num); /* string */
	}
	return TRUE;
}
