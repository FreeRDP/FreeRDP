
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/winpr.h>

/**
 * Naming Files, Paths, and Namespaces:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa365247/
 */

static const TCHAR testPathPrefixFileNamespace[] = _T("\\\\?\\C:\\Program Files\\");
static const TCHAR testPathNoPrefixFileNamespace[] = _T("C:\\Program Files\\");
static const TCHAR testPathPrefixFileNamespaceMinimum[] = _T("\\\\?\\C:");
static const TCHAR testPathNoPrefixFileNamespaceMinimum[] = _T("C:");

static const TCHAR testPathPrefixDeviceNamespace[] = _T("\\\\?\\GLOBALROOT");

int TestPathCchStripPrefix(int argc, char* argv[])
{
	HRESULT status;
	TCHAR Path[PATHCCH_MAX_CCH];
	int i;

	/**
	 * PathCchStripPrefix returns S_OK if the prefix was removed, S_FALSE if
	 * the path did not have a prefix to remove, or an HRESULT failure code.
	 */

	/* Path with prefix (File Namespace) */

	_tcscpy(Path, testPathPrefixFileNamespace);

	status = PathCchStripPrefix(Path, sizeof(testPathPrefixFileNamespace) / sizeof(TCHAR));

	if (status != S_OK)
	{
		_tprintf(_T("PathCchStripPrefix status: 0x%08") _T(PRIX32) _T("\n"), status);
		return -1;
	}

	if (_tcscmp(Path, testPathNoPrefixFileNamespace) != 0)
	{
		_tprintf(_T("Path Mismatch: Actual: %s, Expected: %s\n"), Path,
		         testPathNoPrefixFileNamespace);
		return -1;
	}

	/* Path with prefix (Device Namespace) */

	_tcscpy(Path, testPathPrefixDeviceNamespace);

	status = PathCchStripPrefix(Path, sizeof(testPathPrefixDeviceNamespace) / sizeof(TCHAR));

	if (status != S_FALSE)
	{
		_tprintf(_T("PathCchStripPrefix status: 0x%08") _T(PRIX32) _T("\n"), status);
		return -1;
	}

	if (_tcscmp(Path, testPathPrefixDeviceNamespace) != 0)
	{
		_tprintf(_T("Path Mismatch: Actual: %s, Expected: %s\n"), Path,
		         testPathPrefixDeviceNamespace);
		return -1;
	}

	/* NULL Path */
	status = PathCchStripPrefix(NULL, PATHCCH_MAX_CCH);
	if (status != E_INVALIDARG)
	{
		_tprintf(
		    _T("PathCchStripPrefix with null path unexpectedly succeeded with status 0x%08") _T(
		        PRIX32) _T("\n"),
		    status);
		return -1;
	}

	/* Invalid cchPath values: 0, 1, 2, 3 and > PATHCCH_MAX_CCH */
	for (i = 0; i < 5; i++)
	{
		_tcscpy(Path, testPathPrefixFileNamespace);
		if (i == 4)
			i = PATHCCH_MAX_CCH + 1;
		status = PathCchStripPrefix(Path, i);
		if (status != E_INVALIDARG)
		{
			_tprintf(_T("PathCchStripPrefix with invalid cchPath value %d unexpectedly succeeded ")
			         _T("with status 0x%08") _T(PRIX32) _T("\n"),
			         i, status);
			return -1;
		}
	}

	/* Minimum Path that would get successfully stripped on windows */
	_tcscpy(Path, testPathPrefixFileNamespaceMinimum);
	i = sizeof(testPathPrefixFileNamespaceMinimum) / sizeof(TCHAR);
	i = i - 1; /* include testing of a non-null terminated string */
	status = PathCchStripPrefix(Path, i);
	if (status != S_OK)
	{
		_tprintf(_T("PathCchStripPrefix with minimum valid strippable path length unexpectedly ")
		         _T("returned status 0x%08") _T(PRIX32) _T("\n"),
		         status);
		return -1;
	}
	if (_tcscmp(Path, testPathNoPrefixFileNamespaceMinimum))
	{
		_tprintf(_T("Path Mismatch: Actual: %s, Expected: %s\n"), Path,
		         testPathNoPrefixFileNamespaceMinimum);
		return -1;
	}

	/* Invalid drive letter symbol */
	_tcscpy(Path, _T("\\\\?\\5:"));
	status = PathCchStripPrefix(Path, 6);
	if (status == S_OK)
	{
		_tprintf(
		    _T("PathCchStripPrefix with invalid drive letter symbol unexpectedly succeeded\n"));
		return -1;
	}

	return 0;
}
