/**
 * FreeRDP: A Remote Desktop Protocol Client
 * ASN.1 Encoding Rules (BER/DER common functions)
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Modified by Jiten Pathy
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

#include <freerdp/crypto/er.h>
#include <freerdp/crypto/ber.h>
#include <freerdp/crypto/der.h>

void er_read_length(STREAM* s, int* length)
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
 * Write er length.
 * @param s stream
 * @param length length
 */

int er_write_length(STREAM* s, int length, boolean flag)
{
	if (flag)
		return der_write_length(s, length);
	else
		return ber_write_length(s, length);
}

int _er_skip_length(int length)
{
	if (length > 0x7F)
		return 3;
	else
		return 1;
}

int er_get_content_length(int length)
{
	if (length - 1 > 0x7F)
		return length - 4;
	else
		return length - 2;
}

/**
 * Read er Universal tag.
 * @param s stream
 * @param tag er universally-defined tag
 * @return
 */

boolean er_read_universal_tag(STREAM* s, uint8 tag, boolean pc)
{
	uint8 byte;

	stream_read_uint8(s, byte);

	if (byte != (ER_CLASS_UNIV | ER_PC(pc) | (ER_TAG_MASK & tag)))
		return false;

	return true;
}

/**
 * Write er Universal tag.
 * @param s stream
 * @param tag er universally-defined tag
 * @param pc primitive (false) or constructed (true)
 */

void er_write_universal_tag(STREAM* s, uint8 tag, boolean pc)
{
	stream_write_uint8(s, (ER_CLASS_UNIV | ER_PC(pc)) | (ER_TAG_MASK & tag));
}

/**
 * Read er Application tag.
 * @param s stream
 * @param tag er application-defined tag
 * @param length length
 */

boolean er_read_application_tag(STREAM* s, uint8 tag, int* length)
{
	uint8 byte;

	if (tag > 30)
	{
		stream_read_uint8(s, byte);

		if (byte != ((ER_CLASS_APPL | ER_CONSTRUCT) | ER_TAG_MASK))
			return false;

		stream_read_uint8(s, byte);

		if (byte != tag)
			return false;

		er_read_length(s, length);
	}
	else
	{
		stream_read_uint8(s, byte);

		if (byte != ((ER_CLASS_APPL | ER_CONSTRUCT) | (ER_TAG_MASK & tag)))
			return false;

		er_read_length(s, length);
	}

	return true;
}

/**
 * Write er Application tag.
 * @param s stream
 * @param tag er application-defined tag
 * @param length length
 */

void er_write_application_tag(STREAM* s, uint8 tag, int length, boolean flag)
{
	if (tag > 30)
	{
		stream_write_uint8(s, (ER_CLASS_APPL | ER_CONSTRUCT) | ER_TAG_MASK);
		stream_write_uint8(s, tag);
		er_write_length(s, length, flag);
	}
	else
	{
		stream_write_uint8(s, (ER_CLASS_APPL | ER_CONSTRUCT) | (ER_TAG_MASK & tag));
		er_write_length(s, length, flag);
	}
}

boolean er_read_contextual_tag(STREAM* s, uint8 tag, int* length, boolean pc)
{
	uint8 byte;

	stream_read_uint8(s, byte);

	if (byte != ((ER_CLASS_CTXT | ER_PC(pc)) | (ER_TAG_MASK & tag)))
	{
		stream_rewind(s, 1);
		return false;
	}

	er_read_length(s, length);

	return true;
}

int er_write_contextual_tag(STREAM* s, uint8 tag, int length, boolean pc, boolean flag)
{
	stream_write_uint8(s, (ER_CLASS_CTXT | ER_PC(pc)) | (ER_TAG_MASK & tag));
	return er_write_length(s, length, flag) + 1;
}

int er_skip_contextual_tag(int length)
{
	return _er_skip_length(length) + 1;
}

boolean er_read_sequence_tag(STREAM* s, int* length)
{
	uint8 byte;

	stream_read_uint8(s, byte);

	if (byte != ((ER_CLASS_UNIV | ER_CONSTRUCT) | (ER_TAG_SEQUENCE_OF)))
		return false;

	er_read_length(s, length);

	return true;
}

/**
 * Write er SEQUENCE tag.
 * @param s stream
 * @param length length
 */

int er_write_sequence_tag(STREAM* s, int length, boolean flag)
{
	stream_write_uint8(s, (ER_CLASS_UNIV | ER_CONSTRUCT) | (ER_TAG_MASK & ER_TAG_SEQUENCE));
	return er_write_length(s, length, flag) + 1;
}

