
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/handle.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/collections.h>
#include <winpr/windows.h>

static const CHAR testFile1A[] = "TestFile1A";

static BOOL create_layout_files(size_t level, const char* BasePath, wArrayList* files)
{
	for (size_t x = 0; x < 10; x++)
	{
		CHAR FilePath[PATHCCH_MAX_CCH] = { 0 };
		strncpy(FilePath, BasePath, ARRAYSIZE(FilePath));

		CHAR name[64] = { 0 };
		(void)_snprintf(name, ARRAYSIZE(name), "%zd-TestFile%zd", level, x);
		NativePathCchAppendA(FilePath, PATHCCH_MAX_CCH, name);

		HANDLE hdl =
		    CreateFileA(FilePath, GENERIC_ALL, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hdl == INVALID_HANDLE_VALUE)
			return FALSE;
		ArrayList_Append(files, FilePath);
		(void)CloseHandle(hdl);
	}
	return TRUE;
}

static BOOL create_layout_directories(size_t level, size_t max_level, const char* BasePath,
                                      wArrayList* files)
{
	if (level >= max_level)
		return TRUE;

	CHAR FilePath[PATHCCH_MAX_CCH] = { 0 };
	strncpy(FilePath, BasePath, ARRAYSIZE(FilePath));
	PathCchConvertStyleA(FilePath, ARRAYSIZE(FilePath), PATH_STYLE_NATIVE);
	if (!winpr_PathMakePath(FilePath, NULL))
		return FALSE;
	ArrayList_Append(files, FilePath);

	if (!create_layout_files(level + 1, BasePath, files))
		return FALSE;

	for (size_t x = 0; x < 10; x++)
	{
		CHAR CurFilePath[PATHCCH_MAX_CCH] = { 0 };
		strncpy(CurFilePath, FilePath, ARRAYSIZE(CurFilePath));

		PathCchConvertStyleA(CurFilePath, ARRAYSIZE(CurFilePath), PATH_STYLE_NATIVE);

		CHAR name[64] = { 0 };
		(void)_snprintf(name, ARRAYSIZE(name), "%zd-TestPath%zd", level, x);
		NativePathCchAppendA(CurFilePath, PATHCCH_MAX_CCH, name);

		if (!create_layout_directories(level + 1, max_level, CurFilePath, files))
			return FALSE;
	}
	return TRUE;
}

static BOOL create_layout(const char* BasePath, wArrayList* files)
{
	CHAR BasePathNative[PATHCCH_MAX_CCH] = { 0 };
	memcpy(BasePathNative, BasePath, sizeof(BasePathNative));
	PathCchConvertStyleA(BasePathNative, ARRAYSIZE(BasePathNative), PATH_STYLE_NATIVE);

	return create_layout_directories(0, 3, BasePathNative, files);
}

static void cleanup_layout(const char* BasePath)
{
	winpr_RemoveDirectory_RecursiveA(BasePath);
}

static BOOL find_first_file_success(const char* FilePath)
{
	BOOL rc = FALSE;
	WIN32_FIND_DATAA FindData = { 0 };
	HANDLE hFind = FindFirstFileA(FilePath, &FindData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		printf("FindFirstFile failure: %s (INVALID_HANDLE_VALUE -1)\n", FilePath);
		goto fail;
	}

	printf("FindFirstFile: %s", FindData.cFileName);

	if (strcmp(FindData.cFileName, testFile1A) != 0)
	{
		printf("FindFirstFile failure: Expected: %s, Actual: %s\n", testFile1A, FindData.cFileName);
		goto fail;
	}
	rc = TRUE;
fail:
	if (hFind != INVALID_HANDLE_VALUE)
		FindClose(hFind);
	return rc;
}

