/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Transport Layer Security
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

#ifndef __TLS_H
#define __TLS_H

#include "crypto.h"
#include "certificate.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <freerdp/types.h>
#include <freerdp/utils/stream.h>

typedef struct rdp_tls rdpTls;

struct rdp_tls
{
	SSL* ssl;
	int sockfd;
	SSL_CTX* ctx;
	rdpSettings* settings;
	rdpCertificateStore* certificate_store;
};

boolean tls_connect(rdpTls* tls);
boolean tls_accept(rdpTls* tls, const char* cert_file, const char* privatekey_file);
boolean tls_disconnect(rdpTls* tls);

int tls_read(rdpTls* tls, uint8* data, int length);
int tls_write(rdpTls* tls, uint8* data, int length);

CryptoCert tls_get_certificate(rdpTls* tls);
boolean tls_verify_certificate(rdpTls* tls, CryptoCert cert, char* hostname);
void tls_print_certificate_error(char* hostname, char* fingerprint);
void tls_print_certificate_name_mismatch_error(char* hostname, char* common_name, char** alt_names, int alt_names_count);

boolean tls_print_error(char* func, SSL* connection, int value);

rdpTls* tls_new(rdpSettings* settings);
void tls_free(rdpTls* tls);

#endif /* __TLS_H */
