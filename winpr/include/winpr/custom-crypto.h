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

	WINPR_API void winpr_HMAC_Free(WINPR_HMAC_CTX* ctx);

	WINPR_ATTR_MALLOC(winpr_HMAC_Free, 1)
	WINPR_API WINPR_HMAC_CTX* winpr_HMAC_New(void);
	WINPR_API BOOL winpr_HMAC_Init(WINPR_HMAC_CTX* ctx, WINPR_MD_TYPE md, const void* key,
	                               size_t keylen);
	WINPR_API BOOL winpr_HMAC_Update(WINPR_HMAC_CTX* ctx, const void* input, size_t ilen);
	WINPR_API BOOL winpr_HMAC_Final(WINPR_HMAC_CTX* ctx, void* output, size_t ilen);

	WINPR_API BOOL winpr_HMAC(WINPR_MD_TYPE md, const void* key, size_t keylen, const void* input,
	                          size_t ilen, void* output, size_t olen);

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

	WINPR_API void winpr_Digest_Free(WINPR_DIGEST_CTX* ctx);

	WINPR_ATTR_MALLOC(winpr_Digest_Free, 1)
	WINPR_API WINPR_DIGEST_CTX* winpr_Digest_New(void);
	WINPR_API BOOL winpr_Digest_Init_Allow_FIPS(WINPR_DIGEST_CTX* ctx, WINPR_MD_TYPE md);
	WINPR_API BOOL winpr_Digest_Init(WINPR_DIGEST_CTX* ctx, WINPR_MD_TYPE md);
	WINPR_API BOOL winpr_Digest_Update(WINPR_DIGEST_CTX* ctx, const void* input, size_t ilen);
	WINPR_API BOOL winpr_Digest_Final(WINPR_DIGEST_CTX* ctx, void* output, size_t ilen);

	WINPR_API BOOL winpr_Digest_Allow_FIPS(WINPR_MD_TYPE md, const void* input, size_t ilen,
	                                       void* output, size_t olen);
	WINPR_API BOOL winpr_Digest(WINPR_MD_TYPE md, const void* input, size_t ilen, void* output,
	                            size_t olen);

	WINPR_API BOOL winpr_DigestSign_Init(WINPR_DIGEST_CTX* ctx, WINPR_MD_TYPE md, void* key);
	WINPR_API BOOL winpr_DigestSign_Update(WINPR_DIGEST_CTX* ctx, const void* input, size_t ilen);
	WINPR_API BOOL winpr_DigestSign_Final(WINPR_DIGEST_CTX* ctx, void* output, size_t* piolen);

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

	WINPR_API void winpr_RC4_Free(WINPR_RC4_CTX* ctx);

	WINPR_ATTR_MALLOC(winpr_RC4_Free, 1)
	WINPR_API WINPR_RC4_CTX* winpr_RC4_New_Allow_FIPS(const void* key, size_t keylen);

	WINPR_ATTR_MALLOC(winpr_RC4_Free, 1)
	WINPR_API WINPR_RC4_CTX* winpr_RC4_New(const void* key, size_t keylen);
	WINPR_API BOOL winpr_RC4_Update(WINPR_RC4_CTX* ctx, size_t length, const void* input,
	                                void* output);

#ifdef __cplusplus
}
#endif

/**
 * Generic Cipher API
 */

#define WINPR_AES_BLOCK_SIZE 16

/* cipher operation types */
#define WINPR_CIPHER_MAX_IV_LENGTH 16u
#define WINPR_CIPHER_MAX_KEY_LENGTH 64u

typedef enum
{
	WINPR_ENCRYPT = 0,
	WINPR_DECRYPT = 1
} WINPR_CRYPTO_OPERATION;

