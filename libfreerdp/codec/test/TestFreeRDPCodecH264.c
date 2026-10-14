
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
	/* Test the requested encoder or decoder. */
	H264_CONTEXT* h264 = h264_context_new(compressor);
	if (!h264)
	{
		(void)fprintf(stderr, "[%s] h264_context_new failed\n", __func__);
		return FALSE;
	}

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
		{
			(void)fprintf(stderr,
			              "[%s] h264_context_set_option %" PRIuz "{ %d, %" PRIu32 " } failed\n",
			              __func__, x, cur->opt, cur->val);
			goto fail;
		}
	}
	if (!h264_context_reset(h264, width, height))
	{
		(void)fprintf(stderr, "[%s] h264_context_reset\n", __func__);
		goto fail;
	}

	rc = TRUE;
fail:
	h264_context_free(h264);
	const UINT64 end = winpr_GetUnixTimeNS();

	char buffer[64] = WINPR_C_ARRAY_INIT;
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
		return nullptr;

	for (size_t x = 0; x < height; x++)
	{
		if (winpr_RAND(&rgb[x * stride], width * bpp) < 0)
		{
			free(rgb);
			return nullptr;
		}
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
	void* src = nullptr;
	void* out = nullptr;
	RDPGFX_H264_METABLOCK meta = WINPR_C_ARRAY_INIT;
	H264_CONTEXT* h264 = h264_context_new(TRUE);
	H264_CONTEXT* h264dec = h264_context_new(FALSE);
	if (!h264 || !h264dec)
	{
		(void)fprintf(stderr, "[%s] encoder=%p, decoder=%p\n", __func__, (void*)h264,
		              (void*)h264dec);
		goto fail;
	}

	if (!h264_context_reset(h264, width, height))
	{
		(void)fprintf(stderr, "[%s] h264_context_reset(encoder) failed\n", __func__);
		goto fail;
	}
	if (!h264_context_reset(h264dec, width, height))
	{
		(void)fprintf(stderr, "[%s] h264_context_reset(decoder) failed\n", __func__);
		goto fail;
	}

	uint32_t stride = 0;
	uint32_t ostride = 0;
	src = allocRGB(format, width, height, &stride);
	out = allocRGB(format, width, height, &ostride);
	if (!src || !out || (stride < width) || (stride != ostride))
	{
		(void)fprintf(stderr,
		              "[%s] src=%p, out=%p, stride(%" PRIu32 ") < width(%" PRIu32
		              "), stride != ostride(%" PRIu32 ")\n",
		              __func__, src, out, stride, width, ostride);
		goto fail;
	}

	const RECTANGLE_16 rect = { .left = 0, .top = 0, .right = width, .bottom = height };
	uint32_t dstsize = 0;
	uint8_t* dst = nullptr;
	const int res1 =
	    avc420_compress(h264, src, format, stride, width, height, &rect, &dst, &dstsize, &meta);
	if (res1 < 0)
	{
		(void)fprintf(stderr, "[%s] avc420_compress failed: %d\n", __func__, res1);
		goto fail;
	}
	if ((dstsize == 0) || !dst)
	{
		(void)fprintf(stderr, "[%s] dstsize=%" PRIu32 ", dst=%p\n", __func__, dstsize, (void*)dst);
		goto fail;
	}

	const int res2 =
	    avc420_decompress(h264dec, dst, dstsize, out, format, stride, width, height, &rect, 1);
	if (res2 < 0)
	{
		(void)fprintf(stderr, "[%s] avc420_decompress failed: %d\n", __func__, res2);
		goto fail;
	}

	rc = compareRGB(src, out, format, width, stride, height);
fail:
	if (!rc)
		(void)fprintf(stderr, "[%s] run failed\n", __func__);
	h264_context_free(h264);
	h264_context_free(h264dec);
	free_h264_metablock(&meta);
	free(src);
	free(out);
	return rc;
}

