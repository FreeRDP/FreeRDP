
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/windows.h>

int TestLibraryGetProcAddress(int argc, char* argv[])
{
	char* str;
	int length;
	LPTSTR BasePath;
	HINSTANCE library;
	LPTSTR LibraryPath;

	str = argv[1];

#ifdef UNICODE
	length = MultiByteToWideChar(CP_UTF8, 0, str, strlen(str), NULL, 0);
	BasePath = (WCHAR*) malloc((length + 1) * sizeof(WCHAR));
	MultiByteToWideChar(CP_UTF8, 0, str, length, (LPWSTR) BasePath, length * sizeof(WCHAR));
	BasePath[length] = 0;
#else
	BasePath = _strdup(path);
#endif
	
	
	_tprintf(_T("Base Path: %s\n"), BasePath);

	PathAllocCombine(BasePath, _T("TestLibraryA\\TestLibraryA.dll"), 0, &LibraryPath);

	//_tprintf(_T("Loading Library: %s\n"), LibraryPath);

	/*
	library = LoadLibrary(LibraryPath);

	if (!library)
	{
		printf("LoadLibrary failure\n");
		return -1;
	}

	if (!FreeLibrary(library))
	{
		printf("FreeLibrary failure\n");
		return -1;
	}*/

	return 0;
}
