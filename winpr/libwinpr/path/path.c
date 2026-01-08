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

#include <winpr/config.h>
#include <winpr/version.h>
#include <winpr/build-config.h>

#include <winpr/crt.h>
#include <winpr/tchar.h>

#include <winpr/path.h>
#include <winpr/file.h>

#if defined(WITH_RESOURCE_VERSIONING)
#define STR(x) #x
#endif

static const char PATH_SLASH_CHR = '/';
static const char PATH_SLASH_STR[] = "/";

static const char PATH_BACKSLASH_CHR = '\\';
static const char PATH_BACKSLASH_STR[] = "\\";

#ifdef _WIN32
static const WCHAR PATH_SLASH_CHR_W = L'/';
static const WCHAR PATH_BACKSLASH_CHR_W = L'\\';
static const WCHAR PATH_SLASH_STR_W[] = L"/";
static const WCHAR PATH_BACKSLASH_STR_W[] = L"\\";
#else
#if defined(__BIG_ENDIAN__)
static const WCHAR PATH_SLASH_CHR_W = 0x2f00;
static const WCHAR PATH_BACKSLASH_CHR_W = 0x5c00;
static const WCHAR PATH_SLASH_STR_W[] = { 0x2f00, '\0' };
static const WCHAR PATH_BACKSLASH_STR_W[] = { 0x5c00, '\0' };
#else
static const WCHAR PATH_SLASH_CHR_W = '/';
static const WCHAR PATH_BACKSLASH_CHR_W = '\\';
static const WCHAR PATH_SLASH_STR_W[] = { '/', '\0' };
static const WCHAR PATH_BACKSLASH_STR_W[] = { '\\', '\0' };
#endif
#endif

#ifdef _WIN32
#define PATH_SEPARATOR_CHR PATH_BACKSLASH_CHR
#define PATH_SEPARATOR_STR PATH_BACKSLASH_STR
#define PATH_SEPARATOR_CHR_W PATH_BACKSLASH_CHR_W
#define PATH_SEPARATOR_STR_W PATH_BACKSLASH_STR_W
#else
#define PATH_SEPARATOR_CHR PATH_SLASH_CHR
#define PATH_SEPARATOR_STR PATH_SLASH_STR
#define PATH_SEPARATOR_CHR_W PATH_SLASH_CHR_W
#define PATH_SEPARATOR_STR_W PATH_SLASH_STR_W
#endif

#include "../log.h"
#define TAG WINPR_TAG("path")

/*
 * PathCchAddBackslash
 */

/* Windows-style Paths */

#define DEFINE_UNICODE FALSE
#define CUR_PATH_SEPARATOR_CHR PATH_BACKSLASH_CHR
#define PATH_CCH_ADD_SEPARATOR PathCchAddBackslashA
#include "include/PathCchAddSeparator.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR

#define DEFINE_UNICODE TRUE
#define CUR_PATH_SEPARATOR_CHR PATH_BACKSLASH_CHR_W
#define PATH_CCH_ADD_SEPARATOR PathCchAddBackslashW
#include "include/PathCchAddSeparator.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR

/* Unix-style Paths */

#define DEFINE_UNICODE FALSE
#define CUR_PATH_SEPARATOR_CHR PATH_SLASH_CHR
#define PATH_CCH_ADD_SEPARATOR PathCchAddSlashA
#include "include/PathCchAddSeparator.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR

#define DEFINE_UNICODE TRUE
#define CUR_PATH_SEPARATOR_CHR PATH_SLASH_CHR_W
#define PATH_CCH_ADD_SEPARATOR PathCchAddSlashW
#include "include/PathCchAddSeparator.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR

/* Native-style Paths */

#define DEFINE_UNICODE FALSE
#define CUR_PATH_SEPARATOR_CHR PATH_SEPARATOR_CHR
#define PATH_CCH_ADD_SEPARATOR PathCchAddSeparatorA
#include "include/PathCchAddSeparator.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR

#define DEFINE_UNICODE TRUE
#define CUR_PATH_SEPARATOR_CHR PATH_SEPARATOR_CHR_W
#define PATH_CCH_ADD_SEPARATOR PathCchAddSeparatorW
#include "include/PathCchAddSeparator.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR

/*
 * PathCchRemoveBackslash
 */

HRESULT PathCchRemoveBackslashA(WINPR_ATTR_UNUSED PSTR pszPath, WINPR_ATTR_UNUSED size_t cchPath)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

HRESULT PathCchRemoveBackslashW(WINPR_ATTR_UNUSED PWSTR pszPath, WINPR_ATTR_UNUSED size_t cchPath)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

/*
 * PathCchAddBackslashEx
 */

/* Windows-style Paths */

#define DEFINE_UNICODE FALSE
#define CUR_PATH_SEPARATOR_CHR PATH_BACKSLASH_CHR
#define PATH_CCH_ADD_SEPARATOR_EX PathCchAddBackslashExA
#include "include/PathCchAddSeparatorEx.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR_EX

#define DEFINE_UNICODE TRUE
#define CUR_PATH_SEPARATOR_CHR PATH_BACKSLASH_CHR_W
#define PATH_CCH_ADD_SEPARATOR_EX PathCchAddBackslashExW
#include "include/PathCchAddSeparatorEx.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR_EX

