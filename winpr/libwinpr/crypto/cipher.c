/**
 * WinPR: Windows Portable Runtime
 *
 * Copyright 2015 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/config.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/crypto.h>

#include "../log.h"
#define TAG WINPR_TAG("crypto.cipher")

#if defined(WITH_INTERNAL_RC4)
#include "rc4.h"
#endif

#ifdef WITH_OPENSSL
#include <openssl/aes.h>
#include <openssl/rc4.h>
#include <openssl/des.h>
#include <openssl/evp.h>
#endif

#ifdef WITH_MBEDTLS
#include <mbedtls/md.h>
#include <mbedtls/aes.h>
#include <mbedtls/des.h>
#include <mbedtls/cipher.h>
#if MBEDTLS_VERSION_MAJOR < 3
#define mbedtls_cipher_info_get_iv_size(_info) (_info->iv_size)
#define mbedtls_cipher_info_get_key_bitlen(_info) (_info->key_bitlen)
#endif
#endif

/**
 * RC4
 */

struct winpr_rc4_ctx_private_st
{
#if defined(WITH_INTERNAL_RC4)
	winpr_int_RC4_CTX* ictx;
#else
#if defined(WITH_OPENSSL)
	EVP_CIPHER_CTX* ctx;
#endif
#endif
};

static WINPR_RC4_CTX* winpr_RC4_New_Internal(const BYTE* key, size_t keylen, BOOL override_fips)
{
	if (!key || (keylen == 0))
		return NULL;

	WINPR_RC4_CTX* ctx = (WINPR_RC4_CTX*)calloc(1, sizeof(WINPR_RC4_CTX));
	if (!ctx)
		return NULL;

#if defined(WITH_INTERNAL_RC4)
	WINPR_UNUSED(override_fips);
	ctx->ictx = winpr_int_rc4_new(key, keylen);
	if (!ctx->ictx)
		goto fail;
#elif defined(WITH_OPENSSL)
	const EVP_CIPHER* evp = NULL;

	if (keylen > INT_MAX)
		goto fail;

	ctx->ctx = EVP_CIPHER_CTX_new();
	if (!ctx->ctx)
		goto fail;

	evp = EVP_rc4();

	if (!evp)
		goto fail;

	EVP_CIPHER_CTX_reset(ctx->ctx);
	if (EVP_EncryptInit_ex(ctx->ctx, evp, NULL, NULL, NULL) != 1)
		goto fail;

		/* EVP_CIPH_FLAG_NON_FIPS_ALLOW does not exist before openssl 1.0.1 */
#if !(OPENSSL_VERSION_NUMBER < 0x10001000L)

	if (override_fips == TRUE)
		EVP_CIPHER_CTX_set_flags(ctx->ctx, EVP_CIPH_FLAG_NON_FIPS_ALLOW);

#endif
	EVP_CIPHER_CTX_set_key_length(ctx->ctx, (int)keylen);
	if (EVP_EncryptInit_ex(ctx->ctx, NULL, NULL, key, NULL) != 1)
		goto fail;
#endif
	return ctx;

fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC

	winpr_RC4_Free(ctx);
	WINPR_PRAGMA_DIAG_POP
	return NULL;
}

WINPR_RC4_CTX* winpr_RC4_New_Allow_FIPS(const void* key, size_t keylen)
{
	return winpr_RC4_New_Internal(key, keylen, TRUE);
}

WINPR_RC4_CTX* winpr_RC4_New(const void* key, size_t keylen)
{
	return winpr_RC4_New_Internal(key, keylen, FALSE);
}

BOOL winpr_RC4_Update(WINPR_RC4_CTX* ctx, size_t length, const void* input, void* output)
{
	WINPR_ASSERT(ctx);

#if defined(WITH_INTERNAL_RC4)
	return winpr_int_rc4_update(ctx->ictx, length, input, output);
#elif defined(WITH_OPENSSL)
	WINPR_ASSERT(ctx->ctx);
	int outputLength = 0;
	if (length > INT_MAX)
		return FALSE;

	WINPR_ASSERT(ctx);
	if (EVP_CipherUpdate(ctx->ctx, output, &outputLength, input, (int)length) != 1)
		return FALSE;
	return TRUE;
#endif
	return FALSE;
}

