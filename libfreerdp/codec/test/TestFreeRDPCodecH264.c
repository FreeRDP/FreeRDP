
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <winpr/sysinfo.h>

#include <freerdp/freerdp.h>
#include <freerdp/codec/color.h>

#if defined(WITH_APPLE_VIDEOTOOLBOX)
#include <CoreMedia/CoreMedia.h>
#include <freerdp/codec/video.h>
#include "../h264.h"
#endif

WINPR_ATTR_NODISCARD
static const char* print_ns(UINT64 start, UINT64 end, char* buffer, size_t len)
{
	WINPR_ASSERT(buffer);

	const UINT64 diff = end - start;
	const unsigned ns = diff % 1000;
	const unsigned us = (diff / 1000) % 1000;
	const unsigned ms = (diff / 1000000) % 1000;
	const unsigned s = (diff / 1000000000ULL) % 1000;
	(void)_snprintf(buffer, len, "%03u %03u %03u %03uns", s, ms, us, ns);
	return buffer;
}

WINPR_ATTR_NODISCARD
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

WINPR_ATTR_MALLOC(free, 1)
static void* allocRGB(uint32_t format, uint32_t width, uint32_t height, uint32_t* pstride)
{
	const size_t bpp = FreeRDPGetBytesPerPixel(format);
	const size_t stride = bpp * width + 32;
	WINPR_ASSERT(pstride);
	*pstride = WINPR_ASSERTING_INT_CAST(uint32_t, stride);

	uint8_t* rgb = calloc(stride, height);
	if (!rgb)
		return nullptr;

	const UINT32 color = FreeRDPGetColor(format, 0x12, 0xaa, 0x63, 0x80);
	for (size_t y = 0; y < height; y++)
	{
		uint8_t* line = &rgb[y * stride];
		for (size_t x = 0; x < width; x++)
		{
			FreeRDPWriteColor(&line[bpp * x], format, color);
		}
	}
	return rgb;
}

WINPR_ATTR_NODISCARD
#define printIfFail(line, offset, what, a, b) \
	printIfFail_(__func__, __LINE__, (line), (offset), (what), (a), (b))
