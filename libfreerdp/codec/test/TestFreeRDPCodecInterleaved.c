
#include <freerdp/config.h>

#include <math.h>

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/json.h>
#include <winpr/path.h>

#include <freerdp/freerdp.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/codec/interleaved.h>
#include <winpr/crypto.h>
#include <freerdp/utils/profiler.h>

#include "TestFreeRDPHelpers.h"

// #define CREATE_TEST_OUTPUT

static bool run_encode_decode_single(UINT16 bpp, BITMAP_INTERLEAVED_CONTEXT* encoder,
                                     BITMAP_INTERLEAVED_CONTEXT* decoder
#if defined(WITH_PROFILER)
                                     ,
                                     PROFILER* profiler_comp, PROFILER* profiler_decomp
#endif
)
{
	bool rc2 = false;
	bool rc = 0;
	const UINT32 w = 64;
	const UINT32 h = 64;
	const UINT32 x = 0;
	const UINT32 y = 0;
	const UINT32 format = PIXEL_FORMAT_RGBX32;
	const UINT32 bstep = FreeRDPGetBytesPerPixel(format);
	const size_t step = (13ULL + w) * 4ULL;
	const size_t SrcSize = step * h;
	const int maxDiff = 4 * ((bpp < 24) ? 2 : 1);
	UINT32 DstSize = SrcSize;
	BYTE* pSrcData = calloc(1, SrcSize);
	BYTE* pDstData = calloc(1, SrcSize);
	BYTE* tmp = calloc(1, SrcSize);

	if (!pSrcData || !pDstData || !tmp)
		goto fail;

	winpr_RAND(pSrcData, SrcSize);

	if (!bitmap_interleaved_context_reset(encoder) || !bitmap_interleaved_context_reset(decoder))
		goto fail;

	PROFILER_ENTER(profiler_comp)
	rc =
	    interleaved_compress(encoder, tmp, &DstSize, w, h, pSrcData, format, step, x, y, NULL, bpp);
	PROFILER_EXIT(profiler_comp)

	if (!rc)
		goto fail;

	PROFILER_ENTER(profiler_decomp)
	rc = interleaved_decompress(decoder, tmp, DstSize, w, h, bpp, pDstData, format, step, x, y, w,
	                            h, NULL);
	PROFILER_EXIT(profiler_decomp)

	if (!rc)
		goto fail;

	for (UINT32 i = 0; i < h; i++)
	{
		const BYTE* srcLine = &pSrcData[i * step];
		const BYTE* dstLine = &pDstData[i * step];

		for (UINT32 j = 0; j < w; j++)
		{
			BYTE r = 0;
			BYTE g = 0;
			BYTE b = 0;
			BYTE dr = 0;
			BYTE dg = 0;
			BYTE db = 0;
			const UINT32 srcColor = FreeRDPReadColor(&srcLine[1ULL * j * bstep], format);
			const UINT32 dstColor = FreeRDPReadColor(&dstLine[1ULL * j * bstep], format);
			FreeRDPSplitColor(srcColor, format, &r, &g, &b, NULL, NULL);
			FreeRDPSplitColor(dstColor, format, &dr, &dg, &db, NULL, NULL);

			if (abs(r - dr) > maxDiff)
				goto fail;

			if (abs(g - dg) > maxDiff)
				goto fail;

			if (abs(b - db) > maxDiff)
				goto fail;
		}
	}

	rc2 = true;
fail:
	free(pSrcData);
	free(pDstData);
	free(tmp);
	return rc2;
}

static const char* get_profiler_name(bool encode, UINT16 bpp)
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

static bool run_encode_decode(UINT16 bpp, BITMAP_INTERLEAVED_CONTEXT* encoder,
                              BITMAP_INTERLEAVED_CONTEXT* decoder)
{
	bool rc = false;
	PROFILER_DEFINE(profiler_comp)
	PROFILER_DEFINE(profiler_decomp)
	PROFILER_CREATE(profiler_comp, get_profiler_name(true, bpp))
	PROFILER_CREATE(profiler_decomp, get_profiler_name(false, bpp))

	for (UINT32 x = 0; x < 50; x++)
	{
		if (!run_encode_decode_single(bpp, encoder, decoder
#if defined(WITH_PROFILER)
		                              ,
		                              profiler_comp, profiler_decomp
#endif
		                              ))
			goto fail;
	}

	rc = true;
fail:
	PROFILER_PRINT_HEADER
	PROFILER_PRINT(profiler_comp)
	PROFILER_PRINT(profiler_decomp)
	PROFILER_PRINT_FOOTER
	PROFILER_FREE(profiler_comp)
	PROFILER_FREE(profiler_decomp)
	return rc;
}

