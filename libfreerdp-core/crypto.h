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
#include <openssl/bn.h>
#include <openssl/x509v3.h>
#include <openssl/rand.h>

#if defined(OPENSSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x0090800f)
#define D2I_X509_CONST const
#else
#define D2I_X509_CONST
#endif

#define EXPONENT_MAX_SIZE			4
#define MODULUS_MAX_SIZE			64
#define CA_LOCAL_PATH   ".freerdp/cacert"

#include <freerdp/freerdp.h>
#include <freerdp/utils/blob.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/certstore.h>

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

struct crypto_cert_struct
{
	X509 * px509;
};

typedef struct crypto_sha1_struct* CryptoSha1;
CryptoSha1 crypto_sha1_init(void);
void crypto_sha1_update(CryptoSha1 sha1, uint8* data, uint32 length);
void crypto_sha1_final(CryptoSha1 sha1, uint8* out_data);

typedef struct crypto_md5_struct* CryptoMd5;
CryptoMd5 crypto_md5_init(void);
void crypto_md5_update(CryptoMd5 md5, uint8* data, uint32 length);
void crypto_md5_final(CryptoMd5 md5, uint8* out_data);

typedef struct crypto_rc4_struct* CryptoRc4;
CryptoRc4 crypto_rc4_init(uint8* key, uint32 length);
void crypto_rc4(CryptoRc4 rc4, uint32 length, uint8* in_data, uint8* out_data);
void crypto_rc4_free(CryptoRc4 rc4);

typedef struct crypto_cert_struct* CryptoCert;
CryptoCert crypto_cert_read(uint8* data, uint32 length);
char* cypto_cert_fingerprint(X509* xcert);
void crypto_cert_printinfo(X509* xcert);
void crypto_cert_free(CryptoCert cert);
boolean x509_verify_cert(CryptoCert cert);
boolean crypto_cert_verify(CryptoCert server_cert, CryptoCert cacert);
boolean crypto_cert_get_public_key(CryptoCert cert, rdpBlob* public_key);

void crypto_rsa_encrypt(uint8* input, int length, uint32 key_length, uint8* modulus, uint8* exponent, uint8* output);
void crypto_reverse(uint8* data, int length);
void crypto_nonce(uint8* nonce, int size);

#endif /* __CRYPTO_H */
