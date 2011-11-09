/**
 * FreeRDP: A Remote Desktop Protocol Client
 * GDI Region Functions
 *
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>

#include <freerdp/gdi/region.h>

/**
 * Create a region from rectangular coordinates.\n
 * @msdn{dd183514}
 * @param nLeftRect x1
 * @param nTopRect y1
 * @param nRightRect x2
 * @param nBottomRect y2
 * @return new region
 */

HGDI_RGN gdi_CreateRectRgn(int nLeftRect, int nTopRect, int nRightRect, int nBottomRect)
{
	HGDI_RGN hRgn = (HGDI_RGN) malloc(sizeof(GDI_RGN));
	hRgn->objectType = GDIOBJECT_REGION;
	hRgn->x = nLeftRect;
	hRgn->y = nTopRect;
	hRgn->w = nRightRect - nLeftRect + 1;
	hRgn->h = nBottomRect - nTopRect + 1;
	hRgn->null = 0;
	return hRgn;
}

/**
 * Create a new rectangle.
 * @param xLeft x1
 * @param yTop y1
 * @param xRight x2
 * @param yBottom y2
 * @return new rectangle
 */

HGDI_RECT gdi_CreateRect(int xLeft, int yTop, int xRight, int yBottom)
{
	HGDI_RECT hRect = (HGDI_RECT) malloc(sizeof(GDI_RECT));
	hRect->objectType = GDIOBJECT_RECT;
	hRect->left = xLeft;
	hRect->top = yTop;
	hRect->right = xRight;
	hRect->bottom = yBottom;
	return hRect;
}

/**
 * Convert a rectangle to a region.
 * @param rect source rectangle
 * @param rgn destination region
 */

INLINE void gdi_RectToRgn(HGDI_RECT rect, HGDI_RGN rgn)
{
	rgn->x = rect->left;
	rgn->y = rect->top;
	rgn->w = rect->right - rect->left + 1;
	rgn->h = rect->bottom - rect->top + 1;
}

/**
 * Convert rectangular coordinates to a region.
 * @param left x1
 * @param top y1
 * @param right x2
 * @param bottom y2
 * @param rgn destination region
 */

INLINE void gdi_CRectToRgn(int left, int top, int right, int bottom, HGDI_RGN rgn)
{
	rgn->x = left;
	rgn->y = top;
	rgn->w = right - left + 1;
	rgn->h = bottom - top + 1;
}

/**
 * Convert a rectangle to region coordinates.
 * @param rect source rectangle
 * @param x x1
 * @param y y1
 * @param w width
 * @param h height
 */

INLINE void gdi_RectToCRgn(HGDI_RECT rect, int *x, int *y, int *w, int *h)
{
	*x = rect->left;
	*y = rect->top;
	*w = rect->right - rect->left + 1;
	*h = rect->bottom - rect->top + 1;
}

/**
 * Convert rectangular coordinates to region coordinates.
 * @param left x1
 * @param top y1
 * @param right x2
 * @param bottom y2
 * @param x x1
 * @param y y1
 * @param w width
 * @param h height
 */

INLINE void gdi_CRectToCRgn(int left, int top, int right, int bottom, int *x, int *y, int *w, int *h)
{
	*x = left;
	*y = top;
	*w = right - left + 1;
	*h = bottom - top + 1;
}

/**
 * Convert a region to a rectangle.
 * @param rgn source region
 * @param rect destination rectangle
 */

INLINE void gdi_RgnToRect(HGDI_RGN rgn, HGDI_RECT rect)
{
	rect->left = rgn->x;
	rect->top = rgn->y;
	rect->right = rgn->x + rgn->w - 1;
	rect->bottom = rgn->y + rgn->h - 1;
}

/**
 * Convert region coordinates to a rectangle.
 * @param x x1
 * @param y y1
 * @param w width
 * @param h height
 * @param rect destination rectangle
 */

INLINE void gdi_CRgnToRect(int x, int y, int w, int h, HGDI_RECT rect)
{
	rect->left = x;
	rect->top = y;
	rect->right = x + w - 1;
	rect->bottom = y + h - 1;
}

/**
 * Convert a region to rectangular coordinates.
 * @param rgn source region
 * @param left x1
 * @param top y1
 * @param right x2
 * @param bottom y2
 */

INLINE void gdi_RgnToCRect(HGDI_RGN rgn, int *left, int *top, int *right, int *bottom)
{
	*left = rgn->x;
	*top = rgn->y;
	*right = rgn->x + rgn->w - 1;
	*bottom = rgn->y + rgn->h - 1;
}

/**
 * Convert region coordinates to rectangular coordinates.
 * @param x x1
 * @param y y1
 * @param w width
 * @param h height
 * @param left x1
 * @param top y1
 * @param right x2
 * @param bottom y2
 */

INLINE void gdi_CRgnToCRect(int x, int y, int w, int h, int *left, int *top, int *right, int *bottom)
{
	*left = x;
	*top = y;
	*right = x + w - 1;
	*bottom = y + h - 1;
}

/**
 * Check if copying would involve overlapping regions
 * @param x x1
 * @param y y1
 * @param width width
 * @param height height
 * @param srcx source x1
 * @param srcy source y1
 * @return 1 if there is an overlap, 0 otherwise
 */

