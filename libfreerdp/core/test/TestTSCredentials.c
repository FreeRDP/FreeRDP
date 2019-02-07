#include <stdio.h>
#include <string.h>

#include <winpr/sspi.h>
#include <winpr/wlog.h>
#include <winpr/stream.h>

#include <freerdp/log.h>
#include <freerdp/crypto/ber.h>

#include "../tscredentials.h"

#define TAG FREERDP_TAG("test.TestTSCredentials")

#define printref() printf("%s:%d: in function %-40s:", __FILE__, __LINE__, __FUNCTION__)

#define ERROR(format, ...)						\
	do{								\
		fprintf(stderr, format, ##__VA_ARGS__);			\
		printref();						\
		printf(format "\n", ##__VA_ARGS__);			\
		fflush(stdout);						\
		error_count++;						\
	}while(0)

#define FAILURE(format, ...)						\
	do{								\
		printref();						\
		printf(" FAILURE ");					\
		printf(format "\n", ##__VA_ARGS__);			\
		fflush(stdout);						\
		failure_count++;					\
	}while(0)


#define TEST(condition, format, ...)					\
	if (!(condition))						\
	{								\
		FAILURE("test %s " format, #condition,			\
		        ##__VA_ARGS__);					\
	}

size_t min(size_t a, size_t b)
{
	return (a < b) ? a : b;
}

auth_identity* make_test_smartcard_creds()
{
	csp_data_detail* csp = csp_data_detail_new(1,
		"IAS-ECC",
		"XIRING Leo v2 (8288830623) 00 00",
		"Clé d'authentification1",
		"Middleware IAS ECC Cryptographic Provider");
	auth_identity* identity = auth_identity_new_smartcard(smartcard_creds_new("0000",
			"EXAMPLEUSER",
			"EXAMPLE.DOMAIN",
			csp));
	csp_data_detail_free(csp);
	return identity;
}

auth_identity* make_test_remote_guard_creds()
{
	/*
	* ASN.1 value:
	*
	* value TSRemoteGuardCreds ::= {
	*     logonCred {
	*         packageName  '4D007900530065006300750072006900740079005000610063006B00610067006500'H ,
	*         credBuffer   '4D7920427265617468204973204D792050617373776F7264'H
	*     },
	*     supplementalCreds {
	*         {
	*             packageName '41006C007400650072006E0061007400690076006500530065006300750072006900740079005000610063006B00610067006500'H ,
	*             credBuffer  '4D7920427265617468204973204D79204F746865722050617373776F7264'H
	*         },
	*         {
	*             packageName '500041004D00'H ,
	*             credBuffer  '666F6F62617262617A2170617373'H
	*         }
	*     }
	* }
	*
	*/
	char* package_name;
	char* credentials;
	remote_guard_creds* remote_guard_creds;
	package_name = strdup("MySecurityPackage");
	credentials  = strdup("My Breath Is My Password");
	remote_guard_creds = remote_guard_creds_new_nocopy(package_name, strlen(credentials),
		(BYTE*)credentials);
	package_name = strdup("AlternativeSecurityPackage");
	credentials  = strdup("My Breath Is My Other Password");
	remote_guard_creds_add_supplemental_cred(remote_guard_creds,
	        remote_guard_package_cred_new_nocopy(package_name, strlen(credentials), (BYTE*)credentials));
	package_name = strdup("PAM");
	credentials  = strdup("foobarbaz!pass");
	remote_guard_creds_add_supplemental_cred(remote_guard_creds,
	        remote_guard_package_cred_new_nocopy(package_name, strlen(credentials), (BYTE*)credentials));
	return auth_identity_new_remote_guard(remote_guard_creds);
}

static unsigned failure_count = 0;
static unsigned error_count = 0;

void reset_counters()
{
	failure_count = 0;
	error_count = 0;
}

typedef size_t (*writer_pr)(void* data, wStream* s);

size_t stream_size(size_t allocated_size,  writer_pr writer, void * data)
{
	size_t written_size;
	size_t result_size;
	wStream* s;
	s = Stream_New(NULL, /* some margin: */ 1024 + 2 * allocated_size);
	written_size = writer(data, s);
	Stream_SealLength(s);
	result_size = Stream_Length(s);
	Stream_Free(s, TRUE);
	return result_size;
}



size_t write_csp(void* csp, wStream* s)
{
	return nla_write_ts_csp_data_detail(csp, 3, s);
}

int string_length(char* string); /* in tscredentials.c */

BOOL test_sizeof_smartcard_creds()
{
	csp_data_detail* csp;
	size_t csp_expected_inner;
	size_t  csp_expected;
	auth_identity* identity = make_test_smartcard_creds();
	reset_counters();
	WLog_INFO(TAG, "Testing %s", __FUNCTION__);
	csp = identity->creds.smartcard_creds->csp_data;
	csp_expected_inner = (ber_sizeof_contextual_tag(ber_sizeof_integer(csp->KeySpec))
		+ ber_sizeof_integer(csp->KeySpec)
		+ ber_sizeof_sequence_octet_string(2 * string_length(csp->CardName))
		+ ber_sizeof_sequence_octet_string(2 * string_length(csp->ReaderName))
		+ ber_sizeof_sequence_octet_string(2 * string_length(csp->ContainerName))
		+ ber_sizeof_sequence_octet_string(2 * string_length(csp->CspName)));
	csp_expected = ber_sizeof_contextual_tag(ber_sizeof_sequence(csp_expected_inner))
			+ ber_sizeof_sequence(csp_expected_inner);

	{
		size_t result_inner = nla_sizeof_ts_cspdatadetail_inner(csp);
		size_t result = nla_sizeof_ts_cspdatadetail(csp);
		size_t written_size = stream_size(csp_expected, write_csp, csp);

		TEST(csp_expected_inner == result_inner,
			"cspdatadetail_inner expected = %d != %d = result_inner",
			csp_expected_inner, result_inner);
		TEST(csp_expected == result,
			"cspdatadetail expected = %d != %d = result",
			csp_expected, result);
		TEST(csp_expected == written_size,
			"cspdatadetail expected = %d != %d = written",
			csp_expected, written_size);
	}

	{
		smartcard_creds* creds = identity->creds.smartcard_creds;
		size_t expected_inner = (0
			+ ber_sizeof_sequence_octet_string(2 * string_length(creds->Pin))
			+ csp_expected
			+ ber_sizeof_sequence_octet_string(2 * string_length(creds->UserHint))
			+ ber_sizeof_sequence_octet_string(2 * string_length(creds->DomainHint)));
		size_t expected = ber_sizeof_sequence(expected_inner);
		size_t result_inner = nla_sizeof_ts_smartcard_creds_inner(creds);
		size_t result = nla_sizeof_ts_smartcard_creds(creds);
		size_t written_size = stream_size(expected, (writer_pr)nla_write_ts_smartcard_creds, creds);

		TEST(expected_inner == result_inner,
			"smartcard_creds_inner expected = %d != %d = result",
			expected_inner, result_inner);
		TEST(expected == result,
			"smartcard_creds expected = %d != %d = result",
			expected, result);
		TEST(expected == written_size,
			"smartcard_creds expected = %d != %d = written",
			expected, written_size);
	}

	auth_identity_free(identity);
	return (failure_count == 0) && (error_count == 0);
}

BOOL test_sizeof_ts_credentials()
{
	auth_identity* identity = make_test_smartcard_creds();
	reset_counters();
	WLog_INFO(TAG, "Testing %s", __FUNCTION__);

	{
		size_t expected_inner = nla_sizeof_ts_credentials_inner(identity);
		size_t expected = ber_sizeof_sequence(expected_inner);
		size_t result_inner = nla_sizeof_ts_credentials_inner(identity);
		size_t written_size = stream_size(expected, (writer_pr)nla_write_ts_credentials, identity);
		size_t result = expected;

		TEST(expected_inner == result_inner,
			"credentials_inner expected = %d != %d = result",
			expected_inner, result_inner);
		TEST(expected == result,
			"credentials expected = %d != %d = result",
			expected, result);
		TEST(expected == written_size,
			"credentials expected = %d != %d = written",
			expected, written_size);
	}

	auth_identity_free(identity);
	return (failure_count == 0) && (error_count == 0);
}


void compare_buffers(BYTE* expected_ber, size_t expected_length, BYTE* result_ber, size_t result_length)
{
	TEST(result_length == expected_length, "result length = %d != %d = expected length", result_length,
		expected_length);
	TEST(0 == memcmp(result_ber, expected_ber, min(result_length, expected_length)),
		"BER bytes differ");

	if (failure_count > 0)
	{
		FAILURE();
		WLog_ERR(TAG, "==== Expected:");
		winpr_HexDump(TAG, WLOG_ERROR, expected_ber, expected_length);
		WLog_ERR(TAG, "==== Result:");
		winpr_HexDump(TAG, WLOG_ERROR, result_ber, result_length);
	}
}

void test_creds(auth_identity* identity, BYTE* expected_ber, size_t expected_length)
{
	size_t creds_size;
	size_t written_size;
	size_t result_length;
	BYTE* result_ber;
	wStream* s;
	creds_size = nla_sizeof_ts_creds(identity);
	WLog_INFO(TAG, "ts_creds  size   = %4d", creds_size);
	s = Stream_New(NULL, creds_size);
	written_size = nla_write_ts_creds(identity, s);
	TEST(written_size == creds_size, "written_size = %d ; creds_size = %d", written_size, creds_size);
	WLog_INFO(TAG, "written   size   = %4d", written_size);
	Stream_SealLength(s);
	result_length = Stream_Length(s);
	TEST(written_size == result_length, "written_size = %d ; result_length = %d", written_size,
		result_length);
	WLog_INFO(TAG, "expected length  = %4d", expected_length);
	WLog_INFO(TAG, "result   length  = %4d", result_length);
	result_ber = Stream_Buffer(s);
	compare_buffers(expected_ber, expected_length, result_ber, result_length);
	Stream_Free(s, TRUE);
}

BOOL test_write_smartcard_creds()
{
	static BYTE expected_ber[] =
	{
		0x30, 0x82, 0x01, 0x2f, 0xa0, 0x0a, 0x04, 0x08, 0x30, 0x00, 0x30, 0x00, 0x30, 0x00, 0x30, 0x00,
		0xa1, 0x81, 0xe6, 0x30, 0x81, 0xe3, 0xa0, 0x03, 0x02, 0x01, 0x01, 0xa1, 0x10, 0x04, 0x0e, 0x49,
		0x00, 0x41, 0x00, 0x53, 0x00, 0x2d, 0x00, 0x45, 0x00, 0x43, 0x00, 0x43, 0x00, 0xa2, 0x42, 0x04,
		0x40, 0x58, 0x00, 0x49, 0x00, 0x52, 0x00, 0x49, 0x00, 0x4e, 0x00, 0x47, 0x00, 0x20, 0x00, 0x4c,
		0x00, 0x65, 0x00, 0x6f, 0x00, 0x20, 0x00, 0x76, 0x00, 0x32, 0x00, 0x20, 0x00, 0x28, 0x00, 0x38,
		0x00, 0x32, 0x00, 0x38, 0x00, 0x38, 0x00, 0x38, 0x00, 0x33, 0x00, 0x30, 0x00, 0x36, 0x00, 0x32,
		0x00, 0x33, 0x00, 0x29, 0x00, 0x20, 0x00, 0x30, 0x00, 0x30, 0x00, 0x20, 0x00, 0x30, 0x00, 0x30,
		0x00, 0xa3, 0x30, 0x04, 0x2e, 0x43, 0x00, 0x6c, 0x00, 0xe9, 0x00, 0x20, 0x00, 0x64, 0x00, 0x27,
		0x00, 0x61, 0x00, 0x75, 0x00, 0x74, 0x00, 0x68, 0x00, 0x65, 0x00, 0x6e, 0x00, 0x74, 0x00, 0x69,
		0x00, 0x66, 0x00, 0x69, 0x00, 0x63, 0x00, 0x61, 0x00, 0x74, 0x00, 0x69, 0x00, 0x6f, 0x00, 0x6e,
		0x00, 0x31, 0x00, 0xa4, 0x54, 0x04, 0x52, 0x4d, 0x00, 0x69, 0x00, 0x64, 0x00, 0x64, 0x00, 0x6c,
		0x00, 0x65, 0x00, 0x77, 0x00, 0x61, 0x00, 0x72, 0x00, 0x65, 0x00, 0x20, 0x00, 0x49, 0x00, 0x41,
		0x00, 0x53, 0x00, 0x20, 0x00, 0x45, 0x00, 0x43, 0x00, 0x43, 0x00, 0x20, 0x00, 0x43, 0x00, 0x72,
		0x00, 0x79, 0x00, 0x70, 0x00, 0x74, 0x00, 0x6f, 0x00, 0x67, 0x00, 0x72, 0x00, 0x61, 0x00, 0x70,
		0x00, 0x68, 0x00, 0x69, 0x00, 0x63, 0x00, 0x20, 0x00, 0x50, 0x00, 0x72, 0x00, 0x6f, 0x00, 0x76,
		0x00, 0x69, 0x00, 0x64, 0x00, 0x65, 0x00, 0x72, 0x00, 0xa2, 0x18, 0x04, 0x16, 0x45, 0x00, 0x58,
		0x00, 0x41, 0x00, 0x4d, 0x00, 0x50, 0x00, 0x4c, 0x00, 0x45, 0x00, 0x55, 0x00, 0x53, 0x00, 0x45,
		0x00, 0x52, 0x00, 0xa3, 0x1e, 0x04, 0x1c, 0x45, 0x00, 0x58, 0x00, 0x41, 0x00, 0x4d, 0x00, 0x50,
		0x00, 0x4c, 0x00, 0x45, 0x00, 0x2e, 0x00, 0x44, 0x00, 0x4f, 0x00, 0x4d, 0x00, 0x41, 0x00, 0x49,
		0x00, 0x4e, 0x00,
	};
	static size_t expected_length = sizeof(expected_ber);
	auth_identity* identity = make_test_smartcard_creds();
	reset_counters();
	WLog_INFO(TAG, "Testing %s", __FUNCTION__);
	test_creds(identity, expected_ber, expected_length);
	auth_identity_free(identity);
	return (failure_count == 0) && (error_count == 0);
}

BOOL test_write_remote_guard_creds()
{
	static BYTE expected_ber[] =
	{
		0x30, 0x81, 0xc4, 0xa0, 0x44, 0x30, 0x42, 0xa0, 0x24, 0x04, 0x22, 0x4d, 0x00, 0x79, 0x00, 0x53,
		0x00, 0x65, 0x00, 0x63, 0x00, 0x75, 0x00, 0x72, 0x00, 0x69, 0x00, 0x74, 0x00, 0x79, 0x00, 0x50,
		0x00, 0x61, 0x00, 0x63, 0x00, 0x6b, 0x00, 0x61, 0x00, 0x67, 0x00, 0x65, 0x00, 0xa1, 0x1a, 0x04,
		0x18, 0x4d, 0x79, 0x20, 0x42, 0x72, 0x65, 0x61, 0x74, 0x68, 0x20, 0x49, 0x73, 0x20, 0x4d, 0x79,
		0x20, 0x50, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0xa1, 0x7c, 0x30, 0x7a, 0x30, 0x5a, 0xa0,
		0x36, 0x04, 0x34, 0x41, 0x00, 0x6c, 0x00, 0x74, 0x00, 0x65, 0x00, 0x72, 0x00, 0x6e, 0x00, 0x61,
		0x00, 0x74, 0x00, 0x69, 0x00, 0x76, 0x00, 0x65, 0x00, 0x53, 0x00, 0x65, 0x00, 0x63, 0x00, 0x75,
		0x00, 0x72, 0x00, 0x69, 0x00, 0x74, 0x00, 0x79, 0x00, 0x50, 0x00, 0x61, 0x00, 0x63, 0x00, 0x6b,
		0x00, 0x61, 0x00, 0x67, 0x00, 0x65, 0x00, 0xa1, 0x20, 0x04, 0x1e, 0x4d, 0x79, 0x20, 0x42, 0x72,
		0x65, 0x61, 0x74, 0x68, 0x20, 0x49, 0x73, 0x20, 0x4d, 0x79, 0x20, 0x4f, 0x74, 0x68, 0x65, 0x72,
		0x20, 0x50, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0x30, 0x1c, 0xa0, 0x08, 0x04, 0x06, 0x50,
		0x00, 0x41, 0x00, 0x4d, 0x00, 0xa1, 0x10, 0x04, 0x0e, 0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72, 0x62,
		0x61, 0x7a, 0x21, 0x70, 0x61, 0x73, 0x73
	};
	static size_t expected_length = sizeof(expected_ber);
	auth_identity* identity = make_test_remote_guard_creds();
	reset_counters();
	WLog_INFO(TAG, "Testing %s", __FUNCTION__);
	test_creds(identity, expected_ber, expected_length);
	auth_identity_free(identity);
	return (failure_count == 0) && (error_count == 0);
}

int TestTSCredentials(int argc, char* argv[])
{
	return ((test_sizeof_smartcard_creds() &&
			test_sizeof_ts_credentials() &&
			test_write_smartcard_creds() &&
			test_write_remote_guard_creds())
	        ? 0
	        : 1);
}
