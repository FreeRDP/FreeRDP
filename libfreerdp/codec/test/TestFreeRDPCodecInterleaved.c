
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include <winpr/crt.h>
#include <winpr/print.h>

#include <freerdp/freerdp.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/codec/interleaved.h>
#include <winpr/crypto.h>
#include <freerdp/utils/profiler.h>

static BOOL run_encode_decode_single(UINT16 bpp, BITMAP_INTERLEAVED_CONTEXT* encoder,
                                     BITMAP_INTERLEAVED_CONTEXT* decoder
#if defined(WITH_PROFILER)
                                     ,
                                     PROFILER* profiler_comp, PROFILER* profiler_decomp
#endif
)
{
	BOOL rc2 = FALSE;
	BOOL rc;
	UINT32 i, j;
	const UINT32 w = 64;
	const UINT32 h = 64;
	const UINT32 x = 0;
	const UINT32 y = 0;
	const UINT32 format = PIXEL_FORMAT_RGBX32;
	const UINT32 bstep = GetBytesPerPixel(format);
	const size_t step = (w + 13) * 4;
	const size_t SrcSize = step * h;
	const float maxDiff = 4.0f * ((bpp < 24) ? 2.0f : 1.0f);
	UINT32 DstSize = SrcSize;
	BYTE* pSrcData = malloc(SrcSize);
	BYTE* pDstData = malloc(SrcSize);
	BYTE* tmp = malloc(SrcSize);

	if (!pSrcData || !pDstData || !tmp)
		goto fail;

	winpr_RAND(pSrcData, SrcSize);

	if (!bitmap_interleaved_context_reset(encoder) || !bitmap_interleaved_context_reset(decoder))
		goto fail;

	PROFILER_ENTER(profiler_comp);
	rc =
	    interleaved_compress(encoder, tmp, &DstSize, w, h, pSrcData, format, step, x, y, NULL, bpp);
	PROFILER_EXIT(profiler_comp);

	if (!rc)
		goto fail;

	PROFILER_ENTER(profiler_decomp);
	rc = interleaved_decompress(decoder, tmp, DstSize, w, h, bpp, pDstData, format, step, x, y, w,
	                            h, NULL);
	PROFILER_EXIT(profiler_decomp);

	if (!rc)
		goto fail;

	for (i = 0; i < h; i++)
	{
		const BYTE* srcLine = &pSrcData[i * step];
		const BYTE* dstLine = &pDstData[i * step];

		for (j = 0; j < w; j++)
		{
			BYTE r, g, b, dr, dg, db;
			const UINT32 srcColor = ReadColor(&srcLine[j * bstep], format);
			const UINT32 dstColor = ReadColor(&dstLine[j * bstep], format);
			SplitColor(srcColor, format, &r, &g, &b, NULL, NULL);
			SplitColor(dstColor, format, &dr, &dg, &db, NULL, NULL);

			if (fabsf((float)r - dr) > maxDiff)
				goto fail;

			if (fabsf((float)g - dg) > maxDiff)
				goto fail;

			if (fabsf((float)b - db) > maxDiff)
				goto fail;
		}
	}

	rc2 = TRUE;
fail:
	free(pSrcData);
	free(pDstData);
	free(tmp);
	return rc2;
}

static const char* get_profiler_name(BOOL encode, UINT16 bpp)
{
	switch (bpp)
	{
		case 24:
			if (encode)
				return "interleaved_compress   24bpp";
			else
				return "interleaved_decompress 24bpp";

		case 16:
			if (encode)
				return "interleaved_compress   16bpp";
			else
				return "interleaved_decompress 16bpp";

		case 15:
			if (encode)
				return "interleaved_compress   15bpp";
			else
				return "interleaved_decompress 15bpp";

		default:
			return "configuration error!";
	}
}

static BOOL run_encode_decode(UINT16 bpp, BITMAP_INTERLEAVED_CONTEXT* encoder,
                              BITMAP_INTERLEAVED_CONTEXT* decoder)
{
	BOOL rc = FALSE;
	UINT32 x;
	PROFILER_DEFINE(profiler_comp);
	PROFILER_DEFINE(profiler_decomp);
	PROFILER_CREATE(profiler_comp, get_profiler_name(TRUE, bpp))
	PROFILER_CREATE(profiler_decomp, get_profiler_name(FALSE, bpp))

	for (x = 0; x < 500; x++)
	{
		if (!run_encode_decode_single(bpp, encoder, decoder
#if defined(WITH_PROFILER)
		                              ,
		                              profiler_comp, profiler_decomp
#endif
		                              ))
			goto fail;
	}

	rc = TRUE;
fail:
	PROFILER_PRINT_HEADER
	PROFILER_PRINT(profiler_comp);
	PROFILER_PRINT(profiler_decomp);
	PROFILER_PRINT_FOOTER
	PROFILER_FREE(profiler_comp);
	PROFILER_FREE(profiler_decomp);
	return rc;
}

static BOOL TestColorConversion(void)
{
	const UINT32 formats[] = { PIXEL_FORMAT_RGB15,  PIXEL_FORMAT_BGR15, PIXEL_FORMAT_ABGR15,
		                       PIXEL_FORMAT_ARGB15, PIXEL_FORMAT_BGR16, PIXEL_FORMAT_RGB16 };
	UINT32 x;

	/* Check color conversion 15/16 -> 32bit maps to proper values */
	for (x = 0; x < ARRAYSIZE(formats); x++)
	{
		const UINT32 dstFormat = PIXEL_FORMAT_RGBA32;
		const UINT32 format = formats[x];
		const UINT32 colorLow = FreeRDPGetColor(format, 0, 0, 0, 255);
		const UINT32 colorHigh = FreeRDPGetColor(format, 255, 255, 255, 255);
		const UINT32 colorLow32 = FreeRDPConvertColor(colorLow, format, dstFormat, NULL);
		const UINT32 colorHigh32 = FreeRDPConvertColor(colorHigh, format, dstFormat, NULL);
		BYTE r, g, b, a;
		SplitColor(colorLow32, dstFormat, &r, &g, &b, &a, NULL);
		if ((r != 0) || (g != 0) || (b != 0))
			return FALSE;

		SplitColor(colorHigh32, dstFormat, &r, &g, &b, &a, NULL);
		if ((r != 255) || (g != 255) || (b != 255))
			return FALSE;
	}

	return TRUE;
}

int TestFreeRDPCodecInterleaved(int argc, char* argv[])
{
	BITMAP_INTERLEAVED_CONTEXT *encoder, *decoder;
	int rc = -1;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	encoder = bitmap_interleaved_context_new(TRUE);
	decoder = bitmap_interleaved_context_new(FALSE);

	if (!encoder || !decoder)
		goto fail;

	if (!run_encode_decode(24, encoder, decoder))
		goto fail;

	if (!run_encode_decode(16, encoder, decoder))
		goto fail;

	if (!run_encode_decode(15, encoder, decoder))
		goto fail;

	if (!TestColorConversion())
		goto fail;

	rc = 0;
fail:
	bitmap_interleaved_context_free(encoder);
	bitmap_interleaved_context_free(decoder);
	return rc;
}
