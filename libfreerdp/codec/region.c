/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Thincast Technologies GmbH
 * Copyright 2014 Hardening <contact@hardening-consulting.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <winpr/memory.h>
#include <freerdp/log.h>
#include <freerdp/codec/region.h>

#define TAG FREERDP_TAG("codec")

/*
 * The functions in this file implement the Region abstraction largely inspired from
 * pixman library. The following comment is taken from the pixman code.
 *
 * A Region is simply a set of disjoint(non-overlapping) rectangles, plus an
 * "extent" rectangle which is the smallest single rectangle that contains all
 * the non-overlapping rectangles.
 *
 * A Region is implemented as a "y-x-banded" array of rectangles.  This array
 * imposes two degrees of order.  First, all rectangles are sorted by top side
 * y coordinate first (y1), and then by left side x coordinate (x1).
 *
 * Furthermore, the rectangles are grouped into "bands".  Each rectangle in a
 * band has the same top y coordinate (y1), and each has the same bottom y
 * coordinate (y2).  Thus all rectangles in a band differ only in their left
 * and right side (x1 and x2).  Bands are implicit in the array of rectangles:
 * there is no separate list of band start pointers.
 *
 * The y-x band representation does not minimize rectangles.  In particular,
 * if a rectangle vertically crosses a band (the rectangle has scanlines in
 * the y1 to y2 area spanned by the band), then the rectangle may be broken
 * down into two or more smaller rectangles stacked one atop the other.
 *
 *  -----------                             -----------
 *  |         |                             |         |             band 0
 *  |         |  --------                   -----------  --------
 *  |         |  |      |  in y-x banded    |         |  |      |   band 1
 *  |         |  |      |  form is          |         |  |      |
 *  -----------  |      |                   -----------  --------
 *               |      |                                |      |   band 2
 *               --------                                --------
 *
 * An added constraint on the rectangles is that they must cover as much
 * horizontal area as possible: no two rectangles within a band are allowed
 * to touch.
 *
 * Whenever possible, bands will be merged together to cover a greater vertical
 * distance (and thus reduce the number of rectangles). Two bands can be merged
 * only if the bottom of one touches the top of the other and they have
 * rectangles in the same places (of the same width, of course).
 */

struct _REGION16_DATA {
	long size;
	long nbRects;
};

static REGION16_DATA empty_region = { 0, 0 };

void region16_init(REGION16 *region)
{
	assert(region);

	ZeroMemory(region, sizeof(REGION16));
	region->data = &empty_region;
}

int region16_n_rects(const REGION16 *region)
{
	assert(region);
	assert(region->data);

	return region->data->nbRects;
}

const RECTANGLE_16 *region16_rects(const REGION16 *region, int *nbRects)
{
	REGION16_DATA *data;

	assert(region);

	data = region->data;
	if (!data)
	{
		if (nbRects)
			*nbRects = 0;
		return NULL;
	}

	if (nbRects)
		*nbRects = data->nbRects;
	return (RECTANGLE_16 *)(data + 1);
}

static INLINE RECTANGLE_16 *region16_rects_noconst(REGION16 *region)
{
	REGION16_DATA* data;

	data = region->data;

	if (!data)
		return NULL;

	return (RECTANGLE_16*)(&data[1]);
}

const RECTANGLE_16 *region16_extents(const REGION16 *region)
{
	return &region->extents;
}

static RECTANGLE_16 *region16_extents_noconst(REGION16 *region)
{
	return &region->extents;
}

BOOL rectangle_is_empty(const RECTANGLE_16 *rect)
{
	/* A rectangle with width = 0 or height = 0 should be regarded
	 * as empty.
	 */
	return ((rect->left == rect->right) || (rect->top == rect->bottom)) ? TRUE : FALSE;
}

BOOL region16_is_empty(const REGION16 *region)
{
	assert(region);
	assert(region->data);

	return (region->data->nbRects == 0);
}

BOOL rectangles_equal(const RECTANGLE_16 *r1, const RECTANGLE_16 *r2)
{
	return ((r1->left == r2->left) && (r1->top == r2->top) &&
			(r1->right == r2->right) && (r1->bottom == r2->bottom)) ? TRUE : FALSE;
}

BOOL rectangles_intersects(const RECTANGLE_16 *r1, const RECTANGLE_16 *r2)
{
	RECTANGLE_16 tmp;
	return rectangles_intersection(r1, r2, &tmp);
}

