/**
 * WinPR: Windows Portable Runtime
 * Path Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 David PHAM-VAN <d.phamvan@inuvika.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <winpr/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <winpr/crt.h>
#include <winpr/platform.h>
#include <winpr/file.h>
#include <winpr/tchar.h>
#include <winpr/environment.h>

#include <winpr/path.h>

#if defined(__IOS__)
#include "shell_ios.h"
#endif

#if defined(WIN32)
#include <shlobj.h>
#else
#include <errno.h>
#include <dirent.h>
#endif

static char* GetPath_XDG_CONFIG_HOME(void);
static char* GetPath_XDG_RUNTIME_DIR(void);

/**
 * SHGetKnownFolderPath function:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/bb762188/
 */

/**
 * XDG Base Directory Specification:
 * http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
 */

char* GetEnvAlloc(LPCSTR lpName)
{
	DWORD nSize;
	DWORD nStatus;
	char* env = NULL;

	nSize = GetEnvironmentVariableX(lpName, NULL, 0);

	if (nSize > 0)
	{
		env = malloc(nSize);

		if (!env)
			return NULL;

		nStatus = GetEnvironmentVariableX(lpName, env, nSize);

		if (nStatus != (nSize - 1))
		{
			free(env);
			return NULL;
		}
	}

	return env;
}

static char* GetPath_HOME(void)
{
	char* path = NULL;
#ifdef _WIN32
	path = GetEnvAlloc("UserProfile");
#elif defined(__IOS__)
	path = ios_get_home();
#else
	path = GetEnvAlloc("HOME");
#endif
	return path;
}

static char* GetPath_TEMP(void)
{
	char* path = NULL;
#ifdef _WIN32
	path = GetEnvAlloc("TEMP");
#elif defined(__IOS__)
	path = ios_get_temp();
#else
	path = GetEnvAlloc("TMPDIR");

	if (!path)
		path = _strdup("/tmp");

#endif
	return path;
}

static char* GetPath_XDG_DATA_HOME(void)
{
	char* path = NULL;
#if defined(WIN32) || defined(__IOS__)
	path = GetPath_XDG_CONFIG_HOME();
#else
	size_t size;
	char* home = NULL;
	/**
	 * There is a single base directory relative to which user-specific data files should be
	 * written. This directory is defined by the environment variable $XDG_DATA_HOME.
	 *
	 * $XDG_DATA_HOME defines the base directory relative to which user specific data files should
	 * be stored. If $XDG_DATA_HOME is either not set or empty, a default equal to
	 * $HOME/.local/share should be used.
	 */
	path = GetEnvAlloc("XDG_DATA_HOME");

	if (path)
		return path;

	home = GetPath_HOME();

	if (!home)
		return NULL;

	size = strlen(home) + strlen("/.local/share") + 1;
	path = (char*)malloc(size);

	if (!path)
	{
		free(home);
		return NULL;
	}

	sprintf_s(path, size, "%s%s", home, "/.local/share");
	free(home);
#endif
	return path;
}

static char* GetPath_XDG_CONFIG_HOME(void)
{
	char* path = NULL;
#if defined(WIN32) && !defined(_UWP)
	path = calloc(MAX_PATH, sizeof(char));

	if (!path)
		return NULL;

	if (FAILED(SHGetFolderPathA(0, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, path)))
	{
		free(path);
		return NULL;
	}

#elif defined(__IOS__)
	path = ios_get_data();
#else
	size_t size;
	char* home = NULL;
	/**
	 * There is a single base directory relative to which user-specific configuration files should
	 * be written. This directory is defined by the environment variable $XDG_CONFIG_HOME.
	 *
	 * $XDG_CONFIG_HOME defines the base directory relative to which user specific configuration
	 * files should be stored. If $XDG_CONFIG_HOME is either not set or empty, a default equal to
	 * $HOME/.config should be used.
	 */
	path = GetEnvAlloc("XDG_CONFIG_HOME");

	if (path)
		return path;

	home = GetPath_HOME();

	if (!home)
		home = GetPath_TEMP();

	if (!home)
		return NULL;

	size = strlen(home) + strlen("/.config") + 1;
	path = (char*)malloc(size);

	if (!path)
	{
		free(home);
		return NULL;
	}

	sprintf_s(path, size, "%s%s", home, "/.config");
	free(home);
#endif
	return path;
}

