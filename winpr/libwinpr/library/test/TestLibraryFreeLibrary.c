
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/windows.h>
#include <winpr/library.h>

int TestLibraryFreeLibrary(int argc, char* argv[])
{
	char* str;
	int length;
	LPTSTR BasePath;
	HINSTANCE library;
	LPCTSTR SharedLibraryExtension;
	TCHAR LibraryPath[PATHCCH_MAX_CCH];

	str = argv[1];

#ifdef UNICODE
	length = MultiByteToWideChar(CP_UTF8, 0, str, strlen(str), NULL, 0);
	BasePath = (WCHAR*) malloc((length + 1) * sizeof(WCHAR));
	MultiByteToWideChar(CP_UTF8, 0, str, length, (LPWSTR) BasePath, length * sizeof(WCHAR));
	BasePath[length] = 0;
#else
	BasePath = _strdup(str);
	length = strlen(BasePath);
#endif

	CopyMemory(LibraryPath, BasePath, length * sizeof(TCHAR));
	LibraryPath[length] = 0;

	NativePathCchAppend(LibraryPath, PATHCCH_MAX_CCH, _T("TestLibraryA")); /* subdirectory */
	NativePathCchAppend(LibraryPath, PATHCCH_MAX_CCH, _T("TestLibraryA")); /* file name without extension */

	SharedLibraryExtension = PathGetSharedLibraryExtension(PATH_SHARED_LIB_EXT_WITH_DOT);
	NativePathCchAddExtension(LibraryPath, PATHCCH_MAX_CCH, SharedLibraryExtension); /* add shared library extension */

	_tprintf(_T("Loading Library: %s\n"), LibraryPath);

	library = LoadLibrary(LibraryPath);

	if (!library)
	{
		_tprintf(_T("LoadLibrary failure\n"));
		return -1;
	}

	if (!FreeLibrary(library))
	{
		_tprintf(_T("FreeLibrary failure\n"));
		return -1;
	}

	return 0;
}
