
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/windows.h>
#include <winpr/library.h>
#include <winpr/nt.h>

int TestLibraryLoadLibrary(int argc, char* argv[])
{
	HINSTANCE library = NULL;
	LPCSTR SharedLibraryExtension = NULL;
	CHAR LibraryPath[PATHCCH_MAX_CCH] = { 0 };
	PCHAR p = NULL;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	if (!GetModuleFileNameA(NULL, LibraryPath, PATHCCH_MAX_CCH))
	{
		const UINT32 err = GetLastError();
		const HRESULT herr = HRESULT_FROM_WIN32(err);
		printf("%s: GetModuleFilenameA failed: %s - %s [0x%08" PRIX32 "]\n", __func__,
		       NtStatus2Tag(herr), Win32ErrorCode2Tag(err), err);
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
		const UINT32 err = GetLastError();
		const HRESULT herr = HRESULT_FROM_WIN32(err);
		printf("%s: LoadLibraryA failure: %s - %s [0x%08" PRIX32 "]\n", __func__,
		       NtStatus2Tag(herr), Win32ErrorCode2Tag(err), err);
		return -1;
	}

	if (!FreeLibrary(library))
	{
		const UINT32 err = GetLastError();
		const HRESULT herr = HRESULT_FROM_WIN32(err);
		printf("%s: FreeLibrary failure: %s - %s [0x%08" PRIX32 "]\n", __func__, NtStatus2Tag(herr),
		       Win32ErrorCode2Tag(err), err);
		return -1;
	}

	return 0;
}
