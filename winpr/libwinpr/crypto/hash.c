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
 * HMAC
 */

#ifdef WITH_OPENSSL
const EVP_MD* winpr_openssl_get_evp_md(int md)
{
	const EVP_MD* evp = NULL;

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

WINPR_HMAC_CTX* winpr_HMAC_New(void)
{
	WINPR_HMAC_CTX* ctx = NULL;
#if defined(WITH_OPENSSL)
	HMAC_CTX* hmac = NULL;
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)

	if (!(hmac = (HMAC_CTX*) calloc(1, sizeof(HMAC_CTX))))
		return NULL;

	HMAC_CTX_init(hmac);
#else

	if (!(hmac = HMAC_CTX_new()))
		return NULL;

#endif
	ctx = (WINPR_HMAC_CTX*) hmac;
#elif defined(WITH_MBEDTLS)
	mbedtls_md_context_t* hmac;

	if (!(hmac = (mbedtls_md_context_t*) calloc(1, sizeof(mbedtls_md_context_t))))
		return NULL;

	mbedtls_md_init(hmac);
	ctx = (WINPR_HMAC_CTX*) hmac;
#endif
	return ctx;
}

BOOL winpr_HMAC_Init(WINPR_HMAC_CTX* ctx, WINPR_MD_TYPE md, const BYTE* key, size_t keylen)
{
#if defined(WITH_OPENSSL)
	HMAC_CTX* hmac = (HMAC_CTX*) ctx;
	const EVP_MD* evp = winpr_openssl_get_evp_md(md);

	if (!evp || !hmac)
		return FALSE;

#if (OPENSSL_VERSION_NUMBER < 0x10000000L)
	HMAC_Init_ex(hmac, key, keylen, evp, NULL); /* no return value on OpenSSL 0.9.x */
	return TRUE;
#else

	if (HMAC_Init_ex(hmac, key, keylen, evp, NULL) == 1)
		return TRUE;

#endif
#elif defined(WITH_MBEDTLS)
	mbedtls_md_context_t* hmac = (mbedtls_md_context_t*) ctx;
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
#if defined(WITH_OPENSSL)
	HMAC_CTX* hmac = (HMAC_CTX*) ctx;
#if (OPENSSL_VERSION_NUMBER < 0x10000000L)
	HMAC_Update(hmac, input, ilen); /* no return value on OpenSSL 0.9.x */
	return TRUE;
#else

	if (HMAC_Update(hmac, input, ilen) == 1)
		return TRUE;

#endif
#elif defined(WITH_MBEDTLS)
	mbedtls_md_context_t* mdctx = (mbedtls_md_context_t*) ctx;

	if (mbedtls_md_hmac_update(mdctx, input, ilen) == 0)
		return TRUE;

#endif
	return FALSE;
}

BOOL winpr_HMAC_Final(WINPR_HMAC_CTX* ctx, BYTE* output, size_t olen)
{
#if defined(WITH_OPENSSL)
	HMAC_CTX* hmac;
#elif defined(WITH_MBEDTLS)
	mbedtls_md_context_t* mdctx;
#endif

	if (!ctx)
		return FALSE;

#if defined(WITH_OPENSSL)
	hmac = (HMAC_CTX*) ctx;
#if (OPENSSL_VERSION_NUMBER < 0x10000000L)
	HMAC_Final(hmac, output, NULL); /* no return value on OpenSSL 0.9.x */
	return TRUE;
#else

	if (HMAC_Final(hmac, output, NULL) == 1)
		return TRUE;

#endif
#elif defined(WITH_MBEDTLS)
	mdctx = (mbedtls_md_context_t*) ctx;

	if (mbedtls_md_hmac_finish(mdctx, output) == 0)
		return TRUE;

#endif
	return FALSE;
}

void winpr_HMAC_Free(WINPR_HMAC_CTX* ctx)
{
#if defined(WITH_OPENSSL)
	HMAC_CTX* hmac = (HMAC_CTX*) ctx;

	if (hmac)
	{
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
		HMAC_CTX_cleanup(hmac);
		free(hmac);
#else
		HMAC_CTX_free(hmac);
#endif
	}

#elif defined(WITH_MBEDTLS)
	mbedtls_md_context_t* hmac = (mbedtls_md_context_t*) ctx;

	if (hmac)
	{
		mbedtls_md_free(hmac);
		free(hmac);
	}

#endif
}

BOOL winpr_HMAC(WINPR_MD_TYPE md, const BYTE* key, size_t keylen,
                const BYTE* input, size_t ilen, BYTE* output, size_t olen)
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

