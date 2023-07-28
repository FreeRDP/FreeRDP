
/*
#define DEFINE_UNICODE		FALSE
#define CUR_PATH_SEPARATOR_CHR	'\\'
#define CUR_PATH_SEPARATOR_STR	"\\"
#define PATH_CCH_APPEND		PathCchAppendA
*/

#if DEFINE_UNICODE

HRESULT PATH_CCH_APPEND(PWSTR pszPath, size_t cchPath, PCWSTR pszMore)
{
	BOOL pathBackslash;
	BOOL moreBackslash;
	size_t pszMoreLength;
	size_t pszPathLength;

	if (!pszPath)
		return E_INVALIDARG;

	if (!pszMore)
		return E_INVALIDARG;

	if (cchPath == 0 || cchPath > PATHCCH_MAX_CCH)
		return E_INVALIDARG;

	pszMoreLength = _wcslen(pszMore);
	pszPathLength = _wcslen(pszPath);

	pathBackslash = (pszPath[pszPathLength - 1] == CUR_PATH_SEPARATOR_CHR) ? TRUE : FALSE;
	moreBackslash = (pszMore[0] == CUR_PATH_SEPARATOR_CHR) ? TRUE : FALSE;

	if (pathBackslash && moreBackslash)
	{
		if ((pszPathLength + pszMoreLength - 1) < cchPath)
		{
			WCHAR* ptr = &pszPath[pszPathLength];
			*ptr = '\0';
			_wcsncat(ptr, &pszMore[1], _wcslen(&pszMore[1]));
			return S_OK;
		}
	}
	else if ((pathBackslash && !moreBackslash) || (!pathBackslash && moreBackslash))
	{
		if ((pszPathLength + pszMoreLength) < cchPath)
		{
			WCHAR* ptr = &pszPath[pszPathLength];
			*ptr = '\0';
			_wcsncat(ptr, pszMore, _wcslen(pszMore));
			return S_OK;
		}
	}
	else if (!pathBackslash && !moreBackslash)
	{
		if ((pszPathLength + pszMoreLength + 1) < cchPath)
		{
			const WCHAR sep[] = CUR_PATH_SEPARATOR_STR;
			WCHAR* ptr = &pszPath[pszPathLength];
			*ptr = '\0';
			_wcsncat(ptr, sep, _wcslen(sep));
			_wcsncat(ptr, pszMore, _wcslen(pszMore));
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
	size_t pszMoreLength;
	size_t pszPathLength;

	if (!pszPath)
		return E_INVALIDARG;

	if (!pszMore)
		return E_INVALIDARG;

	if (cchPath == 0 || cchPath > PATHCCH_MAX_CCH)
		return E_INVALIDARG;

	pszPathLength = strlen(pszPath);
	if (pszPathLength > 0)
		pathBackslash = (pszPath[pszPathLength - 1] == CUR_PATH_SEPARATOR_CHR) ? TRUE : FALSE;

	pszMoreLength = strlen(pszMore);
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
			sprintf_s(&pszPath[pszPathLength], cchPath - pszPathLength, CUR_PATH_SEPARATOR_STR "%s",
			          pszMore);
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
