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
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#endif

#ifdef WITH_MBEDTLS
#include <mbedtls/md4.h>
#include <mbedtls/md5.h>
#include <mbedtls/sha1.h>
#include <mbedtls/md.h>
#endif

/**
 * MD5
 */

void winpr_MD5_Init(WINPR_MD5_CTX* ctx)
{
#if defined(WITH_OPENSSL)
	MD5_Init((MD5_CTX*) ctx);
#elif defined(WITH_MBEDTLS) && defined(MBEDTLS_MD5_C)
	mbedtls_md5_init((mbedtls_md5_context*) ctx);
	mbedtls_md5_starts((mbedtls_md5_context*) ctx);
#endif
}

void winpr_MD5_Update(WINPR_MD5_CTX* ctx, const BYTE* input, size_t ilen)
{
#if defined(WITH_OPENSSL)
	MD5_Update((MD5_CTX*) ctx, input, ilen);
#elif defined(WITH_MBEDTLS) && defined(MBEDTLS_MD5_C)
	mbedtls_md5_update((mbedtls_md5_context*) ctx, input, ilen);
#endif
}

void winpr_MD5_Final(WINPR_MD5_CTX* ctx, BYTE* output)
{
#if defined(WITH_OPENSSL)
	MD5_Final(output, (MD5_CTX*) ctx);
#elif defined(WITH_MBEDTLS) && defined(MBEDTLS_MD5_C)
	mbedtls_md5_finish((mbedtls_md5_context*) ctx, output);
	mbedtls_md5_free((mbedtls_md5_context*) ctx);
#endif
}

void winpr_MD5(const BYTE* input, size_t ilen, BYTE* output)
{
	WINPR_MD5_CTX ctx;
	winpr_MD5_Init(&ctx);
	winpr_MD5_Update(&ctx, input, ilen);
	winpr_MD5_Final(&ctx, output);
}

/**
 * MD4
 */

void winpr_MD4_Init(WINPR_MD4_CTX* ctx)
{
#if defined(WITH_OPENSSL)
	MD4_Init((MD4_CTX*) ctx);
#elif defined(WITH_MBEDTLS) && defined(MBEDTLS_MD4_C)
	mbedtls_md4_init((mbedtls_md4_context*) ctx);
	mbedtls_md4_starts((mbedtls_md4_context*) ctx);
#endif
}

void winpr_MD4_Update(WINPR_MD4_CTX* ctx, const BYTE* input, size_t ilen)
{
#if defined(WITH_OPENSSL)
	MD4_Update((MD4_CTX*) ctx, input, ilen);
#elif defined(WITH_MBEDTLS) && defined(MBEDTLS_MD4_C)
	mbedtls_md4_update((mbedtls_md4_context*) ctx, input, ilen);
#endif
}

void winpr_MD4_Final(WINPR_MD4_CTX* ctx, BYTE* output)
{
#if defined(WITH_OPENSSL)
	MD4_Final(output, (MD4_CTX*) ctx);
#elif defined(WITH_MBEDTLS) && defined(MBEDTLS_MD4_C)
	mbedtls_md4_finish((mbedtls_md4_context*) ctx, output);
	mbedtls_md4_free((mbedtls_md4_context*) ctx);
#endif
}

void winpr_MD4(const BYTE* input, size_t ilen, BYTE* output)
{
	WINPR_MD4_CTX ctx;
	winpr_MD4_Init(&ctx);
	winpr_MD4_Update(&ctx, input, ilen);
	winpr_MD4_Final(&ctx, output);
}

/**
 * SHA1
 */

void winpr_SHA1_Init(WINPR_SHA1_CTX* ctx)
{
#if defined(WITH_OPENSSL)
	SHA1_Init((SHA_CTX*) ctx);
#elif defined(WITH_MBEDTLS) && defined(MBEDTLS_SHA1_C)
	mbedtls_sha1_init((mbedtls_sha1_context*) ctx);
	mbedtls_sha1_starts((mbedtls_sha1_context*) ctx);
#endif
}

void winpr_SHA1_Update(WINPR_SHA1_CTX* ctx, const BYTE* input, size_t ilen)
{
#if defined(WITH_OPENSSL)
	SHA1_Update((SHA_CTX*) ctx, input, ilen);
#elif defined(WITH_MBEDTLS) && defined(MBEDTLS_SHA1_C)
	mbedtls_sha1_update((mbedtls_sha1_context*) ctx, input, ilen);
#endif
}

