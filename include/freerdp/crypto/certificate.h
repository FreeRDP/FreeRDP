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

	enum FREERDP_CERT_PARAM
	{
		FREERDP_CERT_RSA_E,
		FREERDP_CERT_RSA_N
	};

	typedef struct rdp_certificate rdpCertificate;

	FREERDP_API void freerdp_certificate_free(rdpCertificate* certificate);

	WINPR_ATTR_MALLOC(freerdp_certificate_free, 1)
	FREERDP_API rdpCertificate* freerdp_certificate_new(void);

	WINPR_ATTR_MALLOC(freerdp_certificate_free, 1)
	FREERDP_API rdpCertificate* freerdp_certificate_new_from_file(const char* file);

	WINPR_ATTR_MALLOC(freerdp_certificate_free, 1)
	FREERDP_API rdpCertificate* freerdp_certificate_new_from_pem(const char* pem);

	WINPR_ATTR_MALLOC(freerdp_certificate_free, 1)
	FREERDP_API rdpCertificate* freerdp_certificate_new_from_der(const BYTE* data, size_t length);

	FREERDP_API BOOL freerdp_certificate_is_rsa(const rdpCertificate* certificate);

	WINPR_ATTR_MALLOC(free, 1)
	FREERDP_API char* freerdp_certificate_get_hash(const rdpCertificate* certificate,
	                                               const char* hash, size_t* plength);

	WINPR_ATTR_MALLOC(free, 1)
	FREERDP_API char* freerdp_certificate_get_fingerprint_by_hash(const rdpCertificate* certificate,
	                                                              const char* hash);

	WINPR_ATTR_MALLOC(free, 1)
	FREERDP_API char*
	freerdp_certificate_get_fingerprint_by_hash_ex(const rdpCertificate* certificate,
	                                               const char* hash, BOOL separator);

	WINPR_ATTR_MALLOC(free, 1)
	FREERDP_API char* freerdp_certificate_get_fingerprint(const rdpCertificate* certificate);

	WINPR_ATTR_MALLOC(free, 1)
	FREERDP_API char* freerdp_certificate_get_pem(const rdpCertificate* certificate,
	                                              size_t* pLength);

	/**
	 * @brief Get the certificate as PEM string
	 * @param certificate A certificate instance to query
	 * @param pLength A pointer to the size in bytes of the PEM string
	 * @param withCertChain \b TRUE to export a full chain PEM, \b FALSE for only the last
	 * certificate in the chain
	 * @return A newly allocated string containing the requested PEM (free to deallocate) or NULL
	 * @since version 3.8.0
	 */
	WINPR_ATTR_MALLOC(free, 1)
	FREERDP_API char* freerdp_certificate_get_pem_ex(const rdpCertificate* certificate,
	                                                 size_t* pLength, BOOL withCertChain);

	WINPR_ATTR_MALLOC(free, 1)
	FREERDP_API BYTE* freerdp_certificate_get_der(const rdpCertificate* certificate,
	                                              size_t* pLength);

	WINPR_ATTR_MALLOC(free, 1)
	FREERDP_API char* freerdp_certificate_get_subject(const rdpCertificate* certificate);

	WINPR_ATTR_MALLOC(free, 1)
	FREERDP_API char* freerdp_certificate_get_issuer(const rdpCertificate* certificate);

	WINPR_ATTR_MALLOC(free, 1)
	FREERDP_API char* freerdp_certificate_get_upn(const rdpCertificate* certificate);

	WINPR_ATTR_MALLOC(free, 1)
	FREERDP_API char* freerdp_certificate_get_email(const rdpCertificate* certificate);

	/**
	 * @brief return the date string of the certificate validity
	 * @param certificate The certificate instance to query
	 * @param startDate \b TRUE return the start date, \b FALSE for the end date
	 * @return A newly allocated string containing the date, use \b free to deallocate
	 * @since version 3.8.0
	 */
	WINPR_ATTR_MALLOC(free, 1)
	FREERDP_API char* freerdp_certificate_get_validity(const rdpCertificate* certificate,
	                                                   BOOL startDate);

	FREERDP_API WINPR_MD_TYPE freerdp_certificate_get_signature_alg(const rdpCertificate* cert);

	WINPR_ATTR_MALLOC(free, 1)
	FREERDP_API char* freerdp_certificate_get_common_name(const rdpCertificate* cert,
	                                                      size_t* plength);

	WINPR_ATTR_MALLOC(free, 1)
	FREERDP_API char** freerdp_certificate_get_dns_names(const rdpCertificate* cert, size_t* pcount,
	                                                     size_t** pplengths);
	FREERDP_API void freerdp_certificate_free_dns_names(size_t count, size_t* lengths,
	                                                    char** names);

	FREERDP_API BOOL freerdp_certificate_check_eku(const rdpCertificate* certificate, int nid);

	FREERDP_API BOOL freerdp_certificate_get_public_key(const rdpCertificate* cert,
	                                                    BYTE** PublicKey, DWORD* PublicKeyLength);

	FREERDP_API BOOL freerdp_certificate_verify(const rdpCertificate* cert,
	                                            const char* certificate_store_path);

	FREERDP_API BOOL freerdp_certificate_is_rdp_security_compatible(const rdpCertificate* cert);

	WINPR_ATTR_MALLOC(free, 1)
	FREERDP_API char* freerdp_certificate_get_param(const rdpCertificate* cert,
	                                                enum FREERDP_CERT_PARAM what, size_t* psize);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CRYPTO_CERTIFICATE_H */
