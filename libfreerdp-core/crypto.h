/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Cryptographic Abstraction Layer
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __CRYPTO_H
#define __CRYPTO_H

#ifdef _WIN32
#include "tcp.h"
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rc4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/bn.h>
#include <openssl/x509v3.h>
#include <openssl/rand.h>

#if defined(OPENSSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x0090800f)
#define D2I_X509_CONST const
#else
#define D2I_X509_CONST
#endif

#define EXPONENT_MAX_SIZE			4
#define MODULUS_MAX_SIZE			256

#include <freerdp/freerdp.h>
#include <freerdp/utils/blob.h>
#include <freerdp/utils/memory.h>

struct crypto_sha1_struct
{
	SHA_CTX sha_ctx;
};

struct crypto_md5_struct
{
	MD5_CTX md5_ctx;
};

struct crypto_rc4_struct
{
	RC4_KEY rc4_key;
};

struct crypto_des3_struct
{
	EVP_CIPHER_CTX des3_ctx;
};

struct crypto_hmac_struct
{
	HMAC_CTX hmac_ctx;
};

struct crypto_cert_struct
{
	X509 * px509;
};

#define	CRYPTO_SHA1_DIGEST_LENGTH	SHA_DIGEST_LENGTH
typedef struct crypto_sha1_struct* CryptoSha1;
CryptoSha1 crypto_sha1_init(void);
void crypto_sha1_update(CryptoSha1 sha1, const uint8* data, uint32 length);
void crypto_sha1_final(CryptoSha1 sha1, uint8* out_data);

#define	CRYPTO_MD5_DIGEST_LENGTH	MD5_DIGEST_LENGTH
typedef struct crypto_md5_struct* CryptoMd5;
CryptoMd5 crypto_md5_init(void);
void crypto_md5_update(CryptoMd5 md5, const uint8* data, uint32 length);
void crypto_md5_final(CryptoMd5 md5, uint8* out_data);

typedef struct crypto_rc4_struct* CryptoRc4;
CryptoRc4 crypto_rc4_init(const uint8* key, uint32 length);
void crypto_rc4(CryptoRc4 rc4, uint32 length, const uint8* in_data, uint8* out_data);
void crypto_rc4_free(CryptoRc4 rc4);

typedef struct crypto_des3_struct* CryptoDes3;
CryptoDes3 crypto_des3_encrypt_init(const uint8* key, const uint8* ivec);
CryptoDes3 crypto_des3_decrypt_init(const uint8* key, const uint8* ivec);
void crypto_des3_encrypt(CryptoDes3 des3, uint32 length, const uint8 *in_data, uint8 *out_data);
void crypto_des3_decrypt(CryptoDes3 des3, uint32 length, const uint8 *in_data, uint8* out_data);
void crypto_des3_free(CryptoDes3 des3);

typedef struct crypto_hmac_struct* CryptoHmac;
CryptoHmac crypto_hmac_new(void);
void crypto_hmac_sha1_init(CryptoHmac hmac, const uint8 *data, uint32 length);
void crypto_hmac_update(CryptoHmac hmac, const uint8 *data, uint32 length);
void crypto_hmac_final(CryptoHmac hmac, uint8 *out_data, uint32 length);
void crypto_hmac_free(CryptoHmac hmac);

typedef struct crypto_cert_struct* CryptoCert;

#include "certificate.h"

CryptoCert crypto_cert_read(uint8* data, uint32 length);
char* crypto_cert_fingerprint(X509* xcert);
char* crypto_cert_subject(X509* xcert);
char* crypto_cert_subject_common_name(X509* xcert, int* length);
char** crypto_cert_subject_alt_name(X509* xcert, int* count, int** lengths);
char* crypto_cert_issuer(X509* xcert);
void crypto_cert_print_info(X509* xcert);
void crypto_cert_free(CryptoCert cert);

boolean x509_verify_certificate(CryptoCert cert, char* certificate_store_path);
rdpCertificateData* crypto_get_certificate_data(X509* xcert, char* hostname);
boolean crypto_cert_get_public_key(CryptoCert cert, rdpBlob* public_key);

#define	TSSK_KEY_LENGTH	64
extern const uint8 tssk_modulus[];
extern const uint8 tssk_privateExponent[];
extern const uint8 tssk_exponent[];

void crypto_rsa_public_encrypt(const uint8* input, int length, uint32 key_length, const uint8* modulus, const uint8* exponent, uint8* output);
void crypto_rsa_public_decrypt(const uint8* input, int length, uint32 key_length, const uint8* modulus, const uint8* exponent, uint8* output);
void crypto_rsa_private_encrypt(const uint8* input, int length, uint32 key_length, const uint8* modulus, const uint8* private_exponent, uint8* output);
void crypto_rsa_private_decrypt(const uint8* input, int length, uint32 key_length, const uint8* modulus, const uint8* private_exponent, uint8* output);
void crypto_reverse(uint8* data, int length);
void crypto_nonce(uint8* nonce, int size);

#endif /* __CRYPTO_H */
