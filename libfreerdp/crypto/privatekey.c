/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Private key Handling
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
#include <openssl/bn.h>

#include "privatekey.h"
#include "cert_common.h"

#include <freerdp/crypto/privatekey.h>

#include <openssl/evp.h>

#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
#include <openssl/core_names.h>
#endif

#include "x509_utils.h"
#include "crypto.h"
#include "opensslcompat.h"

#define TAG FREERDP_TAG("crypto")

struct rdp_private_key
{
	EVP_PKEY* evp;

	rdpCertInfo cert;
	BYTE* PrivateExponent;
	DWORD PrivateExponentLength;
};

/*
 * Terminal Services Signing Keys.
 * Yes, Terminal Services Private Key is publicly available.
 */

static BYTE tssk_modulus[] = { 0x3d, 0x3a, 0x5e, 0xbd, 0x72, 0x43, 0x3e, 0xc9, 0x4d, 0xbb, 0xc1,
	                           0x1e, 0x4a, 0xba, 0x5f, 0xcb, 0x3e, 0x88, 0x20, 0x87, 0xef, 0xf5,
	                           0xc1, 0xe2, 0xd7, 0xb7, 0x6b, 0x9a, 0xf2, 0x52, 0x45, 0x95, 0xce,
	                           0x63, 0x65, 0x6b, 0x58, 0x3a, 0xfe, 0xef, 0x7c, 0xe7, 0xbf, 0xfe,
	                           0x3d, 0xf6, 0x5c, 0x7d, 0x6c, 0x5e, 0x06, 0x09, 0x1a, 0xf5, 0x61,
	                           0xbb, 0x20, 0x93, 0x09, 0x5f, 0x05, 0x6d, 0xea, 0x87 };

static BYTE tssk_privateExponent[] = {
	0x87, 0xa7, 0x19, 0x32, 0xda, 0x11, 0x87, 0x55, 0x58, 0x00, 0x16, 0x16, 0x25, 0x65, 0x68, 0xf8,
	0x24, 0x3e, 0xe6, 0xfa, 0xe9, 0x67, 0x49, 0x94, 0xcf, 0x92, 0xcc, 0x33, 0x99, 0xe8, 0x08, 0x60,
	0x17, 0x9a, 0x12, 0x9f, 0x24, 0xdd, 0xb1, 0x24, 0x99, 0xc7, 0x3a, 0xb8, 0x0a, 0x7b, 0x0d, 0xdd,
	0x35, 0x07, 0x79, 0x17, 0x0b, 0x51, 0x9b, 0xb3, 0xc7, 0x10, 0x01, 0x13, 0xe7, 0x3f, 0xf3, 0x5f
};

static const rdpPrivateKey tssk = { .PrivateExponent = tssk_privateExponent,
	                                .PrivateExponentLength = sizeof(tssk_privateExponent),
	                                .cert = { .Modulus = tssk_modulus,
	                                          .ModulusLength = sizeof(tssk_modulus) } };
const rdpPrivateKey* priv_key_tssk = &tssk;

#if !defined(OPENSSL_VERSION_MAJOR) || (OPENSSL_VERSION_MAJOR < 3)
static RSA* evp_pkey_to_rsa(const rdpPrivateKey* key)
{
	if (!freerdp_key_is_rsa(key))
	{
		WLog_WARN(TAG, "Key is no RSA key");
		return NULL;
	}

	RSA* rsa = NULL;
	BIO* bio = BIO_new(
#if defined(LIBRESSL_VERSION_NUMBER)
	    BIO_s_mem()
#else
	    BIO_s_secmem()
#endif
	);
	if (!bio)
		return NULL;
	const int rc = PEM_write_bio_PrivateKey(bio, key->evp, NULL, NULL, 0, NULL, NULL);
	if (rc != 1)
		goto fail;
	rsa = PEM_read_bio_RSAPrivateKey(bio, NULL, NULL, NULL);
fail:
	BIO_free_all(bio);
	return rsa;
}
#endif

