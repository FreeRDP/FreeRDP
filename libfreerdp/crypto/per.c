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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/crypto/per.h>

/**
 * Read PER length.
 * @param s stream
 * @param length length
 * @return
 */

BOOL per_read_length(wStream* s, UINT16* length)
{
	BYTE byte;

	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, byte);

	if (byte & 0x80)
	{
		if (Stream_GetRemainingLength(s) < 1)
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
 */

void per_write_length(wStream* s, int length)
{
	if (length > 0x7F)
		Stream_Write_UINT16_BE(s, (length | 0x8000));
	else
		Stream_Write_UINT8(s, length);
}

/**
 * Read PER choice.
 * @param s stream
 * @param choice choice
 * @return
 */

BOOL per_read_choice(wStream* s, BYTE* choice)
{
	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, *choice);
	return TRUE;
}

/**
 * Write PER CHOICE.
 * @param s stream
 * @param choice index of chosen field
 */

void per_write_choice(wStream* s, BYTE choice)
{
	Stream_Write_UINT8(s, choice);
}

/**
 * Read PER selection.
 * @param s stream
 * @param selection selection
 * @return
 */

BOOL per_read_selection(wStream* s, BYTE* selection)
{
	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, *selection);
	return TRUE;
}

/**
 * Write PER selection for OPTIONAL fields.
 * @param s stream
 * @param selection bit map of selected fields
 */

void per_write_selection(wStream* s, BYTE selection)
{
	Stream_Write_UINT8(s, selection);
}

/**
 * Read PER number of sets.
 * @param s stream
 * @param number number of sets
 * @return
 */

BOOL per_read_number_of_sets(wStream* s, BYTE* number)
{
	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, *number);
	return TRUE;
}

/**
 * Write PER number of sets for SET OF.
 * @param s stream
 * @param number number of sets
 */

void per_write_number_of_sets(wStream* s, BYTE number)
{
	Stream_Write_UINT8(s, number);
}

/**
 * Read PER padding with zeros.
 * @param s stream
 * @param length
 */

BOOL per_read_padding(wStream* s, int length)
{
	if (((int) Stream_GetRemainingLength(s)) < length)
		return FALSE;

	Stream_Seek(s, length);
	return TRUE;
}

/**
 * Write PER padding with zeros.
 * @param s stream
 * @param length
 */

void per_write_padding(wStream* s, int length)
{
	int i;

	for (i = 0; i < length; i++)
		Stream_Write_UINT8(s, 0);
}

/**
 * Read PER INTEGER.
 * @param s stream
 * @param integer integer
 * @return
 */

BOOL per_read_integer(wStream* s, UINT32* integer)
{
	UINT16 length;

	if (!per_read_length(s, &length))
		return FALSE;

	if (Stream_GetRemainingLength(s) < length)
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
 */

void per_write_integer(wStream* s, UINT32 integer)
{
	if (integer <= 0xFF)
	{
		per_write_length(s, 1);
		Stream_Write_UINT8(s, integer);
	}
	else if (integer <= 0xFFFF)
	{
		per_write_length(s, 2);
		Stream_Write_UINT16_BE(s, integer);
	}
	else if (integer <= 0xFFFFFFFF)
	{
		per_write_length(s, 4);
		Stream_Write_UINT32_BE(s, integer);
	}
}

/**
 * Read PER INTEGER (UINT16).
 * @param s stream
 * @param integer integer
 * @param min minimum value
 * @return
 */

BOOL per_read_integer16(wStream* s, UINT16* integer, UINT16 min)
{
	if (Stream_GetRemainingLength(s) < 2)
		return FALSE;

	Stream_Read_UINT16_BE(s, *integer);

	if (*integer + min > 0xFFFF)
		return FALSE;

	*integer += min;

	return TRUE;
}

/**
 * Write PER INTEGER (UINT16).
 * @param s stream
 * @param integer integer
 * @param min minimum value
 */

void per_write_integer16(wStream* s, UINT16 integer, UINT16 min)
{
	Stream_Write_UINT16_BE(s, integer - min);
}

/**
 * Read PER ENUMERATED.
 * @param s stream
 * @param enumerated enumerated
 * @param count enumeration count
 * @return
 */

BOOL per_read_enumerated(wStream* s, BYTE* enumerated, BYTE count)
{
	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, *enumerated);

	/* check that enumerated value falls within expected range */
	if (*enumerated + 1 > count)
		return FALSE;

	return TRUE;
}

/**
 * Write PER ENUMERATED.
 * @param s stream
 * @param enumerated enumerated
 * @param count enumeration count
 * @return
 */

void per_write_enumerated(wStream* s, BYTE enumerated, BYTE count)
{
	Stream_Write_UINT8(s, enumerated);
}

