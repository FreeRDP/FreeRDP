
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/winpr.h>
#include <winpr/tchar.h>

int TestEnumerateSecurityPackages(int argc, char* argv[])
{
	ULONG cPackages = 0;
	SECURITY_STATUS status = 0;
	SecPkgInfo* pPackageInfo = NULL;
	SecurityFunctionTable* table = NULL;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	sspi_GlobalInit();
	table = InitSecurityInterfaceEx(0);

	status = table->EnumerateSecurityPackages(&cPackages, &pPackageInfo);

	if (status != SEC_E_OK)
	{
		sspi_GlobalFinish();
		return -1;
	}

	_tprintf(_T("\nEnumerateSecurityPackages (%") _T(PRIu32) _T("):\n"), cPackages);

	for (size_t index = 0; index < cPackages; index++)
	{
		_tprintf(_T("\"%s\", \"%s\"\n"), pPackageInfo[index].Name, pPackageInfo[index].Comment);
	}

	table->FreeContextBuffer(pPackageInfo);
	sspi_GlobalFinish();

	return 0;
}