static EVP_PKEY* evp_pkey_utils_from_pem(const char* data, size_t len, BOOL fromFile)
{
	EVP_PKEY* evp = NULL;
	BIO* bio = NULL;
	if (fromFile)
		bio = BIO_new_file(data, "rb");
	else
	{
		if (len > INT_MAX)
			return NULL;
		bio = BIO_new_mem_buf(data, (int)len);
	}

	if (!bio)
	{
		WLog_ERR(TAG, "BIO_new failed for private key");
		return NULL;
	}

	evp = PEM_read_bio_PrivateKey(bio, NULL, NULL, 0);
	BIO_free_all(bio);
	if (!evp)
		WLog_ERR(TAG, "PEM_read_bio_PrivateKey returned NULL [input length %" PRIuz "]", len);

	return evp;
}

static BOOL key_read_private(rdpPrivateKey* key)
{
	BOOL rc = FALSE;

	WINPR_ASSERT(key);
	WINPR_ASSERT(key->evp);

	/* The key is not an RSA key, that means we just return success. */
	if (!freerdp_key_is_rsa(key))
		return TRUE;

#if !defined(OPENSSL_VERSION_MAJOR) || (OPENSSL_VERSION_MAJOR < 3)
	RSA* rsa = evp_pkey_to_rsa(key);
	if (!rsa)
	{
		char ebuffer[256] = { 0 };
		WLog_ERR(TAG, "unable to load RSA key: %s.",
		         winpr_strerror(errno, ebuffer, sizeof(ebuffer)));
		goto fail;
	}

	switch (RSA_check_key(rsa))
	{
		case 0:
			WLog_ERR(TAG, "invalid RSA key");
			goto fail;

		case 1:
			/* Valid key. */
			break;

		default:
		{
			char ebuffer[256] = { 0 };
			WLog_ERR(TAG, "unexpected error when checking RSA key: %s.",
			         winpr_strerror(errno, ebuffer, sizeof(ebuffer)));
			goto fail;
		}
	}

	const BIGNUM* rsa_e = NULL;
	const BIGNUM* rsa_n = NULL;
	const BIGNUM* rsa_d = NULL;

	RSA_get0_key(rsa, &rsa_n, &rsa_e, &rsa_d);
#else
	BIGNUM* rsa_e = NULL;
	BIGNUM* rsa_n = NULL;
	BIGNUM* rsa_d = NULL;

	if (!EVP_PKEY_get_bn_param(key->evp, OSSL_PKEY_PARAM_RSA_N, &rsa_n))
		goto fail;
	if (!EVP_PKEY_get_bn_param(key->evp, OSSL_PKEY_PARAM_RSA_E, &rsa_e))
		goto fail;
	if (!EVP_PKEY_get_bn_param(key->evp, OSSL_PKEY_PARAM_RSA_D, &rsa_d))
		goto fail;
#endif
	if (BN_num_bytes(rsa_e) > 4)
	{
		WLog_ERR(TAG, "RSA public exponent too large");
		goto fail;
	}

	if (!read_bignum(&key->PrivateExponent, &key->PrivateExponentLength, rsa_d, TRUE))
		goto fail;

	if (!cert_info_create(&key->cert, rsa_n, rsa_e))
		goto fail;
	rc = TRUE;
fail:
#if !defined(OPENSSL_VERSION_MAJOR) || (OPENSSL_VERSION_MAJOR < 3)
	RSA_free(rsa);
#else
	BN_free(rsa_d);
	BN_free(rsa_e);
	BN_free(rsa_n);
#endif
	return rc;
}

rdpPrivateKey* freerdp_key_new_from_pem(const char* pem)
{
	rdpPrivateKey* key = freerdp_key_new();
	if (!key || !pem)
		goto fail;
	key->evp = evp_pkey_utils_from_pem(pem, strlen(pem), FALSE);
	if (!key->evp)
		goto fail;
	if (!key_read_private(key))
		goto fail;
	return key;
fail:
	freerdp_key_free(key);
	return NULL;
}

