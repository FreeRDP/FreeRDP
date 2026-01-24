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

//#define WINPR_HAVE_PATHCCH_H	1

#ifdef WINPR_HAVE_PATHCCH_H

#include <Pathcch.h>

#else

#ifdef __cplusplus
extern "C"
{
#endif

#define PATHCCH_ALLOW_LONG_PATHS \
	0x00000001 /* Allow building of \\?\ paths if longer than MAX_PATH */

#define VOLUME_PREFIX _T("\\\\?\\Volume")
#define VOLUME_PREFIX_LEN ((sizeof(VOLUME_PREFIX) / sizeof(TCHAR)) - 1)

	/*
	 * Maximum number of characters we support using the "\\?\" syntax
	 * (0x7FFF + 1 for NULL terminator)
	 */

#define PATHCCH_MAX_CCH 0x8000

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchAddBackslashA(PSTR pszPath, size_t cchPath);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchAddBackslashW(PWSTR pszPath, size_t cchPath);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchRemoveBackslashA(PSTR pszPath, size_t cchPath);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchRemoveBackslashW(PWSTR pszPath, size_t cchPath);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchAddBackslashExA(PSTR pszPath, size_t cchPath,
	                                                              PSTR* ppszEnd,
	                                                              size_t* pcchRemaining);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchAddBackslashExW(PWSTR pszPath, size_t cchPath,
	                                                              PWSTR* ppszEnd,
	                                                              size_t* pcchRemaining);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchRemoveBackslashExA(PSTR pszPath, size_t cchPath,
	                                                                 PSTR* ppszEnd,
	                                                                 size_t* pcchRemaining);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchRemoveBackslashExW(PWSTR pszPath, size_t cchPath,
	                                                                 PWSTR* ppszEnd,
	                                                                 size_t* pcchRemaining);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchAddExtensionA(PSTR pszPath, size_t cchPath,
	                                                            PCSTR pszExt);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchAddExtensionW(PWSTR pszPath, size_t cchPath,
	                                                            PCWSTR pszExt);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchAppendA(PSTR pszPath, size_t cchPath,
	                                                      PCSTR pszMore);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchAppendW(PWSTR pszPath, size_t cchPath,
	                                                      PCWSTR pszMore);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchAppendExA(PSTR pszPath, size_t cchPath,
	                                                        PCSTR pszMore, unsigned long dwFlags);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchAppendExW(PWSTR pszPath, size_t cchPath,
	                                                        PCWSTR pszMore, unsigned long dwFlags);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchCanonicalizeA(PSTR pszPathOut, size_t cchPathOut,
	                                                            PCSTR pszPathIn);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchCanonicalizeW(PWSTR pszPathOut, size_t cchPathOut,
	                                                            PCWSTR pszPathIn);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchCanonicalizeExA(PSTR pszPathOut,
	                                                              size_t cchPathOut,
	                                                              PCSTR pszPathIn,
	                                                              unsigned long dwFlags);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchCanonicalizeExW(PWSTR pszPathOut,
	                                                              size_t cchPathOut,
	                                                              PCWSTR pszPathIn,
	                                                              unsigned long dwFlags);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathAllocCanonicalizeA(PCSTR pszPathIn,
	                                                              unsigned long dwFlags,
	                                                              PSTR* ppszPathOut);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathAllocCanonicalizeW(PCWSTR pszPathIn,
	                                                              unsigned long dwFlags,
	                                                              PWSTR* ppszPathOut);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchCombineA(PSTR pszPathOut, size_t cchPathOut,
	                                                       PCSTR pszPathIn, PCSTR pszMore);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchCombineW(PWSTR pszPathOut, size_t cchPathOut,
	                                                       PCWSTR pszPathIn, PCWSTR pszMore);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchCombineExA(PSTR pszPathOut, size_t cchPathOut,
	                                                         PCSTR pszPathIn, PCSTR pszMore,
	                                                         unsigned long dwFlags);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchCombineExW(PWSTR pszPathOut, size_t cchPathOut,
	                                                         PCWSTR pszPathIn, PCWSTR pszMore,
	                                                         unsigned long dwFlags);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathAllocCombineA(PCSTR pszPathIn, PCSTR pszMore,
	                                                         unsigned long dwFlags,
	                                                         PSTR* ppszPathOut);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathAllocCombineW(PCWSTR pszPathIn, PCWSTR pszMore,
	                                                         unsigned long dwFlags,
	                                                         PWSTR* ppszPathOut);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchFindExtensionA(PCSTR pszPath, size_t cchPath,
	                                                             PCSTR* ppszExt);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchFindExtensionW(PCWSTR pszPath, size_t cchPath,
	                                                             PCWSTR* ppszExt);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchRenameExtensionA(PSTR pszPath, size_t cchPath,
	                                                               PCSTR pszExt);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchRenameExtensionW(PWSTR pszPath, size_t cchPath,
	                                                               PCWSTR pszExt);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchRemoveExtensionA(PSTR pszPath, size_t cchPath);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchRemoveExtensionW(PWSTR pszPath, size_t cchPath);

	WINPR_API WINPR_ATTR_NODISCARD BOOL PathCchIsRootA(PCSTR pszPath);
	WINPR_API WINPR_ATTR_NODISCARD BOOL PathCchIsRootW(PCWSTR pszPath);

	WINPR_API WINPR_ATTR_NODISCARD BOOL PathIsUNCExA(PCSTR pszPath, PCSTR* ppszServer);
	WINPR_API WINPR_ATTR_NODISCARD BOOL PathIsUNCExW(PCWSTR pszPath, PCWSTR* ppszServer);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchSkipRootA(PCSTR pszPath, PCSTR* ppszRootEnd);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchSkipRootW(PCWSTR pszPath, PCWSTR* ppszRootEnd);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchStripToRootA(PSTR pszPath, size_t cchPath);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchStripToRootW(PWSTR pszPath, size_t cchPath);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchStripPrefixA(PSTR pszPath, size_t cchPath);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchStripPrefixW(PWSTR pszPath, size_t cchPath);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchRemoveFileSpecA(PSTR pszPath, size_t cchPath);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchRemoveFileSpecW(PWSTR pszPath, size_t cchPath);

#ifdef UNICODE
#define PathCchAddBackslash PathCchAddBackslashW
#define PathCchRemoveBackslash PathCchRemoveBackslashW
#define PathCchAddBackslashEx PathCchAddBackslashExW
#define PathCchRemoveBackslashEx PathCchRemoveBackslashExW
#define PathCchAddExtension PathCchAddExtensionW
#define PathCchAppend PathCchAppendW
#define PathCchAppendEx PathCchAppendExW
#define PathCchCanonicalize PathCchCanonicalizeW
#define PathCchCanonicalizeEx PathCchCanonicalizeExW
#define PathAllocCanonicalize PathAllocCanonicalizeW
#define PathCchCombine PathCchCombineW
#define PathCchCombineEx PathCchCombineExW
#define PathAllocCombine PathAllocCombineW
#define PathCchFindExtension PathCchFindExtensionW
#define PathCchRenameExtension PathCchRenameExtensionW
#define PathCchRemoveExtension PathCchRemoveExtensionW
#define PathCchIsRoot PathCchIsRootW
#define PathIsUNCEx PathIsUNCExW
#define PathCchSkipRoot PathCchSkipRootW
#define PathCchStripToRoot PathCchStripToRootW
#define PathCchStripPrefix PathCchStripPrefixW
#define PathCchRemoveFileSpec PathCchRemoveFileSpecW
#else
#define PathCchAddBackslash PathCchAddBackslashA
#define PathCchRemoveBackslash PathCchRemoveBackslashA
#define PathCchAddBackslashEx PathCchAddBackslashExA
#define PathCchRemoveBackslashEx PathCchRemoveBackslashExA
#define PathCchAddExtension PathCchAddExtensionA
#define PathCchAppend PathCchAppendA
#define PathCchAppendEx PathCchAppendExA
#define PathCchCanonicalize PathCchCanonicalizeA
#define PathCchCanonicalizeEx PathCchCanonicalizeExA
#define PathAllocCanonicalize PathAllocCanonicalizeA
#define PathCchCombine PathCchCombineA
#define PathCchCombineEx PathCchCombineExA
#define PathAllocCombine PathAllocCombineA
#define PathCchFindExtension PathCchFindExtensionA
#define PathCchRenameExtension PathCchRenameExtensionA
#define PathCchRemoveExtension PathCchRemoveExtensionA
#define PathCchIsRoot PathCchIsRootA
#define PathIsUNCEx PathIsUNCExA
#define PathCchSkipRoot PathCchSkipRootA
#define PathCchStripToRoot PathCchStripToRootA
#define PathCchStripPrefix PathCchStripPrefixA
#define PathCchRemoveFileSpec PathCchRemoveFileSpecA
#endif

	/* Unix-style Paths */

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchAddSlashA(PSTR pszPath, size_t cchPath);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchAddSlashW(PWSTR pszPath, size_t cchPath);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchAddSlashExA(PSTR pszPath, size_t cchPath,
	                                                          PSTR* ppszEnd, size_t* pcchRemaining);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchAddSlashExW(PWSTR pszPath, size_t cchPath,
	                                                          PWSTR* ppszEnd,
	                                                          size_t* pcchRemaining);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT UnixPathCchAddExtensionA(PSTR pszPath, size_t cchPath,
	                                                                PCSTR pszExt);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT UnixPathCchAddExtensionW(PWSTR pszPath, size_t cchPath,
	                                                                PCWSTR pszExt);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT UnixPathCchAppendA(PSTR pszPath, size_t cchPath,
	                                                          PCSTR pszMore);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT UnixPathCchAppendW(PWSTR pszPath, size_t cchPath,
	                                                          PCWSTR pszMore);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT UnixPathAllocCombineA(PCSTR pszPathIn, PCSTR pszMore,
	                                                             unsigned long dwFlags,
	                                                             PSTR* ppszPathOut);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT UnixPathAllocCombineW(PCWSTR pszPathIn, PCWSTR pszMore,
	                                                             unsigned long dwFlags,
	                                                             PWSTR* ppszPathOut);

#ifdef UNICODE
#define PathCchAddSlash PathCchAddSlashW
#define PathCchAddSlashEx PathCchAddSlashExW
#define UnixPathCchAddExtension UnixPathCchAddExtensionW
#define UnixPathCchAppend UnixPathCchAppendW
#define UnixPathAllocCombine UnixPathAllocCombineW
#else
#define PathCchAddSlash PathCchAddSlashA
#define PathCchAddSlashEx PathCchAddSlashExA
#define UnixPathCchAddExtension UnixPathCchAddExtensionA
#define UnixPathCchAppend UnixPathCchAppendA
#define UnixPathAllocCombine UnixPathAllocCombineA
#endif

	/* Native-style Paths */

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchAddSeparatorA(PSTR pszPath, size_t cchPath);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchAddSeparatorW(PWSTR pszPath, size_t cchPath);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchAddSeparatorExA(PSTR pszPath, size_t cchPath,
	                                                              PSTR* ppszEnd,
	                                                              size_t* pcchRemaining);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchAddSeparatorExW(PWSTR pszPath, size_t cchPath,
	                                                              PWSTR* ppszEnd,
	                                                              size_t* pcchRemaining);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT NativePathCchAddExtensionA(PSTR pszPath, size_t cchPath,
	                                                                  PCSTR pszExt);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT NativePathCchAddExtensionW(PWSTR pszPath, size_t cchPath,
	                                                                  PCWSTR pszExt);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT NativePathCchAppendA(PSTR pszPath, size_t cchPath,
	                                                            PCSTR pszMore);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT NativePathCchAppendW(PWSTR pszPath, size_t cchPath,
	                                                            PCWSTR pszMore);

	WINPR_API WINPR_ATTR_NODISCARD HRESULT NativePathAllocCombineA(PCSTR pszPathIn, PCSTR pszMore,
	                                                               unsigned long dwFlags,
	                                                               PSTR* ppszPathOut);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT NativePathAllocCombineW(PCWSTR pszPathIn, PCWSTR pszMore,
	                                                               unsigned long dwFlags,
	                                                               PWSTR* ppszPathOut);

#ifdef UNICODE
#define PathCchAddSeparator PathCchAddSeparatorW
#define PathCchAddSeparatorEx PathCchAddSeparatorExW
#define NativePathCchAddExtension NativePathCchAddExtensionW
#define NativePathCchAppend NativePathCchAppendW
#define NativePathAllocCombine NativePathAllocCombineW
#else
#define PathCchAddSeparator PathCchAddSeparatorA
#define PathCchAddSeparatorEx PathCchAddSeparatorExA
#define NativePathCchAddExtension NativePathCchAddExtensionA
#define NativePathCchAppend NativePathCchAppendA
#define NativePathAllocCombine NativePathAllocCombineA
#endif

	/* Path Portability Functions */

#define PATH_STYLE_WINDOWS 0x00000001
#define PATH_STYLE_UNIX 0x00000002
#define PATH_STYLE_NATIVE 0x00000003

#define PATH_SHARED_LIB_EXT_WITH_DOT 0x00000001
#define PATH_SHARED_LIB_EXT_APPLE_SO 0x00000002
#define PATH_SHARED_LIB_EXT_EXPLICIT 0x80000000
#define PATH_SHARED_LIB_EXT_EXPLICIT_DLL 0x80000001
#define PATH_SHARED_LIB_EXT_EXPLICIT_SO 0x80000002
#define PATH_SHARED_LIB_EXT_EXPLICIT_DYLIB 0x80000003

	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchConvertStyleA(PSTR pszPath, size_t cchPath,
	                                                            unsigned long dwFlags);
	WINPR_API WINPR_ATTR_NODISCARD HRESULT PathCchConvertStyleW(PWSTR pszPath, size_t cchPath,
	                                                            unsigned long dwFlags);

	WINPR_API WINPR_ATTR_NODISCARD char PathGetSeparatorA(unsigned long dwFlags);
	WINPR_API WINPR_ATTR_NODISCARD WCHAR PathGetSeparatorW(unsigned long dwFlags);

	WINPR_API WINPR_ATTR_NODISCARD PCSTR PathGetSharedLibraryExtensionA(unsigned long dwFlags);
	WINPR_API WINPR_ATTR_NODISCARD PCWSTR PathGetSharedLibraryExtensionW(unsigned long dwFlags);

#ifdef UNICODE
#define PathCchConvertStyle PathCchConvertStyleW
#define PathGetSeparator PathGetSeparatorW
#define PathGetSharedLibraryExtension PathGetSharedLibraryExtensionW
#else
#define PathCchConvertStyle PathCchConvertStyleA
#define PathGetSeparator PathGetSeparatorW
#define PathGetSharedLibraryExtension PathGetSharedLibraryExtensionA
#endif

#ifdef __cplusplus
}
#endif

