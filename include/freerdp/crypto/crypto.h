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

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bn.h>
#include <openssl/x509v3.h>

#if defined(OPENSSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x0090800f)
#define D2I_X509_CONST const
#else
#define D2I_X509_CONST
#endif

#define EXPONENT_MAX_SIZE			4

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/crypto/certificate.h>

struct crypto_cert_struct
{
	X509 * px509;
	STACK_OF(X509) *px509chain;
};

#ifdef __cplusplus
 extern "C" {
#endif

typedef struct crypto_cert_struct* CryptoCert;

FREERDP_API CryptoCert crypto_cert_read(BYTE* data, UINT32 length);
FREERDP_API char* crypto_cert_fingerprint(X509* xcert);
FREERDP_API char* crypto_cert_subject(X509* xcert);
FREERDP_API char* crypto_cert_subject_common_name(X509* xcert, int* length);
FREERDP_API char** crypto_cert_subject_alt_name(X509* xcert, int* count,
		int** lengths);
FREERDP_API void crypto_cert_subject_alt_name_free(int count, int *lengths,
		char** alt_name);
FREERDP_API char* crypto_cert_issuer(X509* xcert);
FREERDP_API void crypto_cert_print_info(X509* xcert);
FREERDP_API void crypto_cert_free(CryptoCert cert);

FREERDP_API BOOL x509_verify_certificate(CryptoCert cert, char* certificate_store_path);
FREERDP_API rdpCertificateData* crypto_get_certificate_data(X509* xcert, char* hostname, UINT16 port);
FREERDP_API BOOL crypto_cert_get_public_key(CryptoCert cert, BYTE** PublicKey, DWORD* PublicKeyLength);

#define	TSSK_KEY_LENGTH	64
extern const BYTE tssk_modulus[];
extern const BYTE tssk_privateExponent[];
extern const BYTE tssk_exponent[];

FREERDP_API int crypto_rsa_public_encrypt(const BYTE* input, int length, UINT32 key_length, const BYTE* modulus, const BYTE* exponent, BYTE* output);
FREERDP_API int crypto_rsa_public_decrypt(const BYTE* input, int length, UINT32 key_length, const BYTE* modulus, const BYTE* exponent, BYTE* output);
FREERDP_API int crypto_rsa_private_encrypt(const BYTE* input, int length, UINT32 key_length, const BYTE* modulus, const BYTE* private_exponent, BYTE* output);
FREERDP_API int crypto_rsa_private_decrypt(const BYTE* input, int length, UINT32 key_length, const BYTE* modulus, const BYTE* private_exponent, BYTE* output);
FREERDP_API void crypto_reverse(BYTE* data, int length);

FREERDP_API char* crypto_base64_encode(const BYTE* data, int length);
FREERDP_API void crypto_base64_decode(const char* enc_data, int length, BYTE** dec_data, int* res_length);

#ifdef __cplusplus
 }
#endif

#endif /* FREERDP_CRYPTO_H */
