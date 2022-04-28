
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

int TestString(int argc, char* argv[])
{
	const WCHAR* p;
	size_t pos;
	size_t length;
	WCHAR* context;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

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
		printf("_wcschr error: return value mismatch: Actual: %p, Expected: NULL\n", p);
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
