
#include <stdio.h>
#include <winpr/sspi.h>
#include <winpr/winpr.h>
#include <winpr/tchar.h>

int TestQuerySecurityPackageInfo(int argc, char* argv[])
{
	int rc;
	SECURITY_STATUS status;
	SecPkgInfo* pPackageInfo;
	SecurityFunctionTable* table;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	sspi_GlobalInit();
	table = InitSecurityInterfaceEx(0);

	status = table->QuerySecurityPackageInfo(NTLM_SSP_NAME, &pPackageInfo);

	if (status != SEC_E_OK)
		rc = -1;
	else
	{
		_tprintf(_T("\nQuerySecurityPackageInfo:\n"));
		_tprintf(_T("\"%s\", \"%s\"\n"), pPackageInfo->Name, pPackageInfo->Comment);
		rc = 0;
	}

	table->FreeContextBuffer(pPackageInfo);
	sspi_GlobalFinish();
	return rc;
}
