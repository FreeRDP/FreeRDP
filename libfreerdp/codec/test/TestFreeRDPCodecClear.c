#include <winpr/crt.h>
#include <winpr/print.h>

#include <freerdp/codec/clear.h>

static BYTE TEST_CLEAR_EXAMPLE_1[] = "\x03\xc3\x11\x00";

static BYTE TEST_CLEAR_EXAMPLE_2[] =
    "\x00\x0d\x00\x00\x00\x00\x00\x00\x00\x00\x82\x00\x00\x00\x00\x00"
    "\x00\x00\x4e\x00\x11\x00\x75\x00\x00\x00\x02\x0e\xff\xff\xff\x00"
    "\x00\x00\xdb\xff\xff\x00\x3a\x90\xff\xb6\x66\x66\xb6\xff\xb6\x66"
    "\x00\x90\xdb\xff\x00\x00\x3a\xdb\x90\x3a\x3a\x90\xdb\x66\x00\x00"
    "\xff\xff\xb6\x64\x64\x64\x11\x04\x11\x4c\x11\x4c\x11\x4c\x11\x4c"
    "\x11\x4c\x00\x47\x13\x00\x01\x01\x04\x00\x01\x00\x00\x47\x16\x00"
    "\x11\x02\x00\x47\x29\x00\x11\x01\x00\x49\x0a\x00\x01\x00\x04\x00"
    "\x01\x00\x00\x4a\x0a\x00\x09\x00\x01\x00\x00\x47\x05\x00\x01\x01"
    "\x1c\x00\x01\x00\x11\x4c\x11\x4c\x11\x4c\x00\x47\x0d\x4d\x00\x4d";

static BYTE TEST_CLEAR_EXAMPLE_3[] =
    "\x00\xdf\x0e\x00\x00\x00\x8b\x00\x00\x00\x00\x00\x00\x00\xfe\xfe"
    "\xfe\xff\x80\x05\xff\xff\xff\x40\xfe\xfe\xfe\x40\x00\x00\x3f\x00"
    "\x03\x00\x0b\x00\xfe\xfe\xfe\xc5\xd0\xc6\xd0\xc7\xd0\x68\xd4\x69"
    "\xd4\x6a\xd4\x6b\xd4\x6c\xd4\x6d\xd4\x1a\xd4\x1a\xd4\xa6\xd0\x6e"
    "\xd4\x6f\xd4\x70\xd4\x71\xd4\x72\xd4\x73\xd4\x74\xd4\x21\xd4\x22"
    "\xd4\x23\xd4\x24\xd4\x25\xd4\xd9\xd0\xda\xd0\xdb\xd0\xc5\xd0\xc5"
    "\xd0\xdc\xd0\xc2\xd0\x21\xd4\x22\xd4\x23\xd4\x24\xd4\x25\xd4\xc9"
    "\xd0\xca\xd0\x5a\xd4\x2b\xd1\x28\xd1\x2c\xd1\x75\xd4\x27\xd4\x28"
    "\xd4\x29\xd4\x2a\xd4\x1a\xd4\x1a\xd4\x1a\xd4\xb7\xd0\xb8\xd0\xb9"
    "\xd0\xba\xd0\xbb\xd0\xbc\xd0\xbd\xd0\xbe\xd0\xbf\xd0\xc0\xd0\xc1"
    "\xd0\xc2\xd0\xc3\xd0\xc4\xd0";

static BYTE TEST_CLEAR_EXAMPLE_4[] =
    "\x01\x0b\x78\x00\x00\x00\x00\x00\x46\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x06\x00\x00\x00\x0e\x00\x00\x00\x00\x00\x0f\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xb6\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xb6\x66\xff\xff\xff\xff\xff\xff\xff\xb6\x66\xdb\x90\x3a\xff\xff"
    "\xb6\xff\xff\xff\xff\xff\xff\xff\xff\xff\x46\x91\x47\x91\x48\x91"
    "\x49\x91\x4a\x91\x1b\x91";

static int test_ClearDecompressExample1(void)
{
	int status;
	BYTE* pSrcData;
	UINT32 SrcSize;
	BYTE pDstData[16384];
	CLEAR_CONTEXT* clear;
	clear = clear_context_new(FALSE);
	SrcSize = sizeof(TEST_CLEAR_EXAMPLE_1) - 1;
	pSrcData = (BYTE*) TEST_CLEAR_EXAMPLE_1;
	status = clear_decompress(clear, pSrcData, SrcSize, 0, 0,
	                          pDstData, PIXEL_FORMAT_XRGB32, 64 * 4, 1, 1, 64, 64,
	                          NULL);
	printf("clear_decompress example 1 status: %d\n", status);
	clear_context_free(clear);
	return status;
}

static int test_ClearDecompressExample2(void)
{
	int status;
	BYTE* pSrcData;
	UINT32 SrcSize;
	BYTE* pDstData = NULL;
	CLEAR_CONTEXT* clear;
	clear = clear_context_new(FALSE);
	SrcSize = sizeof(TEST_CLEAR_EXAMPLE_2) - 1;
	pSrcData = (BYTE*) TEST_CLEAR_EXAMPLE_2;
	status = clear_decompress(clear, pSrcData, SrcSize, 0, 0,
	                          pDstData, PIXEL_FORMAT_XRGB32, 0, 0, 0, 0, 0,
	                          NULL);
	printf("clear_decompress example 2 status: %d\n", status);
	clear_context_free(clear);
	return status;
}

static int test_ClearDecompressExample3(void)
{
	int status;
	BYTE* pSrcData;
	UINT32 SrcSize;
	BYTE* pDstData = NULL;
	CLEAR_CONTEXT* clear;
	clear = clear_context_new(FALSE);
	SrcSize = sizeof(TEST_CLEAR_EXAMPLE_3) - 1;
	pSrcData = (BYTE*) TEST_CLEAR_EXAMPLE_3;
	status = clear_decompress(clear, pSrcData, SrcSize, 0, 0,
	                          pDstData, PIXEL_FORMAT_XRGB32, 0, 0, 0, 0, 0,
	                          NULL);
	printf("clear_decompress example 3 status: %d\n", status);
	clear_context_free(clear);
	return status;
}

static int test_ClearDecompressExample4(void)
{
	int status;
	BYTE* pSrcData;
	UINT32 SrcSize;
	BYTE* pDstData = NULL;
	CLEAR_CONTEXT* clear;
	clear = clear_context_new(FALSE);
	SrcSize = sizeof(TEST_CLEAR_EXAMPLE_4) - 1;
	pSrcData = (BYTE*) TEST_CLEAR_EXAMPLE_4;
	status = clear_decompress(clear, pSrcData, SrcSize, 0, 0,
	                          pDstData, PIXEL_FORMAT_XRGB32, 0, 0, 0, 0, 0,
	                          NULL);
	printf("clear_decompress example 4 status: %d\n", status);
	clear_context_free(clear);
	return status;
}

int TestFreeRDPCodecClear(int argc, char* argv[])
{
	if (test_ClearDecompressExample1())
		return -1;

	if (test_ClearDecompressExample2())
		return -1;

	if (test_ClearDecompressExample3())
		return -1;

	if (test_ClearDecompressExample4())
		return -1;

	fprintf(stderr, "TODO: %s not implemented!!!\n", __FUNCTION__);
	return 0;
}

