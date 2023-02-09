/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Cryptographic Abstraction Layer
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#include <errno.h>

#include <openssl/objects.h>

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/assert.h>

#include <freerdp/log.h>
#include <freerdp/crypto/crypto.h>

#include "crypto.h"
#include "privatekey.h"

#define TAG FREERDP_TAG("crypto")

static SSIZE_T crypto_rsa_common(const BYTE* input, size_t length, UINT32 key_length,
                                 const BYTE* modulus, const BYTE* exponent, size_t exponent_size,
                                 BYTE* output, size_t out_length)
{
	BN_CTX* ctx = NULL;
	int output_length = -1;
	BYTE* input_reverse = NULL;
	BYTE* modulus_reverse = NULL;
	BYTE* exponent_reverse = NULL;
	BIGNUM* mod = NULL;
	BIGNUM* exp = NULL;
	BIGNUM* x = NULL;
	BIGNUM* y = NULL;
	size_t bufferSize = 0;

	if (!input || !modulus || !exponent || !output)
		return -1;

	if ((size_t)exponent_size > INT_MAX / 2)
		return -1;

	if (key_length >= INT_MAX / 2 - exponent_size)
		return -1;

	bufferSize = 2ULL * key_length + exponent_size;
	if ((size_t)length > bufferSize)
		bufferSize = (size_t)length;

	input_reverse = (BYTE*)calloc(bufferSize, 1);

	if (!input_reverse)
		return -1;

	modulus_reverse = input_reverse + key_length;
	exponent_reverse = modulus_reverse + key_length;
	memcpy(modulus_reverse, modulus, key_length);
	crypto_reverse(modulus_reverse, key_length);
	memcpy(exponent_reverse, exponent, exponent_size);
	crypto_reverse(exponent_reverse, exponent_size);
	memcpy(input_reverse, input, length);
	crypto_reverse(input_reverse, length);

	if (!(ctx = BN_CTX_new()))
		goto fail;

	if (!(mod = BN_new()))
		goto fail;

	if (!(exp = BN_new()))
		goto fail;

	if (!(x = BN_new()))
		goto fail;

	if (!(y = BN_new()))
		goto fail;

	if (!BN_bin2bn(modulus_reverse, key_length, mod))
		goto fail;

	if (!BN_bin2bn(exponent_reverse, exponent_size, exp))
		goto fail;
	if (!BN_bin2bn(input_reverse, length, x))
		goto fail;
	if (BN_mod_exp(y, x, exp, mod, ctx) != 1)
		goto fail;
	output_length = BN_bn2bin(y, output);
	if (output_length < 0)
		goto fail;
	if ((size_t)output_length > out_length)
		goto fail;
	crypto_reverse(output, output_length);

	if ((size_t)output_length < key_length)
	{
		size_t diff = key_length - output_length;
		if ((size_t)output_length + diff > out_length)
			diff = out_length - (size_t)output_length;
		memset(output + output_length, 0, diff);
	}

fail:
	BN_free(y);
	BN_clear_free(x);
	BN_free(exp);
	BN_free(mod);
	BN_CTX_free(ctx);
	free(input_reverse);
	return output_length;
}

static SSIZE_T crypto_rsa_public(const BYTE* input, size_t length, const rdpCertInfo* cert,
                                 BYTE* output, size_t output_length)
{
	WINPR_ASSERT(cert);
	return crypto_rsa_common(input, length, cert->ModulusLength, cert->Modulus, cert->exponent,
	                         sizeof(cert->exponent), output, output_length);
}

static SSIZE_T crypto_rsa_private(const BYTE* input, size_t length, const rdpPrivateKey* key,
                                  BYTE* output, size_t output_length)
{
	WINPR_ASSERT(key);
	const rdpCertInfo* info = freerdp_key_get_info(key);
	WINPR_ASSERT(info);

	size_t PrivateExponentLength = 0;
	const BYTE* PrivateExponent = freerdp_key_get_exponent(key, &PrivateExponentLength);
	return crypto_rsa_common(input, length, info->ModulusLength, info->Modulus, PrivateExponent,
	                         PrivateExponentLength, output, output_length);
}

SSIZE_T crypto_rsa_public_encrypt(const BYTE* input, size_t length, const rdpCertInfo* cert,
                                  BYTE* output, size_t output_length)
{
	return crypto_rsa_public(input, length, cert, output, output_length);
}

SSIZE_T crypto_rsa_public_decrypt(const BYTE* input, size_t length, const rdpCertInfo* cert,
                                  BYTE* output, size_t output_length)
{
	return crypto_rsa_public(input, length, cert, output, output_length);
}

SSIZE_T crypto_rsa_private_encrypt(const BYTE* input, size_t length, const rdpPrivateKey* key,
                                   BYTE* output, size_t output_length)
{
	return crypto_rsa_private(input, length, key, output, output_length);
}

SSIZE_T crypto_rsa_private_decrypt(const BYTE* input, size_t length, const rdpPrivateKey* key,
                                   BYTE* output, size_t output_length)
{
	return crypto_rsa_private(input, length, key, output, output_length);
}

void crypto_reverse(BYTE* data, size_t length)
{
	size_t i, j;

	if (length < 1)
		return;

	for (i = 0, j = length - 1; i < j; i++, j--)
	{
		const BYTE temp = data[i];
		data[i] = data[j];
		data[j] = temp;
	}
}

char* crypto_read_pem(const char* filename, size_t* plength)
{
	char* pem = NULL;
	FILE* fp = NULL;

	WINPR_ASSERT(filename);

	if (plength)
		*plength = 0;

	fp = winpr_fopen(filename, "r");
	if (!fp)
		goto fail;
	const int rs = _fseeki64(fp, 0, SEEK_END);
	if (rs < 0)
		goto fail;
	const SSIZE_T size = _ftelli64(fp);
	if (size < 0)
		goto fail;
	const int rc = _fseeki64(fp, 0, SEEK_SET);
	if (rc < 0)
		goto fail;

	pem = calloc(size + 1, sizeof(char));
	if (!pem)
		goto fail;

	const size_t fr = fread(pem, (size_t)size, 1, fp);
	if (fr != 1)
		goto fail;

	if (plength)
		*plength = (size_t)size;
	fclose(fp);
	return pem;

fail:
{
	char buffer[8192] = { 0 };
	WLog_WARN(TAG, "Failed to read PEM from file '%s' [%s]", filename,
	          winpr_strerror(errno, buffer, sizeof(buffer)));
}
	fclose(fp);
	free(pem);
	return NULL;
}

BOOL crypto_write_pem(const char* filename, const char* pem, size_t length)
{
	WINPR_ASSERT(filename);
	WINPR_ASSERT(pem || (length == 0));

	WINPR_ASSERT(filename);
	WINPR_ASSERT(pem);

	const size_t size = strnlen(pem, length) + 1;
	size_t rc = 0;
	FILE* fp = winpr_fopen(filename, "w");
	if (!fp)
		goto fail;
	rc = fwrite(pem, 1, size, fp);
	fclose(fp);
fail:
	if (rc == 0)
	{
		char buffer[8192] = { 0 };
		WLog_WARN(TAG, "Failed to write PEM [%" PRIuz "] to file '%s' [%s]", length, filename,
		          winpr_strerror(errno, buffer, sizeof(buffer)));
	}
	return rc == size;
}
