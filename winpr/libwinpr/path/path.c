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

#include <winpr/crt.h>
#include <winpr/heap.h>
#include <winpr/tchar.h>

#include <winpr/path.h>

#define PATH_SLASH_CHR			'/'
#define PATH_SLASH_STR			"/"

#define PATH_BACKSLASH_CHR		'\\'
#define PATH_BACKSLASH_STR		"\\"

#ifdef _WIN32
#define PATH_SLASH_STR_W		L"/"
#define PATH_BACKSLASH_STR_W		L"\\"
#else
#define PATH_SLASH_STR_W		"/"
#define PATH_BACKSLASH_STR_W		"\\"
#endif

#ifdef _WIN32
#define PATH_SEPARATOR_CHR		PATH_BACKSLASH_CHR
#define PATH_SEPARATOR_STR		PATH_BACKSLASH_STR
#define PATH_SEPARATOR_STR_W		PATH_BACKSLASH_STR_W
#else
#define PATH_SEPARATOR_CHR		PATH_SLASH_CHR
#define PATH_SEPARATOR_STR		PATH_SLASH_STR
#define PATH_SEPARATOR_STR_W		PATH_SLASH_STR_W
#endif

#define SHARED_LIBRARY_EXT_DLL		"dll"
#define SHARED_LIBRARY_EXT_SO		"so"
#define SHARED_LIBRARY_EXT_DYLIB	"dylib"

#ifdef _WIN32
#define SHARED_LIBRARY_EXT		SHARED_LIBRARY_EXT_DLL
#elif defined(__APPLE__)
#define SHARED_LIBRARY_EXT		SHARED_LIBRARY_EXT_DYLIB
#else
#define SHARED_LIBRARY_EXT		SHARED_LIBRARY_EXT_SO
#endif

#include "../log.h"
#define TAG WINPR_TAG("path")

/*
 * PathCchAddBackslash
 */

/* Windows-style Paths */

#define DEFINE_UNICODE			FALSE
#define _PATH_SEPARATOR_CHR		PATH_BACKSLASH_CHR
#define PATH_CCH_ADD_SEPARATOR		PathCchAddBackslashA
#include "include/PathCchAddSeparator.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR

#define DEFINE_UNICODE			TRUE
#define _PATH_SEPARATOR_CHR		PATH_BACKSLASH_CHR
#define PATH_CCH_ADD_SEPARATOR		PathCchAddBackslashW
#include "include/PathCchAddSeparator.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR

/* Unix-style Paths */

#define DEFINE_UNICODE			FALSE
#define _PATH_SEPARATOR_CHR		PATH_SLASH_CHR
#define PATH_CCH_ADD_SEPARATOR		PathCchAddSlashA
#include "include/PathCchAddSeparator.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR

#define DEFINE_UNICODE			TRUE
#define _PATH_SEPARATOR_CHR		PATH_SLASH_CHR
#define PATH_CCH_ADD_SEPARATOR		PathCchAddSlashW
#include "include/PathCchAddSeparator.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR

/* Native-style Paths */

#define DEFINE_UNICODE			FALSE
#define _PATH_SEPARATOR_CHR		PATH_SEPARATOR_CHR
#define PATH_CCH_ADD_SEPARATOR		PathCchAddSeparatorA
#include "include/PathCchAddSeparator.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR

#define DEFINE_UNICODE			TRUE
#define _PATH_SEPARATOR_CHR		PATH_SEPARATOR_CHR
#define PATH_CCH_ADD_SEPARATOR		PathCchAddSeparatorW
#include "include/PathCchAddSeparator.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR

/*
 * PathCchRemoveBackslash
 */

HRESULT PathCchRemoveBackslashA(PSTR pszPath, size_t cchPath)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

HRESULT PathCchRemoveBackslashW(PWSTR pszPath, size_t cchPath)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

/*
 * PathCchAddBackslashEx
 */

/* Windows-style Paths */

#define DEFINE_UNICODE			FALSE
#define _PATH_SEPARATOR_CHR		PATH_BACKSLASH_CHR
#define PATH_CCH_ADD_SEPARATOR_EX	PathCchAddBackslashExA
#include "include/PathCchAddSeparatorEx.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR_EX

