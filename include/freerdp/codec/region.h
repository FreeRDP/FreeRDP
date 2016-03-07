/**
 * Copyright © 2014 Thincast Technologies GmbH
 * Copyright © 2014 Hardening <contact@hardening-consulting.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __REGION_H___
#define __REGION_H___

#include <freerdp/api.h>
#include <freerdp/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _REGION16_DATA;
typedef struct _REGION16_DATA REGION16_DATA;

/**
 * @brief
 */
struct _REGION16 {
	RECTANGLE_16 extents;
	REGION16_DATA *data;
};
typedef struct _REGION16 REGION16;

/** computes if two rectangles are equal
 * @param r1 first rectangle
 * @param r2 second rectangle
 * @return if the two rectangles are equal
 */
FREERDP_API BOOL rectangles_equal(const RECTANGLE_16 *r1, const RECTANGLE_16 *r2);

/** computes if two rectangles intersect
 * @param r1 first rectangle
 * @param r2 second rectangle
 * @return if the two rectangles intersect
 */
FREERDP_API BOOL rectangles_intersects(const RECTANGLE_16 *r1, const RECTANGLE_16 *r2);

/** computes the intersection of two rectangles
 * @param r1 first rectangle
 * @param r2 second rectangle
 * @param dst resulting intersection
 * @return if the two rectangles intersect
 */
FREERDP_API BOOL rectangles_intersection(const RECTANGLE_16 *r1, const RECTANGLE_16 *r2, RECTANGLE_16 *dst);

/** initialize a region16
 * @param region the region to initialise
 */
FREERDP_API void region16_init(REGION16 *region);

/** @return the number of rectangles of this region16 */
FREERDP_API int region16_n_rects(const REGION16 *region);

/** returns a pointer to rectangles and the number of rectangles in this region.
 * nbRects can be set to NULL if not interested in the number of rectangles.
 * @param region the input region
 * @param nbRects if non-NULL returns the number of rectangles
 * @return a pointer on the rectangles
 */
FREERDP_API const RECTANGLE_16 *region16_rects(const REGION16 *region, int *nbRects);

/** @return the extents rectangle of this region */
FREERDP_API const RECTANGLE_16 *region16_extents(const REGION16 *region);

/** returns if the rectangle is empty
 * @param rect
 * @return if the rectangle is empty
 */
FREERDP_API BOOL rectangle_is_empty(const RECTANGLE_16 *rect);

/** returns if the region is empty
 * @param region
 * @return if the region is empty
 */
FREERDP_API BOOL region16_is_empty(const REGION16 *region);

/** clears the region, the region is resetted to a (0,0,0,0) region
 * @param region
 */
FREERDP_API void region16_clear(REGION16 *region);

/** dumps the region on stderr
 * @param region the region to dump
 */
FREERDP_API void region16_print(const REGION16 *region);

/** copies the region to another region
 * @param dst destination region
 * @param src source region
 * @return if the operation was successful (false meaning out-of-memory)
 */
FREERDP_API BOOL region16_copy(REGION16 *dst, const REGION16 *src);

/** adds a rectangle in src and stores the resulting region in dst
 * @param dst destination region
 * @param src source region
 * @param rect the rectangle to add
 * @return if the operation was successful (false meaning out-of-memory)
 */
FREERDP_API BOOL region16_union_rect(REGION16 *dst, const REGION16 *src, const RECTANGLE_16 *rect);

/** returns if a rectangle intersects the region
 * @param src the region
 * @param arg2 the rectangle
 * @return if region and rectangle intersect
 */
FREERDP_API BOOL region16_intersects_rect(const REGION16 *src, const RECTANGLE_16 *arg2);

/** computes the intersection between a region and a rectangle
 * @param dst destination region
 * @param src the source region
 * @param arg2 the rectangle that intersects
 * @return if the operation was successful (false meaning out-of-memory)
 */
FREERDP_API BOOL region16_intersect_rect(REGION16 *dst, const REGION16 *src, const RECTANGLE_16 *arg2);

/** release internal data associated with this region
 * @param region the region to release
 */
FREERDP_API void region16_uninit(REGION16 *region);

#ifdef __cplusplus
}
#endif

#endif /* __REGION_H___ */
