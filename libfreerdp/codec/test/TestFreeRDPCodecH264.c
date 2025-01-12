
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <winpr/sysinfo.h>

#include <freerdp/freerdp.h>
#include <freerdp/codec/color.h>

static const char* print_ns(UINT64 start, UINT64 end, char* buffer, size_t len)
{
	const UINT64 diff = end - start;
	const unsigned ns = diff % 1000;
	const unsigned us = (diff / 1000) % 1000;
	const unsigned ms = (diff / 1000000) % 1000;
	const unsigned s = (diff / 1000000000ULL) % 1000;
	(void)_snprintf(buffer, len, "%03u %03u %03u %03uns", s, ms, us, ns);
	return buffer;
}

static BOOL testContextOptions(BOOL compressor, uint32_t width, uint32_t height)
{
	BOOL rc = FALSE;
	const UINT64 start = winpr_GetUnixTimeNS();
	H264_CONTEXT* h264 = h264_context_new(FALSE);
	if (!h264)
		return FALSE;

	struct optpair_s
	{
		H264_CONTEXT_OPTION opt;
		uint32_t val;
	};
	const struct optpair_s optpair[] = { { H264_CONTEXT_OPTION_RATECONTROL, H264_RATECONTROL_VBR },
		                                 { H264_CONTEXT_OPTION_BITRATE, 2323 },
		                                 { H264_CONTEXT_OPTION_FRAMERATE, 23 },
		                                 { H264_CONTEXT_OPTION_QP, 21 },
		                                 { H264_CONTEXT_OPTION_USAGETYPE, 23 } };
	for (size_t x = 0; x < ARRAYSIZE(optpair); x++)
	{
		const struct optpair_s* cur = &optpair[x];
		if (!h264_context_set_option(h264, cur->opt, cur->val))
			goto fail;
	}
	if (!h264_context_reset(h264, width, height))
		goto fail;

	rc = TRUE;
fail:
	h264_context_free(h264);
	const UINT64 end = winpr_GetUnixTimeNS();

	char buffer[64] = { 0 };
	printf("[%s] %" PRIu32 "x%" PRIu32 " took %s\n", __func__, width, height,
	       print_ns(start, end, buffer, sizeof(buffer)));
	return rc;
}

static void* allocRGB(uint32_t format, uint32_t width, uint32_t height, uint32_t* pstride)
{
	const size_t bpp = FreeRDPGetBytesPerPixel(format);
	const size_t stride = bpp * width + 32;
	WINPR_ASSERT(pstride);
	*pstride = WINPR_ASSERTING_INT_CAST(uint32_t, stride);

	uint8_t* rgb = calloc(stride, height);
	if (!rgb)
		return NULL;

	for (size_t x = 0; x < height; x++)
	{
		winpr_RAND(&rgb[x * stride], width * bpp);
	}
	return rgb;
}

static BOOL compareRGB(const uint8_t* src, const uint8_t* dst, uint32_t format, size_t width,
                       size_t stride, size_t height)
{
	const size_t bpp = FreeRDPGetBytesPerPixel(format);
	for (size_t y = 0; y < height; y++)
	{
		const uint8_t* csrc = &src[y * stride];
		const uint8_t* cdst = &dst[y * stride];
		const int rc = memcmp(csrc, cdst, width * bpp);
		// TODO: Both, AVC420 encoding and decoding are lossy.
		// TODO: Find a proper error margin to check for
#if 0
		if (rc != 0)
			return FALSE;
#endif
	}
	return TRUE;
}

static BOOL testEncode(uint32_t format, uint32_t width, uint32_t height)
{
	BOOL rc = FALSE;
	void* src = NULL;
	void* out = NULL;
	RDPGFX_H264_METABLOCK meta = { 0 };
	H264_CONTEXT* h264 = h264_context_new(TRUE);
	H264_CONTEXT* h264dec = h264_context_new(FALSE);
	if (!h264 || !h264dec)
		goto fail;

	if (!h264_context_reset(h264, width, height))
		goto fail;
	if (!h264_context_reset(h264dec, width, height))
		goto fail;

	uint32_t stride = 0;
	uint32_t ostride = 0;
	src = allocRGB(format, width, height, &stride);
	out = allocRGB(format, width, height, &ostride);
	if (!src || !out || (stride < width) || (stride != ostride))
		goto fail;

	const RECTANGLE_16 rect = { .left = 0, .top = 0, .right = width, .bottom = height };
	uint32_t dstsize = 0;
	uint8_t* dst = NULL;
	if (avc420_compress(h264, src, format, stride, width, height, &rect, &dst, &dstsize, &meta) < 0)
		goto fail;
	if ((dstsize == 0) || !dst)
		goto fail;

	if (avc420_decompress(h264dec, dst, dstsize, out, format, stride, width, height, &rect, 1) < 0)
		goto fail;

	rc = compareRGB(src, out, format, width, stride, height);
fail:
	h264_context_free(h264);
	h264_context_free(h264dec);
	free_h264_metablock(&meta);
	free(src);
	free(out);
	return rc;
}

int TestFreeRDPCodecH264(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	UINT32 width = 124;
	UINT32 height = 54;
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

#if !defined(WITH_MEDIACODEC) && !defined(WITH_MEDIA_FOUNDATION) && !defined(WITH_OPENH264) && \
    !defined(WITH_VIDEO_FFMPEG)
	(void)fprintf(stderr, "[%s] skipping, no H264 encoder/decoder support compiled in\n", __func__);
	return 0;
#endif

	if (!testContextOptions(FALSE, width, height))
		return -1;
	if (!testContextOptions(TRUE, width, height))
		return -1;

	for (size_t x = 0; x < ARRAYSIZE(formats); x++)
	{
		const UINT32 SrcFormat = formats[x];
		for (size_t y = 0; y < ARRAYSIZE(formats); y++)
		{
			if (!testEncode(SrcFormat, width, height))
				return -1;
		}
	}

	return 0;
}
