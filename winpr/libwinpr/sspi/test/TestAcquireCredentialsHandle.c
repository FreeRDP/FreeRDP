
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/winpr.h>

static const char* test_User = "User";
static const char* test_Domain = "Domain";
static const char* test_Password = "Password";

int TestAcquireCredentialsHandle(int argc, char* argv[])
{
	int rc = -1;
	SECURITY_STATUS status;
	CredHandle credentials = { 0 };
	TimeStamp expiration;
	SEC_WINNT_AUTH_IDENTITY identity;
	SecurityFunctionTable* table;
	SecPkgCredentials_Names credential_names;
	sspi_GlobalInit();
	table = InitSecurityInterface();
	identity.User = (UINT16*) _strdup(test_User);
	identity.Domain = (UINT16*) _strdup(test_Domain);
	identity.Password = (UINT16*) _strdup(test_Password);

	if (!identity.User || !identity.Domain || !identity.Password)
		goto fail;

	identity.UserLength = strlen(test_User);
	identity.DomainLength = strlen(test_Domain);
	identity.PasswordLength = strlen(test_Password);
	identity.Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;
	status = table->AcquireCredentialsHandle(NULL, NTLM_SSP_NAME,
	         SECPKG_CRED_OUTBOUND, NULL, &identity, NULL, NULL, &credentials, &expiration);

	if (status != SEC_E_OK)
		goto fail;

	status = table->QueryCredentialsAttributes(&credentials, SECPKG_CRED_ATTR_NAMES, &credential_names);

	if (status != SEC_E_OK)
		goto fail;

	rc = 0;
fail:

	if (SecIsValidHandle(&credentials))
		table->FreeCredentialsHandle(&credentials);

	free(identity.User);
	free(identity.Domain);
	free(identity.Password);
	sspi_GlobalFinish();
	return rc;
}

