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

BOOL freerdp_ber_read_length(wStream* s, size_t* length)
{
	BYTE byte = 0;

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
		{
			WLog_ERR(TAG, "ber: unexpected byte 0x%02" PRIx8 ", expected [1,2]", byte);
			return FALSE;
		}
	}
	else
	{
		*length = byte;
	}

	return TRUE;
}

size_t freerdp_ber_write_length(wStream* s, size_t length)
{
	WINPR_ASSERT(s);

	if (length > 0xFF)
	{
		WINPR_ASSERT(length <= UINT16_MAX);
		WINPR_ASSERT(Stream_GetRemainingCapacity(s) >= 3);
		Stream_Write_UINT8(s, 0x80 ^ 2);
		Stream_Write_UINT16_BE(s, (UINT16)length);
		return 3;
	}

	WINPR_ASSERT(length <= UINT8_MAX);
	if (length > 0x7F)
	{
		WINPR_ASSERT(Stream_GetRemainingCapacity(s) >= 2);
		Stream_Write_UINT8(s, 0x80 ^ 1);
		Stream_Write_UINT8(s, (UINT8)length);
		return 2;
	}

	WINPR_ASSERT(Stream_GetRemainingCapacity(s) >= 1);
	Stream_Write_UINT8(s, (UINT8)length);
	return 1;
}

size_t freerdp_ber_sizeof_length(size_t length)
{
	if (length > 0xFF)
		return 3;

	if (length > 0x7F)
		return 2;

	return 1;
}

BOOL freerdp_ber_read_universal_tag(wStream* s, BYTE tag, BOOL pc)
{
	BYTE byte = 0;
	const BYTE expect =
	    (FREERDP_BER_CLASS_UNIV | FREERDP_BER_PC(pc) | (FREERDP_BER_TAG_MASK & tag));

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

size_t freerdp_ber_write_universal_tag(wStream* s, BYTE tag, BOOL pc)
{
	WINPR_ASSERT(s);
	Stream_Write_UINT8(s, (FREERDP_BER_CLASS_UNIV | FREERDP_BER_PC(pc)) |
	                          (FREERDP_BER_TAG_MASK & tag));
	return 1;
}

BOOL freerdp_ber_read_application_tag(wStream* s, BYTE tag, size_t* length)
{
	BYTE byte = 0;

	WINPR_ASSERT(s);
	WINPR_ASSERT(length);

	if (tag > 30)
	{
		const BYTE expect =
		    ((FREERDP_BER_CLASS_APPL | FREERDP_BER_CONSTRUCT) | FREERDP_BER_TAG_MASK);

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

		return freerdp_ber_read_length(s, length);
	}
	else
	{
		const BYTE expect =
		    ((FREERDP_BER_CLASS_APPL | FREERDP_BER_CONSTRUCT) | (FREERDP_BER_TAG_MASK & tag));

		if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
			return FALSE;

		Stream_Read_UINT8(s, byte);

		if (byte != expect)
		{
			WLog_WARN(TAG, "invalid tag, got 0x%02" PRIx8 ", expected 0x%02" PRIx8, byte, expect);
			return FALSE;
		}

		return freerdp_ber_read_length(s, length);
	}

	return TRUE;
}

void freerdp_ber_write_application_tag(wStream* s, BYTE tag, size_t length)
{
	WINPR_ASSERT(s);

	if (tag > 30)
	{
		WINPR_ASSERT(Stream_GetRemainingCapacity(s) >= 2);
		Stream_Write_UINT8(s,
		                   (FREERDP_BER_CLASS_APPL | FREERDP_BER_CONSTRUCT) | FREERDP_BER_TAG_MASK);
		Stream_Write_UINT8(s, tag);
		freerdp_ber_write_length(s, length);
	}
	else
	{
		WINPR_ASSERT(Stream_GetRemainingCapacity(s) >= 1);
		Stream_Write_UINT8(s, (FREERDP_BER_CLASS_APPL | FREERDP_BER_CONSTRUCT) |
		                          (FREERDP_BER_TAG_MASK & tag));
		freerdp_ber_write_length(s, length);
	}
}

BOOL freerdp_ber_read_contextual_tag(wStream* s, BYTE tag, size_t* length, BOOL pc)
{
	const BYTE expect =
	    ((FREERDP_BER_CLASS_CTXT | FREERDP_BER_PC(pc)) | (FREERDP_BER_TAG_MASK & tag));
	BYTE byte = 0;

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

	return freerdp_ber_read_length(s, length);
}

size_t freerdp_ber_write_contextual_tag(wStream* s, BYTE tag, size_t length, BOOL pc)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(Stream_GetRemainingCapacity(s) >= 1);
	Stream_Write_UINT8(s, (FREERDP_BER_CLASS_CTXT | FREERDP_BER_PC(pc)) |
	                          (FREERDP_BER_TAG_MASK & tag));
	return 1 + freerdp_ber_write_length(s, length);
}

