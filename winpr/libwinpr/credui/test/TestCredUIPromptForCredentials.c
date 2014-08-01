
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/credui.h>
#include <winpr/windows.h>

static const TCHAR testTargetName[] = _T("TARGET");

static CREDUI_INFO testUiInfo =
{
	sizeof(CREDUI_INFO),
	NULL,
	_T("Message Text"),
	_T("Caption Text"),
	NULL
};

int TestCredUIPromptForCredentials(int argc, char* argv[])
{
	BOOL fSave;
	DWORD status;
	DWORD dwFlags;
	TCHAR UserName[CREDUI_MAX_USERNAME_LENGTH];
	TCHAR Password[CREDUI_MAX_PASSWORD_LENGTH];
	
	fSave = FALSE;
	ZeroMemory(UserName, sizeof(UserName));
	ZeroMemory(Password, sizeof(Password));
	dwFlags = CREDUI_FLAGS_DO_NOT_PERSIST | CREDUI_FLAGS_EXCLUDE_CERTIFICATES;

	status = CredUIPromptForCredentials(&testUiInfo, testTargetName, NULL, 0,
		UserName, CREDUI_MAX_USERNAME_LENGTH,
		Password, CREDUI_MAX_PASSWORD_LENGTH, &fSave, dwFlags);

	if (status != NO_ERROR)
	{
		printf("CredUIPromptForCredentials unexpected status: 0x%08X\n", status);
		return -1;
	}

	_tprintf(_T("UserName: %s Password: %s\n"), UserName, Password);

	return 0;
}

