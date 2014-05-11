/**
 * Copyright © 2014 Thincast Technologies GmbH
 * Copyright © 2014 Hardening <contact@hardening-consulting.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <freerdp/crypto/crypto.h>

struct Encode64test {
	const char *input;
	int len;
	const char *output;
};

struct Encode64test encodeTests[] = {
		{"\x00", 			1, "AA=="},
		{"\x00\x00", 		2, "AAA="},
		{"\x00\x00\x00", 	3, "AAAA"},
		{"0123456",			7, "MDEyMzQ1Ng=="},
		{"90123456",		8, "OTAxMjM0NTY="},
		{"890123456",		9, "ODkwMTIzNDU2"},
		{"7890123456",		10, "Nzg5MDEyMzQ1Ng=="},

		{NULL, -1, NULL},  /*  /!\ last one  /!\ */
};


int TestBase64(int argc, char* argv[])
{
	int i, testNb = 0;
	int outLen;
	BYTE *decoded;

	testNb++;
	fprintf(stderr, "%d:encode base64...", testNb);

	for (i = 0; encodeTests[i].input; i++)
	{
		char *encoded = crypto_base64_encode((const BYTE *)encodeTests[i].input, encodeTests[i].len);

		if (strcmp(encodeTests[i].output, encoded))
		{
			fprintf(stderr, "ko, error for string %d\n", i);
			return -1;
		}

		free(encoded);
	}
	fprintf(stderr, "ok\n");

	testNb++;
	fprintf(stderr, "%d:decode base64...", testNb);
	for (i = 0; encodeTests[i].input; i++)
	{
		crypto_base64_decode(encodeTests[i].output, strlen(encodeTests[i].output), &decoded, &outLen);

		if (!decoded || (outLen != encodeTests[i].len) || memcmp(encodeTests[i].input, decoded, outLen))
		{
			fprintf(stderr, "ko, error for string %d\n", i);
			return -1;
		}

		free(decoded);
	}
	fprintf(stderr, "ok\n");

	testNb++;
	fprintf(stderr, "%d:decode base64 errors...", testNb);
	crypto_base64_decode("000", 3, &decoded, &outLen);
	if (decoded)
	{
		fprintf(stderr, "ko, badly padded string\n");
		return -1;
	}

	crypto_base64_decode("0=00", 4, &decoded, &outLen);
	if (decoded)
	{
		fprintf(stderr, "ko, = in a wrong place\n");
		return -1;
	}

	crypto_base64_decode("00=0", 4, &decoded, &outLen);
	if (decoded)
	{
		fprintf(stderr, "ko, = in a wrong place\n");
		return -1;
	}
	fprintf(stderr, "ok\n");


	return 0;
}