static BOOL list_directory_dot(const char* BasePath, wArrayList* files)
{
	BOOL rc = FALSE;
	CHAR BasePathDot[PATHCCH_MAX_CCH] = { 0 };
	memcpy(BasePathDot, BasePath, ARRAYSIZE(BasePathDot));
	PathCchConvertStyleA(BasePathDot, ARRAYSIZE(BasePathDot), PATH_STYLE_NATIVE);
	NativePathCchAppendA(BasePathDot, PATHCCH_MAX_CCH, ".");
	WIN32_FIND_DATAA FindData = { 0 };
	HANDLE hFind = FindFirstFileA(BasePathDot, &FindData);
	if (hFind == INVALID_HANDLE_VALUE)
		return FALSE;
	size_t count = 0;
	do
	{
		count++;
		if (strcmp(FindData.cFileName, ".") != 0)
			goto fail;
	} while (FindNextFile(hFind, &FindData));

	rc = TRUE;
fail:
	FindClose(hFind);

	if (count != 1)
		return FALSE;
	return rc;
}

static BOOL list_directory_star(const char* BasePath, wArrayList* files)
{
	CHAR BasePathDot[PATHCCH_MAX_CCH] = { 0 };
	memcpy(BasePathDot, BasePath, ARRAYSIZE(BasePathDot));
	PathCchConvertStyleA(BasePathDot, ARRAYSIZE(BasePathDot), PATH_STYLE_NATIVE);
	NativePathCchAppendA(BasePathDot, PATHCCH_MAX_CCH, "*");
	WIN32_FIND_DATAA FindData = { 0 };
	HANDLE hFind = FindFirstFileA(BasePathDot, &FindData);
	if (hFind == INVALID_HANDLE_VALUE)
		return FALSE;
	size_t count = 0;
	size_t dotcount = 0;
	size_t dotdotcount = 0;
	do
	{
		if (strcmp(FindData.cFileName, ".") == 0)
			dotcount++;
		else if (strcmp(FindData.cFileName, "..") == 0)
			dotdotcount++;
		else
			count++;
	} while (FindNextFile(hFind, &FindData));
	FindClose(hFind);

	const char sep = PathGetSeparatorA(PATH_STYLE_NATIVE);
	size_t fcount = 0;
	const size_t baselen = strlen(BasePath);
	const size_t total = ArrayList_Count(files);
	for (size_t x = 0; x < total; x++)
	{
		const char* path = ArrayList_GetItem(files, x);
		const size_t pathlen = strlen(path);
		if (pathlen < baselen)
			continue;
		const char* skip = &path[baselen];
		if (*skip == sep)
			skip++;
		const char* end = strrchr(skip, sep);
		if (end)
			continue;
		fcount++;
	}

	if (fcount != count)
		return FALSE;
	return TRUE;
}

static BOOL find_first_file_fail(const char* FilePath)
{
	WIN32_FIND_DATAA FindData = { 0 };
	HANDLE hFind = FindFirstFileA(FilePath, &FindData);
	if (hFind == INVALID_HANDLE_VALUE)
		return TRUE;

	FindClose(hFind);
	return FALSE;
}

