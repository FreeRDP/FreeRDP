
#include <freerdp/gdi/gdi.h>

#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/pen.h>
#include <freerdp/gdi/line.h>
#include <freerdp/gdi/brush.h>
#include <freerdp/gdi/region.h>
#include <freerdp/gdi/bitmap.h>
#include <freerdp/gdi/drawing.h>

#include <winpr/crt.h>

int test_gdi_GetDC(void)
{
	HGDI_DC hdc;

	if (!(hdc = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}

	if (hdc->bytesPerPixel != 4)
		return -1;

	if (hdc->bitsPerPixel != 32)
		return -1;

	if (hdc->drawMode != GDI_R2_BLACK)
		return -1;

	return 0;
}

int test_gdi_CreateCompatibleDC(void)
{
	HGDI_DC hdc;
	HGDI_DC chdc;

	if (!(hdc = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}

	hdc->bytesPerPixel = 2;
	hdc->bitsPerPixel = 16;
	hdc->drawMode = GDI_R2_XORPEN;

	if (!(chdc = gdi_CreateCompatibleDC(hdc)))
	{
		printf("gdi_CreateCompatibleDC failed\n");
		return -1;
	}

	if (chdc->bytesPerPixel != hdc->bytesPerPixel)
		return -1;

	if (chdc->bitsPerPixel != hdc->bitsPerPixel)
		return -1;

	if (chdc->drawMode != hdc->drawMode)
		return -1;

	return 0;
}

int test_gdi_CreateBitmap(void)
{
	int bpp;
	int width;
	int height;
	BYTE* data;
	HGDI_BITMAP hBitmap;

	bpp = 32;
	width = 32;
	height = 16;
	if (!(data = (BYTE*) _aligned_malloc(width * height * 4, 16)))
	{
		printf("failed to allocate aligned bitmap data memory\n");
		return -1;
	}

	if (!(hBitmap = gdi_CreateBitmap(width, height, bpp, data)))
	{
		printf("gdi_CreateBitmap failed\n");
		return -1;
	}

	if (hBitmap->objectType != GDIOBJECT_BITMAP)
		return -1;

	if (hBitmap->bitsPerPixel != bpp)
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

int test_gdi_CreateCompatibleBitmap(void)
{
	HGDI_DC hdc;
	int width;
	int height;
	HGDI_BITMAP hBitmap;

	if (!(hdc = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}

	hdc->bytesPerPixel = 4;
	hdc->bitsPerPixel = 32;

	width = 32;
	height = 16;
	hBitmap = gdi_CreateCompatibleBitmap(hdc, width, height);

	if (hBitmap->objectType != GDIOBJECT_BITMAP)
		return -1;

	if (hBitmap->bytesPerPixel != hdc->bytesPerPixel)
		return -1;

	if (hBitmap->bitsPerPixel != hdc->bitsPerPixel)
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

int test_gdi_CreatePen(void)
{
	HGDI_PEN hPen = gdi_CreatePen(GDI_PS_SOLID, 8, 0xAABBCCDD);

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

int test_gdi_CreateSolidBrush(void)
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

int test_gdi_CreatePatternBrush(void)
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

int test_gdi_CreateRectRgn(void)
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

int test_gdi_CreateRect(void)
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

int test_gdi_GetPixel(void)
{
	HGDI_DC hdc;
	int width = 128;
	int height = 64;
	HGDI_BITMAP hBitmap;

	if (!(hdc = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}

	hdc->bytesPerPixel = 4;
	hdc->bitsPerPixel = 32;

	hBitmap = gdi_CreateCompatibleBitmap(hdc, width, height);
	gdi_SelectObject(hdc, (HGDIOBJECT) hBitmap);

	hBitmap->data[(64 * width * 4) + 32 * 4 + 0] = 0xDD;
	hBitmap->data[(64 * width * 4) + 32 * 4 + 1] = 0xCC;
	hBitmap->data[(64 * width * 4) + 32 * 4 + 2] = 0xBB;
	hBitmap->data[(64 * width * 4) + 32 * 4 + 3] = 0xAA;

	if (gdi_GetPixel(hdc, 32, 64) != 0xAABBCCDD)
		return -1;

	gdi_DeleteObject((HGDIOBJECT) hBitmap);

	return 0;
}

int test_gdi_SetPixel(void)
{
	HGDI_DC hdc;
	int width = 128;
	int height = 64;
	HGDI_BITMAP hBitmap;

	if (!(hdc = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}

	hdc->bytesPerPixel = 4;
	hdc->bitsPerPixel = 32;

	hBitmap = gdi_CreateCompatibleBitmap(hdc, width, height);
	gdi_SelectObject(hdc, (HGDIOBJECT) hBitmap);

	gdi_SetPixel(hdc, 32, 64, 0xAABBCCDD);

	if (gdi_GetPixel(hdc, 32, 64) != 0xAABBCCDD)
		return -1;

	gdi_SetPixel(hdc, width - 1, height - 1, 0xAABBCCDD);

	if (gdi_GetPixel(hdc, width - 1, height - 1) != 0xAABBCCDD)
		return -1;

	gdi_DeleteObject((HGDIOBJECT) hBitmap);

	return 0;
}

int test_gdi_SetROP2(void)
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

int test_gdi_MoveToEx(void)
{
	HGDI_DC hdc;
	HGDI_PEN hPen;
	HGDI_POINT prevPoint;

	if (!(hdc = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}

	if (!(hPen = gdi_CreatePen(GDI_PS_SOLID, 8, 0xAABBCCDD)))
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

	if (test_gdi_GetPixel() < 0)
		return -1;

	fprintf(stderr, "test_gdi_SetPixel()\n");

	if (test_gdi_SetPixel() < 0)
		return -1;

	fprintf(stderr, "test_gdi_SetROP2()\n");

	if (test_gdi_SetROP2() < 0)
		return -1;

	fprintf(stderr, "test_gdi_MoveToEx()\n");

	if (test_gdi_MoveToEx() < 0)
		return -1;

	return 0;
}

