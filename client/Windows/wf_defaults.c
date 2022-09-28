#include <Windows.h>
#include <wincred.h>
#include <stdio.h>
#include <malloc.h>
#include <freerdp/settings.h>

static PCWSTR ValidateString(const BYTE* pb, ULONG cb)
{
	if (!pb || !cb)
		return 0;

	if (cb & (sizeof(WCHAR) - 1))
		return 0;

	if (*(WCHAR*)(pb + cb - sizeof(WCHAR)))
		return 0;

	return (PCWSTR)pb;
}

static void AddDefaultSettings_I(rdpSettings* settings, size_t idHostname, size_t idUsername,
                          size_t idPassword)
{
	PCSTR ServerHostname = freerdp_settings_get_string(settings, idHostname);

	if (!ServerHostname)
		return;

	BOOL bExistUserName = freerdp_settings_get_string(settings, idUsername) != 0;
	BOOL bExistPassword = freerdp_settings_get_string(settings, idPassword) != 0;

	if (bExistUserName && bExistPassword)
		return;

	PCREDENTIALA Credential;
	PCWSTR lpWideCharStr;

	PSTR Password;
	PSTR UserName;

	PSTR TargetName = 0;
	int len = _vsnprintf(TargetName, 0, "TERMSRV/%s", (va_list)&ServerHostname);
	len++;
	TargetName = (PSTR)malloc(len);
	_vsnprintf(TargetName, len, "TERMSRV/%s", (va_list)&ServerHostname);

	if (!TargetName)
		return;

	TargetName[len - 1] = 0;

	if (!CredReadA(TargetName, CRED_TYPE_GENERIC, 0, &Credential))
		return;

	if (!bExistPassword)
	{
		ULONG cch = Credential->CredentialBlobSize;

		if (lpWideCharStr = ValidateString(Credential->CredentialBlob, cch))
		{
			Password = 0, len = 0, cch /= sizeof(WCHAR);

			len = WideCharToMultiByte(CP_UTF8, 0, lpWideCharStr, cch, Password, len, 0, 0);
			Password = (PSTR)malloc(len);
			WideCharToMultiByte(CP_UTF8, 0, lpWideCharStr, cch, Password, len, 0, 0);
			
			if (Password)
			{
				freerdp_settings_set_string(settings, idPassword, Password);
			}
		}
	}

	if (!bExistUserName)
	{
		if (UserName = Credential->UserName)
		{
			freerdp_settings_set_string(settings, idUsername, UserName);
		}
	}

	CredFree(Credential);
}

void WINAPI AddDefaultSettings(_Inout_ rdpSettings* settings)
{
	AddDefaultSettings_I(settings, FreeRDP_ServerHostname, FreeRDP_Username, FreeRDP_Password);
	AddDefaultSettings_I(settings, FreeRDP_GatewayHostname, FreeRDP_GatewayUsername, FreeRDP_GatewayPassword);
}