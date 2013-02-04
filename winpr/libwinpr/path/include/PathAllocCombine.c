
/*
#define DEFINE_UNICODE		FALSE
#define _PATH_SEPARATOR_CHR	'\\'
#define _PATH_SEPARATOR_STR	"\\"
#define PATH_ALLOC_COMBINE	PathAllocCombineA
*/

#if DEFINE_UNICODE

HRESULT PATH_ALLOC_COMBINE(PCWSTR pszPathIn, PCWSTR pszMore, unsigned long dwFlags, PWSTR* ppszPathOut)
{
#ifdef _WIN32
	PWSTR pszPathOut;
	BOOL backslashIn;
	BOOL backslashMore;
	int pszMoreLength;
	int pszPathInLength;
	int pszPathOutLength;

	if (!pszPathIn)
		return S_FALSE;

	if (!pszMore)
		return S_FALSE;

	pszPathInLength = lstrlenW(pszPathIn);
	pszMoreLength = lstrlenW(pszMore);

	backslashIn = (pszPathIn[pszPathInLength - 1] == _PATH_SEPARATOR_CHR) ? TRUE : FALSE;
	backslashMore = (pszMore[0] == _PATH_SEPARATOR_CHR) ? TRUE : FALSE;

	if (backslashMore)
	{
		if ((pszPathIn[1] == ':') && (pszPathIn[2] == _PATH_SEPARATOR_CHR))
		{
			size_t sizeOfBuffer;

			pszPathOutLength = 2 + pszMoreLength;
			sizeOfBuffer = (pszPathOutLength + 1) * 2;

			pszPathOut = (PWSTR) HeapAlloc(GetProcessHeap(), 0, sizeOfBuffer * 2);
			swprintf_s(pszPathOut, sizeOfBuffer, L"%c:%s", pszPathIn[0], pszMore);

			*ppszPathOut = pszPathOut;

			return S_OK;
		}
	}
	else
	{
		size_t sizeOfBuffer;

		pszPathOutLength = pszPathInLength + pszMoreLength;
		sizeOfBuffer = (pszPathOutLength + 1) * 2;

		pszPathOut = (PWSTR) HeapAlloc(GetProcessHeap(), 0, sizeOfBuffer * 2);

		if (backslashIn)
			swprintf_s(pszPathOut, sizeOfBuffer, L"%s%s", pszPathIn, pszMore);
		else
			swprintf_s(pszPathOut, sizeOfBuffer, L"%s" _PATH_SEPARATOR_STR L"%s", pszPathIn, pszMore);

		*ppszPathOut = pszPathOut;

		return S_OK;
	}
#endif

	return S_OK;
}

#else

HRESULT PATH_ALLOC_COMBINE(PCSTR pszPathIn, PCSTR pszMore, unsigned long dwFlags, PSTR* ppszPathOut)
{
	PSTR pszPathOut;
	BOOL backslashIn;
	BOOL backslashMore;
	int pszMoreLength;
	int pszPathInLength;
	int pszPathOutLength;

	if (!pszPathIn)
		return S_FALSE;

	if (!pszMore)
		return S_FALSE;

	pszPathInLength = lstrlenA(pszPathIn);
	pszMoreLength = lstrlenA(pszMore);

	backslashIn = (pszPathIn[pszPathInLength - 1] == _PATH_SEPARATOR_CHR) ? TRUE : FALSE;
	backslashMore = (pszMore[0] == _PATH_SEPARATOR_CHR) ? TRUE : FALSE;

	if (backslashMore)
	{
		if ((pszPathIn[1] == ':') && (pszPathIn[2] == _PATH_SEPARATOR_CHR))
		{
			size_t sizeOfBuffer;

			pszPathOutLength = 2 + pszMoreLength;
			sizeOfBuffer = (pszPathOutLength + 1) * 2;

			pszPathOut = (PSTR) HeapAlloc(GetProcessHeap(), 0, sizeOfBuffer * 2);
			sprintf_s(pszPathOut, sizeOfBuffer, "%c:%s", pszPathIn[0], pszMore);

			*ppszPathOut = pszPathOut;

			return S_OK;
		}
	}
	else
	{
		size_t sizeOfBuffer;

		pszPathOutLength = pszPathInLength + pszMoreLength;
		sizeOfBuffer = (pszPathOutLength + 1) * 2;

		pszPathOut = (PSTR) HeapAlloc(GetProcessHeap(), 0, sizeOfBuffer * 2);

		if (backslashIn)
			sprintf_s(pszPathOut, sizeOfBuffer, "%s%s", pszPathIn, pszMore);
		else
			sprintf_s(pszPathOut, sizeOfBuffer, "%s" _PATH_SEPARATOR_STR "%s", pszPathIn, pszMore);

		*ppszPathOut = pszPathOut;

		return S_OK;
	}

	return S_OK;
}

#endif

/*
#undef DEFINE_UNICODE
#undef _PATH_SEPARATOR_CHR
#undef _PATH_SEPARATOR_STR
#undef PATH_ALLOC_COMBINE
*/

