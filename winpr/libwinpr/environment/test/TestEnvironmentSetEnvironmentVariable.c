
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/environment.h>

int TestEnvironmentSetEnvironmentVariable(int argc, char* argv[])
{
	DWORD nSize;
	LPSTR lpBuffer;

	SetEnvironmentVariableA("WINPR_TEST_VARIABLE", "WINPR_TEST_VALUE");

	nSize = GetEnvironmentVariableA("WINPR_TEST_VARIABLE", NULL, 0);

	lpBuffer = (LPSTR) malloc(nSize);
	nSize = GetEnvironmentVariableA("WINPR_TEST_VARIABLE", lpBuffer, nSize);

	printf("GetEnvironmentVariableA(WINPR_TEST_VARIABLE) = %s\n" , lpBuffer);

	if (strcmp(lpBuffer, "WINPR_TEST_VALUE") != 0)
	{
		return -1;
	}

	free(lpBuffer);

	return 0;
}

