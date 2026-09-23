#include <winpr/wtypes.h>
#include <winpr/ntlm.h>
#include <winpr/string.h>
#include <winpr/print.h>
#include <winpr/ssl.h>

typedef struct
{
	const char* password;
	const BYTE expected[16];
	const BOOL result;
	const BOOL shouldMatch;
} test_case_ntowf1_t;

typedef struct
{
	const char* password;
	const char* user;
	const char* domain;
	const BYTE expected[16];
	const BOOL result;
	const BOOL shouldMatch;
} test_case_ntowf2_t;

typedef struct
{
	const char* user;
	const char* domain;
	const BYTE passwordhash[16];
	const BYTE expected[16];
	const BOOL result;
	const BOOL shouldMatch;
} test_case_from_hash_t;

static const test_case_ntowf1_t ntofw1tests[] = {
	{ "foo1", { 0 }, TRUE, FALSE },
	{ "foo1",
	  { 0x05, 0x80, 0x83, 0x4a, 0x04, 0x89, 0xed, 0x37, 0x49, 0x6b, 0x7b, 0xfa, 0xe9, 0x90, 0x9b,
	    0x70 },
	  TRUE,
	  TRUE },
	{ "bar2",
	  { 0xee, 0xd4, 0xd3, 0xb5, 0x87, 0x11, 0x0d, 0x5c, 0x08, 0x51, 0x8c, 0xea, 0xa5, 0x69, 0x1e,
	    0xd2 },
	  TRUE,
	  TRUE },
	{ nullptr, { 0 }, FALSE, FALSE }
};

static const test_case_ntowf2_t ntofw2tests[] = {
	{ nullptr, nullptr, nullptr, { 0 }, FALSE, FALSE },
	{ nullptr, nullptr, "domain1", { 0 }, FALSE, FALSE },
	{ nullptr, "user1", "domain1", { 0 }, FALSE, FALSE },
	{ "pwd1", "user1", "domain1", { 0 }, TRUE, FALSE },
	{ "pwd1",
	  "user1",
	  "domain1",
	  { 0x9b, 0xee, 0x1e, 0x41, 0xa6, 0xd3, 0xf1, 0xf2, 0xc4, 0x00, 0xb8, 0xf9, 0x44, 0xc5, 0x44,
	    0x92 },
	  TRUE,
	  TRUE },
	{ "pwd2",
	  "user2",
	  "domain1",
	  { 0xa5, 0x82, 0xcd, 0x70, 0xce, 0xb0, 0xed, 0x99, 0xe6, 0xf5, 0x45, 0x3e, 0x96, 0x93, 0x5d,
	    0xa5 },
	  TRUE,
	  TRUE },
	{ "pwd3",
	  "user2",
	  "domain2",
	  { 0xf9, 0x4d, 0x1e, 0x10, 0x31, 0xa1, 0x96, 0x0d, 0xa9, 0xaa, 0x64, 0xb3, 0x83, 0x1a, 0x79,
	    0xf7 },
	  TRUE,
	  TRUE }
};

#define hashV1_test1                                    \
	{                                                   \
		0x47, 0x62, 0x01, 0xda, 0x81, 0xd4, 0x48, 0xac, \
		0x69, 0x02, 0x72, 0x46, 0x3b, 0x75, 0xb3, 0xac  \
	}
#define hashV1_test2                                    \
	{                                                   \
		0x76, 0xc2, 0x51, 0x01, 0x35, 0x27, 0x08, 0x8b, \
		0xc6, 0x4c, 0x1b, 0xd4, 0x2c, 0x78, 0x8b, 0xac  \
	}
#define hashV1_test1_foobar1                            \
	{                                                   \
		0x2f, 0xb7, 0x06, 0xe3, 0xf4, 0xf2, 0x74, 0xe7, \
		0x62, 0xfc, 0x53, 0xcb, 0x57, 0x9a, 0x28, 0x3a  \
	}
#define hashV1_test1_foobar1_domain1                    \
	{                                                   \
		0xa2, 0xbc, 0x65, 0x8e, 0xda, 0xfc, 0xc2, 0xef, \
		0xaa, 0x26, 0x41, 0x07, 0xb6, 0x9f, 0x94, 0x5c  \
	}
#define hashV1_test1_foobar1_domain2                    \
	{                                                   \
		0xcf, 0x76, 0xd4, 0xe0, 0xe7, 0x8d, 0xc7, 0xec, \
		0x59, 0x48, 0x9f, 0x47, 0xde, 0x52, 0xbd, 0x5c  \
	}
#define hashV1_test2_foobar1                            \
	{                                                   \
		0x4e, 0xe8, 0xd8, 0xe1, 0xfd, 0x7a, 0x77, 0x2b, \
		0xe7, 0x7c, 0x06, 0xe3, 0xca, 0x6e, 0x47, 0xb6  \
	}
#define hashV1_test2_foobar1_domain1                    \
	{                                                   \
		0x40, 0x74, 0xd1, 0x98, 0x32, 0x38, 0xdb, 0x3f, \
		0x2d, 0x18, 0x1c, 0xd5, 0x0f, 0xb0, 0x70, 0xd2  \
	}
