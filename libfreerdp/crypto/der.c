/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ASN.1 Basic Encoding Rules (DER)
 *
 * Copyright 2011 Samsung, Author Jiten Pathy
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

#include <winpr/assert.h>
#include <winpr/crt.h>
#include <winpr/wtypes.h>

#include <freerdp/crypto/der.h>

size_t freerdp_der_skip_length(size_t length)
{
	if (length > 0x7F && length <= 0xFF)
		return 2;
	else if (length > 0xFF)
		return 3;
	else
		return 1;
}

size_t freerdp_der_write_length(wStream* s, size_t length)
{
	WINPR_ASSERT(length <= UINT16_MAX);

	if (length > 0x7F && length <= 0xFF)
	{
		Stream_Write_UINT8(s, 0x81);
		Stream_Write_UINT8(s, (UINT8)length);
		return 2;
	}
	else if (length > 0xFF)
	{
		Stream_Write_UINT8(s, 0x82);
		Stream_Write_UINT16_BE(s, (UINT16)length);
		return 3;
	}
	else
	{
		Stream_Write_UINT8(s, (UINT8)length);
		return 1;
	}
}

size_t freerdp_der_get_content_length(size_t length)
{
	if (length > 0x81 && length <= 0x102)
		return length - 3;
	else if (length > 0x102)
		return length - 4;
	else
		return length - 2;
}

size_t freerdp_der_skip_contextual_tag(size_t length)
{
	return freerdp_der_skip_length(length) + 1;
}

size_t freerdp_der_write_contextual_tag(wStream* s, BYTE tag, size_t length, BOOL pc)
{
	Stream_Write_UINT8(s,
	                   (FREERDP_ER_CLASS_CTXT | FREERDP_ER_PC(pc)) | (FREERDP_ER_TAG_MASK & tag));
	return freerdp_der_write_length(s, length) + 1;
}

static void freerdp_der_write_universal_tag(wStream* s, BYTE tag, BOOL pc)
{
	Stream_Write_UINT8(s,
	                   (FREERDP_ER_CLASS_UNIV | FREERDP_ER_PC(pc)) | (FREERDP_ER_TAG_MASK & tag));
}

size_t freerdp_der_skip_octet_string(size_t length)
{
	return 1 + freerdp_der_skip_length(length) + length;
}

void freerdp_der_write_octet_string(wStream* s, const BYTE* oct_str, size_t length)
{
	freerdp_der_write_universal_tag(s, FREERDP_ER_TAG_OCTET_STRING, FALSE);
	freerdp_der_write_length(s, length);
	Stream_Write(s, oct_str, length);
}

size_t freerdp_der_skip_sequence_tag(size_t length)
{
	return 1 + freerdp_der_skip_length(length);
}

size_t freerdp_der_write_sequence_tag(wStream* s, size_t length)
{
	Stream_Write_UINT8(s, (FREERDP_ER_CLASS_UNIV | FREERDP_ER_CONSTRUCT) |
	                          (FREERDP_ER_TAG_MASK & FREERDP_ER_TAG_SEQUENCE));
	return freerdp_der_write_length(s, length) + 1;
}

#if defined(WITH_FREERDP_3x_DEPRECATED)
int _der_skip_length(int length)
{
	WINPR_ASSERT(length >= 0);
	const size_t rc = freerdp_der_skip_length((size_t)length);
	WINPR_ASSERT(rc <= INT32_MAX);
	return (int)rc;
}

int der_write_length(wStream* s, int length)
{
	WINPR_ASSERT(length >= 0);
	const size_t rc = freerdp_der_write_length(s, (size_t)length);
	WINPR_ASSERT(rc <= INT32_MAX);
	return (int)rc;
}

int der_get_content_length(int length)
{
	WINPR_ASSERT(length >= 0);
	const size_t rc = freerdp_der_get_content_length((size_t)length);
	WINPR_ASSERT(rc <= INT32_MAX);
	return (int)rc;
}

int der_skip_octet_string(int length)
{
	WINPR_ASSERT(length >= 0);
	const size_t rc = freerdp_der_skip_octet_string((size_t)length);
	WINPR_ASSERT(rc <= INT32_MAX);
	return (int)rc;
}

int der_skip_sequence_tag(int length)
{
	WINPR_ASSERT(length >= 0);
	const size_t rc = freerdp_der_skip_sequence_tag((size_t)length);
	WINPR_ASSERT(rc <= INT32_MAX);
	return (int)rc;
}

int der_write_sequence_tag(wStream* s, int length)
{
	WINPR_ASSERT(length >= 0);
	const size_t rc = freerdp_der_write_sequence_tag(s, (size_t)length);
	WINPR_ASSERT(rc <= INT32_MAX);
	return (int)rc;
}

int der_skip_contextual_tag(int length)
{
	WINPR_ASSERT(length >= 0);
	const size_t rc = freerdp_der_skip_contextual_tag((size_t)length);
	WINPR_ASSERT(rc <= INT32_MAX);
	return (int)rc;
}

int der_write_contextual_tag(wStream* s, BYTE tag, int length, BOOL pc)
{
	WINPR_ASSERT(length >= 0);
	const size_t rc = freerdp_der_write_contextual_tag(s, tag, (size_t)length, pc);
	WINPR_ASSERT(rc <= INT32_MAX);
	return (int)rc;
}

void der_write_octet_string(wStream* s, const BYTE* oct_str, int length)
{
	WINPR_ASSERT(length >= 0);
	freerdp_der_write_octet_string(s, oct_str, (size_t)length);
}
#endif