#define DEFINE_UNICODE			TRUE
#define _PATH_SEPARATOR_CHR		PATH_BACKSLASH_CHR
#define PATH_CCH_ADD_SEPARATOR_EX	PathCchAddBackslashExW
#include "include/PathCchAddSeparatorEx.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR_EX

/* Unix-style Paths */

#define DEFINE_UNICODE			FALSE
#define _PATH_SEPARATOR_CHR		PATH_SLASH_CHR
#define PATH_CCH_ADD_SEPARATOR_EX	PathCchAddSlashExA
#include "include/PathCchAddSeparatorEx.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR_EX

#define DEFINE_UNICODE			TRUE
#define _PATH_SEPARATOR_CHR		PATH_SLASH_CHR
#define PATH_CCH_ADD_SEPARATOR_EX	PathCchAddSlashExW
#include "include/PathCchAddSeparatorEx.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR_EX

/* Native-style Paths */

#define DEFINE_UNICODE			FALSE
#define _PATH_SEPARATOR_CHR		PATH_SEPARATOR_CHR
#define PATH_CCH_ADD_SEPARATOR_EX	PathCchAddSeparatorExA
#include "include/PathCchAddSeparatorEx.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR_EX

#define DEFINE_UNICODE			TRUE
#define _PATH_SEPARATOR_CHR		PATH_SEPARATOR_CHR
#define PATH_CCH_ADD_SEPARATOR_EX	PathCchAddSeparatorExW
#include "include/PathCchAddSeparatorEx.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR_EX

HRESULT PathCchRemoveBackslashExA(PSTR pszPath, size_t cchPath, PSTR* ppszEnd, size_t* pcchRemaining)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

HRESULT PathCchRemoveBackslashExW(PWSTR pszPath, size_t cchPath, PWSTR* ppszEnd, size_t* pcchRemaining)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

/*
 * PathCchAddExtension
 */

/* Windows-style Paths */

#define DEFINE_UNICODE			FALSE
#define _PATH_SEPARATOR_CHR		PATH_BACKSLASH_CHR
#define PATH_CCH_ADD_EXTENSION		PathCchAddExtensionA
#include "include/PathCchAddExtension.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_EXTENSION

#define DEFINE_UNICODE			TRUE
#define _PATH_SEPARATOR_CHR		PATH_BACKSLASH_CHR
#define PATH_CCH_ADD_EXTENSION		PathCchAddExtensionW
#include "include/PathCchAddExtension.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_EXTENSION

/* Unix-style Paths */

#define DEFINE_UNICODE			FALSE
#define _PATH_SEPARATOR_CHR		PATH_SLASH_CHR
#define PATH_CCH_ADD_EXTENSION		UnixPathCchAddExtensionA
#include "include/PathCchAddExtension.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_EXTENSION

#define DEFINE_UNICODE			TRUE
#define _PATH_SEPARATOR_CHR		PATH_SLASH_CHR
#define PATH_CCH_ADD_EXTENSION		UnixPathCchAddExtensionW
#include "include/PathCchAddExtension.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_EXTENSION

/* Native-style Paths */

#define DEFINE_UNICODE			FALSE
#define _PATH_SEPARATOR_CHR		PATH_SEPARATOR_CHR
#define PATH_CCH_ADD_EXTENSION		NativePathCchAddExtensionA
#include "include/PathCchAddExtension.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_EXTENSION

#define DEFINE_UNICODE			TRUE
#define _PATH_SEPARATOR_CHR		PATH_SEPARATOR_CHR
#define PATH_CCH_ADD_EXTENSION		NativePathCchAddExtensionW
#include "include/PathCchAddExtension.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_EXTENSION

/*
 * PathCchAppend
 */

/* Windows-style Paths */

#define DEFINE_UNICODE		FALSE
#define _PATH_SEPARATOR_CHR	PATH_BACKSLASH_CHR
#define _PATH_SEPARATOR_STR	PATH_BACKSLASH_STR
#define PATH_CCH_APPEND		PathCchAppendA
#include "include/PathCchAppend.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef _PATH_SEPARATOR_STR
#undef PATH_CCH_APPEND

#define DEFINE_UNICODE		TRUE
#define _PATH_SEPARATOR_CHR	PATH_BACKSLASH_CHR
#define _PATH_SEPARATOR_STR	PATH_BACKSLASH_STR_W
#define PATH_CCH_APPEND		PathCchAppendW
#include "include/PathCchAppend.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef _PATH_SEPARATOR_STR
#undef PATH_CCH_APPEND

