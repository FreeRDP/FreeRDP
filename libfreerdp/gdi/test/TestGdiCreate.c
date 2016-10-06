
#include <freerdp/gdi/gdi.h>

#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/pen.h>
#include <freerdp/gdi/region.h>
#include <freerdp/gdi/bitmap.h>
#include <freerdp/gdi/gdi.h>

#include <winpr/crt.h>

#include "line.h"
#include "brush.h"
#include "drawing.h"

static const UINT32 colorFormatList[] =
{
	PIXEL_FORMAT_RGB15,
	PIXEL_FORMAT_BGR15,
	PIXEL_FORMAT_RGB16,
	PIXEL_FORMAT_BGR16,
	PIXEL_FORMAT_RGB24,
	PIXEL_FORMAT_BGR24,
	PIXEL_FORMAT_ARGB32,
	PIXEL_FORMAT_ABGR32,
	PIXEL_FORMAT_XRGB32,
	PIXEL_FORMAT_XBGR32,
	PIXEL_FORMAT_RGBX32,
	PIXEL_FORMAT_BGRX32

};
static const UINT32 colorFormatCount = sizeof(colorFormatList) / sizeof(
        colorFormatList[0]);

static int test_gdi_GetDC(void)
{
	HGDI_DC hdc;

	if (!(hdc = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}

	if (hdc->format != PIXEL_FORMAT_XRGB32)
		return -1;

	if (hdc->drawMode != GDI_R2_BLACK)
		return -1;

	return 0;
}

static int test_gdi_CreateCompatibleDC(void)
{
	HGDI_DC hdc;
	HGDI_DC chdc;

	if (!(hdc = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}

	hdc->format = PIXEL_FORMAT_RGB16;
	hdc->drawMode = GDI_R2_XORPEN;

	if (!(chdc = gdi_CreateCompatibleDC(hdc)))
	{
		printf("gdi_CreateCompatibleDC failed\n");
		return -1;
	}

	if (chdc->format != hdc->format)
		return -1;

	if (chdc->drawMode != hdc->drawMode)
		return -1;

	return 0;
}

static int test_gdi_CreateBitmap(void)
{
	UINT32 format = PIXEL_FORMAT_ARGB32;
	UINT32 width;
	UINT32 height;
	BYTE* data;
	HGDI_BITMAP hBitmap;
	width = 32;
	height = 16;

	if (!(data = (BYTE*) _aligned_malloc(width * height * 4, 16)))
	{
		printf("failed to allocate aligned bitmap data memory\n");
		return -1;
	}

	if (!(hBitmap = gdi_CreateBitmap(width, height, format, data)))
	{
		printf("gdi_CreateBitmap failed\n");
		return -1;
	}

	if (hBitmap->objectType != GDIOBJECT_BITMAP)
		return -1;

	if (hBitmap->format != format)
		return -1;

	if (hBitmap->width != width)
		return -1;

	if (hBitmap->height != height)
		return -1;

	if (hBitmap->data != data)
		return -1;

	gdi_DeleteObject((HGDIOBJECT) hBitmap);
	return 0;
}

static int test_gdi_CreateCompatibleBitmap(void)
{
	HGDI_DC hdc;
	UINT32 width;
	UINT32 height;
	HGDI_BITMAP hBitmap;

	if (!(hdc = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}

	hdc->format = PIXEL_FORMAT_ARGB32;
	width = 32;
	height = 16;
	hBitmap = gdi_CreateCompatibleBitmap(hdc, width, height);

	if (hBitmap->objectType != GDIOBJECT_BITMAP)
		return -1;

	if (hBitmap->format != hdc->format)
		return -1;

	if (hBitmap->width != width)
		return -1;

	if (hBitmap->height != height)
		return -1;

	if (!hBitmap->data)
		return -1;

	gdi_DeleteObject((HGDIOBJECT) hBitmap);
	return 0;
}

static int test_gdi_CreatePen(void)
{
	const UINT32 format = PIXEL_FORMAT_RGBA32;
	HGDI_PEN hPen = gdi_CreatePen(GDI_PS_SOLID, 8, 0xAABBCCDD,
	                              format, NULL);

	if (!hPen)
	{
		printf("gdi_CreatePen failed\n");
		return -1;
	}

	if (hPen->style != GDI_PS_SOLID)
		return -1;

	if (hPen->width != 8)
		return -1;

	if (hPen->color != 0xAABBCCDD)
		return -1;

	gdi_DeleteObject((HGDIOBJECT) hPen);
	return 0;
}

static int test_gdi_CreateSolidBrush(void)
{
	HGDI_BRUSH hBrush = gdi_CreateSolidBrush(0xAABBCCDD);

	if (hBrush->objectType != GDIOBJECT_BRUSH)
		return -1;

	if (hBrush->style != GDI_BS_SOLID)
		return -1;

	if (hBrush->color != 0xAABBCCDD)
		return -1;

	gdi_DeleteObject((HGDIOBJECT) hBrush);
	return 0;
}

static int test_gdi_CreatePatternBrush(void)
{
	HGDI_BRUSH hBrush;
	HGDI_BITMAP hBitmap;
	hBitmap = gdi_CreateBitmap(64, 64, 32, NULL);
	hBrush = gdi_CreatePatternBrush(hBitmap);

	if (hBrush->objectType != GDIOBJECT_BRUSH)
		return -1;

	if (hBrush->style != GDI_BS_PATTERN)
		return -1;

	if (hBrush->pattern != hBitmap)
		return -1;

	gdi_DeleteObject((HGDIOBJECT) hBitmap);
	return 0;
}

static int test_gdi_CreateRectRgn(void)
{
	int x1 = 32;
	int y1 = 64;
	int x2 = 128;
	int y2 = 256;
	HGDI_RGN hRegion = gdi_CreateRectRgn(x1, y1, x2, y2);

	if (hRegion->objectType != GDIOBJECT_REGION)
		return -1;

	if (hRegion->x != x1)
		return -1;

	if (hRegion->y != y1)
		return -1;

	if (hRegion->w != x2 - x1 + 1)
		return -1;

	if (hRegion->h != y2 - y1 + 1)
		return -1;

	if (hRegion->null)
		return -1;

	gdi_DeleteObject((HGDIOBJECT) hRegion);
	return 0;
}

static int test_gdi_CreateRect(void)
{
	HGDI_RECT hRect;
	int x1 = 32;
	int y1 = 64;
	int x2 = 128;
	int y2 = 256;

	if (!(hRect = gdi_CreateRect(x1, y1, x2, y2)))
	{
		printf("gdi_CreateRect failed\n");
		return -1;
	}

	if (hRect->objectType != GDIOBJECT_RECT)
		return -1;

	if (hRect->left != x1)
		return -1;

	if (hRect->top != y1)
		return -1;

	if (hRect->right != x2)
		return -1;

	if (hRect->bottom != y2)
		return -1;

	gdi_DeleteObject((HGDIOBJECT) hRect);
	return 0;
}

static BOOL test_gdi_GetPixel(void)
{
	BOOL rc = TRUE;
	UINT32 x;

	for (x = 0; x < colorFormatCount; x++)
	{
		UINT32 i, j;
		UINT32 bpp;
		HGDI_DC hdc;
		UINT32 width = 128;
		UINT32 height = 64;
		HGDI_BITMAP hBitmap;

		if (!(hdc = gdi_GetDC()))
		{
			printf("failed to get gdi device context\n");
			return -1;
		}

		hdc->format = colorFormatList[x];
		hBitmap = gdi_CreateCompatibleBitmap(hdc, width, height);
		gdi_SelectObject(hdc, (HGDIOBJECT) hBitmap);
		bpp = GetBytesPerPixel(hBitmap->format);

		for (i = 0; i < height; i++)
		{
			for (j = 0; j < width; j++)
			{
				UINT32 pixel;
				const UINT32 color = GetColor(hBitmap->format, rand(), rand(), rand(), rand());
				WriteColor(&hBitmap->data[i * hBitmap->scanline + j * bpp], hBitmap->format,
				           color);
				pixel = gdi_GetPixel(hdc, j, i);

				if (pixel != color)
				{
					rc = FALSE;
					break;
				}
			}

			if (!rc)
				break;
		}

		gdi_DeleteObject((HGDIOBJECT) hBitmap);
		gdi_DeleteDC(hdc);
	}

	return rc;
}

static BOOL test_gdi_SetPixel(void)
{
	BOOL rc = TRUE;
	UINT32 x;

	for (x = 0; x < colorFormatCount; x++)
	{
		UINT32 i, j, bpp;
		HGDI_DC hdc;
		UINT32 width = 128;
		UINT32 height = 64;
		HGDI_BITMAP hBitmap;

		if (!(hdc = gdi_GetDC()))
		{
			printf("failed to get gdi device context\n");
			return FALSE;
		}

		hdc->format = colorFormatList[x];
		hBitmap = gdi_CreateCompatibleBitmap(hdc, width, height);
		gdi_SelectObject(hdc, (HGDIOBJECT) hBitmap);
		bpp = GetBytesPerPixel(hBitmap->format);

		for (i = 0; i < height; i++)
		{
			for (j = 0; j < width; j++)
			{
				UINT32 pixel;
				const UINT32 color = GetColor(hBitmap->format, rand(), rand(), rand(), rand());
				gdi_SetPixel(hdc, j, i, color);
				pixel = ReadColor(&hBitmap->data[i * hBitmap->scanline + j * bpp],
				                  hBitmap->format);

				if (pixel != color)
				{
					rc = FALSE;
					break;
				}
			}

			if (!rc)
				break;
		}

		gdi_DeleteObject((HGDIOBJECT) hBitmap);
		gdi_DeleteDC(hdc);
	}

	return rc;
}

static int test_gdi_SetROP2(void)
{
	HGDI_DC hdc;

	if (!(hdc = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}

	gdi_SetROP2(hdc, GDI_R2_BLACK);

	if (hdc->drawMode != GDI_R2_BLACK)
		return -1;

	return 0;
}

static int test_gdi_MoveToEx(void)
{
	HGDI_DC hdc;
	HGDI_PEN hPen;
	HGDI_POINT prevPoint;
	const UINT32 format = PIXEL_FORMAT_RGBA32;
	gdiPalette* palette = NULL;

	if (!(hdc = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}

	if (!(hPen = gdi_CreatePen(GDI_PS_SOLID, 8, 0xAABBCCDD, format, palette)))
	{
		printf("gdi_CreatePen failed\n");
		return -1;
	}

	gdi_SelectObject(hdc, (HGDIOBJECT) hPen);
	gdi_MoveToEx(hdc, 128, 256, NULL);

	if (hdc->pen->posX != 128)
		return -1;

	if (hdc->pen->posY != 256)
		return -1;

	prevPoint = (HGDI_POINT) malloc(sizeof(GDI_POINT));
	ZeroMemory(prevPoint, sizeof(GDI_POINT));
	gdi_MoveToEx(hdc, 64, 128, prevPoint);

	if (prevPoint->x != 128)
		return -1;

	if (prevPoint->y != 256)
		return -1;

	if (hdc->pen->posX != 64)
		return -1;

	if (hdc->pen->posY != 128)
		return -1;

	return 0;
}

int TestGdiCreate(int argc, char* argv[])
{
	fprintf(stderr, "test_gdi_GetDC()\n");

	if (test_gdi_GetDC() < 0)
		return -1;

	fprintf(stderr, "test_gdi_CreateCompatibleDC()\n");

	if (test_gdi_CreateCompatibleDC() < 0)
		return -1;

	fprintf(stderr, "test_gdi_CreateBitmap()\n");

	if (test_gdi_CreateBitmap() < 0)
		return -1;

	fprintf(stderr, "test_gdi_CreateCompatibleBitmap()\n");

	if (test_gdi_CreateCompatibleBitmap() < 0)
		return -1;

	fprintf(stderr, "test_gdi_CreatePen()\n");

	if (test_gdi_CreatePen() < 0)
		return -1;

	fprintf(stderr, "test_gdi_CreateSolidBrush()\n");

	if (test_gdi_CreateSolidBrush() < 0)
		return -1;

	fprintf(stderr, "test_gdi_CreatePatternBrush()\n");

	if (test_gdi_CreatePatternBrush() < 0)
		return -1;

	fprintf(stderr, "test_gdi_CreateRectRgn()\n");

	if (test_gdi_CreateRectRgn() < 0)
		return -1;

	fprintf(stderr, "test_gdi_CreateRect()\n");

	if (test_gdi_CreateRect() < 0)
		return -1;

	fprintf(stderr, "test_gdi_GetPixel()\n");

	if (!test_gdi_GetPixel())
		return -1;

	fprintf(stderr, "test_gdi_SetPixel()\n");

	if (!test_gdi_SetPixel())
		return -1;

	fprintf(stderr, "test_gdi_SetROP2()\n");

	if (test_gdi_SetROP2() < 0)
		return -1;

	fprintf(stderr, "test_gdi_MoveToEx()\n");

	if (test_gdi_MoveToEx() < 0)
		return -1;

	return 0;
}