static char* GetPath_XDG_CACHE_HOME(void)
{
	char* path = NULL;
	char* home = NULL;
#if defined(WIN32)
	home = GetPath_XDG_RUNTIME_DIR();

	if (home)
	{
		path = GetCombinedPath(home, "cache");

		if (!winpr_PathFileExists(path))
			if (!CreateDirectoryA(path, NULL))
				path = NULL;
	}

	free(home);
#elif defined(__IOS__)
	path = ios_get_cache();
#else
	size_t size;
	/**
	 * There is a single base directory relative to which user-specific non-essential (cached) data
	 * should be written. This directory is defined by the environment variable $XDG_CACHE_HOME.
	 *
	 * $XDG_CACHE_HOME defines the base directory relative to which user specific non-essential data
	 * files should be stored. If $XDG_CACHE_HOME is either not set or empty, a default equal to
	 * $HOME/.cache should be used.
	 */
	path = GetEnvAlloc("XDG_CACHE_HOME");

	if (path)
		return path;

	home = GetPath_HOME();

	if (!home)
		return NULL;

	size = strlen(home) + strlen("/.cache") + 1;
	path = (char*)malloc(size);

	if (!path)
	{
		free(home);
		return NULL;
	}

	sprintf_s(path, size, "%s%s", home, "/.cache");
	free(home);
#endif
	return path;
}

char* GetPath_XDG_RUNTIME_DIR(void)
{
	char* path = NULL;
#if defined(WIN32) && !defined(_UWP)
	path = calloc(MAX_PATH, sizeof(char));

	if (!path)
		return NULL;

	if (FAILED(SHGetFolderPathA(0, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, path)))
	{
		free(path);
		return NULL;
	}

#else
	/**
	 * There is a single base directory relative to which user-specific runtime files and other file
	 * objects should be placed. This directory is defined by the environment variable
	 * $XDG_RUNTIME_DIR.
	 *
	 * $XDG_RUNTIME_DIR defines the base directory relative to which user-specific non-essential
	 * runtime files and other file objects (such as sockets, named pipes, ...) should be stored.
	 * The directory MUST be owned by the user, and he MUST be the only one having read and write
	 * access to it. Its Unix access mode MUST be 0700.
	 *
	 * The lifetime of the directory MUST be bound to the user being logged in. It MUST be created
	 * when the user first logs in and if the user fully logs out the directory MUST be removed. If
	 * the user logs in more than once he should get pointed to the same directory, and it is
	 * mandatory that the directory continues to exist from his first login to his last logout on
	 * the system, and not removed in between. Files in the directory MUST not survive reboot or a
	 * full logout/login cycle.
	 *
	 * The directory MUST be on a local file system and not shared with any other system. The
	 * directory MUST by fully-featured by the standards of the operating system. More specifically,
	 * on Unix-like operating systems AF_UNIX sockets, symbolic links, hard links, proper
	 * permissions, file locking, sparse files, memory mapping, file change notifications, a
	 * reliable hard link count must be supported, and no restrictions on the file name character
	 * set should be imposed. Files in this directory MAY be subjected to periodic clean-up. To
	 * ensure that your files are not removed, they should have their access time timestamp modified
	 * at least once every 6 hours of monotonic time or the 'sticky' bit should be set on the file.
	 *
	 * If $XDG_RUNTIME_DIR is not set applications should fall back to a replacement directory with
	 * similar capabilities and print a warning message. Applications should use this directory for
	 * communication and synchronization purposes and should not place larger files in it, since it
	 * might reside in runtime memory and cannot necessarily be swapped out to disk.
	 */
	path = GetEnvAlloc("XDG_RUNTIME_DIR");
#endif

	if (path)
		return path;

	path = GetPath_TEMP();
	return path;
}