/* Unix-style Paths */

#define DEFINE_UNICODE		FALSE
#define _PATH_SEPARATOR_CHR	PATH_SLASH_CHR
#define _PATH_SEPARATOR_STR	PATH_SLASH_STR
#define PATH_CCH_APPEND		UnixPathCchAppendA
#include "include/PathCchAppend.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef _PATH_SEPARATOR_STR
#undef PATH_CCH_APPEND

#define DEFINE_UNICODE		TRUE
#define _PATH_SEPARATOR_CHR	PATH_SLASH_CHR
#define _PATH_SEPARATOR_STR	PATH_SLASH_STR_W
#define PATH_CCH_APPEND		UnixPathCchAppendW
#include "include/PathCchAppend.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef _PATH_SEPARATOR_STR
#undef PATH_CCH_APPEND

/* Native-style Paths */

#define DEFINE_UNICODE		FALSE
#define _PATH_SEPARATOR_CHR	PATH_SEPARATOR_CHR
#define _PATH_SEPARATOR_STR	PATH_SEPARATOR_STR
#define PATH_CCH_APPEND		NativePathCchAppendA
#include "include/PathCchAppend.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef _PATH_SEPARATOR_STR
#undef PATH_CCH_APPEND

#define DEFINE_UNICODE		TRUE
#define _PATH_SEPARATOR_CHR	PATH_SEPARATOR_CHR
#define _PATH_SEPARATOR_STR	PATH_SEPARATOR_STR_W
#define PATH_CCH_APPEND		NativePathCchAppendW
#include "include/PathCchAppend.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef _PATH_SEPARATOR_STR
#undef PATH_CCH_APPEND

/*
 * PathCchAppendEx
 */

HRESULT PathCchAppendExA(PSTR pszPath, size_t cchPath, PCSTR pszMore, unsigned long dwFlags)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

HRESULT PathCchAppendExW(PWSTR pszPath, size_t cchPath, PCWSTR pszMore, unsigned long dwFlags)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

/*
 * PathCchCanonicalize
 */

HRESULT PathCchCanonicalizeA(PSTR pszPathOut, size_t cchPathOut, PCSTR pszPathIn)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

HRESULT PathCchCanonicalizeW(PWSTR pszPathOut, size_t cchPathOut, PCWSTR pszPathIn)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

/*
 * PathCchCanonicalizeEx
 */

HRESULT PathCchCanonicalizeExA(PSTR pszPathOut, size_t cchPathOut, PCSTR pszPathIn, unsigned long dwFlags)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

HRESULT PathCchCanonicalizeExW(PWSTR pszPathOut, size_t cchPathOut, PCWSTR pszPathIn, unsigned long dwFlags)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

/*
 * PathAllocCanonicalize
 */

HRESULT PathAllocCanonicalizeA(PCSTR pszPathIn, unsigned long dwFlags, PSTR* ppszPathOut)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

HRESULT PathAllocCanonicalizeW(PCWSTR pszPathIn, unsigned long dwFlags, PWSTR* ppszPathOut)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

/*
 * PathCchCombine
 */

HRESULT PathCchCombineA(PSTR pszPathOut, size_t cchPathOut, PCSTR pszPathIn, PCSTR pszMore)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

HRESULT PathCchCombineW(PWSTR pszPathOut, size_t cchPathOut, PCWSTR pszPathIn, PCWSTR pszMore)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

/*
 * PathCchCombineEx
 */

HRESULT PathCchCombineExA(PSTR pszPathOut, size_t cchPathOut, PCSTR pszPathIn, PCSTR pszMore, unsigned long dwFlags)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

HRESULT PathCchCombineExW(PWSTR pszPathOut, size_t cchPathOut, PCWSTR pszPathIn, PCWSTR pszMore, unsigned long dwFlags)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

/*
 * PathAllocCombine
 */

/* Windows-style Paths */

#define DEFINE_UNICODE		FALSE
#define _PATH_SEPARATOR_CHR	PATH_BACKSLASH_CHR
#define _PATH_SEPARATOR_STR	PATH_BACKSLASH_STR
#define PATH_ALLOC_COMBINE	PathAllocCombineA
#include "include/PathAllocCombine.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef _PATH_SEPARATOR_STR
#undef PATH_ALLOC_COMBINE

