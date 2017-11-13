
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/winpr.h>

static const char* test_User = "User";
static const char* test_Domain = "Domain";
static const char* test_Password = "Password";

int TestAcquireCredentialsHandle(int argc, char* argv[])
{
	SECURITY_STATUS status;
	CredHandle credentials;
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
	{
		free(identity.User);
		free(identity.Domain);
		free(identity.Password);
		fprintf(stderr, "Memory allocation failed\n");
		return -1;
	}

	identity.UserLength = strlen(test_User);
	identity.DomainLength = strlen(test_Domain);
	identity.PasswordLength = strlen(test_Password);
	identity.Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;
	status = table->AcquireCredentialsHandle(NULL, NTLM_SSP_NAME,
	         SECPKG_CRED_OUTBOUND, NULL, &identity, NULL, NULL, &credentials, &expiration);

	if (status != SEC_E_OK)
	{
		sspi_GlobalFinish();
		return -1;
	}

	status = table->QueryCredentialsAttributes(&credentials, SECPKG_CRED_ATTR_NAMES, &credential_names);

	if (status != SEC_E_OK)
	{
		sspi_GlobalFinish();
		return -1;
	}

	sspi_GlobalFinish();
	return 0;
}

