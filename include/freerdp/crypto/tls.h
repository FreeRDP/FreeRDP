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

#define TLS_ALERT_LEVEL_WARNING				1
#define TLS_ALERT_LEVEL_FATAL				2

#define TLS_ALERT_DESCRIPTION_CLOSE_NOTIFY		0
#define TLS_ALERT_DESCRIPTION_UNEXPECTED_MESSAGE	10
#define TLS_ALERT_DESCRIPTION_BAD_RECORD_MAC		20
#define TLS_ALERT_DESCRIPTION_DECRYPTION_FAILED		21
#define TLS_ALERT_DESCRIPTION_RECORD_OVERFLOW		22
#define TLS_ALERT_DESCRIPTION_DECOMPRESSION_FAILURE	30
#define TLS_ALERT_DESCRIPTION_HANSHAKE_FAILURE		40
#define TLS_ALERT_DESCRIPTION_NO_CERTIFICATE		41
#define TLS_ALERT_DESCRIPTION_BAD_CERTIFICATE		42
#define TLS_ALERT_DESCRIPTION_UNSUPPORTED_CERTIFICATE	43
#define TLS_ALERT_DESCRIPTION_CERTIFICATE_REVOKED	44
#define TLS_ALERT_DESCRIPTION_CERTIFICATE_EXPIRED	45
#define TLS_ALERT_DESCRIPTION_CERTIFICATE_UNKNOWN	46
#define TLS_ALERT_DESCRIPTION_ILLEGAL_PARAMETER		47
#define TLS_ALERT_DESCRIPTION_UNKNOWN_CA		48
#define TLS_ALERT_DESCRIPTION_ACCESS_DENIED		49
#define TLS_ALERT_DESCRIPTION_DECODE_ERROR		50
#define TLS_ALERT_DESCRIPTION_DECRYPT_ERROR		51
#define TLS_ALERT_DESCRIPTION_EXPORT_RESTRICTION	60
#define TLS_ALERT_DESCRIPTION_PROTOCOL_VERSION		70
#define TLS_ALERT_DESCRIPTION_INSUFFICIENT_SECURITY	71
#define TLS_ALERT_DESCRIPTION_INTERNAL_ERROR		80
#define TLS_ALERT_DESCRIPTION_USER_CANCELED		90
#define TLS_ALERT_DESCRIPTION_NO_RENEGOTIATION		100
#define TLS_ALERT_DESCRIPTION_UNSUPPORTED_EXTENSION	110

typedef struct rdp_tls rdpTls;

struct rdp_tls
{
	SSL* ssl;
	BIO* bio;
	void* tsg;
	SSL_CTX* ctx;
	BYTE* PublicKey;
	DWORD PublicKeyLength;
	rdpSettings* settings;
	SecPkgContext_Bindings* Bindings;
	rdpCertificateStore* certificate_store;
	BIO* underlying;
	char* hostname;
	int port;
	int alertLevel;
	int alertDescription;
	BOOL isGatewayTransport;
};

#ifdef __cplusplus
 extern "C" {
#endif

FREERDP_API int tls_connect(rdpTls* tls, BIO *underlying);
FREERDP_API BOOL tls_accept(rdpTls* tls, BIO *underlying, rdpSettings *settings);
FREERDP_API BOOL tls_send_alert(rdpTls* tls);

FREERDP_API int tls_write_all(rdpTls* tls, const BYTE* data, int length);

FREERDP_API int tls_set_alert_code(rdpTls* tls, int level, int description);

FREERDP_API BOOL tls_match_hostname(char *pattern, int pattern_length, char *hostname);
FREERDP_API int tls_verify_certificate(rdpTls* tls, CryptoCert cert, char* hostname, int port);
FREERDP_API void tls_print_certificate_error(char* hostname, UINT16 port,
                                             char* fingerprint, char* hosts_file);
FREERDP_API void tls_print_certificate_name_mismatch_error(
        char* hostname, UINT16 port, char* common_name, char** alt_names,
        int alt_names_count);

FREERDP_API BOOL tls_print_error(char* func, SSL* connection, int value);

FREERDP_API rdpTls* tls_new(rdpSettings* settings);
FREERDP_API void tls_free(rdpTls* tls);

#ifdef __cplusplus
 }
#endif

#endif /* FREERDP_CRYPTO_TLS_H */
