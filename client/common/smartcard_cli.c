/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Smartcard client functions
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

#include <winpr/assert.h>

#include <freerdp/client/utils/smartcard_cli.h>
#include <freerdp/utils/smartcardlogon.h>

BOOL freerdp_smartcard_list(const rdpSettings* settings)
{
	SmartcardCertInfo** certs = NULL;
	DWORD i, count;

	if (!smartcard_enumerateCerts(settings, &certs, &count, FALSE))
		return FALSE;

	for (i = 0; i < count; i++)
	{
		const SmartcardCertInfo* info = certs[i];
		char asciiStr[256] = { 0 };

		WINPR_ASSERT(info);

		printf("%" PRIu32 ": %s\n", i, info->subject);

		if (WideCharToMultiByte(CP_UTF8, 0, info->csp, -1, asciiStr, sizeof(asciiStr), NULL, NULL) >
		    0)
			printf("\t* CSP: %s\n", asciiStr);

		if (WideCharToMultiByte(CP_UTF8, 0, info->reader, -1, asciiStr, sizeof(asciiStr), NULL,
		                        NULL) > 0)
			printf("\t* reader: %s\n", asciiStr);
#ifndef _WIN32
		printf("\t* slotId: %" PRIu32 "\n", info->slotId);
		printf("\t* pkinitArgs: %s\n", info->pkinitArgs);
#endif
		if (WideCharToMultiByte(CP_UTF8, 0, info->containerName, -1, asciiStr, sizeof(asciiStr),
		                        NULL, NULL) > 0)
			printf("\t* containerName: %s\n", asciiStr);
		if (info->upn)
			printf("\t* UPN: %s\n", info->upn);
	}
	smartcardCertList_Free(certs, count);

	return TRUE;
}
