
#include <math.h>
#include <errno.h>

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/crypto.h>
#include <winpr/path.h>

#include <freerdp/freerdp.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/codec/planar.h>

#include "TestFreeRDPHelpers.h"

static const UINT32 colorFormatList[] = {
	PIXEL_FORMAT_RGB15,  PIXEL_FORMAT_BGR15,  PIXEL_FORMAT_RGB16,  PIXEL_FORMAT_BGR16,
	PIXEL_FORMAT_RGB24,  PIXEL_FORMAT_BGR24,  PIXEL_FORMAT_ARGB32, PIXEL_FORMAT_ABGR32,
	PIXEL_FORMAT_XRGB32, PIXEL_FORMAT_XBGR32, PIXEL_FORMAT_RGBX32, PIXEL_FORMAT_BGRX32

};
static const UINT32 colorFormatCount = sizeof(colorFormatList) / sizeof(colorFormatList[0]);

static BOOL CompareBitmap(const BYTE* srcA, UINT32 srcAFormat, const BYTE* srcB, UINT32 srcBFormat,
                          UINT32 width, UINT32 height)
{
	double maxDiff = NAN;
	const UINT32 srcABits = FreeRDPGetBitsPerPixel(srcAFormat);
	const UINT32 srcBBits = FreeRDPGetBitsPerPixel(srcBFormat);
	UINT32 diff = WINPR_ASSERTING_INT_CAST(uint32_t, fabs((double)srcABits - srcBBits));

	/* No support for 8bpp */
	if ((srcABits < 15) || (srcBBits < 15))
		return FALSE;

	/* Compare with following granularity:
	 * 32    -->    24 bpp: Each color channel has 8bpp, no difference expected
	 * 24/32 --> 15/16 bpp: 8bit per channel against 5/6bit per channel, +/- 3bit
	 * 16    -->    15bpp: 5/6bit per channel against 5 bit per channel, +/- 1bit
	 */
	switch (diff)
	{
		case 1:
			maxDiff = 2 * 2.0;
			break;

		case 8:
		case 9:
		case 16:
		case 17:
			maxDiff = 2 * 8.0;
			break;

		default:
			maxDiff = 0.0;
			break;
	}

	if ((srcABits == 32) || (srcBBits == 32))
	{
		if (diff == 8)
			maxDiff = 0.0;
	}

	for (size_t y = 0; y < height; y++)
	{
		const BYTE* lineA = &srcA[y * width * FreeRDPGetBytesPerPixel(srcAFormat)];
		const BYTE* lineB = &srcB[y * width * FreeRDPGetBytesPerPixel(srcBFormat)];

		for (size_t x = 0; x < width; x++)
		{
			BYTE sR = 0;
			BYTE sG = 0;
			BYTE sB = 0;
			BYTE sA = 0;
			BYTE dR = 0;
			BYTE dG = 0;
			BYTE dB = 0;
			BYTE dA = 0;
			const BYTE* a = &lineA[x * FreeRDPGetBytesPerPixel(srcAFormat)];
			const BYTE* b = &lineB[x * FreeRDPGetBytesPerPixel(srcBFormat)];
			UINT32 colorA = FreeRDPReadColor(a, srcAFormat);
			UINT32 colorB = FreeRDPReadColor(b, srcBFormat);
			FreeRDPSplitColor(colorA, srcAFormat, &sR, &sG, &sB, &sA, NULL);
			FreeRDPSplitColor(colorB, srcBFormat, &dR, &dG, &dB, &dA, NULL);

			if (fabs((double)sR - dR) > maxDiff)
				return FALSE;

			if (fabs((double)sG - dG) > maxDiff)
				return FALSE;

			if (fabs((double)sB - dB) > maxDiff)
				return FALSE;

			if (fabs((double)sA - dA) > maxDiff)
				return FALSE;
		}
	}

	return TRUE;
}

