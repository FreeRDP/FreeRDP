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

#define DEFINE_UNICODE			FALSE
#define PATH_SEPARATOR			'\\'
#define PATH_CCH_ADD_SEPARATOR		PathCchAddBackslashA
#include "include/PathCchAddSeparator.c"
#undef DEFINE_UNICODE
#undef PATH_SEPARATOR
#undef PATH_CCH_ADD_SEPARATOR

#define DEFINE_UNICODE			TRUE
#define PATH_SEPARATOR			'\\'
#define PATH_CCH_ADD_SEPARATOR		PathCchAddBackslashW
#include "include/PathCchAddSeparator.c"
#undef DEFINE_UNICODE
#undef PATH_SEPARATOR
#undef PATH_CCH_ADD_SEPARATOR

HRESULT PathCchRemoveBackslashA(PSTR pszPath, size_t cchPath)
{
	return 0;
}

HRESULT PathCchRemoveBackslashW(PWSTR pszPath, size_t cchPath)
{
	return 0;
}

#define DEFINE_UNICODE			FALSE
#define PATH_SEPARATOR			'\\'
#define PATH_CCH_ADD_SEPARATOR_EX	PathCchAddBackslashExA
#include "include/PathCchAddSeparatorEx.c"
#undef DEFINE_UNICODE
#undef PATH_SEPARATOR
#undef PATH_CCH_ADD_SEPARATOR_EX

#define DEFINE_UNICODE			TRUE
#define PATH_SEPARATOR			'\\'
#define PATH_CCH_ADD_SEPARATOR_EX	PathCchAddBackslashExW
#include "include/PathCchAddSeparatorEx.c"
#undef DEFINE_UNICODE
#undef PATH_SEPARATOR
#undef PATH_CCH_ADD_SEPARATOR_EX

HRESULT PathCchRemoveBackslashExA(PSTR pszPath, size_t cchPath, PSTR* ppszEnd, size_t* pcchRemaining)
{
	return 0;
}

HRESULT PathCchRemoveBackslashExW(PWSTR pszPath, size_t cchPath, PWSTR* ppszEnd, size_t* pcchRemaining)
{
	return 0;
}

HRESULT PathCchAddExtensionA(PSTR pszPath, size_t cchPath, PCSTR pszExt)
{
	CHAR* pDot;
	BOOL bExtDot;
	CHAR* pBackslash;
	size_t pszExtLength;
	size_t pszPathLength;

	if (!pszPath)
		return S_FALSE;

	if (!pszExt)
		return S_FALSE;

	pszExtLength = lstrlenA(pszExt);
	pszPathLength = lstrlenA(pszPath);
	bExtDot = (pszExt[0] == '.') ? TRUE : FALSE;

	pDot = strrchr(pszPath, '.');
	pBackslash = strrchr(pszPath, '\\');

	if (pDot && pBackslash)
	{
		if (pDot > pBackslash)
			return S_FALSE;
	}

	if (cchPath > pszPathLength + pszExtLength + ((bExtDot) ? 0 : 1))
	{
		if (bExtDot)
			sprintf_s(&pszPath[pszPathLength], cchPath - pszPathLength, "%s", pszExt);
		else
			sprintf_s(&pszPath[pszPathLength], cchPath - pszPathLength, ".%s", pszExt);

		return S_OK;
	}

	return S_FALSE;
}

HRESULT PathCchAddExtensionW(PWSTR pszPath, size_t cchPath, PCWSTR pszExt)
{
#ifdef _WIN32
	LPTCH pDot;
	BOOL bExtDot;
	LPTCH pBackslash;
	size_t pszExtLength;
	size_t pszPathLength;

	if (!pszPath)
		return S_FALSE;

	if (!pszExt)
		return S_FALSE;

	pszExtLength = lstrlenW(pszExt);
	pszPathLength = lstrlenW(pszPath);
	bExtDot = (pszExt[0] == '.') ? TRUE : FALSE;

	pDot = wcsrchr(pszPath, '.');
	pBackslash = wcsrchr(pszPath, '\\');

	if (pDot && pBackslash)
	{
		if (pDot > pBackslash)
			return S_FALSE;
	}

	if (cchPath > pszPathLength + pszExtLength + ((bExtDot) ? 0 : 1))
	{
		if (bExtDot)
			swprintf_s(&pszPath[pszPathLength], cchPath - pszPathLength, L"%s", pszExt);
		else
			swprintf_s(&pszPath[pszPathLength], cchPath - pszPathLength, L".%s", pszExt);

		return S_OK;
	}
#endif
	return S_FALSE;
}

