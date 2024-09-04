
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/windows.h>
#include <winpr/library.h>

typedef int (*TEST_AB_FN)(int a, int b);

int TestLibraryGetProcAddress(int argc, char* argv[])
{
	int a = 0;
	int b = 0;
	int c = 0;
	HINSTANCE library = NULL;
	TEST_AB_FN pFunctionA = NULL;
	TEST_AB_FN pFunctionB = NULL;
	LPCSTR SharedLibraryExtension = NULL;
	CHAR LibraryPath[PATHCCH_MAX_CCH];
	PCHAR p = NULL;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	if (!GetModuleFileNameA(NULL, LibraryPath, PATHCCH_MAX_CCH))
	{
		printf("%s: GetModuleFilenameA failed: 0x%08" PRIX32 "\n", __func__, GetLastError());
		return -1;
	}

	/* PathCchRemoveFileSpec is not implemented in WinPR */

	if (!(p = strrchr(LibraryPath, PathGetSeparatorA(PATH_STYLE_NATIVE))))
	{
		printf("%s: Error identifying module directory path\n", __func__);
		return -1;
	}

	*p = 0;
	NativePathCchAppendA(LibraryPath, PATHCCH_MAX_CCH, "TestLibraryA");
	SharedLibraryExtension = PathGetSharedLibraryExtensionA(PATH_SHARED_LIB_EXT_WITH_DOT);
	NativePathCchAddExtensionA(LibraryPath, PATHCCH_MAX_CCH, SharedLibraryExtension);
	printf("%s: Loading Library: '%s'\n", __func__, LibraryPath);

	if (!(library = LoadLibraryA(LibraryPath)))
	{
		printf("%s: LoadLibraryA failure: 0x%08" PRIX32 "\n", __func__, GetLastError());
		return -1;
	}

	if (!(pFunctionA = GetProcAddressAs(library, "FunctionA", TEST_AB_FN)))
	{
		printf("%s: GetProcAddress failure (FunctionA)\n", __func__);
		return -1;
	}

	if (!(pFunctionB = GetProcAddressAs(library, "FunctionB", TEST_AB_FN)))
	{
		printf("%s: GetProcAddress failure (FunctionB)\n", __func__);
		return -1;
	}

	a = 2;
	b = 3;
	c = pFunctionA(a, b); /* LibraryA / FunctionA multiplies a and b */

	if (c != (a * b))
	{
		printf("%s: pFunctionA call failed\n", __func__);
		return -1;
	}

	a = 10;
	b = 5;
	c = pFunctionB(a, b); /* LibraryA / FunctionB divides a by b */

	if (c != (a / b))
	{
		printf("%s: pFunctionB call failed\n", __func__);
		return -1;
	}

	if (!FreeLibrary(library))
	{
		printf("%s: FreeLibrary failure: 0x%08" PRIX32 "\n", __func__, GetLastError());
		return -1;
	}

	return 0;
}
