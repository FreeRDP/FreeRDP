
#include <stdio.h>
#include <winpr/sspi.h>
#include <winpr/winpr.h>
#include <winpr/tchar.h>

int TestQuerySecurityPackageInfo(int argc, char* argv[])
{
	SECURITY_STATUS status;
	SecPkgInfo* pPackageInfo;
	sspi_GlobalInit();
	status = QuerySecurityPackageInfo(NTLM_SSP_NAME, &pPackageInfo);

	if (status != SEC_E_OK)
	{
		sspi_GlobalFinish();
		return -1;
	}

	_tprintf(_T("\nQuerySecurityPackageInfo:\n"));
	_tprintf(_T("\"%s\", \"%s\"\n"), pPackageInfo->Name, pPackageInfo->Comment);
	sspi_GlobalFinish();
	return 0;
}