void winpr_RC4_Free(WINPR_RC4_CTX* ctx)
{
	if (!ctx)
		return;

#if defined(WITH_INTERNAL_RC4)
	winpr_int_rc4_free(ctx->ictx);
#elif defined(WITH_OPENSSL)
	EVP_CIPHER_CTX_free(ctx->ctx);
#endif
	free(ctx);
}

/**
 * Generic Cipher API
 */

#ifdef WITH_OPENSSL
extern const EVP_MD* winpr_openssl_get_evp_md(WINPR_MD_TYPE md);
#endif

#ifdef WITH_MBEDTLS
extern mbedtls_md_type_t winpr_mbedtls_get_md_type(int md);
#endif

#if defined(WITH_OPENSSL)
static const EVP_CIPHER* winpr_openssl_get_evp_cipher(int cipher)
{
	const EVP_CIPHER* evp = NULL;

	switch (cipher)
	{
		case WINPR_CIPHER_NULL:
			evp = EVP_enc_null();
			break;

		case WINPR_CIPHER_AES_128_ECB:
			evp = EVP_get_cipherbyname("aes-128-ecb");
			break;

		case WINPR_CIPHER_AES_192_ECB:
			evp = EVP_get_cipherbyname("aes-192-ecb");
			break;

		case WINPR_CIPHER_AES_256_ECB:
			evp = EVP_get_cipherbyname("aes-256-ecb");
			break;

		case WINPR_CIPHER_AES_128_CBC:
			evp = EVP_get_cipherbyname("aes-128-cbc");
			break;

		case WINPR_CIPHER_AES_192_CBC:
			evp = EVP_get_cipherbyname("aes-192-cbc");
			break;

		case WINPR_CIPHER_AES_256_CBC:
			evp = EVP_get_cipherbyname("aes-256-cbc");
			break;

		case WINPR_CIPHER_AES_128_CFB128:
			evp = EVP_get_cipherbyname("aes-128-cfb128");
			break;

		case WINPR_CIPHER_AES_192_CFB128:
			evp = EVP_get_cipherbyname("aes-192-cfb128");
			break;

		case WINPR_CIPHER_AES_256_CFB128:
			evp = EVP_get_cipherbyname("aes-256-cfb128");
			break;

		case WINPR_CIPHER_AES_128_CTR:
			evp = EVP_get_cipherbyname("aes-128-ctr");
			break;

		case WINPR_CIPHER_AES_192_CTR:
			evp = EVP_get_cipherbyname("aes-192-ctr");
			break;

		case WINPR_CIPHER_AES_256_CTR:
			evp = EVP_get_cipherbyname("aes-256-ctr");
			break;

		case WINPR_CIPHER_AES_128_GCM:
			evp = EVP_get_cipherbyname("aes-128-gcm");
			break;

		case WINPR_CIPHER_AES_192_GCM:
			evp = EVP_get_cipherbyname("aes-192-gcm");
			break;

		case WINPR_CIPHER_AES_256_GCM:
			evp = EVP_get_cipherbyname("aes-256-gcm");
			break;

		case WINPR_CIPHER_AES_128_CCM:
			evp = EVP_get_cipherbyname("aes-128-ccm");
			break;

		case WINPR_CIPHER_AES_192_CCM:
			evp = EVP_get_cipherbyname("aes-192-ccm");
			break;

		case WINPR_CIPHER_AES_256_CCM:
			evp = EVP_get_cipherbyname("aes-256-ccm");
			break;

		case WINPR_CIPHER_CAMELLIA_128_ECB:
			evp = EVP_get_cipherbyname("camellia-128-ecb");
			break;

		case WINPR_CIPHER_CAMELLIA_192_ECB:
			evp = EVP_get_cipherbyname("camellia-192-ecb");
			break;

		case WINPR_CIPHER_CAMELLIA_256_ECB:
			evp = EVP_get_cipherbyname("camellia-256-ecb");
			break;

		case WINPR_CIPHER_CAMELLIA_128_CBC:
			evp = EVP_get_cipherbyname("camellia-128-cbc");
			break;

		case WINPR_CIPHER_CAMELLIA_192_CBC:
			evp = EVP_get_cipherbyname("camellia-192-cbc");
			break;

		case WINPR_CIPHER_CAMELLIA_256_CBC:
			evp = EVP_get_cipherbyname("camellia-256-cbc");
			break;

		case WINPR_CIPHER_CAMELLIA_128_CFB128:
			evp = EVP_get_cipherbyname("camellia-128-cfb128");
			break;

		case WINPR_CIPHER_CAMELLIA_192_CFB128:
			evp = EVP_get_cipherbyname("camellia-192-cfb128");
			break;

		case WINPR_CIPHER_CAMELLIA_256_CFB128:
			evp = EVP_get_cipherbyname("camellia-256-cfb128");
			break;

		case WINPR_CIPHER_CAMELLIA_128_CTR:
			evp = EVP_get_cipherbyname("camellia-128-ctr");
			break;

		case WINPR_CIPHER_CAMELLIA_192_CTR:
			evp = EVP_get_cipherbyname("camellia-192-ctr");
			break;

		case WINPR_CIPHER_CAMELLIA_256_CTR:
			evp = EVP_get_cipherbyname("camellia-256-ctr");
			break;

		case WINPR_CIPHER_CAMELLIA_128_GCM:
			evp = EVP_get_cipherbyname("camellia-128-gcm");
			break;

		case WINPR_CIPHER_CAMELLIA_192_GCM:
			evp = EVP_get_cipherbyname("camellia-192-gcm");
			break;

		case WINPR_CIPHER_CAMELLIA_256_GCM:
			evp = EVP_get_cipherbyname("camellia-256-gcm");
			break;

		case WINPR_CIPHER_CAMELLIA_128_CCM:
			evp = EVP_get_cipherbyname("camellia-128-ccm");
			break;

		case WINPR_CIPHER_CAMELLIA_192_CCM:
			evp = EVP_get_cipherbyname("camellia-192-gcm");
			break;

		case WINPR_CIPHER_CAMELLIA_256_CCM:
			evp = EVP_get_cipherbyname("camellia-256-gcm");
			break;

		case WINPR_CIPHER_DES_ECB:
			evp = EVP_get_cipherbyname("des-ecb");
			break;

		case WINPR_CIPHER_DES_CBC:
			evp = EVP_get_cipherbyname("des-cbc");
			break;

		case WINPR_CIPHER_DES_EDE_ECB:
			evp = EVP_get_cipherbyname("des-ede-ecb");
			break;

		case WINPR_CIPHER_DES_EDE_CBC:
			evp = EVP_get_cipherbyname("des-ede-cbc");
			break;

		case WINPR_CIPHER_DES_EDE3_ECB:
			evp = EVP_get_cipherbyname("des-ede3-ecb");
			break;

		case WINPR_CIPHER_DES_EDE3_CBC:
			evp = EVP_get_cipherbyname("des-ede3-cbc");
			break;

		case WINPR_CIPHER_ARC4_128:
			evp = EVP_get_cipherbyname("rc4");
			break;

		case WINPR_CIPHER_BLOWFISH_ECB:
			evp = EVP_get_cipherbyname("blowfish-ecb");
			break;

		case WINPR_CIPHER_BLOWFISH_CBC:
			evp = EVP_get_cipherbyname("blowfish-cbc");
			break;

		case WINPR_CIPHER_BLOWFISH_CFB64:
			evp = EVP_get_cipherbyname("blowfish-cfb64");
			break;

		case WINPR_CIPHER_BLOWFISH_CTR:
			evp = EVP_get_cipherbyname("blowfish-ctr");
			break;
	}

	return evp;
}

