
#include <string.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/sspi.h>
#include <winpr/string.h>

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

	SEC_WINNT_AUTH_IDENTITY identity = WINPR_C_ARRAY_INIT;
	identity.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

	identity.User = ConvertUtf8ToWCharAlloc(testUser, &identity.UserLength);
	if (!identity.User)
		return FALSE;

	identity.Domain = ConvertUtf8ToWCharAlloc(testDomain, &identity.DomainLength);
	if (!identity.Domain)
	{
		free(identity.User);
		return FALSE;
	}

	identity.Password = ConvertUtf8ToWCharAlloc(testPassword, &identity.PasswordLength);
	if (!identity.Password)
	{
		free(identity.User);
		free(identity.Domain);
		return FALSE;
	}

	sspi_FreeAuthIdentity(&identity);

	if (identity.User != nullptr)
	{
		printf("FAIL: identity.User not nullptr after FreeAuthIdentity\n");
		return FALSE;
	}
	if (identity.Domain != nullptr)
	{
		printf("FAIL: identity.Domain not nullptr after FreeAuthIdentity\n");
		return FALSE;
	}
	if (identity.Password != nullptr)
	{
		printf("FAIL: identity.Password not nullptr after FreeAuthIdentity\n");
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
	SecBuffer buffer = WINPR_C_ARRAY_INIT;

	buffer.cbBuffer = (ULONG)strlen(testData);
	buffer.pvBuffer = strdup(testData);
	if (!buffer.pvBuffer)
		return FALSE;

	sspi_SecBufferFree(&buffer);

	if (buffer.pvBuffer != nullptr)
	{
		printf("FAIL: pvBuffer not nullptr after SecBufferFree\n");
		return FALSE;
	}
	if (buffer.cbBuffer != 0)
	{
		printf("FAIL: cbBuffer not zero after SecBufferFree\n");
		return FALSE;
	}

	return TRUE;
}

/* Test nullptr and empty input handling */
static BOOL test_null_handling(void)
{
	/* nullptr should not crash */
	sspi_FreeAuthIdentity(nullptr);
	sspi_SecBufferFree(nullptr);

	/* Empty identity should not crash */
	SEC_WINNT_AUTH_IDENTITY empty = WINPR_C_ARRAY_INIT;
	sspi_FreeAuthIdentity(&empty);

	/* Empty buffer should not crash */
	SecBuffer emptyBuf = WINPR_C_ARRAY_INIT;
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
