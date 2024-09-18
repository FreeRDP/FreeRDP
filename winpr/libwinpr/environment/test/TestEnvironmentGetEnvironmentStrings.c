
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/environment.h>

int TestEnvironmentGetEnvironmentStrings(int argc, char* argv[])
{
	int r = -1;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	LPTCH lpszEnvironmentBlock = GetEnvironmentStrings();
	if (!lpszEnvironmentBlock)
		goto fail;

	TCHAR* p = lpszEnvironmentBlock;
	while (p[0] && p[1])
	{
		const size_t max = _tcslen(p);
		const int rc = _sntprintf(NULL, 0, _T("%s\n"), p);
		if (rc < 1)
		{
			_tprintf(_T("test failed: return %d\n"), rc);
			goto fail;
		}
		if (max != (size_t)(rc - 1))
		{
			_tprintf(_T("test failed: length %") _T(PRIuz) _T(" != %d [%s]\n"), max, rc - 1, p);
			goto fail;
		}
		p += (max + 1);
	}

	r = 0;
fail:
	FreeEnvironmentStrings(lpszEnvironmentBlock);

	return r;
}
