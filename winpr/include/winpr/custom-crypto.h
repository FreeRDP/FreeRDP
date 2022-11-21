/**
 * WinPR: Windows Portable Runtime
 * Cryptography API (CryptoAPI)
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_CUSTOM_CRYPTO_H
#define WINPR_CUSTOM_CRYPTO_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#include <winpr/error.h>

/**
 * Custom Crypto API Abstraction Layer
 */

#define WINPR_MD4_DIGEST_LENGTH 16
#define WINPR_MD5_DIGEST_LENGTH 16
#define WINPR_SHA1_DIGEST_LENGTH 20
#define WINPR_SHA224_DIGEST_LENGTH 28
#define WINPR_SHA256_DIGEST_LENGTH 32
#define WINPR_SHA384_DIGEST_LENGTH 48
#define WINPR_SHA512_DIGEST_LENGTH 64
#define WINPR_RIPEMD160_DIGEST_LENGTH 20
#define WINPR_SHA3_224_DIGEST_LENGTH 28
#define WINPR_SHA3_256_DIGEST_LENGTH 32
#define WINPR_SHA3_384_DIGEST_LENGTH 48
#define WINPR_SHA3_512_DIGEST_LENGTH 64
#define WINPR_SHAKE128_DIGEST_LENGTH 16
#define WINPR_SHAKE256_DIGEST_LENGTH 32

/**
 * HMAC
 */
typedef enum
{
	WINPR_MD_NONE = 0,
	WINPR_MD_MD2 = 1,
	WINPR_MD_MD4 = 2,
	WINPR_MD_MD5 = 3,
	WINPR_MD_SHA1 = 4,
	WINPR_MD_SHA224 = 5,
	WINPR_MD_SHA256 = 6,
	WINPR_MD_SHA384 = 7,
	WINPR_MD_SHA512 = 8,
	WINPR_MD_RIPEMD160 = 9,
	WINPR_MD_SHA3_224 = 10,
	WINPR_MD_SHA3_256 = 11,
	WINPR_MD_SHA3_384 = 12,
	WINPR_MD_SHA3_512 = 13,
	WINPR_MD_SHAKE128 = 14,
	WINPR_MD_SHAKE256 = 15
} WINPR_MD_TYPE;

typedef struct winpr_hmac_ctx_private_st WINPR_HMAC_CTX;

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API WINPR_MD_TYPE winpr_md_type_from_string(const char* name);
	WINPR_API const char* winpr_md_type_to_string(WINPR_MD_TYPE md);

	WINPR_API WINPR_HMAC_CTX* winpr_HMAC_New(void);
	WINPR_API BOOL winpr_HMAC_Init(WINPR_HMAC_CTX* ctx, WINPR_MD_TYPE md, const BYTE* key,
	                               size_t keylen);
	WINPR_API BOOL winpr_HMAC_Update(WINPR_HMAC_CTX* ctx, const BYTE* input, size_t ilen);
	WINPR_API BOOL winpr_HMAC_Final(WINPR_HMAC_CTX* ctx, BYTE* output, size_t ilen);
	WINPR_API void winpr_HMAC_Free(WINPR_HMAC_CTX* ctx);
	WINPR_API BOOL winpr_HMAC(WINPR_MD_TYPE md, const BYTE* key, size_t keylen, const BYTE* input,
	                          size_t ilen, BYTE* output, size_t olen);

#ifdef __cplusplus
}
#endif

/**
 * Generic Digest API
 */

typedef struct winpr_digest_ctx_private_st WINPR_DIGEST_CTX;

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API WINPR_DIGEST_CTX* winpr_Digest_New(void);
	WINPR_API BOOL winpr_Digest_Init_Allow_FIPS(WINPR_DIGEST_CTX* ctx, WINPR_MD_TYPE md);
	WINPR_API BOOL winpr_Digest_Init(WINPR_DIGEST_CTX* ctx, WINPR_MD_TYPE md);
	WINPR_API BOOL winpr_Digest_Update(WINPR_DIGEST_CTX* ctx, const BYTE* input, size_t ilen);
	WINPR_API BOOL winpr_Digest_Final(WINPR_DIGEST_CTX* ctx, BYTE* output, size_t ilen);
	WINPR_API void winpr_Digest_Free(WINPR_DIGEST_CTX* ctx);
	WINPR_API BOOL winpr_Digest_Allow_FIPS(WINPR_MD_TYPE md, const BYTE* input, size_t ilen,
	                                       BYTE* output, size_t olen);
	WINPR_API BOOL winpr_Digest(WINPR_MD_TYPE md, const BYTE* input, size_t ilen, BYTE* output,
	                            size_t olen);

#ifdef __cplusplus
}
#endif

/**
 * Random Number Generation
 */

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API int winpr_RAND(void* output, size_t len);
	WINPR_API int winpr_RAND_pseudo(void* output, size_t len);

#ifdef __cplusplus
}
#endif

/**
 * RC4
 */

