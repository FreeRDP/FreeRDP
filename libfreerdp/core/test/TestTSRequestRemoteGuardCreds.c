#include <stdio.h>
#include <string.h>

#include <winpr/sspi.h>
#include <winpr/wlog.h>
#include <winpr/stream.h>

#include <freerdp/log.h>
#include <freerdp/crypto/ber.h>

#include "../tscredentials.h"

#define TAG FREERDP_TAG("test.TestTSRequestRemoteGuardCreds")

#define printref() printf("%s:%d: in function %-40s:", __FILE__, __LINE__, __FUNCTION__)

#define ERROR(format, ...)                                  \
	do{                                                 \
		fprintf(stderr, format, ##__VA_ARGS__);     \
		printref();                                 \
		printf(format "\n", ##__VA_ARGS__);	    \
		fflush(stdout);                             \
		error_count++;				    \
	}while(0)

#define FAILURE(format, ...)                                \
	do{                                                 \
		printref();                                 \
		printf(" FAILURE ");                        \
		printf(format "\n", ##__VA_ARGS__);	    \
		fflush(stdout);                             \
		failure_count++;			    \
	}while(0)


#define TEST(condition, format, ...)			    \
	if (!(condition))				    \
	{						    \
		FAILURE("test %s " format "\n", #condition, \
			##__VA_ARGS__);			    \
	}

size_t min(size_t a, size_t b)
{
	return (a < b)?a:b;
}

BOOL test_tscredential_write()
{
	unsigned failure_count = 0;
	unsigned error_count = 0;

	/*
	ASN.1 value:

	value TSRemoteGuardCreds :: =
	{
	logonCred {
	packageName  '4D7953656375726974795061636B616765'H,
	credBuffer   '4D7920427265617468204973204D792050617373776F7264'H
	},
	supplementalCreds {
	{
	packageName '416C7465726E617469766553656375726974795061636B616765'H,
	credBuffer  '4D7920427265617468204973204D79204F746865722050617373776F7264'H
	},
	{
	packageName '50414D'H,
	credBuffer  '666F6F62617262617A2170617373'H
	}
	}
	}
	*/
	static BYTE expected_ber[] = {
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

	char * package_name;
	char * credentials;
	remote_guard_creds * remote_guard_creds;
	auth_identity* identity;
	size_t creds_size;
	size_t written_size;
	size_t result_length;
	BYTE * result_ber;
	wStream * s;

	WLog_INFO(TAG, "Testing %s", "test_tscredential_write");
	package_name = strdup("MySecurityPackage");
	credentials  = strdup("My Breath Is My Password");
	remote_guard_creds = remote_guard_creds_new_nocopy(package_name, strlen(credentials), (BYTE*)credentials);
	package_name = strdup("AlternativeSecurityPackage");
	credentials  = strdup("My Breath Is My Other Password");
	remote_guard_creds_add_supplemental_cred(remote_guard_creds, remote_guard_package_cred_new_nocopy(package_name, strlen(credentials), (BYTE*)credentials));
	package_name = strdup("PAM");
	credentials  = strdup("foobarbaz!pass");
	remote_guard_creds_add_supplemental_cred(remote_guard_creds, remote_guard_package_cred_new_nocopy(package_name, strlen(credentials), (BYTE*)credentials));
	identity = auth_identity_new_remote_guard(remote_guard_creds);
	creds_size = nla_sizeof_ts_creds(identity);
	WLog_INFO(TAG, "ts_creds  size   = %4d", creds_size);
	s = Stream_New(NULL, creds_size);
	written_size = nla_write_ts_creds(identity, s);
	TEST(written_size == creds_size, "written_size = %d ; creds_size = %d", written_size, creds_size);
	WLog_INFO(TAG, "written   size   = %4d", written_size);
	Stream_SealLength(s);
	result_length = Stream_Length(s);
	TEST(written_size == result_length, "written_size = %d ; result_length = %d", written_size, result_length);
	WLog_INFO(TAG, "expected length  = %4d", expected_length);
	WLog_INFO(TAG, "result   length  = %4d", result_length);
	result_ber = Stream_Buffer(s);
	TEST(result_length == expected_length, "result length = %d != %d = expected length", result_length, expected_length);
	TEST(0 == memcmp(result_ber, expected_ber, min(result_length, expected_length)),"BER bytes differ");
	if (failure_count > 0)
	{
		FAILURE();
		WLog_ERR(TAG, "==== Expected:");
		winpr_HexDump(TAG, WLOG_ERROR, expected_ber, expected_length);
		WLog_ERR(TAG, "==== Result:");
		winpr_HexDump(TAG, WLOG_ERROR, result_ber, result_length);
	}
	Stream_Free(s, TRUE);
	auth_identity_free(identity);
	return (failure_count == 0) && (error_count == 0);
}


int TestTSRequestRemoteGuardCreds(int argc, char* argv[])
{
	if (test_tscredential_write())
	{
		return 0;
	}
	else
	{
		return 1;
	}
}