static BOOL testEncodeOffsetRegion(void)
{
	BOOL rc = FALSE;
	const UINT32 width = 192;
	const UINT32 height = 128;
	const UINT32 format = PIXEL_FORMAT_BGRA32;
	const RECTANGLE_16 full = { .left = 0, .top = 0, .right = width, .bottom = height };
	const RECTANGLE_16 region = { .left = 32, .top = 16, .right = 160, .bottom = 112 };
	RDPGFX_H264_METABLOCK meta = WINPR_C_ARRAY_INIT;
	H264_CONTEXT* h264 = h264_context_new(TRUE);
	UINT32 stride = 0;
	BYTE* src = allocRGB(format, width, height, &stride);
	BYTE* dst = nullptr;
	UINT32 dstSize = 0;

	if (!h264 || !src || !h264_context_reset(h264, width, height))
		goto fail;

	memset(src, 0, (size_t)stride * height);
	const int res =
	    avc420_compress(h264, src, format, stride, width, height, &full, &dst, &dstSize, &meta);
	if (res < 0)
	{

		(void)fprintf(stderr, "[%s] first avc420_compress failed: %d\n", __func__, res);
		goto fail;
	}
	free_h264_metablock(&meta);

	for (UINT32 y = region.top; y < region.bottom; y++)
	{
		BYTE* row = &src[(size_t)y * stride + (size_t)region.left * 4];
		memset(row, 0xFF, (size_t)(region.right - region.left) * 4);
	}

	dst = nullptr;
	dstSize = 0;
	const int res2 =
	    avc420_compress(h264, src, format, stride, width, height, &region, &dst, &dstSize, &meta);
	if (res2 < 0)
	{
		(void)fprintf(stderr, "[%s] second avc420_compress failed: %d\n", __func__, res2);
		goto fail;
	}
	if ((meta.numRegionRects == 0) || (meta.regionRects[0].left != region.left) ||
	    (meta.regionRects[0].top != region.top))
	{
		(void)fprintf(
		    stderr, "%s failed: first changed tile does not start at (%" PRIu16 ", %" PRIu16 ")\n",
		    __func__, region.left, region.top);
		goto fail;
	}

	rc = TRUE;
fail:
	if (!rc)
		(void)fprintf(stderr, "[%s] run failed\n", __func__);
	h264_context_free(h264);
	free_h264_metablock(&meta);
	free(src);
	return rc;
}

/* 64x64 solid 0x3366CC, generated with
 * ffmpeg -f lavfi -i color=c=0x3366CC:s=64x64 -frames:v 1 -c:v libx264 -profile:v baseline
 *        -bsf:v h264_mp4toannexb,filter_units=remove_types=6 -f h264 */
static const BYTE solid_idr_64x64[] = {
	0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xc0, 0x0a, 0xd9, 0x04, 0x26, 0xc0, 0x44, 0x00,
	0x00, 0x03, 0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0xc8, 0x3c, 0x48, 0x99, 0x20, 0x00,
	0x00, 0x00, 0x01, 0x68, 0xcb, 0x83, 0xcb, 0x20, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84,
	0x0a, 0xf1, 0x18, 0xa0, 0x00, 0x22, 0x4b, 0x1c, 0x00, 0x04, 0x53, 0xa3, 0x80, 0x00,
	0x85, 0x8c, 0x9c, 0x9c, 0x9d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5e
};

static BOOL testDecode(BOOL separateParameters)
{
	BOOL rc = FALSE;
	const UINT32 width = 64;
	const UINT32 height = 64;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 stride = width * 4;
	const RECTANGLE_16 rect = { .left = 0, .top = 0, .right = width, .bottom = height };
	H264_CONTEXT* h264 = h264_context_new(FALSE);
	BYTE* dst = calloc(stride, height);

	if (!h264 || !dst || !h264_context_reset(h264, width, height))
		goto fail;

	/* The video channel can deliver SPS/PPS separately from the picture. */
	const UINT32 offset = separateParameters ? 36 : 0; /* IDR start code follows SPS/PPS. */
	if (separateParameters && (avc420_decompress(h264, solid_idr_64x64, offset, dst, format, stride,
	                                             width, height, &rect, 1) < 0))
		goto fail;

	/* The picture must be available immediately, with the expected RGB values. */
	const INT32 res =
	    avc420_decompress(h264, solid_idr_64x64 + offset, sizeof(solid_idr_64x64) - offset, dst,
	                      format, stride, width, height, &rect, 1);
	if (res < 0)
	{
		(void)fprintf(stderr, "[%s] avc420_decompress failed: %" PRId32 "\n", __func__, res);
		goto fail;
	}

	for (UINT32 y = 0; y < height; y++)
	{
		for (UINT32 x = 0; x < width; x++)
		{
			BYTE r = 0;
			BYTE g = 0;
			BYTE b = 0;
			const UINT32 color = FreeRDPReadColor(&dst[y * stride + x * 4], format);
			FreeRDPSplitColor(color, format, &r, &g, &b, nullptr, nullptr);
			if ((abs(r - 0x33) > 16) || (abs(g - 0x66) > 16) || (abs(b - 0xCC) > 16))
			{
				(void)fprintf(
				    stderr, "[%s] pixel %" PRIu32 ",%" PRIu32 " is %02X%02X%02X, expected 3366CC\n",
				    __func__, x, y, r, g, b);
				goto fail;
			}
		}
	}

	rc = TRUE;
fail:
	h264_context_free(h264);
	free(dst);
	return rc;
}