static bool TestColorConversion(void)
{
	const UINT32 formats[] = { PIXEL_FORMAT_RGB15,  PIXEL_FORMAT_BGR15, PIXEL_FORMAT_ABGR15,
		                       PIXEL_FORMAT_ARGB15, PIXEL_FORMAT_BGR16, PIXEL_FORMAT_RGB16 };

	/* Check color conversion 15/16 -> 32bit maps to proper values */
	for (UINT32 x = 0; x < ARRAYSIZE(formats); x++)
	{
		const UINT32 dstFormat = PIXEL_FORMAT_RGBA32;
		const UINT32 format = formats[x];
		const UINT32 colorLow = FreeRDPGetColor(format, 0, 0, 0, 255);
		const UINT32 colorHigh = FreeRDPGetColor(format, 255, 255, 255, 255);
		const UINT32 colorLow32 = FreeRDPConvertColor(colorLow, format, dstFormat, NULL);
		const UINT32 colorHigh32 = FreeRDPConvertColor(colorHigh, format, dstFormat, NULL);
		BYTE r = 0;
		BYTE g = 0;
		BYTE b = 0;
		BYTE a = 0;
		FreeRDPSplitColor(colorLow32, dstFormat, &r, &g, &b, &a, NULL);
		if ((r != 0) || (g != 0) || (b != 0))
			return false;

		FreeRDPSplitColor(colorHigh32, dstFormat, &r, &g, &b, &a, NULL);
		if ((r != 255) || (g != 255) || (b != 255))
			return false;
	}

	return true;
}

static bool RunEncoderTest(const char* name, uint32_t format, uint32_t width, uint32_t height,
                           uint32_t step, uint32_t bpp)
{
	bool rc = false;
	void* data = NULL;
	void* encdata = NULL;
	BITMAP_INTERLEAVED_CONTEXT* encoder = bitmap_interleaved_context_new(true);
	if (!encoder)
		goto fail;

	size_t srclen = 0;
	data = test_codec_helper_read_data("interleaved", "bmp", name, &srclen);
	if (!data)
		goto fail;

	encdata = calloc(srclen, 1);
	if (!encdata)
		goto fail;

	for (size_t x = 0; x < 42; x++)
	{
		uint32_t enclen = WINPR_ASSERTING_INT_CAST(uint32_t, srclen);
		if (!interleaved_compress(encoder, encdata, &enclen, width, height, data, format, step, 0,
		                          0, NULL, bpp))
			goto fail;

		char encname[128] = { 0 };
		(void)_snprintf(encname, sizeof(encname), "enc-%" PRIu32, bpp);
#if defined(CREATE_TEST_OUTPUT)
		test_codec_helper_write_data("interleaved", encname, name, encdata, enclen);
#else
		if (!test_codec_helper_compare("interleaved", encname, name, encdata, enclen))
			goto fail;
#endif
	}

	rc = true;

fail:
	free(data);
	free(encdata);
	bitmap_interleaved_context_free(encoder);
	return rc;
}

static bool RunDecoderTest(const char* name, uint32_t format, uint32_t width, uint32_t height,
                           uint32_t step, uint32_t bpp)
{
	bool rc = false;
	void* data = NULL;
	void* decdata = NULL;
	BITMAP_INTERLEAVED_CONTEXT* decoder = bitmap_interleaved_context_new(false);
	if (!decoder)
		goto fail;

	char encname[128] = { 0 };
	(void)_snprintf(encname, sizeof(encname), "enc-%" PRIu32, bpp);

	size_t srclen = 0;
	data = test_codec_helper_read_data("interleaved", encname, name, &srclen);
	if (!data)
		goto fail;

	const size_t declen = 1ULL * step * height;
	decdata = calloc(step, height);
	if (!decdata)
		goto fail;

	for (size_t x = 0; x < 42; x++)
	{
		if (!interleaved_decompress(decoder, data, WINPR_ASSERTING_INT_CAST(uint32_t, srclen),
		                            width, height, bpp, decdata, format, step, 0, 0, width, height,
		                            NULL))
			goto fail;

		char decname[128] = { 0 };
		(void)_snprintf(decname, sizeof(decname), "dec-%s", encname);
#if defined(CREATE_TEST_OUTPUT)
		test_codec_helper_write_data("interleaved", decname, name, decdata, declen);
#else
		if (!test_codec_helper_compare("interleaved", decname, name, decdata, declen))
			goto fail;
#endif
	}

	rc = true;

fail:
	free(data);
	free(decdata);
	bitmap_interleaved_context_free(decoder);
	return rc;
}

/* The encoder expects a JSON that describes a test cast:
 *
 * [
 * {
 *   "name": "somestring",
 *   "format": "somestring",
 *   "width": <someint>,
 *   "height": <someint>,
 *   "step": <someint>,
 *   "bpp": <someint>
 * },
 * {...},
 * ...
 * ]
 */
