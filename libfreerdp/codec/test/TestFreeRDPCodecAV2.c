
#include <stdlib.h>

#include <freerdp/freerdp.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/av2.h>

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

static BOOL testEncodeDecode(uint32_t format, uint32_t width, uint32_t height)
{
	BOOL rc = FALSE;
	void* src = nullptr;
	void* out = nullptr;
	RDPGFX_H264_METABLOCK meta = WINPR_C_ARRAY_INIT;
	FREERDP_AV2_CONTEXT* enc = freerdp_av2_context_new(TRUE);
	FREERDP_AV2_CONTEXT* dec = freerdp_av2_context_new(FALSE);
	if (!enc || !dec)
		goto fail;

	if (!freerdp_av2_context_reset(enc, width, height))
		goto fail;
	if (!freerdp_av2_context_reset(dec, width, height))
		goto fail;

	uint32_t stride = 0;
	uint32_t ostride = 0;
	src = allocRGB(format, width, height, &stride);
	out = allocRGB(format, width, height, &ostride);
	if (!src || !out || (stride < width) || (stride != ostride))
		goto fail;

	const RECTANGLE_16 rect = { .left = 0, .top = 0, .right = width, .bottom = height };
	uint8_t* dst = nullptr;
	uint32_t dstsize = 0;
	if (freerdp_av2_compress(enc, src, format, stride, width, height, &rect, &dst, &dstsize,
	                         &meta) < 0)
		goto fail;
	if ((dstsize == 0) || !dst)
		goto fail;

	/* AV2 is a lossy codec, so this only checks that the bitstream produced by the
	 * encoder (always libavm) round-trips through whichever decoder backend is active
	 * (dav2d or libavm) without error. It does not compare decoded pixels. */
	rc = freerdp_av2_decompress(dec, dst, dstsize, out, format, stride, width, height, &rect, 1) >=
	     0;

fail:
	freerdp_av2_context_free(enc);
	freerdp_av2_context_free(dec);
	free_h264_metablock(&meta);
	free(src);
	free(out);
	return rc;
}

/* Encodes a picture and checks the colours of the decoded picture.
 * YUV444 (profile option 1): the chroma differs within each 2x2 block. A YUV444 conversion,
 * that treats the first chroma sample of a 2x2 block as the average of the block (as done for
 * AVC444), corrupts the first pixel of each 2x2 block.
 * YUV420 (profile option 0): the colour changes per 8x8 block, which 4:2:0 keeps. */
static BOOL testColours(uint32_t profile, uint32_t width, uint32_t height)
{
	const uint32_t format = PIXEL_FORMAT_BGRX32;
	const uint32_t bpp = FreeRDPGetBytesPerPixel(format);
	const uint32_t stride = width * bpp;
	BOOL rc = FALSE;
	uint8_t* src = calloc(stride, height);
	uint8_t* out = calloc(stride, height);
	RDPGFX_H264_METABLOCK meta = WINPR_C_ARRAY_INIT;
	FREERDP_AV2_CONTEXT* enc = freerdp_av2_context_new(TRUE);
	FREERDP_AV2_CONTEXT* dec = freerdp_av2_context_new(FALSE);
	if (!src || !out || !enc || !dec)
		goto fail;

	if (!freerdp_av2_context_reset(enc, width, height) ||
	    !freerdp_av2_context_reset(dec, width, height))
		goto fail;
	if (!freerdp_av2_context_set_option(enc, FREERDP_AV2_CONTEXT_OPTION_PROFILE, profile) ||
	    !freerdp_av2_context_set_option(enc, FREERDP_AV2_CONTEXT_OPTION_RATECONTROL,
	                                    FREERDP_AV2_VBR) ||
	    !freerdp_av2_context_set_option(enc, FREERDP_AV2_CONTEXT_OPTION_BITRATE, 100000))
		goto fail;

	for (uint32_t y = 0; y < height; y++)
	{
		for (uint32_t x = 0; x < width; x++)
		{
			const BOOL first =
			    (profile == 1) ? ((x % 2) == 0) && ((y % 2) == 0) : (((x / 8) + (y / 8)) % 2) == 0;
			const uint32_t color = first ? FreeRDPGetColor(format, 0xA0, 0x60, 0x60, 0xFF)
			                             : FreeRDPGetColor(format, 0x60, 0x60, 0xA0, 0xFF);
			if (!FreeRDPWriteColor(&src[y * stride + x * bpp], format, color))
				goto fail;
		}
	}

	const RECTANGLE_16 rect = { .left = 0, .top = 0, .right = width, .bottom = height };
	uint8_t* dst = nullptr;
	uint32_t dstsize = 0;
	if (freerdp_av2_compress(enc, src, format, stride, width, height, &rect, &dst, &dstsize,
	                         &meta) < 0)
		goto fail;
	if (freerdp_av2_decompress(dec, dst, dstsize, out, format, stride, width, height, &rect, 1) < 0)
		goto fail;

	int maxDiff = 0;
	for (uint32_t y = 0; y < height; y++)
	{
		for (uint32_t x = 0; x < width; x++)
		{
			BYTE r1 = 0;
			BYTE g1 = 0;
			BYTE b1 = 0;
			BYTE r2 = 0;
			BYTE g2 = 0;
			BYTE b2 = 0;
			FreeRDPSplitColor(FreeRDPReadColor(&src[y * stride + x * bpp], format), format, &r1,
			                  &g1, &b1, nullptr, nullptr);
			FreeRDPSplitColor(FreeRDPReadColor(&out[y * stride + x * bpp], format), format, &r2,
			                  &g2, &b2, nullptr, nullptr);
			maxDiff = MAX(maxDiff, abs(r1 - r2));
			maxDiff = MAX(maxDiff, abs(g1 - g2));
			maxDiff = MAX(maxDiff, abs(b1 - b2));
		}
	}

	(void)fprintf(stderr, "[%s] %s %" PRIu32 "x%" PRIu32 ": maximum colour difference %d\n",
	              __func__, (profile == 1) ? "YUV444" : "YUV420", width, height, maxDiff);
	rc = maxDiff <= 32;

fail:
	freerdp_av2_context_free(enc);
	freerdp_av2_context_free(dec);
	free_h264_metablock(&meta);
	free(src);
	free(out);
	return rc;
}

