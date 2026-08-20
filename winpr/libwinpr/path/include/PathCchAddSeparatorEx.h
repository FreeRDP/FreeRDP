
#include <winpr/wtypes.h>

/*
#define DEFINE_UNICODE			FALSE
#define CUR_PATH_SEPARATOR_CHR		'\\'
#define PATH_CCH_ADD_SEPARATOR_EX	PathCchAddBackslashExA
*/

#if DEFINE_UNICODE

HRESULT PATH_CCH_ADD_SEPARATOR_EX(PWSTR pszPath, size_t cchPath, WINPR_ATTR_UNUSED PWSTR* ppszEnd,
                                  WINPR_ATTR_UNUSED size_t* pcchRemaining)
{
	WINPR_ASSERT(pszPath || (cchPath == 0));
	if (!pszPath)
		return S_OK;

	const size_t pszPathLength = _wcsnlen(pszPath, cchPath);

	if (pszPath[pszPathLength - 1] == CUR_PATH_SEPARATOR_CHR)
		return S_FALSE;

	if (cchPath > (pszPathLength + 1))
	{
		pszPath[pszPathLength] = CUR_PATH_SEPARATOR_CHR;
		pszPath[pszPathLength + 1] = '\0';

		return S_OK;
	}

	return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
}

#else

HRESULT PATH_CCH_ADD_SEPARATOR_EX(WINPR_ATTR_UNUSED PSTR pszPath, WINPR_ATTR_UNUSED size_t cchPath,
                                  WINPR_ATTR_UNUSED PSTR* ppszEnd,
                                  WINPR_ATTR_UNUSED size_t* pcchRemaining)
{
	WINPR_ASSERT(pszPath || (cchPath == 0));
	if (!pszPath)
		return S_OK;

	const size_t pszPathLength = strnlen(pszPath, cchPath);

	if (pszPath[pszPathLength - 1] == CUR_PATH_SEPARATOR_CHR)
		return S_FALSE;

	if (cchPath > (pszPathLength + 1))
	{
		pszPath[pszPathLength] = CUR_PATH_SEPARATOR_CHR;
		pszPath[pszPathLength + 1] = '\0';

		return S_OK;
	}

	return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
}

#endif

/*
#undef DEFINE_UNICODE
#undef CUR_PATH_SEPARATOR_CHR
#undef PATH_CCH_ADD_SEPARATOR_EX
*/
