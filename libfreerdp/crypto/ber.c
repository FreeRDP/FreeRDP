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

#include <freerdp/config.h>

#include <stdio.h>
#include <winpr/assert.h>
#include <winpr/crt.h>
#include <winpr/string.h>

#include <freerdp/log.h>
#include <freerdp/crypto/ber.h>

#define TAG FREERDP_TAG("crypto")

BOOL ber_read_length(wStream* s, size_t* length)
{
	BYTE byte;

	WINPR_ASSERT(s);
	WINPR_ASSERT(length);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	Stream_Read_UINT8(s, byte);

	if (byte & 0x80)
	{
		byte &= ~(0x80);

		if (!Stream_CheckAndLogRequiredLength(TAG, s, byte))
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
	WINPR_ASSERT(s);

	if (length > 0xFF)
	{
		WINPR_ASSERT(Stream_GetRemainingCapacity(s) >= 3);
		Stream_Write_UINT8(s, 0x80 ^ 2);
		Stream_Write_UINT16_BE(s, length);
		return 3;
	}

	if (length > 0x7F)
	{
		WINPR_ASSERT(Stream_GetRemainingCapacity(s) >= 2);
		Stream_Write_UINT8(s, 0x80 ^ 1);
		Stream_Write_UINT8(s, length);
		return 2;
	}

	WINPR_ASSERT(Stream_GetRemainingCapacity(s) >= 1);
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
 *
 * @param s The stream to read from
 * @param tag BER universally-defined tag
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL ber_read_universal_tag(wStream* s, BYTE tag, BOOL pc)
{
	BYTE byte;
	const BYTE expect = (BER_CLASS_UNIV | BER_PC(pc) | (BER_TAG_MASK & tag));

	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	Stream_Read_UINT8(s, byte);

	if (byte != expect)
	{
		WLog_WARN(TAG, "invalid tag, got 0x%02" PRIx8 ", expected 0x%02" PRIx8, byte, expect);
		return FALSE;
	}

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
	WINPR_ASSERT(s);
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

	WINPR_ASSERT(s);
	WINPR_ASSERT(length);

	if (tag > 30)
	{
		const BYTE expect = ((BER_CLASS_APPL | BER_CONSTRUCT) | BER_TAG_MASK);

		if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
			return FALSE;

		Stream_Read_UINT8(s, byte);

		if (byte != expect)
		{
			WLog_WARN(TAG, "invalid tag, got 0x%02" PRIx8 ", expected 0x%02" PRIx8, byte, expect);
			return FALSE;
		}

		Stream_Read_UINT8(s, byte);

		if (byte != tag)
		{
			WLog_WARN(TAG, "invalid tag, got 0x%02" PRIx8 ", expected 0x%02" PRIx8, byte, tag);
			return FALSE;
		}

		return ber_read_length(s, length);
	}
	else
	{
		const BYTE expect = ((BER_CLASS_APPL | BER_CONSTRUCT) | (BER_TAG_MASK & tag));

		if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
			return FALSE;

		Stream_Read_UINT8(s, byte);

		if (byte != expect)
		{
			WLog_WARN(TAG, "invalid tag, got 0x%02" PRIx8 ", expected 0x%02" PRIx8, byte, expect);
			return FALSE;
		}

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
	WINPR_ASSERT(s);

	if (tag > 30)
	{
		WINPR_ASSERT(Stream_GetRemainingCapacity(s) >= 2);
		Stream_Write_UINT8(s, (BER_CLASS_APPL | BER_CONSTRUCT) | BER_TAG_MASK);
		Stream_Write_UINT8(s, tag);
		ber_write_length(s, length);
	}
	else
	{
		WINPR_ASSERT(Stream_GetRemainingCapacity(s) >= 1);
		Stream_Write_UINT8(s, (BER_CLASS_APPL | BER_CONSTRUCT) | (BER_TAG_MASK & tag));
		ber_write_length(s, length);
	}
}

BOOL ber_read_contextual_tag(wStream* s, BYTE tag, size_t* length, BOOL pc)
{
	const BYTE expect = ((BER_CLASS_CTXT | BER_PC(pc)) | (BER_TAG_MASK & tag));
	BYTE byte;

	WINPR_ASSERT(s);
	WINPR_ASSERT(length);

	if (Stream_GetRemainingLength(s) < 1)
	{
		WLog_VRB(TAG, "short data, got %" PRIuz ", expected %" PRIuz, Stream_GetRemainingLength(s),
		         1);
		return FALSE;
	}

	Stream_Read_UINT8(s, byte);

	if (byte != expect)
	{
		WLog_VRB(TAG, "invalid tag, got 0x%02" PRIx8 ", expected 0x%02" PRIx8, byte, expect);
		Stream_Rewind(s, 1);
		return FALSE;
	}

	return ber_read_length(s, length);
}

size_t ber_write_contextual_tag(wStream* s, BYTE tag, size_t length, BOOL pc)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(Stream_GetRemainingCapacity(s) >= 1);
	Stream_Write_UINT8(s, (BER_CLASS_CTXT | BER_PC(pc)) | (BER_TAG_MASK & tag));
	return 1 + ber_write_length(s, length);
}

size_t ber_sizeof_contextual_tag(size_t length)
{
	return 1 + _ber_sizeof_length(length);
}

BOOL ber_read_sequence_tag(wStream* s, size_t* length)
{
	const BYTE expect = ((BER_CLASS_UNIV | BER_CONSTRUCT) | (BER_TAG_SEQUENCE_OF));
	BYTE byte;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	Stream_Read_UINT8(s, byte);

	if (byte != expect)
	{
		WLog_WARN(TAG, "invalid tag, got 0x%02" PRIx8 ", expected 0x%02" PRIx8, byte, expect);
		return FALSE;
	}

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

	WINPR_ASSERT(enumerated);

	if (!ber_read_universal_tag(s, BER_TAG_ENUMERATED, FALSE) || !ber_read_length(s, &length))
		return FALSE;

	if (length != 1)
	{
		WLog_WARN(TAG, "short data, got %" PRIuz ", expected %" PRIuz, length, 1);
		return FALSE;
	}
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	Stream_Read_UINT8(s, *enumerated);

	/* check that enumerated value falls within expected range */
	if (*enumerated + 1 > count)
	{
		WLog_WARN(TAG, "invalid data, expected %" PRIu8 " < %" PRIu8, *enumerated, count);
		return FALSE;
	}

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
	if (!ber_read_universal_tag(s, BER_TAG_BIT_STRING, FALSE) || !ber_read_length(s, length))
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
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

	WINPR_ASSERT(oct_str || (length == 0));
	size += ber_write_universal_tag(s, BER_TAG_OCTET_STRING, FALSE);
	size += ber_write_length(s, length);
	Stream_Write(s, oct_str, length);
	size += length;
	return size;
}

size_t ber_write_contextual_octet_string(wStream* s, BYTE tag, const BYTE* oct_str, size_t length)
{
	size_t inner = ber_sizeof_octet_string(length);
	size_t ret, r;

	ret = ber_write_contextual_tag(s, tag, inner, TRUE);
	if (!ret)
		return 0;

	r = ber_write_octet_string(s, oct_str, length);
	if (!r)
		return 0;
	return ret + r;
}

size_t ber_write_char_to_unicode_octet_string(wStream* s, const char* str)
{
	WINPR_ASSERT(str);
	size_t size = 0;
	size_t length = strlen(str) + 1;
	size += ber_write_universal_tag(s, BER_TAG_OCTET_STRING, FALSE);
	size += ber_write_length(s, length * sizeof(WCHAR));

	if (Stream_Write_UTF16_String_From_UTF8(s, length, str, length, TRUE) < 0)
		return 0;
	return size + length * sizeof(WCHAR);
}

size_t ber_write_contextual_unicode_octet_string(wStream* s, BYTE tag, LPWSTR str)
{
	WINPR_ASSERT(str);
	size_t len = _wcslen(str) * sizeof(WCHAR);
	size_t inner_len = ber_sizeof_octet_string(len);
	size_t ret;

	ret = ber_write_contextual_tag(s, tag, inner_len, TRUE);
	return ret + ber_write_octet_string(s, (const BYTE*)str, len);
}

size_t ber_write_contextual_char_to_unicode_octet_string(wStream* s, BYTE tag, const char* str)
{
	size_t ret;
	size_t len = strlen(str);
	size_t inner_len = ber_sizeof_octet_string(len * 2);

	WINPR_ASSERT(Stream_GetRemainingCapacity(s) < ber_sizeof_contextual_tag(inner_len) + inner_len);

	ret = ber_write_contextual_tag(s, tag, inner_len, TRUE);
	ret += ber_write_universal_tag(s, BER_TAG_OCTET_STRING, FALSE);
	ret += ber_write_length(s, len * sizeof(WCHAR));

	if (Stream_Write_UTF16_String_From_UTF8(s, len, str, len, TRUE) < 0)
		return 0;

	return ret + len;
}

BOOL ber_read_unicode_octet_string(wStream* s, LPWSTR* str)
{
	LPWSTR ret = NULL;
	size_t length;

	if (!ber_read_octet_string_tag(s, &length))
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, length))
		return FALSE;

	ret = calloc(1, length + 2);
	if (!ret)
		return FALSE;

	memcpy(ret, Stream_Pointer(s), length);
	ret[length / 2] = 0;
	Stream_Seek(s, length);
	*str = ret;
	return TRUE;
}

