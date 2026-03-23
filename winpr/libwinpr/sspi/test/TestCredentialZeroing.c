
#include <string.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/sspi.h>

/**
 * Verify that public credential cleanup functions zero sensitive fields
 * before freeing memory, preventing credential recovery from freed heap.
 */

/* Test that sspi_FreeAuthIdentity zeroes and NULLs all fields */
static BOOL test_FreeAuthIdentity_zeroes_fields(void)
{
	const char* testPassword = "S3cretP@ssw0rd!";
	const char* testUser = "testuser";
	const char* testDomain = "TESTDOMAIN";

	SEC_WINNT_AUTH_IDENTITY identity = { 0 };
	identity.Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;

	identity.UserLength = (UINT32)strlen(testUser);
	identity.User = (UINT16*)strdup(testUser);
	if (!identity.User)
		return FALSE;

	identity.DomainLength = (UINT32)strlen(testDomain);
	identity.Domain = (UINT16*)strdup(testDomain);
	if (!identity.Domain)
	{
		free(identity.User);
		return FALSE;
	}

	identity.PasswordLength = (UINT32)strlen(testPassword);
	identity.Password = (UINT16*)strdup(testPassword);
	if (!identity.Password)
	{
		free(identity.User);
		free(identity.Domain);
		return FALSE;
	}

	sspi_FreeAuthIdentity(&identity);

	if (identity.User != NULL)
	{
		printf("FAIL: identity.User not NULL after FreeAuthIdentity\n");
		return FALSE;
	}
	if (identity.Domain != NULL)
	{
		printf("FAIL: identity.Domain not NULL after FreeAuthIdentity\n");
		return FALSE;
	}
	if (identity.Password != NULL)
	{
		printf("FAIL: identity.Password not NULL after FreeAuthIdentity\n");
		return FALSE;
	}
	if (identity.UserLength != 0 || identity.DomainLength != 0 || identity.PasswordLength != 0)
	{
		printf("FAIL: identity lengths not zeroed after FreeAuthIdentity\n");
		return FALSE;
	}

	return TRUE;
}

/* Test that sspi_SecBufferFree zeroes and NULLs the buffer */
static BOOL test_SecBufferFree_zeroes_buffer(void)
{
	const char* testData = "sensitive-credential-data";
	SecBuffer buffer = { 0 };

	buffer.cbBuffer = (ULONG)strlen(testData);
	buffer.pvBuffer = strdup(testData);
	if (!buffer.pvBuffer)
		return FALSE;

	sspi_SecBufferFree(&buffer);

	if (buffer.pvBuffer != NULL)
	{
		printf("FAIL: pvBuffer not NULL after SecBufferFree\n");
		return FALSE;
	}
	if (buffer.cbBuffer != 0)
	{
		printf("FAIL: cbBuffer not zero after SecBufferFree\n");
		return FALSE;
	}

	return TRUE;
}

/* Test NULL and empty input handling */
static BOOL test_null_handling(void)
{
	/* NULL should not crash */
	sspi_FreeAuthIdentity(NULL);
	sspi_SecBufferFree(NULL);

	/* Empty identity should not crash */
	SEC_WINNT_AUTH_IDENTITY empty = { 0 };
	sspi_FreeAuthIdentity(&empty);

	/* Empty buffer should not crash */
	SecBuffer emptyBuf = { 0 };
	sspi_SecBufferFree(&emptyBuf);

	return TRUE;
}

int TestCredentialZeroing(int argc, char* argv[])
{
	int rc = 0;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!test_FreeAuthIdentity_zeroes_fields())
	{
		printf("FAIL: test_FreeAuthIdentity_zeroes_fields\n");
		rc = -1;
	}

	if (!test_SecBufferFree_zeroes_buffer())
	{
		printf("FAIL: test_SecBufferFree_zeroes_buffer\n");
		rc = -1;
	}

	if (!test_null_handling())
	{
		printf("FAIL: test_null_handling\n");
		rc = -1;
	}

	return rc;
}
