
/*
#define DEFINE_UNICODE		FALSE
#define _PATH_SEPARATOR_CHR	'\\'
#define PATH_CCH_ADD_EXTENSION	PathCchAddExtensionA
*/

#if DEFINE_UNICODE

HRESULT PATH_CCH_ADD_EXTENSION(PWSTR pszPath, size_t cchPath, PCWSTR pszExt)
{
#ifdef _WIN32
	LPWSTR pDot;
	BOOL bExtDot;
	LPWSTR pBackslash;
	size_t pszExtLength;
	size_t pszPathLength;

	if (!pszPath)
		return E_INVALIDARG;

	if (!pszExt)
		return E_INVALIDARG;

	pszExtLength = lstrlenW(pszExt);
	pszPathLength = lstrlenW(pszPath);
	bExtDot = (pszExt[0] == '.') ? TRUE : FALSE;

	pDot = wcsrchr(pszPath, '.');
	pBackslash = wcsrchr(pszPath, _PATH_SEPARATOR_CHR);

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

	pszExtLength = lstrlenA(pszExt);
	pszPathLength = lstrlenA(pszPath);
	bExtDot = (pszExt[0] == '.') ? TRUE : FALSE;

	pDot = strrchr(pszPath, '.');
	pBackslash = strrchr(pszPath, _PATH_SEPARATOR_CHR);

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
#undef _PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_EXTENSION
*/
