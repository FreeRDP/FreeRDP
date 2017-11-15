
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/winpr.h>
#include <winpr/tchar.h>
#include <winpr/dsparse.h>

static LPCTSTR testServiceClass = _T("HTTP");
static LPCTSTR testServiceName = _T("LAB1-W2K8R2-GW.lab1.awake.local");
static LPCTSTR testSpn = _T("HTTP/LAB1-W2K8R2-GW.lab1.awake.local");

int TestDsMakeSpn(int argc, char* argv[])
{
	int rc = -1;
	LPTSTR Spn = NULL;
	DWORD status;
	DWORD SpnLength;
	SpnLength = -1;
	status = DsMakeSpn(testServiceClass, testServiceName, NULL, 0, NULL, &SpnLength, NULL);

	if (status != ERROR_INVALID_PARAMETER)
	{
		_tprintf(_T("DsMakeSpn: expected ERROR_INVALID_PARAMETER\n"));
		goto fail;
	}

	SpnLength = 0;
	status = DsMakeSpn(testServiceClass, testServiceName, NULL, 0, NULL, &SpnLength, NULL);

	if (status != ERROR_BUFFER_OVERFLOW)
	{
		_tprintf(_T("DsMakeSpn: expected ERROR_BUFFER_OVERFLOW\n"));
		goto fail;
	}

	if (SpnLength != 37)
	{
		_tprintf(_T("DsMakeSpn: SpnLength mismatch: Actual: %")_T(PRIu32)_T(", Expected: 37\n"), SpnLength);
		goto fail;
	}

	/* SpnLength includes null terminator */
	Spn = (LPTSTR) calloc(SpnLength, sizeof(TCHAR));

	if (!Spn)
	{
		_tprintf(_T("DsMakeSpn: Unable to allocate memroy\n"));
		goto fail;
	}

	status = DsMakeSpn(testServiceClass, testServiceName, NULL, 0, NULL, &SpnLength, Spn);

	if (status != ERROR_SUCCESS)
	{
		_tprintf(_T("DsMakeSpn: expected ERROR_SUCCESS\n"));
		goto fail;
	}

	if (_tcscmp(Spn, testSpn) != 0)
	{
		_tprintf(_T("DsMakeSpn: SPN mismatch: Actual: %s, Expected: %s\n"), Spn, testSpn);
		goto fail;
	}

	_tprintf(_T("DsMakeSpn: %s\n"), Spn);
	rc = 0;
fail:
	free(Spn);
	return rc;
}

