
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/error.h>
#include <winpr/print.h>
#include <winpr/windows.h>

#define TESTCASE_BUFFER_SIZE 8192

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

typedef struct
{
	char* utf8;
	size_t utf8len;
	WCHAR* utf16;
	size_t utf16len;
} testcase_t;

// TODO: The unit tests do not check for valid code points, so always end the test
// strings with a simple ASCII symbol for now.
static const testcase_t unit_testcases[] = {
	{ "foo", 3, "f\x00o\x00o\x00\x00\x00", 3 },
	{ "foo", 4, "f\x00o\x00o\x00\x00\x00", 4 },
	{ "✊🎅ęʥ꣸𑗊a", 19,
	  "\x0a\x27\x3c\xd8\x85\xdf\x19\x01\xa5\x02\xf8\xa8\x05\xd8\xca\xdd\x61\x00\x00\x00", 9 }
};

static void create_prefix(char* prefix, size_t prefixlen, size_t buffersize, SSIZE_T rc,
                          SSIZE_T inputlen, const testcase_t* test, const char* fkt, size_t line)
{
	_snprintf(prefix, prefixlen,
	          "[%s:%" PRIuz "] '%s' [utf8: %" PRIuz ", utf16: %" PRIuz "] buffersize: %" PRIuz
	          ", rc: %" PRIdz ", inputlen: %" PRIdz ":: ",
	          fkt, line, test->utf8, test->utf8len, test->utf16len, buffersize, rc, inputlen);
}

static BOOL check_short_buffer(const char* prefix, int rc, size_t buffersize,
                               const testcase_t* test, BOOL utf8)
{
	if ((rc > 0) && ((size_t)rc <= buffersize))
		return TRUE;

	size_t len = test->utf8len;
	if (!utf8)
		len = test->utf16len;

	if (buffersize > len)
	{
		fprintf(stderr,
		        "%s length does not match buffersize: %" PRIdz " != %" PRId32
		        ",but is large enough to hold result\n",
		        prefix, rc, buffersize);
		return FALSE;
	}
	const DWORD err = GetLastError();
	if (err != ERROR_INSUFFICIENT_BUFFER)
	{

		fprintf(stderr,
		        "%s length does not match buffersize: %" PRIdz " != %" PRId32
		        ", unexpected GetLastError() 0x08%" PRIx32 "\n",
		        prefix, rc, buffersize, err);
		return FALSE;
	}
	else
		return TRUE;
}

#define compare_utf16(what, buffersize, rc, inputlen, test) \
	compare_utf16_int((what), (buffersize), (rc), (inputlen), (test), __FUNCTION__, __LINE__)
static BOOL compare_utf16_int(const WCHAR* what, size_t buffersize, SSIZE_T rc, SSIZE_T inputlen,
                              const testcase_t* test, const char* fkt, size_t line)
{
	char prefix[8192] = { 0 };
	create_prefix(prefix, ARRAYSIZE(prefix), buffersize, rc, inputlen, test, fkt, line);

	WINPR_ASSERT(what || (buffersize == 0));
	WINPR_ASSERT(test);

	const size_t welen = _wcsnlen(test->utf16, test->utf16len);
	if (buffersize > welen)
	{
		if ((rc < 0) || ((size_t)rc != welen))
		{
			fprintf(stderr, "%s length does not match expectation: %" PRIdz " != %" PRIuz "\n",
			        prefix, rc, welen);
			return FALSE;
		}
	}
	else
	{
		if (!check_short_buffer(prefix, rc, buffersize, test, FALSE))
			return FALSE;
	}

	if ((rc > 0) && (buffersize > (size_t)rc))
	{
		const size_t wlen = _wcsnlen(what, buffersize);
		if ((rc < 0) || (wlen > (size_t)rc))
		{
			fprintf(stderr, "%s length does not match wcslen: %" PRIdz " < %" PRIuz "\n", prefix,
			        rc, wlen);
			return FALSE;
		}
	}

	if (memcmp(test->utf16, what, rc * sizeof(WCHAR)) != 0)
	{
		fprintf(stderr, "%s contents does not match expectations: TODO '%s' != '%s'\n", prefix,
		        test->utf8, test->utf8);
		return FALSE;
	}

	printf("%s success\n", prefix);

	return TRUE;
}

#define compare_utf8(what, buffersize, rc, inputlen, test) \
	compare_utf8_int((what), (buffersize), (rc), (inputlen), (test), __FUNCTION__, __LINE__)
static BOOL compare_utf8_int(const char* what, size_t buffersize, SSIZE_T rc, SSIZE_T inputlen,
                             const testcase_t* test, const char* fkt, size_t line)
{
	char prefix[8192] = { 0 };
	create_prefix(prefix, ARRAYSIZE(prefix), buffersize, rc, inputlen, test, fkt, line);

	WINPR_ASSERT(what || (buffersize == 0));
	WINPR_ASSERT(test);

	const size_t slen = strnlen(test->utf8, test->utf8len);
	if (buffersize > slen)
	{
		if ((rc < 0) || ((size_t)rc != slen))
		{
			fprintf(stderr, "%s length does not match expectation: %" PRIdz " != %" PRIuz "\n",
			        prefix, rc, slen);
			return FALSE;
		}
	}
	else
	{
		if (!check_short_buffer(prefix, rc, buffersize, test, TRUE))
			return FALSE;
	}

	if ((rc > 0) && (buffersize > (size_t)rc))
	{
		const size_t wlen = strnlen(what, buffersize);
		if (wlen != (size_t)rc)
		{
			fprintf(stderr, "%s length does not match strnlen: %" PRIdz " != %" PRIuz "\n", prefix,
			        rc, wlen);
			return FALSE;
		}
	}

	if (memcmp(test->utf8, what, rc) != 0)
	{
		fprintf(stderr, "%s contents does not match expectations: '%s' != '%s'\n", prefix, what,
		        test->utf8);
		return FALSE;
	}
	printf("%s success\n", prefix);

	return TRUE;
}

static BOOL test_convert_to_utf16(const testcase_t* test)
{
	const size_t len[] = { TESTCASE_BUFFER_SIZE, test->utf16len, test->utf16len + 1,
		                   test->utf16len - 1 };
	const size_t max = test->utf16len > 0 ? ARRAYSIZE(len) : ARRAYSIZE(len) - 1;

	const SSIZE_T rc2 = ConvertUtf8ToWChar(test->utf8, NULL, 0);
	const size_t wlen = _wcsnlen(test->utf16, test->utf16len);
	if ((rc2 < 0) || ((size_t)rc2 != wlen))
	{
		char prefix[8192] = { 0 };
		create_prefix(prefix, ARRAYSIZE(prefix), 0, rc2, -1, test, __FUNCTION__, __LINE__);
		fprintf(stderr, "%s ConvertUtf8ToWChar(%s, NULL, 0) expected %" PRIuz ", got %" PRIdz "\n",
		        prefix, test->utf8, wlen, rc2);
		return FALSE;
	}
	for (size_t x = 0; x < max; x++)
	{
		WCHAR buffer[TESTCASE_BUFFER_SIZE] = { 0 };
		const SSIZE_T rc = ConvertUtf8ToWChar(test->utf8, buffer, len[x]);
		if (!compare_utf16(buffer, len[x], rc, -1, test))
			return FALSE;
	}

	return TRUE;
}