void winpr_SHA1_Final(WINPR_SHA1_CTX* ctx, BYTE* output)
{
#if defined(WITH_OPENSSL)
	SHA1_Final(output, (SHA_CTX*) ctx);
#elif defined(WITH_MBEDTLS) && defined(MBEDTLS_SHA1_C)
	mbedtls_sha1_finish((mbedtls_sha1_context*) ctx, output);
	mbedtls_sha1_free((mbedtls_sha1_context*) ctx);
#endif
}

void winpr_SHA1(const BYTE* input, size_t ilen, BYTE* output)
{
	WINPR_SHA1_CTX ctx;
	winpr_SHA1_Init(&ctx);
	winpr_SHA1_Update(&ctx, input, ilen);
	winpr_SHA1_Final(&ctx, output);
}

/**
 * HMAC
 */

#ifdef WITH_OPENSSL
const EVP_MD* winpr_openssl_get_evp_md(int md)
{
	const EVP_MD* evp = NULL;

	OpenSSL_add_all_digests();

	switch (md)
	{
		case WINPR_MD_MD2:
			evp = EVP_get_digestbyname("md2");
			break;

		case WINPR_MD_MD4:
			evp = EVP_get_digestbyname("md4");
			break;

		case WINPR_MD_MD5:
			evp = EVP_get_digestbyname("md5");
			break;

		case WINPR_MD_SHA1:
			evp = EVP_get_digestbyname("sha1");
			break;

		case WINPR_MD_SHA224:
			evp = EVP_get_digestbyname("sha224");
			break;

		case WINPR_MD_SHA256:
			evp = EVP_get_digestbyname("sha256");
			break;

		case WINPR_MD_SHA384:
			evp = EVP_get_digestbyname("sha384");
			break;

		case WINPR_MD_SHA512:
			evp = EVP_get_digestbyname("sha512");
			break;

		case WINPR_MD_RIPEMD160:
			evp = EVP_get_digestbyname("ripemd160");
			break;
	}

	return evp;
}
#endif

#ifdef WITH_MBEDTLS
mbedtls_md_type_t winpr_mbedtls_get_md_type(int md)
{
	mbedtls_md_type_t type = MBEDTLS_MD_NONE;

	switch (md)
	{
		case WINPR_MD_MD2:
			type = MBEDTLS_MD_MD2;
			break;

		case WINPR_MD_MD4:
			type = MBEDTLS_MD_MD4;
			break;

		case WINPR_MD_MD5:
			type = MBEDTLS_MD_MD5;
			break;

		case WINPR_MD_SHA1:
			type = MBEDTLS_MD_SHA1;
			break;

		case WINPR_MD_SHA224:
			type = MBEDTLS_MD_SHA224;
			break;

		case WINPR_MD_SHA256:
			type = MBEDTLS_MD_SHA256;
			break;

		case WINPR_MD_SHA384:
			type = MBEDTLS_MD_SHA384;
			break;

		case WINPR_MD_SHA512:
			type = MBEDTLS_MD_SHA512;
			break;

		case WINPR_MD_RIPEMD160:
			type = MBEDTLS_MD_RIPEMD160;
			break;
	}

	return type;
}
#endif

int winpr_HMAC_Init(WINPR_HMAC_CTX* ctx, int md, const BYTE* key, size_t keylen)
{
#if defined(WITH_OPENSSL)
	const EVP_MD* evp = winpr_openssl_get_evp_md(md);

	if (!evp)
		return -1;

	HMAC_CTX_init((HMAC_CTX*) ctx);

#if (OPENSSL_VERSION_NUMBER < 0x10000000L)
	HMAC_Init_ex((HMAC_CTX*) ctx, key, keylen, evp, NULL);
#else
	if (HMAC_Init_ex((HMAC_CTX*) ctx, key, keylen, evp, NULL) != 1)
		return -1;
#endif

#elif defined(WITH_MBEDTLS)
	const mbedtls_md_info_t* md_info;
	mbedtls_md_type_t md_type = winpr_mbedtls_get_md_type(md);
	md_info = mbedtls_md_info_from_type(md_type);

	if (!md_info)
		return -1;

	mbedtls_md_init((mbedtls_md_context_t*) ctx);

	if (mbedtls_md_setup((mbedtls_md_context_t*) ctx, md_info, 1) != 0)
		return -1;

	if (mbedtls_md_hmac_starts((mbedtls_md_context_t*) ctx, key, keylen) != 0)
		return -1;
#endif
	return 0;
}

