
#include <stdio.h>
#include <winpr/sspi.h>
#include <winpr/winpr.h>

int TestQuerySecurityPackageInfo(int argc, char* argv[])
{
	SECURITY_STATUS status;
	SecPkgInfo* pPackageInfo;

	sspi_GlobalInit();

	status = QuerySecurityPackageInfo(NTLMSP_NAME, &pPackageInfo);

	if (status != SEC_E_OK)
	{
		sspi_GlobalFinish();
		return -1;
	}

	printf("\nQuerySecurityPackageInfo:\n");
	printf("\"%s\", \"%s\"\n", pPackageInfo->Name, pPackageInfo->Comment);

	sspi_GlobalFinish();

	return 0;
}

