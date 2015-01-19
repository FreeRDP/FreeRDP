
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
static const TCHAR testPathOutMorePathBackslash[] = _T("C:\\Microsoft Visual Studio 11.0");

int TestPathAllocCombine(int argc, char* argv[])
{
	HRESULT status;
	LPTSTR PathOut;

	/* Base Path: Backslash, More Path: No Backslash */

	status = PathAllocCombine(testBasePathBackslash, testMorePathNoBackslash, 0, &PathOut);

	if (status != S_OK)
	{
		_tprintf(_T("PathAllocCombine status: 0x%08X\n"), status);
		return -1;
	}

	if (_tcscmp(PathOut, testPathOut) != 0)
	{
		_tprintf(_T("Path Mismatch 1: Actual: %s, Expected: %s\n"), PathOut, testPathOut);
		return -1;
	}

	HeapFree(GetProcessHeap(), 0, PathOut);

	/* Base Path: Backslash, More Path: Backslash */

	status = PathAllocCombine(testBasePathBackslash, testMorePathBackslash, 0, &PathOut);

	if (status != S_OK)
	{
		_tprintf(_T("PathAllocCombine status: 0x%08X\n"), status);
		return -1;
	}

	if (_tcscmp(PathOut, testPathOutMorePathBackslash) != 0)
	{
		_tprintf(_T("Path Mismatch 2: Actual: %s, Expected: %s\n"), PathOut, testPathOutMorePathBackslash);
		return -1;
	}

	HeapFree(GetProcessHeap(), 0, PathOut);

	/* Base Path: No Backslash, More Path: Backslash */

	status = PathAllocCombine(testBasePathNoBackslash, testMorePathBackslash, 0, &PathOut);

	if (status != S_OK)
	{
		_tprintf(_T("PathAllocCombine status: 0x%08X\n"), status);
		return -1;
	}

	if (_tcscmp(PathOut, testPathOutMorePathBackslash) != 0)
	{
		_tprintf(_T("Path Mismatch 3: Actual: %s, Expected: %s\n"), PathOut, testPathOutMorePathBackslash);
		return -1;
	}

	HeapFree(GetProcessHeap(), 0, PathOut);

	/* Base Path: No Backslash, More Path: No Backslash */

	status = PathAllocCombine(testBasePathNoBackslash, testMorePathNoBackslash, 0, &PathOut);

	if (status != S_OK)
	{
		_tprintf(_T("PathAllocCombine status: 0x%08X\n"), status);
		return -1;
	}

	if (_tcscmp(PathOut, testPathOut) != 0)
	{
		_tprintf(_T("Path Mismatch 4: Actual: %s, Expected: %s\n"), PathOut, testPathOut);
		return -1;
	}

	HeapFree(GetProcessHeap(), 0, PathOut);

	return 0;
}

