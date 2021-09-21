
/*
#define DEFINE_UNICODE		FALSE
#define CUR_PATH_SEPARATOR_CHR	'\\'
#define CUR_PATH_SEPARATOR_STR	"\\"
#define PATH_CCH_APPEND		PathCchAppendA
*/

#if DEFINE_UNICODE

HRESULT PATH_CCH_APPEND(PWSTR pszPath, size_t cchPath, PCWSTR pszMore)
{
#ifdef _WIN32
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

	pszMoreLength = lstrlenW(pszMore);
	pszPathLength = lstrlenW(pszPath);

	pathBackslash = (pszPath[pszPathLength - 1] == CUR_PATH_SEPARATOR_CHR) ? TRUE : FALSE;
	moreBackslash = (pszMore[0] == CUR_PATH_SEPARATOR_CHR) ? TRUE : FALSE;

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
			swprintf_s(&pszPath[pszPathLength], cchPath - pszPathLength,
			           CUR_PATH_SEPARATOR_STR L"%s", pszMore);
			return S_OK;
		}
	}
#endif

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

	pszPathLength = lstrlenA(pszPath);
	if (pszPathLength > 0)
		pathBackslash = (pszPath[pszPathLength - 1] == CUR_PATH_SEPARATOR_CHR) ? TRUE : FALSE;

	pszMoreLength = lstrlenA(pszMore);
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
