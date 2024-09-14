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
#include <freerdp/types.h>
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

	FREERDP_API char* crypto_base64_encode(const BYTE* WINPR_RESTRICT data, size_t length);

	/** BASE64 encode data
	 *
	 *  @param data The data to encode
	 *  @param length The lenght of the data in bytes
	 *  @param withCrLf Option to split the encoded data with CRLF linebreaks
	 *
	 *  @since version 3.0.0
	 *
	 *  @return The encoded BASE64 string or \b NULL if failed
	 */
	FREERDP_API char* crypto_base64_encode_ex(const BYTE* WINPR_RESTRICT data, size_t length,
	                                          BOOL withCrLf);

	FREERDP_API void crypto_base64_decode(const char* WINPR_RESTRICT enc_data, size_t length,
	                                      BYTE** WINPR_RESTRICT dec_data,
	                                      size_t* WINPR_RESTRICT res_length);

	FREERDP_API char* crypto_base64url_encode(const BYTE* WINPR_RESTRICT data, size_t length);
	FREERDP_API void crypto_base64url_decode(const char* WINPR_RESTRICT enc_data, size_t length,
	                                         BYTE** WINPR_RESTRICT dec_data,
	                                         size_t* WINPR_RESTRICT res_length);

	FREERDP_API char* crypto_read_pem(const char* WINPR_RESTRICT filename,
	                                  size_t* WINPR_RESTRICT plength);
	FREERDP_API BOOL crypto_write_pem(const char* WINPR_RESTRICT filename,
	                                  const char* WINPR_RESTRICT pem, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CRYPTO_H */
