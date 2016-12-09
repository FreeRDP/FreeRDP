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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include <winpr/crypto.h>

#ifdef WITH_OPENSSL
#include <openssl/aes.h>
#include <openssl/rc4.h>
#include <openssl/des.h>
#include <openssl/evp.h>
#endif

#ifdef WITH_MBEDTLS
#include <mbedtls/md.h>
#include <mbedtls/aes.h>
#include <mbedtls/arc4.h>
#include <mbedtls/des.h>
#include <mbedtls/cipher.h>
#endif

/**
 * RC4
 */

WINPR_RC4_CTX* winpr_RC4_New(const BYTE* key, size_t keylen)
{
	WINPR_RC4_CTX* ctx = NULL;

	if (!key || (keylen == 0))
		return NULL;

	ctx = calloc(1, sizeof(WINPR_RC4_CTX));
	if (!ctx)
		return NULL;

#if defined(WITH_OPENSSL)
	RC4_set_key((RC4_KEY*) ctx, keylen, key);
#elif defined(WITH_MBEDTLS) && defined(MBEDTLS_ARC4_C)
	mbedtls_arc4_init((mbedtls_arc4_context*) ctx);
	mbedtls_arc4_setup((mbedtls_arc4_context*) ctx, key, (unsigned int) keylen);
#endif
	return ctx;
}

BOOL winpr_RC4_Update(WINPR_RC4_CTX* ctx, size_t length, const BYTE* input, BYTE* output)
{
#if defined(WITH_OPENSSL)
	RC4((RC4_KEY*) ctx, length, input, output);
#elif defined(WITH_MBEDTLS) && defined(MBEDTLS_ARC4_C)
	if (mbedtls_arc4_crypt((mbedtls_arc4_context*) ctx, length, input, output) != 0)
		return FALSE;
#endif

	return TRUE;
}

void winpr_RC4_Free(WINPR_RC4_CTX* ctx)
{
	if (!ctx)
		return;

#if defined(WITH_OPENSSL)

#elif defined(WITH_MBEDTLS) && defined(MBEDTLS_ARC4_C)
	mbedtls_arc4_free((mbedtls_arc4_context*) ctx);
#endif
	free(ctx);
}

/**
 * Generic Cipher API
 */

#ifdef WITH_OPENSSL
extern const EVP_MD* winpr_openssl_get_evp_md(int md);
#endif

#ifdef WITH_MBEDTLS
extern mbedtls_md_type_t winpr_mbedtls_get_md_type(int md);
#endif

