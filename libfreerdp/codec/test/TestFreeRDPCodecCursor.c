#include <stdint.h>

#include <winpr/path.h>
#include <winpr/image.h>
#include <freerdp/codec/color.h>

#include "testcases.h"

static BOOL run_testcase(size_t x, const gdiPalette* palette, const rdpPointer* pointer,
                         const uint8_t* ref)
{
	WINPR_ASSERT(palette);
	WINPR_ASSERT(pointer);
	WINPR_ASSERT(ref);

	WLog_INFO("test", "running cursor test case %" PRIuz, x);
	BOOL rc = FALSE;
	const uint32_t format = PIXEL_FORMAT_BGRA32;
	const size_t width = pointer->width;
	const size_t height = pointer->height;
	const size_t xorBpp = pointer->xorBpp;
	const size_t bpp = FreeRDPGetBytesPerPixel(format);
	const size_t stride = width * bpp;

	uint8_t* bmp = calloc(stride, height);
	if (!bmp)
		goto fail;

	const BOOL result = freerdp_image_copy_from_pointer_data(
	    bmp, format, 0, 0, 0, width, height, pointer->xorMaskData, pointer->lengthXorMask,
	    pointer->andMaskData, pointer->lengthAndMask, xorBpp, palette);
	if (!result)
		goto fail;

	rc = TRUE;
	for (size_t y = 0; y < height; y++)
	{
		const uint8_t* linea = &bmp[y * stride];
		const uint8_t* lineb = &ref[y * stride];
		for (size_t x = 0; x < stride; x++)
		{
			const uint8_t a = linea[x];
			const uint8_t b = lineb[x];
			if (a != b)
			{
				printf("xxx: %" PRIuz "x%" PRIuz ": color %" PRIuz " diff 0x%02" PRIx8
				       "<-->0x%02" PRIx8 "\n",
				       x / 4, y, x % 4, a, b);
				rc = FALSE;
			}
		}
	}

fail:
	free(bmp);
	WLog_INFO("test", "cursor test case %" PRIuz ": %s", x, rc ? "success" : "failure");
	return rc;
}

int TestFreeRDPCodecCursor(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	const size_t palette_len = ARRAYSIZE(testcase_palette);
	const size_t pointer_len = ARRAYSIZE(testcase_pointer);
	const size_t bmp_len = ARRAYSIZE(testcase_image_bgra32);
	WINPR_ASSERT(palette_len == pointer_len);
	WINPR_ASSERT(palette_len == bmp_len);

	int rc = 0;
	for (size_t x = 0; x < palette_len; x++)
	{
		const gdiPalette* palette = testcase_palette[x];
		const rdpPointer* pointer = testcase_pointer[x];
		const uint8_t* bmp = testcase_image_bgra32[x];

		if (!run_testcase(x, palette, pointer, bmp))
			rc = -1;
	}
	return rc;
}
