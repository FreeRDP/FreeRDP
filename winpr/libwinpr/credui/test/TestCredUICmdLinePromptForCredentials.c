
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/credui.h>
#include <winpr/windows.h>

static const TCHAR testTargetName[] = _T("TARGET");

int TestCredUICmdLinePromptForCredentials(int argc, char* argv[])
{
	BOOL fSave;
	DWORD status;
	DWORD dwFlags;
	TCHAR UserName[CREDUI_MAX_USERNAME_LENGTH] = { 0 };
	TCHAR Password[CREDUI_MAX_PASSWORD_LENGTH] = { 0 };

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	fSave = FALSE;
	dwFlags = CREDUI_FLAGS_DO_NOT_PERSIST | CREDUI_FLAGS_EXCLUDE_CERTIFICATES;

	status = CredUICmdLinePromptForCredentials(testTargetName, NULL, 0, UserName,
	                                           CREDUI_MAX_USERNAME_LENGTH, Password,
	                                           CREDUI_MAX_PASSWORD_LENGTH, &fSave, dwFlags);

	if (status != NO_ERROR)
	{
		printf("CredUIPromptForCredentials unexpected status: 0x%08" PRIX32 "\n", status);
		return -1;
	}

	_tprintf(_T("UserName: %s Password: %s\n"), UserName, Password);

	return 0;
}
