/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Certificate and private key heplers
 *
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

#ifndef FREERDP_LIB_CORE_CERT_COMMON_H
#define FREERDP_LIB_CORE_CERT_COMMON_H

#include <freerdp/crypto/ber.h>
#include <freerdp/crypto/crypto.h>

#include <freerdp/settings.h>
#include <freerdp/log.h>
#include <freerdp/api.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_LOCAL BOOL cert_info_create(rdpCertInfo* dst, const BIGNUM* rsa, const BIGNUM* rsa_e);
	FREERDP_LOCAL void cert_info_free(rdpCertInfo* info);

	FREERDP_LOCAL BOOL cert_info_clone(rdpCertInfo* dst, const rdpCertInfo* src);
	FREERDP_LOCAL BOOL cert_info_read_modulus(rdpCertInfo* info, size_t size, wStream* s);
	FREERDP_LOCAL BOOL cert_info_read_exponent(rdpCertInfo* info, size_t size, wStream* s);

	FREERDP_LOCAL BOOL read_bignum(BYTE** dst, UINT32* length, const BIGNUM* num, BOOL alloc);

	FREERDP_LOCAL X509* x509_from_rsa(const RSA* rsa);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_LIB_CORE_CERT_COMMON_H */