/* Encodes a few frames of a moving picture into one stream, so that all but the first are inter
 * frames, and checks the colours of each decoded frame. */
static BOOL testSequence(uint32_t width, uint32_t height, uint32_t frames)
{
	const uint32_t format = PIXEL_FORMAT_BGRX32;
	const uint32_t bpp = FreeRDPGetBytesPerPixel(format);
	const uint32_t stride = width * bpp;
	BOOL rc = FALSE;
	uint8_t* src = calloc(stride, height);
	uint8_t* out = calloc(stride, height);
	FREERDP_AV2_CONTEXT* enc = freerdp_av2_context_new(TRUE);
	FREERDP_AV2_CONTEXT* dec = freerdp_av2_context_new(FALSE);
	if (!src || !out || !enc || !dec)
		goto fail;

	if (!freerdp_av2_context_reset(enc, width, height) ||
	    !freerdp_av2_context_reset(dec, width, height))
		goto fail;
	if (!freerdp_av2_context_set_option(enc, FREERDP_AV2_CONTEXT_OPTION_PROFILE, 0) ||
	    !freerdp_av2_context_set_option(enc, FREERDP_AV2_CONTEXT_OPTION_RATECONTROL,
	                                    FREERDP_AV2_VBR) ||
	    !freerdp_av2_context_set_option(enc, FREERDP_AV2_CONTEXT_OPTION_BITRATE, 100000))
		goto fail;

	for (uint32_t f = 0; f < frames; f++)
	{
		/* 8x8 blocks, moving 4 pixels to the right per frame */
		for (uint32_t y = 0; y < height; y++)
		{
			for (uint32_t x = 0; x < width; x++)
			{
				const BOOL first = ((((x + 4 * f) / 8) + (y / 8)) % 2) == 0;
				const uint32_t color = first ? FreeRDPGetColor(format, 0xA0, 0x60, 0x60, 0xFF)
				                             : FreeRDPGetColor(format, 0x60, 0x60, 0xA0, 0xFF);
				if (!FreeRDPWriteColor(&src[y * stride + x * bpp], format, color))
					goto fail;
			}
		}

		RDPGFX_H264_METABLOCK meta = WINPR_C_ARRAY_INIT;
		const RECTANGLE_16 rect = { .left = 0, .top = 0, .right = width, .bottom = height };
		uint8_t* dst = nullptr;
		uint32_t dstsize = 0;
		const INT32 crc = freerdp_av2_compress(enc, src, format, stride, width, height, &rect, &dst,
		                                       &dstsize, &meta);
		free_h264_metablock(&meta);
		if ((crc <= 0) || !dst || (dstsize == 0))
			goto fail;
		if (freerdp_av2_decompress(dec, dst, dstsize, out, format, stride, width, height, &rect,
		                           1) < 0)
			goto fail;

		int maxDiff = 0;
		for (uint32_t y = 0; y < height; y++)
		{
			for (uint32_t x = 0; x < width; x++)
			{
				BYTE r1 = 0;
				BYTE g1 = 0;
				BYTE b1 = 0;
				BYTE r2 = 0;
				BYTE g2 = 0;
				BYTE b2 = 0;
				FreeRDPSplitColor(FreeRDPReadColor(&src[y * stride + x * bpp], format), format, &r1,
				                  &g1, &b1, nullptr, nullptr);
				FreeRDPSplitColor(FreeRDPReadColor(&out[y * stride + x * bpp], format), format, &r2,
				                  &g2, &b2, nullptr, nullptr);
				maxDiff = MAX(maxDiff, abs(r1 - r2));
				maxDiff = MAX(maxDiff, abs(g1 - g2));
				maxDiff = MAX(maxDiff, abs(b1 - b2));
			}
		}

		(void)fprintf(stderr,
		              "[%s] frame %" PRIu32 " %" PRIu32 "x%" PRIu32 " (%" PRIu32
		              " bytes): maximum colour difference %d\n",
		              __func__, f, width, height, dstsize, maxDiff);
		if (maxDiff > 32)
			goto fail;
	}
	rc = TRUE;

fail:
	freerdp_av2_context_free(enc);
	freerdp_av2_context_free(dec);
	free(src);
	free(out);
	return rc;
}

int TestFreeRDPCodecAV2(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

#if !defined(WITH_LIBAVM)
	(void)fprintf(stderr, "[%s] skipping, no AV2 encoder support compiled in\n", __func__);
	return 0;
#else
	const UINT32 width = 124;
	const UINT32 height = 54;
	const UINT32 formats[] = { PIXEL_FORMAT_BGRA32, PIXEL_FORMAT_RGB16 };

	for (size_t x = 0; x < ARRAYSIZE(formats); x++)
	{
		if (!testEncodeDecode(formats[x], width, height))
			return -1;
	}

	if (!testColours(0, 64, 64))
		return -1;
	if (!testColours(1, 64, 64))
		return -1;
	if (!testSequence(64, 64, 4))
		return -1;

	return 0;
#endif
}
