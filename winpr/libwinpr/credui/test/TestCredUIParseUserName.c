
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/credui.h>
#include <winpr/windows.h>

static const TCHAR testUserName1[] = _T("LAB1\\JohnDoe");
static const TCHAR testUserName2[] = _T("JohnDoe@LAB1");

int TestCredUIParseUserName(int argc, char* argv[])
{
	DWORD status;
	TCHAR User[CREDUI_MAX_USERNAME_LENGTH + 1];
	TCHAR Domain[CREDUI_MAX_DOMAIN_TARGET_LENGTH + 1];

	/* Test LAB1\JohnDoe */

	ZeroMemory(User, sizeof(User));
	ZeroMemory(Domain, sizeof(Domain));

	status = CredUIParseUserName(testUserName1, User, sizeof(User) / sizeof(TCHAR),
		Domain, sizeof(Domain) / sizeof(TCHAR));

	printf("CredUIParseUserName status: 0x%08X\n", status);

	_tprintf(_T("UserName: %s -> Domain: %s User: %s\n"), testUserName1, Domain, User);

	/* Test JohnDoe@LAB1 */

	ZeroMemory(User, sizeof(User));
	ZeroMemory(Domain, sizeof(Domain));

	status = CredUIParseUserName(testUserName2, User, sizeof(User) / sizeof(TCHAR),
		Domain, sizeof(Domain) / sizeof(TCHAR));

	printf("CredUIParseUserName status: 0x%08X\n", status);

	_tprintf(_T("UserName: %s -> Domain: %s User: %s\n"), testUserName2, Domain, User);

	return 0;
}

