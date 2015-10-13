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
#include <openssl/rand.h>
#endif

#ifdef WITH_MBEDTLS
#include <mbedtls/md.h>
#include <mbedtls/entropy.h>
#include <mbedtls/havege.h>
#include <mbedtls/hmac_drbg.h>
#endif

int winpr_RAND(BYTE* output, size_t len)
{
#if defined(WITH_OPENSSL)
	if (RAND_bytes(output, len) != 1)
		return -1;
#elif defined(WITH_MBEDTLS) && defined(MBEDTLS_HAVEGE_C)
	mbedtls_havege_state hs;
	mbedtls_havege_init(&hs);

	if (mbedtls_havege_random(&hs, output, len) != 0)
		return -1;

	mbedtls_havege_free(&hs);
#endif
	return 0;
}

int winpr_RAND_pseudo(BYTE* output, size_t len)
{
#if defined(WITH_OPENSSL)
	if (RAND_pseudo_bytes(output, len) != 1)
		return -1;
#elif defined(WITH_MBEDTLS) && defined(MBEDTLS_HAVEGE_C)
	mbedtls_havege_state hs;
	mbedtls_havege_init(&hs);

	if (mbedtls_havege_random(&hs, output, len) != 0)
		return -1;

	mbedtls_havege_free(&hs);
#endif
	return 0;
}