static BOOL test_convert_to_utf16_n(const testcase_t* test)
{
	const size_t len[] = { TESTCASE_BUFFER_SIZE, test->utf16len, test->utf16len + 1,
		                   test->utf16len - 1 };
	const size_t max = test->utf16len > 0 ? ARRAYSIZE(len) : ARRAYSIZE(len) - 1;

	const SSIZE_T rc2 = ConvertUtf8NToWChar(test->utf8, test->utf8len, NULL, 0);
	const size_t wlen = _wcsnlen(test->utf16, test->utf16len);
	if ((rc2 < 0) || ((size_t)rc2 != wlen))
	{
		char prefix[8192] = { 0 };
		create_prefix(prefix, ARRAYSIZE(prefix), 0, rc2, test->utf8len, test, __FUNCTION__,
		              __LINE__);
		fprintf(stderr,
		        "%s ConvertUtf8NToWChar(%s, %" PRIuz ", NULL, 0) expected %" PRIuz ", got %" PRIdz
		        "\n",
		        prefix, test->utf8, test->utf8len, wlen, rc2);
		return FALSE;
	}

	for (size_t x = 0; x < max; x++)
	{
		const size_t ilen[] = { TESTCASE_BUFFER_SIZE, test->utf8len, test->utf8len + 1,
			                    test->utf8len - 1 };
		const size_t imax = test->utf8len > 0 ? ARRAYSIZE(ilen) : ARRAYSIZE(ilen) - 1;

		for (size_t y = 0; y < imax; y++)
		{
			WCHAR buffer[TESTCASE_BUFFER_SIZE] = { 0 };
			SSIZE_T rc = ConvertUtf8NToWChar(test->utf8, ilen[x], buffer, len[x]);
			if (!compare_utf16(buffer, len[x], rc, ilen[x], test))
				return FALSE;
		}
	}
	return TRUE;
}

static BOOL test_convert_to_utf8(const testcase_t* test)
{
	const size_t len[] = { TESTCASE_BUFFER_SIZE, test->utf8len, test->utf8len + 1,
		                   test->utf8len - 1 };
	const size_t max = test->utf8len > 0 ? ARRAYSIZE(len) : ARRAYSIZE(len) - 1;

	const SSIZE_T rc2 = ConvertWCharToUtf8(test->utf16, NULL, 0);
	const size_t wlen = strnlen(test->utf8, test->utf8len);
	if ((rc2 < 0) || ((size_t)rc2 != wlen))
	{
		char prefix[8192] = { 0 };
		create_prefix(prefix, ARRAYSIZE(prefix), 0, rc2, -1, test, __FUNCTION__, __LINE__);
		fprintf(stderr, "%s ConvertWCharToUtf8(%s, NULL, 0) expected %" PRIuz ", got %" PRIdz "\n",
		        prefix, test->utf8, wlen, rc2);
		return FALSE;
	}

	for (size_t x = 0; x < max; x++)
	{
		char buffer[TESTCASE_BUFFER_SIZE] = { 0 };
		SSIZE_T rc = ConvertWCharToUtf8(test->utf16, buffer, len[x]);
		if (!compare_utf8(buffer, len[x], rc, -1, test))
			return FALSE;
	}

	return TRUE;
}

static BOOL test_convert_to_utf8_n(const testcase_t* test)
{
	const size_t len[] = { TESTCASE_BUFFER_SIZE, test->utf8len, test->utf8len + 1,
		                   test->utf8len - 1 };
	const size_t max = test->utf8len > 0 ? ARRAYSIZE(len) : ARRAYSIZE(len) - 1;

	const SSIZE_T rc2 = ConvertWCharNToUtf8(test->utf16, test->utf16len, NULL, 0);
	const size_t wlen = strnlen(test->utf8, test->utf8len);
	if ((rc2 < 0) || ((size_t)rc2 != wlen))
	{
		char prefix[8192] = { 0 };
		create_prefix(prefix, ARRAYSIZE(prefix), 0, rc2, test->utf16len, test, __FUNCTION__,
		              __LINE__);
		fprintf(stderr,
		        "%s ConvertWCharNToUtf8(%s, %" PRIuz ", NULL, 0) expected %" PRIuz ", got %" PRIdz
		        "\n",
		        prefix, test->utf8, test->utf16len, wlen, rc2);
		return FALSE;
	}

	for (size_t x = 0; x < max; x++)
	{
		const size_t ilen[] = { TESTCASE_BUFFER_SIZE, test->utf16len, test->utf16len + 1,
			                    test->utf16len - 1 };
		const size_t imax = test->utf16len > 0 ? ARRAYSIZE(ilen) : ARRAYSIZE(ilen) - 1;

		for (size_t y = 0; y < imax; y++)
		{
			char buffer[TESTCASE_BUFFER_SIZE] = { 0 };
			SSIZE_T rc = ConvertWCharNToUtf8(test->utf16, ilen[x], buffer, len[x]);
			if (!compare_utf8(buffer, len[x], rc, ilen[x], test))
				return FALSE;
		}
	}

	return TRUE;
}

static BOOL test_conversion(const testcase_t* testcases, size_t count)
{
	WINPR_ASSERT(testcases || (count == 0));
	for (size_t x = 0; x < count; x++)
	{
		const testcase_t* test = &testcases[x];

		printf("Running test case %" PRIuz " [%s]\n", x, test->utf8);
		if (!test_convert_to_utf16(test))
			return FALSE;
		if (!test_convert_to_utf16_n(test))
			return FALSE;
		if (!test_convert_to_utf8(test))
			return FALSE;
		if (!test_convert_to_utf8_n(test))
			return FALSE;
	}
	return TRUE;
}

#if defined(WITH_WINPR_DEPRECATED)

#define compare_win_utf16(what, buffersize, rc, inputlen, test) \
	compare_win_utf16_int((what), (buffersize), (rc), (inputlen), (test), __FUNCTION__, __LINE__)
