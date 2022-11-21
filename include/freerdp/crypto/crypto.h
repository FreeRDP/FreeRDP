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

#ifndef FREERDP_CRYPTO_H
#define FREERDP_CRYPTO_H

/* OpenSSL includes windows.h */
#include <winpr/windows.h>
#include <winpr/custom-crypto.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bn.h>
#include <openssl/x509v3.h>

#if OPENSSL_VERSION_NUMBER >= 0x0090800f
#define D2I_X509_CONST const
#else
#define D2I_X509_CONST
#endif

#define EXPONENT_MAX_SIZE 4

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/crypto/certificate.h>

struct crypto_cert_struct
{
	X509* px509;
	STACK_OF(X509) * px509chain;
};

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct crypto_cert_struct* CryptoCert;

	FREERDP_API CryptoCert crypto_cert_read(const BYTE* data, UINT32 length);
	FREERDP_API CryptoCert crypto_cert_pem_read(const char* data);
	FREERDP_API WINPR_MD_TYPE crypto_cert_get_signature_alg(X509* xcert);
	FREERDP_API BYTE* crypto_cert_hash(X509* xcert, const char* hash, UINT32* length);
	FREERDP_API char* crypto_cert_fingerprint_by_hash(X509* xcert, const char* hash);
	FREERDP_API char* crypto_cert_fingerprint_by_hash_ex(X509* xcert, const char* hash,
	                                                     BOOL separator);
	FREERDP_API char* crypto_cert_fingerprint(X509* xcert);
	FREERDP_API BYTE* crypto_cert_pem(X509* xcert, STACK_OF(X509) * chain, size_t* length);
	FREERDP_API X509* crypto_cert_from_pem(const char* data, size_t length, BOOL fromFile);
	FREERDP_API char* crypto_cert_subject(X509* xcert);
	FREERDP_API char* crypto_cert_subject_common_name(X509* xcert, int* length);
	FREERDP_API char** crypto_cert_get_dns_names(X509* xcert, int* count, int** lengths);
	FREERDP_API char* crypto_cert_get_email(X509* x509);
	FREERDP_API char* crypto_cert_get_upn(X509* x509);
	FREERDP_API void crypto_cert_dns_names_free(int count, int* lengths, char** dns_names);
	FREERDP_API char* crypto_cert_issuer(X509* xcert);
	FREERDP_API BOOL crypto_check_eku(X509* scert, int nid);
	FREERDP_API void crypto_cert_print_info(X509* xcert);
	FREERDP_API void crypto_cert_free(CryptoCert cert);

	/*
	Deprecated function names: crypto_cert_subject_alt_name and crypto_cert_subject_alt_name_free.
	Use crypto_cert_get_dns_names and crypto_cert_dns_names_free instead.
	(old names kept for now for compatibility of FREERDP_API).
	Note: email and upn amongst others are also alt_names,
	but the old crypto_cert_get_alt_names returned only the dns_names
	*/
	FREERDP_API char** crypto_cert_subject_alt_name(X509* xcert, int* count, int** lengths);
	FREERDP_API void crypto_cert_subject_alt_name_free(int count, int* lengths, char** alt_names);

	FREERDP_API BOOL x509_verify_certificate(CryptoCert cert, const char* certificate_store_path);
	FREERDP_API rdpCertificateData* crypto_get_certificate_data(X509* xcert, const char* hostname,
	                                                            UINT16 port);
	FREERDP_API BOOL crypto_cert_get_public_key(CryptoCert cert, BYTE** PublicKey,
	                                            DWORD* PublicKeyLength);

#define TSSK_KEY_LENGTH 64
	WINPR_API extern const BYTE tssk_modulus[];
	WINPR_API extern const BYTE tssk_privateExponent[];
	WINPR_API extern const BYTE tssk_exponent[];

	FREERDP_API SSIZE_T crypto_rsa_public_encrypt(const BYTE* input, size_t length,
	                                              size_t key_length, const BYTE* modulus,
	                                              const BYTE* exponent, BYTE* output);
	FREERDP_API SSIZE_T crypto_rsa_public_decrypt(const BYTE* input, size_t length,
	                                              size_t key_length, const BYTE* modulus,
	                                              const BYTE* exponent, BYTE* output);
	FREERDP_API SSIZE_T crypto_rsa_private_encrypt(const BYTE* input, size_t length,
	                                               size_t key_length, const BYTE* modulus,
	                                               const BYTE* private_exponent, BYTE* output);
	FREERDP_API SSIZE_T crypto_rsa_private_decrypt(const BYTE* input, size_t length,
	                                               size_t key_length, const BYTE* modulus,
	                                               const BYTE* private_exponent, BYTE* output);
	FREERDP_API void crypto_reverse(BYTE* data, size_t length);

	FREERDP_API char* crypto_base64_encode(const BYTE* data, size_t length);
	FREERDP_API void crypto_base64_decode(const char* enc_data, size_t length, BYTE** dec_data,
	                                      size_t* res_length);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CRYPTO_H */
