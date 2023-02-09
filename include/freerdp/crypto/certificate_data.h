/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Certificate Handling
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CRYPTO_CERTIFICATE_DATA_H
#define FREERDP_CRYPTO_CERTIFICATE_DATA_H

typedef struct rdp_certificate_data rdpCertificateData;

#include <freerdp/api.h>
#include <freerdp/settings.h>
#include <freerdp/crypto/certificate.h>

#ifdef __cplusplus
extern "C"
{
#endif
	FREERDP_API char* freerdp_certificate_data_hash(const char* hostname, UINT16 port);

	FREERDP_API rdpCertificateData* freerdp_certificate_data_new(const char* hostname, UINT16 port,
	                                                             const rdpCertificate* xcert);
	FREERDP_API rdpCertificateData* freerdp_certificate_data_new_from_pem(const char* hostname,
	                                                                      UINT16 port,
	                                                                      const char* pem,
	                                                                      size_t length);
	FREERDP_API rdpCertificateData*
	freerdp_certificate_data_new_from_file(const char* hostname, UINT16 port, const char* file);

	FREERDP_API void freerdp_certificate_data_free(rdpCertificateData* data);

	FREERDP_API BOOL freerdp_certificate_data_equal(const rdpCertificateData* a,
	                                                const rdpCertificateData* b);

	FREERDP_API const char* freerdp_certificate_data_get_hash(const rdpCertificateData* cert);

	FREERDP_API const char* freerdp_certificate_data_get_host(const rdpCertificateData* cert);
	FREERDP_API UINT16 freerdp_certificate_data_get_port(const rdpCertificateData* cert);

	FREERDP_API const char* freerdp_certificate_data_get_pem(const rdpCertificateData* cert);
	FREERDP_API const char* freerdp_certificate_data_get_subject(const rdpCertificateData* cert);
	FREERDP_API const char* freerdp_certificate_data_get_issuer(const rdpCertificateData* cert);
	FREERDP_API const char*
	freerdp_certificate_data_get_fingerprint(const rdpCertificateData* cert);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CRYPTO_CERTIFICATE_DATA_H */