#elif defined(WITH_MBEDTLS)
mbedtls_cipher_type_t winpr_mbedtls_get_cipher_type(int cipher)
{
	mbedtls_cipher_type_t type = MBEDTLS_CIPHER_NONE;

	switch (cipher)
	{
		case WINPR_CIPHER_NONE:
			type = MBEDTLS_CIPHER_NONE;
			break;

		case WINPR_CIPHER_NULL:
			type = MBEDTLS_CIPHER_NULL;
			break;

		case WINPR_CIPHER_AES_128_ECB:
			type = MBEDTLS_CIPHER_AES_128_ECB;
			break;

		case WINPR_CIPHER_AES_192_ECB:
			type = MBEDTLS_CIPHER_AES_192_ECB;
			break;

		case WINPR_CIPHER_AES_256_ECB:
			type = MBEDTLS_CIPHER_AES_256_ECB;
			break;

		case WINPR_CIPHER_AES_128_CBC:
			type = MBEDTLS_CIPHER_AES_128_CBC;
			break;

		case WINPR_CIPHER_AES_192_CBC:
			type = MBEDTLS_CIPHER_AES_192_CBC;
			break;

		case WINPR_CIPHER_AES_256_CBC:
			type = MBEDTLS_CIPHER_AES_256_CBC;
			break;

		case WINPR_CIPHER_AES_128_CFB128:
			type = MBEDTLS_CIPHER_AES_128_CFB128;
			break;

		case WINPR_CIPHER_AES_192_CFB128:
			type = MBEDTLS_CIPHER_AES_192_CFB128;
			break;

		case WINPR_CIPHER_AES_256_CFB128:
			type = MBEDTLS_CIPHER_AES_256_CFB128;
			break;

		case WINPR_CIPHER_AES_128_CTR:
			type = MBEDTLS_CIPHER_AES_128_CTR;
			break;

		case WINPR_CIPHER_AES_192_CTR:
			type = MBEDTLS_CIPHER_AES_192_CTR;
			break;

		case WINPR_CIPHER_AES_256_CTR:
			type = MBEDTLS_CIPHER_AES_256_CTR;
			break;

		case WINPR_CIPHER_AES_128_GCM:
			type = MBEDTLS_CIPHER_AES_128_GCM;
			break;

		case WINPR_CIPHER_AES_192_GCM:
			type = MBEDTLS_CIPHER_AES_192_GCM;
			break;

		case WINPR_CIPHER_AES_256_GCM:
			type = MBEDTLS_CIPHER_AES_256_GCM;
			break;

		case WINPR_CIPHER_AES_128_CCM:
			type = MBEDTLS_CIPHER_AES_128_CCM;
			break;

		case WINPR_CIPHER_AES_192_CCM:
			type = MBEDTLS_CIPHER_AES_192_CCM;
			break;

		case WINPR_CIPHER_AES_256_CCM:
			type = MBEDTLS_CIPHER_AES_256_CCM;
			break;
	}

	return type;
}
#endif

