
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/windows.h>

static const CHAR testStringA[] = { 'T', 'h', 'e', ' ', 'q', 'u', 'i', 'c', 'k', ' ', 'b',
	                                'r', 'o', 'w', 'n', ' ', 'f', 'o', 'x', ' ', 'j', 'u',
	                                'm', 'p', 's', ' ', 'o', 'v', 'e', 'r', ' ', 't', 'h',
	                                'e', ' ', 'l', 'a', 'z', 'y', ' ', 'd', 'o', 'g', '\0' };

#define testStringA_Length ((sizeof(testStringA) / sizeof(CHAR)) - 1)

static const CHAR testToken1A[] = { 'q', 'u', 'i', 'c', 'k', '\0' };
static const CHAR testToken2A[] = { 'b', 'r', 'o', 'w', 'n', '\0' };
static const CHAR testToken3A[] = { 'f', 'o', 'x', '\0' };

#define testToken1A_Length ((sizeof(testToken1A) / sizeof(CHAR)) - 1)
#define testToken2A_Length ((sizeof(testToken2A) / sizeof(CHAR)) - 1)
#define testToken3A_Length ((sizeof(testToken3A) / sizeof(CHAR)) - 1)

static const CHAR testTokensA[] = { 'q', 'u', 'i',  'c',  'k', '\r', '\n', 'b',  'r',  'o',
	                                'w', 'n', '\r', '\n', 'f', 'o',  'x',  '\r', '\n', '\0' };

#define testTokensA_Length ((sizeof(testTokensA) / sizeof(CHAR)) - 1)

static const CHAR testDelimiterA[] = { '\r', '\n', '\0' };

#define testDelimiterA_Length ((sizeof(testDelimiter) / sizeof(CHAR)) - 1)

struct url_test_pair
{
	const char* what;
	const char* escaped;
};

static const struct url_test_pair url_tests[] = {
	{ "xxx%bar ga<ka>ee#%%#%{h}g{f{e%d|c\\b^a~p[q]r`s;t/u?v:w@x=y&z$xxx",
	  "xxx%25bar%20ga%3Cka%3Eee%23%25%25%23%25%7Bh%7Dg%7Bf%7Be%25d%7Cc%5Cb%5Ea~p%5Bq%5Dr%60s%3Bt%"
	  "2Fu%3Fv%3Aw%40x%3Dy%26z%24xxx" },
	{ "äöúëü", "%C3%A4%C3%B6%C3%BA%C3%AB%C3%BC" },
	{ "🎅🏄🤘😈", "%F0%9F%8E%85%F0%9F%8F%84%F0%9F%A4%98%F0%9F%98%88" },
	{ "foo$.%.^.&.\\.txt+", "foo%24.%25.%5E.%26.%5C.txt%2B" }
};

static BOOL test_url_escape(void)
{
	for (size_t x = 0; x < ARRAYSIZE(url_tests); x++)
	{
		const struct url_test_pair* cur = &url_tests[x];

		char* escaped = winpr_str_url_encode(cur->what, strlen(cur->what) + 1);
		char* what = winpr_str_url_decode(cur->escaped, strlen(cur->escaped) + 1);

		const size_t elen = strlen(escaped);
		const size_t wlen = strlen(what);
		const size_t pelen = strlen(cur->escaped);
		const size_t pwlen = strlen(cur->what);
		BOOL rc = TRUE;
		if (!escaped || (elen != pelen) || (strcmp(escaped, cur->escaped) != 0))
		{
			printf("expected: [%" PRIuz "] %s\n", pelen, cur->escaped);
			printf("got     : [%" PRIuz "] %s\n", elen, escaped);
			rc = FALSE;
		}
		else if (!what || (wlen != pwlen) || (strcmp(what, cur->what) != 0))
		{
			printf("expected: [%" PRIuz "] %s\n", pwlen, cur->what);
			printf("got     : [%" PRIuz "] %s\n", wlen, what);
			rc = FALSE;
		}

		free(escaped);
		free(what);
		if (!rc)
			return FALSE;
	}

	return TRUE;
}

