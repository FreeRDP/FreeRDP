
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/winpr.h>

static const TCHAR testExtDot[] = _T(".exe");
static const TCHAR testExtNoDot[] = _T("exe");
static const TCHAR testPathNoExtension[] = _T("C:\\Windows\\System32\\cmd");
static const TCHAR testPathExtension[] = _T("C:\\Windows\\System32\\cmd.exe");

int TestPathCchAddExtension(int argc, char* argv[])
{
	HRESULT status;
	TCHAR Path[PATHCCH_MAX_CCH];

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	/* Path: no extension, Extension: dot */

	_tcscpy(Path, testPathNoExtension);

	status = PathCchAddExtension(Path, PATHCCH_MAX_CCH, testExtDot);

	if (status != S_OK)
	{
		_tprintf(_T("PathCchAddExtension status: 0x%08") _T(PRIX32) _T("\n"), status);
		return -1;
	}

	if (_tcscmp(Path, testPathExtension) != 0)
	{
		_tprintf(_T("Path Mismatch: Actual: %s, Expected: %s\n"), Path, testPathExtension);
		return -1;
	}

	/* Path: no extension, Extension: no dot */

	_tcscpy(Path, testPathNoExtension);

	status = PathCchAddExtension(Path, PATHCCH_MAX_CCH, testExtNoDot);

	if (status != S_OK)
	{
		_tprintf(_T("PathCchAddExtension status: 0x%08") _T(PRIX32) _T("\n"), status);
		return -1;
	}

	if (_tcscmp(Path, testPathExtension) != 0)
	{
		_tprintf(_T("Path Mismatch: Actual: %s, Expected: %s\n"), Path, testPathExtension);
		return -1;
	}

	/* Path: extension, Extension: dot */

	_tcscpy(Path, testPathExtension);

	status = PathCchAddExtension(Path, PATHCCH_MAX_CCH, testExtDot);

	if (status != S_FALSE)
	{
		_tprintf(_T("PathCchAddExtension status: 0x%08") _T(PRIX32) _T("\n"), status);
		return -1;
	}

	if (_tcscmp(Path, testPathExtension) != 0)
	{
		_tprintf(_T("Path Mismatch: Actual: %s, Expected: %s\n"), Path, testPathExtension);
		return -1;
	}

	/* Path: extension, Extension: no dot */

	_tcscpy(Path, testPathExtension);

	status = PathCchAddExtension(Path, PATHCCH_MAX_CCH, testExtDot);

	if (status != S_FALSE)
	{
		_tprintf(_T("PathCchAddExtension status: 0x%08") _T(PRIX32) _T("\n"), status);
		return -1;
	}

	if (_tcscmp(Path, testPathExtension) != 0)
	{
		_tprintf(_T("Path Mismatch: Actual: %s, Expected: %s\n"), Path, testPathExtension);
		return -1;
	}

	/* Path: NULL */

	status = PathCchAddExtension(NULL, PATHCCH_MAX_CCH, testExtDot);
	if (status != E_INVALIDARG)
	{
		_tprintf(_T("PathCchAddExtension with null buffer returned status: 0x%08") _T(
		             PRIX32) _T(" (expected E_INVALIDARG)\n"),
		         status);
		return -1;
	}

	/* Extension: NULL */

	status = PathCchAddExtension(Path, PATHCCH_MAX_CCH, NULL);
	if (status != E_INVALIDARG)
	{
		_tprintf(_T("PathCchAddExtension with null extension returned status: 0x%08") _T(
		             PRIX32) _T(" (expected E_INVALIDARG)\n"),
		         status);
		return -1;
	}

	/* Insufficient Buffer size */

	_tcscpy(Path, _T("C:\\456789"));
	status = PathCchAddExtension(Path, 9 + 4, _T(".jpg"));
	if (SUCCEEDED(status))
	{
		_tprintf(_T("PathCchAddExtension with insufficient buffer unexpectedly succeeded with ")
		         _T("status: 0x%08") _T(PRIX32) _T("\n"),
		         status);
		return -1;
	}

	/* Minimum required buffer size */

	_tcscpy(Path, _T("C:\\456789"));
	status = PathCchAddExtension(Path, 9 + 4 + 1, _T(".jpg"));
	if (FAILED(status))
	{
		_tprintf(_T("PathCchAddExtension with sufficient buffer unexpectedly failed with status: ")
		         _T("0x%08") _T(PRIX32) _T("\n"),
		         status);
		return -1;
	}

	return 0;
}