char* GetKnownPath(int id)
{
	char* path = NULL;

	switch (id)
	{
		case KNOWN_PATH_HOME:
			path = GetPath_HOME();
			break;

		case KNOWN_PATH_TEMP:
			path = GetPath_TEMP();
			break;

		case KNOWN_PATH_XDG_DATA_HOME:
			path = GetPath_XDG_DATA_HOME();
			break;

		case KNOWN_PATH_XDG_CONFIG_HOME:
			path = GetPath_XDG_CONFIG_HOME();
			break;

		case KNOWN_PATH_XDG_CACHE_HOME:
			path = GetPath_XDG_CACHE_HOME();
			break;

		case KNOWN_PATH_XDG_RUNTIME_DIR:
			path = GetPath_XDG_RUNTIME_DIR();
			break;

		default:
			path = NULL;
			break;
	}

	return path;
}

char* GetKnownSubPath(int id, const char* path)
{
	char* subPath;
	char* knownPath;
	knownPath = GetKnownPath(id);

	if (!knownPath)
		return NULL;

	subPath = GetCombinedPath(knownPath, path);
	free(knownPath);
	return subPath;
}

char* GetEnvironmentPath(char* name)
{
	char* env = NULL;
	DWORD nSize;
	DWORD nStatus;
	nSize = GetEnvironmentVariableX(name, NULL, 0);

	if (nSize)
	{
		env = (LPSTR)malloc(nSize);

		if (!env)
			return NULL;

		nStatus = GetEnvironmentVariableX(name, env, nSize);

		if (nStatus != (nSize - 1))
		{
			free(env);
			return NULL;
		}
	}

	return env;
}

char* GetEnvironmentSubPath(char* name, const char* path)
{
	char* env;
	char* subpath;
	env = GetEnvironmentPath(name);

	if (!env)
		return NULL;

	subpath = GetCombinedPath(env, path);
	free(env);
	return subpath;
}

char* GetCombinedPath(const char* basePath, const char* subPath)
{
	size_t length;
	HRESULT status;
	char* path = NULL;
	char* subPathCpy = NULL;
	size_t basePathLength = 0;
	size_t subPathLength = 0;

	if (basePath)
		basePathLength = strlen(basePath);

	if (subPath)
		subPathLength = strlen(subPath);

	length = basePathLength + subPathLength + 1;
	path = (char*)calloc(1, length + 1);

	if (!path)
		goto fail;

	if (basePath)
		CopyMemory(path, basePath, basePathLength);

	if (FAILED(PathCchConvertStyleA(path, basePathLength, PATH_STYLE_NATIVE)))
		goto fail;

	if (!subPath)
		return path;

	subPathCpy = _strdup(subPath);

	if (!subPathCpy)
		goto fail;

	if (FAILED(PathCchConvertStyleA(subPathCpy, subPathLength, PATH_STYLE_NATIVE)))
		goto fail;

	status = NativePathCchAppendA(path, length + 1, subPathCpy);
	if (FAILED(status))
		goto fail;

	free(subPathCpy);
	return path;

fail:
	free(path);
	free(subPathCpy);
	return NULL;
}

BOOL PathMakePathA(LPCSTR path, LPSECURITY_ATTRIBUTES lpAttributes)
{
#if defined(_UWP)
	return FALSE;
#elif defined(_WIN32)
	return (SHCreateDirectoryExA(NULL, path, lpAttributes) == ERROR_SUCCESS);
#else
	const char delim = PathGetSeparatorA(PATH_STYLE_NATIVE);
	char* dup;
	char* p;
	BOOL result = TRUE;
	/* we only operate on a non-null, absolute path */
#if defined(__OS2__)

	if (!path)
		return FALSE;

#else

	if (!path || *path != delim)
		return FALSE;

#endif

	if (!(dup = _strdup(path)))
		return FALSE;

#ifdef __OS2__
	p = (strlen(dup) > 3) && (dup[1] == ':') && (dup[2] == delim)) ? &dup[3] : dup;

	while (p)
#else
	for (p = dup; p;)