WINPR_CIPHER_CTX* winpr_Cipher_New(int cipher, int op, const void* key, const void* iv)
{
	WINPR_CIPHER_CTX* ctx = NULL;
#if defined(WITH_OPENSSL)
	int operation = 0;
	const EVP_CIPHER* evp = NULL;
	EVP_CIPHER_CTX* octx = NULL;

	if (!(evp = winpr_openssl_get_evp_cipher(cipher)))
		return NULL;

	if (!(octx = EVP_CIPHER_CTX_new()))
		return NULL;

	operation = (op == WINPR_ENCRYPT) ? 1 : 0;

	if (EVP_CipherInit_ex(octx, evp, NULL, key, iv, operation) != 1)
	{
		EVP_CIPHER_CTX_free(octx);
		return NULL;
	}

	EVP_CIPHER_CTX_set_padding(octx, 0);
	ctx = (WINPR_CIPHER_CTX*)octx;
#elif defined(WITH_MBEDTLS)
	int key_bitlen;
	mbedtls_operation_t operation;
	mbedtls_cipher_context_t* mctx;
	mbedtls_cipher_type_t cipher_type = winpr_mbedtls_get_cipher_type(cipher);
	const mbedtls_cipher_info_t* cipher_info = mbedtls_cipher_info_from_type(cipher_type);

	if (!cipher_info)
		return NULL;

	if (!(mctx = (mbedtls_cipher_context_t*)calloc(1, sizeof(mbedtls_cipher_context_t))))
		return NULL;

	operation = (op == WINPR_ENCRYPT) ? MBEDTLS_ENCRYPT : MBEDTLS_DECRYPT;
	mbedtls_cipher_init(mctx);

	if (mbedtls_cipher_setup(mctx, cipher_info) != 0)
	{
		free(mctx);
		return NULL;
	}

	key_bitlen = mbedtls_cipher_get_key_bitlen(mctx);

	if (mbedtls_cipher_setkey(mctx, key, key_bitlen, operation) != 0)
	{
		mbedtls_cipher_free(mctx);
		free(mctx);
		return NULL;
	}

	if (mbedtls_cipher_set_padding_mode(mctx, MBEDTLS_PADDING_NONE) != 0)
	{
		mbedtls_cipher_free(mctx);
		free(mctx);
		return NULL;
	}

	ctx = (WINPR_CIPHER_CTX*)mctx;
#endif
	return ctx;
}

