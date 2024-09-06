
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <winpr/sysinfo.h>

#include <freerdp/freerdp.h>
#include <freerdp/codec/color.h>

#define TEST_RUNS 2

static BOOL TestFreeRDPImageCopy(UINT32 w, UINT32 h, UINT32 srcFormat, UINT32 dstFormat,
                                 size_t runs)
{
	BOOL rc = FALSE;
	const size_t sbpp = FreeRDPGetBytesPerPixel(srcFormat);
	const size_t dbpp = FreeRDPGetBytesPerPixel(dstFormat);
	const size_t srcStep = w * sbpp;
	const size_t dstStep = w * dbpp;
	char* src = calloc(h, srcStep);
	char* dst = calloc(h, dstStep);
	if (!src || !dst)
		goto fail;

	for (size_t x = 0; x < runs; x++)
	{
		winpr_RAND_pseudo(src, h * srcStep);
		const UINT64 start = winpr_GetUnixTimeNS();
		rc = freerdp_image_copy(dst, dstFormat, dstStep, 0, 0, w, h, src, srcFormat, srcStep, 0, 0,
		                        NULL, 0);
		const UINT64 end = winpr_GetUnixTimeNS();

		double ms = end - start;
		ms /= 1000000.0;

		(void)fprintf(stdout,
		              "[%s] copied %" PRIu32 "x%" PRIu32 " [%-20s] -> [%-20s] in %lf ms [%s]\n",
		              __func__, w, h, FreeRDPGetColorFormatName(srcFormat),
		              FreeRDPGetColorFormatName(dstFormat), ms, rc ? "success" : "failure");
		if (!rc)
			break;
	}

fail:
	free(src);
	free(dst);
	return rc;
}

static BOOL TestFreeRDPImageCopy_no_overlap(UINT32 w, UINT32 h, UINT32 srcFormat, UINT32 dstFormat,
                                            size_t runs)
{
	BOOL rc = FALSE;
	const size_t sbpp = FreeRDPGetBytesPerPixel(srcFormat);
	const size_t dbpp = FreeRDPGetBytesPerPixel(dstFormat);
	const size_t srcStep = w * sbpp;
	const size_t dstStep = w * dbpp;
	char* src = calloc(h, srcStep);
	char* dst = calloc(h, dstStep);
	if (!src || !dst)
		goto fail;

	for (size_t x = 0; x < runs; x++)
	{
		winpr_RAND_pseudo(src, h * srcStep);
		const UINT64 start = winpr_GetUnixTimeNS();
		rc = freerdp_image_copy_no_overlap(dst, dstFormat, dstStep, 0, 0, w, h, src, srcFormat,
		                                   srcStep, 0, 0, NULL, 0);
		const UINT64 end = winpr_GetUnixTimeNS();

		double ms = end - start;
		ms /= 1000000.0;

		(void)fprintf(stdout,
		              "[%s] copied %" PRIu32 "x%" PRIu32 " [%-20s] -> [%-20s] in %lf ms [%s]\n",
		              __func__, w, h, FreeRDPGetColorFormatName(srcFormat),
		              FreeRDPGetColorFormatName(dstFormat), ms, rc ? "success" : "failure");
		if (!rc)
			break;
	}

fail:
	free(src);
	free(dst);
	return rc;
}
int TestFreeRDPCodecCopy(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	UINT32 width = 192;
	UINT32 height = 108;
	const UINT32 formats[] = {
		PIXEL_FORMAT_ABGR15, PIXEL_FORMAT_ARGB15, PIXEL_FORMAT_BGR15,  PIXEL_FORMAT_BGR16,
		PIXEL_FORMAT_BGR24,  PIXEL_FORMAT_RGB15,  PIXEL_FORMAT_RGB16,  PIXEL_FORMAT_RGB24,
		PIXEL_FORMAT_ABGR32, PIXEL_FORMAT_ARGB32, PIXEL_FORMAT_XBGR32, PIXEL_FORMAT_XRGB32,
		PIXEL_FORMAT_BGRA32, PIXEL_FORMAT_RGBA32, PIXEL_FORMAT_BGRX32, PIXEL_FORMAT_RGBX32,
	};

	if (argc == 3)
	{
		errno = 0;
		width = strtoul(argv[1], NULL, 0);
		height = strtoul(argv[2], NULL, 0);
		if ((errno != 0) || (width == 0) || (height == 0))
		{
			char buffer[128] = { 0 };
			(void)fprintf(stderr, "%s failed: width=%" PRIu32 ", height=%" PRIu32 ", errno=%s\n",
			              __func__, width, height, winpr_strerror(errno, buffer, sizeof(buffer)));
			return -1;
		}
	}
	for (size_t x = 0; x < ARRAYSIZE(formats); x++)
	{
		const UINT32 SrcFormat = formats[x];
		for (size_t y = 0; y < ARRAYSIZE(formats); y++)
		{
			const UINT32 DstFormat = formats[y];
			if (!TestFreeRDPImageCopy(width, height, SrcFormat, DstFormat, TEST_RUNS))
				return -1;
			if (!TestFreeRDPImageCopy_no_overlap(width, height, SrcFormat, DstFormat, TEST_RUNS))
				return -1;
		}
	}

	return 0;
}
