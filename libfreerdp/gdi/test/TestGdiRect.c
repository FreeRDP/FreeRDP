
#include <freerdp/gdi/gdi.h>

#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/pen.h>
#include <freerdp/gdi/line.h>
#include <freerdp/gdi/shape.h>
#include <freerdp/gdi/brush.h>
#include <freerdp/gdi/region.h>
#include <freerdp/gdi/bitmap.h>
#include <freerdp/gdi/drawing.h>
#include <freerdp/gdi/palette.h>
#include <freerdp/gdi/clipping.h>
#include <freerdp/gdi/32bpp.h>

#include <winpr/crt.h>
#include <winpr/print.h>

int test_gdi_PtInRect(void)
{
	HGDI_RECT hRect;
	int left = 20;
	int top = 40;
	int right = 60;
	int bottom = 80;

	if (!(hRect = gdi_CreateRect(left, top, right, bottom)))
	{
		printf("gdi_CreateRect failed\n");
		return -1;
	}

	if (gdi_PtInRect(hRect, 0, 0))
		return -1;

	if (gdi_PtInRect(hRect, 500, 500))
		return -1;

	if (gdi_PtInRect(hRect, 40, 100))
		return -1;

	if (gdi_PtInRect(hRect, 10, 40))
		return -1;

	if (!gdi_PtInRect(hRect, 30, 50))
		return -1;

	if (!gdi_PtInRect(hRect, left, top))
		return -1;

	if (!gdi_PtInRect(hRect, right, bottom))
		return -1;

	if (!gdi_PtInRect(hRect, right, 60))
		return -1;

	if (!gdi_PtInRect(hRect, 40, bottom))
		return -1;

	return 0;
}

int test_gdi_FillRect(void)
{
	HGDI_DC hdc;
	HGDI_RECT hRect;
	HGDI_BRUSH hBrush;
	HGDI_BITMAP hBitmap;
	GDI_COLOR color;
	GDI_COLOR pixel;
	GDI_COLOR rawPixel;

	int x, y;
	int badPixels;
	int goodPixels;
	int width = 200;
	int height = 300;

	int left = 20;
	int top = 40;
	int right = 60;
	int bottom = 80;

	if (!(hdc = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}

	hdc->bytesPerPixel = 4;
	hdc->bitsPerPixel = 32;

	if (!(hRect = gdi_CreateRect(left, top, right, bottom)))
	{
		printf("gdi_CreateRect failed\n");
		return -1;
	}

	hBitmap = gdi_CreateCompatibleBitmap(hdc, width, height);
	ZeroMemory(hBitmap->data, width * height * hdc->bytesPerPixel);
	gdi_SelectObject(hdc, (HGDIOBJECT) hBitmap);

	color = (GDI_COLOR) ARGB32(0xFF, 0xAA, 0xBB, 0xCC);
	hBrush = gdi_CreateSolidBrush(color);

	gdi_FillRect(hdc, hRect, hBrush);

	badPixels = 0;
	goodPixels = 0;

	for (x = 0; x < width; x++)
	{
		for (y = 0; y < height; y++)
		{
			rawPixel = gdi_GetPixel(hdc, x, y);
			pixel = gdi_get_color_32bpp(hdc, rawPixel);

			if (gdi_PtInRect(hRect, x, y))
			{
				if (pixel == color) {
					goodPixels++;
				}
				else {
					printf("actual:%04X expected:%04X\n", gdi_GetPixel(hdc, x, y), color);
					badPixels++;
				}
			}
			else
			{
				if (pixel == color) {
					badPixels++;
				}
				else {
					goodPixels++;
				}
			}
		}
	}

	if (goodPixels != width * height)
		return -1;

	if (badPixels != 0)
		return -1;

	gdi_DeleteObject((HGDIOBJECT) hBrush);
	gdi_DeleteObject((HGDIOBJECT) hBitmap);

	return 0;
}

int TestGdiRect(int argc, char* argv[])
{
	if (test_gdi_PtInRect() < 0)
		return -1;

	if (test_gdi_FillRect() < 0)
		return -1;

	return 0;
}

