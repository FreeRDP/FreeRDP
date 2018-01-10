/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Region Functions
 *
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016 Thincast Technologies GmbH
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>

#include <freerdp/gdi/region.h>

#include <freerdp/log.h>

#define TAG FREERDP_TAG("gdi.region")

/**
 * Create a region from rectangular coordinates.\n
 * @msdn{dd183514}
 * @param nLeftRect x1
 * @param nTopRect y1
 * @param nRightRect x2
 * @param nBottomRect y2
 * @return new region
 */

HGDI_RGN gdi_CreateRectRgn(UINT32 nLeftRect, UINT32 nTopRect,
                           UINT32 nRightRect, UINT32 nBottomRect)
{
	HGDI_RGN hRgn = (HGDI_RGN) calloc(1, sizeof(GDI_RGN));

	if (!hRgn)
		return NULL;

	hRgn->objectType = GDIOBJECT_REGION;
	hRgn->x = nLeftRect;
	hRgn->y = nTopRect;
	hRgn->w = nRightRect - nLeftRect + 1;
	hRgn->h = nBottomRect - nTopRect + 1;
	hRgn->null = FALSE;
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

HGDI_RECT gdi_CreateRect(UINT32 xLeft, UINT32 yTop,
                         UINT32 xRight, UINT32 yBottom)
{
	HGDI_RECT hRect = (HGDI_RECT) calloc(1, sizeof(GDI_RECT));

	if (!hRect)
		return NULL;

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

INLINE void gdi_CRectToRgn(UINT32 left, UINT32 top,
                           UINT32 right, UINT32 bottom, HGDI_RGN rgn)
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

INLINE void gdi_RectToCRgn(const HGDI_RECT rect, UINT32* x, UINT32* y,
                           UINT32* w, UINT32* h)
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

INLINE void gdi_CRectToCRgn(UINT32 left, UINT32 top, UINT32 right,
                            UINT32 bottom,
                            UINT32* x, UINT32* y, UINT32* w, UINT32* h)
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

INLINE void gdi_CRgnToRect(INT64 x, INT64 y, UINT32 w, UINT32 h,
                           HGDI_RECT rect)
{
	BOOL invalid = FALSE;
	const INT64 r = x + w - 1;
	const INT64 b = y + h - 1;
	rect->left = (x > 0) ? x : 0;
	rect->top = (y > 0) ? y : 0;
	rect->right = rect->left;
	rect->bottom = rect->top;

	if (r > 0)
		rect->right = r;
	else
		invalid = TRUE;

	if (b > 0)
		rect->bottom = b;
	else
		invalid = TRUE;

	if (invalid)
	{
		WLog_DBG(TAG, "Invisible rectangle %"PRId64"x%"PRId64"-%"PRId64"x%"PRId64,
		         x, y, r, b);
	}
}

/**
 * Convert a region to rectangular coordinates.
 * @param rgn source region
 * @param left x1
 * @param top y1
 * @param right x2
 * @param bottom y2
 */

INLINE void gdi_RgnToCRect(HGDI_RGN rgn, UINT32* left, UINT32* top,
                           UINT32* right, UINT32* bottom)
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

INLINE void gdi_CRgnToCRect(UINT32 x, UINT32 y, UINT32 w, UINT32 h,
                            UINT32* left, UINT32* top, UINT32* right, UINT32* bottom)
{
	*left = x;
	*top = y;
	*right = 0;

	if (w > 0)
		*right = x + w - 1;
	else
		WLog_ERR(TAG, "Invalid width");

	*bottom = 0;

	if (h > 0)
		*bottom = y + h - 1;
	else
		WLog_ERR(TAG, "Invalid height");
}

/**
 * Check if copying would involve overlapping regions
 * @param x x1
 * @param y y1
 * @param width width
 * @param height height
 * @param srcx source x1
 * @param srcy source y1
 * @return nonzero if there is an overlap, 0 otherwise
 */

INLINE BOOL gdi_CopyOverlap(UINT32 x, UINT32 y, UINT32 width, UINT32 height,
                            UINT32 srcx, UINT32 srcy)
{
	GDI_RECT dst;
	GDI_RECT src;
	gdi_CRgnToRect(x, y, width, height, &dst);
	gdi_CRgnToRect(srcx, srcy, width, height, &src);
	return (dst.right >= src.left && dst.left <= src.right &&
	        dst.bottom >= src.top && dst.top <= src.bottom) ? TRUE : FALSE;
}

/**
 * Set the coordinates of a given rectangle.\n
 * @msdn{dd145085}
 * @param rc rectangle
 * @param xLeft x1
 * @param yTop y1
 * @param xRight x2
 * @param yBottom y2
 * @return nonzero if successful, 0 otherwise
 */

INLINE BOOL gdi_SetRect(HGDI_RECT rc, UINT32 xLeft, UINT32 yTop,
                        UINT32 xRight, UINT32 yBottom)
{
	rc->left = xLeft;
	rc->top = yTop;
	rc->right = xRight;
	rc->bottom = yBottom;
	return TRUE;
}

/**
 * Set the coordinates of a given region.
 * @param hRgn region
 * @param nXLeft x1
 * @param nYLeft y1
 * @param nWidth width
 * @param nHeight height
 * @return nonzero if successful, 0 otherwise
 */

INLINE BOOL gdi_SetRgn(HGDI_RGN hRgn, UINT32 nXLeft, UINT32 nYLeft,
                       UINT32 nWidth, UINT32 nHeight)
{
	hRgn->x = nXLeft;
	hRgn->y = nYLeft;
	hRgn->w = nWidth;
	hRgn->h = nHeight;
	hRgn->null = FALSE;
	return TRUE;
}

/**
 * Convert rectangular coordinates to a region
 * @param hRgn destination region
 * @param nLeftRect x1
 * @param nTopRect y1
 * @param nRightRect x2
 * @param nBottomRect y2
 * @return nonzero if successful, 0 otherwise
 */

INLINE BOOL gdi_SetRectRgn(HGDI_RGN hRgn, UINT32 nLeftRect, UINT32 nTopRect,
                           UINT32 nRightRect, UINT32 nBottomRect)
{
	gdi_CRectToRgn(nLeftRect, nTopRect, nRightRect, nBottomRect, hRgn);
	hRgn->null = FALSE;
	return TRUE;
}

/**
 * Compare two regions for equality.\n
 * @msdn{dd162700}
 * @param hSrcRgn1 first region
 * @param hSrcRgn2 second region
 * @return nonzero if both regions are equal, 0 otherwise
 */

INLINE BOOL gdi_EqualRgn(HGDI_RGN hSrcRgn1, HGDI_RGN hSrcRgn2)
{
	if ((hSrcRgn1->x == hSrcRgn2->x) &&
	    (hSrcRgn1->y == hSrcRgn2->y) &&
	    (hSrcRgn1->w == hSrcRgn2->w) &&
	    (hSrcRgn1->h == hSrcRgn2->h))
	{
		return TRUE;
	}

	return FALSE;
}

/**
 * Copy coordinates from a rectangle to another rectangle
 * @msdn{dd183481}
 * @param dst destination rectangle
 * @param src source rectangle
 * @return nonzero if successful, 0 otherwise
 */

INLINE BOOL gdi_CopyRect(HGDI_RECT dst, HGDI_RECT src)
{
	dst->left = src->left;
	dst->top = src->top;
	dst->right = src->right;
	dst->bottom = src->bottom;
	return TRUE;
}

/**
 * Check if a point is inside a rectangle.\n
 * @msdn{dd162882}
 * @param rc rectangle
 * @param x point x position
 * @param y point y position
 * @return nonzero if the point is inside, 0 otherwise
 */

INLINE BOOL gdi_PtInRect(HGDI_RECT rc, UINT32 x, UINT32 y)
{
	/*
	 * points on the left and top sides are considered in,
	 * while points on the right and bottom sides are considered out
	 */
	if (x >= rc->left && x <= rc->right)
	{
		if (y >= rc->top && y <= rc->bottom)
		{
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * Invalidate a given region, such that it is redrawn on the next region update.\n
 * @msdn{dd145003}
 * @param hdc device context
 * @param x x1
 * @param y y1
 * @param w width
 * @param h height
 * @return nonzero on success, 0 otherwise
 */

INLINE BOOL gdi_InvalidateRegion(HGDI_DC hdc, UINT32 x, UINT32 y, UINT32 w,
                                 UINT32 h)
{
	GDI_RECT inv;
	GDI_RECT rgn;
	HGDI_RGN invalid;
	HGDI_RGN cinvalid;

	if (!hdc->hwnd)
		return TRUE;

	if (!hdc->hwnd->invalid)
		return TRUE;

	if (w == 0 || h == 0)
		return TRUE;

	cinvalid = hdc->hwnd->cinvalid;

	if ((hdc->hwnd->ninvalid + 1) > hdc->hwnd->count)
	{
		int new_cnt;
		HGDI_RGN new_rgn;
		new_cnt = hdc->hwnd->count * 2;
		new_rgn = (HGDI_RGN) realloc(cinvalid, sizeof(GDI_RGN) * new_cnt);

		if (!new_rgn)
			return FALSE;

		hdc->hwnd->count = new_cnt;
		cinvalid = new_rgn;
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
		invalid->null = FALSE;
		return TRUE;
	}

	gdi_CRgnToRect(x, y, w, h, &rgn);
	gdi_RgnToRect(invalid, &inv);

	if (rgn.left < inv.left)
		inv.left = rgn.left;

	if (rgn.top < inv.top)
		inv.top = rgn.top;

	if (rgn.right > inv.right)
		inv.right = rgn.right;

	if (rgn.bottom > inv.bottom)
		inv.bottom = rgn.bottom;

	gdi_RectToRgn(&inv, invalid);
	return TRUE;
}
