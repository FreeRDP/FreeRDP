/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Certificate Handling
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

#ifndef FREERDP_CRYPTO_CERTIFICATE_H
#define FREERDP_CRYPTO_CERTIFICATE_H

#include <winpr/crypto.h>

#include <freerdp/api.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct rdp_certificate rdpCertificate;

	FREERDP_API rdpCertificate* freerdp_certificate_new(void);
	FREERDP_API rdpCertificate* freerdp_certificate_new_from_file(const char* file);
	FREERDP_API rdpCertificate* freerdp_certificate_new_from_pem(const char* pem);
	FREERDP_API rdpCertificate* freerdp_certificate_new_from_der(const BYTE* data, size_t length);

	FREERDP_API void freerdp_certificate_free(rdpCertificate* certificate);

	FREERDP_API BOOL freerdp_certificate_is_rsa(const rdpCertificate* certificate);

	FREERDP_API char* freerdp_certificate_get_hash(const rdpCertificate* certificate,
	                                               const char* hash, size_t* plength);

	FREERDP_API char* freerdp_certificate_get_fingerprint_by_hash(const rdpCertificate* certificate,
	                                                              const char* hash);
	FREERDP_API char*
	freerdp_certificate_get_fingerprint_by_hash_ex(const rdpCertificate* certificate,
	                                               const char* hash, BOOL separator);
	FREERDP_API char* freerdp_certificate_get_fingerprint(const rdpCertificate* certificate);
	FREERDP_API char* freerdp_certificate_get_pem(const rdpCertificate* certificate,
	                                              size_t* pLength);
	FREERDP_API BYTE* freerdp_certificate_get_der(const rdpCertificate* certificate,
	                                              size_t* pLength);

	FREERDP_API char* freerdp_certificate_get_subject(const rdpCertificate* certificate);
	FREERDP_API char* freerdp_certificate_get_issuer(const rdpCertificate* certificate);

	FREERDP_API char* freerdp_certificate_get_upn(const rdpCertificate* certificate);
	FREERDP_API char* freerdp_certificate_get_email(const rdpCertificate* certificate);

	FREERDP_API WINPR_MD_TYPE freerdp_certificate_get_signature_alg(const rdpCertificate* cert);

	FREERDP_API char* freerdp_certificate_get_common_name(const rdpCertificate* cert,
	                                                      size_t* plength);
	FREERDP_API char** freerdp_certificate_get_dns_names(const rdpCertificate* cert, size_t* pcount,
	                                                     size_t** pplengths);
	FREERDP_API void freerdp_certificate_free_dns_names(size_t count, size_t* lengths,
	                                                    char** names);

	FREERDP_API BOOL freerdp_certificate_check_eku(const rdpCertificate* certificate, int nid);

	FREERDP_API BOOL freerdp_certificate_get_public_key(const rdpCertificate* cert,
	                                                    BYTE** PublicKey, DWORD* PublicKeyLength);

	FREERDP_API BOOL freerdp_certificate_verify(const rdpCertificate* cert,
	                                            const char* certificate_store_path);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CRYPTO_CERTIFICATE_H */
