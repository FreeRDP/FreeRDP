
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/windows.h>
#include <winpr/library.h>

typedef int (*TEST_AB_FN)(int a, int b);

int TestLibraryGetProcAddress(int argc, char* argv[])
{
	char* str;
	int length;
	int a, b, c;
	LPTSTR BasePath;
	HINSTANCE library;
	TEST_AB_FN pFunctionA;
	TEST_AB_FN pFunctionB;
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

	pFunctionA = (TEST_AB_FN) GetProcAddress(library, "FunctionA");

	if (!pFunctionA)
	{
		_tprintf(_T("GetProcAddress failure (FunctionA)\n"));
		return -1;
	}

	pFunctionB = (TEST_AB_FN) GetProcAddress(library, "FunctionB");

	if (!pFunctionB)
	{
		_tprintf(_T("GetProcAddress failure (FunctionB)\n"));
		return -1;
	}

	a = 2;
	b = 3;

	c = pFunctionA(a, b); /* LibraryA / FunctionA multiplies a and b */

	if (c != (a * b))
	{
		_tprintf(_T("pFunctionA call failed\n"));
		return -1;
	}

	a = 10;
	b = 5;

	c = pFunctionB(a, b); /* LibraryA / FunctionB divides a by b */

	if (c != (a / b))
	{
		_tprintf(_T("pFunctionB call failed\n"));
		return -1;
	}

	if (!FreeLibrary(library))
	{
		_tprintf(_T("FreeLibrary failure\n"));
		return -1;
	}

	return 0;
}
