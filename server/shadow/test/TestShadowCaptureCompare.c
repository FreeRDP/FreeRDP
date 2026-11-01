#include <stdio.h>
#include <string.h>

#include <winpr/wtypes.h>
#include <freerdp/codec/color.h>
#include <freerdp/server/shadow.h>

#define WIDTH 64
#define HEIGHT 48

static BYTE surface[WIDTH * HEIGHT * 4];
static BYTE capture[WIDTH * HEIGHT * 4];

/* The same picture in any 32 bpp format. */
static BOOL fill(BYTE* data, UINT32 format, BYTE pad)
{
	for (size_t y = 0; y < HEIGHT; y++)
	{
		for (size_t x = 0; x < WIDTH; x++)
		{
			BYTE* pixel = &data[(y * WIDTH + x) * 4];
			const UINT32 color =
			    FreeRDPGetColor(format, (BYTE)(x * 4), (BYTE)(y * 5), (BYTE)(x ^ y), pad);
			if (!FreeRDPWriteColor(pixel, format, color))
				return FALSE;
		}
	}
	return TRUE;
}

static BOOL expect(const char* what, UINT32 format1, UINT32 format2, int expectedStatus,
                   const RECTANGLE_16* expectedRect)
{
	RECTANGLE_16 rect = { 0 };
	const int status = shadow_capture_compare_with_format(
	    surface, format1, WIDTH * 4, WIDTH, HEIGHT, capture, format2, WIDTH * 4, &rect);
	BOOL ok = (status == expectedStatus);
	if (ok && expectedRect)
		ok = (memcmp(&rect, expectedRect, sizeof(rect)) == 0);
	if (!ok)
		(void)fprintf(stderr,
		              "%s: %s vs %s: status %d, rect %" PRIu16 ",%" PRIu16 "-%" PRIu16 ",%" PRIu16
		              ", expected status %d\n",
		              what, FreeRDPGetColorFormatName(format1), FreeRDPGetColorFormatName(format2),
		              status, rect.left, rect.top, rect.right, rect.bottom, expectedStatus);
	return ok;
}

/* The padding byte of an X format pixel lives where the alpha of its A twin does. */
static void set_padding(BYTE* data, UINT32 alphaFormat, BYTE pad)
{
	BYTE alpha[4] = { 0 };
	(void)FreeRDPWriteColor(alpha, alphaFormat, FreeRDPGetColor(alphaFormat, 0, 0, 0, 0xFF));
	for (size_t i = 0; i < WIDTH * HEIGHT; i++)
	{
		for (size_t b = 0; b < 4; b++)
		{
			if (alpha[b] != 0)
				data[i * 4 + b] = pad;
		}
	}
}

int TestShadowCaptureCompare(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	/* surface format with padding, capture format with alpha, as the X11 backend has them */
	const UINT32 pairs[][2] = { { PIXEL_FORMAT_BGRX32, PIXEL_FORMAT_BGRA32 },
		                        { PIXEL_FORMAT_RGBX32, PIXEL_FORMAT_RGBA32 },
		                        { PIXEL_FORMAT_XRGB32, PIXEL_FORMAT_ARGB32 },
		                        { PIXEL_FORMAT_XBGR32, PIXEL_FORMAT_ABGR32 } };

	for (size_t i = 0; i < ARRAYSIZE(pairs); i++)
	{
		const UINT32 surfaceFormat = pairs[i][0];
		const UINT32 captureFormat = pairs[i][1];

		for (int pad = 0; pad < 2; pad++)
		{
			if (!fill(surface, surfaceFormat, 0xFF) || !fill(capture, captureFormat, 0x00))
				return -1;
			/* X server images leave the alpha byte 0, the surface may hold either value */
			set_padding(surface, captureFormat, pad ? 0xFF : 0x00);
			set_padding(capture, captureFormat, 0x00);

			if (!expect("identical colours", surfaceFormat, captureFormat, 0, nullptr))
				return -1;

			/* one pixel changes: only its 16x16 tile is reported */
			BYTE* pixel = &capture[(20 * WIDTH + 40) * 4];
			pixel[0] ^= 0x80;
			pixel[1] ^= 0x80;
			pixel[2] ^= 0x80;
			pixel[3] ^= 0x80;
			const RECTANGLE_16 tile = { 32, 16, 48, 32 };
			if (!expect("one pixel changed", surfaceFormat, captureFormat, 1, &tile))
				return -1;
		}
	}

	/* the same format keeps comparing every byte */
	if (!fill(surface, PIXEL_FORMAT_BGRA32, 0xFF) || !fill(capture, PIXEL_FORMAT_BGRA32, 0xFF))
		return -1;
	if (!expect("same format", PIXEL_FORMAT_BGRA32, PIXEL_FORMAT_BGRA32, 0, nullptr))
		return -1;
	capture[3] = 0x00;
	if (!expect("same format, alpha changed", PIXEL_FORMAT_BGRA32, PIXEL_FORMAT_BGRA32, 1, nullptr))
		return -1;

	return 0;
}