static BOOL RunTestPlanar(BITMAP_PLANAR_CONTEXT* encplanar, BITMAP_PLANAR_CONTEXT* decplanar,
                          const char* name, const UINT32 srcFormat, const UINT32 dstFormat,
                          const UINT32 width, const UINT32 height)
{
	WINPR_ASSERT(encplanar);
	WINPR_ASSERT(decplanar);
	BOOL rc = FALSE;
	UINT32 dstSize = 0;
	size_t srclen = 0;
	(void)printf("---------------------- start %s [%s] ----------------------\n", __func__, name);
	BYTE* srcBitmap = test_codec_helper_read_data("planar", "bmp", name, &srclen);
	if (!srcBitmap)
		return FALSE;

	BYTE* compressedBitmap = freerdp_bitmap_compress_planar(encplanar, srcBitmap, srcFormat, width,
	                                                        height, 0, NULL, &dstSize);
	BYTE* decompressedBitmap =
	    (BYTE*)calloc(height, 1ULL * width * FreeRDPGetBytesPerPixel(dstFormat));

	if (!test_codec_helper_compare("planar", "enc", name, compressedBitmap, dstSize))
		goto fail;

	(void)printf("%s [%s] --> [%s]: ", __func__, FreeRDPGetColorFormatName(srcFormat),
	             FreeRDPGetColorFormatName(dstFormat));

	if (!compressedBitmap || !decompressedBitmap)
		goto fail;

	if (!freerdp_bitmap_decompress_planar(decplanar, compressedBitmap, dstSize, width, height,
	                                      decompressedBitmap, dstFormat, 0, 0, 0, width, height,
	                                      FALSE))
	{
		(void)printf("failed to decompress experimental bitmap 01: width: %" PRIu32
		             " height: %" PRIu32 "\n",
		             width, height);
		goto fail;
	}

#if 0
    if (!compare("dec", name, decompressedBitmap,
                 1ull * width * height * FreeRDPGetBytesPerPixel(dstFormat)))
        goto fail;

    if (!CompareBitmap(decompressedBitmap, dstFormat, srcBitmap, srcFormat, width, height))
    {
        printf("FAIL");
        goto fail;
    }
#endif

	rc = TRUE;
fail:
	free(srcBitmap);
	free(compressedBitmap);
	free(decompressedBitmap);
	(void)printf("\n");
	(void)printf("%s [%s]: %s\n", __func__, name, rc ? "SUCCESS" : "FAILED");
	(void)printf("----------------------   end %s [%s] ----------------------\n", __func__, name);
	(void)fflush(stdout);
	(void)fflush(stderr);
	return rc;
}

static BOOL RunTestPlanarSingleColor(BITMAP_PLANAR_CONTEXT* planar, const UINT32 srcFormat,
                                     const UINT32 dstFormat)
{
	BOOL rc = FALSE;
	(void)printf("%s: [%s] --> [%s]: ", __func__, FreeRDPGetColorFormatName(srcFormat),
	             FreeRDPGetColorFormatName(dstFormat));
	(void)fflush(stdout);
	(void)fflush(stderr);

	for (UINT32 j = 0; j < 32; j += 8)
	{
		for (UINT32 i = 4; i < 32; i += 8)
		{
			UINT32 compressedSize = 0;
			const UINT32 fill = j;
			const UINT32 color = FreeRDPGetColor(srcFormat, (fill >> 8) & 0xF, (fill >> 4) & 0xF,
			                                     (fill) & 0xF, 0xFF);
			const UINT32 width = i;
			const UINT32 height = i;
			BOOL failed = TRUE;
			const UINT32 srcSize = width * height * FreeRDPGetBytesPerPixel(srcFormat);
			const UINT32 dstSize = width * height * FreeRDPGetBytesPerPixel(dstFormat);
			BYTE* compressedBitmap = NULL;
			BYTE* bmp = malloc(srcSize);
			BYTE* decompressedBitmap = (BYTE*)malloc(dstSize);

			if (!bmp || !decompressedBitmap)
				goto fail_loop;

			for (size_t y = 0; y < height; y++)
			{
				BYTE* line = &bmp[y * width * FreeRDPGetBytesPerPixel(srcFormat)];

				for (size_t x = 0; x < width; x++)
				{
					FreeRDPWriteColor(line, srcFormat, color);
					line += FreeRDPGetBytesPerPixel(srcFormat);
				}
			}

			compressedBitmap = freerdp_bitmap_compress_planar(planar, bmp, srcFormat, width, height,
			                                                  0, NULL, &compressedSize);

			if (!compressedBitmap)
				goto fail_loop;

			if (!freerdp_bitmap_decompress_planar(planar, compressedBitmap, compressedSize, width,
			                                      height, decompressedBitmap, dstFormat, 0, 0, 0,
			                                      width, height, FALSE))
				goto fail_loop;

			if (!CompareBitmap(decompressedBitmap, dstFormat, bmp, srcFormat, width, height))
				goto fail_loop;

			failed = FALSE;
		fail_loop:
			free(bmp);
			free(compressedBitmap);
			free(decompressedBitmap);

			if (failed)
			{
				printf("FAIL");
				goto fail;
			}
		}
	}

	rc = TRUE;
fail:
	(void)printf("\n");
	(void)printf("%s [%s->%s]: %s\n", __func__, FreeRDPGetColorFormatName(srcFormat),
	             FreeRDPGetColorFormatName(dstFormat), rc ? "SUCCESS" : "FAILED");
	(void)fflush(stdout);
	(void)fflush(stderr);
	return rc;
}