#if defined(WITH_OPENSSL)
const EVP_CIPHER* winpr_openssl_get_evp_cipher(int cipher)
{
	const EVP_CIPHER* evp = NULL;

	OpenSSL_add_all_ciphers();

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

		case WINPR_CIPHER_CAMELLIA_128_ECB:
			type = MBEDTLS_CIPHER_CAMELLIA_128_ECB;
			break;

		case WINPR_CIPHER_CAMELLIA_192_ECB:
			type = MBEDTLS_CIPHER_CAMELLIA_192_ECB;
			break;

		case WINPR_CIPHER_CAMELLIA_256_ECB:
			type = MBEDTLS_CIPHER_CAMELLIA_256_ECB;
			break;

		case WINPR_CIPHER_CAMELLIA_128_CBC:
			type = MBEDTLS_CIPHER_CAMELLIA_128_CBC;
			break;

		case WINPR_CIPHER_CAMELLIA_192_CBC:
			type = MBEDTLS_CIPHER_CAMELLIA_192_CBC;
			break;

		case WINPR_CIPHER_CAMELLIA_256_CBC:
			type = MBEDTLS_CIPHER_CAMELLIA_256_CBC;
			break;

		case WINPR_CIPHER_CAMELLIA_128_CFB128:
			type = MBEDTLS_CIPHER_CAMELLIA_128_CFB128;
			break;

		case WINPR_CIPHER_CAMELLIA_192_CFB128:
			type = MBEDTLS_CIPHER_CAMELLIA_192_CFB128;
			break;

		case WINPR_CIPHER_CAMELLIA_256_CFB128:
			type = MBEDTLS_CIPHER_CAMELLIA_256_CFB128;
			break;

		case WINPR_CIPHER_CAMELLIA_128_CTR:
			type = MBEDTLS_CIPHER_CAMELLIA_128_CTR;
			break;

		case WINPR_CIPHER_CAMELLIA_192_CTR:
			type = MBEDTLS_CIPHER_CAMELLIA_192_CTR;
			break;

		case WINPR_CIPHER_CAMELLIA_256_CTR:
			type = MBEDTLS_CIPHER_CAMELLIA_256_CTR;
			break;

		case WINPR_CIPHER_CAMELLIA_128_GCM:
			type = MBEDTLS_CIPHER_CAMELLIA_128_GCM;
			break;

		case WINPR_CIPHER_CAMELLIA_192_GCM:
			type = MBEDTLS_CIPHER_CAMELLIA_192_GCM;
			break;

		case WINPR_CIPHER_CAMELLIA_256_GCM:
			type = MBEDTLS_CIPHER_CAMELLIA_256_GCM;
			break;

		case WINPR_CIPHER_DES_ECB:
			type = MBEDTLS_CIPHER_DES_ECB;
			break;

		case WINPR_CIPHER_DES_CBC:
			type = MBEDTLS_CIPHER_DES_CBC;
			break;

		case WINPR_CIPHER_DES_EDE_ECB:
			type = MBEDTLS_CIPHER_DES_EDE_ECB;
			break;

		case WINPR_CIPHER_DES_EDE_CBC:
			type = MBEDTLS_CIPHER_DES_EDE_CBC;
			break;

		case WINPR_CIPHER_DES_EDE3_ECB:
			type = MBEDTLS_CIPHER_DES_EDE3_ECB;
			break;

		case WINPR_CIPHER_DES_EDE3_CBC:
			type = MBEDTLS_CIPHER_DES_EDE3_CBC;
			break;

		case WINPR_CIPHER_BLOWFISH_ECB:
			type = MBEDTLS_CIPHER_BLOWFISH_ECB;
			break;

		case WINPR_CIPHER_BLOWFISH_CBC:
			type = MBEDTLS_CIPHER_BLOWFISH_CBC;
			break;

		case WINPR_CIPHER_BLOWFISH_CFB64:
			type = MBEDTLS_CIPHER_BLOWFISH_CFB64;
			break;

		case WINPR_CIPHER_BLOWFISH_CTR:
			type = MBEDTLS_CIPHER_BLOWFISH_CTR;
			break;

		case WINPR_CIPHER_ARC4_128:
			type = MBEDTLS_CIPHER_ARC4_128;
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

		case WINPR_CIPHER_CAMELLIA_128_CCM:
			type = MBEDTLS_CIPHER_CAMELLIA_128_CCM;
			break;

		case WINPR_CIPHER_CAMELLIA_192_CCM:
			type = MBEDTLS_CIPHER_CAMELLIA_192_CCM;
			break;

		case WINPR_CIPHER_CAMELLIA_256_CCM:
			type = MBEDTLS_CIPHER_CAMELLIA_256_CCM;
			break;
	}

	return type;
}
#endif

WINPR_CIPHER_CTX* winpr_Cipher_New(int cipher, int op, const BYTE* key, const BYTE* iv)
{
	WINPR_CIPHER_CTX* ctx;
#if defined(WITH_OPENSSL)
	int operation;
	const EVP_CIPHER* evp;
	EVP_CIPHER_CTX* octx;
#elif defined(WITH_MBEDTLS)
	int key_bitlen;
	mbedtls_operation_t operation;
	mbedtls_cipher_type_t cipher_type;
	const mbedtls_cipher_info_t* cipher_info;
#endif

	ctx = calloc(1, sizeof(WINPR_CIPHER_CTX));
	if (!ctx)
		return NULL;

#if defined(WITH_OPENSSL)
	octx = (EVP_CIPHER_CTX*)ctx;
	evp = winpr_openssl_get_evp_cipher(cipher);

	if (!evp)
	{
		free (ctx);
		return NULL;
	}

	operation = (op == WINPR_ENCRYPT) ? 1 : 0;
	EVP_CIPHER_CTX_init(octx);

	if (EVP_CipherInit_ex(octx, evp, NULL, key, iv, operation) != 1)
	{
		EVP_CIPHER_CTX_cleanup(octx);
		free (octx);
		return NULL;
	}

	EVP_CIPHER_CTX_set_padding(octx, 0);
#elif defined(WITH_MBEDTLS)
	cipher_type = winpr_mbedtls_get_cipher_type(cipher);
	cipher_info = mbedtls_cipher_info_from_type(cipher_type);

	if (!cipher_info)
	{
		free (ctx);
		return NULL;
	}

	operation = (op == WINPR_ENCRYPT) ? MBEDTLS_ENCRYPT : MBEDTLS_DECRYPT;
	mbedtls_cipher_init((mbedtls_cipher_context_t*) ctx);

	if (mbedtls_cipher_setup((mbedtls_cipher_context_t*) ctx, cipher_info) != 0)
	{
		free (ctx);
		return NULL;
	}

	key_bitlen = mbedtls_cipher_get_key_bitlen((mbedtls_cipher_context_t*) ctx);

	if (mbedtls_cipher_setkey((mbedtls_cipher_context_t*) ctx, key, key_bitlen, operation) != 0)
	{
		mbedtls_cipher_free((mbedtls_cipher_context_t*) ctx);
		free (ctx);
		return NULL;
	}
#endif
	return ctx;
}

