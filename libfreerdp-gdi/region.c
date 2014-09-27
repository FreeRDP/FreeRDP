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


INLINE static boolean add_cinvalid(GDI_WND *hwnd, int x, int y, int w, int h)
{
	int count;
	GDI_RGN *cinvalid = hdc->hwnd->cinvalid;

	if (hwnd->ninvalid >= hwnd->count)
	{
		count = (hwnd->count + 1) * 2;
		cinvalid = (HGDI_RGN) realloc(cinvalid, sizeof(GDI_RGN) * count);
		if (!cinvalid)
		{
			fprintf(stderr, "add_cinvalid: no memory for %u entries\n", count);
			return false;
		}

		hwnd->cinvalid = cinvalid;
		hwnd->count = count;
	}

	gdi_SetRgn(&cinvalid[hwnd->ninvalid++], x, y, w, h);
	return true;
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
	HGDI_RGN invalid;
	HGDI_RGN cinvalid;
	int i;

	if (hdc->hwnd == NULL)
		return 0;

	if (hdc->hwnd->invalid == NULL)
		return 0;

	if (x<0)
	{
		w+= x;
		x = 0;
	}

	if (y<0)
	{
		h+= x;
		y = 0;
	}

	if (w<=0 || h<=0)
		return 0;

	cinvalid = hdc->hwnd->cinvalid;
	invalid = hdc->hwnd->invalid;

	if (invalid->null)
	{
		invalid->x = x;
		invalid->y = y;
		invalid->w = w;
		invalid->h = h;
		invalid->null = 0;
		hdc->hwnd->ninvalid = 0;
	}
	else
	{
		//check if specified region completely included into any existing
		//full decomposition would take too much CPU to be used per each invalidation
		for (i = hdc->hwnd->ninvalid; i; --i, ++cinvalid)
		{
			if (cinvalid->x <= x && cinvalid->y <= y &&
				cinvalid->x + cinvalid->w >= x + w &&
				cinvalid->y + cinvalid->h >= y + h)
			{
				return 0;
			}
		}
		cinvalid = hdc->hwnd->cinvalid;

		if (invalid->x > x) invalid->x = x;
		if (invalid->y > y) invalid->y = y;

		if (invalid->x + invalid->w < x + w) invalid->w = (x + w) - invalid->x;
		if (invalid->y + invalid->h < y + h) invalid->h = (y + h) - invalid->y;
	}
	
	add_cinvalid(hdc->hwnd, x, y, w, h);
	return 0;
}


INLINE static boolean region_contains_point(const GDI_RGN *r, int x, int y)
{
	return (x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h);
}

/**
 * Substracts partially overlapper regions, creating addition region if result can't fit in two regions
 * This function only works if any vertex of 'expendable' region is inside of 'invariant' region
 * It splits 'expendable' to lesser part(s) puting addition part into 'fragment' parameter if neccessary
 * !!!: Be aware that this function will behave incorrectly if 'expendable' completely inside 'invariant' :!!!
 * @param invariant - 'subtrahend' region
 * @param expendable - 'minuend' region
 * @param fragment - place to put addition resulted region if neccessary
 * @return 0 - if no expendable's vertex inside invariant, 1 - if result fit into 2 regions, 2 - if substraction put addition result part into fragment parameter
 */
