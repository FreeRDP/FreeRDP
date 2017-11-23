#include <winpr/crt.h>
#include <winpr/print.h>

#include <freerdp/codec/ncrush.h>

static const BYTE TEST_BELLS_DATA[] = "for.whom.the.bell.tolls,.the.bell.tolls.for.thee!";

static const BYTE TEST_BELLS_NCRUSH[] =
    "\xfb\x1d\x7e\xe4\xda\xc7\x1d\x70\xf8\xa1\x6b\x1f\x7d\xc0\xbe\x6b"
    "\xef\xb5\xef\x21\x87\xd0\xc5\xe1\x85\x71\xd4\x10\x16\xe7\xda\xfb"
    "\x1d\x7e\xe4\xda\x47\x1f\xb0\xef\xbe\xbd\xff\x2f";

static BOOL test_NCrushCompressBells(void)
{
	BOOL rc = FALSE;
	int status;
	UINT32 Flags;
	UINT32 SrcSize;
	BYTE* pSrcData;
	UINT32 DstSize;
	BYTE* pDstData;
	UINT32 expectedSize;
	BYTE OutputBuffer[65536];
	NCRUSH_CONTEXT* ncrush = ncrush_context_new(TRUE);

	if (!ncrush)
		return rc;

	SrcSize = sizeof(TEST_BELLS_DATA) - 1;
	pSrcData = (BYTE*) TEST_BELLS_DATA;
	expectedSize = sizeof(TEST_BELLS_NCRUSH) - 1;
	pDstData = OutputBuffer;
	DstSize = sizeof(OutputBuffer);
	ZeroMemory(OutputBuffer, sizeof(OutputBuffer));
	status = ncrush_compress(ncrush, pSrcData, SrcSize, &pDstData, &DstSize, &Flags);

	if (status < 0)
		goto fail;

	printf("status: %d Flags: 0x%08"PRIX32" DstSize: %"PRIu32"\n", status, Flags, DstSize);

	if (DstSize != expectedSize)
	{
		printf("NCrushCompressBells: output size mismatch: Actual: %"PRIu32", Expected: %"PRIu32"\n",
		       DstSize, expectedSize);
		printf("Actual\n");
		BitDump(__FUNCTION__, WLOG_INFO, pDstData, DstSize * 8, 0);
		printf("Expected\n");
		BitDump(__FUNCTION__, WLOG_INFO, TEST_BELLS_NCRUSH, expectedSize * 8, 0);
		goto fail;
	}

	if (memcmp(pDstData, TEST_BELLS_NCRUSH, DstSize) != 0)
	{
		printf("NCrushCompressBells: output mismatch\n");
		printf("Actual\n");
		BitDump(__FUNCTION__, WLOG_INFO, pDstData, DstSize * 8, 0);
		printf("Expected\n");
		BitDump(__FUNCTION__, WLOG_INFO, TEST_BELLS_NCRUSH, expectedSize * 8, 0);
		goto fail;
	}

	rc = TRUE;
fail:
	ncrush_context_free(ncrush);
	return rc;
}

static BOOL test_NCrushDecompressBells(void)
{
	BOOL rc = FALSE;
	int status;
	UINT32 Flags;
	BYTE* pSrcData;
	UINT32 SrcSize;
	UINT32 DstSize;
	UINT32 expectedSize;
	BYTE* pDstData = NULL;
	NCRUSH_CONTEXT* ncrush = ncrush_context_new(FALSE);

	if (!ncrush)
		return rc;

	SrcSize = sizeof(TEST_BELLS_NCRUSH) - 1;
	pSrcData = (BYTE*) TEST_BELLS_NCRUSH;
	Flags = PACKET_COMPRESSED | 2;
	expectedSize = sizeof(TEST_BELLS_DATA) - 1;
	status = ncrush_decompress(ncrush, pSrcData, SrcSize, &pDstData, &DstSize, Flags);

	if (status < 0)
		goto fail;

	printf("Flags: 0x%08"PRIX32" DstSize: %"PRIu32"\n", Flags, DstSize);

	if (DstSize != expectedSize)
	{
		printf("NCrushDecompressBells: output size mismatch: Actual: %"PRIu32", Expected: %"PRIu32"\n",
		       DstSize, expectedSize);
		goto fail;
	}

	if (memcmp(pDstData, TEST_BELLS_DATA, DstSize) != 0)
	{
		printf("NCrushDecompressBells: output mismatch\n");
		goto fail;
	}

	rc = TRUE;
fail:
	ncrush_context_free(ncrush);
	return rc;
}

int TestFreeRDPCodecNCrush(int argc, char* argv[])
{
	if (!test_NCrushCompressBells())
		return -1;

	if (!test_NCrushDecompressBells())
		return -1;

	return 0;
}