static int TestFileFindFirstFileA(const char* str)
{
	int rc = -1;
	if (!str)
		return -1;

	CHAR BasePath[PATHCCH_MAX_CCH] = { 0 };

	strncpy(BasePath, str, ARRAYSIZE(BasePath));

	const size_t length = strnlen(BasePath, PATHCCH_MAX_CCH - 1);

	CHAR FilePath[PATHCCH_MAX_CCH] = { 0 };
	CopyMemory(FilePath, BasePath, length * sizeof(CHAR));

	PathCchConvertStyleA(BasePath, length, PATH_STYLE_WINDOWS);

	wArrayList* files = ArrayList_New(FALSE);
	if (!files)
		return -3;
	wObject* obj = ArrayList_Object(files);
	obj->fnObjectFree = winpr_ObjectStringFree;
	obj->fnObjectNew = winpr_ObjectStringClone;

	if (!create_layout(BasePath, files))
		return -1;

	NativePathCchAppendA(FilePath, PATHCCH_MAX_CCH, testFile1A);

	printf("Finding file: %s\n", FilePath);

	if (!find_first_file_fail(FilePath))
		goto fail;

	HANDLE hdl =
	    CreateFileA(FilePath, GENERIC_ALL, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hdl == INVALID_HANDLE_VALUE)
		goto fail;
	(void)CloseHandle(hdl);

	if (!find_first_file_success(FilePath))
		goto fail;

	CHAR BasePathInvalid[PATHCCH_MAX_CCH] = { 0 };
	memcpy(BasePathInvalid, BasePath, ARRAYSIZE(BasePathInvalid));
	PathCchAddBackslashA(BasePathInvalid, PATHCCH_MAX_CCH);

	if (!find_first_file_fail(BasePathInvalid))
		goto fail;

	if (!list_directory_dot(BasePath, files))
		goto fail;

	if (!list_directory_star(BasePath, files))
		goto fail;

	rc = 0;
fail:
	DeleteFileA(FilePath);
	cleanup_layout(BasePath);
	ArrayList_Free(files);
	return rc;
}

static int TestFileFindFirstFileW(const char* str)
{
	WCHAR buffer[32] = { 0 };
	const WCHAR* testFile1W = InitializeConstWCharFromUtf8("TestFile1W", buffer, ARRAYSIZE(buffer));
	int rc = -1;
	if (!str)
		return -1;

	WCHAR BasePath[PATHCCH_MAX_CCH] = { 0 };

	(void)ConvertUtf8ToWChar(str, BasePath, ARRAYSIZE(BasePath));

	const size_t length = _wcsnlen(BasePath, PATHCCH_MAX_CCH - 1);

	WCHAR FilePath[PATHCCH_MAX_CCH] = { 0 };
	CopyMemory(FilePath, BasePath, length * sizeof(WCHAR));

	PathCchConvertStyleW(BasePath, length, PATH_STYLE_WINDOWS);
	NativePathCchAppendW(FilePath, PATHCCH_MAX_CCH, testFile1W);

	CHAR FilePathA[PATHCCH_MAX_CCH] = { 0 };
	(void)ConvertWCharNToUtf8(FilePath, ARRAYSIZE(FilePath), FilePathA, ARRAYSIZE(FilePathA));
	printf("Finding file: %s\n", FilePathA);

	WIN32_FIND_DATAW FindData = { 0 };
	HANDLE hFind = FindFirstFileW(FilePath, &FindData);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		printf("FindFirstFile failure: %s (INVALID_HANDLE_VALUE -1)\n", FilePathA);
		goto fail;
	}

	CHAR cFileName[MAX_PATH] = { 0 };
	(void)ConvertWCharNToUtf8(FindData.cFileName, ARRAYSIZE(FindData.cFileName), cFileName,
	                          ARRAYSIZE(cFileName));

	printf("FindFirstFile: %s", cFileName);

	if (_wcscmp(FindData.cFileName, testFile1W) != 0)
	{
		printf("FindFirstFile failure: Expected: %s, Actual: %s\n", testFile1A, cFileName);
		goto fail;
	}

	rc = 0;
fail:
	DeleteFileW(FilePath);
	FindClose(hFind);
	return rc;
}

int TestFileFindFirstFile(int argc, char* argv[])
{
	char* str = GetKnownSubPath(KNOWN_PATH_TEMP, "TestFileFindFirstFile");
	if (!str)
		return -23;

	cleanup_layout(str);

	int rc1 = -1;
	int rc2 = -1;
	if (winpr_PathMakePath(str, NULL))
	{
		rc1 = TestFileFindFirstFileA(str);
		rc2 = 0; // TestFileFindFirstFileW(str);
		winpr_RemoveDirectory(str);
	}
	free(str);
	return rc1 + rc2;
}
