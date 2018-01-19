/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_LIB_CORE_CERTIFICATE_H
#define FREERDP_LIB_CORE_CERTIFICATE_H

#include "rdp.h"

#include <freerdp/crypto/ber.h>
#include <freerdp/crypto/crypto.h>

#include <freerdp/settings.h>
#include <freerdp/log.h>
#include <freerdp/api.h>

#include <winpr/print.h>
#include <winpr/stream.h>

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

FREERDP_LOCAL BOOL certificate_read_server_certificate(rdpCertificate*
        certificate, BYTE* server_cert, size_t length);

FREERDP_LOCAL rdpCertificate* certificate_clone(rdpCertificate* certificate);

FREERDP_LOCAL rdpCertificate* certificate_new(void);
FREERDP_LOCAL void certificate_free(rdpCertificate* certificate);

FREERDP_LOCAL rdpRsaKey* key_new(const char* keyfile);
FREERDP_LOCAL rdpRsaKey* key_new_from_content(const char* keycontent,
        const char* keyfile);
FREERDP_LOCAL void key_free(rdpRsaKey* key);

#define CERTIFICATE_TAG FREERDP_TAG("core.certificate")
#ifdef WITH_DEBUG_CERTIFICATE
#define DEBUG_CERTIFICATE(...) WLog_DBG(CERTIFICATE_TAG, __VA_ARGS__)
#else
#define DEBUG_CERTIFICATE(...) do { } while (0)
#endif

#endif /* FREERDP_LIB_CORE_CERTIFICATE_H */
