/**
 * WinPR: Windows Portable Runtime
 * Test for NCrypt library
 *
 * Copyright 2021 David Fort <contact@hardening-consulting.com>
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
#include <winpr/error.h>
#include <winpr/ncrypt.h>
#include <winpr/string.h>
#include <winpr/wlog.h>
#include <winpr/smartcard.h>

#define TAG "testNCrypt"

int TestNCryptSmartcard(int argc, char* argv[])
{
	SECURITY_STATUS status;
	NCRYPT_PROV_HANDLE provider;
	PVOID enumState = NULL;
	NCryptKeyName* keyName = NULL;

	status = NCryptOpenStorageProvider(&provider, MS_SMART_CARD_KEY_STORAGE_PROVIDER, 0);
	if (status != ERROR_SUCCESS)
		return 0;

	while ((status = NCryptEnumKeys(provider, NULL, &keyName, &enumState, NCRYPT_SILENT_FLAG)) ==
	       ERROR_SUCCESS)
	{
		NCRYPT_KEY_HANDLE phKey;
		DWORD dwFlags = 0, cbOutput;
		char keyNameStr[256];
		PBYTE certBytes = NULL;

		WideCharToMultiByte(CP_UTF8, 0, keyName->pszName, -1, keyNameStr, sizeof(keyNameStr), NULL,
		                    FALSE);

		status =
		    NCryptOpenKey(provider, &phKey, keyName->pszName, keyName->dwLegacyKeySpec, dwFlags);
		if (status != ERROR_SUCCESS)
		{
			WLog_ERR(TAG, "unable to open key %s", keyNameStr);
			continue;
		}

		status = NCryptGetProperty(phKey, NCRYPT_CERTIFICATE_PROPERTY, NULL, 0, &cbOutput, dwFlags);
		if (status != ERROR_SUCCESS)
		{
			WLog_ERR(TAG, "unable to retrieve certificate for key %s", keyNameStr);
			NCryptFreeObject((NCRYPT_HANDLE)phKey);
			continue;
		}

		certBytes = calloc(1, cbOutput);
		status = NCryptGetProperty(phKey, NCRYPT_CERTIFICATE_PROPERTY, certBytes, cbOutput,
		                           &cbOutput, dwFlags);
		if (status != ERROR_SUCCESS)
		{
			WLog_ERR(TAG, "unable to retrieve certificate for key %s", keyNameStr);
			NCryptFreeObject((NCRYPT_HANDLE)phKey);
			continue;
		}

		NCryptFreeObject((NCRYPT_HANDLE)phKey);
	}

	NCryptFreeObject((NCRYPT_HANDLE)enumState);
	NCryptFreeObject((NCRYPT_HANDLE)provider);
	return 0;
}
