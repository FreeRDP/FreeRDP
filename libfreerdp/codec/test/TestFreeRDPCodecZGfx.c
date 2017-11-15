#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/bitstream.h>

#include <freerdp/freerdp.h>
#include <freerdp/codec/zgfx.h>
#include <freerdp/log.h>

/* Sample from [MS-RDPEGFX] */
static const BYTE TEST_FOX_DATA[] =
    "The quick brown "
    "fox jumps over t"
    "he lazy dog";

static const BYTE TEST_FOX_DATA_SINGLE[] =
    "\xE0\x04\x54\x68\x65\x20\x71\x75\x69\x63\x6B\x20\x62\x72\x6F\x77"
    "\x6E\x20\x66\x6F\x78\x20\x6A\x75\x6D\x70\x73\x20\x6F\x76\x65\x72"
    "\x20\x74\x68\x65\x20\x6C\x61\x7A\x79\x20\x64\x6F\x67";

static const BYTE TEST_FOX_DATA_MULTIPART[] =
    "\xE1\x03\x00\x2B\x00\x00\x00\x11\x00\x00\x00\x04\x54\x68\x65\x20"
    "\x71\x75\x69\x63\x6B\x20\x62\x72\x6F\x77\x6E\x20\x0E\x00\x00\x00"
    "\x04\x66\x6F\x78\x20\x6A\x75\x6D\x70\x73\x20\x6F\x76\x65\x10\x00"
    "\x00\x00\x24\x39\x08\x0E\x91\xF8\xD8\x61\x3D\x1E\x44\x06\x43\x79"
    "\x9C\x02";

static int test_ZGfxCompressFox(void)
{
	int rc = -1;
	int status;
	UINT32 Flags;
	BYTE* pSrcData = NULL;
	UINT32 SrcSize;
	UINT32 DstSize;
	BYTE* pDstData = NULL;
	ZGFX_CONTEXT* zgfx;
	UINT32 expectedSize;
	zgfx = zgfx_context_new(TRUE);

	if (!zgfx)
		return -1;

	SrcSize = sizeof(TEST_FOX_DATA) - 1;
	pSrcData = (BYTE*) TEST_FOX_DATA;
	Flags = 0;
	expectedSize = sizeof(TEST_FOX_DATA_SINGLE) - 1;
	status = zgfx_compress(zgfx, pSrcData, SrcSize, &pDstData, &DstSize, &Flags);

	if (status < 0)
		goto fail;

	printf("flags: 0x%08"PRIX32" size: %"PRIu32"\n", Flags, DstSize);

	if (DstSize != expectedSize)
	{
		printf("test_ZGfxCompressFox: output size mismatch: Actual: %"PRIu32", Expected: %"PRIu32"\n",
		       DstSize, expectedSize);
		goto fail;
	}

	if (memcmp(pDstData, TEST_FOX_DATA_SINGLE, DstSize) != 0)
	{
		printf("test_ZGfxCompressFox: output mismatch\n");
		printf("Actual\n");
		BitDump(__FUNCTION__, WLOG_INFO, pDstData, DstSize * 8, 0);
		printf("Expected\n");
		BitDump(__FUNCTION__, WLOG_INFO, TEST_FOX_DATA_SINGLE, DstSize * 8, 0);
		goto fail;
	}

	rc = 0;
fail:
	free(pDstData);
	zgfx_context_free(zgfx);
	return rc;
}

static int test_ZGfxDecompressFoxSingle(void)
{
	int rc = -1;
	int status;
	UINT32 Flags;
	BYTE* pSrcData;
	UINT32 SrcSize;
	UINT32 DstSize;
	BYTE* pDstData = NULL;
	ZGFX_CONTEXT* zgfx;
	UINT32 expectedSize;
	zgfx = zgfx_context_new(TRUE);

	if (!zgfx)
		return -1;

	SrcSize = sizeof(TEST_FOX_DATA_SINGLE) - 1;
	pSrcData = (BYTE*) TEST_FOX_DATA_SINGLE;
	Flags = 0;
	expectedSize = sizeof(TEST_FOX_DATA) - 1;
	status = zgfx_decompress(zgfx, pSrcData, SrcSize, &pDstData, &DstSize, Flags);

	if (status < 0)
		goto fail;

	printf("flags: 0x%08"PRIX32" size: %"PRIu32"\n", Flags, DstSize);

	if (DstSize != expectedSize)
	{
		printf("test_ZGfxDecompressFoxSingle: output size mismatch: Actual: %"PRIu32", Expected: %"PRIu32"\n",
		       DstSize, expectedSize);
		goto fail;
	}

	if (memcmp(pDstData, TEST_FOX_DATA, DstSize) != 0)
	{
		printf("test_ZGfxDecompressFoxSingle: output mismatch\n");
		printf("Actual\n");
		BitDump(__FUNCTION__, WLOG_INFO, pDstData, DstSize * 8, 0);
		printf("Expected\n");
		BitDump(__FUNCTION__, WLOG_INFO, TEST_FOX_DATA, DstSize * 8, 0);
		goto fail;
	}

	rc = 0;
fail:
	free(pDstData);
	zgfx_context_free(zgfx);
	return rc;
}