static BOOL TestPlanar(const UINT32 format)
{
	BOOL rc = FALSE;
	const DWORD planarFlags = PLANAR_FORMAT_HEADER_NA | PLANAR_FORMAT_HEADER_RLE;
	BITMAP_PLANAR_CONTEXT* encplanar = freerdp_bitmap_planar_context_new(planarFlags, 64, 64);
	BITMAP_PLANAR_CONTEXT* decplanar = freerdp_bitmap_planar_context_new(planarFlags, 64, 64);

	if (!encplanar || !decplanar)
		goto fail;

	if (!RunTestPlanar(encplanar, decplanar, "TEST_RLE_BITMAP_EXPERIMENTAL_01", PIXEL_FORMAT_RGBX32,
	                   format, 64, 64))
		goto fail;

	if (!RunTestPlanar(encplanar, decplanar, "TEST_RLE_BITMAP_EXPERIMENTAL_02", PIXEL_FORMAT_RGBX32,
	                   format, 64, 64))
		goto fail;

	if (!RunTestPlanar(encplanar, decplanar, "TEST_RLE_BITMAP_EXPERIMENTAL_03", PIXEL_FORMAT_RGBX32,
	                   format, 64, 64))
		goto fail;

	if (!RunTestPlanar(encplanar, decplanar, "TEST_RLE_UNCOMPRESSED_BITMAP_16BPP",
	                   PIXEL_FORMAT_RGB16, format, 32, 32))
		goto fail;

	for (UINT32 x = 0; x < colorFormatCount; x++)
	{
		if (!RunTestPlanarSingleColor(encplanar, format, colorFormatList[x]))
			goto fail;
	}

	rc = TRUE;
fail:
	freerdp_bitmap_planar_context_free(encplanar);
	freerdp_bitmap_planar_context_free(decplanar);
	return rc;
}

static UINT32 prand(UINT32 max)
{
	UINT32 tmp = 0;
	if (max <= 1)
		return 1;
	winpr_RAND(&tmp, sizeof(tmp));
	return tmp % (max - 1) + 1;
}