BOOL ber_read_char_from_unicode_octet_string(wStream* s, char** str)
{
	size_t length;
	char* ptr;

	*str = NULL;
	if (!ber_read_octet_string_tag(s, &length))
		return FALSE;

	ptr = Stream_Read_UTF16_String_As_UTF8(s, length / sizeof(WCHAR), NULL);
	if (!ptr)
		return FALSE;
	*str = ptr;
	return TRUE;
}

BOOL ber_read_octet_string_tag(wStream* s, size_t* length)
{
	return ber_read_universal_tag(s, BER_TAG_OCTET_STRING, FALSE) && ber_read_length(s, length);
}

BOOL ber_read_octet_string(wStream* s, BYTE** content, size_t* length)
{
	BYTE* ret;

	WINPR_ASSERT(s);
	WINPR_ASSERT(content);
	WINPR_ASSERT(length);

	if (!ber_read_octet_string_tag(s, length))
		return FALSE;
	if (!Stream_CheckAndLogRequiredLength(TAG, s, *length))
		return FALSE;

	ret = malloc(*length);
	if (!ret)
		return FALSE;

	Stream_Read(s, ret, *length);
	*content = ret;
	return TRUE;
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

size_t ber_sizeof_contextual_octet_string(size_t length)
{
	size_t ret = ber_sizeof_octet_string(length);
	return ber_sizeof_contextual_tag(ret) + ret;
}

/** \brief Read a BER BOOLEAN
 *
 * @param s The stream to read from.
 * @param value A pointer to the value read, must not be NULL
 *
 * \return \b TRUE for success, \b FALSE for any failure
 */

BOOL ber_read_BOOL(wStream* s, BOOL* value)
{
	size_t length;
	BYTE v;

	WINPR_ASSERT(value);
	if (!ber_read_universal_tag(s, BER_TAG_BOOLEAN, FALSE) || !ber_read_length(s, &length))
		return FALSE;

	if (length != 1)
	{
		WLog_WARN(TAG, "short data, got %" PRIuz ", expected %" PRIuz, length, 1);
		return FALSE;
	}
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	Stream_Read_UINT8(s, v);
	*value = (v ? TRUE : FALSE);
	return TRUE;
}

/**
 * Write a BER BOOLEAN
 *
 * @param s A pointer to the stream to write to
 * @param value The value to write
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

	WINPR_ASSERT(s);

	if (!ber_read_universal_tag(s, BER_TAG_INTEGER, FALSE))
		return FALSE;
	if (!ber_read_length(s, &length))
		return FALSE;
	if (!Stream_CheckAndLogRequiredLength(TAG, s, length))
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
		WLog_ERR(TAG, "should implement reading an 8 bytes integer");
		return FALSE;
	}
	else
	{
		WLog_ERR(TAG, "should implement reading an integer with length=%" PRIuz, length);
		return FALSE;
	}

	return TRUE;
}

/**
 * Write a BER INTEGER
 *
 * @param s A pointer to the stream to write to
 * @param value The value to write
 *
 * @return The size in bytes that were written
 */

size_t ber_write_integer(wStream* s, UINT32 value)
{
	WINPR_ASSERT(s);

	if (value < 0x80)
	{
		ber_write_universal_tag(s, BER_TAG_INTEGER, FALSE);
		ber_write_length(s, 1);

		Stream_Write_UINT8(s, value);
		return 3;
	}
	else if (value < 0x8000)
	{
		ber_write_universal_tag(s, BER_TAG_INTEGER, FALSE);
		ber_write_length(s, 2);

		Stream_Write_UINT16_BE(s, value);
		return 4;
	}
	else if (value < 0x800000)
	{
		ber_write_universal_tag(s, BER_TAG_INTEGER, FALSE);
		ber_write_length(s, 3);

		Stream_Write_UINT8(s, (value >> 16));
		Stream_Write_UINT16_BE(s, (value & 0xFFFF));
		return 5;
	}
	else if (value < 0x80000000)
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
}

size_t ber_write_contextual_integer(wStream* s, BYTE tag, UINT32 value)
{
	size_t len = ber_sizeof_integer(value);

	WINPR_ASSERT(s);

	WINPR_ASSERT(Stream_EnsureRemainingCapacity(s, len + 5));

	len += ber_write_contextual_tag(s, tag, len, TRUE);
	ber_write_integer(s, value);
	return len;
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
}

size_t ber_sizeof_contextual_integer(UINT32 value)
{
	size_t intSize = ber_sizeof_integer(value);
	return ber_sizeof_contextual_tag(intSize) + intSize;
}

BOOL ber_read_integer_length(wStream* s, size_t* length)
{
	return ber_read_universal_tag(s, BER_TAG_INTEGER, FALSE) && ber_read_length(s, length);
}
