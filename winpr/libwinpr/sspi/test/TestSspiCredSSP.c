#include <stdio.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/credssp.h>

/* Smoke-test the WinPR CredSSP package without a KDC: it registers, acquires an outbound
 * credential (which wires up the inner Negotiate credential from the CREDSSP_CRED_EX), and
 * frees it. The full exchange (Negotiate tokens, pubKeyAuth, delegated credentials) uses
 * Kerberos and is validated end to end through the lsa4posix daemon against the live DC. */

int TestSspiCredSSP(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	SecurityFunctionTableW* table = InitSecurityInterfaceExW(SSPI_INTERFACE_WINPR);
	if (!table || !table->QuerySecurityPackageInfoW || !table->AcquireCredentialsHandleW ||
	    !table->FreeCredentialsHandle)
	{
		(void)fprintf(stderr, "no WinPR security function table\n");
		return -1;
	}

	WCHAR pkg[] = { 'C', 'R', 'E', 'D', 'S', 'S', 'P', '\0' };

	/* the package must resolve by name */
	PSecPkgInfoW info = nullptr;
	if (table->QuerySecurityPackageInfoW(pkg, &info) != SEC_E_OK)
	{
		(void)fprintf(stderr, "QuerySecurityPackageInfoW(CREDSSP) failed\n");
		return -1;
	}
	if (table->FreeContextBuffer)
		(void)table->FreeContextBuffer(info);

	WCHAR user[] = { 't', 'e', 's', 't', 'u', 's', 'e', 'r', '\0' };
	WCHAR domain[] = { 'T', 'E', 'S', 'T', '\0' };
	WCHAR pass[] = { 'S', 'e', 'c', 'r', 'e', 't', '1', '!', '\0' };
	SEC_WINNT_AUTH_IDENTITY_W identity = { 0 };
	identity.User = (UINT16*)user;
	identity.UserLength = 8;
	identity.Domain = (UINT16*)domain;
	identity.DomainLength = 4;
	identity.Password = (UINT16*)pass;
	identity.PasswordLength = 8;
	identity.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

	CREDSSP_CRED cred = { .Type = CredsspSubmitBufferBothOld };
	cred.pSpnegoCred = &identity;
	CREDSSP_CRED_EX credEx = { .Type = (CREDSPP_SUBMIT_TYPE)CredsspCredEx };
	credEx.Version = CREDSSP_CRED_EX_VERSION;
	credEx.Cred = cred;

	CredHandle creds = { 0 };
	TimeStamp expiry = { 0 };
	const SECURITY_STATUS status = table->AcquireCredentialsHandleW(
	    nullptr, pkg, SECPKG_CRED_OUTBOUND, nullptr, &credEx, nullptr, nullptr, &creds, &expiry);
	if (status != SEC_E_OK)
	{
		(void)fprintf(stderr, "AcquireCredentialsHandle(CREDSSP) failed: 0x%08" PRIx32 "\n",
		              (UINT32)status);
		return -1;
	}

	const SECURITY_STATUS freed = table->FreeCredentialsHandle(&creds);
	if (freed != SEC_E_OK)
	{
		(void)fprintf(stderr, "FreeCredentialsHandle(CREDSSP) failed: 0x%08" PRIx32 "\n",
		              (UINT32)freed);
		return -1;
	}

	return 0;
}
