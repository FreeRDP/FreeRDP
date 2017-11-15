
#include <stdio.h>
#include <winpr/sspi.h>
#include <winpr/winpr.h>
#include <winpr/tchar.h>

int TestQuerySecurityPackageInfo(int argc, char* argv[])
{
	int rc;
	SECURITY_STATUS status;
	SecPkgInfo* pPackageInfo;
	sspi_GlobalInit();
	status = QuerySecurityPackageInfo(NTLM_SSP_NAME, &pPackageInfo);

	if (status != SEC_E_OK)
		rc = -1;
	else
	{
		_tprintf(_T("\nQuerySecurityPackageInfo:\n"));
		_tprintf(_T("\"%s\", \"%s\"\n"), pPackageInfo->Name, pPackageInfo->Comment);
		rc = 0;
	}

	FreeContextBuffer(pPackageInfo);
	sspi_GlobalFinish();
	return rc;
}