static BOOL printIfFail_(const char* fkt, size_t fline, size_t line, size_t offset,
                         const char* what, BYTE a, BYTE b)
{
	const double maxDiff = 8.0;
	const double diffR = fabs(1.0 * a - b);
	if (diffR > maxDiff)
	{
		(void)fprintf(stderr,
		              "[%s:%" PRIuz "]{%" PRIuz "x%" PRIuz "} %s diff %lf [0x%02" PRIx8
		              " - 0x%02" PRIx8 "]\n",
		              fkt, fline, offset, line, what, diffR, a, b);
		return FALSE;
	}
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL compareRGBColors(size_t line, size_t offset, uint32_t colora, uint32_t colorb,
                             uint32_t format)
{
	BYTE cR = 0;
	BYTE cG = 0;
	BYTE cB = 0;
	BYTE cA = 0;
	BYTE dR = 0;
	BYTE dG = 0;
	BYTE dB = 0;
	BYTE dA = 0;

	FreeRDPSplitColor(colora, format, &cR, &cG, &cB, &cA, nullptr);
	FreeRDPSplitColor(colorb, format, &dR, &dG, &dB, &dA, nullptr);

	if (!printIfFail(line, offset, "red", cR, dR))
		return FALSE;
	if (!printIfFail(line, offset, "green", cG, dG))
		return FALSE;
	if (!printIfFail(line, offset, "blue", cB, dB))
		return FALSE;
	if (FreeRDPColorHasAlpha(format))
	{
		if (!printIfFail(line, offset, "alpha", cA, dA))
			return FALSE;
	}
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL compareRGBLine(size_t line, const uint8_t* src, const uint8_t* dst, uint32_t format,
                           size_t width)
{
	const size_t bpp = FreeRDPGetBytesPerPixel(format);
	for (size_t x = 0; x < width; x++)
	{
		const UINT32 srccolor = FreeRDPReadColor(&src[x * bpp], format);
		const UINT32 dstcolor = FreeRDPReadColor(&dst[x * bpp], format);
		if (!compareRGBColors(line, x, srccolor, dstcolor, format))
			return FALSE;
	}
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL compareRGB(const uint8_t* src, const uint8_t* dst, uint32_t format, size_t width,
                       size_t stride, size_t height)
{
	WINPR_ASSERT(src);
	WINPR_ASSERT(dst);

	const size_t bits = FreeRDPGetBitsPerPixel(format);
	if (bits != 32)
	{
		printf("skipping color compare for format %s, only 32bpp colors are checked\n",
		       FreeRDPGetColorFormatName(format));
		return TRUE;
	}

	for (size_t y = 0; y < height; y++)
	{
		const uint8_t* csrc = &src[y * stride];
		const uint8_t* cdst = &dst[y * stride];
		if (!compareRGBLine(y, csrc, cdst, format, width))
			return FALSE;
	}
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL testEncode(uint32_t format, uint32_t width, uint32_t height)
{
	BOOL rc = FALSE;
	void* src = nullptr;
	void* out = nullptr;
	void* copy = nullptr;
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

	copy = calloc(height, stride);
	if (!copy)
		goto fail;
	memcpy(copy, src, 1ull * height * stride);

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
	if (memcmp(copy, src, 1ull * height * stride) != 0)
	{
		(void)fprintf(stderr, "[%s] avc420_compress modified source data!\n", __func__);
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
	free(copy);
	free(out);
	return rc;
}

WINPR_ATTR_NODISCARD
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

WINPR_ATTR_NODISCARD
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

/* 64x64 solid 0x3366CC HEVC Main without picture reordering (as RDP servers send it),
 * generated with
 * ffmpeg -f lavfi -i color=c=0x3366CC:s=64x64 -frames:v 1 -c:v libx265 -tune zerolatency
 *        -bsf:v "hevc_mp4toannexb,filter_units=remove_types=39|40" -f hevc */
static const BYTE solid_idr_64x64_hevc[] = {
	0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
	0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x1e, 0x92, 0x80, 0x90, 0x00, 0x00, 0x00, 0x01,
	0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
	0x00, 0x1e, 0xa0, 0x20, 0x81, 0x05, 0x96, 0x4a, 0x92, 0x4c, 0xaf, 0x01, 0x68, 0x08, 0x00, 0x00,
	0x03, 0x00, 0x08, 0x00, 0x00, 0x03, 0x00, 0xc8, 0x40, 0x00, 0x00, 0x00, 0x01, 0x44, 0x01, 0xc1,
	0x72, 0xb4, 0x22, 0x40, 0x00, 0x00, 0x01, 0x28, 0x01, 0xaf, 0x78, 0xeb, 0x02, 0x01, 0xff, 0xe2,
	0x61, 0xe8, 0x39, 0xd3, 0x4f, 0xed, 0xbf, 0xd4, 0x54, 0x79, 0xc0
};

/* H264_CONTEXT_OPTION_HEVC: the picture must be available immediately, as for H264.
 * Skipped if no subsystem can decode HEVC (only MediaFoundation can, and only with the
 * HEVC Video Extensions installed). */
WINPR_ATTR_NODISCARD
static BOOL testDecodeHEVC(void)
{
	BOOL rc = FALSE;
	const UINT32 width = 64;
	const UINT32 height = 64;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 stride = width * 4;
	const RECTANGLE_16 rect = { .left = 0, .top = 0, .right = width, .bottom = height };
	H264_CONTEXT* h264 = h264_context_new(FALSE);
	BYTE* dst = calloc(stride, height);

	if (!h264 || !dst)
		goto fail;

	if (!h264_context_set_option(h264, H264_CONTEXT_OPTION_HEVC, TRUE))
	{
		(void)fprintf(stderr, "[%s] no HEVC decoder available, skipping\n", __func__);
		rc = TRUE;
		goto fail;
	}

	if (!h264_context_get_option(h264, H264_CONTEXT_OPTION_HEVC) ||
	    !h264_context_reset(h264, width, height))
		goto fail;

	const INT32 res = avc420_decompress(h264, solid_idr_64x64_hevc, sizeof(solid_idr_64x64_hevc),
	                                    dst, format, stride, width, height, &rect, 1);
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

/* 16x16 HEVC 4:4:4 (Range Extensions) IDR, lossless: one pixel wide columns alternating red
 * (even x) and blue (odd x), which 4:2:0 cannot represent. YCbCr BT.709 full range, but the
 * VUI says RGB (matrix 0) as in the streams Windows servers send. Generated with
 * ffmpeg -f lavfi -i "color=black:s=16x16,format=gbrp,
 *        geq=r='if(mod(X\,2)\,0\,255)':g=0:b='if(mod(X\,2)\,255\,0)'" -frames:v 1
 *        -vf "scale=out_color_matrix=bt709:out_range=full,format=yuv444p" -c:v libx265
 *        -tune zerolatency -x265-params lossless=1:colormatrix=gbr:range=limited:
 *        transfer=iec61966-2-1:info=0 -bsf:v "filter_units=remove_types=39|40" -f hevc */
static const BYTE columns_idr_16x16_hevc444[] = {
	0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x04, 0x08, 0x00, 0x00, 0x03, 0x00,
	0x9e, 0x08, 0x00, 0x00, 0x03, 0x00, 0x00, 0xff, 0x92, 0x80, 0x90, 0x00, 0x00, 0x00, 0x01, 0x42,
	0x01, 0x01, 0x04, 0x08, 0x00, 0x00, 0x03, 0x00, 0x9e, 0x08, 0x00, 0x00, 0x03, 0x00, 0x00, 0xff,
	0x90, 0x11, 0x08, 0xb2, 0xc9, 0x55, 0x79, 0x5e, 0x02, 0xd4, 0x08, 0x34, 0x00, 0x10, 0x00, 0x00,
	0x03, 0x00, 0x10, 0x00, 0x00, 0x03, 0x01, 0x90, 0x80, 0x00, 0x00, 0x00, 0x01, 0x44, 0x01, 0xc1,
	0x70, 0x30, 0x62, 0x91, 0x20, 0x00, 0x00, 0x01, 0x28, 0x01, 0xaf, 0x05, 0xb8, 0x24, 0x9a, 0x1c,
	0x60, 0x4b, 0x3f, 0xff, 0xf5, 0x3f, 0xea, 0x3f, 0x80, 0xfc, 0xe7, 0x91, 0xf1, 0x9f, 0x19, 0xe4,
	0x7c, 0x6f, 0x93, 0xe4, 0xfc, 0x6f, 0x93, 0xf1, 0xbe, 0x4f, 0x92, 0x54, 0x3f, 0xfd, 0xfd, 0xb2,
	0x97, 0x10, 0xff, 0x71, 0xae, 0xdf, 0x3b, 0x3a, 0xd5, 0x8e, 0x18, 0x61, 0xac, 0xdf, 0x8d, 0xf8,
	0xdf, 0x8d, 0xd4, 0x30, 0xc2, 0xc8, 0x8f, 0xf5, 0x8c, 0x3a, 0x78, 0x7d, 0x3f, 0x86, 0x3a, 0x3f,
	0xd1, 0xc6, 0xf5, 0x7f, 0x53, 0xa6, 0x31, 0xd1, 0x5e, 0xe5, 0xee, 0x5e, 0xe5, 0x95, 0xfc, 0x76,
	0xb7, 0xae, 0xf0, 0xfe, 0x01, 0x62, 0x12, 0x64, 0xc9, 0x93, 0x13, 0xd3, 0x16, 0x82, 0xbf, 0xfa,
	0xe6, 0xfd, 0x70, 0xad, 0x65, 0x6a, 0x9d, 0xc7, 0xbc, 0x7b, 0xc7, 0xbc, 0x78, 0xcf, 0x83, 0x1f,
	0xaf, 0x3c, 0x4a, 0xae, 0x23, 0x6e, 0x1b, 0xcb, 0xd4, 0x2e, 0x02, 0xe0, 0x2e, 0x02, 0xdd, 0x6e,
	0x22, 0x83, 0x78, 0x04, 0xba, 0x47, 0x92, 0x6b, 0x28, 0xd0, 0x3b, 0x3e, 0xbe, 0xba, 0x07, 0x42,
	0xf9, 0x18, 0x62, 0xf2, 0x67, 0x96, 0xe5, 0xe8, 0x58, 0xb0, 0x23, 0x53, 0x38, 0x11, 0x03, 0x66,
	0xd9, 0xab, 0x63, 0xe1, 0x34, 0xc9, 0xd6, 0xc7, 0x40, 0x26, 0xcd, 0x9b, 0x34, 0xf5, 0x9f, 0xa3,
	0xff, 0xf0, 0xb4, 0xf8, 0x52, 0x08, 0x30, 0x3f, 0xd9, 0x57, 0x35, 0x73, 0x57, 0x35, 0x6a, 0x33,
	0xc0, 0xdb, 0x0b, 0x42, 0x54, 0xa0, 0xc8, 0x9f, 0x42, 0x2a, 0xa7, 0x9e, 0x79, 0xe7, 0x9e, 0x77,
	0x76, 0x89, 0xb6, 0x1c, 0x43, 0x9c, 0x3a, 0x15, 0x40, 0xa9, 0x52, 0xa5, 0x22, 0x95, 0x05, 0x3f,
	0xf3, 0xc3, 0xf9, 0xdb, 0x39, 0xc0, 0x33, 0x14, 0x45, 0xc4, 0x5c, 0x45, 0xc4, 0x54, 0xb9, 0x95,
	0xbe, 0x2c, 0x8c, 0xd2, 0x44, 0xa2, 0x3d, 0x88, 0x1e, 0x93, 0xb9, 0x3b, 0x93, 0xb9, 0x38, 0xd0,
	0x12, 0xbb, 0xe9, 0xad, 0x17, 0x82, 0x0c, 0xbe, 0xe3, 0x83, 0xec, 0xbb, 0x51, 0x55, 0x54, 0xbd,
	0xba, 0xa0, 0x0c, 0x26, 0xe3, 0x1b, 0x2e, 0xdc, 0x5f, 0xf0, 0x28, 0x3c, 0x15, 0x75, 0x39, 0x5b,
	0xe4, 0xb1, 0x1f, 0x38, 0x5f, 0xdf, 0x9a, 0xae, 0x59, 0x71, 0xf5, 0x79, 0x21, 0x85, 0xe2, 0x03,
	0xa6, 0x81, 0x22, 0xbb, 0xc6, 0x10, 0x8d, 0xc9, 0xa9, 0xf7, 0x04, 0x6a, 0xd7, 0x57, 0xf5, 0xd6,
	0x08, 0xb1, 0x49, 0xec, 0x23, 0xc9, 0x1a, 0x7e, 0x4e, 0x87, 0x05, 0x6c, 0xdc, 0x34, 0x02, 0xf7,
	0x05, 0xc8, 0x69, 0xaa, 0xce, 0xf9, 0x24, 0xfb, 0xfc, 0x13, 0xcc, 0x3e, 0xa6, 0xef, 0x26, 0x3c,
	0xb3, 0x12, 0x58, 0xfb, 0x4e, 0x8f, 0x29, 0xff, 0x02, 0x72, 0x65, 0xf7, 0xb1, 0x2e, 0xb4, 0x45,
	0xb9, 0xf3, 0xf3, 0xe3, 0xfd, 0x09, 0xed, 0xd6, 0x5b, 0x5d, 0x6d, 0xfe, 0x16, 0x05, 0x6d, 0x5c,
	0x74, 0xa7, 0xc0, 0x93, 0x4b, 0x3d, 0x87, 0x14, 0xe7, 0x25, 0x2d, 0x8e, 0x19, 0x9b, 0x03, 0x8d,
	0x70, 0xba, 0xb0, 0x13, 0x2c, 0xb8
};

/* H264_CONTEXT_OPTION_HEVC444: every pixel keeps its own colour, also in a region that does not
 * start at the picture origin, and pixels outside the regions are left alone. Skipped if no
 * subsystem can decode HEVC 4:4:4. */
WINPR_ATTR_NODISCARD
static BOOL testDecodeHEVC444(void)
{
	BOOL rc = FALSE;
	const UINT32 width = 16;
	const UINT32 height = 16;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 stride = width * 4;
	/* two regions, the right one starting at an odd column; column 15 and row 0 stay unset */
	const RECTANGLE_16 rects[] = { { .left = 0, .top = 1, .right = 8, .bottom = height },
		                           { .left = 9, .top = 1, .right = 15, .bottom = height } };
	const BYTE fill = 0x42;
	H264_CONTEXT* h264 = h264_context_new(FALSE);
	BYTE* dst = malloc(1ull * stride * height);

	if (!h264 || !dst)
		goto fail;
	memset(dst, fill, 1ull * stride * height);

	if (!h264_context_set_option(h264, H264_CONTEXT_OPTION_HEVC, TRUE) ||
	    !h264_context_set_option(h264, H264_CONTEXT_OPTION_HEVC444, TRUE))
	{
		(void)fprintf(stderr, "[%s] no HEVC 4:4:4 decoder available, skipping\n", __func__);
		rc = TRUE;
		goto fail;
	}

	if (!h264_context_get_option(h264, H264_CONTEXT_OPTION_HEVC444) ||
	    !h264_context_reset(h264, width, height))
		goto fail;

	const INT32 res =
	    avc420_decompress(h264, columns_idr_16x16_hevc444, sizeof(columns_idr_16x16_hevc444), dst,
	                      format, stride, width, height, rects, ARRAYSIZE(rects));
	if (res < 0)
	{
		(void)fprintf(stderr, "[%s] avc420_decompress failed: %" PRId32 "\n", __func__, res);
		goto fail;
	}

	for (UINT32 y = 0; y < height; y++)
	{
		for (UINT32 x = 0; x < width; x++)
		{
			const BYTE* px = &dst[y * stride + x * 4];
			const BOOL inside = (y >= 1) && (x != 8) && (x != 15);
			if (!inside)
			{
				if ((px[0] != fill) || (px[1] != fill) || (px[2] != fill) || (px[3] != fill))
				{
					(void)fprintf(
					    stderr, "[%s] pixel %" PRIu32 ",%" PRIu32 " outside the regions changed\n",
					    __func__, x, y);
					goto fail;
				}
				continue;
			}

			BYTE r = 0;
			BYTE g = 0;
			BYTE b = 0;
			FreeRDPSplitColor(FreeRDPReadColor(px, format), format, &r, &g, &b, nullptr, nullptr);
			const BYTE er = (x % 2) ? 0x00 : 0xFF;
			const BYTE eb = (x % 2) ? 0xFF : 0x00;
			if ((abs(r - er) > 8) || (g > 8) || (abs(b - eb) > 8))
			{
				(void)fprintf(stderr,
				              "[%s] pixel %" PRIu32 ",%" PRIu32
				              " is %02X%02X%02X, expected %02X00%02X\n",
				              __func__, x, y, r, g, b, er, eb);
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

WINPR_ATTR_NODISCARD
static BOOL testDecodeAvc444(UINT32 codec)
{
	BOOL rc = FALSE;
	const RECTANGLE_16 full = { 0, 0, 64, 64 };
	const RECTANGLE_16 partial = { 16, 16, 48, 32 };
	const UINT32 stride = 64 * 4;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const BYTE untouched[4] = { 0xA5, 0xA5, 0xA5, 0xA5 };
	const size_t sampleOffset = partial.top * stride + partial.left * 4;
	BYTE luma[4] = WINPR_C_ARRAY_INIT;
	BYTE combined[64 * 64 * 4] = WINPR_C_ARRAY_INIT;
	BYTE separate[64 * 64 * 4] = WINPR_C_ARRAY_INIT;
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

#if defined(WITH_APPLE_VIDEOTOOLBOX)
/* VideoToolbox writes SPS/PPS first, each with a four-byte Annex B start code. */
WINPR_ATTR_NODISCARD
static BOOL testH264Range(const BYTE* data, size_t size, BOOL fullRange)
{
	WINPR_ASSERT(data);
	const BYTE startCode[] = { 0, 0, 0, 1 };
	const BYTE* parameters[2] = WINPR_C_ARRAY_INIT;
	size_t lengths[2] = WINPR_C_ARRAY_INIT;
	size_t offset = 0;
	for (size_t i = 0; i < ARRAYSIZE(parameters); i++)
	{
		if ((size - offset < 5) || (memcmp(data + offset, startCode, sizeof(startCode)) != 0) ||
		    ((data[offset + 4] & 0x1F) != 7 + i))
			return FALSE;
		offset += sizeof(startCode);
		parameters[i] = data + offset;
		const size_t start = offset;
		while ((size - offset >= sizeof(startCode)) &&
		       (memcmp(data + offset, startCode, sizeof(startCode)) != 0))
			offset++;
		if (size - offset < sizeof(startCode))
			return FALSE;
		lengths[i] = offset - start;
	}
	CMVideoFormatDescriptionRef format = nullptr;
	if (CMVideoFormatDescriptionCreateFromH264ParameterSets(
	        kCFAllocatorDefault, ARRAYSIZE(parameters), parameters, lengths, 4, &format) != noErr)
		return FALSE;
	const BOOL actual = CMFormatDescriptionGetExtension(
	                        format, kCMFormatDescriptionExtension_FullRangeVideo) == kCFBooleanTrue
	                        ? TRUE
	                        : FALSE;
	CFRelease(format);
	if (actual != fullRange)
		(void)fprintf(stderr, "[%s] full range %d, expected %d\n", __func__, actual, fullRange);
	return actual == fullRange ? TRUE : FALSE;
}

WINPR_ATTR_NODISCARD
static BOOL testCameraEncode(UINT32 width, UINT32 height)
{
	BOOL rc = FALSE;
	const BYTE samples[][3] = { { 16, 128, 128 }, { 235, 128, 128 }, { 96, 64, 192 } };
	const size_t pixels = (size_t)width * height;
	BYTE* nv12 = calloc(pixels * 3 / 2, 1);
	wStream* output = Stream_New(nullptr, 1);
	FREERDP_VIDEO_CONTEXT* video = freerdp_video_context_new(width, height);
	H264_CONTEXT* decoder = h264_context_new(FALSE);
	if (!nv12 || !output || !video || !decoder || !h264_context_reset(decoder, width, height) ||
	    !freerdp_video_conversion_supported(FREERDP_VIDEO_FORMAT_NV12, FREERDP_VIDEO_FORMAT_H264))
		goto fail;

	for (size_t frame = 0; frame < ARRAYSIZE(samples); frame++)
	{
		/* Camera redirection reconfigures before every sample. */
		if (!freerdp_video_context_reconfigure(video, width, height, 30, 0,
		                                       H264_CAMERA_VIDEO_REAL_TIME))
			goto fail;
		memset(nv12, samples[frame][0], pixels);
		for (size_t i = 0; i < pixels / 2; i += 2)
		{
			nv12[pixels + i] = samples[frame][1];
			nv12[pixels + i + 1] = samples[frame][2];
		}
		Stream_ResetPosition(output);
		Stream_Write_UINT8(output, 0x5a);
		if (!freerdp_video_sample_convert(video, FREERDP_VIDEO_FORMAT_NV12, nv12, pixels * 3 / 2,
		                                  FREERDP_VIDEO_FORMAT_H264, output) ||
		    (Stream_GetPosition(output) <= 1) || (Stream_Buffer(output)[0] != 0x5a))
			goto fail;
		const BYTE* encoded = Stream_Buffer(output) + 1;
		const size_t size = Stream_GetPosition(output) - 1;
		if ((size > UINT32_MAX) || ((frame == 0) && !testH264Range(encoded, size, FALSE)) ||
		    (decoder->subsystem->Decompress(decoder, encoded, (UINT32)size) <= 0))
			goto fail;
		/* Inspect decoded planes: the desktop RGB converter assumes full-range YUV. */
		for (size_t plane = 0; plane < 3; plane++)
		{
			const UINT32 divisor = plane == 0 ? 1 : 2;
			const size_t center =
			    (size_t)(height / (2 * divisor)) * decoder->iStride[plane] + width / (2 * divisor);
			if (!decoder->pYUVData[plane] ||
			    (abs(decoder->pYUVData[plane][center] - samples[frame][plane]) > 4))
				goto fail;
		}
	}
	rc = TRUE;
fail:
	if (!rc)
		(void)fprintf(stderr, "[%s] NV12 camera encoding failed at %" PRIu32 "x%" PRIu32 "\n",
		              __func__, width, height);
	h264_context_free(decoder);
	freerdp_video_context_free(video);
	Stream_Free(output, TRUE);
	free(nv12);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL testEncodeRange(void)
{
	BOOL rc = FALSE;
	H264_CONTEXT* encoder = h264_context_new(TRUE);
	if (!encoder || !h264_context_reset(encoder, 64, 48))
		goto fail;
	BYTE* planes[3] = WINPR_C_ARRAY_INIT;
	UINT32 strides[3] = WINPR_C_ARRAY_INIT;
	if (h264_get_yuv_buffer(encoder, 64, 64, 48, planes, strides) < 0)
		goto fail;
	for (size_t plane = 0; plane < 3; plane++)
		memset(planes[plane], 128, (size_t)strides[plane] * (plane == 0 ? 48 : 24));
	const H264_USAGETYPE usage[] = { H264_SCREEN_CONTENT_REAL_TIME, H264_CAMERA_VIDEO_REAL_TIME,
		                             H264_SCREEN_CONTENT_NON_REAL_TIME,
		                             H264_CAMERA_VIDEO_NON_REAL_TIME,
		                             H264_SCREEN_CONTENT_REAL_TIME };
	for (size_t i = 0; i < ARRAYSIZE(usage); i++)
	{
		BYTE* encoded = nullptr;
		UINT32 size = 0;
		const BOOL fullRange = i % 2 == 0 ? TRUE : FALSE;
		if (!h264_context_set_option(encoder, H264_CONTEXT_OPTION_USAGETYPE, usage[i]) ||
		    (h264_context_get_option(encoder, H264_CONTEXT_OPTION_USAGETYPE) != usage[i]) ||
		    (h264_compress(encoder, &encoded, &size) < 0) || !encoded ||
		    !testH264Range(encoded, size, fullRange))
			goto fail;
	}
	rc = TRUE;
fail:
	if (!rc)
		(void)fprintf(stderr, "[%s] encoder range change failed\n", __func__);
	h264_context_free(encoder);
	return rc;
}
#endif

WINPR_ATTR_NODISCARD
int TestFreeRDPCodecH264(int argc, char* argv[])
{
	WINPR_ASSERT(argv);

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

	/* The shared flag includes all H.264 backends. */
#if !defined(WITH_GFX_H264)
	(void)fprintf(stderr, "[%s] skipping, no H264 encoder/decoder support compiled in\n", __func__);
	return 0;
#endif

#if defined(WITH_H264_WEBCODECS)
	/* WebCodecs is probed at runtime; node (ctest) has no VideoDecoder. */
	H264_CONTEXT* probe = h264_context_new(FALSE);
	if (!probe)
	{
		(void)fprintf(stderr, "[%s] skipping, the runtime cannot decode H.264 with WebCodecs\n",
		              __func__);
		return 0;
	}
	h264_context_free(probe);
#endif

	/* Decoder tests also run when no encoder backend is configured. */
	if (!testContextOptions(FALSE, width, height))
		return -1;
	if (!testDecode(FALSE))
		return -1;
#if defined(WITH_APPLE_VIDEOTOOLBOX) || defined(WITH_H264_WEBCODECS)
	if (!testDecode(TRUE))
		return -1;
#endif
	if (!testDecodeHEVC())
		return -1;
	if (!testDecodeHEVC444())
		return -1;
	/* Both AVC444 packing formats must support combined and separate updates. */
	if (!testDecodeAvc444(RDPGFX_CODECID_AVC444) || !testDecodeAvc444(RDPGFX_CODECID_AVC444v2))
		return -1;

#if defined(WITH_MEDIA_FOUNDATION) || defined(WITH_OPENH264) || defined(WITH_VIDEO_FFMPEG) || \
    defined(WITH_MEDIACODEC) || defined(WITH_APPLE_VIDEOTOOLBOX)
#if defined(WITH_MEDIA_FOUNDATION)
	/* Windows ships the Media Foundation H.264 encoder, but not every edition: N editions
	 * and Windows Server without the Media Foundation feature have none. */
	H264_CONTEXT* encoder = h264_context_new(TRUE);
	if (!encoder)
	{
		(void)fprintf(stderr, "[%s] H.264 compression tests skipped: no encoder on this system\n",
		              __func__);
		return 0;
	}
	h264_context_free(encoder);
#endif

	/* Decoder-only builds cannot run the compression tests. */
	if (!testContextOptions(TRUE, width, height))
		return -1;
	if (!testEncodeOffsetRegion())
		return -1;

#if defined(WITH_APPLE_VIDEOTOOLBOX)
	H264_CONTEXT* vtEncoder = h264_context_new(TRUE);
	const BOOL useVideoToolbox =
	    vtEncoder && (strcmp(vtEncoder->subsystem->name, "VideoToolbox") == 0) ? TRUE : FALSE;
	h264_context_free(vtEncoder);
	if (useVideoToolbox)
	{
		if (!testCameraEncode(64, 48) || !testCameraEncode(128, 96) || !testEncodeRange())
			return -1;
	}
#endif

	for (size_t x = 0; x < ARRAYSIZE(formats); x++)
	{
		const UINT32 SrcFormat = formats[x];
		for (size_t y = 0; y < ARRAYSIZE(formats); y++)
		{
			if (!testEncode(SrcFormat, width, height))
				return -1;
		}
	}
#else
	(void)fprintf(stderr, "H.264 compression tests skipped: no encoder configured\n");
#endif

	return 0;
}