/**
 * Read PER OBJECT_IDENTIFIER (OID).
 * @param s stream
 * @param oid object identifier (OID)
 * @warning It works correctly only for limited set of OIDs.
 * @return
 */

BOOL per_read_object_identifier(wStream* s, BYTE oid[6])
{
	BYTE t12;
	UINT16 length;
	BYTE a_oid[6];

	if (!per_read_length(s, &length))
		return FALSE;

	if (length != 5)
		return FALSE;

	if (Stream_GetRemainingLength(s) < length)
		return FALSE;

	Stream_Read_UINT8(s, t12); /* first two tuples */
	a_oid[0] = t12 / 40;
	a_oid[1] = t12 % 40;

	Stream_Read_UINT8(s, a_oid[2]); /* tuple 3 */
	Stream_Read_UINT8(s, a_oid[3]); /* tuple 4 */
	Stream_Read_UINT8(s, a_oid[4]); /* tuple 5 */
	Stream_Read_UINT8(s, a_oid[5]); /* tuple 6 */

	if ((a_oid[0] == oid[0]) && (a_oid[1] == oid[1]) &&
		(a_oid[2] == oid[2]) && (a_oid[3] == oid[3]) &&
		(a_oid[4] == oid[4]) && (a_oid[5] == oid[5]))
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/**
 * Write PER OBJECT_IDENTIFIER (OID)
 * @param s stream
 * @param oid object identifier (oid)
 * @warning It works correctly only for limited set of OIDs.
 */

void per_write_object_identifier(wStream* s, BYTE oid[6])
{
	BYTE t12 = oid[0] * 40 + oid[1];
	Stream_Write_UINT8(s, 5); /* length */
	Stream_Write_UINT8(s, t12); /* first two tuples */
	Stream_Write_UINT8(s, oid[2]); /* tuple 3 */
	Stream_Write_UINT8(s, oid[3]); /* tuple 4 */
	Stream_Write_UINT8(s, oid[4]); /* tuple 5 */
	Stream_Write_UINT8(s, oid[5]); /* tuple 6 */
}

/**
 * Write PER string.
 * @param s stream
 * @param str string
 * @param length string length
 */

void per_write_string(wStream* s, BYTE* str, int length)
{
	int i;

	for (i = 0; i < length; i++)
		Stream_Write_UINT8(s, str[i]);
}

/**
 * Read PER OCTET_STRING.
 * @param s stream
 * @param oct_str octet string
 * @param length string length
 * @param min minimum length
 * @return
 */

BOOL per_read_octet_string(wStream* s, BYTE* oct_str, int length, int min)
{
	int i;
	UINT16 mlength;
	BYTE* a_oct_str;

	if (!per_read_length(s, &mlength))
		return FALSE;

	if (mlength + min != length)
		return FALSE;

	if (((int) Stream_GetRemainingLength(s)) < length)
		return FALSE;

	a_oct_str = Stream_Pointer(s);
	Stream_Seek(s, length);

	for (i = 0; i < length; i++)
	{
		if (a_oct_str[i] != oct_str[i])
			return FALSE;
	}

	return TRUE;
}

/**
 * Write PER OCTET_STRING
 * @param s stream
 * @param oct_str octet string
 * @param length string length
 * @param min minimum string length
 */

void per_write_octet_string(wStream* s, BYTE* oct_str, int length, int min)
{
	int i;
	int mlength;

	mlength = (length - min >= 0) ? length - min : min;

	per_write_length(s, mlength);

	for (i = 0; i < length; i++)
		Stream_Write_UINT8(s, oct_str[i]);
}

/**
 * Read PER NumericString.
 * @param s stream
 * @param num_str numeric string
 * @param length string length
 * @param min minimum string length
 */

BOOL per_read_numeric_string(wStream* s, int min)
{
	int length;
	UINT16 mlength;

	if (!per_read_length(s, &mlength))
		return FALSE;

	length = (mlength + min + 1) / 2;

	if (((int) Stream_GetRemainingLength(s)) < length)
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
 */

void per_write_numeric_string(wStream* s, BYTE* num_str, int length, int min)
{
	int i;
	int mlength;
	BYTE num, c1, c2;

	mlength = (length - min >= 0) ? length - min : min;

	per_write_length(s, mlength);

	for (i = 0; i < length; i += 2)
	{
		c1 = num_str[i];
		c2 = ((i + 1) < length) ? num_str[i + 1] : 0x30;

		c1 = (c1 - 0x30) % 10;
		c2 = (c2 - 0x30) % 10;
		num = (c1 << 4) | c2;

		Stream_Write_UINT8(s, num); /* string */
	}
}
