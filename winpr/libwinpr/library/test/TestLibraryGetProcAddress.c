
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/windows.h>
#include <winpr/library.h>
#include <winpr/nt.h>

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

	if (!(pFunctionA = GetProcAddressAs(library, "FunctionA", TEST_AB_FN)))
	{
		const UINT32 err = GetLastError();
		const HRESULT herr = HRESULT_FROM_WIN32(err);
		printf("%s: GetProcAddress failure (FunctionA) %s - %s [0x%08" PRIX32 "]\n", __func__,
		       NtStatus2Tag(herr), Win32ErrorCode2Tag(err), err);
		return -1;
	}

	if (!(pFunctionB = GetProcAddressAs(library, "FunctionB", TEST_AB_FN)))
	{
		const UINT32 err = GetLastError();
		const HRESULT herr = HRESULT_FROM_WIN32(err);
		printf("%s: GetProcAddress failure (FunctionB) %s - %s [0x%08" PRIX32 "]\n", __func__,
		       NtStatus2Tag(herr), Win32ErrorCode2Tag(err), err);
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
		const UINT32 err = GetLastError();
		const HRESULT herr = HRESULT_FROM_WIN32(err);
		printf("%s: FreeLibrary failure: %s - %s [0x%08" PRIX32 "]\n", __func__, NtStatus2Tag(herr),
		       Win32ErrorCode2Tag(err), err);
		return -1;
	}

	return 0;
}
