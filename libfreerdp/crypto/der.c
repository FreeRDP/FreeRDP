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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include <freerdp/crypto/der.h>

int _der_skip_length(int length)
{
	if (length > 0x7F && length <= 0xFF)
		return 2;
	else if (length > 0xFF)
		return 3;
	else
		return 1;
}

int der_write_length(wStream* s, int length)
{
	if (length > 0x7F && length <= 0xFF)
	{
		stream_write_BYTE(s, 0x81);
		stream_write_BYTE(s, length);
		return 2;
	}
	else if (length > 0xFF)
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

int der_get_content_length(int length)
{
	if (length > 0x81 && length <= 0x102)
		return length - 3;
	else if (length > 0x102)
		return length - 4;
	else
		return length - 2;
}

int der_skip_contextual_tag(int length)
{
	return _der_skip_length(length) + 1;
}

int der_write_contextual_tag(wStream* s, BYTE tag, int length, BOOL pc)
{
	stream_write_BYTE(s, (ER_CLASS_CTXT | ER_PC(pc)) | (ER_TAG_MASK & tag));
	return der_write_length(s, length) + 1;
}

void der_write_universal_tag(wStream* s, BYTE tag, BOOL pc)
{
	stream_write_BYTE(s, (ER_CLASS_UNIV | ER_PC(pc)) | (ER_TAG_MASK & tag));
}

int der_skip_octet_string(int length)
{
	return 1 + _der_skip_length(length) + length;
}

void der_write_octet_string(wStream* s, BYTE* oct_str, int length)
{
	der_write_universal_tag(s, ER_TAG_OCTET_STRING, FALSE);
	der_write_length(s, length);
	stream_write(s, oct_str, length);
}

int der_skip_sequence_tag(int length)
{
	return 1 + _der_skip_length(length);
}

int der_write_sequence_tag(wStream* s, int length)
{
	stream_write_BYTE(s, (ER_CLASS_UNIV | ER_CONSTRUCT) | (ER_TAG_MASK & ER_TAG_SEQUENCE));
	return der_write_length(s, length) + 1;
}

