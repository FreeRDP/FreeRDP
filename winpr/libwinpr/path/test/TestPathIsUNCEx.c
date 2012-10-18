
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
	BOOL status;
	LPTSTR Server;
	TCHAR Path[PATHCCH_MAX_CCH];

	/* Path is UNC */

	_tcscpy(Path, testPathUNC);

	status = PathIsUNCEx(Path, (LPCTSTR*) &Server);

	if (!status)
	{
		_tprintf(_T("PathIsUNCEx status: 0x%08X\n"), status);
		return -1;
	}

	if (_tcscmp(Server, testServer) != 0)
	{
		_tprintf(_T("Server Name Mismatch: Actual: %s, Expected: %s\n"), Server, testServer);
		return -1;
	}

	/* Path is not UNC */

	_tcscpy(Path, testPathNotUNC);

	status = PathIsUNCEx(Path, (LPCTSTR*) &Server);

	if (status)
	{
		_tprintf(_T("PathIsUNCEx status: 0x%08X\n"), status);
		return -1;
	}

	return 0;
}

