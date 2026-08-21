#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/platform.h>

#include <freerdp/codec/clear.h>

/* Mirrors the internal CLEAR_BAND_MAX_HEIGHT used by clear_compress()'s band encoder
 * (libfreerdp/codec/clear.c), so tests can size images to span multiple bands. */
#define CLEAR_TEST_BAND_HEIGHT 52

WINPR_PRAGMA_DIAG_PUSH
WINPR_PRAGMA_DIAG_IGNORED_UNUSED_CONST_VAR
/* [MS-RDPEGFX] 4.1.1.1 Example 1 */
static const BYTE PREPARE_CLEAR_EXAMPLE_1[] = "\x03\xc3\x11\x00";
static const BYTE TEST_CLEAR_EXAMPLE_1[] = "\x03\xc3\x11\x00";
WINPR_PRAGMA_DIAG_POP

/* [MS-RDPEGFX] 4.1.1.1 Example 2 */
static const BYTE TEST_CLEAR_EXAMPLE_2[] =
    "\x00\x0d\x00\x00\x00\x00\x00\x00\x00\x00\x82\x00\x00\x00\x00\x00"
    "\x00\x00\x4e\x00\x11\x00\x75\x00\x00\x00\x02\x0e\xff\xff\xff\x00"
    "\x00\x00\xdb\xff\xff\x00\x3a\x90\xff\xb6\x66\x66\xb6\xff\xb6\x66"
    "\x00\x90\xdb\xff\x00\x00\x3a\xdb\x90\x3a\x3a\x90\xdb\x66\x00\x00"
    "\xff\xff\xb6\x64\x64\x64\x11\x04\x11\x4c\x11\x4c\x11\x4c\x11\x4c"
    "\x11\x4c\x00\x47\x13\x00\x01\x01\x04\x00\x01\x00\x00\x47\x16\x00"
    "\x11\x02\x00\x47\x29\x00\x11\x01\x00\x49\x0a\x00\x01\x00\x04\x00"
    "\x01\x00\x00\x4a\x0a\x00\x09\x00\x01\x00\x00\x47\x05\x00\x01\x01"
    "\x1c\x00\x01\x00\x11\x4c\x11\x4c\x11\x4c\x00\x47\x0d\x4d\x00\x4d";

/* [MS-RDPEGFX] 4.1.1.1 Example 3 */
static const BYTE TEST_CLEAR_EXAMPLE_3[] =
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

/* [MS-RDPEGFX] 4.1.1.1 Example 4 */
static const BYTE TEST_CLEAR_EXAMPLE_4[] =
    "\x01\x0b\x78\x00\x00\x00\x00\x00\x46\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x06\x00\x00\x00\x0e\x00\x00\x00\x00\x00\x0f\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xb6\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xb6\x66\xff\xff\xff\xff\xff\xff\xff\xb6\x66\xdb\x90\x3a\xff\xff"
    "\xb6\xff\xff\xff\xff\xff\xff\xff\xff\xff\x46\x91\x47\x91\x48\x91"
    "\x49\x91\x4a\x91\x1b\x91";

static BOOL test_ClearDecompressExample(UINT32 nr, UINT32 width, UINT32 height,
                                        const BYTE* pSrcData, const UINT32 SrcSize)
{
	BOOL rc = FALSE;
	int status = 0;
	BYTE* pDstData = calloc(4ULL * width, height);
	CLEAR_CONTEXT* clear = clear_context_new(FALSE);

	if (!clear || !pDstData)
		goto fail;

	status = clear_decompress(clear, pSrcData, SrcSize, width, height, pDstData,
	                          PIXEL_FORMAT_XRGB32, 0, 0, 0, width, height, nullptr);
	(void)printf("clear_decompress example %" PRIu32 " status: %d\n", nr, status);
	(void)fflush(stdout);
	rc = (status == 0);
fail:
	clear_context_free(clear);
	free(pDstData);
	return rc;
}