#endif

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
	 * directory if \b filename is \b NULL)
	 *
	 *  @param system a boolean indicating the configuration base, \b TRUE for system configuration,
	 * \b FALSE for user configuration
	 *  @param filename an optional configuration file name to append.
	 *
	 *  @return The absolute path of the desired configuration or \b NULL in case of failure. Use \b
	 * free to clean up the allocated string.
	 *
	 *
	 *  @since version 3.9.0
	 */
	WINPR_ATTR_MALLOC(free, 1)
	WINPR_API WINPR_ATTR_NODISCARD char* winpr_GetConfigFilePath(BOOL system, const char* filename);

	WINPR_API WINPR_ATTR_NODISCARD const char* GetKnownPathIdString(int id);

	WINPR_ATTR_MALLOC(free, 1)
	WINPR_API WINPR_ATTR_NODISCARD char* GetKnownPath(eKnownPathTypes id);

	WINPR_ATTR_MALLOC(free, 1)
	WINPR_API WINPR_ATTR_NODISCARD char* GetKnownSubPath(eKnownPathTypes id, const char* path);

	WINPR_ATTR_MALLOC(free, 1)
	WINPR_API WINPR_ATTR_NODISCARD char* GetEnvironmentPath(char* name);

	WINPR_ATTR_MALLOC(free, 1)
	WINPR_API WINPR_ATTR_NODISCARD char* GetEnvironmentSubPath(char* name, const char* path);

	WINPR_ATTR_MALLOC(free, 1)
	WINPR_API WINPR_ATTR_NODISCARD char* GetCombinedPath(const char* basePath, const char* subPath);

	WINPR_API WINPR_ATTR_NODISCARD BOOL PathMakePathA(LPCSTR path,
	                                                  LPSECURITY_ATTRIBUTES lpAttributes);
	WINPR_API WINPR_ATTR_NODISCARD BOOL PathMakePathW(LPCWSTR path,
	                                                  LPSECURITY_ATTRIBUTES lpAttributes);