/* Unix-style Paths */

#define DEFINE_UNICODE FALSE
#define CUR_PATH_SEPARATOR_CHR PATH_SLASH_CHR
#define PATH_CCH_ADD_SEPARATOR_EX PathCchAddSlashExA
#include "include/PathCchAddSeparatorEx.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR_EX

#define DEFINE_UNICODE TRUE
#define CUR_PATH_SEPARATOR_CHR PATH_SLASH_CHR_W
#define PATH_CCH_ADD_SEPARATOR_EX PathCchAddSlashExW
#include "include/PathCchAddSeparatorEx.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR_EX

/* Native-style Paths */

#define DEFINE_UNICODE FALSE
#define CUR_PATH_SEPARATOR_CHR PATH_SEPARATOR_CHR
#define PATH_CCH_ADD_SEPARATOR_EX PathCchAddSeparatorExA
#include "include/PathCchAddSeparatorEx.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR_EX

#define DEFINE_UNICODE TRUE
#define CUR_PATH_SEPARATOR_CHR PATH_SEPARATOR_CHR_W
#define PATH_CCH_ADD_SEPARATOR_EX PathCchAddSeparatorExW
#include "include/PathCchAddSeparatorEx.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR_EX

HRESULT PathCchRemoveBackslashExA(WINPR_ATTR_UNUSED PSTR pszPath, WINPR_ATTR_UNUSED size_t cchPath,
                                  WINPR_ATTR_UNUSED PSTR* ppszEnd,
                                  WINPR_ATTR_UNUSED size_t* pcchRemaining)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

HRESULT PathCchRemoveBackslashExW(WINPR_ATTR_UNUSED PWSTR pszPath, WINPR_ATTR_UNUSED size_t cchPath,
                                  WINPR_ATTR_UNUSED PWSTR* ppszEnd,
                                  WINPR_ATTR_UNUSED size_t* pcchRemaining)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

/*
 * PathCchAddExtension
 */

/* Windows-style Paths */

#define DEFINE_UNICODE FALSE
#define CUR_PATH_SEPARATOR_CHR PATH_BACKSLASH_CHR
#define PATH_CCH_ADD_EXTENSION PathCchAddExtensionA
#include "include/PathCchAddExtension.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_EXTENSION

#define DEFINE_UNICODE TRUE
#define CUR_PATH_SEPARATOR_CHR PATH_BACKSLASH_CHR_W
#define PATH_CCH_ADD_EXTENSION PathCchAddExtensionW
#include "include/PathCchAddExtension.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_EXTENSION

/* Unix-style Paths */

#define DEFINE_UNICODE FALSE
#define CUR_PATH_SEPARATOR_CHR PATH_SLASH_CHR
#define PATH_CCH_ADD_EXTENSION UnixPathCchAddExtensionA
#include "include/PathCchAddExtension.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_EXTENSION

#define DEFINE_UNICODE TRUE
#define CUR_PATH_SEPARATOR_CHR PATH_SLASH_CHR_W
#define PATH_CCH_ADD_EXTENSION UnixPathCchAddExtensionW
#include "include/PathCchAddExtension.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_EXTENSION

/* Native-style Paths */

#define DEFINE_UNICODE FALSE
#define CUR_PATH_SEPARATOR_CHR PATH_SEPARATOR_CHR
#define PATH_CCH_ADD_EXTENSION NativePathCchAddExtensionA
#include "include/PathCchAddExtension.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_EXTENSION

#define DEFINE_UNICODE TRUE
#define CUR_PATH_SEPARATOR_CHR PATH_SEPARATOR_CHR_W
#define PATH_CCH_ADD_EXTENSION NativePathCchAddExtensionW
#include "include/PathCchAddExtension.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_EXTENSION

/*
 * PathCchAppend
 */

/* Windows-style Paths */

#define DEFINE_UNICODE FALSE
#define CUR_PATH_SEPARATOR_CHR PATH_BACKSLASH_CHR
#define CUR_PATH_SEPARATOR_STR PATH_BACKSLASH_STR
#define PATH_CCH_APPEND PathCchAppendA
#include "include/PathCchAppend.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef CUR_PATH_SEPARATOR_STR
#undef PATH_CCH_APPEND

#define DEFINE_UNICODE TRUE
#define CUR_PATH_SEPARATOR_CHR PATH_BACKSLASH_CHR_W
#define CUR_PATH_SEPARATOR_STR PATH_BACKSLASH_STR_W
#define PATH_CCH_APPEND PathCchAppendW
#include "include/PathCchAppend.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef CUR_PATH_SEPARATOR_STR
#undef PATH_CCH_APPEND

/* Unix-style Paths */

#define DEFINE_UNICODE FALSE
#define CUR_PATH_SEPARATOR_CHR PATH_SLASH_CHR
#define CUR_PATH_SEPARATOR_STR PATH_SLASH_STR
#define PATH_CCH_APPEND UnixPathCchAppendA
#include "include/PathCchAppend.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef CUR_PATH_SEPARATOR_STR
#undef PATH_CCH_APPEND

