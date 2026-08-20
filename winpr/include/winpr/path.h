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

#ifndef WINPR_PATH_H
#define WINPR_PATH_H

#include <winpr/winpr.h>
#include <winpr/tchar.h>
#include <winpr/error.h>
#include <winpr/wtypes.h>

#include <winpr/Pathcch.h>

/**
 * Shell Path Functions
 */

typedef enum
{
	KNOWN_PATH_HOME = 1,
	KNOWN_PATH_TEMP = 2,
	KNOWN_PATH_XDG_DATA_HOME = 3,
	KNOWN_PATH_XDG_CONFIG_HOME = 4,
	KNOWN_PATH_XDG_CACHE_HOME = 5,
	KNOWN_PATH_XDG_RUNTIME_DIR = 6,
	KNOWN_PATH_SYSTEM_CONFIG_HOME = 7
} eKnownPathTypes;

#ifdef __cplusplus
extern "C"
{
#endif

	/** @brief Return the absolute path of a configuration file (the path of the configuration
	 * directory if \b filename is \b nullptr)
	 *
	 *  @param system a boolean indicating the configuration base, \b TRUE for system configuration,
	 * \b FALSE for user configuration
	 *  @param filename an optional configuration file name to append.
	 *
	 *  @return The absolute path of the desired configuration or \b nullptr in case of failure. Use
	 * \b free to clean up the allocated string.
	 *
	 *
	 *  @since version 3.9.0
	 */
	WINPR_ATTR_MALLOC(free, 1)
	WINPR_API char* winpr_GetConfigFilePath(BOOL system, const char* filename);

	/** @brief Get a config file sub path with a formatting argument constructing the filename
	 *
	 *  @param system \b TRUE to return a system config path
	 *  @param filename The format string to generate the filename. Must not be \b nullptr. Must not
	 * contain any forbidden characters.
	 *
	 *  @return A (absolute) configuration file path or \b nullptr in case of failure.
	 *  @since version 3.23.0
	 */
	WINPR_ATTR_MALLOC(free, 1)
	WINPR_ATTR_FORMAT_ARG(2, 3)
	WINPR_API char* winpr_GetConfigFilePathV(BOOL system, WINPR_FORMAT_ARG const char* filename,
	                                         ...);

	/** @brief Get a config file sub path with a formatting argument constructing the filename
	 *
	 *  @param system \b TRUE to return a system config path
	 *  @param filename The format string to generate the filename. Must not be \b nullptr. Must not
	 * contain any forbidden characters.
	 *  @param ap The argument list
	 *
	 *  @return A (absolute) configuration file path or \b nullptr in case of failure.
	 *  @since version 3.23.0
	 */
	WINPR_ATTR_MALLOC(free, 1)
	WINPR_ATTR_FORMAT_ARG(2, 0)
	WINPR_API char* winpr_GetConfigFilePathVA(BOOL system, WINPR_FORMAT_ARG const char* filename,
	                                          va_list ap);

	WINPR_ATTR_NODISCARD
	WINPR_API const char* GetKnownPathIdString(int id);

	WINPR_ATTR_MALLOC(free, 1)
	WINPR_API char* GetKnownPath(eKnownPathTypes id);

	WINPR_ATTR_MALLOC(free, 1)
	WINPR_API char* GetKnownSubPath(eKnownPathTypes id, const char* path);

	/** @brief Append a path to some existing known path type.
	 *
	 *  @param id a \ref eKnownPathTypes known path id
	 *  @param path the format string generating the subpath. Must not be \b nullptr
	 *
	 *  @return A string of combined \b id path and \b path or \b nullptr in case of an error.
	 * @since version 3.23.0
	 */
	WINPR_ATTR_MALLOC(free, 1)
	WINPR_ATTR_FORMAT_ARG(2, 3)
	WINPR_API char* GetKnownSubPathV(eKnownPathTypes id, const char* path, ...);

	/** @brief Append a path to some existing known path type.
	 *
	 *  @param id a \ref eKnownPathTypes known path id
	 *  @param path the format string generating the subpath. Must not be \b nullptr
	 *  @param ap a va_list containing the format string arguments
	 * 	     * 	       @return A string of combined \b basePath and \b path or \b nullptr in case of
	 * an error.
	 * *  @version since 3.23.0
	 */
	WINPR_ATTR_MALLOC(free, 1)
	WINPR_ATTR_FORMAT_ARG(2, 0)
	WINPR_API char* GetKnownSubPathVA(eKnownPathTypes id, const char* path, va_list ap);

	WINPR_ATTR_MALLOC(free, 1)
	WINPR_API char* GetEnvironmentPath(char* name);

	WINPR_ATTR_MALLOC(free, 1)
	WINPR_API char* GetEnvironmentSubPath(char* name, const char* path);

	/** @brief Append a path to some existing environment name.
	 *
	 *  @param name The prefix path to use, must not be \b nullptr
	 *  @param path A format string used to generate the path to append. Must not be \b nullptr
	 *
	 *  @return A string of combined \b basePath and \b path or \b nullptr in case of an error.
	 * @version since 3.23.0
	 */
	WINPR_ATTR_MALLOC(free, 1)
	WINPR_ATTR_FORMAT_ARG(2, 3)
	WINPR_API char* GetEnvironmentSubPathV(char* name, WINPR_FORMAT_ARG const char* path, ...);

	/** @brief Append a path to some existing environment name.
	 *
	 * 	       @param name The prefix path to use, must not be \b nullptr
	 *  @param path A format string used to generate the path to append. Must not be \b nullptr
	 *  @param ap a va_list containing the format string arguments
	 *
	 * 	       @return A string of combined \b basePath and \b path or \b nullptr in case of an
	 * error.
	 * *  @version since 3.23.0
	 */
	WINPR_ATTR_MALLOC(free, 1)
	WINPR_ATTR_FORMAT_ARG(2, 0)
	WINPR_API char* GetEnvironmentSubPathVA(char* name, WINPR_FORMAT_ARG const char* path,
	                                        va_list ap);

	WINPR_ATTR_MALLOC(free, 1)
	WINPR_API char* GetCombinedPath(const char* basePath, const char* subPath);

	/** @brief Append a path to some existing path. A system dependent path separator will be added
	 * automatically.
	 *
	 *  @bug before version 3.23.0 the function did not allow subPath to be a format string.
	 *
	 *  @param basePath The prefix path to use, must not be \b nullptr
	 *  @param subPathFmt A format string used to generate the path to append. Must not be \b
	 * nullptr
	 *
	 *  @return A string of combined \b basePath and \b subPathFmt or \b nullptr in case of an
	 * error.
	 */
	WINPR_ATTR_MALLOC(free, 1)
	WINPR_ATTR_FORMAT_ARG(2, 3)
	WINPR_API char* GetCombinedPathV(const char* basePath, WINPR_FORMAT_ARG const char* subPathFmt,
	                                 ...);

	/** @brief Append a path to some existing path. A system dependent path separator will be added
	 * automatically.
	 *
	 *  @param basePath The prefix path to use, must not be \b nullptr
	 *  @param subPathFmt A format string used to generate the path to append. Must not be \b
	 * nullptr
	 *  @param ap a va_list containing the format string arguments
	 *
	 *  @return A string of combined \b basePath and \b subPathFmt or \b nullptr in case of an
	 * error.
	 *  @version since 3.23.0
	 */
	WINPR_ATTR_MALLOC(free, 1)
	WINPR_ATTR_FORMAT_ARG(2, 0)
	WINPR_API char* GetCombinedPathVA(const char* basePath, WINPR_FORMAT_ARG const char* subPathFmt,
	                                  va_list ap);

	WINPR_ATTR_NODISCARD
	WINPR_API BOOL PathMakePathA(LPCSTR path, LPSECURITY_ATTRIBUTES lpAttributes);

	WINPR_ATTR_NODISCARD
	WINPR_API BOOL PathMakePathW(LPCWSTR path, LPSECURITY_ATTRIBUTES lpAttributes);

#if !defined(_WIN32) || defined(_UWP)

	WINPR_ATTR_NODISCARD
	WINPR_API BOOL PathIsRelativeA(LPCSTR pszPath);

	WINPR_ATTR_NODISCARD
	WINPR_API BOOL PathIsRelativeW(LPCWSTR pszPath);

	WINPR_ATTR_NODISCARD
	WINPR_API BOOL PathFileExistsA(LPCSTR pszPath);

	WINPR_ATTR_NODISCARD
	WINPR_API BOOL PathFileExistsW(LPCWSTR pszPath);

	WINPR_ATTR_NODISCARD
	WINPR_API BOOL PathIsDirectoryEmptyA(LPCSTR pszPath);

	WINPR_ATTR_NODISCARD
	WINPR_API BOOL PathIsDirectoryEmptyW(LPCWSTR pszPath);

#ifdef UNICODE
#define PathFileExists PathFileExistsW
#define PathIsDirectoryEmpty PathIsDirectoryEmptyW
#else
#define PathFileExists PathFileExistsA
#define PathIsDirectoryEmpty PathIsDirectoryEmptyA
#endif

#endif

	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_MoveFile(LPCSTR lpExistingFileName, LPCSTR lpNewFileName);

	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_MoveFileEx(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, DWORD dwFlags);

	WINPR_API BOOL winpr_DeleteFile(const char* lpFileName);

	WINPR_API BOOL winpr_RemoveDirectory(LPCSTR lpPathName);

	WINPR_API BOOL winpr_RemoveDirectory_RecursiveA(LPCSTR lpPathName);

	WINPR_API BOOL winpr_RemoveDirectory_RecursiveW(LPCWSTR lpPathName);

	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_PathFileExists(const char* pszPath);

	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_PathMakePath(const char* path, LPSECURITY_ATTRIBUTES lpAttributes);

#ifdef __cplusplus
}
#endif

#ifdef _WIN32
#include <shlwapi.h>
#endif

#endif /* WINPR_PATH_H */
