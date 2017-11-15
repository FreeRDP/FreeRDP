
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/environment.h>

int TestEnvironmentMergeEnvironmentStrings(int argc, char* argv[])
{
#ifndef _WIN32
	TCHAR* p;
	int length;
	LPTCH lpszEnvironmentBlock;
	LPTCH lpsz2Merge = "SHELL=123\0test=1\0test1=2\0DISPLAY=:77\0\0";
	LPTCH lpszMergedEnvironmentBlock;
	lpszEnvironmentBlock = GetEnvironmentStrings();
	lpszMergedEnvironmentBlock = MergeEnvironmentStrings(lpszEnvironmentBlock, lpsz2Merge);
	p = (TCHAR*) lpszMergedEnvironmentBlock;

	while (p[0] && p[1])
	{
		printf("%s\n", p);
		length = strlen(p);
		p += (length + 1);
	}

	FreeEnvironmentStrings(lpszMergedEnvironmentBlock);
	FreeEnvironmentStrings(lpszEnvironmentBlock);
#endif
	return 0;
}