#define DEFINE_UNICODE TRUE
#define CUR_PATH_SEPARATOR_CHR PATH_SLASH_CHR_W
#define CUR_PATH_SEPARATOR_STR PATH_SLASH_STR_W
#define PATH_CCH_APPEND UnixPathCchAppendW
#include "include/PathCchAppend.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef CUR_PATH_SEPARATOR_STR
#undef PATH_CCH_APPEND

/* Native-style Paths */

#define DEFINE_UNICODE FALSE
#define CUR_PATH_SEPARATOR_CHR PATH_SEPARATOR_CHR
#define CUR_PATH_SEPARATOR_STR PATH_SEPARATOR_STR
#define PATH_CCH_APPEND NativePathCchAppendA
#include "include/PathCchAppend.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef CUR_PATH_SEPARATOR_STR
#undef PATH_CCH_APPEND

#define DEFINE_UNICODE TRUE
#define CUR_PATH_SEPARATOR_CHR PATH_SEPARATOR_CHR_W
#define CUR_PATH_SEPARATOR_STR PATH_SEPARATOR_STR_W
#define PATH_CCH_APPEND NativePathCchAppendW
#include "include/PathCchAppend.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef CUR_PATH_SEPARATOR_STR
#undef PATH_CCH_APPEND

/*
 * PathCchAppendEx
 */

HRESULT PathCchAppendExA(WINPR_ATTR_UNUSED PSTR pszPath, WINPR_ATTR_UNUSED size_t cchPath,
                         WINPR_ATTR_UNUSED PCSTR pszMore, WINPR_ATTR_UNUSED unsigned long dwFlags)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

HRESULT PathCchAppendExW(WINPR_ATTR_UNUSED PWSTR pszPath, WINPR_ATTR_UNUSED size_t cchPath,
                         WINPR_ATTR_UNUSED PCWSTR pszMore, WINPR_ATTR_UNUSED unsigned long dwFlags)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

/*
 * PathCchCanonicalize
 */

HRESULT PathCchCanonicalizeA(WINPR_ATTR_UNUSED PSTR pszPathOut, WINPR_ATTR_UNUSED size_t cchPathOut,
                             WINPR_ATTR_UNUSED PCSTR pszPathIn)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

HRESULT PathCchCanonicalizeW(WINPR_ATTR_UNUSED PWSTR pszPathOut,
                             WINPR_ATTR_UNUSED size_t cchPathOut,
                             WINPR_ATTR_UNUSED PCWSTR pszPathIn)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

/*
 * PathCchCanonicalizeEx
 */

HRESULT PathCchCanonicalizeExA(WINPR_ATTR_UNUSED PSTR pszPathOut,
                               WINPR_ATTR_UNUSED size_t cchPathOut,
                               WINPR_ATTR_UNUSED PCSTR pszPathIn,
                               WINPR_ATTR_UNUSED unsigned long dwFlags)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

HRESULT PathCchCanonicalizeExW(WINPR_ATTR_UNUSED PWSTR pszPathOut,
                               WINPR_ATTR_UNUSED size_t cchPathOut,
                               WINPR_ATTR_UNUSED PCWSTR pszPathIn,
                               WINPR_ATTR_UNUSED unsigned long dwFlags)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

/*
 * PathAllocCanonicalize
 */

HRESULT PathAllocCanonicalizeA(WINPR_ATTR_UNUSED PCSTR pszPathIn,
                               WINPR_ATTR_UNUSED unsigned long dwFlags,
                               WINPR_ATTR_UNUSED PSTR* ppszPathOut)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

HRESULT PathAllocCanonicalizeW(WINPR_ATTR_UNUSED PCWSTR pszPathIn,
                               WINPR_ATTR_UNUSED unsigned long dwFlags,
                               WINPR_ATTR_UNUSED PWSTR* ppszPathOut)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

/*
 * PathCchCombine
 */

HRESULT PathCchCombineA(WINPR_ATTR_UNUSED PSTR pszPathOut, WINPR_ATTR_UNUSED size_t cchPathOut,
                        WINPR_ATTR_UNUSED PCSTR pszPathIn, WINPR_ATTR_UNUSED PCSTR pszMore)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

HRESULT PathCchCombineW(WINPR_ATTR_UNUSED PWSTR pszPathOut, WINPR_ATTR_UNUSED size_t cchPathOut,
                        WINPR_ATTR_UNUSED PCWSTR pszPathIn, WINPR_ATTR_UNUSED PCWSTR pszMore)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

/*
 * PathCchCombineEx
 */

HRESULT PathCchCombineExA(WINPR_ATTR_UNUSED PSTR pszPathOut, WINPR_ATTR_UNUSED size_t cchPathOut,
                          WINPR_ATTR_UNUSED PCSTR pszPathIn, WINPR_ATTR_UNUSED PCSTR pszMore,
                          WINPR_ATTR_UNUSED unsigned long dwFlags)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

