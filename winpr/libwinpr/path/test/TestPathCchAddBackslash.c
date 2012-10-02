
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/winpr.h>

static const TCHAR testPathBackslash[] = _T("C:\\Program Files\\");
static const TCHAR testPathNoBackslash[] = _T("C:\\Program Files");

int TestPathCchAddBackslash(int argc, char* argv[])
{
	HRESULT status;
	TCHAR Path[PATHCCH_MAX_CCH];

	_tcscpy(Path, testPathNoBackslash);

	/* Add a backslash to a path without a trailing backslash, expect S_OK */

	status = PathCchAddBackslash(Path, PATHCCH_MAX_CCH);

	if (status != S_OK)
	{
		_tprintf(_T("PathCchAddBackslash status: 0x%08X\n"), (int) status);
		return -1;
	}

	if (_tcscmp(Path, testPathBackslash) != 0)
	{
		_tprintf(_T("Path Mismatch: Actual: %s, Expected: %s\n"), Path, testPathBackslash);
		return -1;
	}

	/* Add a backslash to a path with a trailing backslash, expect S_FALSE */

	_tcscpy(Path, testPathBackslash);

	status = PathCchAddBackslash(Path, PATHCCH_MAX_CCH);

	if (status != S_FALSE)
	{
		_tprintf(_T("PathCchAddBackslash status: 0x%08X\n"), (int) status);
		return -1;
	}

	if (_tcscmp(Path, testPathBackslash) != 0)
	{
		_tprintf(_T("Path Mismatch: Actual: %s, Expected: %s\n"), Path, testPathBackslash);
		return -1;
	}

	return 0;
}

