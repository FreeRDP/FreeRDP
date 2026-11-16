#include <stdio.h>
#include <string.h>

#include <openssl/pem.h>
#include <openssl/x509.h>

#include <winpr/wtypes.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/sysinfo.h>
#include <winpr/thread.h>
#include <winpr/tools/makecert.h>

/* One entry of the certificate's subject or issuer, empty when it has none. */
static void name_entry(const X509_NAME* name, int nid, char* text, size_t size)
{
	text[0] = '\0';
	if (name && (size <= INT32_MAX) && (X509_NAME_get_text_by_NID(name, nid, text, (int)size) < 0))
		text[0] = '\0';
}

/* Makes a certificate with these arguments in dir, and reads its subject and issuer back. */
WINPR_ATTR_NODISCARD
static BOOL make(const char* dir, const char* file, char** argv, int argc, char* cn, char* issuerCN,
                 char* org, size_t size)
{
	BOOL rc = FALSE;
	char* path = nullptr;
	BIO* bio = nullptr;
	X509* x509 = nullptr;
	MAKECERT_CONTEXT* makecert = makecert_context_new();
	if (!makecert)
		goto fail;
	if (makecert_context_process(makecert, argc, argv) < 0)
		goto fail;
	if (makecert_context_set_output_file_name(makecert, file) != 1)
		goto fail;
	if (makecert_context_output_certificate_file(makecert, dir) != 1)
		goto fail;

	{
		char name[64] = { 0 };
		(void)_snprintf(name, sizeof(name), "%s.crt", file);
		path = GetCombinedPath(dir, name);
	}
	if (!path)
		goto fail;
	bio = BIO_new_file(path, "r");
	if (!bio)
		goto fail;
	x509 = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
	if (!x509)
		goto fail;

	name_entry(X509_get_subject_name(x509), NID_commonName, cn, size);
	name_entry(X509_get_issuer_name(x509), NID_commonName, issuerCN, size);
	name_entry(X509_get_subject_name(x509), NID_organizationName, org, size);
	rc = TRUE;
fail:
	if (path)
		(void)winpr_DeleteFile(path);
	free(path);
	X509_free(x509);
	BIO_free(bio);
	makecert_context_free(makecert);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL check(const char* what, const char* dir, char** argv, int argc, const char* expectedCN,
                  const char* expectedOrg)
{
	char cn[256] = { 0 };
	char issuerCN[256] = { 0 };
	char org[256] = { 0 };
	if (!make(dir, "TestMakecertCommonName", argv, argc, cn, issuerCN, org, sizeof(cn)))
	{
		(void)fprintf(stderr, "%s: no certificate\n", what);
		return FALSE;
	}

	const BOOL ok = (strcmp(cn, expectedCN) == 0) && (strcmp(issuerCN, expectedCN) == 0) &&
	                (strcmp(org, expectedOrg) == 0);
	if (!ok)
		(void)fprintf(stderr, "%s: CN '%s' issuer CN '%s' O '%s', expected CN '%s' O '%s'\n", what,
		              cn, issuerCN, org, expectedCN, expectedOrg);
	return ok;
}

WINPR_ATTR_NODISCARD
static BOOL refused(const char* what, const char* dir, char** argv, int argc)
{
	char cn[256] = { 0 };
	char issuerCN[256] = { 0 };
	char org[256] = { 0 };
	if (make(dir, "TestMakecertCommonName", argv, argc, cn, issuerCN, org, sizeof(cn)))
	{
		(void)fprintf(stderr, "%s: a certificate with CN '%s', expected none\n", what, cn);
		return FALSE;
	}
	return TRUE;
}

/* makecert's own default name: the host's DNS name, else its NetBIOS name */
WINPR_ATTR_NODISCARD
static BOOL host_name(char* name, DWORD size)
{
	DWORD length = size;
	if (GetComputerNameExA(ComputerNamePhysicalDnsFullyQualified, name, &length) && (length > 0))
		return TRUE;
	length = size;
	return GetComputerNameExA(ComputerNamePhysicalNetBIOS, name, &length) && (length > 0);
}

int TestMakecertCommonName(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	int rc = -1;
	char host[256] = { 0 };
	char* temp = GetKnownPath(KNOWN_PATH_TEMP);
	char* dir = nullptr;
	if (!temp || !host_name(host, sizeof(host)))
		goto fail;
	{
		char sub[64] = { 0 };
		(void)_snprintf(sub, sizeof(sub), "TestMakecertCommonName-%" PRIu32, GetCurrentProcessId());
		dir = GetCombinedPath(temp, sub);
	}
	if (!dir || !winpr_PathMakePath(dir, nullptr))
		goto fail;

	{
		/* as the shadow server makes its certificate: no -n, the host name */
		char* noName[] = { "makecert", "-rdp", "-live", "-silent", "-y", "1" };
		char* plain[] = { "makecert", "-rdp", "-live", "-silent", "-n", "myhost" };
		char* records[] = { "makecert", "-rdp", "-live", "-silent", "-n", "CN=abc,O=def" };
		/* a common name holds 64 characters: a cloud machine's long name keeps its host name */
		char* longName[] = {
			"makecert", "-rdp",
			"-live",    "-silent",
			"-n",       "fv-az123-456.abcdefghijklmnopqrstuvwxyz0123.bx.internal.cloudapp.net"
		};
		/* a label holds 63 characters: a longer first label is no host name, an error */
		char longLabel[71] = { 0 };
		memset(longLabel, 'x', sizeof(longLabel) - 1);
		char* oneLabel[] = { "makecert", "-rdp", "-live", "-silent", "-n", longLabel };
		char longFirst[64 + sizeof(".example.com")] = { 0 };
		(void)_snprintf(longFirst, sizeof(longFirst), "%.64s.example.com", longLabel);
		char* firstLabel[] = { "makecert", "-rdp", "-live", "-silent", "-n", longFirst };

		BOOL ok = check("no -n", dir, noName, (int)ARRAYSIZE(noName), host, "");
		ok &= check("-n myhost", dir, plain, (int)ARRAYSIZE(plain), "myhost", "");
		ok &= check("-n CN=abc,O=def", dir, records, (int)ARRAYSIZE(records), "abc", "def");
		ok &= check("long name", dir, longName, (int)ARRAYSIZE(longName), "fv-az123-456", "");
		ok &= refused("long label", dir, oneLabel, (int)ARRAYSIZE(oneLabel));
		ok &= refused("long first label", dir, firstLabel, (int)ARRAYSIZE(firstLabel));
		/* no name at all: a certificate without a common name, as before */
		char* empty[] = { "makecert", "-rdp", "-live", "-silent", "-n", "" };
		ok &= check("empty name", dir, empty, (int)ARRAYSIZE(empty), "", "");
		if (ok)
			rc = 0;
	}

fail:
	if (dir)
		(void)winpr_RemoveDirectory(dir);
	free(dir);
	free(temp);
	return rc;
}