HRESULT PathCchCombineExW(WINPR_ATTR_UNUSED PWSTR pszPathOut, WINPR_ATTR_UNUSED size_t cchPathOut,
                          WINPR_ATTR_UNUSED PCWSTR pszPathIn, WINPR_ATTR_UNUSED PCWSTR pszMore,
                          WINPR_ATTR_UNUSED unsigned long dwFlags)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

/*
 * PathAllocCombine
 */

/* Windows-style Paths */

#define DEFINE_UNICODE FALSE
#define CUR_PATH_SEPARATOR_CHR PATH_BACKSLASH_CHR
#define CUR_PATH_SEPARATOR_STR PATH_BACKSLASH_STR
#define PATH_ALLOC_COMBINE PathAllocCombineA
#include "include/PathAllocCombine.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef CUR_PATH_SEPARATOR_STR
#undef PATH_ALLOC_COMBINE

#define DEFINE_UNICODE TRUE
#define CUR_PATH_SEPARATOR_CHR PATH_BACKSLASH_CHR_W
#define CUR_PATH_SEPARATOR_STR PATH_BACKSLASH_STR_W
#define PATH_ALLOC_COMBINE PathAllocCombineW
#include "include/PathAllocCombine.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef CUR_PATH_SEPARATOR_STR
#undef PATH_ALLOC_COMBINE

/* Unix-style Paths */

#define DEFINE_UNICODE FALSE
#define CUR_PATH_SEPARATOR_CHR PATH_SLASH_CHR
#define CUR_PATH_SEPARATOR_STR PATH_SLASH_STR
#define PATH_ALLOC_COMBINE UnixPathAllocCombineA
#include "include/PathAllocCombine.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef CUR_PATH_SEPARATOR_STR
#undef PATH_ALLOC_COMBINE

#define DEFINE_UNICODE TRUE
#define CUR_PATH_SEPARATOR_CHR PATH_SLASH_CHR_W
#define CUR_PATH_SEPARATOR_STR PATH_SLASH_STR_W
#define PATH_ALLOC_COMBINE UnixPathAllocCombineW
#include "include/PathAllocCombine.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef CUR_PATH_SEPARATOR_STR
#undef PATH_ALLOC_COMBINE

/* Native-style Paths */

#define DEFINE_UNICODE FALSE
#define CUR_PATH_SEPARATOR_CHR PATH_SEPARATOR_CHR
#define CUR_PATH_SEPARATOR_STR PATH_SEPARATOR_STR
#define PATH_ALLOC_COMBINE NativePathAllocCombineA
#include "include/PathAllocCombine.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef CUR_PATH_SEPARATOR_STR
#undef PATH_ALLOC_COMBINE

#define DEFINE_UNICODE TRUE
#define CUR_PATH_SEPARATOR_CHR PATH_SEPARATOR_CHR_W
#define CUR_PATH_SEPARATOR_STR PATH_SEPARATOR_STR_W
#define PATH_ALLOC_COMBINE NativePathAllocCombineW
#include "include/PathAllocCombine.h"
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef CUR_PATH_SEPARATOR_STR
#undef PATH_ALLOC_COMBINE

/**
 * PathCchFindExtension
 */

HRESULT PathCchFindExtensionA(PCSTR pszPath, size_t cchPath, PCSTR* ppszExt)
{
	const char* p = (const char*)pszPath;

	if (!pszPath || !cchPath || !ppszExt)
		return E_INVALIDARG;

	/* find end of string */

	while (*p && --cchPath)
	{
		p++;
	}

	if (*p)
	{
		/* pszPath is not null terminated within the cchPath range */
		return E_INVALIDARG;
	}

	/* If no extension is found, ppszExt must point to the string's terminating null */
	*ppszExt = p;

	/* search backwards for '.' */

	while (p > pszPath)
	{
		if (*p == '.')
		{
			*ppszExt = (PCSTR)p;
			break;
		}

		if ((*p == '\\') || (*p == '/') || (*p == ':'))
			break;

		p--;
	}

	return S_OK;
}

HRESULT PathCchFindExtensionW(WINPR_ATTR_UNUSED PCWSTR pszPath, WINPR_ATTR_UNUSED size_t cchPath,
                              WINPR_ATTR_UNUSED PCWSTR* ppszExt)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

/**
 * PathCchRenameExtension
 */

HRESULT PathCchRenameExtensionA(WINPR_ATTR_UNUSED PSTR pszPath, WINPR_ATTR_UNUSED size_t cchPath,
                                WINPR_ATTR_UNUSED PCSTR pszExt)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

HRESULT PathCchRenameExtensionW(WINPR_ATTR_UNUSED PWSTR pszPath, WINPR_ATTR_UNUSED size_t cchPath,
                                WINPR_ATTR_UNUSED PCWSTR pszExt)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

/**
 * PathCchRemoveExtension
 */

HRESULT PathCchRemoveExtensionA(WINPR_ATTR_UNUSED PSTR pszPath, WINPR_ATTR_UNUSED size_t cchPath)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

HRESULT PathCchRemoveExtensionW(WINPR_ATTR_UNUSED PWSTR pszPath, WINPR_ATTR_UNUSED size_t cchPath)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

/**
 * PathCchIsRoot
 */

