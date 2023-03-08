/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Cryptographic Abstraction Layer
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#ifndef FREERDP_CRYPTO_H
#define FREERDP_CRYPTO_H

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/crypto/certificate_data.h>

#ifdef __cplusplus
extern "C"
{
#endif
	struct rdp_CertInfo
	{
		BYTE* Modulus;
		DWORD ModulusLength;
		BYTE exponent[4];
	};
	typedef struct rdp_CertInfo rdpCertInfo;

	FREERDP_API char* crypto_base64_encode(const BYTE* data, size_t length);
	FREERDP_API void crypto_base64_decode(const char* enc_data, size_t length, BYTE** dec_data,
	                                      size_t* res_length);

	FREERDP_API char* crypto_base64url_encode(const BYTE* data, size_t length);
	FREERDP_API void crypto_base64url_decode(const char* enc_data, size_t length, BYTE** dec_data,
	                                         size_t* res_length);

	FREERDP_API char* crypto_read_pem(const char* filename, size_t* plength);
	FREERDP_API BOOL crypto_write_pem(const char* filename, const char* pem, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CRYPTO_H */