#define hashV1_test2_foobar1_domain2                    \
	{                                                   \
		0x2e, 0x63, 0x11, 0x29, 0x4f, 0x47, 0x3e, 0x87, \
		0x72, 0x07, 0x36, 0x52, 0x20, 0xb3, 0xbe, 0x46  \
	}

static const test_case_from_hash_t ntofw2fromhashtests[] = {
	{ nullptr, nullptr, { 0 }, { 0 }, FALSE, FALSE },
	{ "foobar1", nullptr, hashV1_test1, { 0 }, TRUE, FALSE },
	{ "foobar1", nullptr, hashV1_test2, hashV1_test1_foobar1, TRUE, FALSE },
	{ "foobar1", nullptr, hashV1_test1, hashV1_test1_foobar1, TRUE, TRUE },
	{ "foobar2", nullptr, hashV1_test1, hashV1_test1_foobar1, TRUE, FALSE },
	{ "foobar1", "domain1", hashV1_test1, hashV1_test1_foobar1_domain1, TRUE, TRUE },
	{ "foobar1", "domain2", hashV1_test1, hashV1_test1_foobar1_domain1, TRUE, FALSE },
	{ "foobar1", "domain2", hashV1_test1, hashV1_test1_foobar1_domain2, TRUE, TRUE },
	{ "foobar1", nullptr, hashV1_test2, { 0 }, TRUE, FALSE },
	{ "foobar1", nullptr, hashV1_test1, hashV1_test2_foobar1, TRUE, FALSE },
	{ "foobar1", nullptr, hashV1_test2, hashV1_test2_foobar1, TRUE, TRUE },
	{ "foobar2", nullptr, hashV1_test2, hashV1_test2_foobar1, TRUE, FALSE },
	{ "foobar1", "domain1", hashV1_test2, hashV1_test2_foobar1_domain1, TRUE, TRUE },
	{ "foobar1", "domain2", hashV1_test2, hashV1_test2_foobar1_domain1, TRUE, FALSE },
	{ "foobar1", "domain2", hashV1_test2, hashV1_test2_foobar1_domain2, TRUE, TRUE }
};

static BOOL testNTOWF1(size_t x, const test_case_ntowf1_t* test)
{
	WINPR_ASSERT(test);

	BYTE hashA[sizeof(test->expected)] = WINPR_C_ARRAY_INIT;
	BYTE hashW[sizeof(test->expected)] = WINPR_C_ARRAY_INIT;
	size_t pwdlen = 0;
	if (test->password)
		pwdlen = strlen(test->password);

	const BOOL rcA = NTOWFv1A(test->password, pwdlen, hashA);

	WCHAR* passwordW = nullptr;
	size_t pwdwlen = 0;
	if (pwdlen > 0)
		passwordW = ConvertUtf8NToWCharAlloc(test->password, pwdlen, &pwdwlen);
	const BOOL rcW = NTOWFv1W(passwordW, pwdwlen * sizeof(WCHAR), hashW);
	free(passwordW);

	winpr_HexDump("NTOWFv1A", WLOG_INFO, hashA, sizeof(hashA));
	winpr_HexDump("expect", WLOG_INFO, test->expected, sizeof(test->expected));
	WLog_INFO("result", "[%" PRIuz "] got %d, expected %d", x, rcA, test->result);

	winpr_HexDump("NTOWFv1W", WLOG_INFO, hashW, sizeof(hashW));
	winpr_HexDump("expect", WLOG_INFO, test->expected, sizeof(test->expected));
	WLog_INFO("result", "[%" PRIuz "] got %d, expected %d", x, rcW, test->result);

	if (rcA != test->result)
		return FALSE;

	if (memcmp(test->expected, hashA, sizeof(test->expected)) != 0)
	{
		if (test->shouldMatch)
			return FALSE;
	}

	if (rcW != test->result)
		return FALSE;

	if (memcmp(test->expected, hashW, sizeof(test->expected)) != 0)
	{
		if (test->shouldMatch)
			return FALSE;
	}
	return TRUE;
}

