/**
 * FreeRDP: A Remote Desktop Protocol Client
 * ASN.1 Packed Encoding Rules (BER)
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

#include "per.h"

/**
 * Read PER length.
 * @param s stream
 * @param length length
 * @return
 */

boolean per_read_length(STREAM* s, uint16* length)
{
	uint8 byte;

	stream_read_uint8(s, byte);

	if (byte & 0x80)
	{
		byte &= ~(0x80);
		*length = (byte << 8);
		stream_read_uint8(s, byte);
		*length += byte;
	}
	else
	{
		*length = byte;
	}

	return true;
}

/**
 * Write PER length.
 * @param s stream
 * @param length length
 */

void per_write_length(STREAM* s, int length)
{
	if (length > 0x7F)
		stream_write_uint16_be(s, (length | 0x8000));
	else
		stream_write_uint8(s, length);
}

/**
 * Read PER choice.
 * @param s stream
 * @param choice choice
 * @return
 */

boolean per_read_choice(STREAM* s, uint8* choice)
{
	stream_read_uint8(s, *choice);
	return true;
}

/**
 * Write PER CHOICE.
 * @param s stream
 * @param choice index of chosen field
 */

void per_write_choice(STREAM* s, uint8 choice)
{
	stream_write_uint8(s, choice);
}

/**
 * Read PER selection.
 * @param s stream
 * @param selection selection
 * @return
 */

boolean per_read_selection(STREAM* s, uint8* selection)
{
	stream_read_uint8(s, *selection);
	return true;
}

/**
 * Write PER selection for OPTIONAL fields.
 * @param s stream
 * @param selection bit map of selected fields
 */

void per_write_selection(STREAM* s, uint8 selection)
{
	stream_write_uint8(s, selection);
}

/**
 * Read PER number of sets.
 * @param s stream
 * @param number number of sets
 * @return
 */

boolean per_read_number_of_sets(STREAM* s, uint8* number)
{
	stream_read_uint8(s, *number);
	return true;
}

/**
 * Write PER number of sets for SET OF.
 * @param s stream
 * @param number number of sets
 */

void per_write_number_of_sets(STREAM* s, uint8 number)
{
	stream_write_uint8(s, number);
}

/**
 * Read PER padding with zeros.
 * @param s stream
 * @param length
 */

boolean per_read_padding(STREAM* s, int length)
{
	stream_seek(s, length);

	return true;
}

/**
 * Write PER padding with zeros.
 * @param s stream
 * @param length
 */

void per_write_padding(STREAM* s, int length)
{
	int i;

	for (i = 0; i < length; i++)
		stream_write_uint8(s, 0);
}

/**
 * Read PER INTEGER.
 * @param s stream
 * @param integer integer
 * @return
 */

boolean per_read_integer(STREAM* s, uint32* integer)
{
	uint16 length;

	per_read_length(s, &length);

	if (length == 1)
		stream_read_uint8(s, *integer);
	else if (length == 2)
		stream_read_uint16_be(s, *integer);
	else
		return false;

	return true;
}

/**
 * Write PER INTEGER.
 * @param s stream
 * @param integer integer
 */

void per_write_integer(STREAM* s, uint32 integer)
{
	if (integer <= 0xFF)
	{
		per_write_length(s, 1);
		stream_write_uint8(s, integer);
	}
	else if (integer <= 0xFFFF)
	{
		per_write_length(s, 2);
		stream_write_uint16_be(s, integer);
	}
	else if (integer <= 0xFFFFFFFF)
	{
		per_write_length(s, 4);
		stream_write_uint32_be(s, integer);
	}
}

/**
 * Read PER INTEGER (uint16).
 * @param s stream
 * @param integer integer
 * @param min minimum value
 * @return
 */

boolean per_read_integer16(STREAM* s, uint16* integer, uint16 min)
{
	stream_read_uint16_be(s, *integer);

	if (*integer + min > 0xFFFF)
		return false;

	*integer += min;

	return true;
}

/**
 * Write PER INTEGER (uint16).
 * @param s stream
 * @param integer integer
 * @param min minimum value
 */

void per_write_integer16(STREAM* s, uint16 integer, uint16 min)
{
	stream_write_uint16_be(s, integer - min);
}

/**
 * Read PER ENUMERATED.
 * @param s stream
 * @param enumerated enumerated
 * @param count enumeration count
 * @return
 */

boolean per_read_enumerated(STREAM* s, uint8* enumerated, uint8 count)
{
	stream_read_uint8(s, *enumerated);

	/* check that enumerated value falls within expected range */
	if (*enumerated + 1 > count)
		return false;

	return true;
}