#define DEFINE_UNICODE		TRUE
#define _PATH_SEPARATOR_CHR	PATH_BACKSLASH_CHR
#define _PATH_SEPARATOR_STR	PATH_BACKSLASH_STR_W
#define PATH_ALLOC_COMBINE	PathAllocCombineW
#include "include/PathAllocCombine.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef _PATH_SEPARATOR_STR
#undef PATH_ALLOC_COMBINE

/* Unix-style Paths */

#define DEFINE_UNICODE		FALSE
#define _PATH_SEPARATOR_CHR	PATH_SLASH_CHR
#define _PATH_SEPARATOR_STR	PATH_SLASH_STR
#define PATH_ALLOC_COMBINE	UnixPathAllocCombineA
#include "include/PathAllocCombine.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef _PATH_SEPARATOR_STR
#undef PATH_ALLOC_COMBINE

#define DEFINE_UNICODE		TRUE
#define _PATH_SEPARATOR_CHR	PATH_SLASH_CHR
#define _PATH_SEPARATOR_STR	PATH_SLASH_STR_W
#define PATH_ALLOC_COMBINE	UnixPathAllocCombineW
#include "include/PathAllocCombine.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef _PATH_SEPARATOR_STR
#undef PATH_ALLOC_COMBINE

/* Native-style Paths */

#define DEFINE_UNICODE		FALSE
#define _PATH_SEPARATOR_CHR	PATH_SEPARATOR_CHR
#define _PATH_SEPARATOR_STR	PATH_SEPARATOR_STR
#define PATH_ALLOC_COMBINE	NativePathAllocCombineA
#include "include/PathAllocCombine.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef _PATH_SEPARATOR_STR
#undef PATH_ALLOC_COMBINE

#define DEFINE_UNICODE		TRUE
#define _PATH_SEPARATOR_CHR	PATH_SEPARATOR_CHR
#define _PATH_SEPARATOR_STR	PATH_SEPARATOR_STR_W
#define PATH_ALLOC_COMBINE	NativePathAllocCombineW
#include "include/PathAllocCombine.c"
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef _PATH_SEPARATOR_STR
#undef PATH_ALLOC_COMBINE

/**
 * PathCchFindExtension
 */

HRESULT PathCchFindExtensionA(PCSTR pszPath, size_t cchPath, PCSTR* ppszExt)
{
	char* p = (char*) pszPath;

	if (!pszPath || !cchPath || !ppszExt)
		return E_INVALIDARG;

	/* find end of string */

	while (*p && cchPath)
	{
		cchPath--;
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
			*ppszExt = (PCSTR) p;
			break;
		}

		if ((*p == '\\') || (*p == '/') || (*p == ':'))
			break;

		p--;
	}

	return S_OK;
}

HRESULT PathCchFindExtensionW(PCWSTR pszPath, size_t cchPath, PCWSTR* ppszExt)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

/**
 * PathCchRenameExtension
 */

HRESULT PathCchRenameExtensionA(PSTR pszPath, size_t cchPath, PCSTR pszExt)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

HRESULT PathCchRenameExtensionW(PWSTR pszPath, size_t cchPath, PCWSTR pszExt)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

/**
 * PathCchRemoveExtension
 */

HRESULT PathCchRemoveExtensionA(PSTR pszPath, size_t cchPath)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

HRESULT PathCchRemoveExtensionW(PWSTR pszPath, size_t cchPath)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

/**
 * PathCchIsRoot
 */

BOOL PathCchIsRootA(PCSTR pszPath)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return FALSE;
}

BOOL PathCchIsRootW(PCWSTR pszPath)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
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

HRESULT PathCchSkipRootA(PCSTR pszPath, PCSTR* ppszRootEnd)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

HRESULT PathCchSkipRootW(PCWSTR pszPath, PCWSTR* ppszRootEnd)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

/**
 * PathCchStripToRoot
 */

HRESULT PathCchStripToRootA(PSTR pszPath, size_t cchPath)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

HRESULT PathCchStripToRootW(PWSTR pszPath, size_t cchPath)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

