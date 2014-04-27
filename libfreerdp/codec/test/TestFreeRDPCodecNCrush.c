#include <winpr/crt.h>
#include <winpr/print.h>

#include <freerdp/codec/ncrush.h>

static const BYTE TEST_BELLS_DATA[] = "for.whom.the.bell.tolls,.the.bell.tolls.for.thee!";

const BYTE TEST_BELLS_NCRUSH[] =
	"\xfb\x1d\x7e\xe4\xda\xc7\x1d\x70\xf8\xa1\x6b\x1f\x7d\xc0\xbe\x6b"
	"\xef\xb5\xef\x21\x87\xd0\xc5\xe1\x85\x71\xd4\x10\x16\xe7\xda\xfb"
	"\x1d\x7e\xe4\xda\x47\x1f\xb0\xef\xbe\xbd\xff\x2f";

int test_NCrushCompressBells()
{
	int status;
	UINT32 Flags;
	UINT32 SrcSize;
	BYTE* pSrcData;
	UINT32 DstSize;
	BYTE* pDstData;
	UINT32 expectedSize;
	BYTE OutputBuffer[65536];
	NCRUSH_CONTEXT* ncrush;

	ncrush = ncrush_context_new(TRUE);

	SrcSize = sizeof(TEST_BELLS_DATA) - 1;
	pSrcData = (BYTE*) TEST_BELLS_DATA;
	expectedSize = sizeof(TEST_BELLS_NCRUSH) - 1;

	pDstData = OutputBuffer;
	DstSize = sizeof(OutputBuffer);
	ZeroMemory(OutputBuffer, sizeof(OutputBuffer));

	status = ncrush_compress(ncrush, pSrcData, SrcSize, &pDstData, &DstSize, &Flags);

	printf("status: %d Flags: 0x%04X DstSize: %d\n", status, Flags, DstSize);

	if (DstSize != expectedSize)
	{
		printf("NCrushCompressBells: output size mismatch: Actual: %d, Expected: %d\n", DstSize, expectedSize);

		printf("Actual\n");
		BitDump(pDstData, DstSize * 8, 0);

		printf("Expected\n");
		BitDump(TEST_BELLS_NCRUSH, expectedSize * 8, 0);

		return -1;
	}

	if (memcmp(pDstData, TEST_BELLS_NCRUSH, DstSize) != 0)
	{
		printf("NCrushCompressBells: output mismatch\n");

		printf("Actual\n");
		BitDump(pDstData, DstSize * 8, 0);

		printf("Expected\n");
		BitDump(TEST_BELLS_NCRUSH, expectedSize * 8, 0);

		return -1;
	}

	ncrush_context_free(ncrush);

	return 1;
}

int test_NCrushDecompressBells()
{
	int status;
	UINT32 Flags;
	BYTE* pSrcData;
	UINT32 SrcSize;
	UINT32 DstSize;
	UINT32 expectedSize;
	BYTE* pDstData = NULL;
	NCRUSH_CONTEXT* ncrush;

	ncrush = ncrush_context_new(FALSE);

	SrcSize = sizeof(TEST_BELLS_NCRUSH) - 1;
	pSrcData = (BYTE*) TEST_BELLS_NCRUSH;
	Flags = PACKET_COMPRESSED | 2;
	expectedSize = sizeof(TEST_BELLS_DATA) - 1;

	status = ncrush_decompress(ncrush, pSrcData, SrcSize, &pDstData, &DstSize, Flags);
	printf("Flags: 0x%04X DstSize: %d\n", Flags, DstSize);

	if (DstSize != expectedSize)
	{
		printf("NCrushDecompressBells: output size mismatch: Actual: %d, Expected: %d\n", DstSize, expectedSize);
		return -1;
	}

	if (memcmp(pDstData, TEST_BELLS_DATA, DstSize) != 0)
	{
		printf("NCrushDecompressBells: output mismatch\n");
		return -1;
	}

	ncrush_context_free(ncrush);

	return 1;
}

int TestFreeRDPCodecNCrush(int argc, char* argv[])
{
	if (test_NCrushCompressBells() < 0)
		return -1;

	if (test_NCrushDecompressBells() < 0)
		return -1;

	return 0;
}
