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

#include <winpr/crypto.h>

#ifdef WITH_OPENSSL
#include <openssl/crypto.h>
#include <openssl/rand.h>
#endif

#ifdef WITH_MBEDTLS
#include <mbedtls/md.h>
#include <mbedtls/entropy.h>
#ifdef MBEDTLS_HAVEGE_C
#include <mbedtls/havege.h>
#endif
#include <mbedtls/hmac_drbg.h>
#endif

int winpr_RAND(void* output, size_t len)
{
#if defined(WITH_OPENSSL)
	if (len > INT_MAX)
		return -1;
	if (RAND_bytes(output, (int)len) != 1)
		return -1;
#elif defined(WITH_MBEDTLS)
#if defined(MBEDTLS_HAVEGE_C)
	mbedtls_havege_state hs;
	mbedtls_havege_init(&hs);

	if (mbedtls_havege_random(&hs, output, len) != 0)
		return -1;

	mbedtls_havege_free(&hs);
#else
	int status;
	mbedtls_entropy_context entropy;
	mbedtls_hmac_drbg_context hmac_drbg;
	const mbedtls_md_info_t* md_info;

	mbedtls_entropy_init(&entropy);
	mbedtls_hmac_drbg_init(&hmac_drbg);

	md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
	if ((status = mbedtls_hmac_drbg_seed(&hmac_drbg, md_info, mbedtls_entropy_func, &entropy, NULL,
	                                     0)) != 0)
		return -1;

	status = mbedtls_hmac_drbg_random(&hmac_drbg, output, len);
	mbedtls_hmac_drbg_free(&hmac_drbg);
	mbedtls_entropy_free(&entropy);

	if (status != 0)
		return -1;
#endif
#endif
	return 0;
}

int winpr_RAND_pseudo(void* output, size_t len)
{
	return winpr_RAND(output, len);
}
