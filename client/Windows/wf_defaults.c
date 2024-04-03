/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2022 Stefan Koell
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

#include <windows.h>
#include <wincred.h>
#include <stdio.h>
#include <malloc.h>
#include <freerdp/settings.h>

#include "wf_defaults.h"

static PCWSTR ValidateString(const BYTE* pb, ULONG cb)
{
	if (!pb || !cb)
		return 0;

	if (cb % sizeof(WCHAR) != 0)
		return 0;

	return (PCWSTR)pb;
}

static void AddDefaultSettings_I(rdpSettings* settings, size_t idHostname, size_t idUsername,
                                 size_t idDomain, size_t idPassword)
{
	static const PSTR TERMSRV = "TERMSRV/%s";

	PSTR TargetName = NULL;
	PSTR UserName = NULL;
	PWSTR TargetNameW = NULL;
	PWSTR ServerHostNameW = NULL;
	PWSTR ParsedUserNameW = NULL;
	PWSTR ParsedDomainW = NULL;
	PWSTR PasswordNullTerminatedW = NULL;
	PCREDENTIALW Credential = { 0 };

	PCSTR ServerHostname = freerdp_settings_get_string(settings, idHostname);

	if (!ServerHostname)
		return;

	BOOL bExistUserName = freerdp_settings_get_string(settings, idUsername) != 0;
	BOOL bExistPassword = freerdp_settings_get_string(settings, idPassword) != 0;

	if (bExistUserName && bExistPassword)
		return;

	int len = _snprintf(TargetName, 0, TERMSRV, ServerHostname);
	len++;
	TargetName = (PSTR)malloc(len);

	if (!TargetName)
		goto fail;

	_snprintf(TargetName, len, TERMSRV, ServerHostname);

	TargetName[len - 1] = 0;

	TargetNameW = ConvertUtf8ToWCharAlloc(TargetName, NULL);
	if (!TargetNameW)
		goto fail;

	if (!CredReadW(TargetNameW, CRED_TYPE_GENERIC, 0, &Credential))
		goto fail;

	if (!bExistPassword)
	{
		const WCHAR* PasswordW =
		    ValidateString(Credential->CredentialBlob, Credential->CredentialBlobSize);

		PasswordNullTerminatedW = (PWSTR)calloc(Credential->CredentialBlobSize + 1, sizeof(WCHAR));

		if (!PasswordNullTerminatedW)
			goto fail;

		memcpy(PasswordNullTerminatedW, PasswordW, Credential->CredentialBlobSize * sizeof(WCHAR));

		if (PasswordNullTerminatedW)
		{
			if (!freerdp_settings_set_string_from_utf16(settings, idPassword,
			                                            PasswordNullTerminatedW))
			{
				goto fail;
			}
		}
	}

	if (!bExistUserName)
	{
		const WCHAR* UserNameW = Credential->UserName;

		if (UserNameW)
		{
			ParsedUserNameW = calloc(CREDUI_MAX_USERNAME_LENGTH + 1, sizeof(WCHAR));
			if (!ParsedUserNameW)
				goto fail;

			ParsedDomainW = calloc(CREDUI_MAX_DOMAIN_TARGET_LENGTH + 1, sizeof(WCHAR));
			if (!ParsedDomainW)
				goto fail;

			DWORD ParseResult =
			    CredUIParseUserNameW(UserNameW, ParsedUserNameW, CREDUI_MAX_USERNAME_LENGTH + 1,
			                         ParsedDomainW, CREDUI_MAX_DOMAIN_TARGET_LENGTH + 1);

			if (ParseResult == NO_ERROR)
			{
				if (!freerdp_settings_set_string_from_utf16(settings, idUsername, ParsedUserNameW))
					goto fail;

				if (*ParsedDomainW != 0)
				{
					if (!freerdp_settings_set_string_from_utf16(settings, idDomain, ParsedDomainW))
						goto fail;
				}
			}
			else if (ParseResult == ERROR_INVALID_ACCOUNT_NAME)
			{
				if (!freerdp_settings_set_string_from_utf16(settings, idUsername, UserNameW))
					goto fail;
			}
		}
	}

fail:
	if (Credential)
	{
		CredFree(Credential);
	}
	free(TargetName);
	free(UserName);
	free(TargetNameW);
	free(ServerHostNameW);
	free(ParsedUserNameW);
	free(ParsedDomainW);
	free(PasswordNullTerminatedW);
	return;
}

void WINAPI AddDefaultSettings(rdpSettings* settings)
{
	AddDefaultSettings_I(settings, FreeRDP_ServerHostname, FreeRDP_Username, FreeRDP_Domain,
	                     FreeRDP_Password);
	AddDefaultSettings_I(settings, FreeRDP_GatewayHostname, FreeRDP_GatewayUsername,
	                     FreeRDP_GatewayDomain, FreeRDP_GatewayPassword);
}
