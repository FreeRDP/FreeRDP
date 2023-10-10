
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/windows.h>

static WCHAR testStringW[] = { 'T', 'h', 'e', ' ', 'q', 'u', 'i', 'c', 'k', ' ', 'b',
	                           'r', 'o', 'w', 'n', ' ', 'f', 'o', 'x', ' ', 'j', 'u',
	                           'm', 'p', 's', ' ', 'o', 'v', 'e', 'r', ' ', 't', 'h',
	                           'e', ' ', 'l', 'a', 'z', 'y', ' ', 'd', 'o', 'g', '\0' };

#define testStringW_Length ((sizeof(testStringW) / sizeof(WCHAR)) - 1)

static WCHAR testToken1W[] = { 'q', 'u', 'i', 'c', 'k', '\0' };
static WCHAR testToken2W[] = { 'b', 'r', 'o', 'w', 'n', '\0' };
static WCHAR testToken3W[] = { 'f', 'o', 'x', '\0' };

#define testToken1W_Length ((sizeof(testToken1W) / sizeof(WCHAR)) - 1)
#define testToken2W_Length ((sizeof(testToken2W) / sizeof(WCHAR)) - 1)
#define testToken3W_Length ((sizeof(testToken3W) / sizeof(WCHAR)) - 1)

static WCHAR testTokensW[] = { 'q', 'u', 'i',  'c',  'k', '\r', '\n', 'b',  'r',  'o',
	                           'w', 'n', '\r', '\n', 'f', 'o',  'x',  '\r', '\n', '\0' };

#define testTokensW_Length ((sizeof(testTokensW) / sizeof(WCHAR)) - 1)

static WCHAR testDelimiter[] = { '\r', '\n', '\0' };

#define testDelimiter_Length ((sizeof(testDelimiter) / sizeof(WCHAR)) - 1)

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
	const WCHAR* p;
	size_t pos;
	size_t length;
	WCHAR* context;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!test_url_escape())
		return -1;

#ifdef __BIG_ENDIAN__
	/* Be sure that we always use LE encoded string */
	ByteSwapUnicode(testStringW, testStringW_Length);
	ByteSwapUnicode(testToken1W, testToken1W_Length);
	ByteSwapUnicode(testToken2W, testToken2W_Length);
	ByteSwapUnicode(testToken3W, testToken3W_Length);
	ByteSwapUnicode(testTokensW, testTokensW_Length);
	ByteSwapUnicode(testDelimiter, testDelimiter_Length);
#endif

	/* _wcslen */

	length = _wcslen(testStringW);

	if (length != testStringW_Length)
	{
		printf("_wcslen error: length mismatch: Actual: %" PRIuz ", Expected: %" PRIuz "\n", length,
		       testStringW_Length);
		return -1;
	}

	/* _wcschr */

	p = _wcschr(testStringW, 'r');
	pos = (p - testStringW);

	if (pos != 11)
	{
		printf("_wcschr error: position mismatch: Actual: %" PRIuz ", Expected: 11\n", pos);
		return -1;
	}

	p = _wcschr(&testStringW[pos + 1], 'r');
	pos = (p - testStringW);

	if (pos != 29)
	{
		printf("_wcschr error: position mismatch: Actual: %" PRIuz ", Expected: 29\n", pos);
		return -1;
	}

	p = _wcschr(&testStringW[pos + 1], 'r');

	if (p != NULL)
	{
		printf("_wcschr error: return value mismatch: Actual: %p, Expected: NULL\n",
		       (const void*)p);
		return -1;
	}

	/* wcstok_s */

	p = wcstok_s(testTokensW, testDelimiter, &context);

	if (memcmp(p, testToken1W, sizeof(testToken1W)) != 0)
	{
		printf("wcstok_s error: token #1 mismatch\n");
		return -1;
	}

	p = wcstok_s(NULL, testDelimiter, &context);

	if (memcmp(p, testToken2W, sizeof(testToken2W)) != 0)
	{
		printf("wcstok_s error: token #2 mismatch\n");
		return -1;
	}

	p = wcstok_s(NULL, testDelimiter, &context);

	if (memcmp(p, testToken3W, sizeof(testToken3W)) != 0)
	{
		printf("wcstok_s error: token #3 mismatch\n");
		return -1;
	}

	p = wcstok_s(NULL, testDelimiter, &context);

	if (p != NULL)
	{
		printf("wcstok_s error: return value is not NULL\n");
		return -1;
	}

	return 0;
}