BOOL PathCchIsRootA(WINPR_ATTR_UNUSED PCSTR pszPath)
{
	WLog_ERR(TAG, "not implemented");
	return FALSE;
}

BOOL PathCchIsRootW(WINPR_ATTR_UNUSED PCWSTR pszPath)
{
	WLog_ERR(TAG, "not implemented");
	return FALSE;
}

/**
 * PathIsUNCEx
 */

BOOL PathIsUNCExA(PCSTR pszPath, PCSTR* ppszServer)
{
	if (!pszPath)
		return FALSE;

	if ((pszPath[0] == '\\') && (pszPath[1] == '\\'))
	{
		*ppszServer = &pszPath[2];
		return TRUE;
	}

	return FALSE;
}

BOOL PathIsUNCExW(PCWSTR pszPath, PCWSTR* ppszServer)
{
	if (!pszPath)
		return FALSE;

	if ((pszPath[0] == '\\') && (pszPath[1] == '\\'))
	{
		*ppszServer = &pszPath[2];
		return TRUE;
	}

	return FALSE;
}

/**
 * PathCchSkipRoot
 */

HRESULT PathCchSkipRootA(WINPR_ATTR_UNUSED PCSTR pszPath, WINPR_ATTR_UNUSED PCSTR* ppszRootEnd)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

HRESULT PathCchSkipRootW(WINPR_ATTR_UNUSED PCWSTR pszPath, WINPR_ATTR_UNUSED PCWSTR* ppszRootEnd)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

/**
 * PathCchStripToRoot
 */

HRESULT PathCchStripToRootA(WINPR_ATTR_UNUSED PSTR pszPath, WINPR_ATTR_UNUSED size_t cchPath)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

HRESULT PathCchStripToRootW(WINPR_ATTR_UNUSED PWSTR pszPath, WINPR_ATTR_UNUSED size_t cchPath)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

/**
 * PathCchStripPrefix
 */

HRESULT PathCchStripPrefixA(PSTR pszPath, size_t cchPath)
{
	BOOL hasPrefix = 0;

	if (!pszPath)
		return E_INVALIDARG;

	if (cchPath < 4 || cchPath > PATHCCH_MAX_CCH)
		return E_INVALIDARG;

	hasPrefix = ((pszPath[0] == '\\') && (pszPath[1] == '\\') && (pszPath[2] == '?') &&
	             (pszPath[3] == '\\'))
	                ? TRUE
	                : FALSE;

	if (hasPrefix)
	{
		if (cchPath < 6)
			return S_FALSE;

		if (IsCharAlpha(pszPath[4]) && (pszPath[5] == ':')) /* like C: */
		{
			memmove_s(pszPath, cchPath, &pszPath[4], cchPath - 4);
			/* since the passed pszPath must not necessarily be null terminated
			 * and we always have enough space after the strip we can always
			 * ensure the null termination of the stripped result
			 */
			pszPath[cchPath - 4] = 0;
			return S_OK;
		}
	}

	return S_FALSE;
}

HRESULT PathCchStripPrefixW(PWSTR pszPath, size_t cchPath)
{
	BOOL hasPrefix = 0;

	if (!pszPath)
		return E_INVALIDARG;

	if (cchPath < 4 || cchPath > PATHCCH_MAX_CCH)
		return E_INVALIDARG;

	hasPrefix = ((pszPath[0] == '\\') && (pszPath[1] == '\\') && (pszPath[2] == '?') &&
	             (pszPath[3] == '\\'))
	                ? TRUE
	                : FALSE;

	if (hasPrefix)
	{
		if (cchPath < 6)
			return S_FALSE;

		const size_t rc = (_wcslen(&pszPath[4]) + 1);
		if (cchPath < rc)
			return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);

		if (IsCharAlphaW(pszPath[4]) && (pszPath[5] == L':')) /* like C: */
		{
			wmemmove_s(pszPath, cchPath, &pszPath[4], cchPath - 4);
			/* since the passed pszPath must not necessarily be null terminated
			 * and we always have enough space after the strip we can always
			 * ensure the null termination of the stripped result
			 */
			pszPath[cchPath - 4] = 0;
			return S_OK;
		}
	}

	return S_FALSE;
}

/**
 * PathCchRemoveFileSpec
 */

HRESULT PathCchRemoveFileSpecA(WINPR_ATTR_UNUSED PSTR pszPath, WINPR_ATTR_UNUSED size_t cchPath)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

HRESULT PathCchRemoveFileSpecW(WINPR_ATTR_UNUSED PWSTR pszPath, WINPR_ATTR_UNUSED size_t cchPath)
{
	WLog_ERR(TAG, "not implemented");
	return E_NOTIMPL;
}

/*
 * Path Portability Functions
 */

/**
 * PathCchConvertStyle
 */