size_t freerdp_ber_sizeof_contextual_tag(size_t length)
{
	return 1 + freerdp_ber_sizeof_length(length);
}

BOOL freerdp_ber_read_sequence_tag(wStream* s, size_t* length)
{
	const BYTE expect =
	    ((FREERDP_BER_CLASS_UNIV | FREERDP_BER_CONSTRUCT) | (FREERDP_BER_TAG_SEQUENCE_OF));
	BYTE byte = 0;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	Stream_Read_UINT8(s, byte);

	if (byte != expect)
	{
		WLog_WARN(TAG, "invalid tag, got 0x%02" PRIx8 ", expected 0x%02" PRIx8, byte, expect);
		return FALSE;
	}

	return freerdp_ber_read_length(s, length);
}

size_t freerdp_ber_write_sequence_tag(wStream* s, size_t length)
{
	Stream_Write_UINT8(s, (FREERDP_BER_CLASS_UNIV | FREERDP_BER_CONSTRUCT) |
	                          (FREERDP_BER_TAG_MASK & FREERDP_BER_TAG_SEQUENCE));
	return 1 + freerdp_ber_write_length(s, length);
}

size_t freerdp_ber_sizeof_sequence(size_t length)
{
	return 1 + freerdp_ber_sizeof_length(length) + length;
}

size_t freerdp_ber_sizeof_sequence_tag(size_t length)
{
	return 1 + freerdp_ber_sizeof_length(length);
}

BOOL freerdp_ber_read_enumerated(wStream* s, BYTE* enumerated, BYTE count)
{
	size_t length = 0;

	WINPR_ASSERT(enumerated);

	if (!freerdp_ber_read_universal_tag(s, FREERDP_BER_TAG_ENUMERATED, FALSE) ||
	    !freerdp_ber_read_length(s, &length))
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

BOOL freerdp_ber_write_enumerated(wStream* s, BYTE enumerated, BYTE count)
{
	if (enumerated >= count)
		return FALSE;
	freerdp_ber_write_universal_tag(s, FREERDP_BER_TAG_ENUMERATED, FALSE);
	freerdp_ber_write_length(s, 1);
	Stream_Write_UINT8(s, enumerated);
	return TRUE;
}

BOOL freerdp_ber_read_bit_string(wStream* s, size_t* length, BYTE* padding)
{
	if (!freerdp_ber_read_universal_tag(s, FREERDP_BER_TAG_BIT_STRING, FALSE) ||
	    !freerdp_ber_read_length(s, length))
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	Stream_Read_UINT8(s, *padding);
	return TRUE;
}

size_t freerdp_ber_write_octet_string(wStream* s, const BYTE* oct_str, size_t length)
{
	size_t size = 0;

	WINPR_ASSERT(oct_str || (length == 0));
	size += freerdp_ber_write_universal_tag(s, FREERDP_BER_TAG_OCTET_STRING, FALSE);
	size += freerdp_ber_write_length(s, length);
	Stream_Write(s, oct_str, length);
	size += length;
	return size;
}

size_t freerdp_ber_write_contextual_octet_string(wStream* s, BYTE tag, const BYTE* oct_str,
                                                 size_t length)
{
	size_t inner = freerdp_ber_sizeof_octet_string(length);
	size_t ret = 0;
	size_t r = 0;

	ret = freerdp_ber_write_contextual_tag(s, tag, inner, TRUE);
	if (!ret)
		return 0;

	r = freerdp_ber_write_octet_string(s, oct_str, length);
	if (!r)
		return 0;
	return ret + r;
}

size_t freerdp_ber_write_char_to_unicode_octet_string(wStream* s, const char* str)
{
	WINPR_ASSERT(str);
	size_t size = 0;
	size_t length = strlen(str) + 1;
	size += freerdp_ber_write_universal_tag(s, FREERDP_BER_TAG_OCTET_STRING, FALSE);
	size += freerdp_ber_write_length(s, length * sizeof(WCHAR));

	if (Stream_Write_UTF16_String_From_UTF8(s, length, str, length, TRUE) < 0)
		return 0;
	return size + length * sizeof(WCHAR);
}

size_t freerdp_ber_write_contextual_unicode_octet_string(wStream* s, BYTE tag, const WCHAR* str)
{
	WINPR_ASSERT(str);
	size_t len = _wcslen(str) * sizeof(WCHAR);
	size_t inner_len = freerdp_ber_sizeof_octet_string(len);
	size_t ret = 0;

	ret = freerdp_ber_write_contextual_tag(s, tag, inner_len, TRUE);
	return ret + freerdp_ber_write_octet_string(s, (const BYTE*)str, len);
}

size_t freerdp_ber_write_contextual_char_to_unicode_octet_string(wStream* s, BYTE tag,
                                                                 const char* str)
{
	size_t ret = 0;
	size_t len = strlen(str);
	size_t inner_len = freerdp_ber_sizeof_octet_string(len * 2);

	WINPR_ASSERT(Stream_GetRemainingCapacity(s) <
	             freerdp_ber_sizeof_contextual_tag(inner_len) + inner_len);

	ret = freerdp_ber_write_contextual_tag(s, tag, inner_len, TRUE);
	ret += freerdp_ber_write_universal_tag(s, FREERDP_BER_TAG_OCTET_STRING, FALSE);
	ret += freerdp_ber_write_length(s, len * sizeof(WCHAR));

	if (Stream_Write_UTF16_String_From_UTF8(s, len, str, len, TRUE) < 0)
		return 0;

	return ret + len;
}

BOOL freerdp_ber_read_unicode_octet_string(wStream* s, LPWSTR* str)
{
	LPWSTR ret = NULL;
	size_t length = 0;

	if (!freerdp_ber_read_octet_string_tag(s, &length))
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, length))
		return FALSE;

	ret = calloc(1, length + 2);
	if (!ret)
		return FALSE;

	memcpy(ret, Stream_ConstPointer(s), length);
	ret[length / 2] = 0;
	Stream_Seek(s, length);
	*str = ret;
	return TRUE;
}

