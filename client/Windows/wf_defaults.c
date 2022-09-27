#include <Windows.h>
#include <wincred.h>
#include <stdio.h>
#include <malloc.h>

#pragma warning(disable : 4706 4094) // assignment within conditional expression

#define FreeRDP_ServerHostname (20)
#define FreeRDP_Username (21)
#define FreeRDP_Password (22)
#define FreeRDP_GatewayHostname (1986)
#define FreeRDP_GatewayUsername (1987)
#define FreeRDP_GatewayPassword (1988)

typedef struct rdp_settings rdpSettings;

EXTERN_C_START

const char* freerdp_settings_get_string(const rdpSettings* settings, size_t id);
BOOL freerdp_settings_set_string(rdpSettings* settings, size_t id, const char* param);

EXTERN_C_END

PCWSTR ValidateString(const BYTE* pb, ULONG cb)
{
	return pb &&                                       // present ?
	               cb &&                               // size not 0 ?
	               !(cb & (sizeof(WCHAR) - 1)) &&      // cb == n * sizeof(WCHAR) ?
	               !*(WCHAR*)(pb + cb - sizeof(WCHAR)) // 0 terminated ?
	           ? (PCWSTR)pb
	           : 0;
}

void AddDefaultSettings_I(rdpSettings* settings, size_t idHostname, size_t idUsername,
                          size_t idPassword)
{
	PCSTR ServerHostname = freerdp_settings_get_string(settings, idHostname);

	if (!ServerHostname)
	{
		return;
	}

	BOOL bExistUserName = 0 != freerdp_settings_get_string(settings, idUsername);
	BOOL bExistPassword = 0 != freerdp_settings_get_string(settings, idPassword);

	if (bExistUserName && bExistPassword)
	{
		return;
	}

	PCREDENTIALA Credential;
	PCWSTR lpWideCharStr;

	PSTR Password;
	PSTR UserName;

	PSTR TargetName = 0;
	int len = 0;

	while (0 < (len = _vsnprintf(TargetName, len, "TERMSRV/%s", (va_list)&ServerHostname)))
	{
		if (TargetName)
		{
			if (CredReadA(TargetName, CRED_TYPE_GENERIC, 0, &Credential))
			{
				if (!bExistPassword)
				{
					ULONG cch = Credential->CredentialBlobSize;

					if (lpWideCharStr = ValidateString(Credential->CredentialBlob, cch))
					{
						Password = 0, len = 0, cch /= sizeof(WCHAR);

						while (len = WideCharToMultiByte(CP_UTF8, 0, lpWideCharStr, cch, Password,
						                                 len, 0, 0))
						{
							if (Password)
							{
								freerdp_settings_set_string(settings, idPassword, Password);
								break;
							}

							Password = (PSTR)alloca(len);
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

			break;
		}

		TargetName = (PSTR)alloca(++len);
	}
}

EXTERN_C void WINAPI AddDefaultSettings(_Inout_ rdpSettings* settings)
{
	AddDefaultSettings_I(settings, FreeRDP_ServerHostname, FreeRDP_Username, FreeRDP_Password);
	AddDefaultSettings_I(settings, FreeRDP_GatewayHostname, FreeRDP_GatewayUsername,
	                     FreeRDP_GatewayPassword);
}