#endif
	{
		if ((p = strchr(p + 1, delim)))
			*p = '\0';

		if (mkdir(dup, 0777) != 0)
			if (errno != EEXIST)
			{
				result = FALSE;
				break;
			}

		if (p)
			*p = delim;
	}

	free(dup);
	return (result);
#endif
}

BOOL PathMakePathW(LPCWSTR path, LPSECURITY_ATTRIBUTES lpAttributes)
{
#if defined(_UWP)
	return FALSE;
#elif defined(_WIN32)
	return (SHCreateDirectoryExW(NULL, path, lpAttributes) == ERROR_SUCCESS);
#else
	const WCHAR wdelim = PathGetSeparatorW(PATH_STYLE_NATIVE);
	const char delim = PathGetSeparatorA(PATH_STYLE_NATIVE);
	char* dup;
	char* p;
	BOOL result = TRUE;
	/* we only operate on a non-null, absolute path */
#if defined(__OS2__)

	if (!path)
		return FALSE;

#else

	if (!path || *path != wdelim)
		return FALSE;

#endif

	dup = ConvertWCharToUtf8Alloc(path, NULL);
	if (!dup)
		return FALSE;

#ifdef __OS2__
	p = (strlen(dup) > 3) && (dup[1] == ':') && (dup[2] == delim)) ? &dup[3] : dup;

	while (p)
#else
	for (p = dup; p;)
#endif
	{
		if ((p = strchr(p + 1, delim)))
			*p = '\0';

		if (mkdir(dup, 0777) != 0)
			if (errno != EEXIST)
			{
				result = FALSE;
				break;
			}

		if (p)
			*p = delim;
	}

	free(dup);
	return (result);
#endif
}

#if !defined(_WIN32) || defined(_UWP)

BOOL PathIsRelativeA(LPCSTR pszPath)
{
	if (!pszPath)
		return FALSE;

	return pszPath[0] != '/';
}

BOOL PathIsRelativeW(LPCWSTR pszPath)
{
	LPSTR lpFileNameA = NULL;
	BOOL ret = FALSE;

	if (!pszPath)
		goto fail;

	lpFileNameA = ConvertWCharToUtf8Alloc(pszPath, NULL);
	if (!lpFileNameA)
		goto fail;
	ret = PathIsRelativeA(lpFileNameA);
fail:
	free(lpFileNameA);
	return ret;
}

BOOL PathFileExistsA(LPCSTR pszPath)
{
	struct stat stat_info;

	if (stat(pszPath, &stat_info) != 0)
		return FALSE;

	return TRUE;
}

BOOL PathFileExistsW(LPCWSTR pszPath)
{
	LPSTR lpFileNameA = NULL;
	BOOL ret = FALSE;

	if (!pszPath)
		goto fail;
	lpFileNameA = ConvertWCharToUtf8Alloc(pszPath, NULL);
	if (!lpFileNameA)
		goto fail;

	ret = winpr_PathFileExists(lpFileNameA);
fail:
	free(lpFileNameA);
	return ret;
}

BOOL PathIsDirectoryEmptyA(LPCSTR pszPath)
{
	struct dirent* dp;
	int empty = 1;
	DIR* dir = opendir(pszPath);

	if (dir == NULL) /* Not a directory or doesn't exist */
		return 1;

	while ((dp = readdir(dir)) != NULL)
	{
		if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
			continue; /* Skip . and .. */

		empty = 0;
		break;
	}

	closedir(dir);
	return empty;
}

BOOL PathIsDirectoryEmptyW(LPCWSTR pszPath)
{
	LPSTR lpFileNameA = NULL;
	BOOL ret = FALSE;
	if (!pszPath)
		goto fail;
	lpFileNameA = ConvertWCharToUtf8Alloc(pszPath, NULL);
	if (!lpFileNameA)
		goto fail;
	ret = PathIsDirectoryEmptyA(lpFileNameA);
fail:
	free(lpFileNameA);
	return ret;
}

#else

#ifdef _MSC_VER
#pragma comment(lib, "shlwapi.lib")
#endif

#endif

