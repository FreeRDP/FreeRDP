
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/winpr.h>

static const char testPathExtension[] = "C:\\Windows\\System32\\cmd.exe";

int TestPathCchFindExtension(int argc, char* argv[])
{
	PCSTR pszExt;

	if (PathCchFindExtensionA(testPathExtension, sizeof(testPathExtension), &pszExt) != S_OK)
	{
		printf("PathCchFindExtensionA failure: expected S_OK\n");
		return -1;
	}

	printf("Extension: %s\n", pszExt);

	return 0;
}

