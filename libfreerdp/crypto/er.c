/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include <freerdp/crypto/er.h>
#include <freerdp/crypto/ber.h>
#include <freerdp/crypto/der.h>

void er_read_length(wStream* s, int* length)
{
	BYTE byte;

	Stream_Read_UINT8(s, byte);

	if (byte & 0x80)
	{
		byte &= ~(0x80);

		if (byte == 1)
			Stream_Read_UINT8(s, *length);
		if (byte == 2)
			Stream_Read_UINT16_BE(s, *length);
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

int er_write_length(wStream* s, int length, BOOL flag)
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

BOOL er_read_universal_tag(wStream* s, BYTE tag, BOOL pc)
{
	BYTE byte;

	Stream_Read_UINT8(s, byte);

	if (byte != (ER_CLASS_UNIV | ER_PC(pc) | (ER_TAG_MASK & tag)))
		return FALSE;

	return TRUE;
}

/**
 * Write er Universal tag.
 * @param s stream
 * @param tag er universally-defined tag
 * @param pc primitive (FALSE) or constructed (TRUE)
 */

void er_write_universal_tag(wStream* s, BYTE tag, BOOL pc)
{
	Stream_Write_UINT8(s, (ER_CLASS_UNIV | ER_PC(pc)) | (ER_TAG_MASK & tag));
}

/**
 * Read er Application tag.
 * @param s stream
 * @param tag er application-defined tag
 * @param length length
 */

BOOL er_read_application_tag(wStream* s, BYTE tag, int* length)
{
	BYTE byte;

	if (tag > 30)
	{
		Stream_Read_UINT8(s, byte);

		if (byte != ((ER_CLASS_APPL | ER_CONSTRUCT) | ER_TAG_MASK))
			return FALSE;

		Stream_Read_UINT8(s, byte);

		if (byte != tag)
			return FALSE;

		er_read_length(s, length);
	}
	else
	{
		Stream_Read_UINT8(s, byte);

		if (byte != ((ER_CLASS_APPL | ER_CONSTRUCT) | (ER_TAG_MASK & tag)))
			return FALSE;

		er_read_length(s, length);
	}

	return TRUE;
}

/**
 * Write er Application tag.
 * @param s stream
 * @param tag er application-defined tag
 * @param length length
 */

void er_write_application_tag(wStream* s, BYTE tag, int length, BOOL flag)
{
	if (tag > 30)
	{
		Stream_Write_UINT8(s, (ER_CLASS_APPL | ER_CONSTRUCT) | ER_TAG_MASK);
		Stream_Write_UINT8(s, tag);
		er_write_length(s, length, flag);
	}
	else
	{
		Stream_Write_UINT8(s, (ER_CLASS_APPL | ER_CONSTRUCT) | (ER_TAG_MASK & tag));
		er_write_length(s, length, flag);
	}
}

BOOL er_read_contextual_tag(wStream* s, BYTE tag, int* length, BOOL pc)
{
	BYTE byte;

	Stream_Read_UINT8(s, byte);

	if (byte != ((ER_CLASS_CTXT | ER_PC(pc)) | (ER_TAG_MASK & tag)))
	{
		Stream_Rewind(s, 1);
		return FALSE;
	}

	er_read_length(s, length);

	return TRUE;
}

int er_write_contextual_tag(wStream* s, BYTE tag, int length, BOOL pc, BOOL flag)
{
	Stream_Write_UINT8(s, (ER_CLASS_CTXT | ER_PC(pc)) | (ER_TAG_MASK & tag));
	return er_write_length(s, length, flag) + 1;
}

int er_skip_contextual_tag(int length)
{
	return _er_skip_length(length) + 1;
}

BOOL er_read_sequence_tag(wStream* s, int* length)
{
	BYTE byte;

	Stream_Read_UINT8(s, byte);

	if (byte != ((ER_CLASS_UNIV | ER_CONSTRUCT) | (ER_TAG_SEQUENCE_OF)))
		return FALSE;

	er_read_length(s, length);

	return TRUE;
}

/**
 * Write er SEQUENCE tag.
 * @param s stream
 * @param length length
 */

int er_write_sequence_tag(wStream* s, int length, BOOL flag)
{
	Stream_Write_UINT8(s, (ER_CLASS_UNIV | ER_CONSTRUCT) | (ER_TAG_MASK & ER_TAG_SEQUENCE));
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

BOOL er_read_enumerated(wStream* s, BYTE* enumerated, BYTE count)
{
	int length;

	er_read_universal_tag(s, ER_TAG_ENUMERATED, FALSE);
	er_read_length(s, &length);

	if (length == 1)
		Stream_Read_UINT8(s, *enumerated);
	else
		return FALSE;

	/* check that enumerated value falls within expected range */
	if (*enumerated + 1 > count)
		return FALSE;

	return TRUE;
}

void er_write_enumerated(wStream* s, BYTE enumerated, BYTE count, BOOL flag)
{
	er_write_universal_tag(s, ER_TAG_ENUMERATED, FALSE);
	er_write_length(s, 1, flag);
	Stream_Write_UINT8(s, enumerated);
}

BOOL er_read_bit_string(wStream* s, int* length, BYTE* padding)
{
	er_read_universal_tag(s, ER_TAG_BIT_STRING, FALSE);
	er_read_length(s, length);
	Stream_Read_UINT8(s, *padding);

	return TRUE;
}

BOOL er_write_bit_string_tag(wStream* s, UINT32 length, BYTE padding, BOOL flag)
{
	er_write_universal_tag(s, ER_TAG_BIT_STRING, FALSE);
	er_write_length(s, length, flag);
	Stream_Write_UINT8(s, padding);
	return TRUE;
}

BOOL er_read_octet_string(wStream* s, int* length)
{
	if(!er_read_universal_tag(s, ER_TAG_OCTET_STRING, FALSE))
		return FALSE;
	er_read_length(s, length);

	return TRUE;
}

/**
 * Write a er OCTET_STRING
 * @param s stream
 * @param oct_str octet string
 * @param length string length
 */

void er_write_octet_string(wStream* s, BYTE* oct_str, int length, BOOL flag)
{
	er_write_universal_tag(s, ER_TAG_OCTET_STRING, FALSE);
	er_write_length(s, length, flag);
	Stream_Write(s, oct_str, length);
}

int er_write_octet_string_tag(wStream* s, int length, BOOL flag)
{
	er_write_universal_tag(s, ER_TAG_OCTET_STRING, FALSE);
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

BOOL er_read_BOOL(wStream* s, BOOL* value)
{
	int length;
	BYTE v;

	if (!er_read_universal_tag(s, ER_TAG_BOOLEAN, FALSE))
		return FALSE;
	er_read_length(s, &length);
	if (length != 1)
		return FALSE;
	Stream_Read_UINT8(s, v);
	*value = (v ? TRUE : FALSE);
	return TRUE;
}

/**
 * Write a er BOOLEAN
 * @param s
 * @param value
 */

void er_write_BOOL(wStream* s, BOOL value)
{
	er_write_universal_tag(s, ER_TAG_BOOLEAN, FALSE);
	er_write_length(s, 1, FALSE);
	Stream_Write_UINT8(s, (value == TRUE) ? 0xFF : 0);
}

BOOL er_read_integer(wStream* s, UINT32* value)
{
	int length;

	er_read_universal_tag(s, ER_TAG_INTEGER, FALSE);
	er_read_length(s, &length);

	if (value == NULL)
	{
		Stream_Seek(s, length);
		return TRUE;
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
	else
	{
		return FALSE;
	}

	return TRUE;
}

/**
 * Write a er INTEGER
 * @param s
 * @param value
 */

int er_write_integer(wStream* s, INT32 value)
{
	er_write_universal_tag(s, ER_TAG_INTEGER, FALSE);

	if (value <= 127 && value >= -128)
	{
		er_write_length(s, 1, FALSE);
		Stream_Write_UINT8(s, value);
		return 2;
	}
	else if (value <= 32767 && value >= -32768)
	{
		er_write_length(s, 2, FALSE);
		Stream_Write_UINT16_BE(s, value);
		return 3;
	}
	else
	{
		er_write_length(s, 4, FALSE);
		Stream_Write_UINT32_BE(s, value);
		return 5;
	}

	return 0;
}

int er_skip_integer(INT32 value)
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

BOOL er_read_integer_length(wStream* s, int* length)
{
	er_read_universal_tag(s, ER_TAG_INTEGER, FALSE);
	er_read_length(s, length);
	return TRUE;
}
