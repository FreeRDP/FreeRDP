/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ASN.1 Basic Encoding Rules (BER)
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

#include <freerdp/crypto/ber.h>

void ber_read_length(STREAM* s, int* length)
{
	BYTE byte;

	stream_read_BYTE(s, byte);

	if (byte & 0x80)
	{
		byte &= ~(0x80);

		if (byte == 1)
			stream_read_BYTE(s, *length);
		if (byte == 2)
			stream_read_UINT16_be(s, *length);
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
		stream_write_BYTE(s, 0x82);
		stream_write_UINT16_be(s, length);
		return 3;
	}
	else
	{
		stream_write_BYTE(s, length);
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

BOOL ber_read_universal_tag(STREAM* s, BYTE tag, BOOL pc)
{
	BYTE byte;

	stream_read_BYTE(s, byte);

	if (byte != (BER_CLASS_UNIV | BER_PC(pc) | (BER_TAG_MASK & tag)))
		return FALSE;

	return TRUE;
}

/**
 * Write BER Universal tag.
 * @param s stream
 * @param tag BER universally-defined tag
 * @param pc primitive (FALSE) or constructed (TRUE)
 */

void ber_write_universal_tag(STREAM* s, BYTE tag, BOOL pc)
{
	stream_write_BYTE(s, (BER_CLASS_UNIV | BER_PC(pc)) | (BER_TAG_MASK & tag));
}

/**
 * Read BER Application tag.
 * @param s stream
 * @param tag BER application-defined tag
 * @param length length
 */

BOOL ber_read_application_tag(STREAM* s, BYTE tag, int* length)
{
	BYTE byte;

	if (tag > 30)
	{
		stream_read_BYTE(s, byte);

		if (byte != ((BER_CLASS_APPL | BER_CONSTRUCT) | BER_TAG_MASK))
			return FALSE;

		stream_read_BYTE(s, byte);

		if (byte != tag)
			return FALSE;

		ber_read_length(s, length);
	}
	else
	{
		stream_read_BYTE(s, byte);

		if (byte != ((BER_CLASS_APPL | BER_CONSTRUCT) | (BER_TAG_MASK & tag)))
			return FALSE;

		ber_read_length(s, length);
	}

	return TRUE;
}

/**
 * Write BER Application tag.
 * @param s stream
 * @param tag BER application-defined tag
 * @param length length
 */

void ber_write_application_tag(STREAM* s, BYTE tag, int length)
{
	if (tag > 30)
	{
		stream_write_BYTE(s, (BER_CLASS_APPL | BER_CONSTRUCT) | BER_TAG_MASK);
		stream_write_BYTE(s, tag);
		ber_write_length(s, length);
	}
	else
	{
		stream_write_BYTE(s, (BER_CLASS_APPL | BER_CONSTRUCT) | (BER_TAG_MASK & tag));
		ber_write_length(s, length);
	}
}

BOOL ber_read_contextual_tag(STREAM* s, BYTE tag, int* length, BOOL pc)
{
	BYTE byte;

	stream_read_BYTE(s, byte);

	if (byte != ((BER_CLASS_CTXT | BER_PC(pc)) | (BER_TAG_MASK & tag)))
	{
		stream_rewind(s, 1);
		return FALSE;
	}

	ber_read_length(s, length);

	return TRUE;
}

int ber_write_contextual_tag(STREAM* s, BYTE tag, int length, BOOL pc)
{
	stream_write_BYTE(s, (BER_CLASS_CTXT | BER_PC(pc)) | (BER_TAG_MASK & tag));
	return ber_write_length(s, length) + 1;
}

int ber_skip_contextual_tag(int length)
{
	return _ber_skip_length(length) + 1;
}

BOOL ber_read_sequence_tag(STREAM* s, int* length)
{
	BYTE byte;

	stream_read_BYTE(s, byte);

	if (byte != ((BER_CLASS_UNIV | BER_CONSTRUCT) | (BER_TAG_SEQUENCE_OF)))
		return FALSE;

	ber_read_length(s, length);

	return TRUE;
}

/**
 * Write BER SEQUENCE tag.
 * @param s stream
 * @param length length
 */

int ber_write_sequence_tag(STREAM* s, int length)
{
	stream_write_BYTE(s, (BER_CLASS_UNIV | BER_CONSTRUCT) | (BER_TAG_MASK & BER_TAG_SEQUENCE));
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

BOOL ber_read_enumerated(STREAM* s, BYTE* enumerated, BYTE count)
{
	int length;

	ber_read_universal_tag(s, BER_TAG_ENUMERATED, FALSE);
	ber_read_length(s, &length);

	if (length == 1)
		stream_read_BYTE(s, *enumerated);
	else
		return FALSE;

	/* check that enumerated value falls within expected range */
	if (*enumerated + 1 > count)
		return FALSE;

	return TRUE;
}

void ber_write_enumerated(STREAM* s, BYTE enumerated, BYTE count)
{
	ber_write_universal_tag(s, BER_TAG_ENUMERATED, FALSE);
	ber_write_length(s, 1);
	stream_write_BYTE(s, enumerated);
}

BOOL ber_read_bit_string(STREAM* s, int* length, BYTE* padding)
{
	ber_read_universal_tag(s, BER_TAG_BIT_STRING, FALSE);
	ber_read_length(s, length);
	stream_read_BYTE(s, *padding);

	return TRUE;
}

/**
 * Write a BER OCTET_STRING
 * @param s stream
 * @param oct_str octet string
 * @param length string length
 */

void ber_write_octet_string(STREAM* s, const BYTE* oct_str, int length)
{
	ber_write_universal_tag(s, BER_TAG_OCTET_STRING, FALSE);
	ber_write_length(s, length);
	stream_write(s, oct_str, length);
}

BOOL ber_read_octet_string_tag(STREAM* s, int* length)
{
	ber_read_universal_tag(s, BER_TAG_OCTET_STRING, FALSE);
	ber_read_length(s, length);
	return TRUE;
}

int ber_write_octet_string_tag(STREAM* s, int length)
{
	ber_write_universal_tag(s, BER_TAG_OCTET_STRING, FALSE);
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

BOOL ber_read_BOOL(STREAM* s, BOOL* value)
{
	int length;
	BYTE v;

	if (!ber_read_universal_tag(s, BER_TAG_BOOLEAN, FALSE))
		return FALSE;
	ber_read_length(s, &length);
	if (length != 1)
		return FALSE;
	stream_read_BYTE(s, v);
	*value = (v ? TRUE : FALSE);
	return TRUE;
}

/**
 * Write a BER BOOLEAN
 * @param s
 * @param value
 */

void ber_write_BOOL(STREAM* s, BOOL value)
{
	ber_write_universal_tag(s, BER_TAG_BOOLEAN, FALSE);
	ber_write_length(s, 1);
	stream_write_BYTE(s, (value == TRUE) ? 0xFF : 0);
}

BOOL ber_read_integer(STREAM* s, UINT32* value)
{
	int length;

	ber_read_universal_tag(s, BER_TAG_INTEGER, FALSE);
	ber_read_length(s, &length);

	if (value == NULL)
	{
		stream_seek(s, length);
		return TRUE;
	}

	if (length == 1)
		stream_read_BYTE(s, *value);
	else if (length == 2)
		stream_read_UINT16_be(s, *value);
	else if (length == 3)
	{
		BYTE byte;
		stream_read_BYTE(s, byte);
		stream_read_UINT16_be(s, *value);
		*value += (byte << 16);
	}
	else if (length == 4)
		stream_read_UINT32_be(s, *value);
	else
		return FALSE;

	return TRUE;
}

/**
 * Write a BER INTEGER
 * @param s
 * @param value
 */

int ber_write_integer(STREAM* s, UINT32 value)
{
	ber_write_universal_tag(s, BER_TAG_INTEGER, FALSE);

	if (value <= 0xFF)
	{
		ber_write_length(s, 1);
		stream_write_BYTE(s, value);
		return 2;
	}
	else if (value < 0xFF80)
	{
		ber_write_length(s, 2);
		stream_write_UINT16_be(s, value);
		return 3;
	}
	else if (value < 0xFF8000)
	{
		ber_write_length(s, 3);
		stream_write_BYTE(s, (value >> 16));
		stream_write_UINT16_be(s, (value & 0xFFFF));
		return 4;
	}
	else if (value <= 0xFFFFFFFF)
	{
		ber_write_length(s, 4);
		stream_write_UINT32_be(s, value);
		return 5;
	}

	return 0;
}

int ber_skip_integer(UINT32 value)
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

BOOL ber_read_integer_length(STREAM* s, int* length)
{
	ber_read_universal_tag(s, BER_TAG_INTEGER, FALSE);
	ber_read_length(s, length);
	return TRUE;
}