BOOL rectangles_intersection(const RECTANGLE_16 *r1, const RECTANGLE_16 *r2,
		RECTANGLE_16 *dst)
{
	dst->left = MAX(r1->left, r2->left);
	dst->right = MIN(r1->right, r2->right);
	dst->top = MAX(r1->top, r2->top);
	dst->bottom = MIN(r1->bottom, r2->bottom);

	return (dst->left < dst->right) && (dst->top < dst->bottom);
}

void region16_clear(REGION16 *region)
{
	assert(region);
	assert(region->data);

	if (region->data->size)
		free(region->data);

	region->data = &empty_region;

	ZeroMemory(&region->extents, sizeof(region->extents));
}

static INLINE REGION16_DATA* allocateRegion(long nbItems)
{
	long allocSize = sizeof(REGION16_DATA) + (nbItems * sizeof(RECTANGLE_16));

	REGION16_DATA* ret = (REGION16_DATA*) malloc(allocSize);

	if (!ret)
		return ret;

	ret->size = allocSize;
	ret->nbRects = nbItems;

	return ret;
}

BOOL region16_copy(REGION16 *dst, const REGION16 *src)
{
	assert(dst);
	assert(dst->data);
	assert(src);
	assert(src->data);

	if (dst == src)
		return TRUE;

	dst->extents = src->extents;

	if (dst->data->size)
		free(dst->data);

	if (!src->data->size)
	{
		dst->data = &empty_region;
	}
	else
	{
		dst->data = allocateRegion(src->data->nbRects);

		if (!dst->data)
			return FALSE;

		CopyMemory(dst->data, src->data, src->data->size);
	}

	return TRUE;
}

void region16_print(const REGION16 *region)
{
	const RECTANGLE_16 *rects;
	int nbRects, i;
	int currentBandY = -1;

	rects = region16_rects(region, &nbRects);
	WLog_DBG(TAG,  "nrects=%d", nbRects);

	for (i = 0; i < nbRects; i++, rects++)
	{
		if (rects->top != currentBandY)
		{
			currentBandY = rects->top;
			WLog_DBG(TAG,  "band %d: ", currentBandY);
		}

		WLog_DBG(TAG,  "(%d,%d-%d,%d)", rects->left, rects->top, rects->right, rects->bottom);
	}
}

void region16_copy_band_with_union(RECTANGLE_16 *dst,
		const RECTANGLE_16 *src, const RECTANGLE_16 *end,
		UINT16 newTop, UINT16 newBottom,
		const RECTANGLE_16 *unionRect,
		int *dstCounter,
		const RECTANGLE_16 **srcPtr, RECTANGLE_16 **dstPtr)
{
	UINT16 refY = src->top;
	const RECTANGLE_16 *startOverlap, *endOverlap;

	/* merges a band with the given rect
	 * Input:
	 *                   unionRect
	 *               |               |
	 *               |               |
	 * ==============+===============+================================
	 *   |Item1|  |Item2| |Item3|  |Item4|    |Item5|            Band
	 * ==============+===============+================================
	 *    before     |    overlap    |          after
	 *
	 * Resulting band:
	 *   +-----+  +----------------------+    +-----+
	 *   |Item1|  |      Item2           |    |Item3|
	 *   +-----+  +----------------------+    +-----+
	 *
	 *  We first copy as-is items that are before Item2, the first overlapping
	 *  item.
	 *  Then we find the last one that overlap unionRect to agregate Item2, Item3
	 *  and Item4 to create Item2.
	 *  Finally Item5 is copied as Item3.
	 *
	 *  When no unionRect is provided, we skip the two first steps to just copy items
	 */

	if (unionRect)
	{
		/* items before unionRect */
		while ((src < end) && (src->top == refY) && (src->right < unionRect->left))
		{
			dst->top = newTop;
			dst->bottom = newBottom;
			dst->right = src->right;
			dst->left = src->left;

			src++; dst++; *dstCounter += 1;
		}

		/* treat items overlapping with unionRect */
		startOverlap = unionRect;
		endOverlap = unionRect;

		if ((src < end) && (src->top == refY) && (src->left < unionRect->left))
			startOverlap = src;

		while ((src < end) && (src->top == refY) && (src->right < unionRect->right))
		{
			src++;
		}

		if ((src < end) && (src->top == refY) && (src->left < unionRect->right))
		{
			endOverlap = src;
			src++;
		}

		dst->bottom = newBottom;
		dst->top = newTop;
		dst->left = startOverlap->left;
		dst->right = endOverlap->right;
		dst++; *dstCounter += 1;
	}

	/* treat remaining items on the same band */
	while ((src < end) && (src->top == refY))
	{
		dst->top = newTop;
		dst->bottom = newBottom;
		dst->right = src->right;
		dst->left = src->left;

		src++; dst++; *dstCounter += 1;
	}

	if (srcPtr)
		*srcPtr = src;

	*dstPtr = dst;
}

