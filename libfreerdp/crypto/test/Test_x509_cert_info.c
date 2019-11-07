#include <freerdp/crypto/crypto.h>

typedef char* (*get_field_pr)(X509*);
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

static char* crypto_cert_subject_common_name_wo_length(X509* xcert)
{
	int length;
	return crypto_cert_subject_common_name(xcert, &length);
}

static char* certificate_path(void)
{
	/*
	Assume the .pem file is in the same directory as this source file.
	Assume that __FILE__ will be a valid path to this file, even from the current working directory
	where the tests are run. (ie. no chdir occurs between compilation and test running, or __FILE__
	is an absolute path).
	*/
#if defined(_WIN32)
	static const char dirsep = '\\';
#else
	static const char dirsep = '/';
#endif
	static const char filename[] = "Test_x509_cert_info.pem";
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
}

static const certificate_test_t certificate_tests[] = {

	{ ENABLED, "Certificate Common Name", crypto_cert_subject_common_name_wo_length,
	  "TESTJEAN TESTMARTIN 9999999" },

	{ ENABLED, "Certificate subject", crypto_cert_subject,
	  "CN = TESTJEAN TESTMARTIN 9999999, C = FR, O = MINISTERE DES TESTS, OU = 0002 110014016, OU "
	  "= PERSONNES, UID = 9999999, GN = TESTJEAN, SN = TESTMARTIN" },

	{ DISABLED, "Kerberos principal name", 0, "testjean.testmartin@kpn.test.example.com" },

	{ ENABLED, "Certificate e-mail", crypto_cert_get_email, "testjean.testmartin@test.example.com"

	},

	{ ENABLED, "Microsoft's Universal Principal Name", crypto_cert_get_upn,
	  "testjean.testmartin.9999999@upn.test.example.com" },

	{ ENABLED, "Certificate issuer", crypto_cert_issuer,
	  "CN = ADMINISTRATION CENTRALE DES TESTS, C = FR, O = MINISTERE DES TESTS, OU = 0002 "
	  "110014016" },
};

static int TestCertificateFile(const char* certificate_path,
                               const certificate_test_t* certificate_tests, int count)
{
	X509* certificate;
	FILE* certificate_file = fopen(certificate_path, "r");
	int success = 0;
	int i;

	if (!certificate_file)
	{
		printf("%s: failure: cannot open certificate file '%s'\n", __FUNCTION__, certificate_path);
		return -1;
	}

	certificate = PEM_read_X509(certificate_file, 0, 0, 0);
	fclose(certificate_file);

	if (!certificate)
	{
		printf("%s: failure: cannot read certificate file '%s'\n", __FUNCTION__, certificate_path);
		success = -1;
		goto fail;
	}

	for (i = 0; i < count; i++)
	{
		char* result;

		if (certificate_tests[i].status == DISABLED)
		{
			continue;
		}

		result = (certificate_tests[i].get_field ? certificate_tests[i].get_field(certificate) : 0);

		if (result)
		{
			printf("%s: crypto got %-40s -> \"%s\"\n", __FUNCTION__,
			       certificate_tests[i].field_description, result);

			if (0 != strcmp(result, certificate_tests[i].expected_result))
			{
				printf("%s: failure: for %s, actual: \"%s\", expected \"%s\"\n", __FUNCTION__,
				       certificate_tests[i].field_description, result,
				       certificate_tests[i].expected_result);
				success = -1;
			}

			free(result);
		}
		else
		{
			printf("%s: failure: cannot get %s\n", __FUNCTION__,
			       certificate_tests[i].field_description);
		}
	}

fail:
	X509_free(certificate);
	return success;
}

int Test_x509_cert_info(int argc, char* argv[])
{
	char* cert_path = certificate_path();
	int ret;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	ret = TestCertificateFile(cert_path, certificate_tests, ARRAYSIZE(certificate_tests));
	free(cert_path);
	return ret;
}