BOOL freerdp_ber_read_char_from_unicode_octet_string(wStream* s, char** str)
{
	size_t length = 0;
	char* ptr = NULL;

	*str = NULL;
	if (!freerdp_ber_read_octet_string_tag(s, &length))
		return FALSE;

	ptr = Stream_Read_UTF16_String_As_UTF8(s, length / sizeof(WCHAR), NULL);
	if (!ptr)
		return FALSE;
	*str = ptr;
	return TRUE;
}

BOOL freerdp_ber_read_octet_string_tag(wStream* s, size_t* length)
{
	return freerdp_ber_read_universal_tag(s, FREERDP_BER_TAG_OCTET_STRING, FALSE) &&
	       freerdp_ber_read_length(s, length);
}

BOOL freerdp_ber_read_octet_string(wStream* s, BYTE** content, size_t* length)
{
	BYTE* ret = NULL;

	WINPR_ASSERT(s);
	WINPR_ASSERT(content);
	WINPR_ASSERT(length);

	if (!freerdp_ber_read_octet_string_tag(s, length))
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

size_t freerdp_ber_write_octet_string_tag(wStream* s, size_t length)
{
	freerdp_ber_write_universal_tag(s, FREERDP_BER_TAG_OCTET_STRING, FALSE);
	freerdp_ber_write_length(s, length);
	return 1 + freerdp_ber_sizeof_length(length);
}

size_t freerdp_ber_sizeof_octet_string(size_t length)
{
	return 1 + freerdp_ber_sizeof_length(length) + length;
}

size_t freerdp_ber_sizeof_contextual_octet_string(size_t length)
{
	size_t ret = freerdp_ber_sizeof_octet_string(length);
	return freerdp_ber_sizeof_contextual_tag(ret) + ret;
}

BOOL freerdp_ber_read_BOOL(wStream* s, BOOL* value)
{
	size_t length = 0;
	BYTE v = 0;

	WINPR_ASSERT(value);
	if (!freerdp_ber_read_universal_tag(s, FREERDP_BER_TAG_BOOLEAN, FALSE) ||
	    !freerdp_ber_read_length(s, &length))
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

void freerdp_ber_write_BOOL(wStream* s, BOOL value)
{
	freerdp_ber_write_universal_tag(s, FREERDP_BER_TAG_BOOLEAN, FALSE);
	freerdp_ber_write_length(s, 1);
	Stream_Write_UINT8(s, (value == TRUE) ? 0xFF : 0);
}

BOOL freerdp_ber_read_integer(wStream* s, UINT32* value)
{
	size_t length = 0;

	WINPR_ASSERT(s);

	if (!freerdp_ber_read_universal_tag(s, FREERDP_BER_TAG_INTEGER, FALSE))
		return FALSE;
	if (!freerdp_ber_read_length(s, &length))
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
		BYTE byte = 0;
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

size_t freerdp_ber_write_integer(wStream* s, UINT32 value)
{
	WINPR_ASSERT(s);

	if (value < 0x80)
	{
		freerdp_ber_write_universal_tag(s, FREERDP_BER_TAG_INTEGER, FALSE);
		freerdp_ber_write_length(s, 1);

		Stream_Write_UINT8(s, value);
		return 3;
	}
	else if (value < 0x8000)
	{
		freerdp_ber_write_universal_tag(s, FREERDP_BER_TAG_INTEGER, FALSE);
		freerdp_ber_write_length(s, 2);

		Stream_Write_UINT16_BE(s, value);
		return 4;
	}
	else if (value < 0x800000)
	{
		freerdp_ber_write_universal_tag(s, FREERDP_BER_TAG_INTEGER, FALSE);
		freerdp_ber_write_length(s, 3);

		Stream_Write_UINT8(s, (value >> 16));
		Stream_Write_UINT16_BE(s, (value & 0xFFFF));
		return 5;
	}
	else if (value < 0x80000000)
	{
		freerdp_ber_write_universal_tag(s, FREERDP_BER_TAG_INTEGER, FALSE);
		freerdp_ber_write_length(s, 4);

		Stream_Write_UINT32_BE(s, value);
		return 6;
	}
	else
	{
		/* treat as signed integer i.e. NT/HRESULT error codes */
		freerdp_ber_write_universal_tag(s, FREERDP_BER_TAG_INTEGER, FALSE);
		freerdp_ber_write_length(s, 4);

		Stream_Write_UINT32_BE(s, value);
		return 6;
	}
}

size_t freerdp_ber_write_contextual_integer(wStream* s, BYTE tag, UINT32 value)
{
	size_t len = freerdp_ber_sizeof_integer(value);

	WINPR_ASSERT(s);

	WINPR_ASSERT(Stream_EnsureRemainingCapacity(s, len + 5));

	len += freerdp_ber_write_contextual_tag(s, tag, len, TRUE);
	freerdp_ber_write_integer(s, value);
	return len;
}

size_t freerdp_ber_sizeof_integer(UINT32 value)
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

size_t freerdp_ber_sizeof_contextual_integer(UINT32 value)
{
	size_t intSize = freerdp_ber_sizeof_integer(value);
	return freerdp_ber_sizeof_contextual_tag(intSize) + intSize;
}

BOOL freerdp_ber_read_integer_length(wStream* s, size_t* length)
{
	return freerdp_ber_read_universal_tag(s, FREERDP_BER_TAG_INTEGER, FALSE) &&
	       freerdp_ber_read_length(s, length);
}

#if defined(WITH_FREERDP_3x_DEPRECATED)

BOOL ber_read_length(wStream* s, size_t* length)
{
	return freerdp_ber_read_length(s, length);
}

size_t ber_write_length(wStream* s, size_t length)
{
	return freerdp_ber_write_length(s, length);
}

size_t _ber_sizeof_length(size_t length)
{
	return freerdp_ber_sizeof_length(length);
}

BOOL ber_read_universal_tag(wStream* s, BYTE tag, BOOL pc)
{
	return freerdp_ber_read_universal_tag(s, tag, pc);
}

size_t ber_write_universal_tag(wStream* s, BYTE tag, BOOL pc)
{
	return freerdp_ber_write_universal_tag(s, tag, pc);
}

BOOL ber_read_application_tag(wStream* s, BYTE tag, size_t* length)
{
	return freerdp_ber_read_application_tag(s, tag, length);
}

void ber_write_application_tag(wStream* s, BYTE tag, size_t length)
{
	freerdp_ber_write_application_tag(s, tag, length);
}

BOOL ber_read_enumerated(wStream* s, BYTE* enumerated, BYTE count)
{
	return freerdp_ber_read_enumerated(s, enumerated, count);
}

void ber_write_enumerated(wStream* s, BYTE enumerated, BYTE count)
{
	freerdp_ber_write_enumerated(s, enumerated, count);
}

BOOL ber_read_contextual_tag(wStream* s, BYTE tag, size_t* length, BOOL pc)
{
	return freerdp_ber_read_contextual_tag(s, tag, length, pc);
}

size_t ber_write_contextual_tag(wStream* s, BYTE tag, size_t length, BOOL pc)
{
	return freerdp_ber_write_contextual_tag(s, tag, length, pc);
}

size_t ber_sizeof_contextual_tag(size_t length)
{
	return freerdp_ber_sizeof_contextual_tag(length);
}

BOOL ber_read_sequence_tag(wStream* s, size_t* length)
{
	return freerdp_ber_read_sequence_tag(s, length);
}

size_t ber_write_sequence_tag(wStream* s, size_t length)
{
	return freerdp_ber_write_sequence_tag(s, length);
}

size_t ber_sizeof_sequence(size_t length)
{
	return freerdp_ber_sizeof_sequence(length);
}

size_t ber_sizeof_sequence_tag(size_t length)
{
	return freerdp_ber_sizeof_sequence_tag(length);
}

BOOL ber_read_bit_string(wStream* s, size_t* length, BYTE* padding)
{
	return freerdp_ber_read_bit_string(s, length, padding);
}

BOOL ber_read_octet_string_tag(wStream* s, size_t* length)
{
	return freerdp_ber_read_octet_string_tag(s, length);
}

BOOL ber_read_octet_string(wStream* s, BYTE** content, size_t* length)
{
	return freerdp_ber_read_octet_string(s, content, length);
}

size_t ber_write_octet_string_tag(wStream* s, size_t length)
{
	return freerdp_ber_write_octet_string_tag(s, length);
}

size_t ber_sizeof_octet_string(size_t length)
{
	return freerdp_ber_sizeof_octet_string(length);
}

size_t ber_sizeof_contextual_octet_string(size_t length)
{
	return freerdp_ber_sizeof_contextual_octet_string(length);
}

size_t ber_write_char_to_unicode_octet_string(wStream* s, const char* str)
{
	return freerdp_ber_write_char_to_unicode_octet_string(s, str);
}

size_t ber_write_contextual_char_to_unicode_octet_string(wStream* s, BYTE tag, const char* oct_str)
{
	return freerdp_ber_write_contextual_char_to_unicode_octet_string(s, tag, oct_str);
}

size_t ber_write_octet_string(wStream* s, const BYTE* oct_str, size_t length)
{
	return freerdp_ber_write_octet_string(s, oct_str, length);
}

BOOL ber_read_char_from_unicode_octet_string(wStream* s, char** str)
{
	return freerdp_ber_read_char_from_unicode_octet_string(s, str);
}

BOOL ber_read_unicode_octet_string(wStream* s, LPWSTR* str)
{
	return freerdp_ber_read_unicode_octet_string(s, str);
}

size_t ber_write_contextual_octet_string(wStream* s, BYTE tag, const BYTE* oct_str, size_t length)
{
	return freerdp_ber_write_contextual_octet_string(s, tag, oct_str, length);
}

size_t ber_write_contextual_unicode_octet_string(wStream* s, BYTE tag, const LPWSTR str)
{
	return freerdp_ber_write_contextual_unicode_octet_string(s, tag, str);
}

BOOL ber_read_BOOL(wStream* s, BOOL* value)
{
	return freerdp_ber_read_BOOL(s, value);
}

void ber_write_BOOL(wStream* s, BOOL value)
{
	freerdp_ber_write_BOOL(s, value);
}

BOOL ber_read_integer(wStream* s, UINT32* value)
{
	return freerdp_ber_read_integer(s, value);
}

size_t ber_write_integer(wStream* s, UINT32 value)
{
	return freerdp_ber_write_integer(s, value);
}

size_t ber_write_contextual_integer(wStream* s, BYTE tag, UINT32 value)
{
	return freerdp_ber_write_contextual_integer(s, tag, value);
}

BOOL ber_read_integer_length(wStream* s, size_t* length)
{
	return freerdp_ber_read_integer_length(s, length);
}

size_t ber_sizeof_integer(UINT32 value)
{
	return freerdp_ber_sizeof_integer(value);
}

size_t ber_sizeof_contextual_integer(UINT32 value)
{
	return freerdp_ber_sizeof_contextual_integer(value);
}
#endif
