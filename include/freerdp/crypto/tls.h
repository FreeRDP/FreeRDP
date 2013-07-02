/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Transport Layer Security
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

#ifndef FREERDP_CRYPTO_TLS_H
#define FREERDP_CRYPTO_TLS_H

#include "crypto.h"
#include "certificate.h"

#include <winpr/crt.h>
#include <winpr/sspi.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <winpr/stream.h>

typedef struct rdp_tls rdpTls;

struct rdp_tls
{
	SSL* ssl;
	int sockfd;
	SSL_CTX* ctx;
	BYTE* PublicKey;
	DWORD PublicKeyLength;
	rdpSettings* settings;
	SecPkgContext_Bindings* Bindings;
	rdpCertificateStore* certificate_store;
};

FREERDP_API BOOL tls_connect(rdpTls* tls);
FREERDP_API BOOL tls_accept(rdpTls* tls, const char* cert_file, const char* privatekey_file);
FREERDP_API BOOL tls_disconnect(rdpTls* tls);

FREERDP_API int tls_read(rdpTls* tls, BYTE* data, int length);
FREERDP_API int tls_write(rdpTls* tls, BYTE* data, int length);

FREERDP_API int tls_read_all(rdpTls* tls, BYTE* data, int length);
FREERDP_API int tls_write_all(rdpTls* tls, BYTE* data, int length);

FREERDP_API int tls_wait_read(rdpTls* tls);
FREERDP_API int tls_wait_write(rdpTls* tls);

FREERDP_API BOOL tls_match_hostname(char *pattern, int pattern_length, char *hostname);
FREERDP_API BOOL tls_verify_certificate(rdpTls* tls, CryptoCert cert, char* hostname);
FREERDP_API void tls_print_certificate_error(char* hostname, char* fingerprint, char* hosts_file);
FREERDP_API void tls_print_certificate_name_mismatch_error(char* hostname, char* common_name, char** alt_names, int alt_names_count);

FREERDP_API BOOL tls_print_error(char* func, SSL* connection, int value);

FREERDP_API rdpTls* tls_new(rdpSettings* settings);
FREERDP_API void tls_free(rdpTls* tls);

#endif /* FREERDP_CRYPTO_TLS_H */