static BOOL compare_win_utf16_int(const WCHAR* what, size_t buffersize, int rc, int inputlen,
                                  const testcase_t* test, const char* fkt, size_t line)
{
	char prefix[8192] = { 0 };
	create_prefix(prefix, ARRAYSIZE(prefix), buffersize, rc, inputlen, test, fkt, line);

	WINPR_ASSERT(what || (buffersize == 0));
	WINPR_ASSERT(test);

	BOOL isNullTerminated = TRUE;
	if (inputlen > 0)
		isNullTerminated = strnlen(test->utf8, inputlen) < inputlen;
	size_t welen = _wcsnlen(test->utf16, buffersize);
	if (isNullTerminated)
		welen++;

	if (buffersize >= welen)
	{
		if ((inputlen >= 0) && (rc > buffersize))
		{
			fprintf(stderr, "%s length does not match expectation: %d > %" PRIuz "\n", prefix, rc,
			        buffersize);
			return FALSE;
		}
		else if ((inputlen < 0) && (rc != welen))
		{
			fprintf(stderr, "%s length does not match expectation: %d != %" PRIuz "\n", prefix, rc,
			        welen);
			return FALSE;
		}
	}
	else
	{
		if (!check_short_buffer(prefix, rc, buffersize, test, FALSE))
			return FALSE;
	}

	if ((rc > 0) && (buffersize > rc))
	{
		size_t wlen = _wcsnlen(what, buffersize);
		if (isNullTerminated)
			wlen++;
		if ((inputlen >= 0) && (buffersize < rc))
		{
			fprintf(stderr, "%s length does not match wcslen: %d > %" PRIuz "\n", prefix, rc,
			        buffersize);
			return FALSE;
		}
		else if ((inputlen < 0) && (welen > rc))
		{
			fprintf(stderr, "%s length does not match wcslen: %d < %" PRIuz "\n", prefix, rc, wlen);
			return FALSE;
		}
	}

	const size_t cmp_size = MIN(rc, test->utf16len) * sizeof(WCHAR);
	if (memcmp(test->utf16, what, cmp_size) != 0)
	{
		fprintf(stderr, "%s contents does not match expectations: TODO '%s' != '%s'\n", prefix,
		        test->utf8, test->utf8);
		return FALSE;
	}

	printf("%s success\n", prefix);

	return TRUE;
}

#define compare_win_utf8(what, buffersize, rc, inputlen, test) \
	compare_win_utf8_int((what), (buffersize), (rc), (inputlen), (test), __FUNCTION__, __LINE__)
static BOOL compare_win_utf8_int(const char* what, size_t buffersize, SSIZE_T rc, SSIZE_T inputlen,
                                 const testcase_t* test, const char* fkt, size_t line)
{
	char prefix[8192] = { 0 };
	create_prefix(prefix, ARRAYSIZE(prefix), buffersize, rc, inputlen, test, fkt, line);

	WINPR_ASSERT(what || (buffersize == 0));
	WINPR_ASSERT(test);

	BOOL isNullTerminated = TRUE;
	if (inputlen > 0)
		isNullTerminated = _wcsnlen(test->utf16, inputlen) < inputlen;

	size_t slen = strnlen(test->utf8, test->utf8len);
	if (isNullTerminated)
		slen++;

	if (buffersize > slen)
	{
		if ((inputlen >= 0) && (rc > buffersize))
		{
			fprintf(stderr, "%s length does not match expectation: %" PRIdz " > %" PRIuz "\n",
			        prefix, rc, buffersize);
			return FALSE;
		}
		else if ((inputlen < 0) && (rc != slen))
		{
			fprintf(stderr, "%s length does not match expectation: %" PRIdz " != %" PRIuz "\n",
			        prefix, rc, slen);
			return FALSE;
		}
	}
	else
	{
		if (!check_short_buffer(prefix, rc, buffersize, test, TRUE))
			return FALSE;
	}

	if ((rc > 0) && (buffersize > rc))
	{
		size_t wlen = strnlen(what, buffersize);
		if (isNullTerminated)
			wlen++;

		if (wlen > rc)
		{
			fprintf(stderr, "%s length does not match wcslen: %" PRIdz " < %" PRIuz "\n", prefix,
			        rc, wlen);
			return FALSE;
		}
	}

	const size_t cmp_size = MIN(test->utf8len, rc);
	if (memcmp(test->utf8, what, cmp_size) != 0)
	{
		fprintf(stderr, "%s contents does not match expectations: '%s' != '%s'\n", prefix, what,
		        test->utf8);
		return FALSE;
	}
	printf("%s success\n", prefix);

	return TRUE;
}
#endif

#if defined(WITH_WINPR_DEPRECATED)
static BOOL test_win_convert_to_utf16(const testcase_t* test)
{
	const size_t len[] = { TESTCASE_BUFFER_SIZE, test->utf16len, test->utf16len + 1,
		                   test->utf16len - 1 };
	const size_t max = test->utf16len > 0 ? ARRAYSIZE(len) : ARRAYSIZE(len) - 1;

	const int rc2 = MultiByteToWideChar(CP_UTF8, 0, test->utf8, -1, NULL, 0);
	const size_t wlen = _wcsnlen(test->utf16, test->utf16len);
	if (rc2 != wlen + 1)
	{
		char prefix[8192] = { 0 };
		create_prefix(prefix, ARRAYSIZE(prefix), 0, rc2, -1, test, __FUNCTION__, __LINE__);
		fprintf(stderr,
		        "%s MultiByteToWideChar(CP_UTF8, 0, %s, [-1], NULL, 0) expected %" PRIuz
		        ", got %d\n",
		        prefix, test->utf8, wlen + 1, rc2);
		return FALSE;
	}
	for (size_t x = 0; x < max; x++)
	{
		WCHAR buffer[TESTCASE_BUFFER_SIZE] = { 0 };
		const int rc = MultiByteToWideChar(CP_UTF8, 0, test->utf8, -1, buffer, len[x]);
		if (!compare_win_utf16(buffer, len[x], rc, -1, test))
			return FALSE;
	}

	return TRUE;
}

