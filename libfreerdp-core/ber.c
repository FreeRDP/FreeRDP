/**
 * FreeRDP: A Remote Desktop Protocol Client
 * ASN.1 Basic Encoding Rules (BER)
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

#include "ber.h"

void ber_read_length(STREAM* s, int* length)
{
	uint8 byte;

	stream_read_uint8(s, byte);

	if (byte & 0x80)
	{
		byte &= ~(0x80);

		if (byte == 1)
			stream_read_uint8(s, *length);
		if (byte == 2)
			stream_read_uint16_be(s, *length);
	}
	else
	{
		*length = byte;
	}
}

/**
 * Write BER length.
 * @param s stream
 * @param length length
 */

int ber_write_length(STREAM* s, int length)
{
	if (length > 0x7F)
	{
		stream_write_uint8(s, 0x82);
		stream_write_uint16_be(s, length);
		return 3;
	}
	else
	{
		stream_write_uint8(s, length);
		return 1;
	}
}

int _ber_skip_length(int length)
{
	if (length > 0x7F)
		return 3;
	else
		return 1;
}

int ber_get_content_length(int length)
{
	if (length - 1 > 0x7F)
		return length - 4;
	else
		return length - 2;
}

/**
 * Read BER Universal tag.
 * @param s stream
 * @param tag BER universally-defined tag
 * @return
 */

boolean ber_read_universal_tag(STREAM* s, uint8 tag, boolean pc)
{
	uint8 byte;

	stream_read_uint8(s, byte);

	if (byte != (BER_CLASS_UNIV | BER_PC(pc) | (BER_TAG_MASK & tag)))
		return false;

	return true;
}

/**
 * Write BER Universal tag.
 * @param s stream
 * @param tag BER universally-defined tag
 * @param pc primitive (false) or constructed (true)
 */

void ber_write_universal_tag(STREAM* s, uint8 tag, boolean pc)
{
	stream_write_uint8(s, (BER_CLASS_UNIV | BER_PC(pc)) | (BER_TAG_MASK & tag));
}

/**
 * Read BER Application tag.
 * @param s stream
 * @param tag BER application-defined tag
 * @param length length
 */

boolean ber_read_application_tag(STREAM* s, uint8 tag, int* length)
{
	uint8 byte;

	if (tag > 30)
	{
		stream_read_uint8(s, byte);

		if (byte != ((BER_CLASS_APPL | BER_CONSTRUCT) | BER_TAG_MASK))
			return false;

		stream_read_uint8(s, byte);

		if (byte != tag)
			return false;

		ber_read_length(s, length);
	}
	else
	{
		stream_read_uint8(s, byte);

		if (byte != ((BER_CLASS_APPL | BER_CONSTRUCT) | (BER_TAG_MASK & tag)))
			return false;

		ber_read_length(s, length);
	}

	return true;
}

/**
 * Write BER Application tag.
 * @param s stream
 * @param tag BER application-defined tag
 * @param length length
 */

void ber_write_application_tag(STREAM* s, uint8 tag, int length)
{
	if (tag > 30)
	{
		stream_write_uint8(s, (BER_CLASS_APPL | BER_CONSTRUCT) | BER_TAG_MASK);
		stream_write_uint8(s, tag);
		ber_write_length(s, length);
	}
	else
	{
		stream_write_uint8(s, (BER_CLASS_APPL | BER_CONSTRUCT) | (BER_TAG_MASK & tag));
		ber_write_length(s, length);
	}
}

boolean ber_read_contextual_tag(STREAM* s, uint8 tag, int* length, boolean pc)
{
	uint8 byte;

	stream_read_uint8(s, byte);

	if (byte != ((BER_CLASS_CTXT | BER_PC(pc)) | (BER_TAG_MASK & tag)))
	{
		stream_rewind(s, 1);
		return false;
	}

	ber_read_length(s, length);

	return true;
}

int ber_write_contextual_tag(STREAM* s, uint8 tag, int length, boolean pc)
{
	stream_write_uint8(s, (BER_CLASS_CTXT | BER_PC(pc)) | (BER_TAG_MASK & tag));
	return ber_write_length(s, length) + 1;
}

int ber_skip_contextual_tag(int length)
{
	return _ber_skip_length(length) + 1;
}

boolean ber_read_sequence_tag(STREAM* s, int* length)
{
	uint8 byte;

	stream_read_uint8(s, byte);

	if (byte != ((BER_CLASS_UNIV | BER_CONSTRUCT) | (BER_TAG_SEQUENCE_OF)))
		return false;

	ber_read_length(s, length);

	return true;
}

/**
 * Write BER SEQUENCE tag.
 * @param s stream
 * @param length length
 */

int ber_write_sequence_tag(STREAM* s, int length)
{
	stream_write_uint8(s, (BER_CLASS_UNIV | BER_CONSTRUCT) | (BER_TAG_MASK & BER_TAG_SEQUENCE));
	return ber_write_length(s, length) + 1;
}

