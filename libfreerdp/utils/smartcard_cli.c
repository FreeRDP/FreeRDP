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
#include <freerdp/utils/smartcard_cli.h>
#include "../core/smartcardlogon.h"

BOOL freerdp_smartcard_list(rdpSettings* settings)
{
	SmartcardCert certs[64] = { 0 };
	DWORD i, count;

	if (!smartcard_enumerateCerts(settings, certs, 64, &count))
		return FALSE;

	for (i = 0; i < count; i++)
	{
		char readerName[256] = { 0 };

		printf("%d: %s\n", i, certs[i].subject);

		if (WideCharToMultiByte(CP_UTF8, 0, certs[i].reader, -1, readerName, sizeof(readerName),
		                        NULL, NULL) > 0)
			printf("\t* reader: %s\n", readerName);
#ifndef _WIN32
		printf("\t* slotId: %" PRIu32 "\n", certs[i].slotId);
		printf("\t* pkinitArgs: %s\n", certs[i].pkinitArgs);
#endif
		printf("\t* containerName: %s\n", certs[i].containerName);
		if (certs[i].upn)
			printf("\t* UPN: %s\n", certs[i].upn);

		smartcardCert_Free(&certs[i]);
	}
	return TRUE;
}
