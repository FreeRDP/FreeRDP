#include <freerdp/crypto/crypto.h>


#define countof(a)   (sizeof(a)/sizeof(a[0]))

typedef char*   (*get_field_pr)(X509*);
typedef struct
{
	enum
	{
	    DISABLED, ENABLED,
	} status;
	const char* field_description;
	get_field_pr get_field;
	const char* expected_result;
} certificate_test_t;

char*   crypto_cert_subject_common_name_wo_length(X509* xcert)
{
	int length;
	return crypto_cert_subject_common_name(xcert, & length);
}

const char* certificate_path = "Test_x509_cert_info.pem";
const certificate_test_t certificate_tests[] =
{

	{
		ENABLED,
		"Certificate Common Name",
		crypto_cert_subject_common_name_wo_length,
		"TESTJEAN TESTMARTIN 9999999"
	},

	{
		ENABLED,
		"Certificate subject",
		crypto_cert_subject,
		"CN = TESTJEAN TESTMARTIN 9999999, C = FR, O = MINISTERE DES TESTS, OU = 0002 110014016, OU = PERSONNES, UID = 9999999, GN = TESTJEAN, SN = TESTMARTIN"
	},

	{
		DISABLED,
		"Kerberos principal name",
		0,
		"testjean.testmartin@kpn.test.example.com"
	},

	{
		ENABLED,
		"Certificate e-mail",
		crypto_cert_get_email,
		"testjean.testmartin@test.example.com"
	},

	{
		ENABLED,
		"Microsoft's Universal Principal Name",
		crypto_cert_get_upn,
		"testjean.testmartin.9999999@upn.test.example.com"
	},

	{
		ENABLED,
		"Certificate issuer",
		crypto_cert_issuer,
		"CN = ADMINISTRATION CENTRALE DES TESTS, C = FR, O = MINISTERE DES TESTS, OU = 0002 110014016"
	},

};


int TestCertificateFile(const char* certificate_path, const certificate_test_t* certificate_tests,
                        int count)
{
	X509*   certificate;
	FILE*   certificate_file = fopen(certificate_path, "r");
	int success = 0;
	int i;

	if (!certificate_file)
	{
		printf("%s: failure: cannot open certificate file '%s'\n", __FUNCTION__, certificate_path);
		return -1;
	}

	certificate = PEM_read_X509(certificate_file, 0, 0, 0);

	if (!certificate)
	{
		printf("%s: failure: cannot read certificate file '%s'\n", __FUNCTION__, certificate_path);
		success = -1;
		goto fail;
	}

	for (i = 0; i < count; i ++)
	{
		char* result;

		if (certificate_tests[i].status == DISABLED)
		{
			continue;
		}

		result = (certificate_tests[i].get_field
		          ? certificate_tests[i].get_field(certificate)
		          : 0);

		if (result)
		{
			printf("%s: crypto got %-40s -> \"%s\"\n", __FUNCTION__,
			       certificate_tests[i].field_description,
			       result);

			if (0 != strcmp(result, certificate_tests[i].expected_result))
			{
				printf("%s: failure: for %s, actual: \"%s\", expected \"%s\"\n",
				       __FUNCTION__,
				       certificate_tests[i].field_description,
				       result,
				       certificate_tests[i].expected_result);
				success = -1;
			}
		}
		else
		{
			printf("%s: failure: cannot get %s\n", __FUNCTION__,
			       certificate_tests[i].field_description);
		}
	}

fail:
	fclose(certificate_file);
	return success;
}


int Test_x509_cert_info(int argc, char* argv[])
{
	return TestCertificateFile(certificate_path, certificate_tests, countof(certificate_tests));
}

