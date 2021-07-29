
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/environment.h>
#include <winpr/error.h>

#define TEST_NAME "WINPR_TEST_VARIABLE"
#define TEST_VALUE "WINPR_TEST_VALUE"
int TestEnvironmentSetEnvironmentVariable(int argc, char* argv[])
{
	int rc = -1;
	DWORD nSize;
	LPSTR lpBuffer = NULL;
	DWORD error = 0;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	SetEnvironmentVariableA(TEST_NAME, TEST_VALUE);
	nSize = GetEnvironmentVariableA(TEST_NAME, NULL, 0);

	/* check if value returned is len + 1 ) */
	if (nSize != strnlen(TEST_VALUE, sizeof(TEST_VALUE)) + 1)
	{
		printf("GetEnvironmentVariableA not found error\n");
		return -1;
	}

	lpBuffer = (LPSTR)malloc(nSize);

	if (!lpBuffer)
		return -1;

	nSize = GetEnvironmentVariableA(TEST_NAME, lpBuffer, nSize);

	if (nSize != strnlen(TEST_VALUE, sizeof(TEST_VALUE)))
	{
		printf("GetEnvironmentVariableA wrong size returned\n");
		goto fail;
	}

	if (strcmp(lpBuffer, TEST_VALUE) != 0)
	{
		printf("GetEnvironmentVariableA returned value doesn't match\n");
		goto fail;
	}

	nSize = GetEnvironmentVariableA("__xx__notset_", lpBuffer, nSize);
	error = GetLastError();

	if (0 != nSize || ERROR_ENVVAR_NOT_FOUND != error)
	{
		printf("GetEnvironmentVariableA not found error\n");
		goto fail;
	}

	/* clear variable */
	SetEnvironmentVariableA(TEST_NAME, NULL);
	nSize = GetEnvironmentVariableA(TEST_VALUE, NULL, 0);

	if (0 != nSize)
	{
		printf("SetEnvironmentVariableA failed to clear variable\n");
		goto fail;
	}

	rc = 0;
fail:
	free(lpBuffer);
	return rc;
}