static BOOL test_win_convert_to_utf16_n(const testcase_t* test)
{
	const size_t len[] = { TESTCASE_BUFFER_SIZE, test->utf16len, test->utf16len + 1,
		                   test->utf16len - 1 };
	const size_t max = test->utf16len > 0 ? ARRAYSIZE(len) : ARRAYSIZE(len) - 1;

	BOOL isNullTerminated = strnlen(test->utf8, test->utf8len) < test->utf8len;
	const int rc2 = MultiByteToWideChar(CP_UTF8, 0, test->utf8, test->utf8len, NULL, 0);
	size_t wlen = _wcsnlen(test->utf16, test->utf16len);
	if (isNullTerminated)
		wlen++;

	if (rc2 != wlen)
	{
		char prefix[8192] = { 0 };
		create_prefix(prefix, ARRAYSIZE(prefix), 0, rc2, test->utf8len, test, __FUNCTION__,
		              __LINE__);
		fprintf(stderr,
		        "%s MultiByteToWideChar(CP_UTF8, 0, %s, %" PRIuz ", NULL, 0) expected %" PRIuz
		        ", got %d\n",
		        prefix, test->utf8, test->utf8len, wlen, rc2);
		return FALSE;
	}

	for (size_t x = 0; x < max; x++)
	{
		const size_t ilen[] = { TESTCASE_BUFFER_SIZE, test->utf8len, test->utf8len + 1,
			                    test->utf8len - 1 };
		const size_t imax = test->utf8len > 0 ? ARRAYSIZE(ilen) : ARRAYSIZE(ilen) - 1;

		for (size_t y = 0; y < imax; y++)
		{
			char mbuffer[TESTCASE_BUFFER_SIZE] = { 0 };
			WCHAR buffer[TESTCASE_BUFFER_SIZE] = { 0 };
			strncpy(mbuffer, test->utf8, test->utf8len);
			const int rc = MultiByteToWideChar(CP_UTF8, 0, mbuffer, ilen[x], buffer, len[x]);
			if (!compare_win_utf16(buffer, len[x], rc, ilen[x], test))
				return FALSE;
		}
	}
	return TRUE;
}
#endif

#if defined(WITH_WINPR_DEPRECATED)
static BOOL test_win_convert_to_utf8(const testcase_t* test)
{
	const size_t len[] = { TESTCASE_BUFFER_SIZE, test->utf8len, test->utf8len + 1,
		                   test->utf8len - 1 };
	const size_t max = test->utf8len > 0 ? ARRAYSIZE(len) : ARRAYSIZE(len) - 1;

	const int rc2 = WideCharToMultiByte(CP_UTF8, 0, test->utf16, -1, NULL, 0, NULL, NULL);
	const size_t wlen = strnlen(test->utf8, test->utf8len) + 1;
	if (rc2 != wlen)
	{
		char prefix[8192] = { 0 };
		create_prefix(prefix, ARRAYSIZE(prefix), 0, rc2, -1, test, __FUNCTION__, __LINE__);
		fprintf(stderr,
		        "%s WideCharToMultiByte(CP_UTF8, 0, %s, -1, NULL, 0, NULL, NULL) expected %" PRIuz
		        ", got %d\n",
		        prefix, test->utf8, wlen, rc2);
		return FALSE;
	}

	for (size_t x = 0; x < max; x++)
	{
		char buffer[TESTCASE_BUFFER_SIZE] = { 0 };
		int rc = WideCharToMultiByte(CP_UTF8, 0, test->utf16, -1, buffer, len[x], NULL, NULL);
		if (!compare_win_utf8(buffer, len[x], rc, -1, test))
			return FALSE;
	}

	return TRUE;
}

static BOOL test_win_convert_to_utf8_n(const testcase_t* test)
{
	const size_t len[] = { TESTCASE_BUFFER_SIZE, test->utf8len, test->utf8len + 1,
		                   test->utf8len - 1 };
	const size_t max = test->utf8len > 0 ? ARRAYSIZE(len) : ARRAYSIZE(len) - 1;

	const BOOL isNullTerminated = _wcsnlen(test->utf16, test->utf16len) < test->utf16len;
	const int rc2 =
	    WideCharToMultiByte(CP_UTF8, 0, test->utf16, test->utf16len, NULL, 0, NULL, NULL);
	size_t wlen = strnlen(test->utf8, test->utf8len);
	if (isNullTerminated)
		wlen++;

	if (rc2 != wlen)
	{
		char prefix[8192] = { 0 };
		create_prefix(prefix, ARRAYSIZE(prefix), 0, rc2, test->utf16len, test, __FUNCTION__,
		              __LINE__);
		fprintf(stderr,
		        "%s WideCharToMultiByte(CP_UTF8, 0, %s, %" PRIuz
		        ", NULL, 0, NULL, NULL) expected %" PRIuz ", got %d\n",
		        prefix, test->utf8, test->utf16len, wlen, rc2);
		return FALSE;
	}

	for (size_t x = 0; x < max; x++)
	{
		const size_t ilen[] = { TESTCASE_BUFFER_SIZE, test->utf16len, test->utf16len + 1,
			                    test->utf16len - 1 };
		const size_t imax = test->utf16len > 0 ? ARRAYSIZE(ilen) : ARRAYSIZE(ilen) - 1;

		for (size_t y = 0; y < imax; y++)
		{
			WCHAR wbuffer[TESTCASE_BUFFER_SIZE] = { 0 };
			char buffer[TESTCASE_BUFFER_SIZE] = { 0 };
			memcpy(wbuffer, test->utf16, test->utf16len * sizeof(WCHAR));
			const int rc =
			    WideCharToMultiByte(CP_UTF8, 0, wbuffer, ilen[x], buffer, len[x], NULL, NULL);
			if (!compare_win_utf8(buffer, len[x], rc, ilen[x], test))
				return FALSE;
		}
	}

	return TRUE;
}

static BOOL test_win_conversion(const testcase_t* testcases, size_t count)
{
	WINPR_ASSERT(testcases || (count == 0));
	for (size_t x = 0; x < count; x++)
	{
		const testcase_t* test = &testcases[x];

		printf("Running test case %" PRIuz " [%s]\n", x, test->utf8);
		if (!test_win_convert_to_utf16(test))
			return FALSE;
		if (!test_win_convert_to_utf16_n(test))
			return FALSE;
		if (!test_win_convert_to_utf8(test))
			return FALSE;
		if (!test_win_convert_to_utf8_n(test))
			return FALSE;
	}
	return TRUE;
}
#endif

#if defined(WITH_WINPR_DEPRECATED)
/* Letters */

static BYTE c_cedilla_UTF8[] = "\xC3\xA7\x00";
static BYTE c_cedilla_UTF16[] = "\xE7\x00\x00\x00";
static int c_cedilla_cchWideChar = 2;
static int c_cedilla_cbMultiByte = 3;

/* English */

static BYTE en_Hello_UTF8[] = "Hello\0";
static BYTE en_Hello_UTF16[] = "\x48\x00\x65\x00\x6C\x00\x6C\x00\x6F\x00\x00\x00";
static int en_Hello_cchWideChar = 6;
static int en_Hello_cbMultiByte = 6;

static BYTE en_HowAreYou_UTF8[] = "How are you?\0";
static BYTE en_HowAreYou_UTF16[] =
    "\x48\x00\x6F\x00\x77\x00\x20\x00\x61\x00\x72\x00\x65\x00\x20\x00"
    "\x79\x00\x6F\x00\x75\x00\x3F\x00\x00\x00";