rdpPrivateKey* freerdp_key_new_from_file(const char* keyfile)
{

	rdpPrivateKey* key = freerdp_key_new();
	if (!key || !keyfile)
		goto fail;

	key->evp = evp_pkey_utils_from_pem(keyfile, strlen(keyfile), TRUE);
	if (!key->evp)
		goto fail;
	if (!key_read_private(key))
		goto fail;
	return key;
fail:
	freerdp_key_free(key);
	return NULL;
}

rdpPrivateKey* freerdp_key_new(void)
{
	return calloc(1, sizeof(rdpPrivateKey));
}

rdpPrivateKey* freerdp_key_clone(const rdpPrivateKey* key)
{
	if (!key)
		return NULL;

	rdpPrivateKey* _key = (rdpPrivateKey*)calloc(1, sizeof(rdpPrivateKey));

	if (!_key)
		return NULL;

	if (key->evp)
	{
		_key->evp = key->evp;
		if (!_key->evp)
			goto out_fail;
		EVP_PKEY_up_ref(_key->evp);
	}

	if (key->PrivateExponent)
	{
		_key->PrivateExponent = (BYTE*)malloc(key->PrivateExponentLength);

		if (!_key->PrivateExponent)
			goto out_fail;

		CopyMemory(_key->PrivateExponent, key->PrivateExponent, key->PrivateExponentLength);
		_key->PrivateExponentLength = key->PrivateExponentLength;
	}

	if (!cert_info_clone(&_key->cert, &key->cert))
		goto out_fail;

	return _key;
out_fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	freerdp_key_free(_key);
	WINPR_PRAGMA_DIAG_POP
	return NULL;
}

void freerdp_key_free(rdpPrivateKey* key)
{
	if (!key)
		return;

	EVP_PKEY_free(key->evp);
	if (key->PrivateExponent)
		memset(key->PrivateExponent, 0, key->PrivateExponentLength);
	free(key->PrivateExponent);
	cert_info_free(&key->cert);
	free(key);
}

const rdpCertInfo* freerdp_key_get_info(const rdpPrivateKey* key)
{
	WINPR_ASSERT(key);
	if (!freerdp_key_is_rsa(key))
		return NULL;
	return &key->cert;
}

const BYTE* freerdp_key_get_exponent(const rdpPrivateKey* key, size_t* plength)
{
	WINPR_ASSERT(key);
	if (!freerdp_key_is_rsa(key))
	{
		if (plength)
			*plength = 0;
		return NULL;
	}

	if (plength)
		*plength = key->PrivateExponentLength;
	return key->PrivateExponent;
}

EVP_PKEY* freerdp_key_get_evp_pkey(const rdpPrivateKey* key)
{
	WINPR_ASSERT(key);

	EVP_PKEY* evp = key->evp;
	WINPR_ASSERT(evp);
	EVP_PKEY_up_ref(evp);
	return evp;
}

BOOL freerdp_key_is_rsa(const rdpPrivateKey* key)
{
	WINPR_ASSERT(key);
	if (key == priv_key_tssk)
		return TRUE;

	WINPR_ASSERT(key->evp);
	return (EVP_PKEY_id(key->evp) == EVP_PKEY_RSA);
}

size_t freerdp_key_get_bits(const rdpPrivateKey* key)
{
	int rc = -1;
#if !defined(OPENSSL_VERSION_MAJOR) || (OPENSSL_VERSION_MAJOR < 3)
	RSA* rsa = evp_pkey_to_rsa(key);
	if (rsa)
	{
		rc = RSA_bits(rsa);
		RSA_free(rsa);
	}
#else
	rc = EVP_PKEY_get_bits(key->evp);
#endif

	return rc;
}

