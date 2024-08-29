/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Thincast Technologies GmbH
 * Copyright 2014 Hardening <contact@hardening-consulting.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <freerdp/crypto/crypto.h>

struct Encode64test
{
	const char* input;
	size_t len;
	const char* output;
};

static const struct Encode64test encodeTests_base64[] = {
	{ "\x00", 1, "AA==" },
	{ "\x00\x00", 2, "AAA=" },
	{ "\x00\x00\x00", 3, "AAAA" },
	{ "0123456", 7, "MDEyMzQ1Ng==" },
	{ "90123456", 8, "OTAxMjM0NTY=" },
	{ "890123456", 9, "ODkwMTIzNDU2" },
	{ "7890123456", 10, "Nzg5MDEyMzQ1Ng==" },

	{ NULL, -1, NULL }, /*  /!\ last one  /!\ */
};

static const struct Encode64test encodeTests_base64url[] = {
	{ "\x00", 1, "AA" },
	{ "\x00\x00", 2, "AAA" },
	{ "\x00\x00\x00", 3, "AAAA" },
	{ "01?34>6", 7, "MDE_MzQ-Ng" },
	{ "90123456", 8, "OTAxMjM0NTY" },
	{ "890123456", 9, "ODkwMTIzNDU2" },
	{ "78?01>3456", 10, "Nzg_MDE-MzQ1Ng" },

	{ NULL, -1, NULL }, /*  /!\ last one  /!\ */
};

int TestBase64(int argc, char* argv[])
{
	int testNb = 0;
	size_t outLen = 0;
	BYTE* decoded = NULL;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	testNb++;
	(void)fprintf(stderr, "%d:encode base64...", testNb);

	for (int i = 0; encodeTests_base64[i].input; i++)
	{
		char* encoded = crypto_base64_encode((const BYTE*)encodeTests_base64[i].input,
		                                     encodeTests_base64[i].len);

		if (strcmp(encodeTests_base64[i].output, encoded) != 0)
		{
			(void)fprintf(stderr, "ko, error for string %d\n", i);
			return -1;
		}

		free(encoded);
	}

	(void)fprintf(stderr, "ok\n");
	testNb++;
	(void)fprintf(stderr, "%d:encode base64url...", testNb);

	for (int i = 0; encodeTests_base64url[i].input; i++)
	{
		char* encoded = crypto_base64url_encode((const BYTE*)encodeTests_base64url[i].input,
		                                        encodeTests_base64url[i].len);

		if (strcmp(encodeTests_base64url[i].output, encoded) != 0)
		{
			(void)fprintf(stderr, "ko, error for string %d\n", i);
			return -1;
		}

		free(encoded);
	}

	(void)fprintf(stderr, "ok\n");
	testNb++;
	(void)fprintf(stderr, "%d:decode base64...", testNb);

	for (int i = 0; encodeTests_base64[i].input; i++)
	{
		crypto_base64_decode(encodeTests_base64[i].output, strlen(encodeTests_base64[i].output),
		                     &decoded, &outLen);

		if (!decoded || (outLen != encodeTests_base64[i].len) ||
		    memcmp(encodeTests_base64[i].input, decoded, outLen) != 0)
		{
			(void)fprintf(stderr, "ko, error for string %d\n", i);
			return -1;
		}

		free(decoded);
	}

	(void)fprintf(stderr, "ok\n");
	testNb++;
	(void)fprintf(stderr, "%d:decode base64url...", testNb);

	for (int i = 0; encodeTests_base64url[i].input; i++)
	{
		crypto_base64url_decode(encodeTests_base64url[i].output,
		                        strlen(encodeTests_base64url[i].output), &decoded, &outLen);

		if (!decoded || (outLen != encodeTests_base64url[i].len) ||
		    memcmp(encodeTests_base64url[i].input, decoded, outLen) != 0)
		{
			(void)fprintf(stderr, "ko, error for string %d\n", i);
			return -1;
		}

		free(decoded);
	}

	(void)fprintf(stderr, "ok\n");
	testNb++;
	(void)fprintf(stderr, "%d:decode base64 errors...", testNb);
	crypto_base64_decode("000", 3, &decoded, &outLen);

	if (decoded)
	{
		(void)fprintf(stderr, "ko, badly padded string\n");
		return -1;
	}

	crypto_base64_decode("0=00", 4, &decoded, &outLen);

	if (decoded)
	{
		(void)fprintf(stderr, "ko, = in a wrong place\n");
		return -1;
	}

	crypto_base64_decode("00=0", 4, &decoded, &outLen);

	if (decoded)
	{
		(void)fprintf(stderr, "ko, = in a wrong place\n");
		return -1;
	}
	(void)fprintf(stderr, "ok\n");
	testNb++;

	/* test the encode_ex version that will add \r\n */
	(void)fprintf(stderr, "%d:encode base64 with crLf...", testNb);
	const char* longStr = "01234567890123456789012345678901234567890123456789";
	const char* longStrExpected =
	    "MDEyMzQ1Njc4OTAxMjM0NTY3ODkwMTIzNDU2Nzg5MDEyMzQ1Njc4OTAxMjM0NTY3\r\nODk=\r\n";

	char* encoded = crypto_base64_encode_ex((const BYTE*)longStr, strlen(longStr), TRUE);
	if (!encoded || strcmp(encoded, longStrExpected) != 0)
	{
		(void)fprintf(stderr, "problem with encode with CRLF\n");
		return -1;
	}
	free(encoded);
	(void)fprintf(stderr, "ok\n");

	return 0;
}
