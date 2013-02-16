/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Base64 Encoding & Decoding
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/crypto/crypto.h>

static const char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char* crypto_base64_encode(BYTE* data, int length)
{
	int c;
	BYTE* q;
	char* p;
	char* str;
	int i = 0;

	q = data;
	p = str = (char*) malloc((length + 3) * 4 / 3 + 1);

	while (i < length)
	{
		c = q[i++];

		c <<= 8;
		if (i < length)
			c += q[i];
		i++;

		c <<= 8;
		if (i < length)
			c += q[i];
		i++;

		*p++ = base64[(c & 0x00FC0000) >> 18];
		*p++ = base64[(c & 0x0003F000) >> 12];

		if (i > length + 1)
			*p++ = '=';
		else
			*p++ = base64[(c & 0x00000FC0) >> 6];

		if (i > length)
			*p++ = '=';
		else
			*p++ = base64[c & 0x0000003F];
	}

	*p = 0;

	return str;
}

static int base64_decode_char(char c)
{
	if (c >= 'A' && c <= 'Z')
		return c - 'A';

	if (c >= 'a' && c <= 'z')
		return c - 'a' + 26;

	if (c >= '0' && c <= '9')
		return c - '0' + 52;

	if (c == '+')
		return 62;

	if (c == '/')
		return 63;

	if (c == '=')
		return -1;

	return -1;
}

static void* base64_decode(BYTE* s, int length, int* data_len)
{
	char* p;
	int n[4];
	BYTE* q;
	BYTE* data;

	if (length % 4)
		return NULL;

	n[0] = n[1] = n[2] = n[3] = 0;
	q = data = (BYTE*) malloc(length / 4 * 3);

	for (p = (char*) s; *p; )
	{
		n[0] = base64_decode_char(*p++);
		n[1] = base64_decode_char(*p++);
		n[2] = base64_decode_char(*p++);
		n[3] = base64_decode_char(*p++);

		if ((n[0] == -1) || (n[1] == -1))
			return NULL;

		if ((n[2] == -1) && (n[3] != -1))
			return NULL;

		q[0] = (n[0] << 2) + (n[1] >> 4);

		if (n[2] != -1)
			q[1] = ((n[1] & 15) << 4) + (n[2] >> 2);

		if (n[3] != -1)
			q[2] = ((n[2] & 3) << 6) + n[3];

		q += 3;
	}

	*data_len = q - data - (n[2] == -1) - (n[3] == -1);

	return data;
}

void crypto_base64_decode(BYTE* enc_data, int length, BYTE** dec_data, int* res_length)
{
	*dec_data = base64_decode(enc_data, length, res_length);
}
