
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/winpr.h>
#include <winpr/tchar.h>
#include <winpr/dsparse.h>

// LPCTSTR testName = _T("LAB1\\JohnDoe");

int TestDsCrackNames(int argc, char* argv[])
{
#if 0
	HANDLE ds;
	DWORD status;
	PDS_NAME_RESULT pResult;

	status = DsBind(NULL, NULL, &ds);

	if (status != ERROR_SUCCESS)
	{
		_tprintf(_T("DsBind: expected ERROR_SUCCESS: 0x%08")_T(PRIX32)_T("\n"), status);
		return -1;
	}

	status = DsCrackNames(ds, DS_NAME_FLAG_SYNTACTICAL_ONLY, DS_NT4_ACCOUNT_NAME,
		DS_USER_PRINCIPAL_NAME, 1, &testName, &pResult);

	if (status != ERROR_SUCCESS)
	{
		_tprintf(_T("DsCrackNames: expected ERROR_SUCCESS\n"));
		return -1;
	}

	_tprintf(_T("DsCrackNames: pResult->cItems: %")_T(PRIu32)_T("\n"), pResult->cItems);

	_tprintf(_T("DsCrackNames: pResult->rItems[0]: Domain: %s Name: %s Status: 0x%08")_T(PRIX32)_T("\n"),
		pResult->rItems[0].pDomain, pResult->rItems[0].pName, pResult->rItems[0].status);

	status = DsUnBind(&ds);

	if (status != ERROR_SUCCESS)
	{
		_tprintf(_T("DsUnBind: expected ERROR_SUCCESS\n"));
		return -1;
	}
#endif

	return 0;
}
