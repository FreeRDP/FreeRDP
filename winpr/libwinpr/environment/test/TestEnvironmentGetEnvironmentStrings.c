
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/environment.h>

int TestEnvironmentGetEnvironmentStrings(int argc, char* argv[])
{
	TCHAR* p;
	int length;
	LPTCH lpszEnvironmentBlock;

	lpszEnvironmentBlock = GetEnvironmentStrings();

	p = (TCHAR*) lpszEnvironmentBlock;

	while (p[0] && p[1])
	{
		_tprintf(_T("%s\n"), p);
		length = _tcslen(p);
		p += (length + 1);
	}

	FreeEnvironmentStrings(lpszEnvironmentBlock);

	return 0;
}

