
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/windows.h>
#include <winpr/library.h>

typedef int (*TEST_AB_FN)(int a, int b);

int TestLibraryGetProcAddress(int argc, char* argv[])
{
	int a, b, c;
	HINSTANCE library;
	TEST_AB_FN pFunctionA;
	TEST_AB_FN pFunctionB;
	LPCSTR SharedLibraryExtension;
	CHAR LibraryPath[PATHCCH_MAX_CCH];
	PCHAR p;

	if (!GetModuleFileNameA(NULL, LibraryPath, PATHCCH_MAX_CCH))
	{
		printf("%s: GetModuleFilenameA failed: 0x%08X\n", __FUNCTION__, GetLastError());
		return -1;
	}

	/* PathCchRemoveFileSpec is not implemented in WinPR */

	if (!(p = strrchr(LibraryPath, PathGetSeparatorA(PATH_STYLE_NATIVE))))
	{
		printf("%s: Error identifying module directory path\n", __FUNCTION__);
		return -1;
	}
	*p = 0;

	NativePathCchAppendA(LibraryPath, PATHCCH_MAX_CCH, "TestLibraryA");
	SharedLibraryExtension = PathGetSharedLibraryExtensionA(PATH_SHARED_LIB_EXT_WITH_DOT);
	NativePathCchAddExtensionA(LibraryPath, PATHCCH_MAX_CCH, SharedLibraryExtension);

	printf("%s: Loading Library: '%s'\n", __FUNCTION__, LibraryPath);

	if (!(library = LoadLibraryA(LibraryPath)))
	{
		printf("%s: LoadLibraryA failure: 0x%08X\n", __FUNCTION__, GetLastError());
		return -1;
	}

	if (!(pFunctionA = (TEST_AB_FN) GetProcAddress(library, "FunctionA")))
	{
		printf("%s: GetProcAddress failure (FunctionA)\n", __FUNCTION__);
		return -1;
	}

	if (!(pFunctionB = (TEST_AB_FN) GetProcAddress(library, "FunctionB")))
	{
		printf("%s: GetProcAddress failure (FunctionB)\n", __FUNCTION__);
		return -1;
	}

	a = 2;
	b = 3;

	c = pFunctionA(a, b); /* LibraryA / FunctionA multiplies a and b */

	if (c != (a * b))
	{
		printf("%s: pFunctionA call failed\n", __FUNCTION__);
		return -1;
	}

	a = 10;
	b = 5;

	c = pFunctionB(a, b); /* LibraryA / FunctionB divides a by b */

	if (c != (a / b))
	{
		printf("%s: pFunctionB call failed\n", __FUNCTION__);
		return -1;
	}

	if (!FreeLibrary(library))
	{
		printf("%s: FreeLibrary failure: 0x%08X\n", __FUNCTION__, GetLastError());
		return -1;
	}

	return 0;
}
