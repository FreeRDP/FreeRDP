
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/environment.h>
#include <winpr/error.h>

#define TEST_NAME "WINPR_TEST_VARIABLE"
#define TEST_VALUE "WINPR_TEST_VALUE"
int TestEnvironmentSetEnvironmentVariable(int argc, char* argv[])
{
	DWORD nSize;
	LPSTR lpBuffer;
	DWORD error = 0;

	SetEnvironmentVariableA(TEST_NAME, TEST_VALUE);

	nSize = GetEnvironmentVariableA(TEST_NAME, NULL, 0);
	/* check if value returned is len + 1 ) */
	if (nSize != strlen(TEST_VALUE) + 1)
	{
		printf("GetEnvironmentVariableA not found error\n");
		return -1;
	}

	lpBuffer = (LPSTR) malloc(nSize);
	nSize = GetEnvironmentVariableA(TEST_NAME, lpBuffer, nSize);

	if (nSize != strlen(TEST_VALUE))
	{
		printf("GetEnvironmentVariableA wrong size returned\n");
		return -1;
	}

	if (strcmp(lpBuffer, TEST_VALUE) != 0)
	{
		printf("GetEnvironmentVariableA returned value doesn't match\n");
		return -1;
	}

	nSize = GetEnvironmentVariableA("__xx__notset_",lpBuffer, nSize);
	error = GetLastError();
	if (0 != nSize || ERROR_ENVVAR_NOT_FOUND != error)
	{
		printf("GetEnvironmentVariableA not found error\n");
		return -1;
	}

	free(lpBuffer);

	/* clear variable */
	SetEnvironmentVariableA(TEST_NAME, NULL);
	nSize = GetEnvironmentVariableA(TEST_VALUE, NULL, 0);
	if ( 0 != nSize)
	{
		printf("SetEnvironmentVariableA failed to clear variable\n");
		return -1;
	}
	return 0;
}