HRESULT PathCchAppendA(PSTR pszPath, size_t cchPath, PCSTR pszMore)
{
	BOOL pathBackslash;
	BOOL moreBackslash;
	size_t pszMoreLength;
	size_t pszPathLength;

	if (!pszPath)
		return S_FALSE;

	if (!pszMore)
		return S_FALSE;

	pszMoreLength = lstrlenA(pszMore);
	pszPathLength = lstrlenA(pszPath);

	pathBackslash = (pszPath[pszPathLength - 1] == '\\') ? TRUE : FALSE;
	moreBackslash = (pszMore[0] == '\\') ? TRUE : FALSE;

	if (pathBackslash && moreBackslash)
	{
		if ((pszPathLength + pszMoreLength - 1) < cchPath)
		{
			sprintf_s(&pszPath[pszPathLength], cchPath - pszPathLength, "%s", &pszMore[1]);
			return S_OK;
		}
	}
	else if ((pathBackslash && !moreBackslash) || (!pathBackslash && moreBackslash))
	{
		if ((pszPathLength + pszMoreLength) < cchPath)
		{
			sprintf_s(&pszPath[pszPathLength], cchPath - pszPathLength, "%s", pszMore);
			return S_OK;
		}
	}
	else if (!pathBackslash && !moreBackslash)
	{
		if ((pszPathLength + pszMoreLength + 1) < cchPath)
		{
			sprintf_s(&pszPath[pszPathLength], cchPath - pszPathLength, "\\%s", pszMore);
			return S_OK;
		}
	}

	return S_FALSE;
}

HRESULT PathCchAppendW(PWSTR pszPath, size_t cchPath, PCWSTR pszMore)
{
#ifdef _WIN32
	BOOL pathBackslash;
	BOOL moreBackslash;
	size_t pszMoreLength;
	size_t pszPathLength;

	if (!pszPath)
		return S_FALSE;

	if (!pszMore)
		return S_FALSE;

	pszMoreLength = lstrlenW(pszMore);
	pszPathLength = lstrlenW(pszPath);

	pathBackslash = (pszPath[pszPathLength - 1] == '\\') ? TRUE : FALSE;
	moreBackslash = (pszMore[0] == '\\') ? TRUE : FALSE;

	if (pathBackslash && moreBackslash)
	{
		if ((pszPathLength + pszMoreLength - 1) < cchPath)
		{
			swprintf_s(&pszPath[pszPathLength], cchPath - pszPathLength, L"%s", &pszMore[1]);
			return S_OK;
		}
	}
	else if ((pathBackslash && !moreBackslash) || (!pathBackslash && moreBackslash))
	{
		if ((pszPathLength + pszMoreLength) < cchPath)
		{
			swprintf_s(&pszPath[pszPathLength], cchPath - pszPathLength, L"%s", pszMore);
			return S_OK;
		}
	}
	else if (!pathBackslash && !moreBackslash)
	{
		if ((pszPathLength + pszMoreLength + 1) < cchPath)
		{
			swprintf_s(&pszPath[pszPathLength], cchPath - pszPathLength, L"\\%s", pszMore);
			return S_OK;
		}
	}
#endif

	return S_FALSE;
}

HRESULT PathCchAppendExA(PSTR pszPath, size_t cchPath, PCSTR pszMore, unsigned long dwFlags)
{
	return 0;
}

HRESULT PathCchAppendExW(PWSTR pszPath, size_t cchPath, PCWSTR pszMore, unsigned long dwFlags)
{
	return 0;
}

HRESULT PathCchCanonicalizeA(PSTR pszPathOut, size_t cchPathOut, PCSTR pszPathIn)
{
	return 0;
}

HRESULT PathCchCanonicalizeW(PWSTR pszPathOut, size_t cchPathOut, PCWSTR pszPathIn)
{
	return 0;
}

HRESULT PathCchCanonicalizeExA(PSTR pszPathOut, size_t cchPathOut, PCSTR pszPathIn, unsigned long dwFlags)
{
	return 0;
}

HRESULT PathCchCanonicalizeExW(PWSTR pszPathOut, size_t cchPathOut, PCWSTR pszPathIn, unsigned long dwFlags)
{
	return 0;
}

