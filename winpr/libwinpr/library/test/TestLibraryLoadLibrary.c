
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/windows.h>
#include <winpr/library.h>

int TestLibraryLoadLibrary(int argc, char* argv[])
{
	HINSTANCE library;
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

	if (!FreeLibrary(library))
	{
		printf("%s: FreeLibrary failure: 0x%08X\n", __FUNCTION__, GetLastError());
		return -1;
	}

	return 0;
}
