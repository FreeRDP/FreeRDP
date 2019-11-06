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

struct rdp_certificate_data
{
	char* hostname;
	UINT16 port;
	char* subject;
	char* issuer;
	char* fingerprint;
};

struct rdp_certificate_store
{
	char* path;
	char* file;
	char* legacy_file;
	rdpSettings* settings;
	rdpCertificateData* certificate_data;
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API rdpCertificateData* certificate_data_new(const char* hostname, UINT16 port,
	                                                     const char* subject, const char* issuer,
	                                                     const char* fingerprint);
	FREERDP_API void certificate_data_free(rdpCertificateData* certificate_data);
	FREERDP_API rdpCertificateStore* certificate_store_new(rdpSettings* settings);
	FREERDP_API BOOL certificate_data_replace(rdpCertificateStore* certificate_store,
	                                          rdpCertificateData* certificate_data);
	FREERDP_API void certificate_store_free(rdpCertificateStore* certificate_store);
	FREERDP_API int certificate_data_match(rdpCertificateStore* certificate_store,
	                                       rdpCertificateData* certificate_data);
	FREERDP_API BOOL certificate_data_print(rdpCertificateStore* certificate_store,
	                                        rdpCertificateData* certificate_data);
	FREERDP_API BOOL certificate_get_stored_data(rdpCertificateStore* certificate_store,
	                                             rdpCertificateData* certificate_data,
	                                             char** subject, char** issuer, char** fingerprint);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CRYPTO_CERTIFICATE_H */
