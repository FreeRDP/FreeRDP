
/*
#define DEFINE_UNICODE			FALSE
#define PATH_SEPARATOR			'\\'
#define PATH_CCH_ADD_SEPARATOR_EX	PathCchAddBackslashExA
*/

#if DEFINE_UNICODE

HRESULT PATH_CCH_ADD_SEPARATOR_EX(PWSTR pszPath, size_t cchPath, PWSTR* ppszEnd, size_t* pcchRemaining)
{
	size_t pszPathLength;

	if (!pszPath)
		return S_FALSE;

	pszPathLength = lstrlenW(pszPath);

	if (pszPath[pszPathLength - 1] == PATH_SEPARATOR)
		return S_FALSE;

	if (cchPath > (pszPathLength + 1))
	{
		pszPath[pszPathLength] = PATH_SEPARATOR;
		pszPath[pszPathLength + 1] = '\0';

		return S_OK;
	}

	return S_FALSE;
}

#else

HRESULT PATH_CCH_ADD_SEPARATOR_EX(PSTR pszPath, size_t cchPath, PSTR* ppszEnd, size_t* pcchRemaining)
{
	size_t pszPathLength;

	if (!pszPath)
		return S_FALSE;

	pszPathLength = lstrlenA(pszPath);

	if (pszPath[pszPathLength - 1] == PATH_SEPARATOR)
		return S_FALSE;

	if (cchPath > (pszPathLength + 1))
	{
		pszPath[pszPathLength] = PATH_SEPARATOR;
		pszPath[pszPathLength + 1] = '\0';

		return S_OK;
	}

	return S_FALSE;
}

#endif

/*
#undef DEFINE_UNICODE
#undef PATH_SEPARATOR
#undef PATH_CCH_ADD_SEPARATOR_EX
*/

