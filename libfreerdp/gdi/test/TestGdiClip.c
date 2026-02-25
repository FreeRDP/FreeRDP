
#include <freerdp/gdi/gdi.h>

#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/pen.h>
#include <freerdp/gdi/region.h>
#include <freerdp/gdi/bitmap.h>

#include <winpr/crt.h>

#include "line.h"
#include "brush.h"
#include "clipping.h"

static bool test_gdi_coords(size_t testNr, HGDI_DC hdc, const GDI_RGN* clip, const GDI_RGN* input,
                            const GDI_RGN* expect)
{
	bool rc = false;

	WINPR_ASSERT(hdc);
	WINPR_ASSERT(input);

	HGDI_RGN rgn1 = gdi_CreateRectRgn(0, 0, 0, 0);
	HGDI_RGN rgn2 = gdi_CreateRectRgn(0, 0, 0, 0);
	if (!rgn1 || !rgn2)
	{
		(void)fprintf(stderr, "[%s:%" PRIuz "] gdi_CreateRectRgn failed: rgn1=%p, rgn2=%p\n",
		              __func__, testNr, rgn1, rgn2);
		goto fail;
	}

	if (!clip)
	{
		if (!gdi_SetNullClipRgn(hdc))
		{
			(void)fprintf(stderr, "[%s:%" PRIuz "] gdi_SetNullClipRgn failed\n", __func__, testNr);
			goto fail;
		}
	}
	else if (!gdi_SetClipRgn(hdc, clip->x, clip->y, clip->w, clip->h))
	{
		(void)fprintf(stderr, "[%s:%" PRIuz "] gdi_SetClipRgn failed\n", __func__, testNr);
		goto fail;
	}

	if (!gdi_SetRgn(rgn1, input->x, input->y, input->w, input->h))
	{
		(void)fprintf(stderr, "[%s:%" PRIuz "] gdi_SetRgn (rgn1) failed\n", __func__, testNr);
		goto fail;
	}

	if (expect)
	{
		if (!gdi_SetRgn(rgn2, expect->x, expect->y, expect->w, expect->h))
		{
			(void)fprintf(stderr, "[%s:%" PRIuz "] gdi_SetRgn (rgn2) failed\n", __func__, testNr);
			goto fail;
		}
	}

	const BOOL draw =
	    gdi_ClipCoords(hdc, &(rgn1->x), &(rgn1->y), &(rgn1->w), &(rgn1->h), NULL, NULL);
	if (expect)
	{
		if (!draw)
		{
			(void)fprintf(stderr,
			              "[%s:%" PRIuz "] gdi_ClipCoords failed: expected TRUE, got FALSE\n",
			              __func__, testNr);
			goto fail;
		}

		if (!gdi_EqualRgn(rgn1, rgn2))
		{
			char buffer1[64] = WINPR_C_ARRAY_INIT;
			char buffer2[64] = WINPR_C_ARRAY_INIT;
			(void)fprintf(stderr, "[%s:%" PRIuz "] gdi_EqualRgn failed: expected %s, got %s\n",
			              __func__, testNr, buffer1, buffer2);
			goto fail;
		}
	}
	else
	{
		if (draw)
		{
			(void)fprintf(stderr,
			              "[%s:%" PRIuz "] gdi_ClipCoords failed: expected FALSE, got TRUE\n",
			              __func__, testNr);
			goto fail;
		}
	}

	rc = true;

fail:
	gdi_DeleteObject((HGDIOBJECT)rgn1);
	gdi_DeleteObject((HGDIOBJECT)rgn2);
	return rc;
}