static int en_HowAreYou_cchWideChar = 13;
static int en_HowAreYou_cbMultiByte = 13;

/* French */

static BYTE fr_Hello_UTF8[] = "Allo\0";
static BYTE fr_Hello_UTF16[] = "\x41\x00\x6C\x00\x6C\x00\x6F\x00\x00\x00";
static int fr_Hello_cchWideChar = 5;
static int fr_Hello_cbMultiByte = 5;

static BYTE fr_HowAreYou_UTF8[] =
    "\x43\x6F\x6D\x6D\x65\x6E\x74\x20\xC3\xA7\x61\x20\x76\x61\x3F\x00";
static BYTE fr_HowAreYou_UTF16[] =
    "\x43\x00\x6F\x00\x6D\x00\x6D\x00\x65\x00\x6E\x00\x74\x00\x20\x00"
    "\xE7\x00\x61\x00\x20\x00\x76\x00\x61\x00\x3F\x00\x00\x00";
static int fr_HowAreYou_cchWideChar = 15;
static int fr_HowAreYou_cbMultiByte = 16;

/* Russian */

static BYTE ru_Hello_UTF8[] = "\xD0\x97\xD0\xB4\xD0\xBE\xD1\x80\xD0\xBE\xD0\xB2\xD0\xBE\x00";
static BYTE ru_Hello_UTF16[] = "\x17\x04\x34\x04\x3E\x04\x40\x04\x3E\x04\x32\x04\x3E\x04\x00\x00";
static int ru_Hello_cchWideChar = 8;
static int ru_Hello_cbMultiByte = 15;

static BYTE ru_HowAreYou_UTF8[] =
    "\xD0\x9A\xD0\xB0\xD0\xBA\x20\xD0\xB4\xD0\xB5\xD0\xBB\xD0\xB0\x3F\x00";
static BYTE ru_HowAreYou_UTF16[] =
    "\x1A\x04\x30\x04\x3A\x04\x20\x00\x34\x04\x35\x04\x3B\x04\x30\x04"
    "\x3F\x00\x00\x00";
static int ru_HowAreYou_cchWideChar = 10;
static int ru_HowAreYou_cbMultiByte = 17;

/* Arabic */

static BYTE ar_Hello_UTF8[] = "\xD8\xA7\xD9\x84\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85\x20\xD8\xB9\xD9"
                              "\x84\xD9\x8A\xD9\x83\xD9\x85\x00";
static BYTE ar_Hello_UTF16[] = "\x27\x06\x44\x06\x33\x06\x44\x06\x27\x06\x45\x06\x20\x00\x39\x06"
                               "\x44\x06\x4A\x06\x43\x06\x45\x06\x00\x00";
static int ar_Hello_cchWideChar = 13;
static int ar_Hello_cbMultiByte = 24;

static BYTE ar_HowAreYou_UTF8[] = "\xD9\x83\xD9\x8A\xD9\x81\x20\xD8\xAD\xD8\xA7\xD9\x84\xD9\x83\xD8"
                                  "\x9F\x00";
static BYTE ar_HowAreYou_UTF16[] =
    "\x43\x06\x4A\x06\x41\x06\x20\x00\x2D\x06\x27\x06\x44\x06\x43\x06"
    "\x1F\x06\x00\x00";
static int ar_HowAreYou_cchWideChar = 10;
static int ar_HowAreYou_cbMultiByte = 18;

/* Chinese */

static BYTE ch_Hello_UTF8[] = "\xE4\xBD\xA0\xE5\xA5\xBD\x00";
static BYTE ch_Hello_UTF16[] = "\x60\x4F\x7D\x59\x00\x00";
static int ch_Hello_cchWideChar = 3;
static int ch_Hello_cbMultiByte = 7;

static BYTE ch_HowAreYou_UTF8[] = "\xE4\xBD\xA0\xE5\xA5\xBD\xE5\x90\x97\x00";
static BYTE ch_HowAreYou_UTF16[] = "\x60\x4F\x7D\x59\x17\x54\x00\x00";
static int ch_HowAreYou_cchWideChar = 4;
static int ch_HowAreYou_cbMultiByte = 10;

/* Uppercasing */

static BYTE ru_Administrator_lower[] = "\xd0\x90\xd0\xb4\xd0\xbc\xd0\xb8\xd0\xbd\xd0\xb8\xd1\x81"
                                       "\xd1\x82\xd1\x80\xd0\xb0\xd1\x82\xd0\xbe\xd1\x80\x00";

static BYTE ru_Administrator_upper[] = "\xd0\x90\xd0\x94\xd0\x9c\xd0\x98\xd0\x9d\xd0\x98\xd0\xa1"
                                       "\xd0\xa2\xd0\xa0\xd0\x90\xd0\xa2\xd0\x9e\xd0\xa0\x00";

static void string_hexdump(const BYTE* data, size_t length)
{
	size_t offset = 0;

	char* str = winpr_BinToHexString(data, length, TRUE);
	if (!str)
		return;

	while (offset < length)
	{
		const size_t diff = (length - offset) * 3;
		WINPR_ASSERT(diff <= INT_MAX);
		printf("%04" PRIxz " %.*s\n", offset, (int)diff, &str[offset]);
		offset += 16;
	}

	free(str);
}

static int convert_utf8_to_utf16(BYTE* lpMultiByteStr, BYTE* expected_lpWideCharStr,
                                 int expected_cchWideChar)
{
	int rc = -1;
	int length;
	size_t cbMultiByte;
	int cchWideChar;
	LPWSTR lpWideCharStr = NULL;

	cbMultiByte = strlen((char*)lpMultiByteStr);
	cchWideChar = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)lpMultiByteStr, -1, NULL, 0);

	printf("MultiByteToWideChar Input UTF8 String:\n");
	string_hexdump(lpMultiByteStr, cbMultiByte + 1);

	printf("MultiByteToWideChar required cchWideChar: %d\n", cchWideChar);

	if (cchWideChar != expected_cchWideChar)
	{
		printf("MultiByteToWideChar unexpected cchWideChar: actual: %d expected: %d\n", cchWideChar,
		       expected_cchWideChar);
		goto fail;
	}

	lpWideCharStr = (LPWSTR)calloc((size_t)cchWideChar, sizeof(WCHAR));
	if (!lpWideCharStr)
	{
		printf("MultiByteToWideChar: unable to allocate memory for test\n");
		goto fail;
	}
	lpWideCharStr[cchWideChar - 1] =
	    0xFFFF; /* should be overwritten if null terminator is inserted properly */
	length = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)lpMultiByteStr, cbMultiByte + 1, lpWideCharStr,
	                             cchWideChar);

	printf("MultiByteToWideChar converted length (WCHAR): %d\n", length);

	if (!length)
	{
		DWORD error = GetLastError();
		printf("MultiByteToWideChar error: 0x%08" PRIX32 "\n", error);
		goto fail;
	}

	if (length != expected_cchWideChar)
	{
		printf("MultiByteToWideChar unexpected converted length (WCHAR): actual: %d expected: %d\n",
		       length, expected_cchWideChar);
		goto fail;
	}

	if (_wcscmp(lpWideCharStr, (WCHAR*)expected_lpWideCharStr) != 0)
	{
		printf("MultiByteToWideChar unexpected string:\n");

		printf("UTF8 String:\n");
		string_hexdump(lpMultiByteStr, cbMultiByte + 1);

		printf("UTF16 String (actual):\n");
		string_hexdump((BYTE*)lpWideCharStr, length * sizeof(WCHAR));

		printf("UTF16 String (expected):\n");
		string_hexdump((BYTE*)expected_lpWideCharStr, expected_cchWideChar * sizeof(WCHAR));

		goto fail;
	}

	printf("MultiByteToWideChar Output UTF16 String:\n");
	string_hexdump((BYTE*)lpWideCharStr, length * sizeof(WCHAR));
	printf("\n");

	rc = length;