WINPR_DIGEST_CTX* winpr_Digest_New(void)
{
	WINPR_DIGEST_CTX* ctx = NULL;
#if defined(WITH_OPENSSL)
	EVP_MD_CTX* mdctx;
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
	mdctx = EVP_MD_CTX_create();
#else
	mdctx = EVP_MD_CTX_new();
#endif
	ctx = (WINPR_DIGEST_CTX*) mdctx;
#elif defined(WITH_MBEDTLS)
	mbedtls_md_context_t* mdctx;
	mdctx = (mbedtls_md_context_t*) calloc(1, sizeof(mbedtls_md_context_t));

	if (mdctx)
		mbedtls_md_init(mdctx);

	ctx = (WINPR_DIGEST_CTX*) mdctx;
#endif
	return ctx;
}

#if defined(WITH_OPENSSL)
BOOL winpr_Digest_Init_Internal(WINPR_DIGEST_CTX* ctx, WINPR_MD_TYPE md, const EVP_MD* evp)
{
	EVP_MD_CTX* mdctx = (EVP_MD_CTX*) ctx;

	if (!mdctx || !evp)
		return FALSE;

	if (EVP_DigestInit_ex(mdctx, evp, NULL) != 1)
		return FALSE;

	return TRUE;
}

#elif defined(WITH_MBEDTLS)
BOOL winpr_Digest_Init_Internal(WINPR_DIGEST_CTX* ctx, WINPR_MD_TYPE md)
{
	mbedtls_md_context_t* mdctx = (mbedtls_md_context_t*) ctx;
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
#if defined(WITH_OPENSSL)
	EVP_MD_CTX* mdctx = (EVP_MD_CTX*) ctx;
	const EVP_MD* evp = winpr_openssl_get_evp_md(md);

	/* Only MD5 is supported for FIPS allow override */
	if (md != WINPR_MD_MD5)
		return FALSE;

	EVP_MD_CTX_set_flags(mdctx, EVP_MD_CTX_FLAG_NON_FIPS_ALLOW);
	return winpr_Digest_Init_Internal(ctx, md, evp);
#elif defined(WITH_MBEDTLS)

	/* Only MD5 is supported for FIPS allow override */
	if (md != WINPR_MD_MD5)
		return FALSE;

	return winpr_Digest_Init_Internal(ctx, md);
#endif
}

BOOL winpr_Digest_Init(WINPR_DIGEST_CTX* ctx, WINPR_MD_TYPE md)
{
#if defined(WITH_OPENSSL)
	const EVP_MD* evp = winpr_openssl_get_evp_md(md);
	return winpr_Digest_Init_Internal(ctx, md, evp);
#else
	return winpr_Digest_Init_Internal(ctx, md);
#endif
}

BOOL winpr_Digest_Update(WINPR_DIGEST_CTX* ctx, const BYTE* input, size_t ilen)
{
#if defined(WITH_OPENSSL)
	EVP_MD_CTX* mdctx = (EVP_MD_CTX*) ctx;

	if (EVP_DigestUpdate(mdctx, input, ilen) != 1)
		return FALSE;

#elif defined(WITH_MBEDTLS)
	mbedtls_md_context_t* mdctx = (mbedtls_md_context_t*) ctx;

	if (mbedtls_md_update(mdctx, input, ilen) != 0)
		return FALSE;

#endif
	return TRUE;
}

BOOL winpr_Digest_Final(WINPR_DIGEST_CTX* ctx, BYTE* output, size_t olen)
{
#if defined(WITH_OPENSSL)
	EVP_MD_CTX* mdctx = (EVP_MD_CTX*) ctx;

	if (EVP_DigestFinal_ex(mdctx, output, NULL) == 1)
		return TRUE;

#elif defined(WITH_MBEDTLS)
	mbedtls_md_context_t* mdctx = (mbedtls_md_context_t*) ctx;

	if (mbedtls_md_finish(mdctx, output) == 0)
		return TRUE;

#endif
	return FALSE;
}

void winpr_Digest_Free(WINPR_DIGEST_CTX* ctx)
{
#if defined(WITH_OPENSSL)
	EVP_MD_CTX* mdctx = (EVP_MD_CTX*) ctx;

	if (mdctx)
	{
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
		EVP_MD_CTX_destroy(mdctx);
#else
		EVP_MD_CTX_free(mdctx);
#endif
	}

#elif defined(WITH_MBEDTLS)
	mbedtls_md_context_t* mdctx = (mbedtls_md_context_t*) ctx;

	if (mdctx)
	{
		mbedtls_md_free(mdctx);
		free(mdctx);
	}

#endif
}

BOOL winpr_Digest_Allow_FIPS(int md, const BYTE* input, size_t ilen, BYTE* output, size_t olen)
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

BOOL winpr_Digest(int md, const BYTE* input, size_t ilen, BYTE* output, size_t olen)
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
