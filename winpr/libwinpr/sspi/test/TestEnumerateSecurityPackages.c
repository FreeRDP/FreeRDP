
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/winpr.h>
#include <winpr/tchar.h>

int TestEnumerateSecurityPackages(int argc, char* argv[])
{
	int index;
	ULONG cPackages;
	SECURITY_STATUS status;
	SecPkgInfo* pPackageInfo;

	sspi_GlobalInit();

	status = EnumerateSecurityPackages(&cPackages, &pPackageInfo);

	if (status != SEC_E_OK)
	{
		sspi_GlobalFinish();
		return -1;	
	}

	_tprintf(_T("\nEnumerateSecurityPackages (%d):\n"), (unsigned int)cPackages);

	for (index = 0; index < (int) cPackages; index++)
	{
		_tprintf(_T("\"%s\", \"%s\"\n"), pPackageInfo[index].Name, pPackageInfo[index].Comment);
	}

	FreeContextBuffer(pPackageInfo);
	sspi_GlobalFinish();

	return 0;
}