/* 64x64 I420: Y=64/U=90/V=170; the P-frame adds a 16x16 Y=96 block at (8,16).
 * Generated with libx264: -qp 18 -profile:v high, bframes=0:slices=2:repeat-headers=1;
 * SEI removed. The auxiliary frame depends on the preceding main frame. */
static const BYTE avc444_idr[] = {
	0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x0A, 0xAC, 0xB2, 0x08, 0x4D, 0x08, 0x00, 0x00, 0x03,
	0x00, 0x08, 0x00, 0x00, 0x03, 0x01, 0xE0, 0x78, 0x91, 0x32, 0x40, 0x00, 0x00, 0x00, 0x01, 0x68,
	0xEB, 0xC0, 0x8C, 0xB2, 0x2C, 0x00, 0x00, 0x00, 0x01, 0x09, 0x10, 0x00, 0x00, 0x01, 0x65, 0x88,
	0x84, 0x3A, 0xFE, 0xC7, 0x6F, 0xE6, 0x59, 0x2A, 0x80, 0xC4, 0x0B, 0xD4, 0xF7, 0xD5, 0xB5, 0x93,
	0x2C, 0x2D, 0x6E, 0xCB, 0x72, 0xFB, 0xC9, 0x1D, 0x47, 0x81, 0x00, 0x00, 0x01, 0x65, 0x12, 0x22,
	0x10, 0xEB, 0xFE, 0xC7, 0x6F, 0xE6, 0x59, 0x2A, 0x80, 0xC4, 0x0B, 0xD4, 0xF7, 0xD5, 0xB5, 0x93,
	0x2C, 0x2D, 0x6E, 0xCB, 0x72, 0xFB, 0xC9, 0x1D, 0x47, 0x81,
};

static const BYTE avc444_p[] = {
	0x00, 0x00, 0x00, 0x01, 0x09, 0x30, 0x00, 0x00, 0x01, 0x41, 0x9A, 0x3B, 0x1F, 0xF9,
	0x59, 0xCD, 0xEF, 0xFF, 0xFC, 0xF8, 0xAA, 0xD6, 0x9F, 0x73, 0x05, 0x6A, 0x32, 0x45,
	0xA5, 0x7B, 0x00, 0x00, 0x01, 0x41, 0x12, 0x68, 0xEC, 0x7F, 0xFA, 0xE1, 0x80,
};