HRESULT PathCchConvertStyleA(PSTR pszPath, size_t cchPath, unsigned long dwFlags)
{
	if (dwFlags == PATH_STYLE_WINDOWS)
	{
		for (size_t index = 0; index < cchPath; index++)
		{
			if (pszPath[index] == PATH_SLASH_CHR)
				pszPath[index] = PATH_BACKSLASH_CHR;
		}
	}
	else if (dwFlags == PATH_STYLE_UNIX)
	{
		for (size_t index = 0; index < cchPath; index++)
		{
			if (pszPath[index] == PATH_BACKSLASH_CHR)
				pszPath[index] = PATH_SLASH_CHR;
		}
	}
	else if (dwFlags == PATH_STYLE_NATIVE)
	{
		if (PATH_SEPARATOR_CHR == PATH_BACKSLASH_CHR)
		{
			/* Unix-style to Windows-style */

			for (size_t index = 0; index < cchPath; index++)
			{
				if (pszPath[index] == PATH_SLASH_CHR)
					pszPath[index] = PATH_BACKSLASH_CHR;
			}
		}
		else if (PATH_SEPARATOR_CHR == PATH_SLASH_CHR)
		{
			/* Windows-style to Unix-style */

			for (size_t index = 0; index < cchPath; index++)
			{
				if (pszPath[index] == PATH_BACKSLASH_CHR)
					pszPath[index] = PATH_SLASH_CHR;
			}
		}
		else
		{
			/* Unexpected error */
			return E_FAIL;
		}
	}
	else
	{
		/* Gangnam style? */
		return E_FAIL;
	}

	return S_OK;
}

HRESULT PathCchConvertStyleW(PWSTR pszPath, size_t cchPath, unsigned long dwFlags)
{
	if (dwFlags == PATH_STYLE_WINDOWS)
	{
		for (size_t index = 0; index < cchPath; index++)
		{
			if (pszPath[index] == PATH_SLASH_CHR_W)
				pszPath[index] = PATH_BACKSLASH_CHR_W;
		}
	}
	else if (dwFlags == PATH_STYLE_UNIX)
	{
		for (size_t index = 0; index < cchPath; index++)
		{
			if (pszPath[index] == PATH_BACKSLASH_CHR_W)
				pszPath[index] = PATH_SLASH_CHR_W;
		}
	}
	else if (dwFlags == PATH_STYLE_NATIVE)
	{
		if (PATH_SEPARATOR_CHR == PATH_BACKSLASH_CHR_W)
		{
			/* Unix-style to Windows-style */

			for (size_t index = 0; index < cchPath; index++)
			{
				if (pszPath[index] == PATH_SLASH_CHR_W)
					pszPath[index] = PATH_BACKSLASH_CHR_W;
			}
		}
		else if (PATH_SEPARATOR_CHR == PATH_SLASH_CHR_W)
		{
			/* Windows-style to Unix-style */

			for (size_t index = 0; index < cchPath; index++)
			{
				if (pszPath[index] == PATH_BACKSLASH_CHR_W)
					pszPath[index] = PATH_SLASH_CHR_W;
			}
		}
		else
		{
			/* Unexpected error */
			return E_FAIL;
		}
	}
	else
	{
		/* Gangnam style? */
		return E_FAIL;
	}

	return S_OK;
}

/**
 * PathGetSeparator
 */

char PathGetSeparatorA(unsigned long dwFlags)
{
	char separator = PATH_SEPARATOR_CHR;

	if (!dwFlags)
		dwFlags = PATH_STYLE_NATIVE;

	if (dwFlags == PATH_STYLE_WINDOWS)
		separator = PATH_SEPARATOR_CHR;
	else if (dwFlags == PATH_STYLE_UNIX)
		separator = PATH_SEPARATOR_CHR;
	else if (dwFlags == PATH_STYLE_NATIVE)
		separator = PATH_SEPARATOR_CHR;

	return separator;
}

WCHAR PathGetSeparatorW(unsigned long dwFlags)
{
	union
	{
		WCHAR w;
		char c[2];
	} cnv;

	cnv.c[0] = PATH_SEPARATOR_CHR;
	cnv.c[1] = '\0';

	if (!dwFlags)
		dwFlags = PATH_STYLE_NATIVE;

	if (dwFlags == PATH_STYLE_WINDOWS)
		cnv.c[0] = PATH_SEPARATOR_CHR;
	else if (dwFlags == PATH_STYLE_UNIX)
		cnv.c[0] = PATH_SEPARATOR_CHR;
	else if (dwFlags == PATH_STYLE_NATIVE)
		cnv.c[0] = PATH_SEPARATOR_CHR;

	return cnv.w;
}

/**
 * PathGetSharedLibraryExtension
 */
static const CHAR SharedLibraryExtensionDllA[] = "dll";
static const CHAR SharedLibraryExtensionSoA[] = "so";
static const CHAR SharedLibraryExtensionDylibA[] = "dylib";

