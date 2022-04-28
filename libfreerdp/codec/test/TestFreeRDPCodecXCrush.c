#include <winpr/crt.h>
#include <winpr/print.h>

#include "../xcrush.h"

static const BYTE TEST_BELLS_DATA[] = "for.whom.the.bell.tolls,.the.bell.tolls.for.thee!";

static const BYTE TEST_BELLS_DATA_XCRUSH[] =
    "\x12\x00\x66\x6f\x72\x2e\x77\x68\x6f\x6d\x2e\x74\x68\x65\x2e\x62"
    "\x65\x6c\x6c\x2e\x74\x6f\x6c\x6c\x73\x2c\x2e\x74\x68\x65\x2e\x62"
    "\x65\x6c\x6c\x2e\x74\x6f\x6c\x6c\x73\x2e\x66\x6f\x72\x2e\x74\x68"
    "\x65";

static const BYTE TEST_ISLAND_DATA[] = "No man is an island entire of itself; every man "
                                       "is a piece of the continent, a part of the main; "
                                       "if a clod be washed away by the sea, Europe "
                                       "is the less, as well as if a promontory were, as"
                                       "well as any manner of thy friends or of thine "
                                       "own were; any man's death diminishes me, "
                                       "because I am involved in mankind. "
                                       "And therefore never send to know for whom "
                                       "the bell tolls; it tolls for thee.";

static const BYTE TEST_ISLAND_DATA_XCRUSH[] =
    "\x12\x61\x4e\x6f\x20\x6d\x61\x6e\x20\x69\x73\x20\xf8\xd2\xd8\xc2"
    "\xdc\xc8\x40\xca\xdc\xe8\xd2\xe4\xca\x40\xde\xcc\x40\xd2\xe8\xe6"
    "\xca\xd8\xcc\x76\x40\xca\xec\xca\xe4\xf3\xfa\x71\x20\x70\x69\x65"
    "\x63\xfc\x12\xe8\xd0\xca\x40\xc6\xdf\xfb\xcd\xdf\xd0\x58\x40\xc2"
    "\x40\xe0\xc2\xe4\xe9\xfe\x63\xec\xc3\x6b\x0b\x4b\x71\xd9\x03\x4b"
    "\x37\xd7\x31\xb6\x37\xb2\x10\x31\x32\x90\x3b\xb0\xb9\xb4\x32\xb2"
    "\x10\x30\xbb\xb0\xbc\x90\x31\x3c\x90\x7e\x68\x73\x65\x61\x2c\x20"
    "\x45\x75\x72\x6f\x70\x65\xf2\x34\x7d\x38\x6c\x65\x73\x73\xf0\x69"
    "\xcc\x81\xdd\x95\xb1\xb0\x81\x85\xcf\xc0\x94\xe0\xe4\xde\xdb\xe2"
    "\xb3\x7f\x92\x4e\xec\xae\x4c\xbf\x86\x3f\x06\x0c\x2d\xde\x5d\x96"
    "\xe6\x57\x2f\x1e\x53\xc9\x03\x33\x93\x4b\x2b\x73\x23\x99\x03\x7f"
    "\xd2\xb6\x96\xef\x38\x1d\xdb\xbc\x24\x72\x65\x3b\xf5\x5b\xf8\x49"
    "\x3b\x99\x03\x23\x2b\x0b\xa3\x41\x03\x23\x4b\x6b\x4b\x73\x4f\x96"
    "\xce\x64\x0d\xbe\x19\x31\x32\xb1\xb0\xba\xb9\xb2\x90\x24\x90\x30"
    "\xb6\x90\x34\xb7\x3b\x37\xb6\x3b\x79\xd4\xd2\xdd\xec\x18\x6b\x69"
    "\x6e\x64\x2e\x20\x41\xf7\x33\xcd\x47\x26\x56\x66\xff\x74\x9b\xbd"
    "\xbf\x04\x0e\x7e\x31\x10\x3a\x37\x90\x35\xb7\x37\xbb\x90\x7d\x81"
    "\x03\xbb\x43\x7b\x6f\xa8\xe5\x8b\xd0\xf0\xe8\xde\xd8\xd8\xe7\xec"
    "\xf3\xa7\xe4\x7c\xa7\xe2\x9f\x01\x99\x4b\x80";

static void test_dump(const char* fkt, const void* generated, size_t generated_size,
                      const void* expected, size_t expected_size)
{
	printf("[%s] output size mismatch: Actual: %" PRIuz ", Expected: %" PRIuz "\n", fkt,
	       generated_size, expected_size);
	printf("[%s] Actual\n", fkt);
	BitDump(fkt, WLOG_INFO, generated, generated_size * 8ull, 0);
	printf("[%s] Expected\n", fkt);
	BitDump(fkt, WLOG_INFO, expected, expected_size * 8ull, 0);
}

static BOOL test_compare(const char* fkt, const void* generated, size_t generated_size,
                         const void* expected, size_t expected_size)
{
	if (generated_size != expected_size)
	{
		test_dump(fkt, generated, generated_size, expected, expected_size);
		return FALSE;
	}

	if (memcmp(generated, expected, generated_size) != 0)
	{
		test_dump(fkt, generated, generated_size, expected, expected_size);
		return FALSE;
	}

	return TRUE;
}

static BOOL test_run(const char* fkt, const void* src, UINT32 src_size, const void* expected,
                     size_t expected_size)
{
	BOOL rc = FALSE;
	int status = -1;
	UINT32 Flags = 0;
	const BYTE* pDstData = NULL;
	BYTE OutputBuffer[65536] = { 0 };
	UINT32 DstSize = sizeof(OutputBuffer);
	XCRUSH_CONTEXT* xcrush = xcrush_context_new(TRUE);
	if (!xcrush)
		return -1;
	status = xcrush_compress(xcrush, src, src_size, OutputBuffer, &pDstData, &DstSize, &Flags);
	printf("[%s] status: %d Flags: 0x%08" PRIX32 " DstSize: %" PRIu32 "\n", fkt, status, Flags,
	       DstSize);

	rc = test_compare(fkt, pDstData, DstSize, expected, expected_size);

	xcrush_context_free(xcrush);
	return rc;
}

struct test_argument
{
	const char* name;
	const void* src;
	UINT32 src_size;
	const void* expected;
	size_t expected_size;
};

static const struct test_argument tests[] = {
	{ "XCrushCompressIsland", TEST_ISLAND_DATA, sizeof(TEST_ISLAND_DATA) - 1,
	  TEST_ISLAND_DATA_XCRUSH, sizeof(TEST_ISLAND_DATA_XCRUSH) - 1 }
#if 0
	,{ "XCrushCompressBells", TEST_BELLS_DATA, sizeof(TEST_BELLS_DATA) - 1, TEST_BELLS_DATA_XCRUSH,
	  sizeof(TEST_BELLS_DATA_XCRUSH) - 1 }
#endif
};

int TestFreeRDPCodecXCrush(int argc, char* argv[])
{
	int rc = 0;
	size_t x;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	for (x = 0; x < ARRAYSIZE(tests); x++)
	{
		const struct test_argument* arg = &tests[x];

		if (!test_run(arg->name, arg->src, arg->src_size, arg->expected, arg->expected_size))
			rc = -1;
	}

	return rc;
}