BOOL freerdp_key_generate(rdpPrivateKey* key, size_t key_length)
{
	BOOL rc = FALSE;

#if !defined(OPENSSL_VERSION_MAJOR) || (OPENSSL_VERSION_MAJOR < 3)
	RSA* rsa = NULL;
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
	rsa = RSA_generate_key(key_length, RSA_F4, NULL, NULL);
#else
	{
		BIGNUM* bn = BN_secure_new();

		if (!bn)
			return FALSE;

		rsa = RSA_new();

		if (!rsa)
		{
			BN_clear_free(bn);
			return FALSE;
		}

		BN_set_word(bn, RSA_F4);
		const int res = RSA_generate_key_ex(rsa, key_length, bn, NULL);
		BN_clear_free(bn);

		if (res != 1)
			return FALSE;
	}
#endif

	EVP_PKEY_free(key->evp);
	key->evp = EVP_PKEY_new();

	if (!EVP_PKEY_assign_RSA(key->evp, rsa))
	{
		EVP_PKEY_free(key->evp);
		key->evp = NULL;
		RSA_free(rsa);
		return FALSE;
	}

	rc = TRUE;
#else
	EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
	if (!pctx)
		return FALSE;

	if (EVP_PKEY_keygen_init(pctx) != 1)
		goto fail;

	if (key_length > INT_MAX)
		goto fail;

	if (EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, (int)key_length) != 1)
		goto fail;

	EVP_PKEY_free(key->evp);
	key->evp = NULL;

	if (EVP_PKEY_generate(pctx, &key->evp) != 1)
		goto fail;

	rc = TRUE;
fail:
	EVP_PKEY_CTX_free(pctx);
#endif
	return rc;
}

BYTE* freerdp_key_get_param(const rdpPrivateKey* key, enum FREERDP_KEY_PARAM param, size_t* plength)
{
	BYTE* buf = NULL;

	WINPR_ASSERT(key);
	WINPR_ASSERT(plength);

	*plength = 0;

	BIGNUM* bn = NULL;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L

	const char* pk = NULL;
	switch (param)
	{
		case FREERDP_KEY_PARAM_RSA_D:
			pk = OSSL_PKEY_PARAM_RSA_D;
			break;
		case FREERDP_KEY_PARAM_RSA_E:
			pk = OSSL_PKEY_PARAM_RSA_E;
			break;
		case FREERDP_KEY_PARAM_RSA_N:
			pk = OSSL_PKEY_PARAM_RSA_N;
			break;
		default:
			return NULL;
	}

	if (!EVP_PKEY_get_bn_param(key->evp, pk, &bn))
		return NULL;
#else
	{
		const RSA* rsa = EVP_PKEY_get0_RSA(key->evp);
		if (!rsa)
			return NULL;

		const BIGNUM* cbn = NULL;
		switch (param)
		{
			case FREERDP_KEY_PARAM_RSA_D:
#if OPENSSL_VERSION_NUMBER >= 0x10101007L
				cbn = RSA_get0_d(rsa);
#endif
				break;
			case FREERDP_KEY_PARAM_RSA_E:
#if OPENSSL_VERSION_NUMBER >= 0x10101007L
				cbn = RSA_get0_e(rsa);
#endif
				break;
			case FREERDP_KEY_PARAM_RSA_N:
#if OPENSSL_VERSION_NUMBER >= 0x10101007L
				cbn = RSA_get0_n(rsa);
#endif
				break;
			default:
				return NULL;
		}
		if (!cbn)
			return NULL;
		bn = BN_dup(cbn);
		if (!bn)
			return NULL;
	}
#endif

	const int length = BN_num_bytes(bn);
	if (length < 0)
		goto fail;

	const size_t alloc_size = (size_t)length + 1ull;
	buf = calloc(alloc_size, sizeof(BYTE));
	if (!buf)
		goto fail;

	const int bnlen = BN_bn2bin(bn, buf);
	if (bnlen != length)
	{
		free(buf);
		buf = NULL;
	}
	else
		*plength = length;

fail:
	BN_free(bn);
	return buf;
}

WINPR_DIGEST_CTX* freerdp_key_digest_sign(rdpPrivateKey* key, WINPR_MD_TYPE digest)
{
	WINPR_DIGEST_CTX* md_ctx = winpr_Digest_New();
	if (!md_ctx)
		return NULL;

	if (!winpr_DigestSign_Init(md_ctx, digest, key->evp))
	{
		winpr_Digest_Free(md_ctx);
		return NULL;
	}
	return md_ctx;
}
