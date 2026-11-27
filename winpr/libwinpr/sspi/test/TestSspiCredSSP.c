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

	WCHAR pkg[] = { winpr_wchar_ToLE('C'), winpr_wchar_ToLE('R'), winpr_wchar_ToLE('E'),
		            winpr_wchar_ToLE('D'), winpr_wchar_ToLE('S'), winpr_wchar_ToLE('S'),
		            winpr_wchar_ToLE('P'), winpr_wchar_ToLE('\0') };

	/* the package must resolve by name */
	PSecPkgInfoW info = nullptr;
	if (table->QuerySecurityPackageInfoW(pkg, &info) != SEC_E_OK)
	{
		(void)fprintf(stderr, "QuerySecurityPackageInfoW(CREDSSP) failed\n");
		return -1;
	}
	if (table->FreeContextBuffer)
		(void)table->FreeContextBuffer(info);

	WCHAR user[] = { winpr_wchar_ToLE('t'), winpr_wchar_ToLE('e'), winpr_wchar_ToLE('s'),
		             winpr_wchar_ToLE('t'), winpr_wchar_ToLE('u'), winpr_wchar_ToLE('s'),
		             winpr_wchar_ToLE('e'), winpr_wchar_ToLE('r'), winpr_wchar_ToLE('\0') };
	WCHAR domain[] = { winpr_wchar_ToLE('T'), winpr_wchar_ToLE('E'), winpr_wchar_ToLE('S'),
		               winpr_wchar_ToLE('T'), winpr_wchar_ToLE('\0') };
	WCHAR pass[] = { winpr_wchar_ToLE('S'), winpr_wchar_ToLE('e'), winpr_wchar_ToLE('c'),
		             winpr_wchar_ToLE('r'), winpr_wchar_ToLE('e'), winpr_wchar_ToLE('t'),
		             winpr_wchar_ToLE('1'), winpr_wchar_ToLE('!'), winpr_wchar_ToLE('\0') };
	SEC_WINNT_AUTH_IDENTITY_W identity = WINPR_C_ARRAY_INIT;
	identity.User = (UINT16*)user;
	identity.UserLength = _wcsnlen(user, ARRAYSIZE(user));
	identity.Domain = (UINT16*)domain;
	identity.DomainLength = _wcsnlen(domain, ARRAYSIZE(domain));
	identity.Password = (UINT16*)pass;
	identity.PasswordLength = _wcsnlen(pass, ARRAYSIZE(pass));
	identity.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

	CREDSSP_CRED cred = { .Type = CredsspSubmitBufferBothOld };
	cred.pSpnegoCred = &identity;
	CREDSSP_CRED_EX credEx = { .Type = (CREDSPP_SUBMIT_TYPE)CredsspCredEx };
	credEx.Version = CREDSSP_CRED_EX_VERSION;
	credEx.Cred = cred;

	CredHandle creds = WINPR_C_ARRAY_INIT;
	TimeStamp expiry = WINPR_C_ARRAY_INIT;
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