static int test_gdi_ClipCoords(void)
{
	int rc = -1;
	HGDI_DC hdc = NULL;
	HGDI_BITMAP bmp = NULL;
	const UINT32 format = PIXEL_FORMAT_ARGB32;

	if (!(hdc = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}

	hdc->format = format;
	bmp = gdi_CreateBitmapEx(1024, 768, PIXEL_FORMAT_XRGB32, 0, NULL, NULL);
	gdi_SelectObject(hdc, (HGDIOBJECT)bmp);
	gdi_SetNullClipRgn(hdc);

	struct testcase_t
	{
		const GDI_RGN* clip;
		const GDI_RGN* in;
		const GDI_RGN* expect;
	};

	const GDI_RGN rgn0x0_1024x768 = { 0, 0, 0, 1024, 768, FALSE };
	const GDI_RGN rgn20x20_100x100 = { 0, 20, 20, 100, 100, FALSE };
	const GDI_RGN rgn100x300_250x100 = { 0, 100, 300, 250, 100, FALSE };
	const GDI_RGN rgn100x300_300x100 = { 0, 100, 300, 300, 100, FALSE };
	const GDI_RGN rgn300x20_100x100 = { 0, 300, 20, 100, 100, FALSE };
	const GDI_RGN rgn300x100_300x300 = { 0, 300, 100, 300, 300, FALSE };
	const GDI_RGN rgn300x300_50x100 = { 0, 300, 300, 50, 100, FALSE };
	const GDI_RGN rgn300x300_100x100 = { 0, 300, 300, 100, 100, FALSE };
	const GDI_RGN rgn300x300_300x100 = { 0, 300, 300, 300, 100, FALSE };
	const GDI_RGN rgn300x300_300x200 = { 0, 300, 300, 100, 200, FALSE };
	const GDI_RGN rgn300x420_100x100 = { 0, 300, 420, 100, 100, FALSE };
	const GDI_RGN rgn350x300_50x100 = { 0, 350, 300, 50, 100, FALSE };
	const GDI_RGN rgn350x300_200x100 = { 0, 350, 300, 200, 100, FALSE };
	const GDI_RGN rgn420x420_100x100 = { 0, 420, 420, 100, 100, FALSE };

	const struct testcase_t testcases[] = {
		/* null clipping region */
		{ NULL, &rgn20x20_100x100, &rgn20x20_100x100 },
		/* region all inside clipping region */
		{ &rgn0x0_1024x768, &rgn20x20_100x100, &rgn20x20_100x100 },
		/* region all outside clipping region, on the left */
		{ &rgn300x300_100x100, &rgn20x20_100x100, NULL },
		/* region all outside clipping region, on the right */
		{ &rgn300x300_100x100, &rgn420x420_100x100, NULL },
		/* region all outside clipping region, on top */
		{ &rgn300x300_100x100, &rgn300x20_100x100, NULL },
		/* region all outside clipping region, at the bottom */
		{ &rgn300x300_100x100, &rgn300x420_100x100, NULL },
		/* left outside, right = clip, top = clip, bottom = clip */
		{ &rgn300x300_100x100, &rgn100x300_300x100, &rgn300x300_100x100 },
		/* left outside, right inside, top = clip, bottom = clip */
		{ &rgn300x300_100x100, &rgn100x300_250x100, &rgn300x300_50x100 },
		/* left = clip, right outside, top = clip, bottom = clip */
		{ &rgn300x300_100x100, &rgn300x300_300x100, &rgn300x300_100x100 },
		/* left inside, right outside, top = clip, bottom = clip */
		{ &rgn300x300_100x100, &rgn350x300_200x100, &rgn350x300_50x100 },
		/* top outside, bottom = clip, left = clip, right = clip */
		{ &rgn300x300_100x100, &rgn300x100_300x300, &rgn300x300_100x100 },
		/* top = clip, bottom outside, left = clip, right = clip */
		{ &rgn300x300_100x100, &rgn300x300_300x200, &rgn300x300_100x100 },
		/* top = clip, bottom = clip, top = clip, bottom = clip */
		{ &rgn300x300_100x100, &rgn300x300_100x100, &rgn300x300_100x100 }
	};

	for (size_t x = 0; x < ARRAYSIZE(testcases); x++)
	{
		const struct testcase_t* cur = &testcases[x];
		if (!test_gdi_coords(x, hdc, cur->clip, cur->in, cur->expect))
			goto fail;
	}

	rc = 0;
fail:
	gdi_DeleteObject((HGDIOBJECT)bmp);
	gdi_DeleteDC(hdc);
	return rc;
}

static int test_gdi_InvalidateRegion(void)
{
	int rc = -1;
	HGDI_DC hdc = NULL;
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
	gdi_SelectObject(hdc, (HGDIOBJECT)bmp);
	gdi_SetNullClipRgn(hdc);
	hdc->hwnd = (HGDI_WND)calloc(1, sizeof(GDI_WND));
	hdc->hwnd->invalid = gdi_CreateRectRgn(0, 0, 0, 0);
	hdc->hwnd->invalid->null = TRUE;
	invalid = hdc->hwnd->invalid;
	hdc->hwnd->count = 16;
	hdc->hwnd->cinvalid = (HGDI_RGN)calloc(hdc->hwnd->count, sizeof(GDI_RGN));
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
	return rc;
}

int TestGdiClip(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	(void)fprintf(stderr, "test_gdi_ClipCoords()\n");

	if (test_gdi_ClipCoords() < 0)
		return -1;

	(void)fprintf(stderr, "test_gdi_InvalidateRegion()\n");

	if (test_gdi_InvalidateRegion() < 0)
		return -1;

	return 0;
}
