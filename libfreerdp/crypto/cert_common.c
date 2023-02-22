/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Certificate Handling
 *
 * Copyright 2011 Jiten Pathy
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <winpr/assert.h>
#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/crypto.h>

#include <openssl/pem.h>
#include <openssl/rsa.h>

#include "cert_common.h"
#include "crypto.h"
#include "opensslcompat.h"

#define TAG FREERDP_TAG("core")

static BOOL cert_info_allocate(rdpCertInfo* info, size_t size);

BOOL read_bignum(BYTE** dst, UINT32* length, const BIGNUM* num, BOOL alloc)
{
	WINPR_ASSERT(dst);
	WINPR_ASSERT(length);
	WINPR_ASSERT(num);

	if (alloc)
	{
		*dst = NULL;
		*length = 0;
	}

	const int len = BN_num_bytes(num);
	if (len < 0)
		return FALSE;

	if (!alloc)
	{
		if (*length < (UINT32)len)
			return FALSE;
	}

	if (len > 0)
	{
		if (alloc)
		{
			*dst = malloc((size_t)len);
			if (!*dst)
				return FALSE;
		}
		BN_bn2bin(num, *dst);
		crypto_reverse(*dst, (size_t)len);
		*length = (UINT32)len;
	}

	return TRUE;
}

BOOL cert_info_create(rdpCertInfo* dst, const BIGNUM* rsa, const BIGNUM* rsa_e)
{
	const rdpCertInfo empty = { 0 };

	WINPR_ASSERT(dst);
	WINPR_ASSERT(rsa);

	*dst = empty;

	if (!read_bignum(&dst->Modulus, &dst->ModulusLength, rsa, TRUE))
		goto fail;

	UINT32 len = sizeof(dst->exponent);
	BYTE* ptr = &dst->exponent[0];
	if (!read_bignum(&ptr, &len, rsa_e, FALSE))
		goto fail;
	return TRUE;

fail:
	cert_info_free(dst);
	return FALSE;
}

BOOL cert_info_clone(rdpCertInfo* dst, const rdpCertInfo* src)
{
	WINPR_ASSERT(dst);
	WINPR_ASSERT(src);

	*dst = *src;

	dst->Modulus = NULL;
	dst->ModulusLength = 0;
	if (src->ModulusLength > 0)
	{
		dst->Modulus = malloc(src->ModulusLength);
		if (!dst->Modulus)
			return FALSE;
		memcpy(dst->Modulus, src->Modulus, src->ModulusLength);
		dst->ModulusLength = src->ModulusLength;
	}
	return TRUE;
}

void cert_info_free(rdpCertInfo* info)
{
	WINPR_ASSERT(info);
	free(info->Modulus);
	info->ModulusLength = 0;
	info->Modulus = NULL;
}

BOOL cert_info_allocate(rdpCertInfo* info, size_t size)
{
	WINPR_ASSERT(info);
	cert_info_free(info);

	info->Modulus = (BYTE*)malloc(size);

	if (!info->Modulus && (size > 0))
		return FALSE;
	info->ModulusLength = (UINT32)size;
	return TRUE;
}

BOOL cert_info_read_modulus(rdpCertInfo* info, size_t size, wStream* s)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, size))
		return FALSE;
	if (size > UINT32_MAX)
		return FALSE;
	if (!cert_info_allocate(info, size))
		return FALSE;
	Stream_Read(s, info->Modulus, info->ModulusLength);
	return TRUE;
}

BOOL cert_info_read_exponent(rdpCertInfo* info, size_t size, wStream* s)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, size))
		return FALSE;
	if (size > 4)
		return FALSE;
	if (!info->Modulus || (info->ModulusLength == 0))
		return FALSE;
	Stream_Read(s, &info->exponent[4 - size], size);
	crypto_reverse(info->Modulus, info->ModulusLength);
	crypto_reverse(info->exponent, 4);
	return TRUE;
}

X509* x509_from_rsa(const RSA* rsa)
{
	EVP_PKEY* pubkey = NULL;
	X509* x509 = NULL;
	BIO* bio = BIO_new(BIO_s_secmem());
	if (!bio)
		return NULL;

	const int rc = PEM_write_bio_RSA_PUBKEY(bio, (RSA*)rsa);
	if (rc != 1)
		goto fail;

	pubkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
	if (!pubkey)
		goto fail;

	x509 = X509_new();
	if (!x509)
		goto fail;

	const int res = X509_set_pubkey(x509, pubkey);
	if (res != 1)
	{
		X509_free(x509);
		x509 = NULL;
		goto fail;
	}
fail:
	BIO_free_all(bio);
	EVP_PKEY_free(pubkey);
	return x509;
}
