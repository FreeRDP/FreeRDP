
/*
#define DEFINE_UNICODE			FALSE
#define _PATH_SEPARATOR_CHR		'\\'
#define PATH_CCH_ADD_SEPARATOR_EX	PathCchAddBackslashExA
*/

#if DEFINE_UNICODE

HRESULT PATH_CCH_ADD_SEPARATOR_EX(PWSTR pszPath, size_t cchPath, PWSTR* ppszEnd,
                                  size_t* pcchRemaining)
{
	size_t pszPathLength;

	if (!pszPath)
		return E_INVALIDARG;

	pszPathLength = lstrlenW(pszPath);

	if (pszPath[pszPathLength - 1] == _PATH_SEPARATOR_CHR)
		return S_FALSE;

	if (cchPath > (pszPathLength + 1))
	{
		pszPath[pszPathLength] = _PATH_SEPARATOR_CHR;
		pszPath[pszPathLength + 1] = '\0';

		return S_OK;
	}

	return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
}

#else

HRESULT PATH_CCH_ADD_SEPARATOR_EX(PSTR pszPath, size_t cchPath, PSTR* ppszEnd,
                                  size_t* pcchRemaining)
{
	size_t pszPathLength;

	if (!pszPath)
		return E_INVALIDARG;

	pszPathLength = lstrlenA(pszPath);

	if (pszPath[pszPathLength - 1] == _PATH_SEPARATOR_CHR)
		return S_FALSE;

	if (cchPath > (pszPathLength + 1))
	{
		pszPath[pszPathLength] = _PATH_SEPARATOR_CHR;
		pszPath[pszPathLength + 1] = '\0';

		return S_OK;
	}

	return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
}

#endif

/*
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR_EX
*/
