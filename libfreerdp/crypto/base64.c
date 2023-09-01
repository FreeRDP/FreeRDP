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

#include <freerdp/config.h>

#include <winpr/crt.h>

#include <freerdp/crypto/crypto.h>

static const char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char base64url[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static char* base64_encode_ex(const char* alphabet, const BYTE* data, size_t length, BOOL pad,
                              BOOL crLf, size_t lineSize)
{
	int c;
	const BYTE* q;
	char* p;
	char* ret;
	int i = 0;
	int blocks;
	size_t outLen = (length + 3) * 4 / 3;
	size_t extra = 0;
	if (crLf)
	{
		size_t nCrLf = (outLen + lineSize - 1) / lineSize;
		extra = nCrLf * 2;
	}
	size_t outCounter = 0;

	q = data;
	p = ret = (char*)malloc(outLen + extra + 1ull);
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

		*p++ = alphabet[(c & 0x00FC0000) >> 18];
		*p++ = alphabet[(c & 0x0003F000) >> 12];
		*p++ = alphabet[(c & 0x00000FC0) >> 6];
		*p++ = alphabet[c & 0x0000003F];

		outCounter += 4;
		if (crLf && (outCounter % lineSize == 0))
		{
			*p++ = '\r';
			*p++ = '\n';
		}
	}

	/* then remainder */
	switch (length % 3)
	{
		case 0:
			break;
		case 1:
			c = (q[0] << 16);
			*p++ = alphabet[(c & 0x00FC0000) >> 18];
			*p++ = alphabet[(c & 0x0003F000) >> 12];
			if (pad)
			{
				*p++ = '=';
				*p++ = '=';
			}
			break;
		case 2:
			c = (q[0] << 16) + (q[1] << 8);
			*p++ = alphabet[(c & 0x00FC0000) >> 18];
			*p++ = alphabet[(c & 0x0003F000) >> 12];
			*p++ = alphabet[(c & 0x00000FC0) >> 6];
			if (pad)
				*p++ = '=';
			break;
	}

	if (crLf && length % 3)
	{
		*p++ = '\r';
		*p++ = '\n';
	}
	*p = 0;

	return ret;
}

static char* base64_encode(const char* alphabet, const BYTE* data, size_t length, BOOL pad)
{
	return base64_encode_ex(alphabet, data, length, pad, FALSE, 64);
}

static int base64_decode_char(const char* alphabet, char c)
{
	char* p = NULL;

	if (c == '\0')
		return -1;

	if ((p = strchr(alphabet, c)))
		return p - alphabet;

	return -1;
}

static void* base64_decode(const char* alphabet, const char* s, size_t length, size_t* data_len,
                           BOOL pad)
{
	int n[4];
	BYTE* q;
	BYTE* data;
	size_t nBlocks, i, outputLen;
	int remainder = length % 4;

	if ((pad && remainder > 0) || (remainder == 1))
		return NULL;

	if (!pad && remainder)
		length += 4 - remainder;

	q = data = (BYTE*)malloc(length / 4 * 3 + 1);
	if (!q)
		return NULL;

	/* first treat complete blocks */
	nBlocks = (length / 4);
	outputLen = 0;

	if (nBlocks < 1)
	{
		free(data);
		return NULL;
	}

	for (i = 0; i < nBlocks - 1; i++, q += 3)
	{
		n[0] = base64_decode_char(alphabet, *s++);
		n[1] = base64_decode_char(alphabet, *s++);
		n[2] = base64_decode_char(alphabet, *s++);
		n[3] = base64_decode_char(alphabet, *s++);

		if ((n[0] == -1) || (n[1] == -1) || (n[2] == -1) || (n[3] == -1))
			goto out_free;

		q[0] = (n[0] << 2) + (n[1] >> 4);
		q[1] = ((n[1] & 15) << 4) + (n[2] >> 2);
		q[2] = ((n[2] & 3) << 6) + n[3];
		outputLen += 3;
	}

	/* treat last block */
	n[0] = base64_decode_char(alphabet, *s++);
	n[1] = base64_decode_char(alphabet, *s++);
	if ((n[0] == -1) || (n[1] == -1))
		goto out_free;

	n[2] = remainder == 2 ? -1 : base64_decode_char(alphabet, *s++);
	n[3] = remainder >= 2 ? -1 : base64_decode_char(alphabet, *s++);

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

	if (data_len)
		*data_len = outputLen;
	data[outputLen] = '\0';

	return data;
out_free:
	free(data);
	return NULL;
}

char* crypto_base64_encode_ex(const BYTE* data, size_t length, BOOL withCrLf)
{
	return base64_encode_ex(base64, data, length, TRUE, withCrLf, 64);
}

char* crypto_base64_encode(const BYTE* data, size_t length)
{
	return base64_encode(base64, data, length, TRUE);
}

void crypto_base64_decode(const char* enc_data, size_t length, BYTE** dec_data, size_t* res_length)
{
	*dec_data = base64_decode(base64, enc_data, length, res_length, TRUE);
}

char* crypto_base64url_encode(const BYTE* data, size_t length)
{
	return base64_encode(base64url, data, length, FALSE);
}

void crypto_base64url_decode(const char* enc_data, size_t length, BYTE** dec_data,
                             size_t* res_length)
{
	*dec_data = base64_decode(base64url, enc_data, length, res_length, FALSE);
}
