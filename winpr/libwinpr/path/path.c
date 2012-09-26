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

#include <winpr/path.h>

HRESULT PathCchAddBackslashA(PSTR pszPath, size_t cchPath)
{
	return 0;
}

HRESULT PathCchAddBackslashW(PWSTR pszPath, size_t cchPath)
{
	return 0;
}

HRESULT PathCchRemoveBackslashA(PSTR pszPath, size_t cchPath)
{
	return 0;
}

HRESULT PathCchRemoveBackslashW(PWSTR pszPath, size_t cchPath)
{
	return 0;
}

HRESULT PathCchAddBackslashExA(PSTR pszPath, size_t cchPath, PSTR* ppszEnd, size_t* pcchRemaining)
{
	return 0;
}

HRESULT PathCchAddBackslashExW(PWSTR pszPath, size_t cchPath, PWSTR* ppszEnd, size_t* pcchRemaining)
{
	return 0;
}

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
	return 0;
}

HRESULT PathCchAddExtensionW(PWSTR pszPath, size_t cchPath, PCWSTR pszExt)
{
	return 0;
}

HRESULT PathCchAppendA(PSTR pszPath, size_t cchPath, PCSTR pszMore)
{
	return 0;
}

HRESULT PathCchAppendW(PWSTR pszPath, size_t cchPath, PCWSTR pszMore)
{
	return 0;
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
	return 0;
}

HRESULT PathAllocCombineW(PCWSTR pszPathIn, PCWSTR pszMore, unsigned long dwFlags, PWSTR* ppszPathOut)
{
	return 0;
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
	return 0;
}

BOOL PathIsUNCExW(PCWSTR pszPath, PCWSTR* ppszServer)
{
	return 0;
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
	return 0;
}

HRESULT PathCchStripPrefixW(PWSTR pszPath, size_t cchPath)
{
	return 0;
}

HRESULT PathCchRemoveFileSpecA(PSTR pszPath, size_t cchPath)
{
	return 0;
}

HRESULT PathCchRemoveFileSpecW(PWSTR pszPath, size_t cchPath)
{
	return 0;
}

