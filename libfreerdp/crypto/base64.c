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

char* crypto_base64_encode(const BYTE* data, int length)
{
	int c;
	const BYTE* q;
	char* p;
	char* ret;
	int i = 0;
	int blocks;

	q = data;
	p = ret = (char*) malloc((length + 3) * 4 / 3 + 1);
	if (!p)
		return NULL;

	/* b1, b2, b3 are input bytes
	 *
	 * 0         1         2
	 * 012345678901234567890123
	 * |  b1  |  b2   |  b3   |
	 *
	 * [ c1 ]     [  c3 ]
	 *      [  c2 ]     [  c4 ]
	 *
	 * c1, c2, c3, c4 are output chars in base64
	 */

	/* first treat complete blocks */
	blocks = length - (length % 3);
	for (i = 0; i < blocks; i += 3, q += 3)
	{
		c = (q[0] << 16) + (q[1] << 8) + q[2];

		*p++ = base64[(c & 0x00FC0000) >> 18];
		*p++ = base64[(c & 0x0003F000) >> 12];
		*p++ = base64[(c & 0x00000FC0) >> 6];
		*p++ = base64[c & 0x0000003F];
	}

	/* then remainder */
	switch (length % 3)
	{
	case 0:
		break;
	case 1:
		c = (q[0] << 16);
		*p++ = base64[(c & 0x00FC0000) >> 18];
		*p++ = base64[(c & 0x0003F000) >> 12];
		*p++ = '=';
		*p++ = '=';
		break;
	case 2:
		c = (q[0] << 16) + (q[1] << 8);
		*p++ = base64[(c & 0x00FC0000) >> 18];
		*p++ = base64[(c & 0x0003F000) >> 12];
		*p++ = base64[(c & 0x00000FC0) >> 6];
		*p++ = '=';
		break;
	}

	*p = 0;

	return ret;
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

static void* base64_decode(const char* s, int length, int* data_len)
{
	int n[4];
	BYTE* q;
	BYTE* data;
	int nBlocks, i, outputLen;

	if (length % 4)
		return NULL;

	q = data = (BYTE*) malloc(length / 4 * 3);

	/* first treat complete blocks */
	nBlocks = (length / 4);
	outputLen = 0;

	for (i = 0; i < nBlocks-1; i++, q += 3)
	{
		n[0] = base64_decode_char(*s++);
		n[1] = base64_decode_char(*s++);
		n[2] = base64_decode_char(*s++);
		n[3] = base64_decode_char(*s++);

		if ((n[0] == -1) || (n[1] == -1) || (n[2] == -1) || (n[3] == -1))
			goto out_free;

		q[0] = (n[0] << 2) + (n[1] >> 4);
		q[1] = ((n[1] & 15) << 4) + (n[2] >> 2);
		q[2] = ((n[2] & 3) << 6) + n[3];
		outputLen += 3;
	}

	/* treat last block */
	n[0] = base64_decode_char(*s++);
	n[1] = base64_decode_char(*s++);
	if ((n[0] == -1) || (n[1] == -1))
		goto out_free;

	n[2] = base64_decode_char(*s++);
	n[3] = base64_decode_char(*s++);

	q[0] = (n[0] << 2) + (n[1] >> 4);
	if (n[2] == -1)
	{
		/* XX== */
		outputLen += 1;
		if (n[3] != -1)
			goto out_free;

		q[1] = ((n[1] & 15) << 4);
	}
	else if (n[3] == -1)
	{
		/* yyy= */
		outputLen += 2;
		q[1] = ((n[1] & 15) << 4) + (n[2] >> 2);
		q[2] = ((n[2] & 3) << 6);
	}
	else
	{
		/* XXXX */
		outputLen += 3;
		q[0] = (n[0] << 2) + (n[1] >> 4);
		q[1] = ((n[1] & 15) << 4) + (n[2] >> 2);
		q[2] = ((n[2] & 3) << 6) + n[3];
	}

	*data_len = outputLen;

	return data;
out_free:
	free(data);
	return NULL;
}

void crypto_base64_decode(const char* enc_data, int length, BYTE** dec_data, int* res_length)
{
	*dec_data = base64_decode(enc_data, length, res_length);
}