BOOL winpr_MoveFile(LPCSTR lpExistingFileName, LPCSTR lpNewFileName)
{
#ifndef _WIN32
	return MoveFileA(lpExistingFileName, lpNewFileName);
#else
	BOOL result = FALSE;
	LPWSTR lpExistingFileNameW = NULL;
	LPWSTR lpNewFileNameW = NULL;

	if (!lpExistingFileName || !lpNewFileName)
		return FALSE;

	lpExistingFileNameW = ConvertUtf8ToWCharAlloc(lpExistingFileName, NULL);
	if (!lpExistingFileNameW)
		goto cleanup;
	lpNewFileNameW = ConvertUtf8ToWCharAlloc(lpNewFileName, NULL);
	if (!lpNewFileNameW)
		goto cleanup;

	result = MoveFileW(lpExistingFileNameW, lpNewFileNameW);

cleanup:
	free(lpExistingFileNameW);
	free(lpNewFileNameW);
	return result;
#endif
}

BOOL winpr_MoveFileEx(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, DWORD dwFlags)
{
#ifndef _WIN32
	return MoveFileExA(lpExistingFileName, lpNewFileName, dwFlags);
#else
	BOOL result = FALSE;
	LPWSTR lpExistingFileNameW = NULL;
	LPWSTR lpNewFileNameW = NULL;

	if (!lpExistingFileName || !lpNewFileName)
		return FALSE;

	lpExistingFileNameW = ConvertUtf8ToWCharAlloc(lpExistingFileName, NULL);
	if (!lpExistingFileNameW)
		goto cleanup;
	lpNewFileNameW = ConvertUtf8ToWCharAlloc(lpNewFileName, NULL);
	if (!lpNewFileNameW)
		goto cleanup;

	result = MoveFileExW(lpExistingFileNameW, lpNewFileNameW, dwFlags);

cleanup:
	free(lpExistingFileNameW);
	free(lpNewFileNameW);
	return result;
#endif
}

BOOL winpr_DeleteFile(const char* lpFileName)
{
#ifndef _WIN32
	return DeleteFileA(lpFileName);
#else
	LPWSTR lpFileNameW = NULL;
	BOOL result = FALSE;

	if (lpFileName)
	{
		lpFileNameW = ConvertUtf8ToWCharAlloc(lpFileName, NULL);
		if (!lpFileNameW)
			goto cleanup;
	}

	result = DeleteFileW(lpFileNameW);

cleanup:
	free(lpFileNameW);
	return result;
#endif
}

BOOL winpr_RemoveDirectory(LPCSTR lpPathName)
{
#ifndef _WIN32
	return RemoveDirectoryA(lpPathName);
#else
	LPWSTR lpPathNameW = NULL;
	BOOL result = FALSE;

	if (lpPathName)
	{
		lpPathNameW = ConvertUtf8ToWCharAlloc(lpPathName, NULL);
		if (!lpPathNameW)
			goto cleanup;
	}

	result = RemoveDirectoryW(lpPathNameW);

cleanup:
	free(lpPathNameW);
	return result;
#endif
}

BOOL winpr_PathFileExists(const char* pszPath)
{
	if (!pszPath)
		return FALSE;
#ifndef _WIN32
	return PathFileExistsA(pszPath);
#else
	WCHAR* pathW = ConvertUtf8ToWCharAlloc(pszPath, NULL);
	BOOL result = FALSE;

	if (!pathW)
		return FALSE;

	result = PathFileExistsW(pathW);
	free(pathW);

	return result;
#endif
}

BOOL winpr_PathMakePath(const char* path, LPSECURITY_ATTRIBUTES lpAttributes)
{
	if (!path)
		return FALSE;
#ifndef _WIN32
	return PathMakePathA(path, lpAttributes);
#else
	WCHAR* pathW = ConvertUtf8ToWCharAlloc(path, NULL);
	BOOL result = FALSE;

	if (!pathW)
		return FALSE;

	result = SHCreateDirectoryExW(NULL, pathW, lpAttributes) == ERROR_SUCCESS;
	free(pathW);

	return result;
#endif
}
