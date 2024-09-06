#include <winpr/file.h>
#include <winpr/string.h>
#include "../x509_utils.h"

typedef char* (*get_field_pr)(const X509*);
typedef struct
{
	enum
	{
		DISABLED,
		ENABLED,
	} status;
	const char* field_description;
	get_field_pr get_field;
	const char* expected_result;
} certificate_test_t;

static char* x509_utils_subject_common_name_wo_length(const X509* xcert)
{
	size_t length = 0;
	return x509_utils_get_common_name(xcert, &length);
}

static char* certificate_path(const char* filename)
{
	/*
	Assume the .pem file is in the same directory as this source file.
	Assume that __FILE__ will be a valid path to this file, even from the current working directory
	where the tests are run. (ie. no chdir occurs between compilation and test running, or __FILE__
	is an absolute path).
	*/
	static const char dirsep = '/';
#ifdef TEST_SOURCE_DIR
	const char* file = TEST_SOURCE_DIR;
	const size_t flen = strlen(file) + sizeof(dirsep) + strlen(filename) + sizeof(char);
	char* result = calloc(1, flen);
	if (!result)
		return NULL;
	(void)_snprintf(result, flen, "%s%c%s", file, dirsep, filename);
	return result;
#else
	const char* file = __FILE__;
	const char* last_dirsep = strrchr(file, dirsep);

	if (last_dirsep)
	{
		const size_t filenameLen = strlen(filename);
		const size_t dirsepLen = last_dirsep - file + 1;
		char* result = malloc(dirsepLen + filenameLen + 1);
		if (!result)
			return NULL;
		strncpy(result, file, dirsepLen);
		strncpy(result + dirsepLen, filename, filenameLen + 1);
		return result;
	}
	else
	{
		/* No dirsep => relative path in same directory */
		return _strdup(filename);
	}
#endif
}

static const certificate_test_t certificate_tests[] = {

	{ ENABLED, "Certificate Common Name", x509_utils_subject_common_name_wo_length,
	  "TESTJEAN TESTMARTIN 9999999" },

	{ ENABLED, "Certificate subject", x509_utils_get_subject,
	  "CN = TESTJEAN TESTMARTIN 9999999, C = FR, O = MINISTERE DES TESTS, OU = 0002 110014016, OU "
	  "= PERSONNES, UID = 9999999, GN = TESTJEAN, SN = TESTMARTIN" },

	{ DISABLED, "Kerberos principal name", 0, "testjean.testmartin@kpn.test.example.com" },

	{ ENABLED, "Certificate e-mail", x509_utils_get_email, "testjean.testmartin@test.example.com"

	},

	{ ENABLED, "Microsoft's Universal Principal Name", x509_utils_get_upn,
	  "testjean.testmartin.9999999@upn.test.example.com" },

	{ ENABLED, "Certificate issuer", x509_utils_get_issuer,
	  "CN = ADMINISTRATION CENTRALE DES TESTS, C = FR, O = MINISTERE DES TESTS, OU = 0002 "
	  "110014016" },
};

static int TestCertificateFile(const char* certificate_path,
                               const certificate_test_t* ccertificate_tests, size_t count)
{
	int success = 0;

	X509* certificate = x509_utils_from_pem(certificate_path, strlen(certificate_path), TRUE);

	if (!certificate)
	{
		printf("%s: failure: cannot read certificate file '%s'\n", __func__, certificate_path);
		success = -1;
		goto fail;
	}

	for (size_t i = 0; i < count; i++)
	{
		const certificate_test_t* test = &ccertificate_tests[i];
		char* result = NULL;

		if (test->status == DISABLED)
		{
			continue;
		}

		result = (test->get_field ? test->get_field(certificate) : 0);

		if (result)
		{
			printf("%s: crypto got %-40s -> \"%s\"\n", __func__, test->field_description, result);

			if (0 != strcmp(result, test->expected_result))
			{
				printf("%s: failure: for %s, actual: \"%s\", expected \"%s\"\n", __func__,
				       test->field_description, result, test->expected_result);
				success = -1;
			}

			free(result);
		}
		else
		{
			printf("%s: failure: cannot get %s\n", __func__, test->field_description);
		}
	}

fail:
	X509_free(certificate);
	return success;
}

