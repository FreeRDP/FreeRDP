/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Logging in with smartcards
 *
 * Copyright 2022 David Fort <contact@hardening-consulting.com>
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
#ifndef LIBFREERDP_CORE_SMARTCARDLOGON_H
#define LIBFREERDP_CORE_SMARTCARDLOGON_H

#include <freerdp/settings.h>
#include <freerdp/crypto/crypto.h>

typedef struct sSmartCardCerts SmartcardCerts;

typedef struct
{
	LPWSTR reader;
	CryptoCert certificate;
	char* pkinitArgs;
	UINT32 slotId;
	char* containerName;
	char* upn;
	char* userHint;
	char* domainHint;
	char* subject;
	char* issuer;
	BYTE atr[256];
	DWORD atrLength;
	BYTE sha1Hash[20];
} SmartcardCertInfo;

FREERDP_API BOOL smartcard_enumerateCerts(const rdpSettings* settings, SmartcardCerts** scCert,
                                          DWORD* retCount);
FREERDP_API const SmartcardCertInfo* smartcard_getCertInfo(SmartcardCerts* scCerts, DWORD index);
FREERDP_API void smartcardCerts_Free(SmartcardCerts* scCert);

#endif /* LIBFREERDP_CORE_SMARTCARDLOGON_H */