static int test_ZGfxDecompressFoxMultipart(void)
{
	int rc = -1;
	int status;
	UINT32 Flags;
	BYTE* pSrcData;
	UINT32 SrcSize;
	UINT32 DstSize;
	BYTE* pDstData = NULL;
	ZGFX_CONTEXT* zgfx;
	UINT32 expectedSize;
	zgfx = zgfx_context_new(TRUE);

	if (!zgfx)
		return -1;

	SrcSize = sizeof(TEST_FOX_DATA_MULTIPART) - 1;
	pSrcData = (BYTE*) TEST_FOX_DATA_MULTIPART;
	Flags = 0;
	expectedSize = sizeof(TEST_FOX_DATA) - 1;
	status = zgfx_decompress(zgfx, pSrcData, SrcSize, &pDstData, &DstSize, Flags);

	if (status < 0)
		goto fail;

	printf("flags: 0x%08"PRIX32" size: %"PRIu32"\n", Flags, DstSize);

	if (DstSize != expectedSize)
	{
		printf("test_ZGfxDecompressFoxSingle: output size mismatch: Actual: %"PRIu32", Expected: %"PRIu32"\n",
		       DstSize, expectedSize);
		goto fail;
	}

	if (memcmp(pDstData, TEST_FOX_DATA, DstSize) != 0)
	{
		printf("test_ZGfxDecompressFoxSingle: output mismatch\n");
		printf("Actual\n");
		BitDump(__FUNCTION__, WLOG_INFO, pDstData, DstSize * 8, 0);
		printf("Expected\n");
		BitDump(__FUNCTION__, WLOG_INFO, TEST_FOX_DATA, DstSize * 8, 0);
		goto fail;
	}

	rc = 0;
fail:
	free(pDstData);
	zgfx_context_free(zgfx);
	return rc;
}

static int test_ZGfxCompressConsistent(void)
{
	int rc = -1;
	int status;
	UINT32 Flags;
	BYTE* pSrcData;
	UINT32 SrcSize;
	UINT32 DstSize;
	BYTE* pDstData = NULL;
	UINT32 DstSize2;
	BYTE* pDstData2 = NULL;
	ZGFX_CONTEXT* zgfx;
	UINT32 expectedSize;
	BYTE BigBuffer[65536];
	memset(BigBuffer, 0xaa, sizeof(BigBuffer));
	memcpy(BigBuffer, TEST_FOX_DATA, sizeof(TEST_FOX_DATA) - 1);
	zgfx = zgfx_context_new(TRUE);

	if (!zgfx)
		return -1;

	/* Compress */
	expectedSize = SrcSize = sizeof(BigBuffer);
	pSrcData = (BYTE*) BigBuffer;
	Flags = 0;
	status = zgfx_compress(zgfx, pSrcData, SrcSize, &pDstData2, &DstSize2, &Flags);

	if (status < 0)
		goto fail;

	printf("Compress: flags: 0x%08"PRIX32" size: %"PRIu32"\n", Flags, DstSize2);
	/* Decompress */
	status = zgfx_decompress(zgfx, pDstData2, DstSize2, &pDstData, &DstSize, Flags);

	if (status < 0)
		goto fail;

	printf("Decompress: flags: 0x%08"PRIX32" size: %"PRIu32"\n", Flags, DstSize);

	if (DstSize != expectedSize)
	{
		printf("test_ZGfxDecompressFoxSingle: output size mismatch: Actual: %"PRIu32", Expected: %"PRIu32"\n",
		       DstSize, expectedSize);
		goto fail;
	}

	if (memcmp(pDstData, BigBuffer, DstSize) != 0)
	{
		printf("test_ZGfxDecompressFoxSingle: output mismatch\n");
		printf("Actual\n");
		BitDump(__FUNCTION__, WLOG_INFO, pDstData, 64 * 8, 0);
		printf("...\n");
		BitDump(__FUNCTION__, WLOG_INFO, pDstData + DstSize - 64, 64 * 8, 0);
		printf("Expected\n");
		BitDump(__FUNCTION__, WLOG_INFO, BigBuffer, 64 * 8, 0);
		printf("...\n");
		BitDump(__FUNCTION__, WLOG_INFO, BigBuffer + DstSize - 64, 64 * 8, 0);
		printf("Middle Result\n");
		BitDump(__FUNCTION__, WLOG_INFO, pDstData2, 64 * 8, 0);
		printf("...\n");
		BitDump(__FUNCTION__, WLOG_INFO, pDstData2 + DstSize2 - 64, 64 * 8, 0);
		goto fail;
	}

	rc = 0;
fail:
	free(pDstData);
	free(pDstData2);
	zgfx_context_free(zgfx);
	return rc;
}

int TestFreeRDPCodecZGfx(int argc, char* argv[])
{
	if (test_ZGfxCompressFox() < 0)
		return -1;

	if (test_ZGfxDecompressFoxSingle() < 0)
		return -1;

	if (test_ZGfxDecompressFoxMultipart() < 0)
		return -1;

	if (test_ZGfxCompressConsistent() < 0)
		return -1;

	return 0;
}