fail:
	free(lpWideCharStr);

	return rc;
}
#endif

#if defined(WITH_WINPR_DEPRECATED)
static int convert_utf16_to_utf8(BYTE* lpWideCharStr, BYTE* expected_lpMultiByteStr,
                                 int expected_cbMultiByte)
{
	int rc = -1;
	int length;
	int cchWideChar;
	int cbMultiByte;
	LPSTR lpMultiByteStr = NULL;

	cchWideChar = _wcslen((WCHAR*)lpWideCharStr);
	cbMultiByte = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)lpWideCharStr, -1, NULL, 0, NULL, NULL);

	printf("WideCharToMultiByte Input UTF16 String:\n");
	string_hexdump(lpWideCharStr, (cchWideChar + 1) * sizeof(WCHAR));

	printf("WideCharToMultiByte required cbMultiByte: %d\n", cbMultiByte);

	if (cbMultiByte != expected_cbMultiByte)
	{
		printf("WideCharToMultiByte unexpected cbMultiByte: actual: %d expected: %d\n", cbMultiByte,
		       expected_cbMultiByte);
		goto fail;
	}

	lpMultiByteStr = (LPSTR)malloc(cbMultiByte);
	if (!lpMultiByteStr)
	{
		printf("WideCharToMultiByte: unable to allocate memory for test\n");
		goto fail;
	}
	lpMultiByteStr[cbMultiByte - 1] =
	    (CHAR)0xFF; /* should be overwritten if null terminator is inserted properly */
	length = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)lpWideCharStr, cchWideChar + 1,
	                             lpMultiByteStr, cbMultiByte, NULL, NULL);

	printf("WideCharToMultiByte converted length (BYTE): %d\n", length);

	if (!length)
	{
		DWORD error = GetLastError();
		printf("WideCharToMultiByte error: 0x%08" PRIX32 "\n", error);
		goto fail;
	}

	if (length != expected_cbMultiByte)
	{
		printf("WideCharToMultiByte unexpected converted length (BYTE): actual: %d expected: %d\n",
		       length, expected_cbMultiByte);
		goto fail;
	}

	if (strcmp(lpMultiByteStr, (char*)expected_lpMultiByteStr) != 0)
	{
		printf("WideCharToMultiByte unexpected string:\n");

		printf("UTF16 String:\n");
		string_hexdump((BYTE*)lpWideCharStr, (cchWideChar + 1) * sizeof(WCHAR));

		printf("UTF8 String (actual):\n");
		string_hexdump((BYTE*)lpMultiByteStr, cbMultiByte);

		printf("UTF8 String (expected):\n");
		string_hexdump((BYTE*)expected_lpMultiByteStr, expected_cbMultiByte);

		goto fail;
	}

	printf("WideCharToMultiByte Output UTF8 String:\n");
	string_hexdump((BYTE*)lpMultiByteStr, cbMultiByte);
	printf("\n");

	rc = length;
fail:
	free(lpMultiByteStr);

	return rc;
}
#endif

#if defined(WITH_WINPR_DEPRECATED)
static BOOL test_unicode_uppercasing(BYTE* lower, BYTE* upper)
{
	WCHAR* lowerW = NULL;
	int lowerLength;
	WCHAR* upperW = NULL;
	int upperLength;

	lowerLength = ConvertToUnicode(CP_UTF8, 0, (LPSTR)lower, -1, &lowerW, 0);
	upperLength = ConvertToUnicode(CP_UTF8, 0, (LPSTR)upper, -1, &upperW, 0);

	CharUpperBuffW(lowerW, lowerLength);

	if (_wcscmp(lowerW, upperW) != 0)
	{
		printf("Lowercase String:\n");
		string_hexdump((BYTE*)lowerW, lowerLength * 2);

		printf("Uppercase String:\n");
		string_hexdump((BYTE*)upperW, upperLength * 2);

		return FALSE;
	}

	free(lowerW);
	free(upperW);

	printf("success\n\n");
	return TRUE;
}
#endif

