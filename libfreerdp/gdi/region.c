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

#include <freerdp/config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>

#include <freerdp/gdi/region.h>

#include <freerdp/log.h>

#define TAG FREERDP_TAG("gdi.region")

static char* gdi_rect_str(char* buffer, size_t size, const HGDI_RECT rect)
{
	if (!buffer || (size < 1) || !rect)
		return NULL;

	_snprintf(buffer, size - 1,
	          "[top/left=%" PRId32 "x%" PRId32 "-bottom/right%" PRId32 "x%" PRId32 "]", rect->top,
	          rect->left, rect->bottom, rect->right);
	buffer[size - 1] = '\0';

	return buffer;
}

static char* gdi_regn_str(char* buffer, size_t size, const HGDI_RGN rgn)
{
	if (!buffer || (size < 1) || !rgn)
		return NULL;

	_snprintf(buffer, size - 1, "[%" PRId32 "x%" PRId32 "-%" PRId32 "x%" PRId32 "]", rgn->x, rgn->y,
	          rgn->w, rgn->h);
	buffer[size - 1] = '\0';

	return buffer;
}

/**
 * Create a region from rectangular coordinates.\n
 * @msdn{dd183514}
 * @param nLeftRect x1
 * @param nTopRect y1
 * @param nRightRect x2
 * @param nBottomRect y2
 * @return new region
 */