int ber_skip_sequence(int length)
{
	return 1 + _ber_skip_length(length) + length;
}

int ber_skip_sequence_tag(int length)
{
	return 1 + _ber_skip_length(length);
}

boolean ber_read_enumerated(STREAM* s, uint8* enumerated, uint8 count)
{
	int length;

	ber_read_universal_tag(s, BER_TAG_ENUMERATED, false);
	ber_read_length(s, &length);

	if (length == 1)
		stream_read_uint8(s, *enumerated);
	else
		return false;

	/* check that enumerated value falls within expected range */
	if (*enumerated + 1 > count)
		return false;

	return true;
}

void ber_write_enumerated(STREAM* s, uint8 enumerated, uint8 count)
{
	ber_write_universal_tag(s, BER_TAG_ENUMERATED, false);
	ber_write_length(s, 1);
	stream_write_uint8(s, enumerated);
}

boolean ber_read_bit_string(STREAM* s, int* length, uint8* padding)
{
	ber_read_universal_tag(s, BER_TAG_BIT_STRING, false);
	ber_read_length(s, length);
	stream_read_uint8(s, *padding);

	return true;
}

boolean ber_read_octet_string(STREAM* s, int* length)
{
	ber_read_universal_tag(s, BER_TAG_OCTET_STRING, false);
	ber_read_length(s, length);

	return true;
}

/**
 * Write a BER OCTET_STRING
 * @param s stream
 * @param oct_str octet string
 * @param length string length
 */

void ber_write_octet_string(STREAM* s, const uint8* oct_str, int length)
{
	ber_write_universal_tag(s, BER_TAG_OCTET_STRING, false);
	ber_write_length(s, length);
	stream_write(s, oct_str, length);
}

int ber_write_octet_string_tag(STREAM* s, int length)
{
	ber_write_universal_tag(s, BER_TAG_OCTET_STRING, false);
	ber_write_length(s, length);
	return 1 + _ber_skip_length(length);
}

int ber_skip_octet_string(int length)
{
	return 1 + _ber_skip_length(length) + length;
}

/**
 * Read a BER BOOLEAN
 * @param s
 * @param value
 */

boolean ber_read_boolean(STREAM* s, boolean* value)
{
	int length;
	uint8 v;

	if (!ber_read_universal_tag(s, BER_TAG_BOOLEAN, false))
		return false;
	ber_read_length(s, &length);
	if (length != 1)
		return false;
	stream_read_uint8(s, v);
	*value = (v ? true : false);
	return true;
}

/**
 * Write a BER BOOLEAN
 * @param s
 * @param value
 */

void ber_write_boolean(STREAM* s, boolean value)
{
	ber_write_universal_tag(s, BER_TAG_BOOLEAN, false);
	ber_write_length(s, 1);
	stream_write_uint8(s, (value == true) ? 0xFF : 0);
}

boolean ber_read_integer(STREAM* s, uint32* value)
{
	int length;

	ber_read_universal_tag(s, BER_TAG_INTEGER, false);
	ber_read_length(s, &length);

	if (value == NULL)
	{
		stream_seek(s, length);
		return true;
	}

	if (length == 1)
		stream_read_uint8(s, *value);
	else if (length == 2)
		stream_read_uint16_be(s, *value);
	else if (length == 3)
	{
		uint8 byte;
		stream_read_uint8(s, byte);
		stream_read_uint16_be(s, *value);
		*value += (byte << 16);
	}
	else if (length == 4)
		stream_read_uint32_be(s, *value);
	else
		return false;

	return true;
}

/**
 * Write a BER INTEGER
 * @param s
 * @param value
 */

int ber_write_integer(STREAM* s, uint32 value)
{
	ber_write_universal_tag(s, BER_TAG_INTEGER, false);

	if (value <= 0xFF)
	{
		ber_write_length(s, 1);
		stream_write_uint8(s, value);
		return 2;
	}
	else if (value < 0xFF80)
	{
		ber_write_length(s, 2);
		stream_write_uint16_be(s, value);
		return 3;
	}
	else if (value < 0xFF8000)
	{
		ber_write_length(s, 3);
		stream_write_uint8(s, (value >> 16));
		stream_write_uint16_be(s, (value & 0xFFFF));
		return 4;
	}
	else if (value <= 0xFFFFFFFF)
	{
		ber_write_length(s, 4);
		stream_write_uint32_be(s, value);
		return 5;
	}

	return 0;
}

int ber_skip_integer(uint32 value)
{
	if (value <= 0xFF)
	{
		return _ber_skip_length(1) + 2;
	}
	else if (value <= 0xFFFF)
	{
		return _ber_skip_length(2) + 3;
	}
	else if (value <= 0xFFFFFFFF)
	{
		return _ber_skip_length(4) + 5;
	}

	return 0;
}

boolean ber_read_integer_length(STREAM* s, int* length)
{
	ber_read_universal_tag(s, BER_TAG_INTEGER, false);
	ber_read_length(s, length);
	return true;
}
