
#include <stdlib.h>

#include <freerdp/freerdp.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/av1.h>

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

static BOOL testEncodeDecode(uint32_t format, uint32_t width, uint32_t height, BOOL padded)
{
	BOOL rc = FALSE;
	void* src = nullptr;
	void* out = nullptr;
	RDPGFX_H264_METABLOCK meta = WINPR_C_ARRAY_INIT;
	FREERDP_AV1_CONTEXT* enc = freerdp_av1_context_new(TRUE);
	FREERDP_AV1_CONTEXT* dec = freerdp_av1_context_new(FALSE);
	if (!enc || !dec)
		goto fail;

	if (!freerdp_av1_context_reset(enc, width, height))
		goto fail;
	const uint32_t dstWidth = padded ? (width + 15U) & ~15U : width;
	const uint32_t dstHeight = padded ? (height + 15U) & ~15U : height;
	if (!freerdp_av1_context_reset(dec, dstWidth, dstHeight))
		goto fail;

	uint32_t stride = 0;
	uint32_t ostride = 0;
	src = allocRGB(format, width, height, &stride);
	out = allocRGB(format, dstWidth, dstHeight, &ostride);
	if (!src || !out || (stride < width))
		goto fail;

	const RECTANGLE_16 rect = { .left = 0, .top = 0, .right = width, .bottom = height };
	uint8_t* dst = nullptr;
	uint32_t dstsize = 0;
	if (freerdp_av1_compress(enc, src, format, stride, width, height, &rect, &dst, &dstsize,
	                         &meta) < 0)
		goto fail;
	if ((dstsize == 0) || !dst)
		goto fail;

	/* AV1 is a lossy codec, so this only checks that the bitstream produced by the
	 * encoder (always libaom) round-trips through whichever decoder backend is active
	 * (dav1d or libaom) without error. It does not compare decoded pixels. */
	/* Reset and decode the same keyframe repeatedly, including a padded destination.
	 * Padding must stay untouched; ASAN also checks the decoded source plane boundaries. */
	for (size_t round = 0; round < 4; round++)
	{
		if (!freerdp_av1_context_reset(dec, dstWidth, dstHeight))
			goto fail;
		memset(out, 0xA5, (size_t)ostride * dstHeight);
		if (freerdp_av1_decompress(dec, dst, dstsize, out, format, ostride, dstWidth, dstHeight,
		                           &rect, 1) < 0)
			goto fail;
		for (size_t y = 0; y < dstHeight; y++)
		{
			const size_t start = y < height ? width * FreeRDPGetBytesPerPixel(format) : 0;
			for (size_t x = start; x < ostride; x++)
			{
				if (((const BYTE*)out)[y * ostride + x] != 0xA5)
					goto fail;
			}
		}
	}
	rc = TRUE;

fail:
	freerdp_av1_context_free(enc);
	freerdp_av1_context_free(dec);
	free_h264_metablock(&meta);
	free(src);
	free(out);
	return rc;
}

int TestFreeRDPCodecAV1(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

#if !defined(WITH_LIBAOM)
	(void)fprintf(stderr, "[%s] skipping, no AV1 encoder support compiled in\n", __func__);
	return 0;
#else
	const UINT32 width = 124;
	const UINT32 height = 54;
	const UINT32 formats[] = { PIXEL_FORMAT_BGRA32, PIXEL_FORMAT_RGB16 };

	for (size_t x = 0; x < ARRAYSIZE(formats); x++)
	{
		if (!testEncodeDecode(formats[x], width, height, FALSE))
			return -1;
		if (!testEncodeDecode(formats[x], width, height, TRUE))
			return -1;
	}

	return 0;
#endif
}
