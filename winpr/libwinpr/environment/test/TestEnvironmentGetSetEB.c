
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/environment.h>

int TestEnvironmentGetSetEB(int argc, char* argv[])
{
	int rc = 0;
#ifndef _WIN32
	char test[1024];
	TCHAR* p = NULL;
	DWORD length;
	LPTCH lpszEnvironmentBlock = "SHELL=123\0test=1\0test1=2\0DISPLAY=WINPR_TEST_VALUE\0\0";
	LPTCH lpszEnvironmentBlockNew = NULL;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	rc = -1;
	/* Get length of an variable */
	length = GetEnvironmentVariableEBA(lpszEnvironmentBlock, "DISPLAY", NULL, 0);

	if (0 == length)
		return -1;

	/* Get the variable itself */
	p = (LPSTR)malloc(length);

	if (!p)
		goto fail;

	if (GetEnvironmentVariableEBA(lpszEnvironmentBlock, "DISPLAY", p, length) != length - 1)
		goto fail;

	printf("GetEnvironmentVariableA(WINPR_TEST_VARIABLE) = %s\n", p);

	if (strcmp(p, "WINPR_TEST_VALUE") != 0)
		goto fail;

	/* Get length of an non-existing variable */
	length = GetEnvironmentVariableEBA(lpszEnvironmentBlock, "BLA", NULL, 0);

	if (0 != length)
	{
		printf("Unset variable returned\n");
		goto fail;
	}

	/* Get length of an similar called variables */
	length = GetEnvironmentVariableEBA(lpszEnvironmentBlock, "XDISPLAY", NULL, 0);

	if (0 != length)
	{
		printf("Similar named variable returned (XDISPLAY, length %d)\n", length);
		goto fail;
	}

	length = GetEnvironmentVariableEBA(lpszEnvironmentBlock, "DISPLAYX", NULL, 0);

	if (0 != length)
	{
		printf("Similar named variable returned (DISPLAYX, length %d)\n", length);
		goto fail;
	}

	length = GetEnvironmentVariableEBA(lpszEnvironmentBlock, "DISPLA", NULL, 0);

	if (0 != length)
	{
		printf("Similar named variable returned (DISPLA, length %d)\n", length);
		goto fail;
	}

	length = GetEnvironmentVariableEBA(lpszEnvironmentBlock, "ISPLAY", NULL, 0);

	if (0 != length)
	{
		printf("Similar named variable returned (ISPLAY, length %d)\n", length);
		goto fail;
	}

	/* Set variable in empty environment block */
	if (SetEnvironmentVariableEBA(&lpszEnvironmentBlockNew, "test", "5"))
	{
		if (GetEnvironmentVariableEBA(lpszEnvironmentBlockNew, "test", test, 1023))
		{
			if (strcmp(test, "5") != 0)
				goto fail;
		}
		else
			goto fail;
	}

	/* Clear variable */
	if (SetEnvironmentVariableEBA(&lpszEnvironmentBlockNew, "test", NULL))
	{
		if (GetEnvironmentVariableEBA(lpszEnvironmentBlockNew, "test", test, 1023))
			goto fail;
		else
		{
			// not found .. this is expected
		}
	}

	free(lpszEnvironmentBlockNew);
	lpszEnvironmentBlockNew = (LPTCH)calloc(1024, sizeof(TCHAR));

	if (!lpszEnvironmentBlockNew)
		goto fail;

	memcpy(lpszEnvironmentBlockNew, lpszEnvironmentBlock, length);

	/* Set variable in empty environment block */
	if (SetEnvironmentVariableEBA(&lpszEnvironmentBlockNew, "test", "5"))
	{
		if (0 != GetEnvironmentVariableEBA(lpszEnvironmentBlockNew, "testr", test, 1023))
		{
			printf("GetEnvironmentVariableEBA returned unset variable\n");
			goto fail;
		}

		if (GetEnvironmentVariableEBA(lpszEnvironmentBlockNew, "test", test, 1023))
		{
			if (strcmp(test, "5") != 0)
				goto fail;
		}
		else
			goto fail;
	}

	rc = 0;
fail:
	free(p);
	free(lpszEnvironmentBlockNew);
#endif
	return rc;
}