static BOOL FuzzPlanar(void)
{
	(void)printf("---------------------- start %s ----------------------\n", __func__);
	BOOL rc = FALSE;
	const DWORD planarFlags = PLANAR_FORMAT_HEADER_NA | PLANAR_FORMAT_HEADER_RLE;
	BITMAP_PLANAR_CONTEXT* planar = freerdp_bitmap_planar_context_new(planarFlags, 64, 64);

	if (!planar)
		goto fail;

	for (UINT32 x = 0; x < 100; x++)
	{
		BYTE data[0x10000] = WINPR_C_ARRAY_INIT;
		size_t dataSize = 0x10000;
		BYTE dstData[0x10000] = WINPR_C_ARRAY_INIT;

		UINT32 DstFormat = 0;
		UINT32 nDstStep = 0;
		UINT32 nXDst = 0;
		UINT32 nYDst = 0;
		UINT32 nDstWidth = 0;
		UINT32 nDstHeight = 0;
		BOOL invalid = TRUE;
		do
		{
			switch (prand(17) - 1)
			{
				case 0:
					DstFormat = PIXEL_FORMAT_RGB8;
					break;
				case 1:
					DstFormat = PIXEL_FORMAT_BGR15;
					break;
				case 2:
					DstFormat = PIXEL_FORMAT_RGB15;
					break;
				case 3:
					DstFormat = PIXEL_FORMAT_ABGR15;
					break;
				case 4:
					DstFormat = PIXEL_FORMAT_ABGR15;
					break;
				case 5:
					DstFormat = PIXEL_FORMAT_BGR16;
					break;
				case 6:
					DstFormat = PIXEL_FORMAT_RGB16;
					break;
				case 7:
					DstFormat = PIXEL_FORMAT_BGR24;
					break;
				case 8:
					DstFormat = PIXEL_FORMAT_RGB24;
					break;
				case 9:
					DstFormat = PIXEL_FORMAT_BGRA32;
					break;
				case 10:
					DstFormat = PIXEL_FORMAT_BGRX32;
					break;
				case 11:
					DstFormat = PIXEL_FORMAT_RGBA32;
					break;
				case 12:
					DstFormat = PIXEL_FORMAT_RGBX32;
					break;
				case 13:
					DstFormat = PIXEL_FORMAT_ABGR32;
					break;
				case 14:
					DstFormat = PIXEL_FORMAT_XBGR32;
					break;
				case 15:
					DstFormat = PIXEL_FORMAT_ARGB32;
					break;
				case 16:
					DstFormat = PIXEL_FORMAT_XRGB32;
					break;
				default:
					break;
			}
			nDstStep = prand(sizeof(dstData));
			nXDst = prand(nDstStep);
			nYDst = prand(sizeof(dstData) / nDstStep);
			nDstWidth = prand(nDstStep / FreeRDPGetBytesPerPixel(DstFormat));
			nDstHeight = prand(sizeof(dstData) / nDstStep);
			invalid = nXDst * FreeRDPGetBytesPerPixel(DstFormat) + (nYDst + nDstHeight) * nDstStep >
			          sizeof(dstData);
		} while (invalid);
		printf("DstFormat=%s, nXDst=%" PRIu32 ", nYDst=%" PRIu32 ", nDstWidth=%" PRIu32
		       ", nDstHeight=%" PRIu32 ", nDstStep=%" PRIu32 ", total size=%" PRIuz "\n",
		       FreeRDPGetColorFormatName(DstFormat), nXDst, nYDst, nDstWidth, nDstHeight, nDstStep,
		       sizeof(dstData));
		freerdp_planar_switch_bgr(planar, ((prand(2) % 2) != 0) ? TRUE : FALSE);
		freerdp_bitmap_decompress_planar(planar, data, dataSize, prand(4096), prand(4096), dstData,
		                                 DstFormat, nDstStep, nXDst, nYDst, nDstWidth, nDstHeight,
		                                 ((prand(2) % 2) != 0) ? TRUE : FALSE);
	}

	rc = TRUE;
fail:
	freerdp_bitmap_planar_context_free(planar);
	(void)printf("\n");
	(void)printf("%s: %s\n", __func__, rc ? "SUCCESS" : "FAILED");
	(void)printf("----------------------   end %s ----------------------\n", __func__);
	(void)fflush(stdout);
	(void)fflush(stderr);
	return rc;
}

int TestFreeRDPCodecPlanar(int argc, char* argv[])
{
	int rc = -1;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!FuzzPlanar())
		goto fail;

	for (UINT32 x = 0; x < colorFormatCount; x++)
	{
		if (!TestPlanar(colorFormatList[x]))
			goto fail;
	}

	rc = 0;
fail:
	printf("test returned %d\n", rc);
	return rc;
}
