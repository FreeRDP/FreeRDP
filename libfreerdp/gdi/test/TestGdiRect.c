
#include <freerdp/gdi/gdi.h>

#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/pen.h>
#include <freerdp/gdi/shape.h>
#include <freerdp/gdi/region.h>
#include <freerdp/gdi/bitmap.h>

#include <winpr/crt.h>
#include <winpr/print.h>

#include "line.h"
#include "brush.h"
#include "clipping.h"

static int test_gdi_PtInRect(void)
{
	int rc = -1;
	HGDI_RECT hRect;
	UINT32 left = 20;
	UINT32 top = 40;
	UINT32 right = 60;
	UINT32 bottom = 80;

	if (!(hRect = gdi_CreateRect(left, top, right, bottom)))
	{
		printf("gdi_CreateRect failed\n");
		return rc;
	}

	if (gdi_PtInRect(hRect, 0, 0))
		goto fail;

	if (gdi_PtInRect(hRect, 500, 500))
		goto fail;

	if (gdi_PtInRect(hRect, 40, 100))
		goto fail;

	if (gdi_PtInRect(hRect, 10, 40))
		goto fail;

	if (!gdi_PtInRect(hRect, 30, 50))
		goto fail;

	if (!gdi_PtInRect(hRect, left, top))
		goto fail;

	if (!gdi_PtInRect(hRect, right, bottom))
		goto fail;

	if (!gdi_PtInRect(hRect, right, 60))
		goto fail;

	if (!gdi_PtInRect(hRect, 40, bottom))
		goto fail;

	rc = 0;
fail:
	gdi_DeleteObject((HGDIOBJECT)hRect);
	return rc;
}

int test_gdi_FillRect(void)
{
	int rc = -1;
	HGDI_DC hdc = NULL;
	HGDI_RECT hRect = NULL;
	HGDI_BRUSH hBrush = NULL;
	HGDI_BITMAP hBitmap = NULL;
	UINT32 color;
	UINT32 pixel;
	UINT32 rawPixel;
	UINT32 x, y;
	UINT32 badPixels;
	UINT32 goodPixels;
	UINT32 width = 200;
	UINT32 height = 300;
	UINT32 left = 20;
	UINT32 top = 40;
	UINT32 right = 60;
	UINT32 bottom = 80;

	if (!(hdc = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		goto fail;
	}

	hdc->format = PIXEL_FORMAT_XRGB32;

	if (!(hRect = gdi_CreateRect(left, top, right, bottom)))
	{
		printf("gdi_CreateRect failed\n");
		goto fail;
	}

	hBitmap = gdi_CreateCompatibleBitmap(hdc, width, height);
	ZeroMemory(hBitmap->data, width * height * GetBytesPerPixel(hdc->format));
	gdi_SelectObject(hdc, (HGDIOBJECT) hBitmap);
	color = FreeRDPGetColor(PIXEL_FORMAT_ARGB32, 0xAA, 0xBB, 0xCC, 0xFF);
	hBrush = gdi_CreateSolidBrush(color);
	gdi_FillRect(hdc, hRect, hBrush);
	badPixels = 0;
	goodPixels = 0;

	for (x = 0; x < width; x++)
	{
		for (y = 0; y < height; y++)
		{
			rawPixel = gdi_GetPixel(hdc, x, y);
			pixel = FreeRDPConvertColor(rawPixel, hdc->format, PIXEL_FORMAT_ARGB32, NULL);

			if (gdi_PtInRect(hRect, x, y))
			{
				if (pixel == color)
				{
					goodPixels++;
				}
				else
				{
					printf("actual:%08"PRIX32" expected:%08"PRIX32"\n", gdi_GetPixel(hdc, x, y), color);
					badPixels++;
				}
			}
			else
			{
				if (pixel == color)
				{
					badPixels++;
				}
				else
				{
					goodPixels++;
				}
			}
		}
	}

	if (goodPixels != width * height)
		goto fail;

	if (badPixels != 0)
		goto fail;

	rc = 0;
fail:
	gdi_DeleteObject((HGDIOBJECT) hBrush);
	gdi_DeleteObject((HGDIOBJECT) hBitmap);
	gdi_DeleteObject((HGDIOBJECT)hRect);
	gdi_DeleteDC(hdc);
	return rc;
}

int TestGdiRect(int argc, char* argv[])
{
	if (test_gdi_PtInRect() < 0)
		return -1;

	if (test_gdi_FillRect() < 0)
		return -1;

	return 0;
}