/**
 * Write PER ENUMERATED.
 * @param s stream
 * @param enumerated enumerated
 * @param count enumeration count
 * @return
 */

void per_write_enumerated(STREAM* s, uint8 enumerated, uint8 count)
{
	stream_write_uint8(s, enumerated);
}

/**
 * Read PER OBJECT_IDENTIFIER (OID).
 * @param s stream
 * @param oid object identifier (OID)
 * @return
 */

boolean per_read_object_identifier(STREAM* s, uint8 oid[6])
{
	uint8 t12;
	uint16 length;
	uint8 a_oid[6];


	per_read_length(s, &length); /* length */

	if (length != 5)
		return false;

	stream_read_uint8(s, t12); /* first two tuples */
	a_oid[0] = (t12 >> 4);
	a_oid[1] = (t12 & 0x0F);

	stream_read_uint8(s, a_oid[2]); /* tuple 3 */
	stream_read_uint8(s, a_oid[3]); /* tuple 4 */
	stream_read_uint8(s, a_oid[4]); /* tuple 5 */
	stream_read_uint8(s, a_oid[5]); /* tuple 6 */

	if ((a_oid[0] == oid[0]) && (a_oid[1] == oid[1]) &&
		(a_oid[2] == oid[2]) && (a_oid[3] == oid[3]) &&
		(a_oid[4] == oid[4]) && (a_oid[5] == oid[5]))
	{
		return true;
	}
	else
	{
		return false;
	}
}

/**
 * Write PER OBJECT_IDENTIFIER (OID)
 * @param s stream
 * @param oid object identifier (oid)
 */

void per_write_object_identifier(STREAM* s, uint8 oid[6])
{
	uint8 t12 = (oid[0] << 4) & (oid[1] & 0x0F);
	stream_write_uint8(s, 5); /* length */
	stream_write_uint8(s, t12); /* first two tuples */
	stream_write_uint8(s, oid[2]); /* tuple 3 */
	stream_write_uint8(s, oid[3]); /* tuple 4 */
	stream_write_uint8(s, oid[4]); /* tuple 5 */
	stream_write_uint8(s, oid[5]); /* tuple 6 */
}

/**
 * Write PER string.
 * @param s stream
 * @param str string
 * @param length string length
 */

void per_write_string(STREAM* s, uint8* str, int length)
{
	int i;

	for (i = 0; i < length; i++)
		stream_write_uint8(s, str[i]);
}

/**
 * Read PER OCTET_STRING.
 * @param s stream
 * @param oct_str octet string
 * @param length string length
 * @param min minimum length
 * @return
 */

boolean per_read_octet_string(STREAM* s, uint8* oct_str, int length, int min)
{
	int i;
	uint16 mlength;
	uint8* a_oct_str;

	per_read_length(s, &mlength);

	if (mlength + min != length)
		return false;

	a_oct_str = s->p;
	stream_seek(s, length);

	for (i = 0; i < length; i++)
	{
		if (a_oct_str[i] != oct_str[i])
			return false;
	}

	return true;
}

/**
 * Write PER OCTET_STRING
 * @param s stream
 * @param oct_str octet string
 * @param length string length
 * @param min minimum string length
 */

void per_write_octet_string(STREAM* s, uint8* oct_str, int length, int min)
{
	int i;
	int mlength;

	mlength = (length - min >= 0) ? length - min : min;

	per_write_length(s, mlength);

	for (i = 0; i < length; i++)
		stream_write_uint8(s, oct_str[i]);
}

/**
 * Read PER NumericString.
 * @param s stream
 * @param num_str numeric string
 * @param length string length
 * @param min minimum string length
 */

boolean per_read_numeric_string(STREAM* s, int min)
{
	int i;
	int length;
	uint16 mlength;

	per_read_length(s, &mlength);

	length = mlength + min;

	for (i = 0; i < length; i += 2)
	{
		stream_seek(s, 1);
	}

	return true;
}

/**
 * Write PER NumericString.
 * @param s stream
 * @param num_str numeric string
 * @param length string length
 * @param min minimum string length
 */

void per_write_numeric_string(STREAM* s, uint8* num_str, int length, int min)
{
	int i;
	int mlength;
	uint8 num, c1, c2;

	mlength = (length - min >= 0) ? length - min : min;

	per_write_length(s, mlength);

	for (i = 0; i < length; i += 2)
	{
		c1 = num_str[i];
		c2 = ((i + 1) < length) ? num_str[i + 1] : 0x30;

		c1 = (c1 - 0x30) % 10;
		c2 = (c2 - 0x30) % 10;
		num = (c1 << 4) | c2;

		stream_write_uint8(s, num); /* string */
	}
}
