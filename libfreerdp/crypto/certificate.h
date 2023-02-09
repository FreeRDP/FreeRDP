/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Certificate Handling
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#include <freerdp/crypto/crypto.h>
#include <freerdp/crypto/certificate.h>

#include <openssl/x509.h>

/* Certificate Version */
#define CERT_CHAIN_VERSION_1 0x00000001
#define CERT_CHAIN_VERSION_2 0x00000002
#define CERT_CHAIN_VERSION_MASK 0x7FFFFFFF
#define CERT_PERMANENTLY_ISSUED 0x00000000
#define CERT_TEMPORARILY_ISSUED 0x80000000

#define SIGNATURE_ALG_RSA 0x00000001
#define KEY_EXCHANGE_ALG_RSA 0x00000001

#define BB_RSA_KEY_BLOB 6
#define BB_RSA_SIGNATURE_BLOB 8

FREERDP_LOCAL rdpCertificate* freerdp_certificate_new_from_x509(const X509* xcert,
                                                                const STACK_OF(X509) * chain);

FREERDP_LOCAL BOOL freerdp_certificate_read_server_cert(rdpCertificate* certificate,
                                                        const BYTE* server_cert, size_t length);
FREERDP_LOCAL SSIZE_T freerdp_certificate_write_server_cert(const rdpCertificate* certificate,
                                                            UINT32 dwVersion, wStream* s);

FREERDP_LOCAL rdpCertificate* freerdp_certificate_clone(const rdpCertificate* certificate);

FREERDP_LOCAL const rdpCertInfo* freerdp_certificate_get_info(const rdpCertificate* certificate);

/** \brief returns a pointer to a X509 structure.
 *  Call X509_free when done.
 */
FREERDP_LOCAL X509* freerdp_certificate_get_x509(rdpCertificate* certificate);

/** \brief returns a pointer to a RSA structure.
 *  Call RSA_free when done.
 */
FREERDP_LOCAL RSA* freerdp_certificate_get_RSA(const rdpCertificate* key);

#endif /* FREERDP_LIB_CORE_CERTIFICATE_H */
