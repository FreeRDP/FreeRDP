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

typedef struct rdp_certificate rdpCertificate;

#include "rdp.h"
#include "ber.h"
#include "crypto.h"

#include <freerdp/utils/blob.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/hexdump.h>

/* Certificate Version */
#define CERT_CHAIN_VERSION_1		0x00000001
#define CERT_CHAIN_VERSION_2		0x00000002
#define CERT_CHAIN_VERSION_MASK		0x7FFFFFFF
#define CERT_PERMANENTLY_ISSUED		0x00000000
#define CERT_TEMPORARILY_ISSUED		0x80000000

typedef struct
{
	uint32 length;
	uint8* data;
} CERT_BLOB;

typedef struct
{
	uint32 count;
	CERT_BLOB* array;
} X509_CERT_CHAIN;

typedef struct
{
	BLOB modulus;
	uint8 exponent[4];
} CERT_INFO;

struct rdp_certificate
{
	struct rdp_rdp* rdp;
	CERT_INFO cert_info;
	X509_CERT_CHAIN* x509_cert_chain;
};

void certificate_read_x509_certificate(CERT_BLOB* cert, CERT_INFO* info);

X509_CERT_CHAIN* certificate_new_x509_certificate_chain(uint32 count);
void certificate_free_x509_certificate_chain(X509_CERT_CHAIN* x509_cert_chain);

void certificate_read_server_proprietary_certificate(rdpCertificate* certificate, STREAM* s);
void certificate_read_server_x509_certificate_chain(rdpCertificate* certificate, STREAM* s);
void certificate_read_server_certificate(rdpCertificate* certificate, uint8* server_cert, int length);

rdpCertificate* certificate_new(rdpRdp* rdp);
void certificate_free(rdpCertificate* certificate);

//#define WITH_DEBUG_CERTIFICATE	1

#ifdef WITH_DEBUG_CERTIFICATE
#define DEBUG_CERTIFICATE(fmt, ...) DEBUG_CLASS(CERTIFICATE, fmt, ## __VA_ARGS__)
#else
#define DEBUG_CERTIFICATE(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* __CERTIFICATE_H */