static RECTANGLE_16* next_band(RECTANGLE_16* band1, RECTANGLE_16* endPtr, int* nbItems)
{
	UINT16 refY = band1->top;

	*nbItems = 0;

	while((band1 < endPtr) && (band1->top == refY))
	{
		band1++;
		*nbItems += 1;
	}

	return band1;
}

static BOOL band_match(const RECTANGLE_16* band1, const RECTANGLE_16* band2, RECTANGLE_16* endPtr)
{
	int refBand2 = band2->top;
	const RECTANGLE_16* band2Start = band2;

	while ((band1 < band2Start) && (band2 < endPtr) && (band2->top == refBand2))
	{
		if ((band1->left != band2->left) || (band1->right != band2->right))
			return FALSE;

		band1++;
		band2++;
	}

	if (band1 != band2Start)
		return FALSE;

	return (band2 == endPtr) || (band2->top != refBand2);
}

/** compute if the rectangle is fully included in the band
 * @param band a pointer on the beginning of the band
 * @param endPtr end of the region
 * @param rect the rectangle to test
 * @return if rect is fully included in an item of the band
 */
static BOOL rectangle_contained_in_band(const RECTANGLE_16 *band, const RECTANGLE_16 *endPtr,
		const RECTANGLE_16 *rect)
{
	UINT16 refY = band->top;

	if ((band->top > rect->top) || (rect->bottom > band->bottom))
		return FALSE;

	/* note: as the band is sorted from left to right, once we've seen an item
	 * 		that is after rect->left we're sure that the result is False.
	 */
	while( (band < endPtr) && (band->top == refY) && (band->left <= rect->left))
	{
		if (rect->right <= band->right)
			return TRUE;

		band++;
	}
	return FALSE;
}

BOOL region16_simplify_bands(REGION16 *region)
{
	/** Simplify consecutive bands that touch and have the same items
	 *
	 *  ====================          ====================
	 *     | 1 |  | 2   |               |   |  |     |
	 *  ====================            |   |  |     |
	 *     | 1 |  | 2   |	   ====>    | 1 |  |  2  |
	 *  ====================            |   |  |     |
	 *     | 1 |  | 2   |               |   |  |     |
	 *  ====================          ====================
	 *
	 */
	RECTANGLE_16 *band1, *band2, *endPtr, *endBand, *tmp;
	int nbRects, finalNbRects;
	int bandItems, toMove;

	finalNbRects = nbRects = region16_n_rects(region);

	if (nbRects < 2)
		return TRUE;

	band1 = region16_rects_noconst(region);
	endPtr = band1 + nbRects;

	do
	{
		band2 = next_band(band1, endPtr, &bandItems);

		if (band2 == endPtr)
			break;

		if ((band1->bottom == band2->top) && band_match(band1, band2, endPtr))
		{
			/* adjust the bottom of band1 items */
			tmp = band1;
			while (tmp < band2)
			{
				tmp->bottom = band2->bottom;
				tmp++;
			}

			/* override band2, we don't move band1 pointer as the band after band2
			 * may be merged too */
			endBand = band2 + bandItems;
			toMove = (endPtr - endBand) * sizeof(RECTANGLE_16);

			if (toMove)
				MoveMemory(band2, endBand, toMove);

			finalNbRects -= bandItems;
			endPtr -= bandItems;
		}
		else
		{
			band1 = band2;
		}
	}
	while(TRUE);

	if (finalNbRects != nbRects)
	{
		int allocSize = sizeof(REGION16_DATA) + (finalNbRects * sizeof(RECTANGLE_16));
		region->data = realloc(region->data, allocSize);

		if (!region->data)
		{
			region->data = &empty_region;
			return FALSE;
		}

		region->data->nbRects = finalNbRects;
		region->data->size = allocSize;
	}

	return TRUE;
}

