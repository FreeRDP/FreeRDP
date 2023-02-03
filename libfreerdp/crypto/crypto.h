/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Cryptographic Abstraction Layer
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_LIB_CRYPTO_H
#define FREERDP_LIB_CRYPTO_H

/* OpenSSL includes windows.h */
#include <winpr/windows.h>
#include <winpr/custom-crypto.h>

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/crypto/crypto.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_LOCAL SSIZE_T crypto_rsa_public_encrypt(const BYTE* input, size_t length,
	                                                const rdpCertInfo* cert, BYTE* output,
	                                                size_t output_length);
	FREERDP_LOCAL SSIZE_T crypto_rsa_public_decrypt(const BYTE* input, size_t length,
	                                                const rdpCertInfo* cert, BYTE* output,
	                                                size_t output_length);
	FREERDP_LOCAL SSIZE_T crypto_rsa_private_encrypt(const BYTE* input, size_t length,
	                                                 const rdpPrivateKey* key, BYTE* output,
	                                                 size_t output_length);
	FREERDP_LOCAL SSIZE_T crypto_rsa_private_decrypt(const BYTE* input, size_t length,
	                                                 const rdpPrivateKey* key, BYTE* output,
	                                                 size_t output_length);

	FREERDP_LOCAL void crypto_reverse(BYTE* data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_LIB_CRYPTO_H */