/* cipher types */
typedef enum
{
	WINPR_CIPHER_NONE = 0,
	WINPR_CIPHER_NULL = 1,
	WINPR_CIPHER_AES_128_ECB = 2,
	WINPR_CIPHER_AES_192_ECB = 3,
	WINPR_CIPHER_AES_256_ECB = 4,
	WINPR_CIPHER_AES_128_CBC = 5,
	WINPR_CIPHER_AES_192_CBC = 6,
	WINPR_CIPHER_AES_256_CBC = 7,
	WINPR_CIPHER_AES_128_CFB128 = 8,
	WINPR_CIPHER_AES_192_CFB128 = 9,
	WINPR_CIPHER_AES_256_CFB128 = 10,
	WINPR_CIPHER_AES_128_CTR = 11,
	WINPR_CIPHER_AES_192_CTR = 12,
	WINPR_CIPHER_AES_256_CTR = 13,
	WINPR_CIPHER_AES_128_GCM = 14,
	WINPR_CIPHER_AES_192_GCM = 15,
	WINPR_CIPHER_AES_256_GCM = 16,
	WINPR_CIPHER_CAMELLIA_128_ECB = 17,
	WINPR_CIPHER_CAMELLIA_192_ECB = 18,
	WINPR_CIPHER_CAMELLIA_256_ECB = 19,
	WINPR_CIPHER_CAMELLIA_128_CBC = 20,
	WINPR_CIPHER_CAMELLIA_192_CBC = 21,
	WINPR_CIPHER_CAMELLIA_256_CBC = 22,
	WINPR_CIPHER_CAMELLIA_128_CFB128 = 23,
	WINPR_CIPHER_CAMELLIA_192_CFB128 = 24,
	WINPR_CIPHER_CAMELLIA_256_CFB128 = 25,
	WINPR_CIPHER_CAMELLIA_128_CTR = 26,
	WINPR_CIPHER_CAMELLIA_192_CTR = 27,
	WINPR_CIPHER_CAMELLIA_256_CTR = 28,
	WINPR_CIPHER_CAMELLIA_128_GCM = 29,
	WINPR_CIPHER_CAMELLIA_192_GCM = 30,
	WINPR_CIPHER_CAMELLIA_256_GCM = 31,
	WINPR_CIPHER_DES_ECB = 32,
	WINPR_CIPHER_DES_CBC = 33,
	WINPR_CIPHER_DES_EDE_ECB = 34,
	WINPR_CIPHER_DES_EDE_CBC = 35,
	WINPR_CIPHER_DES_EDE3_ECB = 36,
	WINPR_CIPHER_DES_EDE3_CBC = 37,
	WINPR_CIPHER_BLOWFISH_ECB = 38,
	WINPR_CIPHER_BLOWFISH_CBC = 39,
	WINPR_CIPHER_BLOWFISH_CFB64 = 40,
	WINPR_CIPHER_BLOWFISH_CTR = 41,
	WINPR_CIPHER_ARC4_128 = 42,
	WINPR_CIPHER_AES_128_CCM = 43,
	WINPR_CIPHER_AES_192_CCM = 44,
	WINPR_CIPHER_AES_256_CCM = 45,
	WINPR_CIPHER_CAMELLIA_128_CCM = 46,
	WINPR_CIPHER_CAMELLIA_192_CCM = 47,
	WINPR_CIPHER_CAMELLIA_256_CCM = 48,
} WINPR_CIPHER_TYPE;

typedef struct winpr_cipher_ctx_private_st WINPR_CIPHER_CTX;

#ifdef __cplusplus
extern "C"
{
#endif

	/** @brief convert a cipher string to an enum value
	 *
	 *  @param name the name of the cipher
	 *  @return the \b WINPR_CIPHER_* value matching or \b WINPR_CIPHER_NONE if not found.
	 *
	 *  @since version 3.10.0
	 */
	WINPR_API WINPR_CIPHER_TYPE winpr_cipher_type_from_string(const char* name);

	/** @brief convert a cipher enum value to string
	 *
	 *  @param md the cipher enum value
	 *  @return the string representation of the value
	 *
	 *  @since version 3.10.0
	 */
	WINPR_API const char* winpr_cipher_type_to_string(WINPR_CIPHER_TYPE md);

	WINPR_API void winpr_Cipher_Free(WINPR_CIPHER_CTX* ctx);

	WINPR_ATTR_MALLOC(winpr_Cipher_Free, 1)
	WINPR_API WINPR_DEPRECATED_VAR("[since 3.10.0] use winpr_Cipher_NewEx",
	                               WINPR_CIPHER_CTX* winpr_Cipher_New(WINPR_CIPHER_TYPE cipher,
	                                                                  WINPR_CRYPTO_OPERATION op,
	                                                                  const void* key,
	                                                                  const void* iv));

	/** @brief Create a new \b WINPR_CIPHER_CTX
	 *
	 * creates a new stream cipher. Only the ciphers supported by your SSL library are available,
	 * fallback to WITH_INTERNAL_RC4 is not possible.
	 *
	 * @param cipher The cipher to create the context for
	 * @param op Operation \b WINPR_ENCRYPT or \b WINPR_DECRYPT
	 * @param key A pointer to the key material (size must match expectations for the cipher used)
	 * @param keylen The length in bytes of key material
	 * @param iv A pointer to the IV material (size must match expectations for the cipher used)
	 * @param ivlen The length in bytes of the IV
	 *
	 * @return A newly allocated context or \b NULL
	 *
	 * @since version 3.10.0
	 */
	WINPR_ATTR_MALLOC(winpr_Cipher_Free, 1)
	WINPR_API WINPR_CIPHER_CTX* winpr_Cipher_NewEx(WINPR_CIPHER_TYPE cipher,
	                                               WINPR_CRYPTO_OPERATION op, const void* key,
	                                               size_t keylen, const void* iv, size_t ivlen);
	WINPR_API BOOL winpr_Cipher_SetPadding(WINPR_CIPHER_CTX* ctx, BOOL enabled);
	WINPR_API BOOL winpr_Cipher_Update(WINPR_CIPHER_CTX* ctx, const void* input, size_t ilen,
	                                   void* output, size_t* olen);
	WINPR_API BOOL winpr_Cipher_Final(WINPR_CIPHER_CTX* ctx, void* output, size_t* olen);

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

	WINPR_API int winpr_Cipher_BytesToKey(int cipher, WINPR_MD_TYPE md, const void* salt,
	                                      const void* data, size_t datal, size_t count, void* key,
	                                      void* iv);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_CUSTOM_CRYPTO_H */