/**
 * PathCchStripPrefix
 */

HRESULT PathCchStripPrefixA(PSTR pszPath, size_t cchPath)
{
	BOOL hasPrefix;

	if (!pszPath)
		return E_INVALIDARG;

	if (cchPath < 4 || cchPath > PATHCCH_MAX_CCH)
		return E_INVALIDARG;

	hasPrefix = ((pszPath[0] == '\\') && (pszPath[1] == '\\') &&
		(pszPath[2] == '?') && (pszPath[3] == '\\')) ? TRUE : FALSE;

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
	BOOL hasPrefix;

	if (!pszPath)
		return E_INVALIDARG;

	if (cchPath < 4 || cchPath > PATHCCH_MAX_CCH)
		return E_INVALIDARG;

	hasPrefix = ((pszPath[0] == '\\') && (pszPath[1] == '\\') &&
		(pszPath[2] == '?') && (pszPath[3] == '\\')) ? TRUE : FALSE;

	if (hasPrefix)
	{
		if (cchPath < 6)
			return S_FALSE;

		if (cchPath < (lstrlenW(&pszPath[4]) + 1))
			return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);

		if (IsCharAlpha(pszPath[4]) && (pszPath[5] == ':')) /* like C: */
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

HRESULT PathCchRemoveFileSpecA(PSTR pszPath, size_t cchPath)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return E_NOTIMPL;
}

HRESULT PathCchRemoveFileSpecW(PWSTR pszPath, size_t cchPath)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
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
	size_t index;

	if (dwFlags == PATH_STYLE_WINDOWS)
	{
		for (index = 0; index < cchPath; index++)
		{
			if (pszPath[index] == PATH_SLASH_CHR)
				pszPath[index] = PATH_BACKSLASH_CHR;
		}
	}
	else if (dwFlags == PATH_STYLE_UNIX)
	{
		for (index = 0; index < cchPath; index++)
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

			for (index = 0; index < cchPath; index++)
			{
				if (pszPath[index] == PATH_SLASH_CHR)
					pszPath[index] = PATH_BACKSLASH_CHR;
			}
		}
		else if (PATH_SEPARATOR_CHR == PATH_SLASH_CHR)
		{
			/* Windows-style to Unix-style */

			for (index = 0; index < cchPath; index++)
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
	size_t index;

	if (dwFlags == PATH_STYLE_WINDOWS)
	{
		for (index = 0; index < cchPath; index++)
		{
			if (pszPath[index] == PATH_SLASH_CHR)
				pszPath[index] = PATH_BACKSLASH_CHR;
		}
	}
	else if (dwFlags == PATH_STYLE_UNIX)
	{
		for (index = 0; index < cchPath; index++)
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

			for (index = 0; index < cchPath; index++)
			{
				if (pszPath[index] == PATH_SLASH_CHR)
					pszPath[index] = PATH_BACKSLASH_CHR;
			}
		}
		else if (PATH_SEPARATOR_CHR == PATH_SLASH_CHR)
		{
			/* Windows-style to Unix-style */

			for (index = 0; index < cchPath; index++)
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
	WCHAR separator = PATH_SEPARATOR_CHR;

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

/**
 * PathGetSharedLibraryExtension
 */

static const CHAR SharedLibraryExtensionDllA[] = "dll";
static const CHAR SharedLibraryExtensionSoA[] = "so";
static const CHAR SharedLibraryExtensionDylibA[] = "dylib";

static const WCHAR SharedLibraryExtensionDllW[] = { 'd','l','l','\0' };
static const WCHAR SharedLibraryExtensionSoW[] = { 's','o','\0' };
static const WCHAR SharedLibraryExtensionDylibW[] = { 'd','y','l','i','b','\0' };

static const CHAR SharedLibraryExtensionDotDllA[] = ".dll";
static const CHAR SharedLibraryExtensionDotSoA[] = ".so";
static const CHAR SharedLibraryExtensionDotDylibA[] = ".dylib";

static const WCHAR SharedLibraryExtensionDotDllW[] = { '.','d','l','l','\0' };
static const WCHAR SharedLibraryExtensionDotSoW[] = { '.','s','o','\0' };
static const WCHAR SharedLibraryExtensionDotDylibW[] = { '.','d','y','l','i','b','\0' };

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
