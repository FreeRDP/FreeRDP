
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/winpr.h>

static const TCHAR testServer[] = _T("server\\share\\path\\file");
static const TCHAR testPathUNC[] = _T("\\\\server\\share\\path\\file");
static const TCHAR testPathNotUNC[] = _T("C:\\share\\path\\file");

int TestPathIsUNCEx(int argc, char* argv[])
{
	BOOL status = 0;
	LPCTSTR Server = NULL;
	TCHAR Path[PATHCCH_MAX_CCH] = { 0 };

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	/* Path is UNC */

	_tcsncpy(Path, testPathUNC, ARRAYSIZE(Path));

	status = PathIsUNCEx(Path, &Server);

	if (!status)
	{
		_tprintf(_T("PathIsUNCEx status: 0x%d\n"), status);
		return -1;
	}

	if (_tcsncmp(Server, testServer, ARRAYSIZE(testServer)) != 0)
	{
		_tprintf(_T("Server Name Mismatch: Actual: %s, Expected: %s\n"), Server, testServer);
		return -1;
	}

	/* Path is not UNC */

	_tcsncpy(Path, testPathNotUNC, ARRAYSIZE(Path));

	status = PathIsUNCEx(Path, &Server);

	if (status)
	{
		_tprintf(_T("PathIsUNCEx status: 0x%08") _T(PRIX32) _T("\n"), status);
		return -1;
	}

	return 0;
}
