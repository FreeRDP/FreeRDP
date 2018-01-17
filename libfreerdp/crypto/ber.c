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

#include <stdio.h>
#include <winpr/crt.h>

#include <freerdp/log.h>
#include <freerdp/crypto/ber.h>

#define TAG FREERDP_TAG("crypto")

BOOL ber_read_length(wStream* s, size_t* length)
{
	BYTE byte;

	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, byte);

	if (byte & 0x80)
	{
		byte &= ~(0x80);

		if (Stream_GetRemainingLength(s) < byte)
			return FALSE;

		if (byte == 1)
			Stream_Read_UINT8(s, *length);
		else if (byte == 2)
			Stream_Read_UINT16_BE(s, *length);
		else
			return FALSE;
	}
	else
	{
		*length = byte;
	}

	return TRUE;
}

/**
 * Write BER length.
 * @param s stream
 * @param length length
 */

size_t ber_write_length(wStream* s, size_t length)
{
	if (length > 0xFF)
	{
		Stream_Write_UINT8(s, 0x80 ^ 2);
		Stream_Write_UINT16_BE(s, length);
		return 3;
	}

	if (length > 0x7F)
	{
		Stream_Write_UINT8(s, 0x80 ^ 1);
		Stream_Write_UINT8(s, length);
		return 2;
	}

	Stream_Write_UINT8(s, length);
	return 1;
}

size_t _ber_sizeof_length(size_t length)
{
	if (length > 0xFF)
		return 3;

	if (length > 0x7F)
		return 2;

	return 1;
}

/**
 * Read BER Universal tag.
 * @param s stream
 * @param tag BER universally-defined tag
 * @return
 */

BOOL ber_read_universal_tag(wStream* s, BYTE tag, BOOL pc)
{
	BYTE byte;

	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, byte);

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

size_t ber_write_universal_tag(wStream* s, BYTE tag, BOOL pc)
{
	Stream_Write_UINT8(s, (BER_CLASS_UNIV | BER_PC(pc)) | (BER_TAG_MASK & tag));
	return 1;
}

/**
 * Read BER Application tag.
 * @param s stream
 * @param tag BER application-defined tag
 * @param length length
 */

BOOL ber_read_application_tag(wStream* s, BYTE tag, size_t* length)
{
	BYTE byte;

	if (tag > 30)
	{
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, byte);

		if (byte != ((BER_CLASS_APPL | BER_CONSTRUCT) | BER_TAG_MASK))
			return FALSE;

		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, byte);

		if (byte != tag)
			return FALSE;

		return ber_read_length(s, length);
	}
	else
	{
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, byte);

		if (byte != ((BER_CLASS_APPL | BER_CONSTRUCT) | (BER_TAG_MASK & tag)))
			return FALSE;

		return ber_read_length(s, length);
	}

	return TRUE;
}

/**
 * Write BER Application tag.
 * @param s stream
 * @param tag BER application-defined tag
 * @param length length
 */

void ber_write_application_tag(wStream* s, BYTE tag, size_t length)
{
	if (tag > 30)
	{
		Stream_Write_UINT8(s, (BER_CLASS_APPL | BER_CONSTRUCT) | BER_TAG_MASK);
		Stream_Write_UINT8(s, tag);
		ber_write_length(s, length);
	}
	else
	{
		Stream_Write_UINT8(s, (BER_CLASS_APPL | BER_CONSTRUCT) | (BER_TAG_MASK & tag));
		ber_write_length(s, length);
	}
}

BOOL ber_read_contextual_tag(wStream* s, BYTE tag, size_t* length, BOOL pc)
{
	BYTE byte;

	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, byte);

	if (byte != ((BER_CLASS_CTXT | BER_PC(pc)) | (BER_TAG_MASK & tag)))
	{
		Stream_Rewind(s, 1);
		return FALSE;
	}

	return ber_read_length(s, length);
}

size_t ber_write_contextual_tag(wStream* s, BYTE tag, size_t length, BOOL pc)
{
	Stream_Write_UINT8(s, (BER_CLASS_CTXT | BER_PC(pc)) | (BER_TAG_MASK & tag));
	return 1 + ber_write_length(s, length);
}

size_t ber_sizeof_contextual_tag(size_t length)
{
	return 1 + _ber_sizeof_length(length);
}

BOOL ber_read_sequence_tag(wStream* s, size_t* length)
{
	BYTE byte;

	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, byte);

	if (byte != ((BER_CLASS_UNIV | BER_CONSTRUCT) | (BER_TAG_SEQUENCE_OF)))
		return FALSE;

	return ber_read_length(s, length);
}

/**
 * Write BER SEQUENCE tag.
 * @param s stream
 * @param length length
 */

size_t ber_write_sequence_tag(wStream* s, size_t length)
{
	Stream_Write_UINT8(s, (BER_CLASS_UNIV | BER_CONSTRUCT) | (BER_TAG_MASK & BER_TAG_SEQUENCE));
	return 1 + ber_write_length(s, length);
}

size_t ber_sizeof_sequence(size_t length)
{
	return 1 + _ber_sizeof_length(length) + length;
}

size_t ber_sizeof_sequence_tag(size_t length)
{
	return 1 + _ber_sizeof_length(length);
}

BOOL ber_read_enumerated(wStream* s, BYTE* enumerated, BYTE count)
{
	size_t length;

	if (!ber_read_universal_tag(s, BER_TAG_ENUMERATED, FALSE) ||
	    !ber_read_length(s, &length))
		return FALSE;

	if (length != 1 || Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, *enumerated);

	/* check that enumerated value falls within expected range */
	if (*enumerated + 1 > count)
		return FALSE;

	return TRUE;
}

