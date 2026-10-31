#include <stdio.h>
#include <winpr/sspi.h>
#include <winpr/winpr.h>
#include <winpr/tchar.h>
#include <winpr/credssp.h>
#include <winpr/schannel.h>

/* The TSSSP package must register in the WinPR SSPI table on every platform so
 * that core/tsssp.c resolves it the same way everywhere. On Windows the package
 * forwards to the native secur32 provider, so acquiring an outbound credential
 * handle is exercised too (best effort: a host without the provider is not a
 * test failure). */

static int test_query_package(void)
{
	SecPkgInfoW* info = nullptr;
	SecurityFunctionTableW* table = InitSecurityInterfaceExW(0);
	if (!table || !table->QuerySecurityPackageInfoW)
	{
		(void)fprintf(stderr, "no WinPR security function table\n");
		return -1;
	}

	WCHAR name[] = { 'T', 'S', 'S', 'S', 'P', '\0' };
	const SECURITY_STATUS status = table->QuerySecurityPackageInfoW(name, &info);
	if (status != SEC_E_OK)
	{
		(void)fprintf(stderr, "QuerySecurityPackageInfoW(TSSSP) failed: 0x%08" PRIx32 "\n",
		              (UINT32)status);
		return -1;
	}

	int rc = 0;
	if (!info || !info->Name || (_wcscmp(info->Name, name) != 0))
	{
		(void)fprintf(stderr, "QuerySecurityPackageInfoW(TSSSP) returned an unexpected name\n");
		rc = -1;
	}

	if (table->FreeContextBuffer)
		(void)table->FreeContextBuffer(info);
	return rc;
}

#if defined(_WIN32) && !defined(_UWP)
static void test_acquire_credentials(void)
{
	SecurityFunctionTableW* table = InitSecurityInterfaceExW(0);
	if (!table || !table->AcquireCredentialsHandleW || !table->FreeCredentialsHandle)
		return;

	SCH_CREDENTIALS sc = { 0 };
	sc.dwVersion = SCH_CREDENTIALS_VERSION;

	CREDSSP_CRED cred = { 0 };
	cred.Type = CredsspSubmitBufferBothOld;
	cred.pSchannelCred = &sc;
	cred.pSpnegoCred = nullptr;

	CREDSSP_CRED_EX credEx = { 0 };
	credEx.Type = (CREDSPP_SUBMIT_TYPE)CredsspCredEx;
	credEx.Version = CREDSSP_CRED_EX_VERSION;
	credEx.Flags = CREDSSP_FLAG_REDIRECT;
	credEx.Cred = cred;

	WCHAR pkg[] = { 'T', 'S', 'S', 'S', 'P', '\0' };
	CredHandle handle = { 0 };
	TimeStamp expiry = { 0 };
	const SECURITY_STATUS status = table->AcquireCredentialsHandleW(
	    nullptr, pkg, SECPKG_CRED_OUTBOUND, nullptr, &credEx, nullptr, nullptr, &handle, &expiry);

	(void)fprintf(stderr, "AcquireCredentialsHandleW(TSSSP) returned 0x%08" PRIx32 "\n",
	              (UINT32)status);
	if (status == SEC_E_OK)
		(void)table->FreeCredentialsHandle(&handle);
}
#endif

int TestSspiTSSSP(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	sspi_GlobalInit();

	int rc = test_query_package();

#if defined(_WIN32) && !defined(_UWP)
	if (rc == 0)
		test_acquire_credentials();
#endif

	sspi_GlobalFinish();
	return rc;
}
