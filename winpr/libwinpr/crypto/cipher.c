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
#include <mbedtls/aes.h>
#include <mbedtls/arc4.h>
#include <mbedtls/des.h>
#include <mbedtls/cipher.h>
#endif

/**
 * RC4
 */

void winpr_RC4_Init(WINPR_RC4_CTX* ctx, const BYTE* key, size_t keylen)
{
#if defined(WITH_OPENSSL)
	RC4_set_key((RC4_KEY*) ctx, keylen, key);
#elif defined(WITH_MBEDTLS) && defined(MBEDTLS_ARC4_C)
	mbedtls_arc4_init((mbedtls_arc4_context*) ctx);
	mbedtls_arc4_setup((mbedtls_arc4_context*) ctx, key, keylen);
#endif
}

int winpr_RC4_Update(WINPR_RC4_CTX* ctx, size_t length, const BYTE* input, BYTE* output)
{
#if defined(WITH_OPENSSL)
	RC4((RC4_KEY*) ctx, length, input, output);
#elif defined(WITH_MBEDTLS) && defined(MBEDTLS_ARC4_C)
	mbedtls_arc4_crypt((mbedtls_arc4_context*) ctx, length, input, output);
#endif
	return 0;
}

void winpr_RC4_Final(WINPR_RC4_CTX* ctx)
{
#if defined(WITH_OPENSSL)

#elif defined(WITH_MBEDTLS) && defined(MBEDTLS_ARC4_C)
	mbedtls_arc4_free((mbedtls_arc4_context*) ctx);
#endif
}

/**
 * Generic Cipher API
 */