#if defined(WITH_WINPR_DEPRECATED)
static BOOL test_ConvertFromUnicode_wrapper(void)
{
	const BYTE src1[] =
	    "\x52\x00\x49\x00\x43\x00\x48\x00\x20\x00\x54\x00\x45\x00\x58\x00\x54\x00\x20\x00\x46\x00"
	    "\x4f\x00\x52\x00\x4d\x00\x41\x00\x54\x00\x40\x00\x40\x00\x40\x00";
	const BYTE src2[] = "\x52\x00\x49\x00\x43\x00\x48\x00\x20\x00\x54\x00\x45\x00\x58\x00\x54\x00"
	                    "\x20\x00\x46\x00\x4f\x00\x52\x00\x4d\x00\x41\x00\x54\x00\x00\x00";
	/*               00  01  02  03  04  05  06  07  08  09  10  11  12  13  14  15  16  17  18 */
	const CHAR cmp0[] = { 'R', 'I', 'C', 'H', ' ', 'T', 'E', 'X', 'T',
		                  ' ', 'F', 'O', 'R', 'M', 'A', 'T', 0 };
	CHAR* dst = NULL;
	int i;

	/* Test unterminated unicode string:
	 * ConvertFromUnicode must always null-terminate, even if the src string isn't
	 */

	printf("Input UTF16 String:\n");
	string_hexdump((const BYTE*)src1, 19 * sizeof(WCHAR));

	i = ConvertFromUnicode(CP_UTF8, 0, (const WCHAR*)src1, 16, &dst, 0, NULL, NULL);
	if (i != 16)
	{
		fprintf(stderr, "ConvertFromUnicode failure A1: unexpectedly returned %d instead of 16\n",
		        i);
		goto fail;
	}
	if (dst == NULL)
	{
		fprintf(stderr, "ConvertFromUnicode failure A2: destination ist NULL\n");
		goto fail;
	}
	if ((i = strlen(dst)) != 16)
	{
		fprintf(stderr, "ConvertFromUnicode failure A3: dst length is %d instead of 16\n", i);
		goto fail;
	}
	if (strcmp(dst, cmp0))
	{
		fprintf(stderr, "ConvertFromUnicode failure A4: data mismatch\n");
		goto fail;
	}
	printf("Output UTF8 String:\n");
	string_hexdump((BYTE*)dst, i + 1);

	free(dst);
	dst = NULL;

	/* Test null-terminated string */

	printf("Input UTF16 String:\n");
	string_hexdump((const BYTE*)src2, (_wcslen((const WCHAR*)src2) + 1) * sizeof(WCHAR));

	i = ConvertFromUnicode(CP_UTF8, 0, (const WCHAR*)src2, -1, &dst, 0, NULL, NULL);
	if (i != 17)
	{
		fprintf(stderr, "ConvertFromUnicode failure B1: unexpectedly returned %d instead of 17\n",
		        i);
		goto fail;
	}
	if (dst == NULL)
	{
		fprintf(stderr, "ConvertFromUnicode failure B2: destination ist NULL\n");
		goto fail;
	}
	if ((i = strlen(dst)) != 16)
	{
		fprintf(stderr, "ConvertFromUnicode failure B3: dst length is %d instead of 16\n", i);
		goto fail;
	}
	if (strcmp(dst, cmp0))
	{
		fprintf(stderr, "ConvertFromUnicode failure B: data mismatch\n");
		goto fail;
	}
	printf("Output UTF8 String:\n");
	string_hexdump((BYTE*)dst, i + 1);

	free(dst);
	dst = NULL;

	printf("success\n\n");

	return TRUE;

fail:
	free(dst);
	return FALSE;
}

static BOOL test_ConvertToUnicode_wrapper(void)
{
	/*               00  01  02  03  04  05  06  07  08  09  10  11  12  13  14  15  16  17  18 */
	const CHAR src1[] = { 'R', 'I', 'C', 'H', ' ', 'T', 'E', 'X', 'T', ' ',
		                  'F', 'O', 'R', 'M', 'A', 'T', '@', '@', '@' };
	const CHAR src2[] = { 'R', 'I', 'C', 'H', ' ', 'T', 'E', 'X', 'T',
		                  ' ', 'F', 'O', 'R', 'M', 'A', 'T', 0 };
	const BYTE cmp0[] = "\x52\x00\x49\x00\x43\x00\x48\x00\x20\x00\x54\x00\x45\x00\x58\x00\x54\x00"
	                    "\x20\x00\x46\x00\x4f\x00\x52\x00\x4d\x00\x41\x00\x54\x00\x00\x00";
	WCHAR* dst = NULL;
	int ii;
	size_t i;

	/* Test static string buffers of differing sizes */
	{
		char name[] = "someteststring";
		const BYTE cmp[] = { 's', 0, 'o', 0, 'm', 0, 'e', 0, 't', 0, 'e', 0, 's', 0, 't', 0,
			                 's', 0, 't', 0, 'r', 0, 'i', 0, 'n', 0, 'g', 0, 0,   0 };
		WCHAR xname[128] = { 0 };
		LPWSTR aname = NULL;
		LPWSTR wname = &xname[0];
		const size_t len = strnlen(name, ARRAYSIZE(name) - 1);
		ii = ConvertToUnicode(CP_UTF8, 0, name, len, &wname, ARRAYSIZE(xname));
		if (ii != (SSIZE_T)len)
			goto fail;

		if (memcmp(wname, cmp, sizeof(cmp)) != 0)
			goto fail;

		ii = ConvertToUnicode(CP_UTF8, 0, name, len, &aname, 0);
		if (ii != (SSIZE_T)len)
			goto fail;
		ii = memcmp(aname, cmp, sizeof(cmp));
		free(aname);
		if (ii != 0)
			goto fail;
	}

	/* Test unterminated unicode string:
	 * ConvertToUnicode must always null-terminate, even if the src string isn't
	 */

	printf("Input UTF8 String:\n");
	string_hexdump((const BYTE*)src1, 19);

	ii = ConvertToUnicode(CP_UTF8, 0, src1, 16, &dst, 0);
	if (ii != 16)
	{
		fprintf(stderr, "ConvertToUnicode failure A1: unexpectedly returned %d instead of 16\n",
		        ii);
		goto fail;
	}
	i = (size_t)ii;
	if (dst == NULL)
	{
		fprintf(stderr, "ConvertToUnicode failure A2: destination ist NULL\n");
		goto fail;
	}
	if ((i = _wcslen(dst)) != 16)
	{
		fprintf(stderr, "ConvertToUnicode failure A3: dst length is %" PRIuz " instead of 16\n", i);
		goto fail;
	}
	if (_wcscmp(dst, (const WCHAR*)cmp0))
	{
		fprintf(stderr, "ConvertToUnicode failure A4: data mismatch\n");
		goto fail;
	}
	printf("Output UTF16 String:\n");
	string_hexdump((const BYTE*)dst, (i + 1) * sizeof(WCHAR));

	free(dst);
	dst = NULL;

	/* Test null-terminated string */

	printf("Input UTF8 String:\n");
	string_hexdump((const BYTE*)src2, strlen(src2) + 1);

	i = ConvertToUnicode(CP_UTF8, 0, src2, -1, &dst, 0);
	if (i != 17)
	{
		fprintf(stderr,
		        "ConvertToUnicode failure B1: unexpectedly returned %" PRIuz " instead of 17\n", i);
		goto fail;
	}
	if (dst == NULL)
	{
		fprintf(stderr, "ConvertToUnicode failure B2: destination ist NULL\n");
		goto fail;
	}
	if ((i = _wcslen(dst)) != 16)
	{
		fprintf(stderr, "ConvertToUnicode failure B3: dst length is %" PRIuz " instead of 16\n", i);
		goto fail;
	}
	if (_wcscmp(dst, (const WCHAR*)cmp0))
	{
		fprintf(stderr, "ConvertToUnicode failure B: data mismatch\n");
		goto fail;
	}
	printf("Output UTF16 String:\n");
	string_hexdump((BYTE*)dst, (i + 1) * 2);

	free(dst);
	dst = NULL;

	printf("success\n\n");

	return TRUE;

fail:
	free(dst);
	return FALSE;
}
#endif

