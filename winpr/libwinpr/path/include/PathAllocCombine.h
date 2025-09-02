
/*
#define DEFINE_UNICODE		FALSE
#define CUR_PATH_SEPARATOR_CHR	'\\'
#define CUR_PATH_SEPARATOR_STR	"\\"
#define PATH_ALLOC_COMBINE	PathAllocCombineA
*/

/**
 * FIXME: These implementations of the PathAllocCombine functions have
 * several issues:
 * - pszPathIn or pszMore may be NULL (but not both)
 * - no check if pszMore is fully qualified (if so, it must be directly
 *   copied to the output buffer without being combined with pszPathIn.
 * - if pszMore begins with a _single_ backslash it must be combined with
 *   only the root of the path pointed to by pszPathIn and there's no code
 *   to extract the root of pszPathIn.
 * - the function will crash with some short string lengths of the parameters
 */

#include <stdlib.h>
#include <string.h>

#include <winpr/wtypes.h>
#include <winpr/string.h>
#include <winpr/error.h>
#include <winpr/wlog.h>

#if DEFINE_UNICODE

HRESULT PATH_ALLOC_COMBINE(PCWSTR pszPathIn, PCWSTR pszMore,
                           WINPR_ATTR_UNUSED unsigned long dwFlags, PWSTR* ppszPathOut)
{
	WLog_WARN("TODO", "has known bugs and needs fixing.");

	if (!ppszPathOut)
		return E_INVALIDARG;

	if (!pszPathIn && !pszMore)
		return E_INVALIDARG;

	if (!pszMore)
		return E_FAIL; /* valid but not implemented, see top comment */

	if (!pszPathIn)
		return E_FAIL; /* valid but not implemented, see top comment */

	const size_t pszPathInLength = _wcslen(pszPathIn);
	const size_t pszMoreLength = _wcslen(pszMore);

	/* prevent segfaults - the complete implementation below is buggy */
	if (pszPathInLength < 3)
		return E_FAIL;

	const BOOL backslashIn =
	    (pszPathIn[pszPathInLength - 1] == CUR_PATH_SEPARATOR_CHR) ? TRUE : FALSE;
	const BOOL backslashMore = (pszMore[0] == CUR_PATH_SEPARATOR_CHR) ? TRUE : FALSE;

	if (backslashMore)
	{
		if ((pszPathIn[1] == ':') && (pszPathIn[2] == CUR_PATH_SEPARATOR_CHR))
		{
			const WCHAR colon[] = { ':', '\0' };
			const size_t pszPathOutLength = sizeof(WCHAR) + pszMoreLength;
			const size_t sizeOfBuffer = (pszPathOutLength + 1) * sizeof(WCHAR);
			PWSTR pszPathOut = (PWSTR)calloc(sizeOfBuffer, sizeof(WCHAR));

			if (!pszPathOut)
				return E_OUTOFMEMORY;

			_wcsncat(pszPathOut, &pszPathIn[0], 1);
			_wcsncat(pszPathOut, colon, ARRAYSIZE(colon));
			_wcsncat(pszPathOut, pszMore, pszMoreLength);
			*ppszPathOut = pszPathOut;
			return S_OK;
		}
	}
	else
	{
		const size_t pszPathOutLength = pszPathInLength + pszMoreLength;
		const size_t sizeOfBuffer = (pszPathOutLength + 1) * 2;
		PWSTR pszPathOut = (PWSTR)calloc(sizeOfBuffer, 2);

		if (!pszPathOut)
			return E_OUTOFMEMORY;

		_wcsncat(pszPathOut, pszPathIn, pszPathInLength);
		if (!backslashIn)
			_wcsncat(pszPathOut, CUR_PATH_SEPARATOR_STR, ARRAYSIZE(CUR_PATH_SEPARATOR_STR));
		_wcsncat(pszPathOut, pszMore, pszMoreLength);

		*ppszPathOut = pszPathOut;
		return S_OK;
	}

	return E_FAIL;
}

#else

HRESULT PATH_ALLOC_COMBINE(PCSTR pszPathIn, PCSTR pszMore, WINPR_ATTR_UNUSED unsigned long dwFlags,
                           PSTR* ppszPathOut)
{
	WLog_WARN("TODO", "has known bugs and needs fixing.");

	if (!ppszPathOut)
		return E_INVALIDARG;

	if (!pszPathIn && !pszMore)
		return E_INVALIDARG;

	if (!pszMore)
		return E_FAIL; /* valid but not implemented, see top comment */

	if (!pszPathIn)
		return E_FAIL; /* valid but not implemented, see top comment */

	const size_t pszPathInLength = strlen(pszPathIn);
	const size_t pszMoreLength = strlen(pszMore);

	/* prevent segfaults - the complete implementation below is buggy */
	if (pszPathInLength < 3)
		return E_FAIL;

	const BOOL backslashIn =
	    (pszPathIn[pszPathInLength - 1] == CUR_PATH_SEPARATOR_CHR) ? TRUE : FALSE;
	const BOOL backslashMore = (pszMore[0] == CUR_PATH_SEPARATOR_CHR) ? TRUE : FALSE;

	if (backslashMore)
	{
		if ((pszPathIn[1] == ':') && (pszPathIn[2] == CUR_PATH_SEPARATOR_CHR))
		{
			const size_t pszPathOutLength = 2 + pszMoreLength;
			const size_t sizeOfBuffer = (pszPathOutLength + 1) * 2;
			PSTR pszPathOut = calloc(sizeOfBuffer, 2);

			if (!pszPathOut)
				return E_OUTOFMEMORY;

			(void)sprintf_s(pszPathOut, sizeOfBuffer, "%c:%s", pszPathIn[0], pszMore);
			*ppszPathOut = pszPathOut;
			return S_OK;
		}
	}
	else
	{
		const size_t pszPathOutLength = pszPathInLength + pszMoreLength;
		const size_t sizeOfBuffer = (pszPathOutLength + 1) * 2;
		PSTR pszPathOut = calloc(sizeOfBuffer, 2);

		if (!pszPathOut)
			return E_OUTOFMEMORY;

		if (backslashIn)
			(void)sprintf_s(pszPathOut, sizeOfBuffer, "%s%s", pszPathIn, pszMore);
		else
			(void)sprintf_s(pszPathOut, sizeOfBuffer, "%s%s%s", pszPathIn, CUR_PATH_SEPARATOR_STR,
			                pszMore);

		*ppszPathOut = pszPathOut;
		return S_OK;
	}

	return E_FAIL;
}

#endif

/*
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef CUR_PATH_SEPARATOR_STR
#undef PATH_ALLOC_COMBINE
*/