static const CHAR SharedLibraryExtensionDotDllA[] = ".dll";
static const CHAR SharedLibraryExtensionDotSoA[] = ".so";
static const CHAR SharedLibraryExtensionDotDylibA[] = ".dylib";
PCSTR PathGetSharedLibraryExtensionA(unsigned long dwFlags)
{
	if (dwFlags & PATH_SHARED_LIB_EXT_EXPLICIT)
	{
		if (dwFlags & PATH_SHARED_LIB_EXT_WITH_DOT)
		{
			if (dwFlags & PATH_SHARED_LIB_EXT_EXPLICIT_DLL)
				return SharedLibraryExtensionDotDllA;

			if (dwFlags & PATH_SHARED_LIB_EXT_EXPLICIT_SO)
				return SharedLibraryExtensionDotSoA;

			if (dwFlags & PATH_SHARED_LIB_EXT_EXPLICIT_DYLIB)
				return SharedLibraryExtensionDotDylibA;
		}
		else
		{
			if (dwFlags & PATH_SHARED_LIB_EXT_EXPLICIT_DLL)
				return SharedLibraryExtensionDllA;

			if (dwFlags & PATH_SHARED_LIB_EXT_EXPLICIT_SO)
				return SharedLibraryExtensionSoA;

			if (dwFlags & PATH_SHARED_LIB_EXT_EXPLICIT_DYLIB)
				return SharedLibraryExtensionDylibA;
		}
	}

	if (dwFlags & PATH_SHARED_LIB_EXT_WITH_DOT)
	{
#ifdef _WIN32
		return SharedLibraryExtensionDotDllA;
#elif defined(__APPLE__)
		if (dwFlags & PATH_SHARED_LIB_EXT_APPLE_SO)
			return SharedLibraryExtensionDotSoA;
		else
			return SharedLibraryExtensionDotDylibA;
#else
		return SharedLibraryExtensionDotSoA;
#endif
	}
	else
	{
#ifdef _WIN32
		return SharedLibraryExtensionDllA;
#elif defined(__APPLE__)
		if (dwFlags & PATH_SHARED_LIB_EXT_APPLE_SO)
			return SharedLibraryExtensionSoA;
		else
			return SharedLibraryExtensionDylibA;
#else
		return SharedLibraryExtensionSoA;
#endif
	}

	return NULL;
}

PCWSTR PathGetSharedLibraryExtensionW(unsigned long dwFlags)
{
	static WCHAR buffer[6][16] = { 0 };
	const WCHAR* SharedLibraryExtensionDotDllW = InitializeConstWCharFromUtf8(
	    SharedLibraryExtensionDotDllA, buffer[0], ARRAYSIZE(buffer[0]));
	const WCHAR* SharedLibraryExtensionDotSoW =
	    InitializeConstWCharFromUtf8(SharedLibraryExtensionDotSoA, buffer[1], ARRAYSIZE(buffer[1]));
	const WCHAR* SharedLibraryExtensionDotDylibW = InitializeConstWCharFromUtf8(
	    SharedLibraryExtensionDotDylibA, buffer[2], ARRAYSIZE(buffer[2]));
	const WCHAR* SharedLibraryExtensionDllW =
	    InitializeConstWCharFromUtf8(SharedLibraryExtensionDllA, buffer[3], ARRAYSIZE(buffer[3]));
	const WCHAR* SharedLibraryExtensionSoW =
	    InitializeConstWCharFromUtf8(SharedLibraryExtensionSoA, buffer[4], ARRAYSIZE(buffer[4]));
	const WCHAR* SharedLibraryExtensionDylibW =
	    InitializeConstWCharFromUtf8(SharedLibraryExtensionDylibA, buffer[5], ARRAYSIZE(buffer[5]));

	if (dwFlags & PATH_SHARED_LIB_EXT_EXPLICIT)
	{
		if (dwFlags & PATH_SHARED_LIB_EXT_WITH_DOT)
		{
			if (dwFlags & PATH_SHARED_LIB_EXT_EXPLICIT_DLL)
				return SharedLibraryExtensionDotDllW;

			if (dwFlags & PATH_SHARED_LIB_EXT_EXPLICIT_SO)
				return SharedLibraryExtensionDotSoW;

			if (dwFlags & PATH_SHARED_LIB_EXT_EXPLICIT_DYLIB)
				return SharedLibraryExtensionDotDylibW;
		}
		else
		{
			if (dwFlags & PATH_SHARED_LIB_EXT_EXPLICIT_DLL)
				return SharedLibraryExtensionDllW;

			if (dwFlags & PATH_SHARED_LIB_EXT_EXPLICIT_SO)
				return SharedLibraryExtensionSoW;

			if (dwFlags & PATH_SHARED_LIB_EXT_EXPLICIT_DYLIB)
				return SharedLibraryExtensionDylibW;
		}
	}

	if (dwFlags & PATH_SHARED_LIB_EXT_WITH_DOT)
	{
#ifdef _WIN32
		return SharedLibraryExtensionDotDllW;
#elif defined(__APPLE__)
		if (dwFlags & PATH_SHARED_LIB_EXT_APPLE_SO)
			return SharedLibraryExtensionDotSoW;
		else
			return SharedLibraryExtensionDotDylibW;
#else
		return SharedLibraryExtensionDotSoW;
#endif
	}
	else
	{
#ifdef _WIN32
		return SharedLibraryExtensionDllW;
#elif defined(__APPLE__)
		if (dwFlags & PATH_SHARED_LIB_EXT_APPLE_SO)
			return SharedLibraryExtensionSoW;
		else
			return SharedLibraryExtensionDylibW;
#else
		return SharedLibraryExtensionSoW;
#endif
	}

	return NULL;
}