HGDI_RGN gdi_CreateRectRgn(INT32 nLeftRect, INT32 nTopRect, INT32 nRightRect, INT32 nBottomRect)
{
	INT64 w, h;
	HGDI_RGN hRgn;

	w = nRightRect - nLeftRect + 1ll;
	h = nBottomRect - nTopRect + 1ll;
	if ((w < 0) || (h < 0) || (w > INT32_MAX) || (h > INT32_MAX))
	{
		WLog_ERR(TAG,
		         "Can not create region top/left=%" PRId32 "x%" PRId32 "-bottom/right=%" PRId32
		         "x%" PRId32,
		         nTopRect, nLeftRect, nBottomRect, nRightRect);
		return NULL;
	}
	hRgn = (HGDI_RGN)calloc(1, sizeof(GDI_RGN));

	if (!hRgn)
		return NULL;

	hRgn->objectType = GDIOBJECT_REGION;
	hRgn->x = nLeftRect;
	hRgn->y = nTopRect;
	hRgn->w = w;
	hRgn->h = h;
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

HGDI_RECT gdi_CreateRect(INT32 xLeft, INT32 yTop, INT32 xRight, INT32 yBottom)
{
	HGDI_RECT hRect;

	if (xLeft > xRight)
		return NULL;
	if (yTop > yBottom)
		return NULL;

	hRect = (HGDI_RECT)calloc(1, sizeof(GDI_RECT));

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

BOOL gdi_RectToRgn(const HGDI_RECT rect, HGDI_RGN rgn)
{
	BOOL rc = TRUE;
	INT64 w, h;
	w = rect->right - rect->left + 1ll;
	h = rect->bottom - rect->top + 1ll;

	if ((w < 0) || (h < 0) || (w > INT32_MAX) || (h > INT32_MAX))
	{
		WLog_ERR(TAG,
		         "Can not create region top/left=%" PRId32 "x%" PRId32 "-bottom/right=%" PRId32
		         "x%" PRId32,
		         rect->top, rect->left, rect->bottom, rect->right);
		w = 0;
		h = 0;
		rc = FALSE;
	}

	rgn->x = rect->left;
	rgn->y = rect->top;
	rgn->w = w;
	rgn->h = h;

	return rc;
}

/**
 * Convert rectangular coordinates to a region.
 * @param left x1
 * @param top y1
 * @param right x2
 * @param bottom y2
 * @param rgn destination region
 */

BOOL gdi_CRectToRgn(INT32 left, INT32 top, INT32 right, INT32 bottom, HGDI_RGN rgn)
{
	BOOL rc = TRUE;
	INT64 w, h;
	w = right - left + 1ll;
	h = bottom - top + 1ll;

	if (!rgn)
		return FALSE;

	if ((w < 0) || (h < 0) || (w > INT32_MAX) || (h > INT32_MAX))
	{
		WLog_ERR(TAG,
		         "Can not create region top/left=%" PRId32 "x%" PRId32 "-bottom/right=%" PRId32
		         "x%" PRId32,
		         top, left, bottom, right);
		w = 0;
		h = 0;
		rc = FALSE;
	}

	rgn->x = left;
	rgn->y = top;
	rgn->w = w;
	rgn->h = h;
	return rc;
}

/**
 * Convert a rectangle to region coordinates.
 * @param rect source rectangle
 * @param x x1
 * @param y y1
 * @param w width
 * @param h height
 */

BOOL gdi_RectToCRgn(const HGDI_RECT rect, INT32* x, INT32* y, INT32* w, INT32* h)
{
	BOOL rc = TRUE;
	INT64 tmp;
	*x = rect->left;
	*y = rect->top;
	tmp = rect->right - rect->left + 1;
	if ((tmp < 0) || (tmp > INT32_MAX))
	{
		char buffer[256];
		WLog_ERR(TAG, "[%s] rectangle invalid %s", __FUNCTION__,
		         gdi_rect_str(buffer, sizeof(buffer), rect));
		*w = 0;
		rc = FALSE;
	}
	else
		*w = tmp;
	tmp = rect->bottom - rect->top + 1;
	if ((tmp < 0) || (tmp > INT32_MAX))
	{
		char buffer[256];
		WLog_ERR(TAG, "[%s] rectangle invalid %s", __FUNCTION__,
		         gdi_rect_str(buffer, sizeof(buffer), rect));
		*h = 0;
		rc = FALSE;
	}
	else
		*h = tmp;
	return rc;
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

BOOL gdi_CRectToCRgn(INT32 left, INT32 top, INT32 right, INT32 bottom, INT32* x, INT32* y, INT32* w,
                     INT32* h)
{
	INT64 wl, hl;
	BOOL rc = TRUE;
	wl = right - left + 1ll;
	hl = bottom - top + 1ll;

	if ((left > right) || (top > bottom) || (wl <= 0) || (hl <= 0) || (wl > INT32_MAX) ||
	    (hl > INT32_MAX))
	{
		WLog_ERR(TAG,
		         "Can not create region top/left=%" PRId32 "x%" PRId32 "-bottom/right=%" PRId32
		         "x%" PRId32,
		         top, left, bottom, right);
		wl = 0;
		hl = 0;
		rc = FALSE;
	}

	*x = left;
	*y = top;
	*w = wl;
	*h = hl;
	return rc;
}

/**
 * Convert a region to a rectangle.
 * @param rgn source region
 * @param rect destination rectangle
 */

BOOL gdi_RgnToRect(const HGDI_RGN rgn, HGDI_RECT rect)
{
	INT64 r, b;
	BOOL rc = TRUE;
	r = rgn->x + rgn->w - 1ll;
	b = rgn->y + rgn->h - 1ll;

	if ((r < INT32_MIN) || (r > INT32_MAX) || (b < INT32_MIN) || (b > INT32_MAX))
	{
		char buffer[256];
		WLog_ERR(TAG, "Can not create region %s", gdi_regn_str(buffer, sizeof(buffer), rgn));
		r = rgn->x;
		b = rgn->y;
		rc = FALSE;
	}
	rect->left = rgn->x;
	rect->top = rgn->y;
	rect->right = r;
	rect->bottom = b;

	return rc;
}

/**
 * Convert region coordinates to a rectangle.
 * @param x x1
 * @param y y1
 * @param w width
 * @param h height
 * @param rect destination rectangle
 */

INLINE BOOL gdi_CRgnToRect(INT64 x, INT64 y, INT32 w, INT32 h, HGDI_RECT rect)
{
	BOOL invalid = FALSE;
	const INT64 r = x + w - 1;
	const INT64 b = y + h - 1;
	rect->left = (x > 0) ? x : 0;
	rect->top = (y > 0) ? y : 0;
	rect->right = rect->left;
	rect->bottom = rect->top;

	if ((w <= 0) || (h <= 0))
		invalid = TRUE;

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
		WLog_DBG(TAG, "Invisible rectangle %" PRId64 "x%" PRId64 "-%" PRId64 "x%" PRId64, x, y, r,
		         b);
		return FALSE;
	}

	return TRUE;
}

/**
 * Convert a region to rectangular coordinates.
 * @param rgn source region
 * @param left x1
 * @param top y1
 * @param right x2
 * @param bottom y2
 */

BOOL gdi_RgnToCRect(const HGDI_RGN rgn, INT32* left, INT32* top, INT32* right, INT32* bottom)
{
	BOOL rc = TRUE;
	if ((rgn->w < 0) || (rgn->h < 0))
	{
		char buffer[256];
		WLog_ERR(TAG, "Can not create region %s", gdi_regn_str(buffer, sizeof(buffer), rgn));
		rc = FALSE;
	}

	*left = rgn->x;
	*top = rgn->y;
	*right = rgn->x + rgn->w - 1;
	*bottom = rgn->y + rgn->h - 1;

	return rc;
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

INLINE BOOL gdi_CRgnToCRect(INT32 x, INT32 y, INT32 w, INT32 h, INT32* left, INT32* top,
                            INT32* right, INT32* bottom)
{
	BOOL rc = TRUE;
	*left = x;
	*top = y;
	*right = 0;

	if (w > 0)
		*right = x + w - 1;
	else
	{
		WLog_ERR(TAG, "Invalid width");
		rc = FALSE;
	}

	*bottom = 0;

	if (h > 0)
		*bottom = y + h - 1;
	else
	{
		WLog_ERR(TAG, "Invalid height");
		rc = FALSE;
	}

	return rc;
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

INLINE BOOL gdi_CopyOverlap(INT32 x, INT32 y, INT32 width, INT32 height, INT32 srcx, INT32 srcy)
{
	GDI_RECT dst;
	GDI_RECT src;
	gdi_CRgnToRect(x, y, width, height, &dst);
	gdi_CRgnToRect(srcx, srcy, width, height, &src);

	if (dst.right < src.left)
		return FALSE;
	if (dst.left > src.right)
		return FALSE;
	if (dst.bottom < src.top)
		return FALSE;
	if (dst.top > src.bottom)
		return FALSE;
	return TRUE;
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

INLINE BOOL gdi_SetRect(HGDI_RECT rc, INT32 xLeft, INT32 yTop, INT32 xRight, INT32 yBottom)
{
	if (!rc)
		return FALSE;
	if (xLeft > xRight)
		return FALSE;
	if (yTop > yBottom)
		return FALSE;

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

INLINE BOOL gdi_SetRgn(HGDI_RGN hRgn, INT32 nXLeft, INT32 nYLeft, INT32 nWidth, INT32 nHeight)
{
	if (!hRgn)
		return FALSE;

	if ((nWidth < 0) || (nHeight < 0))
		return FALSE;

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

INLINE BOOL gdi_SetRectRgn(HGDI_RGN hRgn, INT32 nLeftRect, INT32 nTopRect, INT32 nRightRect,
                           INT32 nBottomRect)
{
	if (!gdi_CRectToRgn(nLeftRect, nTopRect, nRightRect, nBottomRect, hRgn))
		return FALSE;
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

INLINE BOOL gdi_EqualRgn(const HGDI_RGN hSrcRgn1, const HGDI_RGN hSrcRgn2)
{
	if ((hSrcRgn1->x == hSrcRgn2->x) && (hSrcRgn1->y == hSrcRgn2->y) &&
	    (hSrcRgn1->w == hSrcRgn2->w) && (hSrcRgn1->h == hSrcRgn2->h))
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

INLINE BOOL gdi_CopyRect(HGDI_RECT dst, const HGDI_RECT src)
{
	if (!dst || !src)
		return FALSE;

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

INLINE BOOL gdi_PtInRect(const HGDI_RECT rc, INT32 x, INT32 y)
{
	/*
	 * points on the left and top sides are considered in,
	 * while points on the right and bottom sides are considered out
	 */
	if ((x >= rc->left) && (x <= rc->right))
	{
		if ((y >= rc->top) && (y <= rc->bottom))
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

INLINE BOOL gdi_InvalidateRegion(HGDI_DC hdc, INT32 x, INT32 y, INT32 w, INT32 h)
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

	if ((hdc->hwnd->ninvalid + 1) > (INT64)hdc->hwnd->count)
	{
		size_t new_cnt;
		HGDI_RGN new_rgn;
		new_cnt = hdc->hwnd->count * 2;
		if (new_cnt > UINT32_MAX)
			return FALSE;

		new_rgn = (HGDI_RGN)realloc(cinvalid, sizeof(GDI_RGN) * new_cnt);

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