int TestUnicodeConversion(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!test_conversion(unit_testcases, ARRAYSIZE(unit_testcases)))
		return -1;

#if defined(WITH_WINPR_DEPRECATED)
	if (!test_win_conversion(unit_testcases, ARRAYSIZE(unit_testcases)))
		return -1;

	/* Letters */

	printf("Letters\n");

	if (convert_utf8_to_utf16(c_cedilla_UTF8, c_cedilla_UTF16, c_cedilla_cchWideChar) < 1)
		return -1;

	if (convert_utf16_to_utf8(c_cedilla_UTF16, c_cedilla_UTF8, c_cedilla_cbMultiByte) < 1)
		return -1;

	/* English */

	printf("English\n");

	if (convert_utf8_to_utf16(en_Hello_UTF8, en_Hello_UTF16, en_Hello_cchWideChar) < 1)
		return -1;
	if (convert_utf8_to_utf16(en_HowAreYou_UTF8, en_HowAreYou_UTF16, en_HowAreYou_cchWideChar) < 1)
		return -1;

	if (convert_utf16_to_utf8(en_Hello_UTF16, en_Hello_UTF8, en_Hello_cbMultiByte) < 1)
		return -1;
	if (convert_utf16_to_utf8(en_HowAreYou_UTF16, en_HowAreYou_UTF8, en_HowAreYou_cbMultiByte) < 1)
		return -1;

	/* French */

	printf("French\n");

	if (convert_utf8_to_utf16(fr_Hello_UTF8, fr_Hello_UTF16, fr_Hello_cchWideChar) < 1)
		return -1;
	if (convert_utf8_to_utf16(fr_HowAreYou_UTF8, fr_HowAreYou_UTF16, fr_HowAreYou_cchWideChar) < 1)
		return -1;

	if (convert_utf16_to_utf8(fr_Hello_UTF16, fr_Hello_UTF8, fr_Hello_cbMultiByte) < 1)
		return -1;
	if (convert_utf16_to_utf8(fr_HowAreYou_UTF16, fr_HowAreYou_UTF8, fr_HowAreYou_cbMultiByte) < 1)
		return -1;

	/* Russian */

	printf("Russian\n");

	if (convert_utf8_to_utf16(ru_Hello_UTF8, ru_Hello_UTF16, ru_Hello_cchWideChar) < 1)
		return -1;
	if (convert_utf8_to_utf16(ru_HowAreYou_UTF8, ru_HowAreYou_UTF16, ru_HowAreYou_cchWideChar) < 1)
		return -1;

	if (convert_utf16_to_utf8(ru_Hello_UTF16, ru_Hello_UTF8, ru_Hello_cbMultiByte) < 1)
		return -1;
	if (convert_utf16_to_utf8(ru_HowAreYou_UTF16, ru_HowAreYou_UTF8, ru_HowAreYou_cbMultiByte) < 1)
		return -1;

	/* Arabic */

	printf("Arabic\n");

	if (convert_utf8_to_utf16(ar_Hello_UTF8, ar_Hello_UTF16, ar_Hello_cchWideChar) < 1)
		return -1;
	if (convert_utf8_to_utf16(ar_HowAreYou_UTF8, ar_HowAreYou_UTF16, ar_HowAreYou_cchWideChar) < 1)
		return -1;

	if (convert_utf16_to_utf8(ar_Hello_UTF16, ar_Hello_UTF8, ar_Hello_cbMultiByte) < 1)
		return -1;
	if (convert_utf16_to_utf8(ar_HowAreYou_UTF16, ar_HowAreYou_UTF8, ar_HowAreYou_cbMultiByte) < 1)
		return -1;

	/* Chinese */

	printf("Chinese\n");

	if (convert_utf8_to_utf16(ch_Hello_UTF8, ch_Hello_UTF16, ch_Hello_cchWideChar) < 1)
		return -1;
	if (convert_utf8_to_utf16(ch_HowAreYou_UTF8, ch_HowAreYou_UTF16, ch_HowAreYou_cchWideChar) < 1)
		return -1;

	if (convert_utf16_to_utf8(ch_Hello_UTF16, ch_Hello_UTF8, ch_Hello_cbMultiByte) < 1)
		return -1;
	if (convert_utf16_to_utf8(ch_HowAreYou_UTF16, ch_HowAreYou_UTF8, ch_HowAreYou_cbMultiByte) < 1)
		return -1;

#endif

		/* Uppercasing */
#if defined(WITH_WINPR_DEPRECATED)
	printf("Uppercasing\n");

	if (!test_unicode_uppercasing(ru_Administrator_lower, ru_Administrator_upper))
		return -1;
#endif

		/* ConvertFromUnicode */
#if defined(WITH_WINPR_DEPRECATED)
	printf("ConvertFromUnicode\n");

	if (!test_ConvertFromUnicode_wrapper())
		return -1;

	/* ConvertToUnicode */

	printf("ConvertToUnicode\n");

	if (!test_ConvertToUnicode_wrapper())
		return -1;
#endif
	/*

	    printf("----------------------------------------------------------\n\n");

	    if (0)
	    {
	        BYTE src[] = { 'R',0,'I',0,'C',0,'H',0,' ',0, 'T',0,'E',0,'X',0,'T',0,'
	   ',0,'F',0,'O',0,'R',0,'M',0,'A',0,'T',0,'@',0,'@',0 };
	        //BYTE src[] = { 'R',0,'I',0,'C',0,'H',0,' ',0,  0,0,  'T',0,'E',0,'X',0,'T',0,'
	   ',0,'F',0,'O',0,'R',0,'M',0,'A',0,'T',0,'@',0,'@',0 };
	        //BYTE src[] = { 0,0,'R',0,'I',0,'C',0,'H',0,' ',0, 'T',0,'E',0,'X',0,'T',0,'
	   ',0,'F',0,'O',0,'R',0,'M',0,'A',0,'T',0,'@',0,'@',0 }; char* dst = NULL; int num; num =
	   ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) src, 16, &dst, 0, NULL, NULL);
	        printf("ConvertFromUnicode returned %d dst=[%s]\n", num, dst);
	        string_hexdump((BYTE*)dst, num+1);
	    }
	    if (1)
	    {
	        char src[] = "RICH TEXT FORMAT@@@@@@";
	        WCHAR *dst = NULL;
	        int num;
	        num = ConvertToUnicode(CP_UTF8, 0, src, 16, &dst, 0);
	        printf("ConvertToUnicode returned %d dst=%p\n", num, (void*) dst);
	        string_hexdump((BYTE*)dst, num * 2 + 2);

	    }
	*/

	return 0;
}