BOOL winpr_Cipher_SetPadding(WINPR_CIPHER_CTX* ctx, BOOL enabled)
{
	WINPR_ASSERT(ctx);

#if defined(WITH_OPENSSL)
	EVP_CIPHER_CTX_set_padding((EVP_CIPHER_CTX*)ctx, enabled);
#elif defined(WITH_MBEDTLS)
	mbedtls_cipher_padding_t option = enabled ? MBEDTLS_PADDING_PKCS7 : MBEDTLS_PADDING_NONE;
	if (mbedtls_cipher_set_padding_mode((mbedtls_cipher_context_t*)ctx, option) != 0)
		return FALSE;
#else
	return FALSE;
#endif
	return TRUE;
}

BOOL winpr_Cipher_Update(WINPR_CIPHER_CTX* ctx, const void* input, size_t ilen, void* output,
                         size_t* olen)
{
#if defined(WITH_OPENSSL)
	int outl = (int)*olen;

	if (ilen > INT_MAX)
	{
		WLog_ERR(TAG, "input length %" PRIuz " > %d, abort", ilen, INT_MAX);
		return FALSE;
	}

	WINPR_ASSERT(ctx);
	if (EVP_CipherUpdate((EVP_CIPHER_CTX*)ctx, output, &outl, input, (int)ilen) == 1)
	{
		*olen = (size_t)outl;
		return TRUE;
	}

#elif defined(WITH_MBEDTLS)

	if (mbedtls_cipher_update((mbedtls_cipher_context_t*)ctx, input, ilen, output, olen) == 0)
		return TRUE;

#endif

	WLog_ERR(TAG, "Failed to update the data");
	return FALSE;
}

BOOL winpr_Cipher_Final(WINPR_CIPHER_CTX* ctx, void* output, size_t* olen)
{
#if defined(WITH_OPENSSL)
	int outl = (int)*olen;

	if (EVP_CipherFinal_ex((EVP_CIPHER_CTX*)ctx, output, &outl) == 1)
	{
		*olen = (size_t)outl;
		return TRUE;
	}

#elif defined(WITH_MBEDTLS)

	if (mbedtls_cipher_finish((mbedtls_cipher_context_t*)ctx, output, olen) == 0)
		return TRUE;

#endif
	return FALSE;
}

