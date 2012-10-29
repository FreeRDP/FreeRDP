
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <google/cmockery.h>

#include <winpr/crt.h>
#include <winpr/winpr.h>
#include <winpr/print.h>
#include <freerdp/freerdp.h>

#include "rts.h"

/* mocks */

extern void rts_generate_cookie(BYTE* cookie);
extern int rpc_in_write(rdpRpc* rpc, BYTE* data, int length);
extern int rpc_out_write(rdpRpc* rpc, BYTE* data, int length);

BYTE testCookie[16] = "\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC";

void rts_generate_cookie(BYTE* cookie)
{
	CopyMemory(cookie, testCookie, 16);
}

int rpc_in_write(rdpRpc* rpc, BYTE* data, int length)
{
	printf("rpc_in_write: %d\n", length);
	//winpr_HexDump(data, length);
	return length;
}

int rpc_out_write(rdpRpc* rpc, BYTE* data, int length)
{
	printf("rpc_out_write: %d\n", length);
	//winpr_HexDump(data, length);
	return length;
}

/* tests */

void test_rts_generate_cookie(void **state)
{
	BYTE cookie[16];
	rts_generate_cookie(cookie);
	assert_memory_equal(cookie, testCookie, 16);
}

void test_rpc_in_write(void **state)
{
	int status;
	status = rpc_in_write(NULL, NULL, 64);
	assert_int_equal(status, 64);
}

int TestCoreRts(int argc, char* argv[])
{
	const UnitTest tests[] =
	{
		unit_test(test_rts_generate_cookie),
		unit_test(test_rpc_in_write),
	};

	return run_tests(tests);
}
