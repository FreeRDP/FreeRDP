
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/environment.h>

int TestEnvironmentGetSetEB(int argc, char* argv[])
{
#ifndef _WIN32
	char test[1024];
	TCHAR* p;
	int length;
	LPTCH lpszEnvironmentBlock = "SHELL=123\0test=1\0test1=2\0DISPLAY=WINPR_TEST_VALUE\0\0";
	LPTCH lpszEnvironmentBlockNew = NULL;

	length = GetEnvironmentVariableEBA(lpszEnvironmentBlock,"DISPLAY", NULL, 0);

	p = (LPSTR) malloc(length);
	length = GetEnvironmentVariableEBA(lpszEnvironmentBlock,"DISPLAY", p, length);

	printf("GetEnvironmentVariableA(WINPR_TEST_VARIABLE) = %s\n" , p);

	if (strcmp(p, "WINPR_TEST_VALUE") != 0)
	{
		return -1;
	}

	free(p);

	lpszEnvironmentBlockNew = (LPTCH) malloc(1024);
	memcpy(lpszEnvironmentBlockNew,lpszEnvironmentBlock,56);

	if (SetEnvironmentVariableEBA(&lpszEnvironmentBlockNew, "test", "5"))
	{
		if (GetEnvironmentVariableEBA(lpszEnvironmentBlockNew,"test", test, 1023))
		{
			if (strcmp(test,"5") != 0)
			{
				return -1;
			}
		}
		else
		{
			return -1;
		}
	}

	//free(lpszEnvironmentBlockNew);

	if (SetEnvironmentVariableEBA(&lpszEnvironmentBlockNew, "test", NULL))
	{
		if (GetEnvironmentVariableEBA(lpszEnvironmentBlockNew,"test", test, 1023))
		{
			return -1;
		}
		else
		{
			// not found .. this is expected
		}
	}

	free(lpszEnvironmentBlockNew);
#endif

	return 0;
}