const char* GetKnownPathIdString(int id)
{
	switch (id)
	{
		case KNOWN_PATH_HOME:
			return "KNOWN_PATH_HOME";
		case KNOWN_PATH_TEMP:
			return "KNOWN_PATH_TEMP";
		case KNOWN_PATH_XDG_DATA_HOME:
			return "KNOWN_PATH_XDG_DATA_HOME";
		case KNOWN_PATH_XDG_CONFIG_HOME:
			return "KNOWN_PATH_XDG_CONFIG_HOME";
		case KNOWN_PATH_XDG_CACHE_HOME:
			return "KNOWN_PATH_XDG_CACHE_HOME";
		case KNOWN_PATH_XDG_RUNTIME_DIR:
			return "KNOWN_PATH_XDG_RUNTIME_DIR";
		case KNOWN_PATH_SYSTEM_CONFIG_HOME:
			return "KNOWN_PATH_SYSTEM_CONFIG_HOME";
		default:
			return "KNOWN_PATH_UNKNOWN_ID";
	}
}

static char* concat(const char* path, size_t pathlen, const char* name, size_t namelen)
{
	const size_t strsize = pathlen + namelen + 2;
	char* str = calloc(strsize, sizeof(char));
	if (!str)
		return NULL;

	winpr_str_append(path, str, strsize, "");
	winpr_str_append(name, str, strsize, "");
	return str;
}

BOOL winpr_RemoveDirectory_RecursiveA(LPCSTR lpPathName)
{
	BOOL ret = FALSE;

	if (!lpPathName)
		return FALSE;

	const size_t pathnamelen = strlen(lpPathName);
	const size_t path_slash_len = pathnamelen + 3;
	char* path_slash = calloc(pathnamelen + 4, sizeof(char));
	if (!path_slash)
		return FALSE;
	strncat(path_slash, lpPathName, pathnamelen);

	const char star[] = "*";
	const HRESULT hr = NativePathCchAppendA(path_slash, path_slash_len, star);
	HANDLE dir = INVALID_HANDLE_VALUE;
	if (FAILED(hr))
		goto fail;

	{
		WIN32_FIND_DATAA findFileData = { 0 };
		dir = FindFirstFileA(path_slash, &findFileData);

		if (dir == INVALID_HANDLE_VALUE)
			goto fail;

		ret = TRUE;
		path_slash[path_slash_len - 1] = '\0'; /* remove trailing '*' */
		do
		{
			const size_t len = strnlen(findFileData.cFileName, ARRAYSIZE(findFileData.cFileName));

			if ((len == 1 && findFileData.cFileName[0] == '.') ||
			    (len == 2 && findFileData.cFileName[0] == '.' && findFileData.cFileName[1] == '.'))
			{
				continue;
			}

			char* fullpath = concat(path_slash, path_slash_len, findFileData.cFileName, len);
			if (!fullpath)
				goto fail;

			if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				ret = winpr_RemoveDirectory_RecursiveA(fullpath);
			else
			{
				WINPR_PRAGMA_DIAG_PUSH
				WINPR_PRAGMA_DIAG_IGNORED_DEPRECATED_DECL
				ret = DeleteFileA(fullpath);
				WINPR_PRAGMA_DIAG_POP
			}

			free(fullpath);

			if (!ret)
				break;
		} while (ret && FindNextFileA(dir, &findFileData) != 0);
	}

	if (ret)
	{
		WINPR_PRAGMA_DIAG_PUSH
		WINPR_PRAGMA_DIAG_IGNORED_DEPRECATED_DECL
		if (!RemoveDirectoryA(lpPathName))
			ret = FALSE;
		WINPR_PRAGMA_DIAG_POP
	}

fail:
	FindClose(dir);
	free(path_slash);
	return ret;
}

BOOL winpr_RemoveDirectory_RecursiveW(LPCWSTR lpPathName)
{
	char* name = ConvertWCharToUtf8Alloc(lpPathName, NULL);
	if (!name)
		return FALSE;
	const BOOL rc = winpr_RemoveDirectory_RecursiveA(name);
	free(name);
	return rc;
}

char* winpr_GetConfigFilePath(BOOL system, const char* filename)
{
	eKnownPathTypes id = system ? KNOWN_PATH_SYSTEM_CONFIG_HOME : KNOWN_PATH_XDG_CONFIG_HOME;

#if defined(WINPR_USE_VENDOR_PRODUCT_CONFIG_DIR)
	char* vendor = GetKnownSubPath(id, WINPR_VENDOR_STRING);
	if (!vendor)
		return NULL;
#if defined(WITH_RESOURCE_VERSIONING)
	const char* prod = WINPR_PRODUCT_STRING STR(WINPR_VERSION_MAJOR);
#else
	const char* prod = WINPR_PRODUCT_STRING;
#endif
	char* base = GetCombinedPath(vendor, prod);
	free(vendor);
#else
	char* base = GetKnownSubPath(id, "winpr");
#endif

	if (!base)
		return NULL;
	if (!filename)
		return base;

	char* path = GetCombinedPath(base, filename);
	free(base);

	return path;
}