static bool isObjectValid(const WINPR_JSON* obj)
{
	if (!obj || !WINPR_JSON_IsObject(obj))
		return false;

	const char* strvalues[] = { "name", "format" };
	for (size_t x = 0; x < ARRAYSIZE(strvalues); x++)
	{
		const char* val = strvalues[x];

		if (!WINPR_JSON_HasObjectItem(obj, val))
			return false;

		WINPR_JSON* jval = WINPR_JSON_GetObjectItem(obj, val);
		if (!jval)
			return false;

		if (!WINPR_JSON_IsString(jval))
			return false;
	}

	const char* values[] = { "width", "height", "step" };
	for (size_t x = 0; x < ARRAYSIZE(values); x++)
	{
		const char* val = values[x];
		if (!WINPR_JSON_HasObjectItem(obj, val))
			return false;
		WINPR_JSON* jval = WINPR_JSON_GetObjectItem(obj, val);
		if (!jval)
			return false;
		if (!WINPR_JSON_IsNumber(jval))
			return false;
		const double dval = WINPR_JSON_GetNumberValue(jval);
		if (dval <= 0.0)
			return false;
	}

	{
		const char* val = "bpp";

		if (!WINPR_JSON_HasObjectItem(obj, val))
			return false;

		WINPR_JSON* jval = WINPR_JSON_GetObjectItem(obj, val);
		if (!jval)
			return false;

		if (!WINPR_JSON_IsArray(jval))
			return false;

		for (size_t x = 0; x < WINPR_JSON_GetArraySize(jval); x++)
		{
			WINPR_JSON* aval = WINPR_JSON_GetArrayItem(jval, x);
			if (!jval || !WINPR_JSON_IsNumber(aval))
				return false;
		}
	}
	return true;
}

static bool TestEncoder(void)
{
	bool rc = false;
	WINPR_JSON* json = NULL;
	char* file = NULL;
	char* path = GetCombinedPath(CMAKE_CURRENT_SOURCE_DIR, "interleaved");
	if (!path)
		goto fail;
	file = GetCombinedPath(path, "encoder.json");
	if (!file)
		goto fail;

	json = WINPR_JSON_ParseFromFile(file);
	if (!json)
		goto fail;

	if (!WINPR_JSON_IsArray(json))
		goto fail;

	for (size_t x = 0; x < WINPR_JSON_GetArraySize(json); x++)
	{
		WINPR_JSON* obj = WINPR_JSON_GetArrayItem(json, x);
		if (!isObjectValid(obj))
			goto fail;

		const char* name = WINPR_JSON_GetStringValue(WINPR_JSON_GetObjectItem(obj, "name"));
		const uint32_t format = WINPR_ASSERTING_INT_CAST(
		    uint32_t, FreeRDPGetColorFromatFromName(
		                  WINPR_JSON_GetStringValue(WINPR_JSON_GetObjectItem(obj, "format"))));
		const uint32_t width = WINPR_ASSERTING_INT_CAST(
		    uint32_t, WINPR_JSON_GetNumberValue(WINPR_JSON_GetObjectItem(obj, "width")));
		const uint32_t height = WINPR_ASSERTING_INT_CAST(
		    uint32_t, WINPR_JSON_GetNumberValue(WINPR_JSON_GetObjectItem(obj, "height")));
		const uint32_t step = WINPR_ASSERTING_INT_CAST(
		    uint32_t, WINPR_JSON_GetNumberValue(WINPR_JSON_GetObjectItem(obj, "step")));

		WINPR_JSON* jbpp = WINPR_JSON_GetObjectItem(obj, "bpp");
		for (size_t x = 0; x < WINPR_JSON_GetArraySize(jbpp); x++)
		{
			const uint32_t bpp = WINPR_ASSERTING_INT_CAST(
			    uint32_t, WINPR_JSON_GetNumberValue(WINPR_JSON_GetArrayItem(jbpp, x)));
			if (!RunEncoderTest(name, format, width, height, step, bpp))
				goto fail;
			if (!RunDecoderTest(name, format, width, height, step, bpp))
				goto fail;
		}
	}

	rc = true;
fail:
	WINPR_JSON_Delete(json);
	free(path);
	free(file);
	return rc;
}

int TestFreeRDPCodecInterleaved(int argc, char* argv[])
{
	BITMAP_INTERLEAVED_CONTEXT* encoder = NULL;
	BITMAP_INTERLEAVED_CONTEXT* decoder = NULL;
	int rc = -1;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	encoder = bitmap_interleaved_context_new(true);
	decoder = bitmap_interleaved_context_new(false);

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

	if (!TestEncoder())
		goto fail;

	rc = 0;
fail:
	bitmap_interleaved_context_free(encoder);
	bitmap_interleaved_context_free(decoder);
	return rc;
}