HRESULT PathAllocCanonicalizeA(PCSTR pszPathIn, unsigned long dwFlags, PSTR* ppszPathOut)
{
	return 0;
}

HRESULT PathAllocCanonicalizeW(PCWSTR pszPathIn, unsigned long dwFlags, PWSTR* ppszPathOut)
{
	return 0;
}

HRESULT PathCchCombineA(PSTR pszPathOut, size_t cchPathOut, PCSTR pszPathIn, PCSTR pszMore)
{
	return 0;
}

HRESULT PathCchCombineW(PWSTR pszPathOut, size_t cchPathOut, PCWSTR pszPathIn, PCWSTR pszMore)
{
	return 0;
}

HRESULT PathCchCombineExA(PSTR pszPathOut, size_t cchPathOut, PCSTR pszPathIn, PCSTR pszMore, unsigned long dwFlags)
{
	return 0;
}

HRESULT PathCchCombineExW(PWSTR pszPathOut, size_t cchPathOut, PCWSTR pszPathIn, PCWSTR pszMore, unsigned long dwFlags)
{
	return 0;
}

HRESULT PathAllocCombineA(PCSTR pszPathIn, PCSTR pszMore, unsigned long dwFlags, PSTR* ppszPathOut)
{
	PSTR pszPathOut;
	BOOL backslashIn;
	BOOL backslashMore;
	int pszMoreLength;
	int pszPathInLength;
	int pszPathOutLength;

	if (!pszPathIn)
		return S_FALSE;

	if (!pszMore)
		return S_FALSE;

	pszPathInLength = lstrlenA(pszPathIn);
	pszMoreLength = lstrlenA(pszMore);

	backslashIn = (pszPathIn[pszPathInLength - 1] == '\\') ? TRUE : FALSE;
	backslashMore = (pszMore[0] == '\\') ? TRUE : FALSE;

	if (backslashMore)
	{
		if ((pszPathIn[1] == ':') && (pszPathIn[2] == '\\'))
		{
			size_t sizeOfBuffer;

			pszPathOutLength = 2 + pszMoreLength;
			sizeOfBuffer = (pszPathOutLength + 1) * 2;

			pszPathOut = (PSTR) HeapAlloc(GetProcessHeap(), 0, sizeOfBuffer * 2);
			sprintf_s(pszPathOut, sizeOfBuffer, "%c:%s", pszPathIn[0], pszMore);

			*ppszPathOut = pszPathOut;

			return S_OK;
		}
	}
	else
	{
		size_t sizeOfBuffer;

		pszPathOutLength = pszPathInLength + pszMoreLength;
		sizeOfBuffer = (pszPathOutLength + 1) * 2;

		pszPathOut = (PSTR) HeapAlloc(GetProcessHeap(), 0, sizeOfBuffer * 2);

		if (backslashIn)
			sprintf_s(pszPathOut, sizeOfBuffer, "%s%s", pszPathIn, pszMore);
		else
			sprintf_s(pszPathOut, sizeOfBuffer, "%s\\%s", pszPathIn, pszMore);

		*ppszPathOut = pszPathOut;

		return S_OK;
	}

	return S_OK;
}

HRESULT PathAllocCombineW(PCWSTR pszPathIn, PCWSTR pszMore, unsigned long dwFlags, PWSTR* ppszPathOut)
{
#ifdef _WIN32
	PWSTR pszPathOut;
	BOOL backslashIn;
	BOOL backslashMore;
	int pszMoreLength;
	int pszPathInLength;
	int pszPathOutLength;

	if (!pszPathIn)
		return S_FALSE;

	if (!pszMore)
		return S_FALSE;

	pszPathInLength = lstrlenW(pszPathIn);
	pszMoreLength = lstrlenW(pszMore);

	backslashIn = (pszPathIn[pszPathInLength - 1] == '\\') ? TRUE : FALSE;
	backslashMore = (pszMore[0] == '\\') ? TRUE : FALSE;

	if (backslashMore)
	{
		if ((pszPathIn[1] == ':') && (pszPathIn[2] == '\\'))
		{
			size_t sizeOfBuffer;

			pszPathOutLength = 2 + pszMoreLength;
			sizeOfBuffer = (pszPathOutLength + 1) * 2;

			pszPathOut = (PWSTR) HeapAlloc(GetProcessHeap(), 0, sizeOfBuffer * 2);
			swprintf_s(pszPathOut, sizeOfBuffer, L"%c:%s", pszPathIn[0], pszMore);

			*ppszPathOut = pszPathOut;

			return S_OK;
		}
	}
	else
	{
		size_t sizeOfBuffer;

		pszPathOutLength = pszPathInLength + pszMoreLength;
		sizeOfBuffer = (pszPathOutLength + 1) * 2;

		pszPathOut = (PWSTR) HeapAlloc(GetProcessHeap(), 0, sizeOfBuffer * 2);

		if (backslashIn)
			swprintf_s(pszPathOut, sizeOfBuffer, L"%s%s", pszPathIn, pszMore);
		else
			swprintf_s(pszPathOut, sizeOfBuffer, L"%s\\%s", pszPathIn, pszMore);

		*ppszPathOut = pszPathOut;

		return S_OK;
	}
#endif

	return S_OK;
}

