
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/winpr.h>

static const char testPathExtension[] = "C:\\Windows\\System32\\cmd.exe";

int TestPathCchFindExtension(int argc, char* argv[])
{
	PCSTR pszExt;
	PCSTR pszTmp;
	HRESULT hr;

	/* Test invalid args */

	hr = PathCchFindExtensionA(NULL, sizeof(testPathExtension), &pszExt);
	if (SUCCEEDED(hr))
	{
		printf("PathCchFindExtensionA unexpectedly succeeded with pszPath = NULL. result: 0x%08X\n", (ULONG)hr);
		return -1;
	}

	hr = PathCchFindExtensionA(testPathExtension, 0, &pszExt);
	if (SUCCEEDED(hr))
	{
		printf("PathCchFindExtensionA unexpectedly succeeded with cchPath = 0. result: 0x%08X\n", (ULONG)hr);
		return -1;
	}

	hr = PathCchFindExtensionA(testPathExtension, sizeof(testPathExtension), NULL);
	if (SUCCEEDED(hr))
	{
		printf("PathCchFindExtensionA unexpectedly succeeded with ppszExt = NULL. result: 0x%08X\n", (ULONG)hr);
		return -1;
	}


	/* Test missing null-termination of pszPath */

	hr = PathCchFindExtensionA("c:\\45.789", 9, &pszExt); /* nb: correct would be 10 */
	if (SUCCEEDED(hr))
	{
		printf("PathCchFindExtensionA unexpectedly succeeded with unterminated pszPath. result: 0x%08X\n", (ULONG)hr);
		return -1;
	}


	/* Test passing of an empty terminated string (must succeed) */

	pszExt = NULL;
	pszTmp = "";
	hr = PathCchFindExtensionA(pszTmp, 1, &pszExt);
	if (hr != S_OK)
	{
		printf("PathCchFindExtensionA failed with an empty terminated string. result: 0x%08X\n", (ULONG)hr);
		return -1;
	}
	/* pszExt must point to the strings terminating 0 now */
	if (pszExt != pszTmp)
	{
		printf("PathCchFindExtensionA failed with an empty terminated string:  pszExt pointer mismatch\n");
		return -1;
	}


	/* Test a path without file extension (must succeed) */

	pszExt = NULL;
	pszTmp = "c:\\4.678\\";
	hr = PathCchFindExtensionA(pszTmp, 10, &pszExt);
	if (hr != S_OK)
	{
		printf("PathCchFindExtensionA failed with a directory path. result: 0x%08X\n", (ULONG)hr);
		return -1;
	}
	/* The extension must not have been found and pszExt must point to the
	 * strings terminating NULL now */
	if (pszExt != &pszTmp[9])
	{
		printf("PathCchFindExtensionA failed with a directory path: pszExt pointer mismatch\n");
		return -1;
	}


	/* Non-special tests */

	pszExt = NULL;
	if (PathCchFindExtensionA(testPathExtension, sizeof(testPathExtension), &pszExt) != S_OK)
	{
		printf("PathCchFindExtensionA failure: expected S_OK\n");
		return -1;
	}

	if (!pszExt || strcmp(pszExt, ".exe"))
	{
		printf("PathCchFindExtensionA failure: unexpected extension\n");
		return -1;
	}

	printf("Extension: %s\n", pszExt);

	return 0;
}