#if !defined(_WIN32) || defined(_UWP)

	WINPR_API WINPR_ATTR_NODISCARD BOOL PathIsRelativeA(LPCSTR pszPath);
	WINPR_API WINPR_ATTR_NODISCARD BOOL PathIsRelativeW(LPCWSTR pszPath);

	WINPR_API WINPR_ATTR_NODISCARD BOOL PathFileExistsA(LPCSTR pszPath);
	WINPR_API WINPR_ATTR_NODISCARD BOOL PathFileExistsW(LPCWSTR pszPath);

	WINPR_API WINPR_ATTR_NODISCARD BOOL PathIsDirectoryEmptyA(LPCSTR pszPath);
	WINPR_API WINPR_ATTR_NODISCARD BOOL PathIsDirectoryEmptyW(LPCWSTR pszPath);

#ifdef UNICODE
#define PathFileExists PathFileExistsW
#define PathIsDirectoryEmpty PathIsDirectoryEmptyW
#else
#define PathFileExists PathFileExistsA
#define PathIsDirectoryEmpty PathIsDirectoryEmptyA
#endif

#endif

	WINPR_API WINPR_ATTR_NODISCARD BOOL winpr_MoveFile(LPCSTR lpExistingFileName,
	                                                   LPCSTR lpNewFileName);
	WINPR_API WINPR_ATTR_NODISCARD BOOL winpr_MoveFileEx(LPCSTR lpExistingFileName,
	                                                     LPCSTR lpNewFileName, DWORD dwFlags);
	WINPR_API WINPR_ATTR_NODISCARD BOOL winpr_DeleteFile(const char* lpFileName);
	WINPR_API WINPR_ATTR_NODISCARD BOOL winpr_RemoveDirectory(LPCSTR lpPathName);
	WINPR_API WINPR_ATTR_NODISCARD BOOL winpr_RemoveDirectory_RecursiveA(LPCSTR lpPathName);
	WINPR_API WINPR_ATTR_NODISCARD BOOL winpr_RemoveDirectory_RecursiveW(LPCWSTR lpPathName);
	WINPR_API WINPR_ATTR_NODISCARD BOOL winpr_PathFileExists(const char* pszPath);
	WINPR_API WINPR_ATTR_NODISCARD BOOL winpr_PathMakePath(const char* path,
	                                                       LPSECURITY_ATTRIBUTES lpAttributes);

#ifdef __cplusplus
}
#endif

#ifdef _WIN32
#include <shlwapi.h>
#endif

#endif /* WINPR_PATH_H */
