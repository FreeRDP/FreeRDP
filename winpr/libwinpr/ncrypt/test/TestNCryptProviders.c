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

int TestNCryptProviders(int argc, char* argv[])
{
	SECURITY_STATUS status;
	DWORD nproviders, i;
	NCryptProviderName* providers;

	status = NCryptEnumStorageProviders(&nproviders, &providers, NCRYPT_SILENT_FLAG);
	if (status != ERROR_SUCCESS)
		return -1;

	for (i = 0; i < nproviders; i++)
	{
		char providerNameStr[256];

		WideCharToMultiByte(CP_UTF8, 0, providers[i].pszName, -1, providerNameStr,
		                    sizeof(providerNameStr), NULL, FALSE);
		printf("%d: %s\n", i, providerNameStr);
	}

	NCryptFreeBuffer(providers);
	return 0;
}