#if defined(WITH_OPENSSL)
const EVP_CIPHER* winpr_openssl_get_evp_cipher(int cipher)
{
	const EVP_CIPHER* evp = NULL;

	switch (cipher)
	{
		case WINPR_CIPHER_NULL:
			evp = EVP_enc_null();
			break;

#ifndef OPENSSL_NO_AES
		case WINPR_CIPHER_AES_128_ECB:
			evp = EVP_aes_128_ecb();
			break;

		case WINPR_CIPHER_AES_192_ECB:
			evp = EVP_aes_192_ecb();
			break;

		case WINPR_CIPHER_AES_256_ECB:
			evp = EVP_aes_256_ecb();
			break;

		case WINPR_CIPHER_AES_128_CBC:
			evp = EVP_aes_128_cbc();
			break;

		case WINPR_CIPHER_AES_192_CBC:
			evp = EVP_aes_192_cbc();
			break;

		case WINPR_CIPHER_AES_256_CBC:
			evp = EVP_aes_256_cbc();
			break;

		case WINPR_CIPHER_AES_128_CFB128:
			evp = EVP_aes_128_cfb128();
			break;

		case WINPR_CIPHER_AES_192_CFB128:
			evp = EVP_aes_192_cfb128();
			break;

		case WINPR_CIPHER_AES_256_CFB128:
			evp = EVP_aes_256_cfb128();
			break;

		case WINPR_CIPHER_AES_128_CTR:
			evp = EVP_aes_128_ctr();
			break;

		case WINPR_CIPHER_AES_192_CTR:
			evp = EVP_aes_192_ctr();
			break;

		case WINPR_CIPHER_AES_256_CTR:
			evp = EVP_aes_256_ctr();
			break;

		case WINPR_CIPHER_AES_128_GCM:
			evp = EVP_aes_128_gcm();
			break;

		case WINPR_CIPHER_AES_192_GCM:
			evp = EVP_aes_192_gcm();
			break;

		case WINPR_CIPHER_AES_256_GCM:
			evp = EVP_aes_256_gcm();
			break;

		case WINPR_CIPHER_AES_128_CCM:
			evp = EVP_aes_128_ccm();
			break;

		case WINPR_CIPHER_AES_192_CCM:
			evp = EVP_aes_192_ccm();
			break;

		case WINPR_CIPHER_AES_256_CCM:
			evp = EVP_aes_256_ccm();
			break;
#endif

#ifndef OPENSSL_NO_CAMELLIA
		case WINPR_CIPHER_CAMELLIA_128_ECB:
			evp = EVP_camellia_128_ecb();
			break;

		case WINPR_CIPHER_CAMELLIA_192_ECB:
			evp = EVP_camellia_192_ecb();
			break;

		case WINPR_CIPHER_CAMELLIA_256_ECB:
			evp = EVP_camellia_256_ecb();
			break;

		case WINPR_CIPHER_CAMELLIA_128_CBC:
			evp = EVP_camellia_128_cbc();
			break;

		case WINPR_CIPHER_CAMELLIA_192_CBC:
			evp = EVP_camellia_192_cbc();
			break;

		case WINPR_CIPHER_CAMELLIA_256_CBC:
			evp = EVP_camellia_256_cbc();
			break;

		case WINPR_CIPHER_CAMELLIA_128_CFB128:
			evp = EVP_camellia_128_cfb128();
			break;

		case WINPR_CIPHER_CAMELLIA_192_CFB128:
			evp = EVP_camellia_192_cfb128();
			break;

		case WINPR_CIPHER_CAMELLIA_256_CFB128:
			evp = EVP_camellia_256_cfb128();
			break;

		case WINPR_CIPHER_CAMELLIA_128_CTR:
		case WINPR_CIPHER_CAMELLIA_192_CTR:
		case WINPR_CIPHER_CAMELLIA_256_CTR:
		case WINPR_CIPHER_CAMELLIA_128_GCM:
		case WINPR_CIPHER_CAMELLIA_192_GCM:
		case WINPR_CIPHER_CAMELLIA_256_GCM:
		case WINPR_CIPHER_CAMELLIA_128_CCM:
		case WINPR_CIPHER_CAMELLIA_192_CCM:
		case WINPR_CIPHER_CAMELLIA_256_CCM:
			break;
#endif

#ifndef OPENSSL_NO_DES
		case WINPR_CIPHER_DES_ECB:
			evp = EVP_des_ecb();
			break;

		case WINPR_CIPHER_DES_CBC:
			evp = EVP_des_cbc();
			break;

		case WINPR_CIPHER_DES_EDE_ECB:
			evp = EVP_des_ede_ecb();
			break;

		case WINPR_CIPHER_DES_EDE_CBC:
			evp = EVP_des_ede_cbc();
			break;

		case WINPR_CIPHER_DES_EDE3_ECB:
			evp = EVP_des_ede3_ecb();
			break;

		case WINPR_CIPHER_DES_EDE3_CBC:
			evp = EVP_des_ede3_cbc();
			break;
#endif

#ifndef OPENSSL_NO_RC4
		case WINPR_CIPHER_ARC4_128:
			evp = EVP_rc4();
			break;
#endif

		case WINPR_CIPHER_BLOWFISH_ECB:
		case WINPR_CIPHER_BLOWFISH_CBC:
		case WINPR_CIPHER_BLOWFISH_CFB64:
		case WINPR_CIPHER_BLOWFISH_CTR:
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

void winpr_Cipher_Init(WINPR_CIPHER_CTX* ctx, int cipher, int op, const BYTE* key, const BYTE* iv)
{
#if defined(WITH_OPENSSL)
	int operation;
	const EVP_CIPHER* evp;
	evp = winpr_openssl_get_evp_cipher(cipher);
	operation = (op == WINPR_ENCRYPT) ? 1 : 0;
	EVP_CIPHER_CTX_init((EVP_CIPHER_CTX*) ctx);
	EVP_CipherInit_ex((EVP_CIPHER_CTX*) ctx, evp, NULL, key, iv, operation);
#elif defined(WITH_MBEDTLS)
	int key_bitlen;
	mbedtls_operation_t operation;
	mbedtls_cipher_type_t cipher_type;
	const mbedtls_cipher_info_t* cipher_info;
	cipher_type = winpr_mbedtls_get_cipher_type(cipher);
	cipher_info = mbedtls_cipher_info_from_type(cipher_type);
	operation = (op == WINPR_ENCRYPT) ? MBEDTLS_ENCRYPT : MBEDTLS_DECRYPT;
	mbedtls_cipher_init((mbedtls_cipher_context_t*) ctx);
	mbedtls_cipher_setup((mbedtls_cipher_context_t*) ctx, cipher_info);
	key_bitlen = mbedtls_cipher_get_key_bitlen((mbedtls_cipher_context_t*) ctx);
	mbedtls_cipher_setkey((mbedtls_cipher_context_t*) ctx, key, key_bitlen, operation);
#endif
}

int winpr_Cipher_Update(WINPR_CIPHER_CTX* ctx, const BYTE* input, size_t ilen, BYTE* output, size_t* olen)
{
#if defined(WITH_OPENSSL)
	int outl = (int) *olen;
	EVP_CipherUpdate((EVP_CIPHER_CTX*) ctx, output, &outl, input, ilen);
	*olen = (size_t) outl;
#elif defined(WITH_MBEDTLS)
	mbedtls_cipher_update((mbedtls_cipher_context_t*) ctx, input, ilen, output, olen);
#endif
	return 0;
}

void winpr_Cipher_Final(WINPR_CIPHER_CTX* ctx, BYTE* output, size_t* olen)
{
#if defined(WITH_OPENSSL)
	int outl = (int) *olen;
	EVP_CipherFinal_ex((EVP_CIPHER_CTX*) ctx, output, &outl);
	EVP_CIPHER_CTX_cleanup((EVP_CIPHER_CTX*) ctx);
	*olen = (size_t) outl;
#elif defined(WITH_MBEDTLS)
	mbedtls_cipher_finish((mbedtls_cipher_context_t*) ctx, output, olen);
	mbedtls_cipher_free((mbedtls_cipher_context_t*) ctx);
#endif
}