BOOL region16_union_rect(REGION16 *dst, const REGION16 *src, const RECTANGLE_16 *rect)
{
	const RECTANGLE_16* srcExtents;
	RECTANGLE_16* dstExtents;
	const RECTANGLE_16 *currentBand, *endSrcRect, *nextBand;
	REGION16_DATA* newItems = NULL;
	RECTANGLE_16* dstRect = NULL;
	int usedRects, srcNbRects;
	UINT16 topInterBand;

	assert(src);
	assert(src->data);
	assert(dst);

	srcExtents = region16_extents(src);
	dstExtents = region16_extents_noconst(dst);

	if (!region16_n_rects(src))
	{
		/* source is empty, so the union is rect */
		dst->extents = *rect;
		dst->data = allocateRegion(1);

		if (!dst->data)
			return FALSE;

		dstRect = region16_rects_noconst(dst);

		dstRect->top = rect->top;
		dstRect->left = rect->left;
		dstRect->right = rect->right;
		dstRect->bottom = rect->bottom;

		return TRUE;
	}

	newItems = allocateRegion((1 + region16_n_rects(src)) * 4);

	if (!newItems)
		return FALSE;

	dstRect = (RECTANGLE_16*) (&newItems[1]);
	usedRects = 0;

	/* adds the piece of rect that is on the top of src */
	if (rect->top < srcExtents->top)
	{
		dstRect->top = rect->top;
		dstRect->left = rect->left;
		dstRect->right = rect->right;
		dstRect->bottom = MIN(srcExtents->top, rect->bottom);

		usedRects++;
		dstRect++;
	}

	/* treat possibly overlapping region */
	currentBand = region16_rects(src, &srcNbRects);
	endSrcRect = currentBand + srcNbRects;

	while (currentBand < endSrcRect)
	{
		if ((currentBand->bottom <= rect->top) || (rect->bottom <= currentBand->top) ||
			rectangle_contained_in_band(currentBand, endSrcRect, rect))
		{
			/* no overlap between rect and the band, rect is totally below or totally above
			 * the current band, or rect is already covered by an item of the band.
			 * let's copy all the rectangles from this band
						+----+
						|    |   rect (case 1)
						+----+

			   =================
                   band of srcRect
			   =================
						+----+
						|    |   rect (case 2)
						+----+
			 */
			region16_copy_band_with_union(dstRect,
					currentBand, endSrcRect,
					currentBand->top, currentBand->bottom,
					NULL, &usedRects,
					&nextBand, &dstRect);
			topInterBand = rect->top;
		}
		else
		{

			/* rect overlaps the band:
		                           |    |  |    |
             ====^=================|    |==|    |=========================== band
                 |   top split     |    |  |    |
                 v                 | 1  |  | 2  |
                 ^                 |    |  |    |  +----+   +----+
                 |   merge zone    |    |  |    |  |    |   | 4  |
                 v                 +----+  |    |  |    |   +----+
                 ^                         |    |  | 3  |
                 |   bottom split          |    |  |    |
             ====v=========================|    |==|    |===================
                                           |    |  |    |

			 possible cases:
			 1) no top split, merge zone then a bottom split. The band will be splitted
			  in two
			 2) not band split, only the merge zone, band merged with rect but not splitted
			 3) a top split, the merge zone and no bottom split. The band will be split
			 in two
			 4) a top split, the merge zone and also a bottom split. The band will be
			 splitted in 3, but the coalesce algorithm may merge the created bands
			 */
			UINT16 mergeTop = currentBand->top;
			UINT16 mergeBottom = currentBand->bottom;

			/* test if we need a top split, case 3 and 4 */
			if (rect->top > currentBand->top)
			{
				region16_copy_band_with_union(dstRect,
						currentBand, endSrcRect,
						currentBand->top, rect->top,
						NULL, &usedRects,
						&nextBand, &dstRect);
				mergeTop = rect->top;
			}

			/* do the merge zone (all cases) */
			if (rect->bottom < currentBand->bottom)
				mergeBottom = rect->bottom;

			region16_copy_band_with_union(dstRect,
					currentBand, endSrcRect,
					mergeTop, mergeBottom,
					rect, &usedRects,
					&nextBand, &dstRect);

			/* test if we need a bottom split, case 1 and 4 */
			if (rect->bottom < currentBand->bottom)
			{
				region16_copy_band_with_union(dstRect,
						currentBand, endSrcRect,
						mergeBottom, currentBand->bottom,
						NULL, &usedRects,
						&nextBand, &dstRect);
			}
			topInterBand = currentBand->bottom;
		}

		/* test if a piece of rect should be inserted as a new band between
		 * the current band and the next one. band n and n+1 shouldn't touch.
		 *
		 * ==============================================================
		 *                                                        band n
		 *            +------+                    +------+
		 * ===========| rect |====================|      |===============
		 *            |      |    +------+        |      |
		 *            +------+    | rect |        | rect |
		 *                        +------+        |      |
		 * =======================================|      |================
		 *                                        +------+         band n+1
		 * ===============================================================
		 *
		 */
		if ((nextBand < endSrcRect) && (nextBand->top != currentBand->bottom) &&
				(rect->bottom > currentBand->bottom) && (rect->top < nextBand->top))
		{
			dstRect->right = rect->right;
			dstRect->left = rect->left;
			dstRect->top = topInterBand;
			dstRect->bottom = MIN(nextBand->top, rect->bottom);
			dstRect++; usedRects++;
		}

		currentBand = nextBand;
	}

	/* adds the piece of rect that is below src */
	if (srcExtents->bottom < rect->bottom)
	{
		dstRect->top = MAX(srcExtents->bottom, rect->top);
		dstRect->left = rect->left;
		dstRect->right = rect->right;
		dstRect->bottom = rect->bottom;

		usedRects++;
		dstRect++;
	}

	if ((src == dst) && (src->data->size))
		free(src->data);

	dstExtents->top = MIN(rect->top, srcExtents->top);
	dstExtents->left = MIN(rect->left, srcExtents->left);
	dstExtents->bottom = MAX(rect->bottom, srcExtents->bottom);
	dstExtents->right = MAX(rect->right, srcExtents->right);

	newItems->size = sizeof(REGION16_DATA) + (usedRects * sizeof(RECTANGLE_16));
	dst->data = realloc(newItems, newItems->size);

	if (!dst->data)
	{
		free(newItems);
		return FALSE;
	}

	dst->data->nbRects = usedRects;

	return region16_simplify_bands(dst);
}

