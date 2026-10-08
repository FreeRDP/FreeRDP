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

	WINPR_ATTR_NODISCARD
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
	 * @return A newly allocated string containing the requested PEM (free to deallocate) or nullptr
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

	WINPR_ATTR_NODISCARD
	FREERDP_API WINPR_MD_TYPE freerdp_certificate_get_signature_alg(const rdpCertificate* cert);

	WINPR_ATTR_MALLOC(free, 1)
	FREERDP_API char* freerdp_certificate_get_common_name(const rdpCertificate* cert,
	                                                      size_t* plength);

	FREERDP_API void freerdp_certificate_free_dns_names(size_t count, size_t* lengths,
	                                                    char** names);

	/** @brief get DNS SAN entries from a certificate.
	 *
	 *  @param cert The certificate to extract the data from
	 *  @param pcount A pointer that will be set to the number of entries, must not be \b NULL
	 *  @param pplengths A pointer to an array that will hold the string length of each entry
	 *  @return an allocated array of strings containing the DNS SAN addresses or NULL if not found
	 */
	WINPR_ATTR_MALLOC(freerdp_certificate_free_dns_names, 3)
	FREERDP_API char** freerdp_certificate_get_dns_names(const rdpCertificate* cert, size_t* pcount,
	                                                     size_t** pplengths);

	/** @brief free IP san entries.
	 *
	 *  @param count The number of entries in the arrays
	 *  @param lengths The array containing the string lengths
	 *  @param  names The array containing the strings
	 *
	 *  @since version 3.33.0
	 */
	FREERDP_API void freerdp_certificate_free_ip_names(size_t count, size_t* lengths, char** names);

	/** @brief get IP SAN entries from a certificate.
	 *
	 *  @param cert The certificate to extract the data from
	 *  @param pcount A pointer that will be set to the number of entries, must not be \b NULL
	 *  @param pplengths A pointer to an array that will hold the string length of each entry
	 *  @return an allocated array of strings containing the IP SAN addresses or NULL if not found
	 *
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_MALLOC(freerdp_certificate_free_ip_names, 3)
	FREERDP_API char** freerdp_certificate_get_ip_names(const rdpCertificate* cert, size_t* pcount,
	                                                    size_t** pplengths);

	/** @brief Check if a given hostname matches the certificate given names.
	 *
	 *  Matching algorithm:
	 *  1. if hostname is an IPv6/IPv6 address
	 *    a. Check IP SAN entries exist
	 *      I. Check if the hostname matches one of them, return TRUE
	 *      II. Return FALSE if no match
	 *  2. Check if DNS SAN entries exist
	 *    a. If a DNS SAN entry matches return TRUE
	 *    b. Return FALSE if no match
	 *  3. Check if the common name matches
	 *
	 *  @param cert The certificate to check against
	 *  @param hostname The hostname to check
	 *  @param hostlen The string length of the hostname
	 *
	 *  @return TRUE if a match exists, FALSE otherwise
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_certificate_matches_hostname(const rdpCertificate* cert,
	                                                      const char* hostname, size_t hostlen);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_certificate_check_eku(const rdpCertificate* certificate, int nid);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_certificate_get_public_key(const rdpCertificate* cert,
	                                                    BYTE** PublicKey, DWORD* PublicKeyLength);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_certificate_verify(const rdpCertificate* cert,
	                                            const char* certificate_store_path);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_certificate_is_rdp_security_compatible(const rdpCertificate* cert);

	WINPR_ATTR_MALLOC(free, 1)
	FREERDP_API char* freerdp_certificate_get_param(const rdpCertificate* cert,
	                                                enum FREERDP_CERT_PARAM what, size_t* psize);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CRYPTO_CERTIFICATE_H */
