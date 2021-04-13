/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Certificate Handling
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

#ifndef FREERDP_CRYPTO_CERTIFICATE_H
#define FREERDP_CRYPTO_CERTIFICATE_H

typedef struct rdp_certificate_data rdpCertificateData;
typedef struct rdp_certificate_store rdpCertificateStore;

#include <freerdp/crypto/ber.h>
#include <freerdp/crypto/crypto.h>

#include <freerdp/api.h>
#include <freerdp/settings.h>

#include <winpr/print.h>
#include <winpr/stream.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API rdpCertificateData* certificate_data_new(const char* hostname, UINT16 port);
	FREERDP_API void certificate_data_free(rdpCertificateData* certificate_data);

	FREERDP_API const char* certificate_data_get_host(const rdpCertificateData* cert);
	FREERDP_API UINT16 certificate_data_get_port(const rdpCertificateData* cert);

	FREERDP_API BOOL certificate_data_set_pem(rdpCertificateData* cert, const char* pem);
	FREERDP_API BOOL certificate_data_set_subject(rdpCertificateData* cert, const char* subject);
	FREERDP_API BOOL certificate_data_set_issuer(rdpCertificateData* cert, const char* issuer);
	FREERDP_API BOOL certificate_data_set_fingerprint(rdpCertificateData* cert,
	                                                  const char* fingerprint);
	FREERDP_API const char* certificate_data_get_pem(const rdpCertificateData* cert);
	FREERDP_API const char* certificate_data_get_subject(const rdpCertificateData* cert);
	FREERDP_API const char* certificate_data_get_issuer(const rdpCertificateData* cert);
	FREERDP_API const char* certificate_data_get_fingerprint(const rdpCertificateData* cert);

	FREERDP_API rdpCertificateStore* certificate_store_new(const rdpSettings* settings);
	FREERDP_API void certificate_store_free(rdpCertificateStore* certificate_store);

	FREERDP_API int certificate_store_contains_data(rdpCertificateStore* certificate_store,
	                                                const rdpCertificateData* certificate_data);
	FREERDP_API rdpCertificateData*
	certificate_store_load_data(rdpCertificateStore* certificate_store, const char* host,
	                            UINT16 port);
	FREERDP_API BOOL certificate_store_save_data(rdpCertificateStore* certificate_store,
	                                             const rdpCertificateData* certificate_data);
	FREERDP_API BOOL certificate_store_remove_data(rdpCertificateStore* certificate_store,
	                                               const rdpCertificateData* certificate_data);

	FREERDP_API const char*
	certificate_store_get_hosts_file(const rdpCertificateStore* certificate_store);
	FREERDP_API const char*
	certificate_store_get_certs_path(const rdpCertificateStore* certificate_store);
	FREERDP_API const char*
	certificate_store_get_hosts_path(const rdpCertificateStore* certificate_store);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CRYPTO_CERTIFICATE_H */
