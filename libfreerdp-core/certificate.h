/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Certificate Handling
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

#ifndef __CERTIFICATE_H
#define __CERTIFICATE_H

typedef struct rdp_certificate_data rdpCertificateData;
typedef struct rdp_certificate_store rdpCertificateStore;

#include "rdp.h"
#include "ber.h"
#include "crypto.h"

#include <freerdp/settings.h>
#include <freerdp/utils/blob.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/hexdump.h>

/* Certificate Version */
#define CERT_CHAIN_VERSION_1		0x00000001
#define CERT_CHAIN_VERSION_2		0x00000002
#define CERT_CHAIN_VERSION_MASK		0x7FFFFFFF
#define CERT_PERMANENTLY_ISSUED		0x00000000
#define CERT_TEMPORARILY_ISSUED		0x80000000

#define SIGNATURE_ALG_RSA		0x00000001
#define KEY_EXCHANGE_ALG_RSA		0x00000001

#define BB_RSA_KEY_BLOB        		6
#define BB_RSA_SIGNATURE_BLOB  		8

struct rdp_key
{
	rdpBlob modulus;
	rdpBlob private_exponent;
	uint8 exponent[4];
};

struct rdp_certificate_data
{
	char* hostname;
	char* fingerprint;
};

struct rdp_certificate_store
{
	FILE* fp;
	char* path;
	char* file;
	rdpSettings* settings;
	rdpCertificateData* certificate_data;
};

rdpCertificateData* certificate_data_new(char* hostname, char* fingerprint);
void certificate_data_free(rdpCertificateData* certificate_data);
rdpCertificateStore* certificate_store_new(rdpSettings* settings);
void certificate_store_free(rdpCertificateStore* certificate_store);
int certificate_data_match(rdpCertificateStore* certificate_store, rdpCertificateData* certificate_data);
void certificate_data_print(rdpCertificateStore* certificate_store, rdpCertificateData* certificate_data);

void certificate_read_x509_certificate(rdpCertBlob* cert, rdpCertInfo* info);

rdpX509CertChain* certificate_new_x509_certificate_chain(uint32 count);
void certificate_free_x509_certificate_chain(rdpX509CertChain* x509_cert_chain);

boolean certificate_read_server_proprietary_certificate(rdpCertificate* certificate, STREAM* s);
boolean certificate_read_server_x509_certificate_chain(rdpCertificate* certificate, STREAM* s);
boolean certificate_read_server_certificate(rdpCertificate* certificate, uint8* server_cert, int length);

rdpCertificate* certificate_new();
void certificate_free(rdpCertificate* certificate);

rdpKey* key_new(const char *keyfile);
void key_free(rdpKey* key);

#ifdef WITH_DEBUG_CERTIFICATE
#define DEBUG_CERTIFICATE(fmt, ...) DEBUG_CLASS(CERTIFICATE, fmt, ## __VA_ARGS__)
#else
#define DEBUG_CERTIFICATE(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* __CERTIFICATE_H */