void winpr_Cipher_Free(WINPR_CIPHER_CTX* ctx)
{
	if (!ctx)
		return;

#if defined(WITH_OPENSSL)
	EVP_CIPHER_CTX_free((EVP_CIPHER_CTX*)ctx);
#elif defined(WITH_MBEDTLS)
	mbedtls_cipher_free((mbedtls_cipher_context_t*)ctx);
	free(ctx);
#endif
}

/**
 * Key Generation
 */

int winpr_Cipher_BytesToKey(int cipher, WINPR_MD_TYPE md, const void* salt, const void* data,
                            size_t datal, size_t count, void* key, void* iv)
{
	/**
	 * Key and IV generation compatible with OpenSSL EVP_BytesToKey():
	 * https://www.openssl.org/docs/manmaster/crypto/EVP_BytesToKey.html
	 */
#if defined(WITH_OPENSSL)
	const EVP_MD* evp_md = NULL;
	const EVP_CIPHER* evp_cipher = NULL;
	evp_md = winpr_openssl_get_evp_md(md);
	evp_cipher = winpr_openssl_get_evp_cipher(cipher);
	WINPR_ASSERT(datal <= INT_MAX);
	WINPR_ASSERT(count <= INT_MAX);
	return EVP_BytesToKey(evp_cipher, evp_md, salt, data, (int)datal, (int)count, key, iv);
#elif defined(WITH_MBEDTLS)
	int rv = 0;
	BYTE md_buf[64];
	int niv, nkey, addmd = 0;
	unsigned int mds = 0;
	mbedtls_md_context_t ctx;
	const mbedtls_md_info_t* md_info;
	mbedtls_cipher_type_t cipher_type;
	const mbedtls_cipher_info_t* cipher_info;
	mbedtls_md_type_t md_type = winpr_mbedtls_get_md_type(md);
	md_info = mbedtls_md_info_from_type(md_type);
	cipher_type = winpr_mbedtls_get_cipher_type(cipher);
	cipher_info = mbedtls_cipher_info_from_type(cipher_type);
	nkey = mbedtls_cipher_info_get_key_bitlen(cipher_info) / 8;
	niv = mbedtls_cipher_info_get_iv_size(cipher_info);

	if ((nkey > 64) || (niv > 64))
		return 0;

	if (!data)
		return nkey;

	mbedtls_md_init(&ctx);

	if (mbedtls_md_setup(&ctx, md_info, 0) != 0)
		goto err;

	while (1)
	{
		if (mbedtls_md_starts(&ctx) != 0)
			goto err;

		if (addmd++)
		{
			if (mbedtls_md_update(&ctx, md_buf, mds) != 0)
				goto err;
		}

		if (mbedtls_md_update(&ctx, data, datal) != 0)
			goto err;

		if (salt)
		{
			if (mbedtls_md_update(&ctx, salt, 8) != 0)
				goto err;
		}

		if (mbedtls_md_finish(&ctx, md_buf) != 0)
			goto err;

		mds = mbedtls_md_get_size(md_info);

		for (unsigned int i = 1; i < (unsigned int)count; i++)
		{
			if (mbedtls_md_starts(&ctx) != 0)
				goto err;

			if (mbedtls_md_update(&ctx, md_buf, mds) != 0)
				goto err;

			if (mbedtls_md_finish(&ctx, md_buf) != 0)
				goto err;
		}

		i = 0;

		if (nkey)
		{
			while (1)
			{
				if (nkey == 0)
					break;

				if (i == mds)
					break;

				if (key)
					*(BYTE*)(key++) = md_buf[i];

				nkey--;
				i++;
			}
		}

		if (niv && (i != mds))
		{
			while (1)
			{
				if (niv == 0)
					break;

				if (i == mds)
					break;

				if (iv)
					*(BYTE*)(iv++) = md_buf[i];

				niv--;
				i++;
			}
		}

		if ((nkey == 0) && (niv == 0))
			break;
	}

	rv = mbedtls_cipher_info_get_key_bitlen(cipher_info) / 8;
err:
	mbedtls_md_free(&ctx);
	SecureZeroMemory(md_buf, 64);
	return rv;
#else
	return 0;
#endif
}
