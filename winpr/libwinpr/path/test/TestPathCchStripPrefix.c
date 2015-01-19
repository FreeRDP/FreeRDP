
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

static const TCHAR testPathPrefixDeviceNamespace[] = _T("\\\\?\\GLOBALROOT");

int TestPathCchStripPrefix(int argc, char* argv[])
{
	HRESULT status;
	TCHAR Path[PATHCCH_MAX_CCH];

	/* Path with prefix (File Namespace) */

	_tcscpy(Path, testPathPrefixFileNamespace);

	status = PathCchStripPrefix(Path, sizeof(testPathPrefixFileNamespace) / sizeof(TCHAR));

	if (status != S_OK)
	{
		_tprintf(_T("PathCchStripPrefix status: 0x%08X\n"), status);
		return -1;
	}

	if (_tcscmp(Path, testPathNoPrefixFileNamespace) != 0)
	{
		_tprintf(_T("Path Mismatch: Actual: %s, Expected: %s\n"), Path, testPathNoPrefixFileNamespace);
		return -1;
	}

	/* Path with prefix (Device Namespace) */

	_tcscpy(Path, testPathPrefixDeviceNamespace);

	status = PathCchStripPrefix(Path, sizeof(testPathPrefixDeviceNamespace) / sizeof(TCHAR));

	if (status != S_FALSE)
	{
		_tprintf(_T("PathCchStripPrefix status: 0x%08X\n"), status);
		return -1;
	}

	if (_tcscmp(Path, testPathPrefixDeviceNamespace) != 0)
	{
		_tprintf(_T("Path Mismatch: Actual: %s, Expected: %s\n"), Path, testPathPrefixDeviceNamespace);
		return -1;
	}

	return 0;
}