static uint8 substract_regions_if_vertex_inside(const GDI_RGN *invariant, GDI_RGN *expendable, GDI_RGN *fragment)
{
	if (region_contains_point(invariant, expendable->x, expendable->y))
	{//left-top  of exp inside of inv, subcases:
		if (region_contains_point(invariant, expendable->x + expendable->w - 1, expendable->y))
		{   //        ________
			//       |   __   |
			//       |  |  |  |
			//       |__|__|__|		
			//          |__|

			expendable->h = (expendable->y + expendable->h) - (invariant->y + invariant->h);
			expendable->y = invariant->y + invariant->h;
			return 1;
		}

		if (region_contains_point(invariant, expendable->x, expendable->y + expendable->h - 1))
		{   //        ________
			//       |      __|___
			//       |     |  |   |
			//       |     |__|___|
			//       |________|

			expendable->w = (expendable->x + expendable->w) - (invariant->x + invariant->w);
			expendable->x = invariant->x + invariant->w;
			return 1;
		}

		//        ________
		//       |        |  
		//       |      __|__ 
		//       |     |  |  |
		//       |_____|__|~~|
		//             |_____|


		fragment->x = invariant->x + invariant->w;
		fragment->y = expendable->y;
		fragment->w = expendable->x + expendable->w - fragment->x;
		fragment->h = (invariant->y + invariant->h) - fragment->y;

		expendable->h = (expendable->y + expendable->h) - (invariant->y + invariant->h);
		expendable->y = invariant->y + invariant->h;
		return 2;
	}



	if (region_contains_point(invariant, expendable->x + expendable->w - 1, expendable->y + expendable->h - 1))
	{//right-bottom of exp inside of inv, subcases:
		if (region_contains_point(invariant, expendable->x, expendable->y + expendable->h - 1))
		{   //           __
			//          |  |
			//        __|__|__
			//       |  |__|  |
			//       |        |
			//       |________|		

			expendable->h = invariant->y - expendable->y;
			return 1;
		}

		if (region_contains_point(invariant, expendable->x + expendable->w - 1, expendable->y))
		{   //        ________
			//    ___|_       |
			//   |   | |      |
			//   |___|_|      |
			//       |________|

			expendable->w = invariant->x - expendable->x;
			return 1;
		}

		//     _____
		//    |   __|_____
		//    |~~|  |     |
		//    |__|__|     |
		//       |        |
		//       |________|

		fragment->x = expendable->x;
		fragment->y = invariant->y;
		fragment->w = invariant->x - fragment->x;
		fragment->h = expendable->y + expendable->h - fragment->y;

		expendable->h = invariant->y - expendable->y;
		return 2;
	}


	if (region_contains_point(invariant, expendable->x + expendable->w - 1, expendable->y))
	{   //        ________
		//       |        |
		//     __|__      |
		//    |  |  |     |
		//    |~~|__|_____|
		//    |_____|

		fragment->x = expendable->x;
		fragment->y = expendable->y;
		fragment->w = invariant->x - expendable->x;
		fragment->h = invariant->y + invariant->h - expendable->y;

		expendable->h = expendable->y + expendable->h  - (invariant->y + invariant->h);
		expendable->y = invariant->y + invariant->h;
		return 2;
	}

	if (region_contains_point(invariant, expendable->x, expendable->y + expendable->h - 1))
	{   //              _____
		//             |     |
		//        -----|-- ~~|
		//       |     |__|__|
		//       |        |
		//       |        |
		//       |________|	

		fragment->x = invariant->x + invariant->w;
		fragment->y = invariant->y;
		fragment->w = expendable->x + expendable->w - fragment->x;
		fragment->h = expendable->y + expendable->h - fragment->y;

		expendable->h = invariant->y - expendable->y;
		return 2;
	}

	return 0;
}

/**
 * Substracts partially overlapping regions, creating addition region if result can't fit in two regions
 * This function automatically selects subtrahend / minuend region, so any may be changed as result
 * !!!: Be aware that this function will behave incorrectly if one of regions completely inside another :!!!
 * @param first - 1st region to substract
 * @param second - 2nd region to substract 
 * @param fragment - place to put addition resulted region if neccessary
 * @return 0 - if regions are not partially overlapped, 1 - if substraction result fit into 2 regions, 2 - if substraction put addition result part into fragment parameter
 */
static unsigned char substract_partial_overlap_regions(GDI_RGN *first, GDI_RGN *second, GDI_RGN *fragment)
{
	uint8 rv;

	if (second->w>first->w) //prefer wider region as invariant
	{
		GDI_RGN *t = second;
		second = first;
		first = t;
	}

	rv = substract_regions_if_vertex_inside(first, second, fragment);
	if (!rv)
	{
		rv = substract_regions_if_vertex_inside(second, first, fragment);
		if (!rv &&
			first->x < second->x && first->x + first->w > second->x + second->w &&
			first->y > second->y && first->y + first->h < second->y + second->h)
		{   //        ___2nd__    
			//     __|________|__ 
			//    |  |        |  |
			//    |__|________|__|1st
			//       |________|	

			fragment->x = second->x;
			fragment->w = second->w;
			fragment->y = first->y + first->h;
			fragment->h = second->y + second->h - fragment->y;

			second->h = first->y - second->y;
			rv = 2;
		}
	}

	return rv;
}

