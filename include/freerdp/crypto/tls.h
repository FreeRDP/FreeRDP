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

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/utils/stream.h>

typedef struct rdp_tls rdpTls;

struct rdp_tls
{
	SSL* ssl;
	int sockfd;
	SSL_CTX* ctx;
	rdpBlob public_key;
	rdpSettings* settings;
	rdpCertificateStore* certificate_store;
};

FREERDP_API boolean tls_connect(rdpTls* tls);
FREERDP_API boolean tls_accept(rdpTls* tls, const char* cert_file, const char* privatekey_file);
FREERDP_API boolean tls_disconnect(rdpTls* tls);

FREERDP_API int tls_read(rdpTls* tls, uint8* data, int length);
FREERDP_API int tls_write(rdpTls* tls, uint8* data, int length);

FREERDP_API boolean tls_verify_certificate(rdpTls* tls, CryptoCert cert, char* hostname);
FREERDP_API void tls_print_certificate_error(char* hostname, char* fingerprint);
FREERDP_API void tls_print_certificate_name_mismatch_error(char* hostname, char* common_name, char** alt_names, int alt_names_count);

FREERDP_API boolean tls_print_error(char* func, SSL* connection, int value);

FREERDP_API rdpTls* tls_new(rdpSettings* settings);
FREERDP_API void tls_free(rdpTls* tls);

#endif /* FREERDP_CRYPTO_TLS_H */