int winpr_HMAC_Update(WINPR_HMAC_CTX* ctx, const BYTE* input, size_t ilen)
{
#if defined(WITH_OPENSSL)
#if (OPENSSL_VERSION_NUMBER < 0x10000000L)
	HMAC_Update((HMAC_CTX*) ctx, input, ilen);
#else
	if (HMAC_Update((HMAC_CTX*) ctx, input, ilen) != 1)
		return -1;
#endif
#elif defined(WITH_MBEDTLS)
	if (mbedtls_md_hmac_update((mbedtls_md_context_t*) ctx, input, ilen) != 0)
		return -1;
#endif
	return 0;
}

int winpr_HMAC_Final(WINPR_HMAC_CTX* ctx, BYTE* output)
{
#if defined(WITH_OPENSSL)
#if (OPENSSL_VERSION_NUMBER < 0x10000000L)
	HMAC_Final((HMAC_CTX*) ctx, output, NULL);
#else
	if (HMAC_Final((HMAC_CTX*) ctx, output, NULL) != 1)
		return -1;
#endif
	HMAC_CTX_cleanup((HMAC_CTX*) ctx);
#elif defined(WITH_MBEDTLS)
	if (mbedtls_md_hmac_finish((mbedtls_md_context_t*) ctx, output) != 0)
		return -1;

	mbedtls_md_free((mbedtls_md_context_t*) ctx);
#endif
	return 0;
}

int winpr_HMAC(int md, const BYTE* key, size_t keylen, const BYTE* input, size_t ilen, BYTE* output)
{
	WINPR_HMAC_CTX ctx;

	if (winpr_HMAC_Init(&ctx, md, key, keylen) != 0)
		return -1;

	if (winpr_HMAC_Update(&ctx, input, ilen) != 0)
		return -1;

	if (winpr_HMAC_Final(&ctx, output) != 0)
		return -1;

	return 0;
}

/**
 * Generic Digest API
 */

int winpr_Digest_Init(WINPR_DIGEST_CTX* ctx, int md)
{
#if defined(WITH_OPENSSL)
	const EVP_MD* evp = winpr_openssl_get_evp_md(md);

	if (!evp)
		return -1;

	EVP_MD_CTX_init((EVP_MD_CTX*) ctx);

	if (EVP_DigestInit_ex((EVP_MD_CTX*) ctx, evp, NULL) != 1)
		return -1;
#elif defined(WITH_MBEDTLS)
	const mbedtls_md_info_t* md_info;
	mbedtls_md_type_t md_type = winpr_mbedtls_get_md_type(md);
	md_info = mbedtls_md_info_from_type(md_type);

	if (!md_info)
		return -1;

	mbedtls_md_init((mbedtls_md_context_t*) ctx);

	if (mbedtls_md_setup((mbedtls_md_context_t*) ctx, md_info, 0) != 0)
		return -1;

	if (mbedtls_md_starts((mbedtls_md_context_t*) ctx) != 0)
		return -1;
#endif
	return 0;
}

int winpr_Digest_Update(WINPR_DIGEST_CTX* ctx, const BYTE* input, size_t ilen)
{
#if defined(WITH_OPENSSL)
	if (EVP_DigestUpdate((EVP_MD_CTX*) ctx, input, ilen) != 1)
		return -1;
#elif defined(WITH_MBEDTLS)
	if (mbedtls_md_update((mbedtls_md_context_t*) ctx, input, ilen) != 0)
		return -1;
#endif
	return 0;
}

int winpr_Digest_Final(WINPR_DIGEST_CTX* ctx, BYTE* output)
{
#if defined(WITH_OPENSSL)
	if (EVP_DigestFinal_ex((EVP_MD_CTX*) ctx, output, NULL) != 1)
		return -1;
#elif defined(WITH_MBEDTLS)
	if (mbedtls_md_finish((mbedtls_md_context_t*) ctx, output) != 0)
		return -1;

	mbedtls_md_free((mbedtls_md_context_t*) ctx);
#endif
	return 0;
}

int winpr_Digest(int md, const BYTE* input, size_t ilen, BYTE* output)
{
	WINPR_DIGEST_CTX ctx;

	if (winpr_Digest_Init(&ctx, md) != 0)
		return -1;

	if (winpr_Digest_Update(&ctx, input, ilen) != 0)
		return -1;

	if (winpr_Digest_Final(&ctx, output) != 0)
		return -1;

	return 0;
}
