
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/windows.h>
#include <winpr/library.h>

int TestLibraryGetModuleFileName(int argc, char* argv[])
{
	char ModuleFileName[4096];
	DWORD len;

	/* Test insufficient buffer size behaviour */
	SetLastError(ERROR_SUCCESS);
	len = GetModuleFileNameA(NULL, ModuleFileName, 2);
	if (len != 2)
	{
		printf("%s: GetModuleFileNameA unexpectedly returned %u instead of 2\n",
			__FUNCTION__, len);
		return -1;
	}
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	{
		printf("%s: Invalid last error value: 0x%08X. Expected 0x%08X (ERROR_INSUFFICIENT_BUFFER)\n",
			__FUNCTION__, GetLastError(), ERROR_INSUFFICIENT_BUFFER);
		return -1;
	}

	/* Test with real/sufficient buffer size */
	SetLastError(ERROR_SUCCESS);
	len = GetModuleFileNameA(NULL, ModuleFileName, sizeof(ModuleFileName));
	if (len == 0)
	{
		printf("%s: GetModuleFileNameA failed with error 0x%08X\n",
			__FUNCTION__, GetLastError());
		return -1;
	}
	if (len == sizeof(ModuleFileName))
	{
		printf("%s: GetModuleFileNameA unexpectedly returned nSize\n",
			__FUNCTION__);
		return -1;
	}
	if (GetLastError() != ERROR_SUCCESS)
	{
		printf("%s: Invalid last error value: 0x%08X. Expected 0x%08X (ERROR_SUCCESS)\n",
			__FUNCTION__, GetLastError(), ERROR_SUCCESS);
		return -1;
	}

	printf("GetModuleFileNameA: %s\n", ModuleFileName);

	return 0;
}
