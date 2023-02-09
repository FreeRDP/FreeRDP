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

#ifndef FREERDP_CRYPTO_CERTIFICATE_STORE_H
#define FREERDP_CRYPTO_CERTIFICATE_STORE_H

typedef struct rdp_certificate_store rdpCertificateStore;

#include <freerdp/api.h>
#include <freerdp/settings.h>
#include <freerdp/crypto/certificate_data.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef enum
	{
		CERT_STORE_NOT_FOUND = 1,
		CERT_STORE_MATCH = 0,
		CERT_STORE_MISMATCH = -1
	} freerdp_certificate_store_result;

	FREERDP_API rdpCertificateStore* freerdp_certificate_store_new(const rdpSettings* settings);
	FREERDP_API void freerdp_certificate_store_free(rdpCertificateStore* store);

	FREERDP_API freerdp_certificate_store_result freerdp_certificate_store_contains_data(
	    rdpCertificateStore* store, const rdpCertificateData* data);
	FREERDP_API rdpCertificateData*
	freerdp_certificate_store_load_data(rdpCertificateStore* store, const char* host, UINT16 port);
	FREERDP_API BOOL freerdp_certificate_store_save_data(rdpCertificateStore* store,
	                                                     const rdpCertificateData* data);
	FREERDP_API BOOL freerdp_certificate_store_remove_data(rdpCertificateStore* store,
	                                                       const rdpCertificateData* data);

	FREERDP_API const char*
	freerdp_certificate_store_get_certs_path(const rdpCertificateStore* store);
	FREERDP_API const char*
	freerdp_certificate_store_get_hosts_path(const rdpCertificateStore* store);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CRYPTO_CERTIFICATE_STORE_H */