static BOOL test_ClearCompressDecompressRoundtrip(UINT32 width, UINT32 height)
{
	BOOL rc = FALSE;
	BYTE* pDstData = nullptr;
	BYTE* pCompressed = nullptr;
	UINT32 compressedSize = 0;
	const UINT32 DstFormat = PIXEL_FORMAT_BGRX32;
	const UINT32 srcStep = width * FreeRDPGetBytesPerPixel(DstFormat);
	BYTE* pSrcData = calloc(1ULL * srcStep, height);
	CLEAR_CONTEXT* clearEnc = clear_context_new(TRUE);
	CLEAR_CONTEXT* clearDec = clear_context_new(FALSE);

	pDstData = calloc(1ULL * srcStep, height);
	if (!pSrcData || !pDstData || !clearEnc || !clearDec)
		goto fail;

	/* Mix of flat runs (compresses well) and a varying pattern (forces many short runs). */
	for (UINT32 y = 0; y < height; y++)
	{
		BYTE* pLine = &pSrcData[1ULL * y * srcStep];
		for (UINT32 x = 0; x < width; x++)
		{
			BYTE r = 0;
			BYTE g = 0;
			BYTE b = 0;

			if (x < width / 2)
			{
				r = 0x10;
				g = 0x80;
				b = 0xF0;
			}
			else
			{
				r = WINPR_ASSERTING_INT_CAST(BYTE, (x * 7 + y * 13) & 0xFF);
				g = WINPR_ASSERTING_INT_CAST(BYTE, (x * 3 + y * 5) & 0xFF);
				b = WINPR_ASSERTING_INT_CAST(BYTE, (x + y) & 0xFF);
			}

			const UINT32 color = FreeRDPGetColor(DstFormat, r, g, b, 0xFF);
			FreeRDPWriteColor(&pLine[1ULL * x * FreeRDPGetBytesPerPixel(DstFormat)], DstFormat,
			                  color);
		}
	}

	if (!clear_compress(clearEnc, pSrcData, DstFormat, srcStep, width, height, &pCompressed,
	                    &compressedSize))
	{
		(void)fprintf(stderr, "clear_compress failed for %" PRIu32 "x%" PRIu32 "\n", width, height);
		goto fail;
	}

	if (clear_decompress(clearDec, pCompressed, compressedSize, width, height, pDstData, DstFormat,
	                     srcStep, 0, 0, width, height, nullptr) != 0)
	{
		(void)fprintf(stderr, "clear_decompress failed for %" PRIu32 "x%" PRIu32 "\n", width,
		              height);
		goto fail;
	}

	if (memcmp(pSrcData, pDstData, 1ULL * srcStep * height) != 0)
	{
		(void)fprintf(stderr, "clear roundtrip mismatch for %" PRIu32 "x%" PRIu32 "\n", width,
		              height);
		goto fail;
	}

	rc = TRUE;
fail:
	clear_context_free(clearEnc);
	clear_context_free(clearDec);
	free(pSrcData);
	free(pDstData);
	return rc;
}

/* Content that varies unpredictably along each row (defeats row-major residual RLE) but is
 * constant down every column (every band's V-Bar for a given x is byte-identical), across
 * enough rows for several 52-row bands. This exercises clear_compress_bands_data()'s
 * VBAR_CACHE_HIT/SHORT_VBAR_CACHE_MISS path and should compress far better than residual-only
 * encoding of the same image would. */