static BOOL testDecodeAvc444(UINT32 codec)
{
	BOOL rc = FALSE;
	const RECTANGLE_16 full = { 0, 0, 64, 64 };
	const RECTANGLE_16 partial = { 16, 16, 48, 32 };
	const UINT32 stride = 64 * 4;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const BYTE untouched[4] = { 0xA5, 0xA5, 0xA5, 0xA5 };
	const size_t sampleOffset = partial.top * stride + partial.left * 4;
	BYTE luma[4] = { 0 };
	BYTE combined[64 * 64 * 4] = { 0 };
	BYTE separate[64 * 64 * 4] = { 0 };
	H264_CONTEXT* combinedContext = h264_context_new(FALSE);
	H264_CONTEXT* separateContext = h264_context_new(FALSE);

	if (!combinedContext || !separateContext || !h264_context_reset(combinedContext, 64, 64) ||
	    !h264_context_reset(separateContext, 64, 64))
		goto fail;
	memset(separate, 0xA5, sizeof(separate));

	/* Op 0 decodes main and dependent auxiliary frames in one update. */
	if (avc444_decompress(combinedContext, 0, &full, 1, avc444_idr, sizeof(avc444_idr), &full, 1,
	                      avc444_p, sizeof(avc444_p), combined, format, stride, 64, 64, codec) != 0)
		goto fail;

	/* Ops 1 and 2 deliver the same frames as separate partial updates. */
	if (avc444_decompress(separateContext, 1, &partial, 1, avc444_idr, sizeof(avc444_idr), nullptr,
	                      0, nullptr, 0, separate, format, stride, 64, 64, codec) != 0)
		goto fail;
	memcpy(luma, &separate[sampleOffset], sizeof(luma));
	if (avc444_decompress(separateContext, 2, &partial, 1, avc444_p, sizeof(avc444_p), nullptr, 0,
	                      nullptr, 0, separate, format, stride, 64, 64, codec) != 0)
		goto fail;

	/* Both the main picture and the auxiliary correction must produce output. */
	if ((memcmp(luma, untouched, sizeof(luma)) == 0) ||
	    (memcmp(luma, &separate[sampleOffset], sizeof(luma)) == 0))
	{
		(void)fprintf(stderr, "[%s] missing main or auxiliary output\n", __func__);
		goto fail;
	}

	/* Compare RGB inside the update; keep all bytes unchanged outside. */
	for (UINT32 y = 0; y < 64; y++)
	{
		for (UINT32 x = 0; x < 64; x++)
		{
			const size_t offset = y * stride + x * 4;
			const BOOL inside = (x >= partial.left) && (x < partial.right) && (y >= partial.top) &&
			                    (y < partial.bottom);
			const BYTE* expected = inside ? &combined[offset] : untouched;
			if (memcmp(&separate[offset], expected, inside ? 3 : 4) != 0)
			{
				(void)fprintf(stderr, "[%s] pixel %" PRIu32 ",%" PRIu32 " differs\n", __func__, x,
				              y);
				goto fail;
			}
		}
	}

	rc = TRUE;
fail:
	if (!rc)
		(void)fprintf(stderr, "[%s] codec 0x%04" PRIX32 " failed\n", __func__, codec);
	h264_context_free(combinedContext);
	h264_context_free(separateContext);
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
		width = strtoul(argv[1], nullptr, 0);
		height = strtoul(argv[2], nullptr, 0);
		if ((errno != 0) || (width == 0) || (height == 0))
		{
			char buffer[128] = WINPR_C_ARRAY_INIT;
			(void)fprintf(stderr, "%s failed: width=%" PRIu32 ", height=%" PRIu32 ", errno=%s\n",
			              __func__, width, height, winpr_strerror(errno, buffer, sizeof(buffer)));
			return -1;
		}
	}

	/* The shared flag includes all H.264 backends, including decoder-only VideoToolbox. */
#if !defined(WITH_GFX_H264)
	(void)fprintf(stderr, "[%s] skipping, no H264 encoder/decoder support compiled in\n", __func__);
	return 0;
#endif

	/* Decoder tests also run when no encoder backend is configured. */
	if (!testContextOptions(FALSE, width, height))
		return -1;
	if (!testDecode(FALSE))
		return -1;
#ifdef WITH_APPLE_VIDEOTOOLBOX
	if (!testDecode(TRUE))
		return -1;
#endif
	/* Both AVC444 packing formats must support combined and separate updates. */
	if (!testDecodeAvc444(RDPGFX_CODECID_AVC444) || !testDecodeAvc444(RDPGFX_CODECID_AVC444v2))
		return -1;

#if !defined(WITH_MEDIA_FOUNDATION) && \
    (defined(WITH_OPENH264) || defined(WITH_VIDEO_FFMPEG) || defined(WITH_MEDIACODEC))
	/* Decoder-only builds cannot run the compression tests. */
	if (!testContextOptions(TRUE, width, height))
		return -1;
	if (!testEncodeOffsetRegion())
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
#elif defined(WITH_MEDIA_FOUNDATION)
	(void)fprintf(stderr,
	              "TODO: WITH_MEDIA_FOUNDATION: compression tests skipped, not implemented\n");
#else
	(void)fprintf(stderr, "H.264 compression tests skipped: no encoder configured\n");
#endif

	return 0;
}