int TestString(int argc, char* argv[])
{
	const WCHAR* p = NULL;
	size_t pos = 0;
	size_t length = 0;
	WCHAR* context = NULL;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!test_url_escape())
		return -1;

	/* _wcslen */
	WCHAR testStringW[ARRAYSIZE(testStringA)] = { 0 };
	(void)ConvertUtf8NToWChar(testStringA, ARRAYSIZE(testStringA), testStringW,
	                          ARRAYSIZE(testStringW));
	const size_t testStringW_Length = testStringA_Length;
	length = _wcslen(testStringW);

	if (length != testStringW_Length)
	{
		printf("_wcslen error: length mismatch: Actual: %" PRIuz ", Expected: %" PRIuz "\n", length,
		       testStringW_Length);
		return -1;
	}

	/* _wcschr */
	union
	{
		char c[2];
		WCHAR w;
	} search;
	search.c[0] = 'r';
	search.c[1] = '\0';

	p = _wcschr(testStringW, search.w);
	pos = (p - testStringW);

	if (pos != 11)
	{
		printf("_wcschr error: position mismatch: Actual: %" PRIuz ", Expected: 11\n", pos);
		return -1;
	}

	p = _wcschr(&testStringW[pos + 1], search.w);
	pos = (p - testStringW);

	if (pos != 29)
	{
		printf("_wcschr error: position mismatch: Actual: %" PRIuz ", Expected: 29\n", pos);
		return -1;
	}

	p = _wcschr(&testStringW[pos + 1], search.w);

	if (p != NULL)
	{
		printf("_wcschr error: return value mismatch: Actual: %p, Expected: NULL\n",
		       (const void*)p);
		return -1;
	}

	/* wcstok_s */
	WCHAR testDelimiterW[ARRAYSIZE(testDelimiterA)] = { 0 };
	WCHAR testTokensW[ARRAYSIZE(testTokensA)] = { 0 };
	(void)ConvertUtf8NToWChar(testTokensA, ARRAYSIZE(testTokensA), testTokensW,
	                          ARRAYSIZE(testTokensW));
	(void)ConvertUtf8NToWChar(testDelimiterA, ARRAYSIZE(testDelimiterA), testDelimiterW,
	                          ARRAYSIZE(testDelimiterW));
	p = wcstok_s(testTokensW, testDelimiterW, &context);

	WCHAR testToken1W[ARRAYSIZE(testToken1A)] = { 0 };
	(void)ConvertUtf8NToWChar(testToken1A, ARRAYSIZE(testToken1A), testToken1W,
	                          ARRAYSIZE(testToken1W));
	if (memcmp(p, testToken1W, sizeof(testToken1W)) != 0)
	{
		printf("wcstok_s error: token #1 mismatch\n");
		return -1;
	}

	p = wcstok_s(NULL, testDelimiterW, &context);

	WCHAR testToken2W[ARRAYSIZE(testToken2A)] = { 0 };
	(void)ConvertUtf8NToWChar(testToken2A, ARRAYSIZE(testToken2A), testToken2W,
	                          ARRAYSIZE(testToken2W));
	if (memcmp(p, testToken2W, sizeof(testToken2W)) != 0)
	{
		printf("wcstok_s error: token #2 mismatch\n");
		return -1;
	}

	p = wcstok_s(NULL, testDelimiterW, &context);

	WCHAR testToken3W[ARRAYSIZE(testToken3A)] = { 0 };
	(void)ConvertUtf8NToWChar(testToken3A, ARRAYSIZE(testToken3A), testToken3W,
	                          ARRAYSIZE(testToken3W));
	if (memcmp(p, testToken3W, sizeof(testToken3W)) != 0)
	{
		printf("wcstok_s error: token #3 mismatch\n");
		return -1;
	}

	p = wcstok_s(NULL, testDelimiterW, &context);

	if (p != NULL)
	{
		printf("wcstok_s error: return value is not NULL\n");
		return -1;
	}

	return 0;
}