int er_skip_sequence(int length)
{
	return 1 + _er_skip_length(length) + length;
}

int er_skip_sequence_tag(int length)
{
	return 1 + _er_skip_length(length);
}

boolean er_read_enumerated(STREAM* s, uint8* enumerated, uint8 count)
{
	int length;

	er_read_universal_tag(s, ER_TAG_ENUMERATED, false);
	er_read_length(s, &length);

	if (length == 1)
		stream_read_uint8(s, *enumerated);
	else
		return false;

	/* check that enumerated value falls within expected range */
	if (*enumerated + 1 > count)
		return false;

	return true;
}

void er_write_enumerated(STREAM* s, uint8 enumerated, uint8 count, boolean flag)
{
	er_write_universal_tag(s, ER_TAG_ENUMERATED, false);
	er_write_length(s, 1, flag);
	stream_write_uint8(s, enumerated);
}

boolean er_read_bit_string(STREAM* s, int* length, uint8* padding)
{
	er_read_universal_tag(s, ER_TAG_BIT_STRING, false);
	er_read_length(s, length);
	stream_read_uint8(s, *padding);

	return true;
}

boolean er_write_bit_string_tag(STREAM* s, uint32 length, uint8 padding, boolean flag)
{
	er_write_universal_tag(s, ER_TAG_BIT_STRING, false);
	er_write_length(s, length, flag);
	stream_write_uint8(s, padding);
	return true;
}

boolean er_read_octet_string(STREAM* s, int* length)
{
	if(!er_read_universal_tag(s, ER_TAG_OCTET_STRING, false))
		return false;
	er_read_length(s, length);

	return true;
}

/**
 * Write a er OCTET_STRING
 * @param s stream
 * @param oct_str octet string
 * @param length string length
 */

void er_write_octet_string(STREAM* s, uint8* oct_str, int length, boolean flag)
{
	er_write_universal_tag(s, ER_TAG_OCTET_STRING, false);
	er_write_length(s, length, flag);
	stream_write(s, oct_str, length);
}

int er_write_octet_string_tag(STREAM* s, int length, boolean flag)
{
	er_write_universal_tag(s, ER_TAG_OCTET_STRING, false);
	er_write_length(s, length, flag);
	return 1 + _er_skip_length(length);
}

int er_skip_octet_string(int length)
{
	return 1 + _er_skip_length(length) + length;
}

/**
 * Read a er BOOLEAN
 * @param s
 * @param value
 */

boolean er_read_boolean(STREAM* s, boolean* value)
{
	int length;
	uint8 v;

	if (!er_read_universal_tag(s, ER_TAG_BOOLEAN, false))
		return false;
	er_read_length(s, &length);
	if (length != 1)
		return false;
	stream_read_uint8(s, v);
	*value = (v ? true : false);
	return true;
}

/**
 * Write a er BOOLEAN
 * @param s
 * @param value
 */

void er_write_boolean(STREAM* s, boolean value)
{
	er_write_universal_tag(s, ER_TAG_BOOLEAN, false);
	er_write_length(s, 1, false);
	stream_write_uint8(s, (value == true) ? 0xFF : 0);
}

boolean er_read_integer(STREAM* s, uint32* value)
{
	int length;

	er_read_universal_tag(s, ER_TAG_INTEGER, false);
	er_read_length(s, &length);

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
 * Write a er INTEGER
 * @param s
 * @param value
 */

int er_write_integer(STREAM* s, sint32 value)
{
	er_write_universal_tag(s, ER_TAG_INTEGER, false);

	if (value <= 127 && value >= -128)
	{
		er_write_length(s, 1, false);
		stream_write_uint8(s, value);
		return 2;
	}
	else if (value <= 32767 && value >= -32768)
	{
		er_write_length(s, 2, false);
		stream_write_uint16_be(s, value);
		return 3;
	}
	else
	{
		er_write_length(s, 4, false);
		stream_write_uint32_be(s, value);
		return 5;
	}

	return 0;
}

int er_skip_integer(sint32 value)
{
	if (value <= 127 && value >= -128)
	{
		return _er_skip_length(1) + 2;
	}
	else if (value <= 32767 && value >= -32768)
	{
		return _er_skip_length(2) + 3;
	}
	else
	{
		return _er_skip_length(4) + 5;
	}

	return 0;
}

boolean er_read_integer_length(STREAM* s, int* length)
{
	er_read_universal_tag(s, ER_TAG_INTEGER, false);
	er_read_length(s, length);
	return true;
}
