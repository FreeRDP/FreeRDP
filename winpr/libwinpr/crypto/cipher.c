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
#endif

#ifdef WITH_MBEDTLS
#include <mbedtls/aes.h>
#include <mbedtls/arc4.h>
#include <mbedtls/des.h>
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