typedef struct winpr_rc4_ctx_private_st WINPR_RC4_CTX;

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API WINPR_RC4_CTX* winpr_RC4_New_Allow_FIPS(const BYTE* key, size_t keylen);
	WINPR_API WINPR_RC4_CTX* winpr_RC4_New(const BYTE* key, size_t keylen);
	WINPR_API BOOL winpr_RC4_Update(WINPR_RC4_CTX* ctx, size_t length, const BYTE* input,
	                                BYTE* output);
	WINPR_API void winpr_RC4_Free(WINPR_RC4_CTX* ctx);

#ifdef __cplusplus
}
#endif

/**
 * Generic Cipher API
 */

#define WINPR_AES_BLOCK_SIZE 16

/* cipher operation types */
#define WINPR_ENCRYPT 0
#define WINPR_DECRYPT 1

/* cipher types */
#define WINPR_CIPHER_NONE 0
#define WINPR_CIPHER_NULL 1
#define WINPR_CIPHER_AES_128_ECB 2
#define WINPR_CIPHER_AES_192_ECB 3
#define WINPR_CIPHER_AES_256_ECB 4
#define WINPR_CIPHER_AES_128_CBC 5
#define WINPR_CIPHER_AES_192_CBC 6
#define WINPR_CIPHER_AES_256_CBC 7
#define WINPR_CIPHER_AES_128_CFB128 8
#define WINPR_CIPHER_AES_192_CFB128 9
#define WINPR_CIPHER_AES_256_CFB128 10
#define WINPR_CIPHER_AES_128_CTR 11
#define WINPR_CIPHER_AES_192_CTR 12
#define WINPR_CIPHER_AES_256_CTR 13
#define WINPR_CIPHER_AES_128_GCM 14
#define WINPR_CIPHER_AES_192_GCM 15
#define WINPR_CIPHER_AES_256_GCM 16
#define WINPR_CIPHER_CAMELLIA_128_ECB 17
#define WINPR_CIPHER_CAMELLIA_192_ECB 18
#define WINPR_CIPHER_CAMELLIA_256_ECB 19
#define WINPR_CIPHER_CAMELLIA_128_CBC 20
#define WINPR_CIPHER_CAMELLIA_192_CBC 21
#define WINPR_CIPHER_CAMELLIA_256_CBC 22
#define WINPR_CIPHER_CAMELLIA_128_CFB128 23
#define WINPR_CIPHER_CAMELLIA_192_CFB128 24
#define WINPR_CIPHER_CAMELLIA_256_CFB128 25
#define WINPR_CIPHER_CAMELLIA_128_CTR 26
#define WINPR_CIPHER_CAMELLIA_192_CTR 27
#define WINPR_CIPHER_CAMELLIA_256_CTR 28
#define WINPR_CIPHER_CAMELLIA_128_GCM 29
#define WINPR_CIPHER_CAMELLIA_192_GCM 30
#define WINPR_CIPHER_CAMELLIA_256_GCM 31
#define WINPR_CIPHER_DES_ECB 32
#define WINPR_CIPHER_DES_CBC 33
#define WINPR_CIPHER_DES_EDE_ECB 34
#define WINPR_CIPHER_DES_EDE_CBC 35
#define WINPR_CIPHER_DES_EDE3_ECB 36
#define WINPR_CIPHER_DES_EDE3_CBC 37
#define WINPR_CIPHER_BLOWFISH_ECB 38
#define WINPR_CIPHER_BLOWFISH_CBC 39
#define WINPR_CIPHER_BLOWFISH_CFB64 40
#define WINPR_CIPHER_BLOWFISH_CTR 41
#define WINPR_CIPHER_ARC4_128 42
#define WINPR_CIPHER_AES_128_CCM 43
#define WINPR_CIPHER_AES_192_CCM 44
#define WINPR_CIPHER_AES_256_CCM 45
#define WINPR_CIPHER_CAMELLIA_128_CCM 46
#define WINPR_CIPHER_CAMELLIA_192_CCM 47
#define WINPR_CIPHER_CAMELLIA_256_CCM 48

typedef struct winpr_cipher_ctx_private_st WINPR_CIPHER_CTX;

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API WINPR_CIPHER_CTX* winpr_Cipher_New(int cipher, int op, const BYTE* key,
	                                             const BYTE* iv);
	WINPR_API BOOL winpr_Cipher_Update(WINPR_CIPHER_CTX* ctx, const BYTE* input, size_t ilen,
	                                   BYTE* output, size_t* olen);
	WINPR_API BOOL winpr_Cipher_Final(WINPR_CIPHER_CTX* ctx, BYTE* output, size_t* olen);
	WINPR_API void winpr_Cipher_Free(WINPR_CIPHER_CTX* ctx);

#ifdef __cplusplus
}
#endif

/**
 * Key Generation
 */

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API int winpr_Cipher_BytesToKey(int cipher, int md, const BYTE* salt, const BYTE* data,
	                                      int datal, int count, BYTE* key, BYTE* iv);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_CUSTOM_CRYPTO_H */