static BOOL testNTOWF2(size_t x, const test_case_ntowf2_t* test)
{
	WINPR_ASSERT(test);

	BYTE hashA[sizeof(test->expected)] = WINPR_C_ARRAY_INIT;
	BYTE hashW[sizeof(test->expected)] = WINPR_C_ARRAY_INIT;

	size_t pwdlen = 0;
	if (test->password)
		pwdlen = strlen(test->password);

	size_t userlen = 0;
	if (test->user)
		userlen = strlen(test->user);

	size_t domainlen = 0;
	if (test->domain)
		domainlen = strlen(test->domain);

	const BOOL rcA =
	    NTOWFv2A(test->password, pwdlen, test->user, userlen, test->domain, domainlen, hashA);

	WCHAR* passwordW = nullptr;
	size_t pwdwlen = 0;
	if (pwdlen > 0)
		passwordW = ConvertUtf8NToWCharAlloc(test->password, pwdlen, &pwdwlen);

	WCHAR* userW = nullptr;
	size_t userwlen = 0;
	if (userlen > 0)
		userW = ConvertUtf8NToWCharAlloc(test->user, userlen, &userwlen);

	WCHAR* domainW = nullptr;
	size_t domainwlen = 0;
	if (domainlen > 0)
		domainW = ConvertUtf8NToWCharAlloc(test->domain, domainlen, &domainwlen);

	const BOOL rcW = NTOWFv2W(passwordW, pwdwlen * sizeof(WCHAR), userW, userwlen * sizeof(WCHAR),
	                          domainW, domainlen * sizeof(WCHAR), hashW);
	free(userW);
	free(domainW);
	free(passwordW);

	winpr_HexDump("NTOWFv2A", WLOG_INFO, hashA, sizeof(hashA));
	winpr_HexDump("expect", WLOG_INFO, test->expected, sizeof(test->expected));
	WLog_INFO("result", "[%" PRIuz "] got %d, expected %d", x, rcA, test->result);

	winpr_HexDump("NTOWFv2W", WLOG_INFO, hashW, sizeof(hashW));
	winpr_HexDump("expect", WLOG_INFO, test->expected, sizeof(test->expected));
	WLog_INFO("result", "[%" PRIuz "] got %d, expected %d", x, rcW, test->result);

	if (rcA != test->result)
		return FALSE;
	if (memcmp(test->expected, hashA, sizeof(test->expected)) != 0)
	{
		if (test->shouldMatch)
			return FALSE;
	}

	if (rcW != test->result)
		return FALSE;
	if (memcmp(test->expected, hashW, sizeof(test->expected)) != 0)
	{
		if (test->shouldMatch)
			return FALSE;
	}
	return TRUE;
}

static BOOL testNTOWFv2FromHash(size_t x, const test_case_from_hash_t* test)
{
	WINPR_ASSERT(test);

	BYTE hashA[sizeof(test->expected)] = WINPR_C_ARRAY_INIT;
	BYTE hashW[sizeof(test->expected)] = WINPR_C_ARRAY_INIT;

	size_t userlen = 0;
	if (test->user)
		userlen = strlen(test->user);

	size_t domainlen = 0;
	if (test->domain)
		domainlen = strlen(test->domain);

	const BOOL rcA =
	    NTOWFv2FromHashA(test->passwordhash, test->user, userlen, test->domain, domainlen, hashA);

	WCHAR* userW = nullptr;
	size_t userwlen = 0;
	if (userlen > 0)
		userW = ConvertUtf8NToWCharAlloc(test->user, userlen, &userwlen);

	WCHAR* domainW = nullptr;
	size_t domainwlen = 0;
	if (domainlen > 0)
		domainW = ConvertUtf8NToWCharAlloc(test->domain, domainlen, &domainwlen);

	const BOOL rcW = NTOWFv2FromHashW(test->passwordhash, userW, userwlen * sizeof(WCHAR), domainW,
	                                  domainwlen * sizeof(WCHAR), hashW);

	free(userW);
	free(domainW);

	winpr_HexDump("NTOWFv2A", WLOG_INFO, hashA, sizeof(hashA));
	winpr_HexDump("expect", WLOG_INFO, test->expected, sizeof(test->expected));
	WLog_INFO("result", "[%" PRIuz "] got %d, expected %d", x, rcA, test->result);

	winpr_HexDump("NTOWFv2W", WLOG_INFO, hashW, sizeof(hashW));
	winpr_HexDump("expect", WLOG_INFO, test->expected, sizeof(test->expected));
	WLog_INFO("result", "[%" PRIuz "] got %d, expected %d", x, rcW, test->result);

	if (rcA != test->result)
		return FALSE;
	if (memcmp(test->expected, hashA, sizeof(test->expected)) != 0)
	{
		if (test->shouldMatch)
			return FALSE;
	}

	if (rcW != test->result)
		return FALSE;
	if (memcmp(test->expected, hashW, sizeof(test->expected)) != 0)
	{
		if (test->shouldMatch)
			return FALSE;
	}
	return TRUE;
}

int TestNTLM(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	/* Make sure we load the OpenSSL legacy provider if needed */
	if (!winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT))
		return -1;

	for (size_t x = 0; x < ARRAYSIZE(ntofw1tests); x++)
	{
		const test_case_ntowf1_t* cur = &ntofw1tests[x];
		if (!testNTOWF1(x, cur))
		{
			WLog_WARN("testNTOWF1", "[%" PRIuz "] test failed", x);
			return -1;
		}
	}
	for (size_t x = 0; x < ARRAYSIZE(ntofw2tests); x++)
	{
		const test_case_ntowf2_t* cur = &ntofw2tests[x];
		if (!testNTOWF2(x, cur))
		{
			WLog_WARN("testNTOWF2", "[%" PRIuz "] test failed", x);
			return -2;
		}
	}
	for (size_t x = 0; x < ARRAYSIZE(ntofw2fromhashtests); x++)
	{
		const test_case_from_hash_t* cur = &ntofw2fromhashtests[x];
		if (!testNTOWFv2FromHash(x, cur))
		{
			WLog_WARN("testNTOWFv2FromHash", "[%" PRIuz "] test failed", x);
			return -2;
		}
	}
	return 0;
}