BOOL winpr_Cipher_Update(WINPR_CIPHER_CTX* ctx, const BYTE* input, size_t ilen, BYTE* output, size_t* olen)
{
#if defined(WITH_OPENSSL)
	int outl = (int) *olen;

	if (EVP_CipherUpdate((EVP_CIPHER_CTX*) ctx, output, &outl, input, ilen) != 1)
		return FALSE;

	*olen = (size_t) outl;
#elif defined(WITH_MBEDTLS)
	if (mbedtls_cipher_update((mbedtls_cipher_context_t*) ctx, input, ilen, output, olen) != 0)
		return FALSE;
#endif
	return TRUE;
}

BOOL winpr_Cipher_Final(WINPR_CIPHER_CTX* ctx, BYTE* output, size_t* olen)
{
#if defined(WITH_OPENSSL)
	int outl = (int) *olen;

	if (EVP_CipherFinal_ex((EVP_CIPHER_CTX*) ctx, output, &outl) != 1)
		return FALSE;

	*olen = (size_t) outl;
#elif defined(WITH_MBEDTLS)
	if (mbedtls_cipher_finish((mbedtls_cipher_context_t*) ctx, output, olen) != 0)
		return FALSE;
#endif
	return TRUE;
}

void winpr_Cipher_Free(WINPR_CIPHER_CTX* ctx)
{
	if (!ctx)
		return;

#if defined(WITH_OPENSSL)
	EVP_CIPHER_CTX_cleanup((EVP_CIPHER_CTX*) ctx);
#elif defined(WITH_MBEDTLS)
	mbedtls_cipher_free((mbedtls_cipher_context_t*) ctx);
#endif
}


/**
 * Key Generation
 */

int winpr_openssl_BytesToKey(int cipher, int md, const BYTE* salt, const BYTE* data, int datal, int count, BYTE* key, BYTE* iv)
{
	/**
	 * Key and IV generation compatible with OpenSSL EVP_BytesToKey():
	 * https://www.openssl.org/docs/manmaster/crypto/EVP_BytesToKey.html
	 */

#if defined(WITH_OPENSSL)
	const EVP_MD* evp_md;
	const EVP_CIPHER* evp_cipher;
	evp_md = winpr_openssl_get_evp_md(md);
	evp_cipher = winpr_openssl_get_evp_cipher(cipher);
	return EVP_BytesToKey(evp_cipher, evp_md, salt, data, datal, count, key, iv);
#elif defined(WITH_MBEDTLS)
	int rv = 0;
	BYTE md_buf[64];
	int niv, nkey, addmd = 0;
	unsigned int mds = 0, i;
	mbedtls_md_context_t ctx;
	const mbedtls_md_info_t* md_info;
	mbedtls_cipher_type_t cipher_type;
	const mbedtls_cipher_info_t* cipher_info;

	mbedtls_md_type_t md_type = winpr_mbedtls_get_md_type(md);
	md_info = mbedtls_md_info_from_type(md_type);

	cipher_type = winpr_mbedtls_get_cipher_type(cipher);
	cipher_info = mbedtls_cipher_info_from_type(cipher_type);

	nkey = cipher_info->key_bitlen / 8;
	niv = cipher_info->iv_size;

	if ((nkey > 64) || (niv > 64))
		return 0;

	if (!data)
		return nkey;

	mbedtls_md_init(&ctx);

	while (1)
	{
		if (mbedtls_md_setup(&ctx, md_info, 0) != 0)
			return 0;

		if (mbedtls_md_starts(&ctx) != 0)
			return 0;

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

		for (i = 1; i < (unsigned int) count; i++)
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
					*(key++) = md_buf[i];
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
					*(iv++) = md_buf[i];
				niv--;
				i++;
			}
		}

		if ((nkey == 0) && (niv == 0))
			break;
	}

	rv = cipher_info->key_bitlen / 8;
err:
	mbedtls_md_free(&ctx);
	SecureZeroMemory(md_buf, 64);
	return rv;
#endif

	return 0;
}
