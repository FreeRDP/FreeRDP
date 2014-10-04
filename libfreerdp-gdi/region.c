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


INLINE static boolean add_cinvalid(GDI_WND *hwnd, const GDI_RGN *r)
{
	int count;
	GDI_RGN *cinvalid = hwnd->cinvalid;

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

	gdi_SetRgn(&cinvalid[hwnd->ninvalid++], r->x, r->y, r->w, r->h);
	return true;
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
		{
			//        ________
			//       |   __   |
			//       |  |  |  |
			//       |__|__|__|
			//          |__|

			expendable->h = (expendable->y + expendable->h) - (invariant->y + invariant->h);
			expendable->y = invariant->y + invariant->h;
			return 1;
		}

		if (region_contains_point(invariant, expendable->x, expendable->y + expendable->h - 1))
		{
			//        ________
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
		{
			//           __
			//          |  |
			//        __|__|__
			//       |  |__|  |
			//       |        |
			//       |________|

			expendable->h = invariant->y - expendable->y;
			return 1;
		}

		if (region_contains_point(invariant, expendable->x + expendable->w - 1, expendable->y))
		{
			//        ________
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
	{
		//        ________
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
	{
		//              _____
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


typedef struct EmptyRegions_
{
	GDI_RGN tmp;
	GDI_RGN *cache[0x20];
	uint8 count;
} EmptyRegions;

static boolean clear_region_if_inside_any_other(GDI_RGN *r, const GDI_RGN *begin, const GDI_RGN *end)
{
	const GDI_RGN *ci;
	const int r_right = r->x + r->w;
	const int r_bottom = r->y + r->h;

	for (ci = begin; ci != end; ++ci)
	{
		if (r != ci && ci->x <= r->x && ci->y <= r->y && 
			ci->x + ci->w >= r_right && ci->y + ci->h >= r_bottom)
		{
			r->w = 0;
			r->h = 0;
			return true;
		}
	}

	return false;
}

static unsigned char decompose_partial_overlap_region_pair(GDI_RGN *first, GDI_RGN *second, EmptyRegions *empties, GDI_RGN *others_begin, GDI_RGN *others_end)
{
	uint8 rv;
	GDI_RGN *fragment = empties->count ? empties->cache[empties->count - 1] : &empties->tmp;
	GDI_RGN *cleared;

	if (second->w>first->w) //prefer wider region as invariant
	{
		cleared = second;
		second = first;
		first = cleared;
	}
	cleared =NULL;

	rv = substract_regions_if_vertex_inside(first, second, fragment);
	if (rv)
	{
		if (second->w==0 || second->h==0 || clear_region_if_inside_any_other(second, others_begin, others_end))
			cleared = second;
		if (rv==2 && clear_region_if_inside_any_other(fragment, others_begin, others_end))
			rv = 1;
	}
	else
	{
		rv = substract_regions_if_vertex_inside(second, first, fragment);
		if (rv)
		{
			if (first->w==0 || first->h==0 || clear_region_if_inside_any_other(first, others_begin, others_end))
				cleared = first;

			if (rv==2 && clear_region_if_inside_any_other(fragment, others_begin, others_end))
				rv = 1;
		}
		else if ( first->x < second->x && first->x + first->w > second->x + second->w &&
				first->y > second->y && first->y + first->h < second->y + second->h)
		{
			//        ___2nd__
			//     __|________|__
			//    |  |        |  |
			//    |__|________|__|1st
			//       |________|

			fragment->x = second->x;
			fragment->w = second->w;
			fragment->y = first->y + first->h;
			fragment->h = second->y + second->h - fragment->y;

			second->h = first->y - second->y;

			if (second->w==0 || second->h==0 || clear_region_if_inside_any_other(second, others_begin, others_end))
				cleared = second;

			if (clear_region_if_inside_any_other(fragment, others_begin, others_end))
				rv = 1;
			else
				rv = 2;
		}
	}

	if (cleared)
	{
		if (rv==2)
		{
			memcpy(cleared, fragment, sizeof(*cleared));
			fragment->w = 0;
			rv = 1;
		}
		else if (empties->count < (sizeof(empties->cache)/sizeof(empties->cache[0])) )
		{
			empties->cache[empties->count] = cleared;
			empties->count++;
		}
	}

	return rv;
}

INLINE static uint8 combine_regions_if_sidebyside_horizontally(GDI_RGN *ci, GDI_RGN *cj)
{
	int tmp;
	if (ci->h==cj->h && ci->y==cj->y)
	{
		if (ci->x <= cj->x && ci->x + ci->w >= cj->x)
		{//ci horizontally combines or consumes cj
			tmp = cj->x + cj->w - ci->x;
			if (ci->w < tmp)
				ci->w = tmp;
			cj->w = 0;
			return 2;
		}

		if (cj->x <= ci->x && cj->x + cj->w >= ci->x)
		{//cj horizontally combines or consumes ci
			tmp = ci->x + ci->w - cj->x;
			if (cj->w < tmp)
				cj->w = tmp;
			ci->w = 0;
			return 1;
		}
	}

	return 0;
}

INLINE static uint8 combine_regions_if_sidebyside_vertically(GDI_RGN *ci, GDI_RGN *cj)
{
	int tmp;
	if (ci->w==cj->w && ci->x==cj->x)
	{
		if (ci->y <= cj->y && ci->y + ci->h >= cj->y)
		{//ci vertically combines or consumes cj
			tmp = cj->y + cj->h - ci->y;
			if (ci->h < tmp)
				ci->h = tmp;
			cj->w = 0;
			return 2;
		}

		if (cj->y <= ci->y && cj->y + cj->h >= ci->y)
		{//cj vertically combines or consumes ci
			tmp = ci->y + ci->h - cj->y;
			if (cj->h < tmp)
				cj->h = tmp;
			ci->w = 0;
			return 1;
		}
	}

	return 0;
}

static void decompose_sidebyside_regions(GDI_RGN *begin, GDI_RGN *end, const GDI_RGN *invalid)
{
	GDI_RGN *ci, *cj;
	uint8 r;
	boolean loop_flag;
	int n = 0;

	do
	{
		loop_flag = false;
		for (ci = begin; ci != end; ++ci)
		{
			if (ci->w<=0) continue;

			//first check ci for horizontal merge against subsequent entries
			for (cj = ci + 1; cj != end; ++cj)
			{
				if (cj->w > 0)
				{
					r = combine_regions_if_sidebyside_horizontally(ci, cj);
					if (r)
					{
						++n;
						if (r==1)
							break;//ci was 'eaten' by cj, so no sence in continuing this loop
					}
				}
			}

			if (cj != end || //ci was eaten -> dot check its for vertical merge
				(invalid && ci->h >= invalid->h) ) //ci already has maximum possible height
			{
				continue;
			}
		}

		for (ci = begin; ci != end; ++ci)
		{
			if (ci->w<=0) continue;

			//ci not emptied yet, check it for vertical merge against preceedings 
			for (cj = ci+1; cj != end; ++cj)
			{
				r = combine_regions_if_sidebyside_vertically(ci, cj);
				if (r==2)
				{
					++n;
					if (!invalid || ci->w < invalid->w)//otherwise no sense in horizontal merge of this region
						loop_flag = true;
				}
				else if (r==1)
				{
					++n;
					if (!invalid || cj->w < invalid->w)//otherwise no sense in horizontal merge of this region
						loop_flag = true;
					break;
				}
			}
		}
	} while (loop_flag);

//	if (n > 50 || (n > 1 && n * 2 >= end - begin))
//		end = begin + purge_empty_regions(begin, end);
}


INLINE static void put_to_empties(EmptyRegions *empties, GDI_RGN *r)
{
	const uint8 count = empties->count;
	if (count < sizeof(empties->cache) / sizeof(empties->cache[0]))
	{
		empties->cache[count] = r;
		empties->count = count + 1;
	}
}


static boolean decompose_regions_inside_regions(GDI_RGN *begin, GDI_RGN *end, EmptyRegions *empties)
{//returns true if there're any other intersections remain
	boolean rv = false;
	GDI_RGN *ci, *cj;
	int ci_right, ci_bottom;
	int cj_right, cj_bottom;

	for (ci = begin; ci!=end; ++ci)
	{
		if (ci->w<=0)
		{
			put_to_empties(empties, ci);
			continue;
		}

		ci_right = ci->x + ci->w;
		ci_bottom = ci->y + ci->h;

		for (cj = ci + 1; cj!=end; ++cj)
		{
			if (cj->w<=0) continue;

			cj_right = cj->x + cj->w;
			cj_bottom = cj->y + cj->h;
			if (ci->x < cj_right &&  cj->x < ci_right &&
				ci->y < cj_bottom && cj->y < ci_bottom)
			{
				if (ci->x <= cj->x && ci->y <= cj->y &&
					ci_right >= cj_right && ci_bottom >= cj_bottom)
				{ //ci consumes cj
					cj->w = 0; 
				}
				else if (cj->x <= ci->x && cj->y <= ci->y &&
					cj_right >= ci_right && cj_bottom >= ci_bottom)
				{ //cj consumes ci
					ci->w = 0; 
					put_to_empties(empties, ci);
					break;//no need no check anything more against this ci
				}
				else
					rv = true;
			}
		}
	}

	return rv;
}

static void decompose_invalid_regions(GDI_WND *hwnd)
{
	uint8 r;
	boolean need_decompose_sidebyside_regions;
	boolean loop_flag;
	GDI_RGN *ci, *cj;
	GDI_RGN *begin = hwnd->cinvalid;
	GDI_RGN *end = begin + hwnd->ninvalid;
	EmptyRegions empties;
	empties.count = 0;

	decompose_sidebyside_regions(begin, end, hwnd->invalid);

	if (!decompose_regions_inside_regions(begin, end, &empties))
		return;

	need_decompose_sidebyside_regions = false;
	for (;;)
	{
		loop_flag = false;
		for (ci = begin; ci!=end; ++ci)
		{
			if (ci->w<=0)
				continue;

			for (cj = ci + 1; cj!=end; ++cj)
			{
				if (cj->w<=0)
					continue;

				r = decompose_partial_overlap_region_pair(ci, cj, &empties, begin, end);
				if (r)
				{
					if (r>1)
					{
						if (empties.count==0)
						{
							if (!add_cinvalid(hwnd, &empties.tmp))
								return;

							ci = hwnd->cinvalid + (ci - begin);
							cj = hwnd->cinvalid + (cj - begin);

							begin = hwnd->cinvalid;
							end = begin + hwnd->ninvalid;
						}
						else
							empties.count--;
					}
					loop_flag = true;
					if (cj->w<=0) break;
				}
			}
		}
		if (!loop_flag) break;
		need_decompose_sidebyside_regions = true;
	}


	if (need_decompose_sidebyside_regions)
	{//do full sidebyside decomposition cuz this time there will be no 'inside' decomposition
		decompose_sidebyside_regions(begin, end, NULL);
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
	if (hdc->hwnd && hdc->hwnd->invalid && hdc->hwnd->invalid->w && hdc->hwnd->invalid->h)
		decompose_invalid_regions(hdc->hwnd);
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
	HGDI_RGN invalid;
	HGDI_RGN cinvalid;
	int ninvalid;
	int binvalid;
	uint8 r;
	GDI_RGN arg;
	if (hdc->hwnd == NULL)
		return 0;

	if (hdc->hwnd->invalid == NULL)
		return 0;

	gdi_SetRgn(&arg, x, y, w, h);

	if (arg.x < 0)
	{
		arg.w+= arg.x;
		arg.x = 0;
	}

	if (arg.y < 0)
	{
		arg.h+= arg.x;
		arg.y = 0;
	}

	if (arg.w <= 0 || arg.h <= 0)
		return 0;

	cinvalid = hdc->hwnd->cinvalid;
	invalid = hdc->hwnd->invalid;
	if (invalid->null)
	{
		gdi_SetRgn(invalid, arg.x, arg.y, arg.w, arg.h);
		hdc->hwnd->ninvalid = 0;
		hdc->hwnd->binvalid = 0;
	}
	else
	{
		ninvalid = hdc->hwnd->ninvalid;
		binvalid = hdc->hwnd->binvalid;

		if (binvalid>=ninvalid)
			hdc->hwnd->binvalid = binvalid = 0;

		if (invalid->x > arg.x)
		{
			invalid->w += invalid->x - arg.x;
			invalid->x = arg.x;
		}
		if (invalid->y > arg.y)
		{
			invalid->h += invalid->y - arg.y;
			invalid->y = arg.y;
		}

		if (invalid->x + invalid->w < arg.x + arg.w)
			invalid->w = (arg.x + arg.w) - invalid->x;

		if (invalid->y + invalid->h < arg.y + arg.h)
			invalid->h = (arg.y + arg.h) - invalid->y;
# if 1
		if (ninvalid)
		{
			//first check if this region is subpart of existing biggest region
			if (cinvalid[binvalid].x <= arg.x && cinvalid[binvalid].y <= arg.y && 
				cinvalid[binvalid].x + cinvalid[binvalid].w >= arg.x + arg.w &&
				cinvalid[binvalid].y + cinvalid[binvalid].h >= arg.y + arg.h)
			{//yes, its completely subpart of biggest region - do not add it to array
				return 0;
			}

			//then may be it will even increase biggest ever region?
			r = combine_regions_if_sidebyside_horizontally(&cinvalid[binvalid], &arg);
			if (!r) r = combine_regions_if_sidebyside_vertically(&cinvalid[binvalid], &arg);
			if (r)
			{
				if (r==1)
					gdi_SetRgn(&cinvalid[binvalid], arg.x, arg.y, arg.w, arg.h);
				return 0;
			}

			//check if this region can be combined with last in array - its quite frequent case
			if (ninvalid>1)
			{
				r = combine_regions_if_sidebyside_horizontally(&cinvalid[ninvalid-1], &arg);
				if (!r) r = combine_regions_if_sidebyside_vertically(&cinvalid[ninvalid-1], &arg);
				if (r)
				{
					if (r==1) //first arg was 'eaten' by second, so copy result back to cinvalid
						gdi_SetRgn(&cinvalid[ninvalid-1], arg.x, arg.y, arg.w, arg.h);

					if (cinvalid[ninvalid-1].w >= cinvalid[binvalid].w &&
						cinvalid[ninvalid-1].h >= cinvalid[binvalid].h)
					{//we have new champion
						binvalid = ninvalid - 1;
					}
					return 0;
				}
			}

			if (arg.w >= cinvalid[binvalid].w && arg.h >= cinvalid[binvalid].h)
			{//we will have new champion
				binvalid = ninvalid;
			}
		}
# endif
	}

	add_cinvalid(hdc->hwnd, &arg);
	return 0;
}



