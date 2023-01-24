/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Certificate Handling
 *
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

#ifndef FREERDP_CERTIFICATE_H
#define FREERDP_CERTIFICATE_H

#include <freerdp/api.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/* Certificates */

	struct rdp_CertBlob
	{
		UINT32 length;
		BYTE* data;
	};
	typedef struct rdp_CertBlob rdpCertBlob;

	struct rdp_X509CertChain
	{
		UINT32 count;
		rdpCertBlob* array;
	};
	typedef struct rdp_X509CertChain rdpX509CertChain;

	struct rdp_CertInfo
	{
		BYTE* Modulus;
		DWORD ModulusLength;
		BYTE exponent[4];
	};
	typedef struct rdp_CertInfo rdpCertInfo;

	struct rdp_certificate
	{
		rdpCertInfo cert_info;
		rdpX509CertChain x509_cert_chain;
	};
	typedef struct rdp_certificate rdpCertificate;

	typedef struct
	{
		rdpCertInfo cert;
		BYTE* PrivateExponent;
		DWORD PrivateExponentLength;
	} rdpRsaKey;

	FREERDP_API rdpCertificate* freerdp_certificate_new(void);
	FREERDP_API rdpCertificate* freerdp_certificate_new_from_file(const char* file);
	FREERDP_API rdpCertificate* freerdp_certificate_new_from_pem(const char* pem);
	FREERDP_API void freerdp_certificate_free(rdpCertificate* certificate);
	FREERDP_API void* freerdp_certificate_get_x509(const rdpCertificate* certificate);

	FREERDP_API rdpRsaKey* freerdp_key_new_from_file(const char* keyfile);
	FREERDP_API rdpRsaKey* freerdp_key_new_from_pem(const char* keycontent, const char* keyfile);
	FREERDP_API void freerdp_key_free(rdpRsaKey* key);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CERTIFICATE_H */
