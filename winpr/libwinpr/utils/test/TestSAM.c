#include <stdlib.h>
#include <string.h>

#include <winpr/wtypes.h>
#include <winpr/string.h>
#include <winpr/sam.h>
#include <winpr/file.h>
#include <winpr/crypto.h>
#include <winpr/path.h>

const char* sam_entries[] = {
	"test1:::1910bd9285a6b8c9344d9f5cc74e0878:::",      /* test1, xxxxxx */
	"test2:::1bfc28ca2a4c218b032f4a0309b31f20:::",      /* test2, aaaaaabbbbbb */
	"test1:domain::eab3412c36d6bbb1e3f6eaaf2775696e:::" /* test1, domain, pppppppp */
};

typedef struct
{
	const char* pwd;
	BYTE LmHash[16];
	BYTE NtHash[16];
} test_case_t;

const test_case_t hashes[] = { { .pwd = "xxxxxx",
	                             .LmHash = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	                             .NtHash = { 0x19, 0x10, 0xbd, 0x92, 0x85, 0xa6, 0xb8, 0xc9, 0x34,
	                                         0x4d, 0x9f, 0x5c, 0xc7, 0x4e, 0x08, 0x78 } },
	                           { .pwd = "aaaaaabbbbbb",
	                             .LmHash = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	                             .NtHash = { 0x1b, 0xfc, 0x28, 0xca, 0x2a, 0x4c, 0x21, 0x8b, 0x03,
	                                         0x2f, 0x4a, 0x03, 0x09, 0xb3, 0x1f, 0x20 } },
	                           { .pwd = "pppppppp",
	                             .LmHash = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	                             .NtHash = { 0xea, 0xb3, 0x41, 0x2c, 0x36, 0xd6, 0xbb, 0xb1, 0xe3,
	                                         0xf6, 0xea, 0xaf, 0x27, 0x75, 0x69, 0x6e } } };

static BOOL testHashA(const WINPR_SAM_ENTRY* entry, const char* pwd)
{
	if (!entry)
		return FALSE;
	for (size_t x = 0; x < ARRAYSIZE(hashes); x++)
	{
		const test_case_t* cur = &hashes[x];
		if (strcmp(cur->pwd, pwd) != 0)
			continue;

		if (memcmp(entry->LmHash, cur->LmHash, sizeof(entry->LmHash)) != 0)
			return FALSE;
		return (memcmp(entry->NtHash, cur->NtHash, sizeof(entry->NtHash)) == 0);
	}
	return FALSE;
}

static BOOL testHashW(const WINPR_SAM_ENTRY* entry, const WCHAR* pwd)
{
	if (!pwd)
		return FALSE;
	char* pwdA = ConvertWCharToUtf8Alloc(pwd, nullptr);
	if (!pwdA)
		return FALSE;
	const BOOL rc = testHashA(entry, pwdA);
	free(pwdA);
	return rc;
}

static BOOL testA(WINPR_SAM* sam, const char* name, const char* domain, const char* pwd,
                  BOOL expectSuccess)
{
	BOOL rc = !expectSuccess;
	size_t nlen = 0;
	if (name)
		nlen = strlen(name);

	size_t dlen = 0;
	if (domain)
		dlen = strlen(domain);

	WINPR_SAM_ENTRY* entry = SamLookupUserA(sam, name, nlen, domain, dlen);
	if (!entry)
		goto fail;

	if (entry->UserLength != nlen)
		goto fail;
	if (entry->DomainLength != dlen)
		goto fail;

	if (name)
	{
		if (strncmp(name, entry->User, nlen + 1) != 0)
			goto fail;
	}

	if (domain)
	{
		if (strncmp(domain, entry->Domain, dlen + 1) != 0)
			goto fail;
	}

	rc = testHashA(entry, pwd);
	if (!expectSuccess)
		rc = !rc;

fail:
	SamFreeEntry(sam, entry);
	return rc;
}

