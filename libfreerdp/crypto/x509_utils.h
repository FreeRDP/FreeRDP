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

#ifndef FREERDP_LIB_X509_UTILS_H
#define FREERDP_LIB_X509_UTILS_H

#include <winpr/custom-crypto.h>

#include <openssl/x509.h>

#include <freerdp/api.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_LOCAL WINPR_MD_TYPE x509_utils_get_signature_alg(const X509* xcert);
	FREERDP_LOCAL BYTE* x509_utils_get_hash(const X509* xcert, const char* hash, size_t* length);

	FREERDP_LOCAL BYTE* x509_utils_to_pem(const X509* xcert, const STACK_OF(X509) * chain,
	                                      size_t* length);
	FREERDP_LOCAL X509* x509_utils_from_pem(const char* data, size_t length, BOOL fromFile);

	FREERDP_LOCAL char* x509_utils_get_subject(const X509* xcert);
	FREERDP_LOCAL char* x509_utils_get_issuer(const X509* xcert);
	FREERDP_LOCAL char* x509_utils_get_email(const X509* x509);
	FREERDP_LOCAL char* x509_utils_get_upn(const X509* x509);

	FREERDP_LOCAL char* x509_utils_get_common_name(const X509* xcert, size_t* plength);
	FREERDP_LOCAL char** x509_utils_get_dns_names(const X509* xcert, size_t* count,
	                                              size_t** pplengths);

	FREERDP_LOCAL void x509_utils_dns_names_free(size_t count, size_t* lengths, char** dns_names);

	FREERDP_LOCAL BOOL x509_utils_check_eku(const X509* scert, int nid);
	FREERDP_LOCAL void x509_utils_print_info(const X509* xcert);

	FREERDP_LOCAL BOOL x509_utils_verify(X509* xcert, STACK_OF(X509) * chain,
	                                     const char* certificate_store_path);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_LIB_X509_UTILS_H */
