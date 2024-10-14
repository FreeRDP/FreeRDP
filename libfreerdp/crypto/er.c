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

#include <stdint.h>

#include <freerdp/config.h>

#include <winpr/crt.h>

#include <freerdp/crypto/er.h>
#include <freerdp/crypto/ber.h>
#include <freerdp/crypto/der.h>

void freerdp_er_read_length(wStream* s, size_t* length)
{
	BYTE byte = 0;

	Stream_Read_UINT8(s, byte);

	if (!length)
		return;

	*length = 0;
	if (!s)
		return;

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

size_t freerdp_er_write_length(wStream* s, size_t length, BOOL flag)
{
	if (flag)
		return freerdp_der_write_length(s, length);
	else
		return freerdp_ber_write_length(s, length);
}

size_t freerdp_er_skip_length(size_t length)
{
	if (length > 0x7F)
		return 3;
	else
		return 1;
}

size_t freerdp_er_get_content_length(size_t length)
{
	if (length - 1 > 0x7F)
		return length - 4;
	else
		return length - 2;
}

BOOL freerdp_er_read_universal_tag(wStream* s, BYTE tag, BOOL pc)
{
	BYTE byte = 0;

	Stream_Read_UINT8(s, byte);

	if (byte != (FREERDP_ER_CLASS_UNIV | FREERDP_ER_PC(pc) | (FREERDP_ER_TAG_MASK & tag)))
		return FALSE;

	return TRUE;
}

void freerdp_er_write_universal_tag(wStream* s, BYTE tag, BOOL pc)
{
	Stream_Write_UINT8(s,
	                   (FREERDP_ER_CLASS_UNIV | FREERDP_ER_PC(pc)) | (FREERDP_ER_TAG_MASK & tag));
}

BOOL freerdp_er_read_application_tag(wStream* s, BYTE tag, size_t* length)
{
	BYTE byte = 0;

	if (tag > 30)
	{
		Stream_Read_UINT8(s, byte);

		if (byte != ((FREERDP_ER_CLASS_APPL | FREERDP_ER_CONSTRUCT) | FREERDP_ER_TAG_MASK))
			return FALSE;

		Stream_Read_UINT8(s, byte);

		if (byte != tag)
			return FALSE;

		freerdp_er_read_length(s, length);
	}
	else
	{
		Stream_Read_UINT8(s, byte);

		if (byte != ((FREERDP_ER_CLASS_APPL | FREERDP_ER_CONSTRUCT) | (FREERDP_ER_TAG_MASK & tag)))
			return FALSE;

		freerdp_er_read_length(s, length);
	}

	return TRUE;
}

void freerdp_er_write_application_tag(wStream* s, BYTE tag, size_t length, BOOL flag)
{
	if (tag > 30)
	{
		Stream_Write_UINT8(s, (FREERDP_ER_CLASS_APPL | FREERDP_ER_CONSTRUCT) | FREERDP_ER_TAG_MASK);
		Stream_Write_UINT8(s, tag);
		freerdp_er_write_length(s, length, flag);
	}
	else
	{
		Stream_Write_UINT8(s, (FREERDP_ER_CLASS_APPL | FREERDP_ER_CONSTRUCT) |
		                          (FREERDP_ER_TAG_MASK & tag));
		freerdp_er_write_length(s, length, flag);
	}
}

BOOL freerdp_er_read_contextual_tag(wStream* s, BYTE tag, size_t* length, BOOL pc)
{
	BYTE byte = 0;

	Stream_Read_UINT8(s, byte);

	if (byte != ((FREERDP_ER_CLASS_CTXT | FREERDP_ER_PC(pc)) | (FREERDP_ER_TAG_MASK & tag)))
	{
		Stream_Rewind(s, 1);
		return FALSE;
	}

	freerdp_er_read_length(s, length);

	return TRUE;
}

size_t freerdp_er_write_contextual_tag(wStream* s, BYTE tag, size_t length, BOOL pc, BOOL flag)
{
	Stream_Write_UINT8(s,
	                   (FREERDP_ER_CLASS_CTXT | FREERDP_ER_PC(pc)) | (FREERDP_ER_TAG_MASK & tag));
	return freerdp_er_write_length(s, length, flag) + 1;
}

size_t freerdp_er_skip_contextual_tag(size_t length)
{
	return freerdp_er_skip_length(length) + 1;
}

BOOL freerdp_er_read_sequence_tag(wStream* s, size_t* length)
{
	BYTE byte = 0;

	Stream_Read_UINT8(s, byte);

	if (byte != ((FREERDP_ER_CLASS_UNIV | FREERDP_ER_CONSTRUCT) | (FREERDP_ER_TAG_SEQUENCE_OF)))
		return FALSE;

	freerdp_er_read_length(s, length);

	return TRUE;
}

size_t freerdp_er_write_sequence_tag(wStream* s, size_t length, BOOL flag)
{
	Stream_Write_UINT8(s, (FREERDP_ER_CLASS_UNIV | FREERDP_ER_CONSTRUCT) |
	                          (FREERDP_ER_TAG_MASK & FREERDP_ER_TAG_SEQUENCE));
	return freerdp_er_write_length(s, length, flag) + 1;
}

size_t freerdp_er_skip_sequence(size_t length)
{
	return 1 + freerdp_er_skip_length(length) + length;
}

size_t freerdp_er_skip_sequence_tag(size_t length)
{
	return 1 + freerdp_er_skip_length(length);
}

BOOL freerdp_er_read_enumerated(wStream* s, BYTE* enumerated, BYTE count)
{
	size_t length = 0;

	freerdp_er_read_universal_tag(s, FREERDP_ER_TAG_ENUMERATED, FALSE);
	freerdp_er_read_length(s, &length);

	if (length == 1)
		Stream_Read_UINT8(s, *enumerated);
	else
		return FALSE;

	/* check that enumerated value falls within expected range */
	if (*enumerated + 1 > count)
		return FALSE;

	return TRUE;
}

void freerdp_er_write_enumerated(wStream* s, BYTE enumerated, BYTE count, BOOL flag)
{
	freerdp_er_write_universal_tag(s, FREERDP_ER_TAG_ENUMERATED, FALSE);
	freerdp_er_write_length(s, 1, flag);
	Stream_Write_UINT8(s, enumerated);
}

BOOL freerdp_er_read_bit_string(wStream* s, size_t* length, BYTE* padding)
{
	freerdp_er_read_universal_tag(s, FREERDP_ER_TAG_BIT_STRING, FALSE);
	freerdp_er_read_length(s, length);
	Stream_Read_UINT8(s, *padding);

	return TRUE;
}

BOOL freerdp_er_write_bit_string_tag(wStream* s, UINT32 length, BYTE padding, BOOL flag)
{
	freerdp_er_write_universal_tag(s, FREERDP_ER_TAG_BIT_STRING, FALSE);
	freerdp_er_write_length(s, length, flag);
	Stream_Write_UINT8(s, padding);
	return TRUE;
}

BOOL freerdp_er_read_octet_string(wStream* s, size_t* length)
{
	if (!freerdp_er_read_universal_tag(s, FREERDP_ER_TAG_OCTET_STRING, FALSE))
		return FALSE;
	freerdp_er_read_length(s, length);

	return TRUE;
}

void freerdp_er_write_octet_string(wStream* s, const BYTE* oct_str, size_t length, BOOL flag)
{
	freerdp_er_write_universal_tag(s, FREERDP_ER_TAG_OCTET_STRING, FALSE);
	freerdp_er_write_length(s, length, flag);
	Stream_Write(s, oct_str, length);
}

size_t freerdp_er_write_octet_string_tag(wStream* s, size_t length, BOOL flag)
{
	freerdp_er_write_universal_tag(s, FREERDP_ER_TAG_OCTET_STRING, FALSE);
	freerdp_er_write_length(s, length, flag);
	return 1 + freerdp_er_skip_length(length);
}

size_t freerdp_er_skip_octet_string(size_t length)
{
	return 1 + freerdp_er_skip_length(length) + length;
}

BOOL freerdp_er_read_BOOL(wStream* s, BOOL* value)
{
	size_t length = 0;
	BYTE v = 0;

	if (!freerdp_er_read_universal_tag(s, FREERDP_ER_TAG_BOOLEAN, FALSE))
		return FALSE;
	freerdp_er_read_length(s, &length);
	if (length != 1)
		return FALSE;
	Stream_Read_UINT8(s, v);
	*value = (v ? TRUE : FALSE);
	return TRUE;
}

void freerdp_er_write_BOOL(wStream* s, BOOL value)
{
	freerdp_er_write_universal_tag(s, FREERDP_ER_TAG_BOOLEAN, FALSE);
	freerdp_er_write_length(s, 1, FALSE);
	Stream_Write_UINT8(s, (value == TRUE) ? 0xFF : 0);
}

BOOL freerdp_er_read_integer(wStream* s, INT32* value)
{
	size_t length = 0;

	freerdp_er_read_universal_tag(s, FREERDP_ER_TAG_INTEGER, FALSE);
	freerdp_er_read_length(s, &length);

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
		BYTE byte = 0;
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

size_t freerdp_er_write_integer(wStream* s, INT32 value)
{
	freerdp_er_write_universal_tag(s, FREERDP_ER_TAG_INTEGER, FALSE);

	if (value <= 127 && value >= -128)
	{
		freerdp_er_write_length(s, 1, FALSE);
		Stream_Write_UINT8(s, value);
		return 2;
	}
	else if (value <= 32767 && value >= -32768)
	{
		freerdp_er_write_length(s, 2, FALSE);
		Stream_Write_UINT16_BE(s, value);
		return 3;
	}
	else
	{
		freerdp_er_write_length(s, 4, FALSE);
		Stream_Write_UINT32_BE(s, value);
		return 5;
	}
}

size_t freerdp_er_skip_integer(INT32 value)
{
	if (value <= 127 && value >= -128)
	{
		return freerdp_er_skip_length(1) + 2;
	}
	else if (value <= 32767 && value >= -32768)
	{
		return freerdp_er_skip_length(2) + 3;
	}
	else
	{
		return freerdp_er_skip_length(4) + 5;
	}
}

BOOL freerdp_er_read_integer_length(wStream* s, size_t* length)
{
	freerdp_er_read_universal_tag(s, FREERDP_ER_TAG_INTEGER, FALSE);
	freerdp_er_read_length(s, length);
	return TRUE;
}

#if defined(WITH_FREERDP_3x_DEPRECATED)

void er_read_length(wStream* s, int* length)
{
	WINPR_ASSERT(length);
	size_t len = 0;
	freerdp_er_read_length(s, &len);
	WINPR_ASSERT(len <= INT32_MAX);
	*length = (int)len;
}

int er_write_length(wStream* s, int length, BOOL flag)
{
	WINPR_ASSERT(length >= 0);
	const size_t rc = freerdp_er_write_length(s, (size_t)length, flag);
	WINPR_ASSERT(rc <= INT32_MAX);
	return (int)rc;
}

int _er_skip_length(int length)
{
	WINPR_ASSERT(length >= 0);
	const size_t rc = freerdp_er_skip_length((size_t)length);
	WINPR_ASSERT(rc <= INT32_MAX);
	return (int)rc;
}

int er_get_content_length(int length)
{
	WINPR_ASSERT(length >= 0);
	const size_t rc = freerdp_er_get_content_length((size_t)length);
	WINPR_ASSERT(rc <= INT32_MAX);
	return (int)rc;
}

BOOL er_read_universal_tag(wStream* s, BYTE tag, BOOL pc)
{
	return freerdp_er_read_universal_tag(s, tag, pc);
}

void er_write_universal_tag(wStream* s, BYTE tag, BOOL pc)
{
	freerdp_er_write_universal_tag(s, tag, pc);
}

BOOL er_read_application_tag(wStream* s, BYTE tag, int* length)
{
	WINPR_ASSERT(length);
	size_t len = 0;
	return freerdp_er_read_application_tag(s, tag, &len);
	WINPR_ASSERT(len <= INT32_MAX);
	*length = (int)len;
}

void er_write_application_tag(wStream* s, BYTE tag, int length, BOOL flag)
{
	WINPR_ASSERT(length >= 0);
	freerdp_er_write_application_tag(s, tag, (size_t)length, flag);
}

BOOL er_read_enumerated(wStream* s, BYTE* enumerated, BYTE count)
{
	WINPR_ASSERT(enumerated);
	return freerdp_er_read_enumerated(s, enumerated, count);
}

void er_write_enumerated(wStream* s, BYTE enumerated, BYTE count, BOOL flag)
{
	freerdp_er_write_enumerated(s, enumerated, count, flag);
}

BOOL er_read_contextual_tag(wStream* s, BYTE tag, int* length, BOOL pc)
{
	WINPR_ASSERT(length);
	size_t len = 0;
	const BOOL rc = freerdp_er_read_contextual_tag(s, tag, &len, pc);
	WINPR_ASSERT(len <= INT32_MAX);
	*length = (int)len;
	return rc;
}

int er_write_contextual_tag(wStream* s, BYTE tag, int length, BOOL pc, BOOL flag)
{
	WINPR_ASSERT(length >= 0);
	const size_t rc = freerdp_er_write_contextual_tag(s, tag, (size_t)length, pc, flag);
	WINPR_ASSERT(rc <= INT32_MAX);
	return (int)rc;
}

int er_skip_contextual_tag(int length)
{
	WINPR_ASSERT(length >= 0);
	const size_t rc = freerdp_er_skip_contextual_tag((size_t)length);
	WINPR_ASSERT(rc <= INT32_MAX);
	return (int)rc;
}

BOOL er_read_sequence_tag(wStream* s, int* length)
{
	WINPR_ASSERT(length);
	size_t len = 0;
	const BOOL rc = freerdp_er_read_sequence_tag(s, &len);
	WINPR_ASSERT(len <= INT32_MAX);
	*length = (int)len;
	return rc;
}

int er_write_sequence_tag(wStream* s, int length, BOOL flag)
{
	WINPR_ASSERT(length >= 0);
	const size_t rc = freerdp_er_write_sequence_tag(s, (size_t)length, flag);
	WINPR_ASSERT(rc <= INT32_MAX);
	return (int)rc;
}

int er_skip_sequence(int length)
{
	WINPR_ASSERT(length >= 0);
	const size_t rc = freerdp_er_skip_sequence((size_t)length);
	WINPR_ASSERT(rc <= INT32_MAX);
	return (int)rc;
}

int er_skip_sequence_tag(int length)
{
	WINPR_ASSERT(length >= 0);
	const size_t rc = freerdp_er_skip_sequence_tag((size_t)length);
	WINPR_ASSERT(rc <= INT32_MAX);
	return (int)rc;
}

BOOL er_read_bit_string(wStream* s, int* length, BYTE* padding)
{
	WINPR_ASSERT(length);
	size_t len = 0;
	const BOOL rc = freerdp_er_read_bit_string(s, &len, padding);
	WINPR_ASSERT(len <= INT32_MAX);
	*length = (int)len;
	return rc;
}

BOOL er_write_bit_string_tag(wStream* s, UINT32 length, BYTE padding, BOOL flag)
{
	return freerdp_er_write_bit_string_tag(s, length, padding, flag);
}

BOOL er_read_octet_string(wStream* s, int* length)
{
	WINPR_ASSERT(length);
	size_t len = 0;
	const BOOL rc = freerdp_er_read_octet_string(s, &len);
	WINPR_ASSERT(len <= INT32_MAX);
	*length = (int)len;
	return rc;
}

void er_write_octet_string(wStream* s, BYTE* oct_str, int length, BOOL flag)
{
	WINPR_ASSERT(length >= 0);
	freerdp_er_write_octet_string(s, oct_str, (size_t)length, flag);
}

int er_write_octet_string_tag(wStream* s, int length, BOOL flag)
{
	WINPR_ASSERT(length >= 0);
	const size_t rc = freerdp_er_write_octet_string_tag(s, (size_t)length, flag);
	WINPR_ASSERT(rc <= INT32_MAX);
	return (int)rc;
}

int er_skip_octet_string(int length)
{
	WINPR_ASSERT(length >= 0);
	const size_t rc = freerdp_er_skip_octet_string((size_t)length);
	WINPR_ASSERT(rc <= INT32_MAX);
	return (int)rc;
}

BOOL er_read_BOOL(wStream* s, BOOL* value)
{
	return freerdp_er_read_BOOL(s, value);
}

void er_write_BOOL(wStream* s, BOOL value)
{
	freerdp_er_write_BOOL(s, value);
}

BOOL er_read_integer(wStream* s, INT32* value)
{
	return freerdp_er_read_integer_length(s, value);
}

int er_write_integer(wStream* s, INT32 value)
{
	const size_t rc = freerdp_er_write_integer(s, value);
	WINPR_ASSERT(rc <= INT32_MAX);
	return (int)rc;
}

BOOL er_read_integer_length(wStream* s, int* length)
{
	WINPR_ASSERT(length);
	size_t len = 0;
	const BOOL rc = freerdp_er_read_integer_length(s, &len);
	WINPR_ASSERT(len <= INT32_MAX);
	*length = (int)len;
	return rc;
}

int er_skip_integer(INT32 value)
{
	WINPR_ASSERT(value >= 0);
	const size_t rc = freerdp_er_skip_length(value);
	WINPR_ASSERT(rc <= INT32_MAX);
	return (int)rc;
}
#endif
