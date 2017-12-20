
#include <freerdp/gdi/gdi.h>

#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/pen.h>
#include <freerdp/gdi/region.h>
#include <freerdp/gdi/bitmap.h>

#include <winpr/crt.h>

#include "line.h"
#include "brush.h"
#include "clipping.h"

static int test_gdi_ClipCoords(void)
{
	int rc = -1;
	BOOL draw;
	HGDI_DC hdc;
	HGDI_RGN rgn1 = NULL;
	HGDI_RGN rgn2 = NULL;
	HGDI_BITMAP bmp = NULL;
	const UINT32 format = PIXEL_FORMAT_ARGB32;

	if (!(hdc = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}

	hdc->format = format;
	bmp = gdi_CreateBitmapEx(1024, 768, PIXEL_FORMAT_XRGB32, 0, NULL, NULL);
	gdi_SelectObject(hdc, (HGDIOBJECT) bmp);
	gdi_SetNullClipRgn(hdc);
	rgn1 = gdi_CreateRectRgn(0, 0, 0, 0);
	rgn2 = gdi_CreateRectRgn(0, 0, 0, 0);
	rgn1->null = TRUE;
	rgn2->null = TRUE;
	/* null clipping region */
	gdi_SetNullClipRgn(hdc);
	gdi_SetRgn(rgn1, 20, 20, 100, 100);
	gdi_SetRgn(rgn2, 20, 20, 100, 100);
	gdi_ClipCoords(hdc, &(rgn1->x), &(rgn1->y), &(rgn1->w), &(rgn1->h), NULL, NULL);

	if (!gdi_EqualRgn(rgn1, rgn2))
		goto fail;

	/* region all inside clipping region */
	gdi_SetClipRgn(hdc, 0, 0, 1024, 768);
	gdi_SetRgn(rgn1, 20, 20, 100, 100);
	gdi_SetRgn(rgn2, 20, 20, 100, 100);
	gdi_ClipCoords(hdc, &(rgn1->x), &(rgn1->y), &(rgn1->w), &(rgn1->h), NULL, NULL);

	if (!gdi_EqualRgn(rgn1, rgn2))
		goto fail;

	/* region all outside clipping region, on the left */
	gdi_SetClipRgn(hdc, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 20, 20, 100, 100);
	gdi_SetRgn(rgn2, 0, 0, 0, 0);
	draw = gdi_ClipCoords(hdc, &(rgn1->x), &(rgn1->y), &(rgn1->w), &(rgn1->h), NULL,
	                      NULL);

	if (draw)
		goto fail;

	/* region all outside clipping region, on the right */
	gdi_SetClipRgn(hdc, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 420, 420, 100, 100);
	gdi_SetRgn(rgn2, 0, 0, 0, 0);
	draw = gdi_ClipCoords(hdc, &(rgn1->x), &(rgn1->y), &(rgn1->w), &(rgn1->h), NULL,
	                      NULL);

	if (draw)
		goto fail;

	/* region all outside clipping region, on top */
	gdi_SetClipRgn(hdc, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 300, 20, 100, 100);
	gdi_SetRgn(rgn2, 0, 0, 0, 0);
	draw = gdi_ClipCoords(hdc, &(rgn1->x), &(rgn1->y), &(rgn1->w), &(rgn1->h), NULL,
	                      NULL);

	if (draw)
		goto fail;

	/* region all outside clipping region, at the bottom */
	gdi_SetClipRgn(hdc, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 300, 420, 100, 100);
	gdi_SetRgn(rgn2, 0, 0, 0, 0);
	draw = gdi_ClipCoords(hdc, &(rgn1->x), &(rgn1->y), &(rgn1->w), &(rgn1->h), NULL,
	                      NULL);

	if (draw)
		goto fail;

	/* left outside, right = clip, top = clip, bottom = clip */
	gdi_SetClipRgn(hdc, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 100, 300, 300, 100);
	gdi_SetRgn(rgn2, 300, 300, 100, 100);
	gdi_ClipCoords(hdc, &(rgn1->x), &(rgn1->y), &(rgn1->w), &(rgn1->h), NULL, NULL);

	if (!gdi_EqualRgn(rgn1, rgn2))
		goto fail;

	/* left outside, right inside, top = clip, bottom = clip */
	gdi_SetClipRgn(hdc, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 100, 300, 250, 100);
	gdi_SetRgn(rgn2, 300, 300, 50, 100);
	gdi_ClipCoords(hdc, &(rgn1->x), &(rgn1->y), &(rgn1->w), &(rgn1->h), NULL, NULL);

	if (!gdi_EqualRgn(rgn1, rgn2))
		goto fail;

	/* left = clip, right outside, top = clip, bottom = clip */
	gdi_SetClipRgn(hdc, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 300, 300, 300, 100);
	gdi_SetRgn(rgn2, 300, 300, 100, 100);
	gdi_ClipCoords(hdc, &(rgn1->x), &(rgn1->y), &(rgn1->w), &(rgn1->h), NULL, NULL);

	if (!gdi_EqualRgn(rgn1, rgn2))
		goto fail;

	/* left inside, right outside, top = clip, bottom = clip */
	gdi_SetClipRgn(hdc, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 350, 300, 200, 100);
	gdi_SetRgn(rgn2, 350, 300, 50, 100);
	gdi_ClipCoords(hdc, &(rgn1->x), &(rgn1->y), &(rgn1->w), &(rgn1->h), NULL, NULL);

	if (!gdi_EqualRgn(rgn1, rgn2))
		goto fail;

	/* top outside, bottom = clip, left = clip, right = clip */
	gdi_SetClipRgn(hdc, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 300, 100, 300, 300);
	gdi_SetRgn(rgn2, 300, 300, 100, 100);
	gdi_ClipCoords(hdc, &(rgn1->x), &(rgn1->y), &(rgn1->w), &(rgn1->h), NULL, NULL);

	if (!gdi_EqualRgn(rgn1, rgn2))
		goto fail;

	/* top = clip, bottom outside, left = clip, right = clip */
	gdi_SetClipRgn(hdc, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 300, 300, 100, 200);
	gdi_SetRgn(rgn2, 300, 300, 100, 100);
	gdi_ClipCoords(hdc, &(rgn1->x), &(rgn1->y), &(rgn1->w), &(rgn1->h), NULL, NULL);

	if (!gdi_EqualRgn(rgn1, rgn2))
		goto fail;

	/* top = clip, bottom = clip, top = clip, bottom = clip */
	gdi_SetClipRgn(hdc, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 300, 300, 100, 100);
	gdi_SetRgn(rgn2, 300, 300, 100, 100);
	gdi_ClipCoords(hdc, &(rgn1->x), &(rgn1->y), &(rgn1->w), &(rgn1->h), NULL, NULL);

	if (!gdi_EqualRgn(rgn1, rgn2))
		goto fail;

	rc = 0;
fail:

	gdi_DeleteObject((HGDIOBJECT)rgn1);
	gdi_DeleteObject((HGDIOBJECT)rgn2);
	gdi_DeleteObject((HGDIOBJECT)bmp);

	gdi_DeleteDC(hdc);
	return rc;
}

static int test_gdi_InvalidateRegion(void)
{
	int rc = -1;
	HGDI_DC hdc;
	HGDI_RGN rgn1 = NULL;
	HGDI_RGN rgn2 = NULL;
	HGDI_RGN invalid = NULL;
	HGDI_BITMAP bmp = NULL;
	const UINT32 format = PIXEL_FORMAT_XRGB32;

	if (!(hdc = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}

	hdc->format = format;
	bmp = gdi_CreateBitmapEx(1024, 768, PIXEL_FORMAT_XRGB32, 0, NULL, NULL);
	gdi_SelectObject(hdc, (HGDIOBJECT) bmp);
	gdi_SetNullClipRgn(hdc);
	hdc->hwnd = (HGDI_WND) calloc(1, sizeof(GDI_WND));
	hdc->hwnd->invalid = gdi_CreateRectRgn(0, 0, 0, 0);
	hdc->hwnd->invalid->null = TRUE;
	invalid = hdc->hwnd->invalid;
	hdc->hwnd->count = 16;
	hdc->hwnd->cinvalid = (HGDI_RGN) calloc(hdc->hwnd->count, sizeof(GDI_RGN));
	rgn1 = gdi_CreateRectRgn(0, 0, 0, 0);
	rgn2 = gdi_CreateRectRgn(0, 0, 0, 0);
	rgn1->null = TRUE;
	rgn2->null = TRUE;
	/* no previous invalid region */
	invalid->null = TRUE;
	gdi_SetRgn(rgn1, 300, 300, 100, 100);
	gdi_SetRgn(rgn2, 300, 300, 100, 100);
	gdi_InvalidateRegion(hdc, rgn1->x, rgn1->y, rgn1->w, rgn1->h);

	if (!gdi_EqualRgn(invalid, rgn2))
		goto fail;

	/* region same as invalid region */
	gdi_SetRgn(invalid, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 300, 300, 100, 100);
	gdi_SetRgn(rgn2, 300, 300, 100, 100);
	gdi_InvalidateRegion(hdc, rgn1->x, rgn1->y, rgn1->w, rgn1->h);

	if (!gdi_EqualRgn(invalid, rgn2))
		goto fail;

	/* left outside */
	gdi_SetRgn(invalid, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 100, 300, 300, 100);
	gdi_SetRgn(rgn2, 100, 300, 300, 100);
	gdi_InvalidateRegion(hdc, rgn1->x, rgn1->y, rgn1->w, rgn1->h);

	if (!gdi_EqualRgn(invalid, rgn2))
		goto fail;

	/* right outside */
	gdi_SetRgn(invalid, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 300, 300, 300, 100);
	gdi_SetRgn(rgn2, 300, 300, 300, 100);
	gdi_InvalidateRegion(hdc, rgn1->x, rgn1->y, rgn1->w, rgn1->h);

	if (!gdi_EqualRgn(invalid, rgn2))
		goto fail;

	/* top outside */
	gdi_SetRgn(invalid, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 300, 100, 100, 300);
	gdi_SetRgn(rgn2, 300, 100, 100, 300);
	gdi_InvalidateRegion(hdc, rgn1->x, rgn1->y, rgn1->w, rgn1->h);

	if (!gdi_EqualRgn(invalid, rgn2))
		goto fail;

	/* bottom outside */
	gdi_SetRgn(invalid, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 300, 300, 100, 300);
	gdi_SetRgn(rgn2, 300, 300, 100, 300);
	gdi_InvalidateRegion(hdc, rgn1->x, rgn1->y, rgn1->w, rgn1->h);

	if (!gdi_EqualRgn(invalid, rgn2))
		goto fail;

	/* left outside, right outside */
	gdi_SetRgn(invalid, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 100, 300, 600, 300);
	gdi_SetRgn(rgn2, 100, 300, 600, 300);
	gdi_InvalidateRegion(hdc, rgn1->x, rgn1->y, rgn1->w, rgn1->h);

	if (!gdi_EqualRgn(invalid, rgn2))
		goto fail;

	/* top outside, bottom outside */
	gdi_SetRgn(invalid, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 300, 100, 100, 500);
	gdi_SetRgn(rgn2, 300, 100, 100, 500);
	gdi_InvalidateRegion(hdc, rgn1->x, rgn1->y, rgn1->w, rgn1->h);

	if (!gdi_EqualRgn(invalid, rgn2))
		goto fail;

	/* all outside, left */
	gdi_SetRgn(invalid, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 100, 300, 100, 100);
	gdi_SetRgn(rgn2, 100, 300, 300, 100);
	gdi_InvalidateRegion(hdc, rgn1->x, rgn1->y, rgn1->w, rgn1->h);

	if (!gdi_EqualRgn(invalid, rgn2))
		goto fail;

	/* all outside, right */
	gdi_SetRgn(invalid, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 700, 300, 100, 100);
	gdi_SetRgn(rgn2, 300, 300, 500, 100);
	gdi_InvalidateRegion(hdc, rgn1->x, rgn1->y, rgn1->w, rgn1->h);

	if (!gdi_EqualRgn(invalid, rgn2))
		goto fail;

	/* all outside, top */
	gdi_SetRgn(invalid, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 300, 100, 100, 100);
	gdi_SetRgn(rgn2, 300, 100, 100, 300);
	gdi_InvalidateRegion(hdc, rgn1->x, rgn1->y, rgn1->w, rgn1->h);

	if (!gdi_EqualRgn(invalid, rgn2))
		goto fail;

	/* all outside, bottom */
	gdi_SetRgn(invalid, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 300, 500, 100, 100);
	gdi_SetRgn(rgn2, 300, 300, 100, 300);
	gdi_InvalidateRegion(hdc, rgn1->x, rgn1->y, rgn1->w, rgn1->h);

	if (!gdi_EqualRgn(invalid, rgn2))
		goto fail;

	/* all outside */
	gdi_SetRgn(invalid, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 100, 100, 600, 600);
	gdi_SetRgn(rgn2, 100, 100, 600, 600);
	gdi_InvalidateRegion(hdc, rgn1->x, rgn1->y, rgn1->w, rgn1->h);

	if (!gdi_EqualRgn(invalid, rgn2))
		goto fail;

	/* everything */
	gdi_SetRgn(invalid, 300, 300, 100, 100);
	gdi_SetRgn(rgn1, 0, 0, 1024, 768);
	gdi_SetRgn(rgn2, 0, 0, 1024, 768);
	gdi_InvalidateRegion(hdc, rgn1->x, rgn1->y, rgn1->w, rgn1->h);

	if (!gdi_EqualRgn(invalid, rgn2))
		goto fail;

	rc = 0;
fail:

	gdi_DeleteObject((HGDIOBJECT)rgn1);
	gdi_DeleteObject((HGDIOBJECT)rgn2);
	gdi_DeleteObject((HGDIOBJECT)bmp);

	gdi_DeleteDC(hdc);
	return 0;
}

int TestGdiClip(int argc, char* argv[])
{
	fprintf(stderr, "test_gdi_ClipCoords()\n");

	if (test_gdi_ClipCoords() < 0)
		return -1;

	fprintf(stderr, "test_gdi_InvalidateRegion()\n");

	if (test_gdi_InvalidateRegion() < 0)
		return -1;

	return 0;
}