HRESULT PathCchFindExtensionA(PCSTR pszPath, size_t cchPath, PCSTR* ppszExt)
{
	return 0;
}

HRESULT PathCchFindExtensionW(PCWSTR pszPath, size_t cchPath, PCWSTR* ppszExt)
{
	return 0;
}

HRESULT PathCchRenameExtensionA(PSTR pszPath, size_t cchPath, PCSTR pszExt)
{
	return 0;
}

HRESULT PathCchRenameExtensionW(PWSTR pszPath, size_t cchPath, PCWSTR pszExt)
{
	return 0;
}

HRESULT PathCchRemoveExtensionA(PSTR pszPath, size_t cchPath)
{
	return 0;
}

HRESULT PathCchRemoveExtensionW(PWSTR pszPath, size_t cchPath)
{
	return 0;
}

BOOL PathCchIsRootA(PCSTR pszPath)
{
	return 0;
}

BOOL PathCchIsRootW(PCWSTR pszPath)
{
	return 0;
}

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

HRESULT PathCchSkipRootA(PCSTR pszPath, PCSTR* ppszRootEnd)
{
	return 0;
}

HRESULT PathCchSkipRootW(PCWSTR pszPath, PCWSTR* ppszRootEnd)
{
	return 0;
}

HRESULT PathCchStripToRootA(PSTR pszPath, size_t cchPath)
{
	return 0;
}

HRESULT PathCchStripToRootW(PWSTR pszPath, size_t cchPath)
{
	return 0;
}

HRESULT PathCchStripPrefixA(PSTR pszPath, size_t cchPath)
{
	BOOL hasPrefix;
	BOOL deviceNamespace;

	if (!pszPath)
		return S_FALSE;

	if (cchPath < 4)
		return S_FALSE;

	hasPrefix = ((pszPath[0] == '\\') && (pszPath[1] == '\\') &&
		(pszPath[2] == '?') && (pszPath[3] == '\\')) ? TRUE : FALSE;

	if (hasPrefix)
	{
		if (cchPath < 7)
			return S_FALSE;

		deviceNamespace = ((pszPath[5] == ':') && (pszPath[6] == '\\')) ? TRUE : FALSE;

		if (deviceNamespace)
		{
			memmove_s(pszPath, cchPath, &pszPath[4], cchPath - 4);
			return S_OK;
		}
	}

	return S_FALSE;
}

HRESULT PathCchStripPrefixW(PWSTR pszPath, size_t cchPath)
{
	BOOL hasPrefix;
	BOOL deviceNamespace;

	if (!pszPath)
		return S_FALSE;

	if (cchPath < 4)
		return S_FALSE;

	hasPrefix = ((pszPath[0] == '\\') && (pszPath[1] == '\\') &&
		(pszPath[2] == '?') && (pszPath[3] == '\\')) ? TRUE : FALSE;

	if (hasPrefix)
	{
		if (cchPath < 7)
			return S_FALSE;

		deviceNamespace = ((pszPath[5] == ':') && (pszPath[6] == '\\')) ? TRUE : FALSE;

		if (deviceNamespace)
		{
			wmemmove_s(pszPath, cchPath, &pszPath[4], cchPath - 4);
			return S_OK;
		}
	}

	return S_FALSE;
}

HRESULT PathCchRemoveFileSpecA(PSTR pszPath, size_t cchPath)
{
	return 0;
}

HRESULT PathCchRemoveFileSpecW(PWSTR pszPath, size_t cchPath)
{
	return 0;
}