static BOOL test_ClearCompressBandsRoundtrip(void)
{
	BOOL rc = FALSE;
	const UINT32 width = 37;
	const UINT32 height = 3 * CLEAR_TEST_BAND_HEIGHT + 5;
	BYTE* pDstData = nullptr;
	BYTE* pCompressed = nullptr;
	UINT32 compressedSize = 0;
	const UINT32 DstFormat = PIXEL_FORMAT_BGRX32;
	const UINT32 srcStep = width * FreeRDPGetBytesPerPixel(DstFormat);
	BYTE* pSrcData = calloc(1ULL * srcStep, height);
	CLEAR_CONTEXT* clearEnc = clear_context_new(TRUE);
	CLEAR_CONTEXT* clearDec = clear_context_new(FALSE);

	pDstData = calloc(1ULL * srcStep, height);
	if (!pSrcData || !pDstData || !clearEnc || !clearDec)
		goto fail;

	for (UINT32 y = 0; y < height; y++)
	{
		BYTE* pLine = &pSrcData[1ULL * y * srcStep];
		for (UINT32 x = 0; x < width; x++)
		{
			const BYTE r = WINPR_ASSERTING_INT_CAST(BYTE, (x * 97 + x * x * 13) & 0xFF);
			const BYTE g = WINPR_ASSERTING_INT_CAST(BYTE, (x * 53 + x * x * 7) & 0xFF);
			const BYTE b = WINPR_ASSERTING_INT_CAST(BYTE, (x * 31) & 0xFF);
			const UINT32 color = FreeRDPGetColor(DstFormat, r, g, b, 0xFF);
			FreeRDPWriteColor(&pLine[1ULL * x * FreeRDPGetBytesPerPixel(DstFormat)], DstFormat,
			                  color);
		}
	}

	if (!clear_compress(clearEnc, pSrcData, DstFormat, srcStep, width, height, &pCompressed,
	                    &compressedSize))
	{
		(void)fprintf(stderr, "clear_compress (bands) failed for %" PRIu32 "x%" PRIu32 "\n", width,
		              height);
		goto fail;
	}

	if (clear_decompress(clearDec, pCompressed, compressedSize, width, height, pDstData, DstFormat,
	                     srcStep, 0, 0, width, height, nullptr) != 0)
	{
		(void)fprintf(stderr, "clear_decompress (bands) failed for %" PRIu32 "x%" PRIu32 "\n",
		              width, height);
		goto fail;
	}

	if (memcmp(pSrcData, pDstData, 1ULL * srcStep * height) != 0)
	{
		(void)fprintf(stderr, "clear bands roundtrip mismatch for %" PRIu32 "x%" PRIu32 "\n", width,
		              height);
		goto fail;
	}

	/* Column content repeats identically in every band, so from the second band onward every
	 * column should be a 2-byte VBAR_CACHE_HIT: only the first (52-row) and last (5-row, a
	 * different height so it can't match the cache) bands pay the full raw per-column cost. A
	 * residual-only encoding of this per-row-varying content would cost close to the raw BGRX32
	 * size (4 bytes/pixel); confirm the V-Bar cache brought the total well under half of that. */
	if (compressedSize >= (1ULL * srcStep * height) / 2)
	{
		(void)fprintf(stderr,
		              "clear bands compressedSize %" PRIu32 " did not benefit from caching (raw "
		              "size %" PRIu32 "x%" PRIu32 " * 4)\n",
		              compressedSize, width, height);
		goto fail;
	}

	rc = TRUE;
fail:
	clear_context_free(clearEnc);
	clear_context_free(clearDec);
	free(pSrcData);
	free(pDstData);
	return rc;
}

int TestFreeRDPCodecClear(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	/* Example 1 needs a filled glyph cache
	if (!test_ClearDecompressExample(1, 8, 9, TEST_CLEAR_EXAMPLE_1,
	                                 sizeof(TEST_CLEAR_EXAMPLE_1)))
	    return -1;
	*/
	if (!test_ClearDecompressExample(2, 78, 17, TEST_CLEAR_EXAMPLE_2, sizeof(TEST_CLEAR_EXAMPLE_2)))
		return -1;

	if (!test_ClearDecompressExample(3, 64, 24, TEST_CLEAR_EXAMPLE_3, sizeof(TEST_CLEAR_EXAMPLE_3)))
		return -1;

	if (!test_ClearDecompressExample(4, 7, 15, TEST_CLEAR_EXAMPLE_4, sizeof(TEST_CLEAR_EXAMPLE_4)))
		return -1;

	if (!test_ClearCompressDecompressRoundtrip(64, 48))
		return -1;

	if (!test_ClearCompressDecompressRoundtrip(1, 1))
		return -1;

	if (!test_ClearCompressDecompressRoundtrip(3, 200))
		return -1;

	if (!test_ClearCompressBandsRoundtrip())
		return -1;

	return 0;
}
