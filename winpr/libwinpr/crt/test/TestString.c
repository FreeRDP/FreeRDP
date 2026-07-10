
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/string.h>
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

static BOOL test_winpr_asprintf(void)
{
	BOOL rc = FALSE;
	const char test[] = "test string case";
	const size_t len = strnlen(test, sizeof(test));

	char* str = nullptr;
	size_t slen = 0;
	const int res = winpr_asprintf(&str, &slen, "%s", test);
	if (!str)
		goto fail;
	if (res < 0)
		goto fail;
	if ((size_t)res != len)
		goto fail;
	if (len != slen)
		goto fail;
	if (strnlen(str, slen + 10) != slen)
		goto fail;
	rc = TRUE;
fail:
	free(str);
	return rc;
}

static BOOL test_winpr_strnstr(void)
{
	struct strnstr_test
	{
		const char* haystack;
		const char* needle;
		size_t hlen;
		int expected; /* offset of the match, or -1 when NULL is expected */
	};
	const struct strnstr_test tests[] = {
		/* regression: a needle longer than hlen must never match on a prefix.
		 * winpr_strnstr used to clamp the needle length to hlen, so these
		 * returned the haystack instead of NULL (see gateway http_response_recv
		 * SIGSEGV). */
		{ "ab", "\r\n", 0, -1 },
		{ "x!", "xy", 1, -1 },
		{ "abc", "abc", 2, -1 },
		/* an empty needle returns the haystack */
		{ "abc", "", 5, 0 },
		/* basic matches */
		{ "a\r\nb", "\r\n", 4, 1 },
		{ "xxab", "ab", 4, 2 },
		{ "xxxx", "ab", 4, -1 },
		{ "abc", "abc", 3, 0 },
		/* hlen == 0 finds nothing */
		{ "abc", "a", 0, -1 },
		/* an hlen larger than the string just searches the whole string */
		{ "abc", "a", SIZE_MAX, 0 },
	};

	for (size_t i = 0; i < ARRAYSIZE(tests); i++)
	{
		const struct strnstr_test* t = &tests[i];
		char haystack[32] = { 0 };
		strncpy(haystack, t->haystack, sizeof(haystack) - 1);

		char* res = winpr_strnstr(haystack, t->needle, t->hlen);
		char* exp = (t->expected < 0) ? nullptr : haystack + t->expected;
		if (res != exp)
		{
			printf("winpr_strnstr error: case %" PRIuz " (\"%s\", \"%s\", %" PRIuz "): "
			       "actual %p, expected %p\n",
			       i, t->haystack, t->needle, t->hlen, (void*)res, (void*)exp);
			return FALSE;
		}
	}

	/* An embedded NUL terminates the search, matching native strnstr: "cd" sits
	 * past the NUL, so it must not be found even though hlen spans it. */
	char embedded[8] = { 'a', 'b', '\0', 'c', 'd', '\0', '\0', '\0' };
	if (winpr_strnstr(embedded, "cd", 5) != nullptr)
	{
		printf("winpr_strnstr error: matched past an embedded NUL terminator\n");
		return FALSE;
	}

	return TRUE;
}

static BOOL test_valid_url(void)
{
	struct test_t
	{
		char* string;
		size_t len;
		BOOL isUrl;
	};

	static const struct test_t tests[] = { { "foo.bar.blabla", 15, TRUE },
		                                   { "somehostname", 12, TRUE },
		                                   { "somehostname:1234", 17, TRUE },
		                                   { "tsv://somehostname", 18, TRUE },
		                                   { "tsv://somehostname/path/foo/gaga", 32, TRUE },
		                                   { "tsv://somehostname:1234", 23, TRUE },
		                                   { "tsv://somehostname:1234/path/foo", 33, TRUE },
		                                   { "lala\rgaga\n", 12, FALSE },
		                                   { "192.168.0.1", 11, TRUE },
		                                   //{ "192.168.0.1:1234", 16, FALSE },
		                                   //{ "192.168.0.1:1234/foo/", 21, FALSE },
		                                   { "192.168.0.1/bar/foo/", 20, TRUE },
		                                   { "[::1]", 5, FALSE },
		                                   { "[::1]:1234", 8, FALSE },
		                                   { "[2001:db8:3333:4444:5555:6666:7777:8888]", 40,
		                                     FALSE },
		                                   { "[2001:db8::1]", 13, FALSE } };
	BOOL rc = TRUE;
	for (size_t x = 0; x < ARRAYSIZE(tests); x++)
	{
		const struct test_t* cur = &tests[x];

		const BOOL rc1 = winpr_str_is_valid_url(cur->string);
		const BOOL rc2 = winpr_str_is_valid_urlN(cur->string, cur->len);

#if defined(WINPR_HAVE_REGCOMP) || defined(WITH_URIPARSER)
		if (rc1 != rc2)
			rc = FALSE;
		if (rc1 != cur->isUrl)
			rc = FALSE;
#else
		fprintf(stderr, "[%s] TODO: !defined(WINPR_HAVE_REGCOMP) && !defined(WITH_URIPARSER)\n",
		        __func__);
#endif
	}

	return rc;
}