static void decompose_sidebyside_regions(GDI_RGN *cinvalid, GDI_RGN *ce)
{
	uint8 flag;
	int tmp;
	GDI_RGN *ci, *cj;

	for (;;)
	{
		for (ci = cinvalid; ci!=ce; ++ci)
		{
			if (ci->w<0) continue;

			for (cj = ci + 1; cj!=ce; ++cj)
			{
				if (cj->w<0) continue;

				if (ci->h==cj->h && ci->y==cj->y)
				{
					if (ci->x <= cj->x && ci->x + ci->w >= cj->x)
					{//[i] horizontally combines or consumes [j] 
						tmp = cj->x + cj->w - ci->x;
						if (ci->w < tmp)
							ci->w = tmp;
						cj->w = -1; 
					}
					else if (cj->x <= ci->x && cj->x + cj->w >= ci->x)
					{//[j] horizontally combines or consumes [i] 
						tmp = ci->x + ci->w - cj->x;
						ci->w = (cj->w < tmp) ? tmp : cj->w;
						ci->x = cj->x;
						cj->w = -1; 
					}
				}
			}
		}

		flag = 0;		 
		for (ci = cinvalid; ci!=ce; ++ci)
		{
			if (ci->w<0) continue;

			for (cj = ci + 1; cj!=ce; ++cj)
			{
				if (cj->w<0) continue;

				if (ci->w==cj->w && ci->x==cj->x)
				{
					if (ci->y <= cj->y && ci->y + ci->h >= cj->y)
					{//[i] vertically combines or consumes [j] 
						tmp = cj->y + cj->h - ci->y;
						if (ci->h < tmp)
							ci->h = tmp;
						cj->w = -1; 
						flag = 1;
					}
					else if (cj->y <= ci->y && cj->y + cj->h >= ci->y)
					{//[j] vertically combines or consumes [i] 
						tmp = ci->y + ci->h - cj->y;
						ci->h = (cj->h < tmp) ? tmp : cj->h;
						ci->y = cj->y;
						cj->w = -1; 
						flag = 1;
					}
				}
			}
		}
		if (!flag) break;
	}
}

static void decompose_invalid_regions(GDI_WND *hwnd)
{
	uint8 flag;
	boolean need_final_sidebyside_decomposition = false;
	int tmp, ninvalid = hwnd->ninvalid;
	GDI_RGN *cinvalid = hwnd->cinvalid;
	GDI_RGN ctmp, *cf, *ci, *cj, *ce = cinvalid + ninvalid;

	decompose_sidebyside_regions(cinvalid, ce);

	for (;;)
	{
		flag = 0;
		cf = &ctmp;
		for (ci = cinvalid; ci!=ce; ++ci)
		{
			if (ci->w<0)
			{
				cf = ci;
				continue;
			}

			for (cj = ci + 1; cj!=ce; ++cj)
			{
				if (cj->w<0) continue;

				if (ci->x < cj->x + cj->w && 
					cj->x < ci->x + ci->w &&
					ci->y < cj->y + cj->h && 
					cj->y < ci->y + ci->h)
				{
					if (ci->x<=cj->x && ci->y<=cj->y &&
						ci->x + ci->w>=cj->x + cj->w &&
						ci->y + ci->h>=cj->y + cj->h)
					{ //[i] consumes [j]
						cj->w = -1; 
					}
					else if (cj->x<=ci->x && cj->y<=ci->y &&
						cj->x + cj->w>=ci->x + ci->w &&
						cj->y + cj->h>=ci->y + ci->h)
					{ //[j] consumes [i]
						ci->w = -1; 
						cf = ci;
						break;//no need no check anything more against this [i]
					}
					else
						flag = 1;
				}
			}
		}

		if (!flag)
		{
			if (need_final_sidebyside_decomposition)
				decompose_sidebyside_regions(cinvalid, ce);
			break;
		}

		flag = 0;
		need_final_sidebyside_decomposition = true;
		for (ci = cinvalid; ci!=ce; ++ci)
		{
			if (ci->w<0) continue;

			for (cj = ci + 1; cj!=ce; ++cj)
			{
				if (cj->w<0) continue;

				flag = substract_partial_overlap_regions(ci, cj, cf);

				if (flag)
				{
					if (flag>1 && cf==&ctmp)
					{
						if (!add_cinvalid(hwnd, ctmp.x, ctmp.y, ctmp.w, ctmp.h))
							return;

						cinvalid = hwnd->cinvalid;
						ninvalid = hwnd->ninvalid;
						ce = cinvalid + ninvalid;
					}
					break;
				}
			}
			if (flag) break;
		}
	}
}


/**
 * Decomposes invalid regions array of specific hdc, so it will contain
 * minimal non-overlapping rectangles set
 * @param hdc device context
 * @return
 */
INLINE int gdi_DecomposeInvalidArea(HGDI_DC hdc)
{
	decompose_invalid_regions(hdc->hwnd);
	return 0;
}

