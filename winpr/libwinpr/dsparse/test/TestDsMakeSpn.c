
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/winpr.h>
#include <winpr/tchar.h>
#include <winpr/dsparse.h>

LPCTSTR testServiceClass = _T("HTTP");
LPCTSTR testServiceName = _T("LAB1-W2K8R2-GW.lab1.awake.local");
LPCTSTR testSpn = _T("HTTP/LAB1-W2K8R2-GW.lab1.awake.local");

int TestDsMakeSpn(int argc, char* argv[])
{
	LPTSTR Spn;
	DWORD status;
	DWORD SpnLength;

	SpnLength = -1;
	status = DsMakeSpn(testServiceClass, testServiceName, NULL, 0, NULL, &SpnLength, NULL);

	if (status != ERROR_INVALID_PARAMETER)
	{
		_tprintf(_T("DsMakeSpn: expected ERROR_INVALID_PARAMETER\n"));
		return -1;
	}

	SpnLength = 0;
	status = DsMakeSpn(testServiceClass, testServiceName, NULL, 0, NULL, &SpnLength, NULL);

	if (status != ERROR_BUFFER_OVERFLOW)
	{
		_tprintf(_T("DsMakeSpn: expected ERROR_BUFFER_OVERFLOW\n"));
		return -1;
	}

	if (SpnLength != 37)
	{
		_tprintf(_T("DsMakeSpn: SpnLength mismatch: Actual: %d, Expected: %d\n"), SpnLength, 37);
		return -1;
	}

	/* SpnLength includes null terminator */
	Spn = (LPTSTR) malloc(SpnLength * sizeof(TCHAR));

	status = DsMakeSpn(testServiceClass, testServiceName, NULL, 0, NULL, &SpnLength, Spn);

	if (status != ERROR_SUCCESS)
	{
		_tprintf(_T("DsMakeSpn: expected ERROR_SUCCESS\n"));
		return -1;
	}

	if (_tcscmp(Spn, testSpn) != 0)
	{
		_tprintf(_T("DsMakeSpn: SPN mismatch: Actual: %s, Expected: %s\n"), Spn, testSpn);
		return -1;
	}

	_tprintf(_T("DsMakeSpn: %s\n"), Spn);

	return 0;
}