/* clang-format off */
/*
These certificates were generated with the following commands:

openssl ecparam -name P-256 -out /tmp/p256.pem
openssl req -x509 -newkey ec:/tmp/p256.pem -keyout /dev/null -days 3650 -nodes -subj "/CN=Test" -out ecdsa_sha1_cert.pem -sha1
openssl req -x509 -newkey ec:/tmp/p256.pem -keyout /dev/null -days 3650 -nodes -subj "/CN=Test" -out ecdsa_sha256_cert.pem -sha256
openssl req -x509 -newkey ec:/tmp/p256.pem -keyout /dev/null -days 3650 -nodes -subj "/CN=Test" -out ecdsa_sha384_cert.pem -sha384
openssl req -x509 -newkey ec:/tmp/p256.pem -keyout /dev/null -days 3650 -nodes -subj "/CN=Test" -out ecdsa_sha512_cert.pem -sha512

openssl req -x509 -newkey rsa:2048 -keyout /dev/null -days 3650 -nodes -subj "/CN=Test" -out rsa_pkcs1_sha1_cert.pem -sha1
openssl req -x509 -newkey rsa:2048 -keyout /dev/null -days 3650 -nodes -subj "/CN=Test" -out rsa_pkcs1_sha256_cert.pem -sha256
openssl req -x509 -newkey rsa:2048 -keyout /dev/null -days 3650 -nodes -subj "/CN=Test" -out rsa_pkcs1_sha384_cert.pem -sha384
openssl req -x509 -newkey rsa:2048 -keyout /dev/null -days 3650 -nodes -subj "/CN=Test" -out rsa_pkcs1_sha512_cert.pem -sha512

openssl req -x509 -newkey rsa:2048 -keyout /dev/null -days 3650 -nodes -subj "/CN=Test" -out rsa_pss_sha1_cert.pem -sha1 -sigopt rsa_padding_mode:pss -sigopt rsa_pss_saltlen:digest
openssl req -x509 -newkey rsa:2048 -keyout /dev/null -days 3650 -nodes -subj "/CN=Test" -out rsa_pss_sha256_cert.pem -sha256 -sigopt rsa_padding_mode:pss -sigopt rsa_pss_saltlen:digest
openssl req -x509 -newkey rsa:2048 -keyout /dev/null -days 3650 -nodes -subj "/CN=Test" -out rsa_pss_sha384_cert.pem -sha384 -sigopt rsa_padding_mode:pss -sigopt rsa_pss_saltlen:digest
openssl req -x509 -newkey rsa:2048 -keyout /dev/null -days 3650 -nodes -subj "/CN=Test" -out rsa_pss_sha512_cert.pem -sha512 -sigopt rsa_padding_mode:pss -sigopt rsa_pss_saltlen:digest
openssl req -x509 -newkey rsa:2048 -keyout /dev/null -days 3650 -nodes -subj "/CN=Test" -out rsa_pss_sha256_mgf1_sha384_cert.pem -sha256 -sigopt rsa_padding_mode:pss -sigopt rsa_pss_saltlen:digest -sigopt rsa_mgf1_md:sha384
*/
/* clang-format on */

typedef struct
{
	const char* filename;
	WINPR_MD_TYPE expected;
} signature_alg_test_t;

static const signature_alg_test_t signature_alg_tests[] = {
	{ "rsa_pkcs1_sha1_cert.pem", WINPR_MD_SHA1 },
	{ "rsa_pkcs1_sha256_cert.pem", WINPR_MD_SHA256 },
	{ "rsa_pkcs1_sha384_cert.pem", WINPR_MD_SHA384 },
	{ "rsa_pkcs1_sha512_cert.pem", WINPR_MD_SHA512 },

	{ "ecdsa_sha1_cert.pem", WINPR_MD_SHA1 },
	{ "ecdsa_sha256_cert.pem", WINPR_MD_SHA256 },
	{ "ecdsa_sha384_cert.pem", WINPR_MD_SHA384 },
	{ "ecdsa_sha512_cert.pem", WINPR_MD_SHA512 },

	{ "rsa_pss_sha1_cert.pem", WINPR_MD_SHA1 },
	{ "rsa_pss_sha256_cert.pem", WINPR_MD_SHA256 },
	{ "rsa_pss_sha384_cert.pem", WINPR_MD_SHA384 },
	{ "rsa_pss_sha512_cert.pem", WINPR_MD_SHA512 },
	/*
	PSS may use different digests for the message hash and MGF-1 hash. In this case, RFC 5929
	leaves the tls-server-end-point hash unspecified, so it should return WINPR_MD_NONE.
	*/
	{ "rsa_pss_sha256_mgf1_sha384_cert.pem", WINPR_MD_NONE },
};

static int TestSignatureAlgorithm(const signature_alg_test_t* test)
{
	int success = 0;
	WINPR_MD_TYPE signature_alg = WINPR_MD_NONE;
	char* path = certificate_path(test->filename);
	X509* certificate = x509_utils_from_pem(path, strlen(path), TRUE);

	if (!certificate)
	{
		printf("%s: failure: cannot read certificate file '%s'\n", __func__, path);
		success = -1;
		goto fail;
	}

	signature_alg = x509_utils_get_signature_alg(certificate);
	if (signature_alg != test->expected)
	{
		const char* signature_alg_string =
		    signature_alg == WINPR_MD_NONE ? "none" : winpr_md_type_to_string(signature_alg);
		const char* expected_string =
		    test->expected == WINPR_MD_NONE ? "none" : winpr_md_type_to_string(test->expected);
		printf("%s: failure: for \"%s\", actual: %s, expected %s\n", __func__, test->filename,
		       signature_alg_string, expected_string);
		success = -1;
		goto fail;
	}

fail:
	X509_free(certificate);
	free(path);
	return success;
}

int Test_x509_utils(int argc, char* argv[])
{
	char* cert_path = certificate_path("Test_x509_cert_info.pem");
	int ret = 0;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	ret = TestCertificateFile(cert_path, certificate_tests, ARRAYSIZE(certificate_tests));
	free(cert_path);
	if (ret != 0)
		return ret;

	for (size_t i = 0; i < ARRAYSIZE(signature_alg_tests); i++)
	{
		ret = TestSignatureAlgorithm(&signature_alg_tests[i]);
		if (ret != 0)
			return ret;
	}

	return ret;
}
