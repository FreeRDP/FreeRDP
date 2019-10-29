
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/environment.h>

int TestEnvironmentGetEnvironmentStrings(int argc, char* argv[])
{
	int r = -1;
	TCHAR* p;
	size_t length;
	LPTCH lpszEnvironmentBlock;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	lpszEnvironmentBlock = GetEnvironmentStrings();

	p = (TCHAR*) lpszEnvironmentBlock;

	while (p[0] && p[1])
	{
		const int rc = _tprintf(_T("%s\n"), p);
		if (rc < 1)
			goto fail;
		length = _tcslen(p);
		if (length != (size_t)(rc - 1))
			goto fail;
		p += (length + 1);
	}

	r = 0;
fail:
	FreeEnvironmentStrings(lpszEnvironmentBlock);

	return r;
}