static BOOL testW(WINPR_SAM* sam, const WCHAR* name, const WCHAR* domain, const WCHAR* pwd,
                  BOOL expectSuccess)
{
	BOOL rc = !expectSuccess;
	size_t nlen = 0;
	if (name)
		nlen = _wcslen(name) * sizeof(WCHAR);

	size_t dlen = 0;
	if (domain)
		dlen = _wcslen(domain) * sizeof(WCHAR);

	WINPR_SAM_ENTRY* entry = SamLookupUserW(sam, name, nlen, domain, dlen);
	if (!entry)
		goto fail;

	if (entry->UserLength != nlen)
		goto fail;
	if (entry->DomainLength != dlen)
		goto fail;

	if (name)
	{
		if (_wcsncmp(name, (const WCHAR*)entry->User, nlen / sizeof(WCHAR) + 1) != 0)
			goto fail;
	}

	if (domain)
	{
		if (_wcsncmp(domain, (const WCHAR*)entry->Domain, dlen / sizeof(WCHAR) + 1) != 0)
			goto fail;
	}

	rc = testHashW(entry, pwd);
	if (!expectSuccess)
		rc = !rc;

fail:
	SamFreeEntry(sam, entry);
	return rc;
}

static BOOL test(WINPR_SAM* sam, const char* name, const char* domain, const char* pwd,
                 BOOL expectSuccess)
{
	if (!testA(sam, name, domain, pwd, expectSuccess))
		return FALSE;

	WCHAR* nameW = nullptr;
	WCHAR* domainW = nullptr;
	WCHAR* pwdW = nullptr;
	if (name)
		nameW = ConvertUtf8ToWCharAlloc(name, nullptr);
	if (domain)
		domainW = ConvertUtf8ToWCharAlloc(domain, nullptr);
	if (pwd)
		pwdW = ConvertUtf8ToWCharAlloc(pwd, nullptr);

	const BOOL rc = testW(sam, nameW, domainW, pwdW, expectSuccess);
fail:
	free(nameW);
	free(domainW);
	free(pwdW);
	return rc;
}

WINPR_ATTR_MALLOC(free, 1)
static char* prepare(void)
{
	char tpath[MAX_PATH] = WINPR_C_ARRAY_INIT;

	size_t nr = 0;
	if (winpr_RAND(&nr, sizeof(nr)) < 0)
		return nullptr;
	(void)snprintf(tpath, sizeof(tpath), "sam-test-%016" PRIxz, nr);
	char* tmp = GetKnownSubPath(KNOWN_PATH_TEMP, tpath);
	if (!tmp)
		return nullptr;

	FILE* fp = fopen(tmp, "w");
	if (!fp)
	{
		free(tmp);
		return nullptr;
	}

	for (size_t x = 0; x < ARRAYSIZE(sam_entries); x++)
	{
		const char* entry = sam_entries[x];
		(void)fprintf(fp, "%s\r\n", entry);
	}
	(void)fclose(fp);

	return tmp;
}

int TestSAM(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	int res = -1;

	char* tmp = prepare();
	if (!tmp)
		return -1;

	WINPR_SAM* sam = SamOpen(tmp, TRUE);
	if (!sam)
		goto fail;

	if (!test(sam, "test1", nullptr, "xxxxxx", TRUE))
		goto fail;
	if (!test(sam, "test2", nullptr, "aaaaaabbbbbb", TRUE))
		goto fail;
	if (!test(sam, "test1", "domain", "pppppppp", TRUE))
		goto fail;
	if (!test(sam, "test1", "foobar", "xxxxxx", FALSE))
		goto fail;
	if (!test(sam, "test3", nullptr, "xxxxxx", FALSE))
		goto fail;
	if (!test(sam, "test1", nullptr, "xxxxxxe", FALSE))
		goto fail;

	res = 0;
fail:
	SamClose(sam);
	if (tmp)
		DeleteFile(tmp);
	free(tmp);
	return res;
}