BOOL region16_intersects_rect(const REGION16 *src, const RECTANGLE_16 *arg2)
{
	const RECTANGLE_16 *rect, *endPtr, *srcExtents;
	int nbRects;

	assert(src);
	assert(src->data);

	rect = region16_rects(src, &nbRects);

	if (!nbRects)
		return FALSE;

	srcExtents = region16_extents(src);

	if (nbRects == 1)
		return rectangles_intersects(srcExtents, arg2);

	if (!rectangles_intersects(srcExtents, arg2))
		return FALSE;

	endPtr = rect + nbRects;

	for (endPtr = rect + nbRects; (rect < endPtr) && (arg2->bottom > rect->top); rect++)
	{
		if (rectangles_intersects(rect, arg2))
			return TRUE;
	}

	return FALSE;
}

BOOL region16_intersect_rect(REGION16 *dst, const REGION16 *src, const RECTANGLE_16 *rect)
{
	REGION16_DATA *newItems;
	const RECTANGLE_16 *srcPtr, *endPtr, *srcExtents;
	RECTANGLE_16 *dstPtr;
	int nbRects, usedRects;
	RECTANGLE_16 common, newExtents;

	assert(src);
	assert(src->data);

	srcPtr = region16_rects(src, &nbRects);

	if (!nbRects)
	{
		region16_clear(dst);
		return TRUE;
	}

	srcExtents = region16_extents(src);

	if (nbRects == 1)
	{
		BOOL intersects = rectangles_intersection(srcExtents, rect, &common);

		region16_clear(dst);

		if (intersects)
			return region16_union_rect(dst, dst, &common);

		return TRUE;
	}

	newItems = allocateRegion(nbRects);

	if (!newItems)
		return FALSE;

	dstPtr = (RECTANGLE_16*) (&newItems[1]);
	usedRects = 0;
	ZeroMemory(&newExtents, sizeof(newExtents));

	/* accumulate intersecting rectangles, the final region16_simplify_bands() will
	 * do all the bad job to recreate correct rectangles
	 */
	for (endPtr = srcPtr + nbRects; (srcPtr < endPtr) && (rect->bottom > srcPtr->top); srcPtr++)
	{
		if (rectangles_intersection(srcPtr, rect, &common))
		{
			*dstPtr = common;
			usedRects++;
			dstPtr++;

			if (rectangle_is_empty(&newExtents))
			{
				/* Check if the existing newExtents is empty. If it is empty, use 
				 * new common directly. We do not need to check common rectangle 
				 * because the rectangles_intersection() ensures that it is not empty.
				 */
				newExtents = common;
			}
			else
			{
				newExtents.top = MIN(common.top, newExtents.top);
				newExtents.left = MIN(common.left, newExtents.left);
				newExtents.bottom = MAX(common.bottom, newExtents.bottom);
				newExtents.right = MAX(common.right, newExtents.right);
			}
		}
	}

	newItems->nbRects = usedRects;
	newItems->size = sizeof(REGION16_DATA) + (usedRects * sizeof(RECTANGLE_16));

	if (dst->data->size)
		free(dst->data);

	dst->data = realloc(newItems, newItems->size);

	if (!dst->data)
	{
		free(newItems);
		return FALSE;
	}

	dst->extents = newExtents;
	return region16_simplify_bands(dst);
}

void region16_uninit(REGION16 *region)
{
	assert(region);

	if (region->data)
	{
		if (region->data->size)
			free(region->data);

		region->data = NULL;
	}
}
