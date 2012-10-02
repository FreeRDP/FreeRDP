
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/winpr.h>

static const TCHAR testBasePathBackslash[] = _T("C:\\Program Files\\");
static const TCHAR testBasePathNoBackslash[] = _T("C:\\Program Files");
static const TCHAR testMorePathBackslash[] = _T("\\Microsoft Visual Studio 11.0");
static const TCHAR testMorePathNoBackslash[] = _T("Microsoft Visual Studio 11.0");
static const TCHAR testPathOut[] = _T("C:\\Program Files\\Microsoft Visual Studio 11.0");

int TestPathCchAppend(int argc, char* argv[])
{
	HRESULT status;
	TCHAR Path[PATHCCH_MAX_CCH];

	/* Base Path: Backslash, More Path: No Backslash */

	_tcscpy(Path, testBasePathBackslash);

	status = PathCchAppend(Path, PATHCCH_MAX_CCH, testMorePathNoBackslash);

	if (status != S_OK)
	{
		_tprintf(_T("PathCchAppend status: 0x%08X\n"), status);
		return -1;
	}

	if (_tcscmp(Path, testPathOut) != 0)
	{
		_tprintf(_T("Path Mismatch: Actual: %s, Expected: %s\n"), Path, testPathOut);
		return -1;
	}

	/* Base Path: Backslash, More Path: Backslash */

	_tcscpy(Path, testBasePathBackslash);

	status = PathCchAppend(Path, PATHCCH_MAX_CCH, testMorePathBackslash);

	if (status != S_OK)
	{
		_tprintf(_T("PathCchAppend status: 0x%08X\n"), status);
		return -1;
	}

	if (_tcscmp(Path, testPathOut) != 0)
	{
		_tprintf(_T("Path Mismatch: Actual: %s, Expected: %s\n"), Path, testPathOut);
		return -1;
	}

	/* Base Path: No Backslash, More Path: Backslash */

	_tcscpy(Path, testBasePathNoBackslash);

	status = PathCchAppend(Path, PATHCCH_MAX_CCH, testMorePathBackslash);

	if (status != S_OK)
	{
		_tprintf(_T("PathCchAppend status: 0x%08X\n"), status);
		return -1;
	}

	if (_tcscmp(Path, testPathOut) != 0)
	{
		_tprintf(_T("Path Mismatch: Actual: %s, Expected: %s\n"), Path, testPathOut);
		return -1;
	}

	/* Base Path: No Backslash, More Path: No Backslash */

	_tcscpy(Path, testBasePathNoBackslash);

	status = PathCchAppend(Path, PATHCCH_MAX_CCH, testMorePathNoBackslash);

	if (status != S_OK)
	{
		_tprintf(_T("PathCchAppend status: 0x%08X\n"), status);
		return -1;
	}

	if (_tcscmp(Path, testPathOut) != 0)
	{
		_tprintf(_T("Path Mismatch: Actual: %s, Expected: %s\n"), Path, testPathOut);
		return -1;
	}

	return 0;
}

