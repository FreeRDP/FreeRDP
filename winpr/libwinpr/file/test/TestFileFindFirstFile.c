
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/windows.h>

static TCHAR testFile1[] = _T("TestFile1");

int TestFileFindFirstFile(int argc, char* argv[])
{
	char* str;
	int length;
	HANDLE hFind;
	LPTSTR BasePath;
	WIN32_FIND_DATA FindData;
	TCHAR FilePath[PATHCCH_MAX_CCH];
	str = argv[1];
#ifdef UNICODE
	length = MultiByteToWideChar(CP_UTF8, 0, str, strlen(str), NULL, 0);
	BasePath = (WCHAR*) calloc((length + 1), sizeof(WCHAR));

	if (!BasePath)
	{
		_tprintf(_T("Unable to allocate memory\n"));
		return -1;
	}

	MultiByteToWideChar(CP_UTF8, 0, str, length, (LPWSTR) BasePath, length * sizeof(WCHAR));
	BasePath[length] = 0;
#else
	BasePath = _strdup(str);

	if (!BasePath)
	{
		printf("Unable to allocate memory\n");
		return -1;
	}

	length = strlen(BasePath);
#endif
	CopyMemory(FilePath, BasePath, length * sizeof(TCHAR));
	FilePath[length] = 0;
	PathCchConvertStyle(BasePath, length, PATH_STYLE_WINDOWS);
	NativePathCchAppend(FilePath, PATHCCH_MAX_CCH, _T("TestFile1"));
	free(BasePath);
	_tprintf(_T("Finding file: %s\n"), FilePath);
	hFind = FindFirstFile(FilePath, &FindData);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		_tprintf(_T("FindFirstFile failure: %s (INVALID_HANDLE_VALUE -1)\n"), FilePath);
		return -1;
	}

	_tprintf(_T("FindFirstFile: %s"), FindData.cFileName);

	if (_tcscmp(FindData.cFileName, testFile1) != 0)
	{
		_tprintf(_T("FindFirstFile failure: Expected: %s, Actual: %s\n"),
		         testFile1, FindData.cFileName);
		return -1;
	}

	FindClose(hFind);
	return 0;
}
