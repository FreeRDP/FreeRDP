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

#if defined(WITH_INTERNAL_MD4)
#include "md4.h"
#endif

#if defined(WITH_INTERNAL_MD5)
#include "md5.h"
#include "hmac_md5.h"
#endif

/**
 * HMAC
 */

#ifdef WITH_OPENSSL
extern const EVP_MD* winpr_openssl_get_evp_md(WINPR_MD_TYPE md);
#endif

#ifdef WITH_OPENSSL
const EVP_MD* winpr_openssl_get_evp_md(WINPR_MD_TYPE md)
{
	const char* name = winpr_md_type_to_string(md);
	if (!name)
		return NULL;
	return EVP_get_digestbyname(name);
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

struct hash_map
{
	const char* name;
	WINPR_MD_TYPE md;
};
static const struct hash_map hashes[] = { { "md2", WINPR_MD_MD2 },
	                                      { "md4", WINPR_MD_MD4 },
	                                      { "md5", WINPR_MD_MD5 },
	                                      { "sha1", WINPR_MD_SHA1 },
	                                      { "sha224", WINPR_MD_SHA224 },
	                                      { "sha256", WINPR_MD_SHA256 },
	                                      { "sha384", WINPR_MD_SHA384 },
	                                      { "sha512", WINPR_MD_SHA512 },
	                                      { "sha3_224", WINPR_MD_SHA3_224 },
	                                      { "sha3_256", WINPR_MD_SHA3_256 },
	                                      { "sha3_384", WINPR_MD_SHA3_384 },
	                                      { "sha3_512", WINPR_MD_SHA3_512 },
	                                      { "shake128", WINPR_MD_SHAKE128 },
	                                      { "shake256", WINPR_MD_SHAKE256 },
	                                      { NULL, WINPR_MD_NONE } };

WINPR_MD_TYPE winpr_md_type_from_string(const char* name)
{
	const struct hash_map* cur = hashes;
	while (cur->name)
	{
		if (_stricmp(cur->name, name) == 0)
			return cur->md;
		cur++;
	}
	return WINPR_MD_NONE;
}

const char* winpr_md_type_to_string(WINPR_MD_TYPE md)
{
	const struct hash_map* cur = hashes;
	while (cur->name)
	{
		if (cur->md == md)
			return cur->name;
		cur++;
	}
	return NULL;
}

struct winpr_hmac_ctx_private_st
{
	WINPR_MD_TYPE md;

#if defined(WITH_INTERNAL_MD5)
	WINPR_HMAC_MD5_CTX hmac_md5;
#endif
#if defined(WITH_OPENSSL)
	HMAC_CTX* hmac;
#endif
#if defined(WITH_MBEDTLS)
	mbedtls_md_context_t hmac;
#endif
};

WINPR_HMAC_CTX* winpr_HMAC_New(void)
{
	WINPR_HMAC_CTX* ctx = calloc(1, sizeof(WINPR_HMAC_CTX));
	if (!ctx)
		return NULL;
#if defined(WITH_OPENSSL)
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || \
    (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x2070000fL)

	if (!(ctx->hmac = (HMAC_CTX*)calloc(1, sizeof(HMAC_CTX))))
		goto fail;

	HMAC_CTX_init(ctx->hmac);
#else

	if (!(ctx->hmac = HMAC_CTX_new()))
		goto fail;

#endif
#elif defined(WITH_MBEDTLS)
	mbedtls_md_init(&ctx->hmac);
#endif
	return ctx;

fail:
	winpr_HMAC_Free(ctx);
	return NULL;
}

BOOL winpr_HMAC_Init(WINPR_HMAC_CTX* ctx, WINPR_MD_TYPE md, const BYTE* key, size_t keylen)
{
	WINPR_ASSERT(ctx);

	ctx->md = md;
	switch (ctx->md)
	{
#if defined(WITH_INTERNAL_MD5)
		case WINPR_MD_MD5:
			hmac_md5_init(&ctx->hmac_md5, key, keylen);
			return TRUE;
#endif
		default:
			break;
	}

#if defined(WITH_OPENSSL)
	HMAC_CTX* hmac = ctx->hmac;
	const EVP_MD* evp = winpr_openssl_get_evp_md(md);

	if (!evp || !hmac)
		return FALSE;

	if (keylen > INT_MAX)
		return FALSE;
#if (OPENSSL_VERSION_NUMBER < 0x10000000L) || \
    (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x2070000fL)
	HMAC_Init_ex(hmac, key, (int)keylen, evp, NULL); /* no return value on OpenSSL 0.9.x */
	return TRUE;
#else

	if (HMAC_Init_ex(hmac, key, (int)keylen, evp, NULL) == 1)
		return TRUE;

#endif
#elif defined(WITH_MBEDTLS)
	mbedtls_md_context_t* hmac = &ctx->hmac;
	mbedtls_md_type_t md_type = winpr_mbedtls_get_md_type(md);
	const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(md_type);

	if (!md_info || !hmac)
		return FALSE;

	if (hmac->md_info != md_info)
	{
		mbedtls_md_free(hmac); /* can be called at any time after mbedtls_md_init */

		if (mbedtls_md_setup(hmac, md_info, 1) != 0)
			return FALSE;
	}

	if (mbedtls_md_hmac_starts(hmac, key, keylen) == 0)
		return TRUE;

#endif
	return FALSE;
}

BOOL winpr_HMAC_Update(WINPR_HMAC_CTX* ctx, const BYTE* input, size_t ilen)
{
	WINPR_ASSERT(ctx);

	switch (ctx->md)
	{
#if defined(WITH_INTERNAL_MD5)
		case WINPR_MD_MD5:
			hmac_md5_update(&ctx->hmac_md5, input, ilen);
			return TRUE;
#endif
		default:
			break;
	}

#if defined(WITH_OPENSSL)
	HMAC_CTX* hmac = ctx->hmac;
#if (OPENSSL_VERSION_NUMBER < 0x10000000L) || \
    (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x2070000fL)
	HMAC_Update(hmac, input, ilen); /* no return value on OpenSSL 0.9.x */
	return TRUE;
#else

	if (HMAC_Update(hmac, input, ilen) == 1)
		return TRUE;

#endif
#elif defined(WITH_MBEDTLS)
	mbedtls_md_context_t* mdctx = &ctx->hmac;

	if (mbedtls_md_hmac_update(mdctx, input, ilen) == 0)
		return TRUE;

#endif
	return FALSE;
}

BOOL winpr_HMAC_Final(WINPR_HMAC_CTX* ctx, BYTE* output, size_t olen)
{
	WINPR_ASSERT(ctx);

	switch (ctx->md)
	{
#if defined(WITH_INTERNAL_MD5)
		case WINPR_MD_MD5:
			if (olen < WINPR_MD5_DIGEST_LENGTH)
				return FALSE;
			hmac_md5_finalize(&ctx->hmac_md5, output);
			return TRUE;
#endif
		default:
			break;
	}

#if defined(WITH_OPENSSL)
	HMAC_CTX* hmac = ctx->hmac;
#if (OPENSSL_VERSION_NUMBER < 0x10000000L) || \
    (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x2070000fL)
	HMAC_Final(hmac, output, NULL); /* no return value on OpenSSL 0.9.x */
	return TRUE;
#else

	if (HMAC_Final(hmac, output, NULL) == 1)
		return TRUE;

#endif
#elif defined(WITH_MBEDTLS)
	mbedtls_md_context_t* mdctx = &ctx->hmac;

	if (mbedtls_md_hmac_finish(mdctx, output) == 0)
		return TRUE;

#endif
	return FALSE;
}

void winpr_HMAC_Free(WINPR_HMAC_CTX* ctx)
{
	if (!ctx)
		return;

#if defined(WITH_OPENSSL)
	HMAC_CTX* hmac = ctx->hmac;

	if (hmac)
	{
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || \
    (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x2070000fL)
		HMAC_CTX_cleanup(hmac);
		free(hmac);
#else
		HMAC_CTX_free(hmac);
#endif
	}

#elif defined(WITH_MBEDTLS)
	mbedtls_md_context_t* hmac = &ctx->hmac;

	if (hmac)
		mbedtls_md_free(hmac);

#endif

	free(ctx);
}

BOOL winpr_HMAC(WINPR_MD_TYPE md, const BYTE* key, size_t keylen, const BYTE* input, size_t ilen,
                BYTE* output, size_t olen)
{
	BOOL result = FALSE;
	WINPR_HMAC_CTX* ctx = winpr_HMAC_New();

	if (!ctx)
		return FALSE;

	if (!winpr_HMAC_Init(ctx, md, key, keylen))
		goto out;

	if (!winpr_HMAC_Update(ctx, input, ilen))
		goto out;

	if (!winpr_HMAC_Final(ctx, output, olen))
		goto out;

	result = TRUE;
out:
	winpr_HMAC_Free(ctx);
	return result;
}

/**
 * Generic Digest API
 */

struct winpr_digest_ctx_private_st
{
	WINPR_MD_TYPE md;

#if defined(WITH_INTERNAL_MD4)
	WINPR_MD4_CTX md4;
#endif
#if defined(WITH_INTERNAL_MD5)
	WINPR_MD5_CTX md5;
#endif
#if defined(WITH_OPENSSL)
	EVP_MD_CTX* mdctx;
#endif
#if defined(WITH_MBEDTLS)
	mbedtls_md_context_t* mdctx;
#endif
};

WINPR_DIGEST_CTX* winpr_Digest_New(void)
{
	WINPR_DIGEST_CTX* ctx = calloc(1, sizeof(WINPR_DIGEST_CTX));
	if (!ctx)
		return NULL;

#if defined(WITH_OPENSSL)
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || \
    (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x2070000fL)
	ctx->mdctx = EVP_MD_CTX_create();
#else
	ctx->mdctx = EVP_MD_CTX_new();
#endif
	if (!ctx->mdctx)
		goto fail;

#elif defined(WITH_MBEDTLS)
	ctx->mdctx = (mbedtls_md_context_t*)calloc(1, sizeof(mbedtls_md_context_t));

	if (!ctx->mdctx)
		goto fail;

	mbedtls_md_init(ctx->mdctx);
#endif
	return ctx;

fail:
	winpr_Digest_Free(ctx);
	return NULL;
}

#if defined(WITH_OPENSSL)
static BOOL winpr_Digest_Init_Internal(WINPR_DIGEST_CTX* ctx, const EVP_MD* evp)
{
	WINPR_ASSERT(ctx);
	EVP_MD_CTX* mdctx = ctx->mdctx;

	if (!mdctx || !evp)
		return FALSE;

	if (EVP_DigestInit_ex(mdctx, evp, NULL) != 1)
		return FALSE;

	return TRUE;
}

#elif defined(WITH_MBEDTLS)
static BOOL winpr_Digest_Init_Internal(WINPR_DIGEST_CTX* ctx, WINPR_MD_TYPE md)
{
	WINPR_ASSERT(ctx);
	mbedtls_md_context_t* mdctx = ctx->mdctx;
	mbedtls_md_type_t md_type = winpr_mbedtls_get_md_type(md);
	const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(md_type);

	if (!md_info)
		return FALSE;

	if (mdctx->md_info != md_info)
	{
		mbedtls_md_free(mdctx); /* can be called at any time after mbedtls_md_init */

		if (mbedtls_md_setup(mdctx, md_info, 0) != 0)
			return FALSE;
	}

	if (mbedtls_md_starts(mdctx) != 0)
		return FALSE;

	return TRUE;
}
#endif

BOOL winpr_Digest_Init_Allow_FIPS(WINPR_DIGEST_CTX* ctx, WINPR_MD_TYPE md)
{
	WINPR_ASSERT(ctx);

#if defined(WITH_OPENSSL)
	const EVP_MD* evp = winpr_openssl_get_evp_md(md);

	/* Only MD5 is supported for FIPS allow override */
	if (md != WINPR_MD_MD5)
		return FALSE;

	EVP_MD_CTX_set_flags(ctx->mdctx, EVP_MD_CTX_FLAG_NON_FIPS_ALLOW);
	return winpr_Digest_Init_Internal(ctx, evp);
#elif defined(WITH_MBEDTLS)

	/* Only MD5 is supported for FIPS allow override */
	if (md != WINPR_MD_MD5)
		return FALSE;

	return winpr_Digest_Init_Internal(ctx, md);
#endif
}

BOOL winpr_Digest_Init(WINPR_DIGEST_CTX* ctx, WINPR_MD_TYPE md)
{
	WINPR_ASSERT(ctx);

	ctx->md = md;
	switch (md)
	{
#if defined(WITH_INTERNAL_MD4)
		case WINPR_MD_MD4:
			winpr_MD4_Init(&ctx->md4);
			return TRUE;
#endif
#if defined(WITH_INTERNAL_MD5)
		case WINPR_MD_MD5:
			winpr_MD5_Init(&ctx->md5);
			return TRUE;
#endif
		default:
			break;
	}

#if defined(WITH_OPENSSL)
	const EVP_MD* evp = winpr_openssl_get_evp_md(md);
	return winpr_Digest_Init_Internal(ctx, evp);
#else
	return winpr_Digest_Init_Internal(ctx, md);
#endif
}

BOOL winpr_Digest_Update(WINPR_DIGEST_CTX* ctx, const BYTE* input, size_t ilen)
{
	WINPR_ASSERT(ctx);

	switch (ctx->md)
	{
#if defined(WITH_INTERNAL_MD4)
		case WINPR_MD_MD4:
			winpr_MD4_Update(&ctx->md4, input, ilen);
			return TRUE;
#endif
#if defined(WITH_INTERNAL_MD5)
		case WINPR_MD_MD5:
			winpr_MD5_Update(&ctx->md5, input, ilen);
			return TRUE;
#endif
		default:
			break;
	}

#if defined(WITH_OPENSSL)
	EVP_MD_CTX* mdctx = ctx->mdctx;

	if (EVP_DigestUpdate(mdctx, input, ilen) != 1)
		return FALSE;

#elif defined(WITH_MBEDTLS)
	mbedtls_md_context_t* mdctx = ctx->mdctx;

	if (mbedtls_md_update(mdctx, input, ilen) != 0)
		return FALSE;

#endif
	return TRUE;
}

BOOL winpr_Digest_Final(WINPR_DIGEST_CTX* ctx, BYTE* output, size_t olen)
{
	WINPR_ASSERT(ctx);

	switch (ctx->md)
	{
#if defined(WITH_INTERNAL_MD4)
		case WINPR_MD_MD4:
			if (olen < WINPR_MD4_DIGEST_LENGTH)
				return FALSE;
			winpr_MD4_Final(output, &ctx->md4);
			return TRUE;
#endif
#if defined(WITH_INTERNAL_MD5)
		case WINPR_MD_MD5:
			if (olen < WINPR_MD5_DIGEST_LENGTH)
				return FALSE;
			winpr_MD5_Final(output, &ctx->md5);
			return TRUE;
#endif

		default:
			break;
	}

#if defined(WITH_OPENSSL)
	EVP_MD_CTX* mdctx = ctx->mdctx;

	if (EVP_DigestFinal_ex(mdctx, output, NULL) == 1)
		return TRUE;

#elif defined(WITH_MBEDTLS)
	mbedtls_md_context_t* mdctx = ctx->mdctx;

	if (mbedtls_md_finish(mdctx, output) == 0)
		return TRUE;

#endif
	return FALSE;
}

void winpr_Digest_Free(WINPR_DIGEST_CTX* ctx)
{
	if (!ctx)
		return;
#if defined(WITH_OPENSSL)
	if (ctx->mdctx)
	{
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || \
    (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x2070000fL)
		EVP_MD_CTX_destroy(ctx->mdctx);
#else
		EVP_MD_CTX_free(ctx->mdctx);
#endif
	}

#elif defined(WITH_MBEDTLS)
	if (ctx->mdctx)
	{
		mbedtls_md_free(ctx->mdctx);
		free(ctx->mdctx);
	}

#endif
	free(ctx);
}

BOOL winpr_Digest_Allow_FIPS(WINPR_MD_TYPE md, const BYTE* input, size_t ilen, BYTE* output,
                             size_t olen)
{
	BOOL result = FALSE;
	WINPR_DIGEST_CTX* ctx = winpr_Digest_New();

	if (!ctx)
		return FALSE;

	if (!winpr_Digest_Init_Allow_FIPS(ctx, md))
		goto out;

	if (!winpr_Digest_Update(ctx, input, ilen))
		goto out;

	if (!winpr_Digest_Final(ctx, output, olen))
		goto out;

	result = TRUE;
out:
	winpr_Digest_Free(ctx);
	return result;
}

BOOL winpr_Digest(WINPR_MD_TYPE md, const BYTE* input, size_t ilen, BYTE* output, size_t olen)
{
	BOOL result = FALSE;
	WINPR_DIGEST_CTX* ctx = winpr_Digest_New();

	if (!ctx)
		return FALSE;

	if (!winpr_Digest_Init(ctx, md))
		goto out;

	if (!winpr_Digest_Update(ctx, input, ilen))
		goto out;

	if (!winpr_Digest_Final(ctx, output, olen))
		goto out;

	result = TRUE;
out:
	winpr_Digest_Free(ctx);
	return result;
}
