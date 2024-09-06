
/*
#define DEFINE_UNICODE		FALSE
#define CUR_PATH_SEPARATOR_CHR	'\\'
#define PATH_CCH_ADD_EXTENSION	PathCchAddExtensionA
*/

#if DEFINE_UNICODE

HRESULT PATH_CCH_ADD_EXTENSION(PWSTR pszPath, size_t cchPath, PCWSTR pszExt)
{
	LPWSTR pDot;
	BOOL bExtDot;
	LPWSTR pBackslash;
	size_t pszExtLength;
	size_t pszPathLength;

	if (!pszPath)
		return E_INVALIDARG;

	if (!pszExt)
		return E_INVALIDARG;

	pszExtLength = _wcslen(pszExt);
	pszPathLength = _wcslen(pszPath);
	bExtDot = (pszExt[0] == '.') ? TRUE : FALSE;

	pDot = _wcsrchr(pszPath, '.');
	pBackslash = _wcsrchr(pszPath, CUR_PATH_SEPARATOR_CHR);

	if (pDot && pBackslash)
	{
		if (pDot > pBackslash)
			return S_FALSE;
	}

	if (cchPath > pszPathLength + pszExtLength + ((bExtDot) ? 0 : 1))
	{
		const WCHAR dot[] = { '.', '\0' };
		WCHAR* ptr = &pszPath[pszPathLength];
		*ptr = '\0';

		if (!bExtDot)
			_wcsncat(ptr, dot, _wcslen(dot));
		_wcsncat(ptr, pszExt, pszExtLength);

		return S_OK;
	}

	return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
}

#else

HRESULT PATH_CCH_ADD_EXTENSION(PSTR pszPath, size_t cchPath, PCSTR pszExt)
{
	CHAR* pDot;
	BOOL bExtDot;
	CHAR* pBackslash;
	size_t pszExtLength;
	size_t pszPathLength;

	if (!pszPath)
		return E_INVALIDARG;

	if (!pszExt)
		return E_INVALIDARG;

	pszExtLength = strlen(pszExt);
	pszPathLength = strlen(pszPath);
	bExtDot = (pszExt[0] == '.') ? TRUE : FALSE;

	pDot = strrchr(pszPath, '.');
	pBackslash = strrchr(pszPath, CUR_PATH_SEPARATOR_CHR);

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

	return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
}

#endif

/*
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_EXTENSION
*/
