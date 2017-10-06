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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <winpr/crt.h>
#include <winpr/platform.h>
#include <winpr/heap.h>
#include <winpr/file.h>
#include <winpr/tchar.h>
#include <winpr/environment.h>

#include <winpr/path.h>

#if defined(__IOS__)
#include "shell_ios.h"
#endif

#if defined(WIN32)
#include <Shlobj.h>
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

static char* GetEnvAlloc(LPCSTR lpName)
{
	DWORD length;
	char* env = NULL;
	length = GetEnvironmentVariableA(lpName, NULL, 0);

	if (length > 0)
	{
		env = malloc(length);

		if (!env)
			return NULL;

		if (GetEnvironmentVariableA(lpName, env, length) != length - 1)
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
	size_t size;
	char* path = NULL;
#if defined(WIN32) || defined(__IOS__)
	path = GetPath_XDG_CONFIG_HOME();
#else
	char* home = NULL;
	/**
	 * There is a single base directory relative to which user-specific data files should be written.
	 * This directory is defined by the environment variable $XDG_DATA_HOME.
	 *
	 * $XDG_DATA_HOME defines the base directory relative to which user specific data files should be stored.
	 * If $XDG_DATA_HOME is either not set or empty, a default equal to $HOME/.local/share should be used.
	 */
	path = GetEnvAlloc("XDG_DATA_HOME");

	if (path)
		return path;

	home = GetPath_HOME();

	if (!home)
		return NULL;

	size = strlen(home) + strlen("/.local/share") + 1;
	path = (char*) malloc(size);

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
	size_t size;
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
	char* home = NULL;
	/**
	 * There is a single base directory relative to which user-specific configuration files should be written.
	 * This directory is defined by the environment variable $XDG_CONFIG_HOME.
	 *
	 * $XDG_CONFIG_HOME defines the base directory relative to which user specific configuration files should be stored.
	 * If $XDG_CONFIG_HOME is either not set or empty, a default equal to $HOME/.config should be used.
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
	path = (char*) malloc(size);

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
	size_t size;
	char* path = NULL;
	char* home = NULL;
#if defined(WIN32)
	home = GetPath_XDG_RUNTIME_DIR();

	if (home)
	{
		path = GetCombinedPath(home, "cache");

		if (!PathFileExistsA(path))
			if (!CreateDirectoryA(path, NULL))
				path = NULL;
	}

	free(home);
#elif defined(__IOS__)
	path = ios_get_cache();
#else
	/**
	 * There is a single base directory relative to which user-specific non-essential (cached) data should be written.
	 * This directory is defined by the environment variable $XDG_CACHE_HOME.
	 *
	 * $XDG_CACHE_HOME defines the base directory relative to which user specific non-essential data files should be stored.
	 * If $XDG_CACHE_HOME is either not set or empty, a default equal to $HOME/.cache should be used.
	 */
	path = GetEnvAlloc("XDG_CACHE_HOME");

	if (path)
		return path;

	home = GetPath_HOME();

	if (!home)
		return NULL;

	size = strlen(home) + strlen("/.cache") + 1;
	path = (char*) malloc(size);

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

	if (FAILED(SHGetFolderPathA(0, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT,
	                            path)))
	{
		free(path);
		return NULL;
	}

#else
	/**
	 * There is a single base directory relative to which user-specific runtime files and other file objects should be placed.
	 * This directory is defined by the environment variable $XDG_RUNTIME_DIR.
	 *
	 * $XDG_RUNTIME_DIR defines the base directory relative to which user-specific non-essential runtime files and other
	 * file objects (such as sockets, named pipes, ...) should be stored. The directory MUST be owned by the user,
	 * and he MUST be the only one having read and write access to it. Its Unix access mode MUST be 0700.
	 *
	 * The lifetime of the directory MUST be bound to the user being logged in. It MUST be created when the user first
	 * logs in and if the user fully logs out the directory MUST be removed. If the user logs in more than once he should
	 * get pointed to the same directory, and it is mandatory that the directory continues to exist from his first login
	 * to his last logout on the system, and not removed in between. Files in the directory MUST not survive reboot or a
	 * full logout/login cycle.
	 *
	 * The directory MUST be on a local file system and not shared with any other system. The directory MUST by fully-featured
	 * by the standards of the operating system. More specifically, on Unix-like operating systems AF_UNIX sockets,
	 * symbolic links, hard links, proper permissions, file locking, sparse files, memory mapping, file change notifications,
	 * a reliable hard link count must be supported, and no restrictions on the file name character set should be imposed.
	 * Files in this directory MAY be subjected to periodic clean-up. To ensure that your files are not removed, they should
	 * have their access time timestamp modified at least once every 6 hours of monotonic time or the 'sticky' bit should be
	 * set on the file.
	 *
	 * If $XDG_RUNTIME_DIR is not set applications should fall back to a replacement directory with similar capabilities and
	 * print a warning message. Applications should use this directory for communication and synchronization purposes and
	 * should not place larger files in it, since it might reside in runtime memory and cannot necessarily be swapped out to disk.
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
	nSize = GetEnvironmentVariableA(name, NULL, 0);

	if (nSize)
	{
		env = (LPSTR) malloc(nSize);

		if (!env)
			return NULL;

		if (GetEnvironmentVariableA(name, env, nSize) != nSize - 1)
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
	int length;
	HRESULT status;
	char* path = NULL;
	char* subPathCpy;
	int basePathLength = 0;
	int subPathLength = 0;

	if (basePath)
		basePathLength = (int) strlen(basePath);

	if (subPath)
		subPathLength = (int) strlen(subPath);

	length = basePathLength + subPathLength + 1;
	path = (char*) malloc(length + 1);

	if (!path)
		return NULL;

	if (basePath)
		CopyMemory(path, basePath, basePathLength);

	path[basePathLength] = '\0';

	if (FAILED(PathCchConvertStyleA(path, basePathLength, PATH_STYLE_NATIVE)))
	{
		free(path);
		return NULL;
	}

	if (!subPath)
		return path;

	subPathCpy = _strdup(subPath);

	if (!subPathCpy)
	{
		free(path);
		return NULL;
	}

	if (FAILED(PathCchConvertStyleA(subPathCpy, subPathLength, PATH_STYLE_NATIVE)))
	{
		free(path);
		free(subPathCpy);
		return NULL;
	}

	status = NativePathCchAppendA(path, length + 1, subPathCpy);
	free(subPathCpy);

	if (FAILED(status))
	{
		free(path);
		return NULL;
	}
	else
		return path;
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
	if (!path || *path != delim)
		return FALSE;

	if (!(dup = _strdup(path)))
		return FALSE;

	for (p = dup; p;)
	{
		if ((p = strchr(p + 1, delim)))
			* p = '\0';

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
	BOOL ret;

	if (ConvertFromUnicode(CP_UTF8, 0, pszPath, -1, &lpFileNameA, 0, NULL, NULL) < 1)
		return FALSE;

	ret = PathFileExistsA(lpFileNameA);
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
			continue;    /* Skip . and .. */

		empty = 0;
		break;
	}

	closedir(dir);
	return empty;
}


BOOL PathIsDirectoryEmptyW(LPCWSTR pszPath)
{
	LPSTR lpFileNameA = NULL;
	BOOL ret;

	if (ConvertFromUnicode(CP_UTF8, 0, pszPath, -1, &lpFileNameA, 0, NULL, NULL) < 1)
		return FALSE;

	ret = PathIsDirectoryEmptyA(lpFileNameA);
	free(lpFileNameA);
	return ret;
}


#else

#ifdef _WIN32
#pragma comment(lib, "shlwapi.lib")
#endif

#endif
