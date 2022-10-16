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

typedef struct SmartcardKeyInfo_st SmartcardKeyInfo;

typedef struct SmartcardCertInfo_st
{
	LPWSTR csp;
	LPWSTR reader;
	CryptoCert certificate;
	char* pkinitArgs;
	UINT32 slotId;
	char* keyName;
	WCHAR* containerName;
	char* upn;
	char* userHint;
	char* domainHint;
	char* subject;
	char* issuer;
	BYTE sha1Hash[20];
	SmartcardKeyInfo* key_info;
} SmartcardCertInfo;

FREERDP_API BOOL smartcard_enumerateCerts(const rdpSettings* settings, SmartcardCertInfo*** scCerts,
                                          DWORD* retCount, BOOL gateway);
FREERDP_API BOOL smartcard_getCert(const rdpContext* context, SmartcardCertInfo** cert,
                                   BOOL gateway);
FREERDP_API void smartcardCertInfo_Free(SmartcardCertInfo* pscCert);
FREERDP_API void smartcardCertList_Free(SmartcardCertInfo** pscCert, DWORD count);

#endif /* LIBFREERDP_CORE_SMARTCARDLOGON_H */
