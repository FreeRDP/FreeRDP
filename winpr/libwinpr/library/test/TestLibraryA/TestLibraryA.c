
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/tchar.h>
#include <winpr/windows.h>

WINPR_API int FunctionA(int a, int b);
WINPR_API int FunctionB(int a, int b);

int FunctionA(int a, int b)
{
	return (a * b); /* multiply */
}

int FunctionB(int a, int b)
{
	return (a / b); /* divide */
}
