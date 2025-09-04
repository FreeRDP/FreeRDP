
/*
#define DEFINE_UNICODE		FALSE
#define CUR_PATH_SEPARATOR_CHR	'\\'
#define CUR_PATH_SEPARATOR_STR	"\\"
#define PATH_CCH_APPEND		PathCchAppendA
*/

#include <string.h>

#include <winpr/wtypes.h>
#include <winpr/error.h>
#include <winpr/path.h>

#if defined(DEFINE_UNICODE) && (DEFINE_UNICODE != 0)

HRESULT PATH_CCH_APPEND(PWSTR pszPath, size_t cchPath, PCWSTR pszMore)
{
	if (!pszPath)
		return E_INVALIDARG;

	if (!pszMore)
		return E_INVALIDARG;

	if ((cchPath == 0) || (cchPath > PATHCCH_MAX_CCH))
		return E_INVALIDARG;

	const size_t pszMoreLength = _wcsnlen(pszMore, cchPath);
	const size_t pszPathLength = _wcsnlen(pszPath, cchPath);

	BOOL pathBackslash = FALSE;
	if (pszPathLength > 0)
		pathBackslash = (pszPath[pszPathLength - 1] == CUR_PATH_SEPARATOR_CHR) ? TRUE : FALSE;

	const BOOL moreBackslash = (pszMore[0] == CUR_PATH_SEPARATOR_CHR) ? TRUE : FALSE;

	if (pathBackslash && moreBackslash)
	{
		if (pszMoreLength < 1)
			return E_INVALIDARG;

		if ((pszPathLength + pszMoreLength - 1) < cchPath)
		{
			WCHAR* ptr = &pszPath[pszPathLength];
			*ptr = '\0';
			_wcsncat(ptr, &pszMore[1], pszMoreLength - 1);
			return S_OK;
		}
	}
	else if ((pathBackslash && !moreBackslash) || (!pathBackslash && moreBackslash))
	{
		if ((pszPathLength + pszMoreLength) < cchPath)
		{
			WCHAR* ptr = &pszPath[pszPathLength];
			*ptr = '\0';
			_wcsncat(ptr, pszMore, pszMoreLength);
			return S_OK;
		}
	}
	else if (!pathBackslash && !moreBackslash)
	{
		if ((pszPathLength + pszMoreLength + 1) < cchPath)
		{
			WCHAR* ptr = &pszPath[pszPathLength];
			*ptr = '\0';
			_wcsncat(ptr, CUR_PATH_SEPARATOR_STR,
			         _wcsnlen(CUR_PATH_SEPARATOR_STR, ARRAYSIZE(CUR_PATH_SEPARATOR_STR)));
			_wcsncat(ptr, pszMore, pszMoreLength);
			return S_OK;
		}
	}

	return HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);
}

#else

HRESULT PATH_CCH_APPEND(PSTR pszPath, size_t cchPath, PCSTR pszMore)
{
	BOOL pathBackslash = FALSE;
	BOOL moreBackslash = FALSE;

	if (!pszPath)
		return E_INVALIDARG;

	if (!pszMore)
		return E_INVALIDARG;

	if ((cchPath == 0) || (cchPath > PATHCCH_MAX_CCH))
		return E_INVALIDARG;

	const size_t pszPathLength = strnlen(pszPath, cchPath);
	if (pszPathLength > 0)
		pathBackslash = (pszPath[pszPathLength - 1] == CUR_PATH_SEPARATOR_CHR) ? TRUE : FALSE;

	const size_t pszMoreLength = strnlen(pszMore, cchPath);
	if (pszMoreLength > 0)
		moreBackslash = (pszMore[0] == CUR_PATH_SEPARATOR_CHR) ? TRUE : FALSE;

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
			sprintf_s(&pszPath[pszPathLength], cchPath - pszPathLength, "%s%s",
			          CUR_PATH_SEPARATOR_STR, pszMore);
			return S_OK;
		}
	}

	return HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);
}

#endif

/*
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef CUR_PATH_SEPARATOR_STR
#undef PATH_CCH_APPEND
*/
