
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/tchar.h>
#include <winpr/windows.h>

int TestLibraryFreeLibrary(int argc, char* argv[])
{
	HINSTANCE library;

	library = LoadLibrary(_T("kernel32.dll"));

	if (!library)
	{
		printf("LoadLibrary failure\n");
		return -1;
	}

	if (!FreeLibrary(library))
	{
		printf("FreeLibrary failure\n");
		return -1;
	}

	return 0;
}
