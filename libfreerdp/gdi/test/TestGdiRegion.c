
#include <freerdp/gdi/gdi.h>

#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/pen.h>
#include <freerdp/gdi/region.h>
#include <freerdp/gdi/bitmap.h>

#include <winpr/crt.h>
#include <winpr/print.h>

#include "helpers.h"

int TestGdiRegion(int argc, char* argv[])
{
	int rc = -1;
	INT32 x, y, w, h;
	INT32 l, r, t, b;
	HGDI_RGN rgn1 = NULL;
	HGDI_RGN rgn2 = NULL;
	HGDI_RECT rect1 = NULL;
	HGDI_RECT rect2 = NULL;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	rgn1 = gdi_CreateRectRgn(111, 2, 65, 77);
	rect1 = gdi_CreateRect(2311, 11, 42, 17);
	if (rgn1 || rect1)
		goto fail;
	rgn1 = gdi_CreateRectRgn(1, 2, 65, 77);
	rgn2 = gdi_CreateRectRgn(11, 2, 65, 77);
	rect1 = gdi_CreateRect(23, 11, 42, 17);
	rect2 = gdi_CreateRect(23, 11, 42, 17);
	if (!rgn1 || !rgn2 || !rect1 || !rect2)
		goto fail;

	if (!gdi_RectToRgn(rect1, rgn1))
		goto fail;
	if (rgn1->x != rect1->left)
		goto fail;
	if (rgn1->y != rect1->top)
		goto fail;
	if (rgn1->w != (rect1->right - rect1->left + 1))
		goto fail;
	if (rgn1->h != (rect1->bottom - rect1->top + 1))
		goto fail;

	if (gdi_CRectToRgn(1123, 111, 333, 444, rgn2))
		goto fail;
	if (gdi_CRectToRgn(123, 1111, 333, 444, rgn2))
		goto fail;
	if (!gdi_CRectToRgn(123, 111, 333, 444, rgn2))
		goto fail;
	if (rgn2->x != 123)
		goto fail;
	if (rgn2->y != 111)
		goto fail;
	if (rgn2->w != (333 - 123 + 1))
		goto fail;
	if (rgn2->h != (444 - 111 + 1))
		goto fail;

	if (!gdi_RectToCRgn(rect1, &x, &y, &w, &h))
		goto fail;
	if (rect1->left != x)
		goto fail;
	if (rect1->top != y)
		goto fail;
	if (rect1->right != (x + w - 1))
		goto fail;
	if (rect1->bottom != (y + h - 1))
		goto fail;

	w = 23;
	h = 42;
	if (gdi_CRectToCRgn(1, 2, 0, 4, &x, &y, &w, &h))
		goto fail;
	if ((w != 0) || (h != 0))
		goto fail;
	w = 23;
	h = 42;
	if (gdi_CRectToCRgn(1, 2, 3, 1, &x, &y, &w, &h))
		goto fail;
	if ((w != 0) || (h != 0))
		goto fail;
	w = 23;
	h = 42;
	if (!gdi_CRectToCRgn(1, 2, 3, 4, &x, &y, &w, &h))
		goto fail;
	if (x != 1)
		goto fail;
	if (y != 2)
		goto fail;
	if (w != (3 - 1 + 1))
		goto fail;
	if (h != (4 - 2 + 1))
		goto fail;

	if (!gdi_RgnToRect(rgn1, rect2))
		goto fail;

	if (rgn1->x != rect2->left)
		goto fail;
	if (rgn1->y != rect2->top)
		goto fail;
	if (rgn1->w != (rect2->right - rect2->left + 1))
		goto fail;
	if (rgn1->h != (rect2->bottom - rect2->top + 1))
		goto fail;

	if (gdi_CRgnToRect(1, 2, 0, 4, rect2))
		goto fail;
	if (gdi_CRgnToRect(1, 2, -1, 4, rect2))
		goto fail;
	if (gdi_CRgnToRect(1, 2, 3, 0, rect2))
		goto fail;
	if (gdi_CRgnToRect(1, 2, 3, -1, rect2))
		goto fail;
	if (!gdi_CRgnToRect(1, 2, 3, 4, rect2))
		goto fail;
	if (rect2->left != 1)
		goto fail;
	if (rect2->right != (1 + 3 - 1))
		goto fail;
	if (rect2->top != 2)
		goto fail;
	if (rect2->bottom != (2 + 4 - 1))
		goto fail;

	if (!gdi_RgnToCRect(rgn1, &l, &t, &r, &b))
		goto fail;
	if (rgn1->x != l)
		goto fail;
	if (rgn1->y != t)
		goto fail;
	if (rgn1->w != (r - l + 1))
		goto fail;
	if (rgn1->h != (b - t + 1))
		goto fail;

	if (gdi_CRgnToCRect(1, 2, -1, 4, &l, &t, &r, &b))
		goto fail;
	if (gdi_CRgnToCRect(1, 2, 0, 4, &l, &t, &r, &b))
		goto fail;
	if (gdi_CRgnToCRect(1, 2, 3, -1, &l, &t, &r, &b))
		goto fail;
	if (gdi_CRgnToCRect(1, 2, 3, -0, &l, &t, &r, &b))
		goto fail;
	if (!gdi_CRgnToCRect(1, 2, 3, 4, &l, &t, &r, &b))
		goto fail;
	if (l != 1)
		goto fail;
	if (t != 2)
		goto fail;
	if (r != (1 + 3 - 1))
		goto fail;
	if (b != 2 + 4 - 1)
		goto fail;

	if (gdi_CopyOverlap(1, 2, 5, 3, -5, 3))
		goto fail;
	if (gdi_CopyOverlap(1, 2, 5, 3, 3, -2))
		goto fail;
	if (!gdi_CopyOverlap(1, 2, 5, 3, 2, 3))
		goto fail;

	if (gdi_SetRect(rect2, -4, 500, 66, -5))
		goto fail;
	if (gdi_SetRect(rect2, -4, -500, -66, -5))
		goto fail;
	if (!gdi_SetRect(rect2, -4, 500, 66, 754))
		goto fail;

	if (gdi_SetRgn(NULL, -23, -42, 33, 99))
		goto fail;
	if (gdi_SetRgn(rgn2, -23, -42, -33, 99))
		goto fail;
	if (gdi_SetRgn(rgn2, -23, -42, 33, -99))
		goto fail;
	if (!gdi_SetRgn(rgn2, -23, -42, 33, 99))
		goto fail;
	if (rgn2->x != -23)
		goto fail;
	if (rgn2->y != -42)
		goto fail;
	if (rgn2->w != 33)
		goto fail;
	if (rgn2->h != 99)
		goto fail;
	if (rgn2->null)
		goto fail;

	if (gdi_SetRectRgn(NULL, 33, 22, 44, 33))
		goto fail;
	if (gdi_SetRectRgn(rgn1, 331, 22, 44, 33))
		goto fail;
	if (gdi_SetRectRgn(rgn1, 33, 122, 44, 33))
		goto fail;
	if (!gdi_SetRectRgn(rgn1, 33, 22, 44, 33))
		goto fail;
	if (rgn1->x != 33)
		goto fail;
	if (rgn1->y != 22)
		goto fail;
	if (rgn1->w != (44 - 33 + 1))
		goto fail;
	if (rgn1->h != (33 - 22 + 1))
		goto fail;

	if (gdi_EqualRgn(rgn1, rgn2))
		goto fail;
	if (!gdi_EqualRgn(rgn1, rgn1))
		goto fail;

	if (gdi_CopyRect(rect1, NULL))
		goto fail;
	if (gdi_CopyRect(NULL, rect1))
		goto fail;
	if (gdi_CopyRect(NULL, NULL))
		goto fail;
	if (!gdi_CopyRect(rect1, rect2))
		goto fail;

	if (rect1->left != rect2->left)
		goto fail;
	if (rect1->top != rect2->top)
		goto fail;
	if (rect1->right != rect2->right)
		goto fail;
	if (rect1->bottom != rect2->bottom)
		goto fail;

	if (gdi_PtInRect(rect1, -23, 550))
		goto fail;
	if (gdi_PtInRect(rect1, 2, 3))
		goto fail;
	if (!gdi_PtInRect(rect1, 2, 550))
		goto fail;

	// BOOL gdi_InvalidateRegion(HGDI_DC hdc, INT32 x, INT32 y, INT32 w, INT32 h);

	rc = 0;
fail:
	free(rgn1);
	free(rgn2);
	free(rect1);
	free(rect2);
	return rc;
}
