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

/**
 * Write BER length.
 * @param s stream
 * @param length length
 */

void ber_write_length(STREAM* s, int length)
{
	if (length > 0x7F)
	{
		stream_write_uint8(s, 0x82);
		stream_write_uint16_be(s, length);
	}
	else
	{
		stream_write_uint8(s, length);
	}
}

/**
 * Write BER Universal tag.
 * @param s stream
 * @param tag BER universally-defined tag
 * @param length length
 */

void ber_write_universal_tag(STREAM* s, uint8 tag, int length)
{
	stream_write_uint8(s, (BER_CLASS_UNIV | BER_PRIMITIVE) | (BER_TAG_MASK & tag));
	ber_write_length(s, length);
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

/**
 * Write a BER OCTET_STRING
 * @param s stream
 * @param oct_str octet string
 * @param length string length
 */

void ber_write_octet_string(STREAM* s, uint8* oct_str, int length)
{
	ber_write_universal_tag(s, BER_TAG_OCTET_STRING);
	stream_write(s, oct_str, length);
}

/**
 * Write a BER BOOLEAN
 * @param s
 * @param value
 */

void ber_write_boolean(STREAM* s, boolean value)
{
	ber_write_universal_tag(s, BER_TAG_BOOLEAN);
	stream_write_uint8(s, (value == True) ? 0xFF : 0);
}

/**
 * Write a BER INTEGER
 * @param s
 * @param value
 */

void ber_write_integer(STREAM* s, uint32 value)
{
	ber_write_universal_tag(s, BER_TAG_INTEGER);

	if (value <= 0xFF)
	{
		stream_write_uint8(s, value);
	}
	else if (value <= 0xFFFF)
	{
		stream_write_uint16_be(s, value);
	}
	else if (value <= 0xFFFFFFFF)
	{
		stream_write_uint32_be(s, value);
	}
}