void ber_write_enumerated(wStream* s, BYTE enumerated, BYTE count)
{
	ber_write_universal_tag(s, BER_TAG_ENUMERATED, FALSE);
	ber_write_length(s, 1);
	Stream_Write_UINT8(s, enumerated);
}

BOOL ber_read_bit_string(wStream* s, size_t* length, BYTE* padding)
{
	if (!ber_read_universal_tag(s, BER_TAG_BIT_STRING, FALSE) ||
	    !ber_read_length(s, length))
		return FALSE;

	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, *padding);
	return TRUE;
}

/**
 * Write a BER OCTET_STRING
 * @param s stream
 * @param oct_str octet string
 * @param length string length
 */

size_t ber_write_octet_string(wStream* s, const BYTE* oct_str, size_t length)
{
	size_t size = 0;
	size += ber_write_universal_tag(s, BER_TAG_OCTET_STRING, FALSE);
	size += ber_write_length(s, length);
	Stream_Write(s, oct_str, length);
	size += length;
	return size;
}

BOOL ber_read_octet_string_tag(wStream* s, size_t* length)
{
	return
	    ber_read_universal_tag(s, BER_TAG_OCTET_STRING, FALSE) &&
	    ber_read_length(s, length);
}

size_t ber_write_octet_string_tag(wStream* s, size_t length)
{
	ber_write_universal_tag(s, BER_TAG_OCTET_STRING, FALSE);
	ber_write_length(s, length);
	return 1 + _ber_sizeof_length(length);
}

size_t ber_sizeof_octet_string(size_t length)
{
	return 1 + _ber_sizeof_length(length) + length;
}

/**
 * Read a BER BOOLEAN
 * @param s
 * @param value
 */

BOOL ber_read_BOOL(wStream* s, BOOL* value)
{
	size_t length;
	BYTE v;

	if (!ber_read_universal_tag(s, BER_TAG_BOOLEAN, FALSE) ||
	    !ber_read_length(s, &length))
		return FALSE;

	if (length != 1 || Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, v);
	*value = (v ? TRUE : FALSE);
	return TRUE;
}

/**
 * Write a BER BOOLEAN
 * @param s
 * @param value
 */

void ber_write_BOOL(wStream* s, BOOL value)
{
	ber_write_universal_tag(s, BER_TAG_BOOLEAN, FALSE);
	ber_write_length(s, 1);
	Stream_Write_UINT8(s, (value == TRUE) ? 0xFF : 0);
}

BOOL ber_read_integer(wStream* s, UINT32* value)
{
	size_t length;

	if (!ber_read_universal_tag(s, BER_TAG_INTEGER, FALSE) ||
	    !ber_read_length(s, &length) ||
	    (Stream_GetRemainingLength(s) < length))
		return FALSE;

	if (value == NULL)
	{
		// even if we don't care the integer value, check the announced size
		return Stream_SafeSeek(s, length);
	}

	if (length == 1)
	{
		Stream_Read_UINT8(s, *value);
	}
	else if (length == 2)
	{
		Stream_Read_UINT16_BE(s, *value);
	}
	else if (length == 3)
	{
		BYTE byte;
		Stream_Read_UINT8(s, byte);
		Stream_Read_UINT16_BE(s, *value);
		*value += (byte << 16);
	}
	else if (length == 4)
	{
		Stream_Read_UINT32_BE(s, *value);
	}
	else if (length == 8)
	{
		WLog_ERR(TAG,  "should implement reading an 8 bytes integer");
		return FALSE;
	}
	else
	{
		WLog_ERR(TAG,  "should implement reading an integer with length=%d", length);
		return FALSE;
	}

	return TRUE;
}

/**
 * Write a BER INTEGER
 * @param s
 * @param value
 */

size_t ber_write_integer(wStream* s, UINT32 value)
{
	if (value <  0x80)
	{
		ber_write_universal_tag(s, BER_TAG_INTEGER, FALSE);
		ber_write_length(s, 1);
		Stream_Write_UINT8(s, value);
		return 3;
	}
	else if (value <  0x8000)
	{
		ber_write_universal_tag(s, BER_TAG_INTEGER, FALSE);
		ber_write_length(s, 2);
		Stream_Write_UINT16_BE(s, value);
		return 4;
	}
	else if (value <  0x800000)
	{
		ber_write_universal_tag(s, BER_TAG_INTEGER, FALSE);
		ber_write_length(s, 3);
		Stream_Write_UINT8(s, (value >> 16));
		Stream_Write_UINT16_BE(s, (value & 0xFFFF));
		return 5;
	}
	else if (value <  0x80000000)
	{
		ber_write_universal_tag(s, BER_TAG_INTEGER, FALSE);
		ber_write_length(s, 4);
		Stream_Write_UINT32_BE(s, value);
		return 6;
	}
	else
	{
		/* treat as signed integer i.e. NT/HRESULT error codes */
		ber_write_universal_tag(s, BER_TAG_INTEGER, FALSE);
		ber_write_length(s, 4);
		Stream_Write_UINT32_BE(s, value);
		return 6;
	}

	return 0;
}

size_t ber_sizeof_integer(UINT32 value)
{
	if (value < 0x80)
	{
		return 3;
	}
	else if (value < 0x8000)
	{
		return 4;
	}
	else if (value < 0x800000)
	{
		return 5;
	}
	else if (value < 0x80000000)
	{
		return 6;
	}
	else
	{
		/* treat as signed integer i.e. NT/HRESULT error codes */
		return 6;
	}

	return 0;
}

BOOL ber_read_integer_length(wStream* s, size_t* length)
{
	return
	    ber_read_universal_tag(s, BER_TAG_INTEGER, FALSE) &&
	    ber_read_length(s, length);
}