INLINE int gdi_CopyOverlap(int x, int y, int width, int height, int srcx, int srcy)
{
	GDI_RECT dst;
	GDI_RECT src;

	gdi_CRgnToRect(x, y, width, height, &dst);
	gdi_CRgnToRect(srcx, srcy, width, height, &src);

	return (dst.right > src.left && dst.left < src.right &&
		dst.bottom > src.top && dst.top < src.bottom) ? 1 : 0;
}

/**
 * Set the coordinates of a given rectangle.\n
 * @msdn{dd145085}
 * @param rc rectangle
 * @param xLeft x1
 * @param yTop y1
 * @param xRight x2
 * @param yBottom y2
 * @return 1 if successful, 0 otherwise
 */

INLINE int gdi_SetRect(HGDI_RECT rc, int xLeft, int yTop, int xRight, int yBottom)
{
	rc->left = xLeft;
	rc->top = yTop;
	rc->right = xRight;
	rc->bottom = yBottom;
	return 1;
}

/**
 * Set the coordinates of a given region.
 * @param hRgn region
 * @param nXLeft x1
 * @param nYLeft y1
 * @param nWidth width
 * @param nHeight height
 * @return
 */

INLINE int gdi_SetRgn(HGDI_RGN hRgn, int nXLeft, int nYLeft, int nWidth, int nHeight)
{
	hRgn->x = nXLeft;
	hRgn->y = nYLeft;
	hRgn->w = nWidth;
	hRgn->h = nHeight;
	hRgn->null = 0;
	return 0;
}

/**
 * Convert rectangular coordinates to a region
 * @param hRgn destination region
 * @param nLeftRect x1
 * @param nTopRect y1
 * @param nRightRect x2
 * @param nBottomRect y2
 * @return
 */

INLINE int gdi_SetRectRgn(HGDI_RGN hRgn, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect)
{
	gdi_CRectToRgn(nLeftRect, nTopRect, nRightRect, nBottomRect, hRgn);
	hRgn->null = 0;
	return 0;
}

/**
 * Compare two regions for equality.\n
 * @msdn{dd162700}
 * @param hSrcRgn1 first region
 * @param hSrcRgn2 second region
 * @return 1 if both regions are equal, 0 otherwise
 */

INLINE int gdi_EqualRgn(HGDI_RGN hSrcRgn1, HGDI_RGN hSrcRgn2)
{
	if ((hSrcRgn1->x == hSrcRgn2->x) &&
	    (hSrcRgn1->y == hSrcRgn2->y) &&
	    (hSrcRgn1->w == hSrcRgn2->w) &&
	    (hSrcRgn1->h == hSrcRgn2->h))
	{
		return 1;
	}

	return 0;
}

/**
 * Copy coordinates from a rectangle to another rectangle
 * @param dst destination rectangle
 * @param src source rectangle
 * @return 1 if successful, 0 otherwise
 */

INLINE int gdi_CopyRect(HGDI_RECT dst, HGDI_RECT src)
{
	dst->left = src->left;
	dst->top = src->top;
	dst->right = src->right;
	dst->bottom = src->bottom;
	return 1;
}

/**
 * Check if a point is inside a rectangle.\n
 * @msdn{dd162882}
 * @param rc rectangle
 * @param x point x position
 * @param y point y position
 * @return 1 if the point is inside, 0 otherwise
 */

INLINE int gdi_PtInRect(HGDI_RECT rc, int x, int y)
{
	/*
	 * points on the left and top sides are considered in,
	 * while points on the right and bottom sides are considered out
	 */

	if (x >= rc->left && x <= rc->right)
	{
		if (y >= rc->top && y <= rc->bottom)
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Invalidate a given region, such that it is redrawn on the next region update.\n
 * @msdn{dd145003}
 * @param hdc device context
 * @param x x1
 * @param y y1
 * @param w width
 * @param h height
 * @return
 */

INLINE int gdi_InvalidateRegion(HGDI_DC hdc, int x, int y, int w, int h)
{
	GDI_RECT inv;
	GDI_RECT rgn;
	HGDI_RGN invalid;
	HGDI_RGN cinvalid;

	if (hdc->hwnd == NULL)
		return 0;

	if (hdc->hwnd->invalid == NULL)
		return 0;

	cinvalid = hdc->hwnd->cinvalid;

	if (hdc->hwnd->ninvalid + 1 > hdc->hwnd->count)
	{
		hdc->hwnd->count *= 2;
		cinvalid = (HGDI_RGN) realloc(cinvalid, sizeof(GDI_RGN) * (hdc->hwnd->count));
	}

	gdi_SetRgn(&cinvalid[hdc->hwnd->ninvalid++], x, y, w, h);
	hdc->hwnd->cinvalid = cinvalid;

	invalid = hdc->hwnd->invalid;

	if (invalid->null)
	{
		invalid->x = x;
		invalid->y = y;
		invalid->w = w;
		invalid->h = h;
		invalid->null = 0;
		return 0;
	}

	gdi_CRgnToRect(x, y, w, h, &rgn);
	gdi_RgnToRect(invalid, &inv);

	if (rgn.left < 0)
		rgn.left = 0;

	if (rgn.top < 0)
		rgn.top = 0;

	if (rgn.left < inv.left)
		inv.left = rgn.left;

	if (rgn.top < inv.top)
		inv.top = rgn.top;

	if (rgn.right > inv.right)
		inv.right = rgn.right;

	if (rgn.bottom > inv.bottom)
		inv.bottom = rgn.bottom;

	gdi_RectToRgn(&inv, invalid);

	return 0;
}
