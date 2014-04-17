/**
 * WinPR: Windows Portable Runtime
 * Path Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <winpr/heap.h>
#include <winpr/tchar.h>
#include <winpr/environment.h>

#include <winpr/path.h>

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
	DWORD length;
	char* env = NULL;

	length = GetEnvironmentVariableA(lpName, NULL, 0);

	if (length > 0)
	{
		env = malloc(length + 1);
		GetEnvironmentVariableA(lpName, env, length + 1);
		env[length] = '\0';
	}

	return env;
}

char* GetPath_HOME()
{
	char* path = NULL;

#ifdef _WIN32
	path = GetEnvAlloc("UserProfile");
#elif defined(ANDROID)
	path = malloc(2);
	strcpy(path, "/");
#else
	path = GetEnvAlloc("HOME");
#endif

	return path;
}

char* GetPath_TEMP()
{
	char* path = NULL;

#ifdef _WIN32
	path = GetEnvAlloc("TEMP");
#else
	path = GetEnvAlloc("TMPDIR");

	if (!path)
		path = _strdup("/tmp");
#endif

	return path;
}

char* GetPath_XDG_DATA_HOME()
{
	char* path = NULL;
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

	path = (char*) malloc(strlen(home) + strlen("/.local/share") + 1);
	sprintf(path, "%s%s", home, "/.local/share");

	free(home);

	return path;
}

char* GetPath_XDG_CONFIG_HOME()
{
	char* path = NULL;
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

	path = (char*) malloc(strlen(home) + strlen("/.config") + 1);
	sprintf(path, "%s%s", home, "/.config");

	free(home);

	return path;
}

char* GetPath_XDG_CACHE_HOME()
{
	char* path = NULL;
	char* home = NULL;

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

	path = (char*) malloc(strlen(home) + strlen("/.cache") + 1);
	sprintf(path, "%s%s", home, "/.cache");

	free(home);

	return path;
}

char* GetPath_XDG_RUNTIME_DIR()
{
	char* path = NULL;

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
	subPath = GetCombinedPath(knownPath, path);

	free(knownPath);

	return subPath;
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
		basePathLength = strlen(basePath);
	if (subPath)
		subPathLength = strlen(subPath);

	length = basePathLength + subPathLength + 1;
	path = (char*) malloc(length + 1);
	if (!path)
		return NULL;

	if (basePath)
		CopyMemory(path, basePath, basePathLength);
	path[basePathLength] = '\0';

	PathCchConvertStyleA(path, basePathLength, PATH_STYLE_NATIVE);

	if (!subPath)
		return path;

	subPathCpy = _strdup(subPath);
	PathCchConvertStyleA(subPathCpy, subPathLength, PATH_STYLE_NATIVE);

	status = NativePathCchAppendA(path, length + 1, subPathCpy);

	free(subPathCpy);

	return path;
}

//#ifndef _WIN32

BOOL PathFileExistsA(LPCSTR pszPath)
{
	struct stat stat_info;

	if (stat(pszPath, &stat_info) != 0)
		return FALSE;

	return TRUE;
}

BOOL PathFileExistsW(LPCWSTR pszPath)
{
	return FALSE;
}

//#endif