static BOOL test_newline(void)
{
	struct test_t
	{
		char* string;
		size_t len;
		BOOL hasNewlines;
	};

	const struct test_t tests[] = { { "foo.bar.blabla", 15, FALSE },
		                            { "somehostname", 12, FALSE },
		                            { "lala\rgaga\n", 13, TRUE } };

	for (size_t x = 0; x < ARRAYSIZE(tests); x++)
	{
		const struct test_t* cur = &tests[x];

		const BOOL rc1 = winpr_str_has_newlines(cur->string);
		if (rc1 != cur->hasNewlines)
			return FALSE;
	}

	return TRUE;
}

int TestString(int argc, char* argv[])
{
	const WCHAR* p = nullptr;
	size_t pos = 0;
	size_t length = 0;
	WCHAR* context = nullptr;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!test_valid_url())
		return -1;

	if (!test_newline())
		return -1;

	if (!test_winpr_asprintf())
		return -1;

	if (!test_url_escape())
		return -1;

	if (!test_winpr_strnstr())
		return -1;

	/* _wcslen */
	WCHAR testStringW[ARRAYSIZE(testStringA)] = WINPR_C_ARRAY_INIT;
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

	if (p != nullptr)
	{
		printf("_wcschr error: return value mismatch: Actual: %p, Expected: nullptr\n",
		       (const void*)p);
		return -1;
	}

	/* wcstok_s */
	WCHAR testDelimiterW[ARRAYSIZE(testDelimiterA)] = WINPR_C_ARRAY_INIT;
	WCHAR testTokensW[ARRAYSIZE(testTokensA)] = WINPR_C_ARRAY_INIT;
	(void)ConvertUtf8NToWChar(testTokensA, ARRAYSIZE(testTokensA), testTokensW,
	                          ARRAYSIZE(testTokensW));
	(void)ConvertUtf8NToWChar(testDelimiterA, ARRAYSIZE(testDelimiterA), testDelimiterW,
	                          ARRAYSIZE(testDelimiterW));
	p = wcstok_s(testTokensW, testDelimiterW, &context);

	WCHAR testToken1W[ARRAYSIZE(testToken1A)] = WINPR_C_ARRAY_INIT;
	(void)ConvertUtf8NToWChar(testToken1A, ARRAYSIZE(testToken1A), testToken1W,
	                          ARRAYSIZE(testToken1W));
	if (memcmp(p, testToken1W, sizeof(testToken1W)) != 0)
	{
		printf("wcstok_s error: token #1 mismatch\n");
		return -1;
	}

	p = wcstok_s(nullptr, testDelimiterW, &context);

	WCHAR testToken2W[ARRAYSIZE(testToken2A)] = WINPR_C_ARRAY_INIT;
	(void)ConvertUtf8NToWChar(testToken2A, ARRAYSIZE(testToken2A), testToken2W,
	                          ARRAYSIZE(testToken2W));
	if (memcmp(p, testToken2W, sizeof(testToken2W)) != 0)
	{
		printf("wcstok_s error: token #2 mismatch\n");
		return -1;
	}

	p = wcstok_s(nullptr, testDelimiterW, &context);

	WCHAR testToken3W[ARRAYSIZE(testToken3A)] = WINPR_C_ARRAY_INIT;
	(void)ConvertUtf8NToWChar(testToken3A, ARRAYSIZE(testToken3A), testToken3W,
	                          ARRAYSIZE(testToken3W));
	if (memcmp(p, testToken3W, sizeof(testToken3W)) != 0)
	{
		printf("wcstok_s error: token #3 mismatch\n");
		return -1;
	}

	p = wcstok_s(nullptr, testDelimiterW, &context);

	if (p != nullptr)
	{
		printf("wcstok_s error: return value is not nullptr\n");
		return -1;
	}

	return 0;
}
