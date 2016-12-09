
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

	/* Get length of an variable */
	length = GetEnvironmentVariableEBA(lpszEnvironmentBlock,"DISPLAY", NULL, 0);
	if (0 == length)
		return -1;

	/* Get the variable itself */
	p = (LPSTR) malloc(length);
	if (!p)
		return -1;

	length = GetEnvironmentVariableEBA(lpszEnvironmentBlock,"DISPLAY", p, length);

	printf("GetEnvironmentVariableA(WINPR_TEST_VARIABLE) = %s\n" , p);

	if (strcmp(p, "WINPR_TEST_VALUE") != 0)
	{
		free(p);
		return -1;
	}

	free(p);

	/* Get length of an non-existing variable */
	length = GetEnvironmentVariableEBA(lpszEnvironmentBlock,"BLA", NULL, 0);
	if (0 != length)
	{
		printf("Unset variable returned\n");
		return -1;
	}

	/* Get length of an similar called variables */
	length = GetEnvironmentVariableEBA(lpszEnvironmentBlock,"XDISPLAY", NULL, 0);
	if (0 != length)
	{
		printf("Similar named variable returned (XDISPLAY, length %d)\n", length);
		return -1;
	}
	length = GetEnvironmentVariableEBA(lpszEnvironmentBlock,"DISPLAYX", NULL, 0);
	if (0 != length)
	{
		printf("Similar named variable returned (DISPLAYX, length %d)\n", length);
		return -1;
	}
	length = GetEnvironmentVariableEBA(lpszEnvironmentBlock,"DISPLA", NULL, 0);
	if (0 != length)
	{
		printf("Similar named variable returned (DISPLA, length %d)\n", length);
		return -1;
	}
	length = GetEnvironmentVariableEBA(lpszEnvironmentBlock,"ISPLAY", NULL, 0);
	if (0 != length)
	{
		printf("Similar named variable returned (ISPLAY, length %d)\n", length);
		return -1;
	}

	/* Set variable in empty environment block */
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
	/* Clear variable */
	if (SetEnvironmentVariableEBA(&lpszEnvironmentBlockNew, "test", NULL))
	{
		if (GetEnvironmentVariableEBA(lpszEnvironmentBlockNew,"test", test, 1023))
		{
			free(lpszEnvironmentBlockNew);
			return -1;
		}
		else
		{
			// not found .. this is expected
		}
	}
	free(lpszEnvironmentBlockNew);

	lpszEnvironmentBlockNew = (LPTCH) calloc(1024, sizeof(TCHAR));
	if (!lpszEnvironmentBlockNew)
		return -1;

	memcpy(lpszEnvironmentBlockNew,lpszEnvironmentBlock,length);

	/* Set variable in empty environment block */
	if (SetEnvironmentVariableEBA(&lpszEnvironmentBlockNew, "test", "5"))
	{
		if (0 != GetEnvironmentVariableEBA(lpszEnvironmentBlockNew,"testr", test, 1023))
		{
			printf("GetEnvironmentVariableEBA returned unset variable\n");
			free(lpszEnvironmentBlockNew);
			return -1;
		}
		if (GetEnvironmentVariableEBA(lpszEnvironmentBlockNew,"test", test, 1023))
		{
			if (strcmp(test,"5") != 0)
			{
				free(lpszEnvironmentBlockNew);
				return -1;
			}
		}
		else
		{
			free(lpszEnvironmentBlockNew);
			return -1;
		}
	}

	free(lpszEnvironmentBlockNew);
#endif

	return 0;
}

