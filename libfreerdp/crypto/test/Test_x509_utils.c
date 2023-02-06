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

static char* certificate_path(void)
{
	/*
	Assume the .pem file is in the same directory as this source file.
	Assume that __FILE__ will be a valid path to this file, even from the current working directory
	where the tests are run. (ie. no chdir occurs between compilation and test running, or __FILE__
	is an absolute path).
	*/
	static const char dirsep = '/';
	static const char filename[] = "Test_x509_cert_info.pem";
#ifdef TEST_SOURCE_DIR
	const char* file = TEST_SOURCE_DIR;
	const size_t flen = sizeof(filename) + sizeof(dirsep) + strlen(file) + sizeof(char);
	char* result = calloc(1, flen);
	if (!result)
		return NULL;
	_snprintf(result, flen - 1, "%s%c%s", file, dirsep, filename);
	return result;
#else
	const char* file = __FILE__;
	const char* last_dirsep = strrchr(file, dirsep);

	if (last_dirsep)
	{
		const size_t filenameLen = strnlen(filename, sizeof(filename));
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
		printf("%s: failure: cannot read certificate file '%s'\n", __FUNCTION__, certificate_path);
		success = -1;
		goto fail;
	}

	for (size_t i = 0; i < count; i++)
	{
		const certificate_test_t* test = &ccertificate_tests[i];
		char* result;

		if (test->status == DISABLED)
		{
			continue;
		}

		result = (test->get_field ? test->get_field(certificate) : 0);

		if (result)
		{
			printf("%s: crypto got %-40s -> \"%s\"\n", __FUNCTION__, test->field_description,
			       result);

			if (0 != strcmp(result, test->expected_result))
			{
				printf("%s: failure: for %s, actual: \"%s\", expected \"%s\"\n", __FUNCTION__,
				       test->field_description, result, test->expected_result);
				success = -1;
			}

			free(result);
		}
		else
		{
			printf("%s: failure: cannot get %s\n", __FUNCTION__, test->field_description);
		}
	}

fail:
	X509_free(certificate);
	return success;
}

int Test_x509_utils(int argc, char* argv[])
{
	char* cert_path = certificate_path();
	int ret;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	ret = TestCertificateFile(cert_path, certificate_tests, ARRAYSIZE(certificate_tests));
	free(cert_path);
	return ret;
}
