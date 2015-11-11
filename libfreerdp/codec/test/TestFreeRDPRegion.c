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

#include <winpr/crt.h>
#include <winpr/print.h>

#include <freerdp/codec/region.h>


static BOOL compareRectangles(const RECTANGLE_16 *src1, const RECTANGLE_16 *src2, int nb)
{
	int i;
	for (i = 0; i< nb; i++, src1++, src2++)
	{
		if (memcmp(src1, src2, sizeof(RECTANGLE_16)))
		{
			fprintf(stderr, "expecting rect %d (%d,%d-%d,%d) and have (%d,%d-%d,%d)\n",
					i, src2->left, src2->top, src2->right, src2->bottom,
					src1->left, src1->top, src1->right, src1->bottom
					);
			return FALSE;
		}
	}
	return TRUE;
}


static int test_basic() {
	REGION16 region;
	int retCode = -1;
	const RECTANGLE_16 *rects;
	int nbRects;

	/* R1 + R2 ==> disjointed rects */
	RECTANGLE_16 r1 = {  0, 101, 200, 201};
	RECTANGLE_16 r2 = {150, 301, 250, 401};

	RECTANGLE_16 r1_r2[] = {
		{0,   101, 200, 201},
		{150, 301, 250, 401}
	};

	/* r1 */
	region16_init(&region);
	if (!region16_union_rect(&region, &region, &r1))
		goto out;;
	rects = region16_rects(&region, &nbRects);
	if (!rects || nbRects != 1 || memcmp(rects, &r1, sizeof(RECTANGLE_16)))
		goto out;

	/* r1 + r2 */
	if (!region16_union_rect(&region, &region, &r2))
		goto out;;
	rects = region16_rects(&region, &nbRects);
	if (!rects || nbRects != 2 || !compareRectangles(rects, r1_r2, nbRects))
		goto out;


	/* clear region */
	region16_clear(&region);
	region16_rects(&region, &nbRects);
	if (nbRects)
		goto out;

	retCode = 0;
out:
	region16_uninit(&region);
	return retCode;
}


static int test_r1_r3() {
	REGION16 region;
	int retCode = -1;
	const RECTANGLE_16 *rects;
	int nbRects;

	RECTANGLE_16 r1 = {  0, 101, 200, 201};
	RECTANGLE_16 r3 = {150, 151, 250, 251};
	RECTANGLE_16 r1_r3[] = {
		{  0, 101, 200, 151},
		{  0, 151, 250, 201},
		{150, 201, 250, 251}
	};

	region16_init(&region);
	/*
	 * +===============================================================
	 * |
	 * |+-----+                +-----+
	 * || r1  |                |     |
	 * ||   +-+------+         +-----+--------+
	 * ||   |    r3  |         |              |
	 * |+---+        | ====>   +-----+--------+
	 * |    |        |               |        |
	 * |    +--------+               +--------+
	 */

	/* R1 + R3  */
	if (!region16_union_rect(&region, &region, &r1))
		goto out;
	if (!region16_union_rect(&region, &region, &r3))
		goto out;
	rects = region16_rects(&region, &nbRects);
	if (!rects || nbRects != 3 || !compareRectangles(rects, r1_r3, nbRects))
		goto out;


	/* R3 + R1  */
	region16_clear(&region);
	if (!region16_union_rect(&region, &region, &r3))
		goto out;
	if (!region16_union_rect(&region, &region, &r1))
		goto out;
	rects = region16_rects(&region, &nbRects);
	if (!rects || nbRects != 3 || !compareRectangles(rects, r1_r3, nbRects))
		goto out;

	retCode = 0;
out:
	region16_uninit(&region);
	return retCode;
}


static int test_r9_r10() {
	REGION16 region;
	int retCode = -1;
	const RECTANGLE_16 *rects;
	int nbRects;

	/*
	 * +===============================================================
	 * |
	 * |   +---+                +---+
	 * |+--|r10|-+           +--+---+-+
	 * ||r9|   | |           |        |
	 * ||  |   | |           |        |
	 * ||  |   | |  =====>   |        |
	 * ||  |   | |           |        |
	 * ||  |   | |           |        |
 	 * |+--|   |-+           +--+---+-+
 	 * |   +---+                +---+
	 */
	RECTANGLE_16 r9 = {  0, 100, 400, 200};
	RECTANGLE_16 r10 = {200,  0, 300, 300};
	RECTANGLE_16 r9_r10[] = {
			{200,   0, 300, 100},
			{  0, 100, 400, 200},
			{200, 200, 300, 300},
	};

	region16_init(&region);
	if (!region16_union_rect(&region, &region, &r9))
		goto out;
	if (!region16_union_rect(&region, &region, &r10))
		goto out;
	rects = region16_rects(&region, &nbRects);
	if (!rects || nbRects != 3 || !compareRectangles(rects, r9_r10, nbRects))
		goto out;

	retCode = 0;
out:
	region16_uninit(&region);
	return retCode;
}


static int test_r1_r5() {
	REGION16 region;
	int retCode = -1;
	const RECTANGLE_16 *rects;
	int nbRects;

	RECTANGLE_16 r1 = {  0, 101, 200, 201};
	RECTANGLE_16 r5 = {150, 121, 300, 131};

	RECTANGLE_16 r1_r5[] = {
		{  0, 101, 200, 121},
		{  0, 121, 300, 131},
		{  0, 131, 200, 201}
	};

	region16_init(&region);
	/*
	 * +===============================================================
	 * |
	 * |+--------+               +--------+
	 * || r1     |               |        |
	 * ||     +--+----+          +--------+----+
	 * ||     |  r5   |  =====>  |             |
	 * ||     +-------+          +--------+----+
	 * ||        |               |        |
 	 * |+--------+               +--------+
 	 * |
 	 *
	 */
	if (!region16_union_rect(&region, &region, &r1))
		goto out;
	if (!region16_union_rect(&region, &region, &r5))
		goto out;
	rects = region16_rects(&region, &nbRects);
	if (!rects || nbRects != 3 || !compareRectangles(rects, r1_r5, nbRects))
		goto out;


	retCode = 0;
out:
	region16_uninit(&region);
	return retCode;
}

static int test_r1_r6() {
	REGION16 region;
	int retCode = -1;
	const RECTANGLE_16 *rects;
	int nbRects;

	RECTANGLE_16 r1 = {  0, 101, 200, 201};
	RECTANGLE_16 r6 = {150, 121, 170, 131};

	region16_init(&region);
	/*
	 * +===============================================================
	 * |
	 * |+--------+           +--------+
	 * || r1     |           |        |
	 * ||   +--+ |           |        |
	 * ||   |r6| |  =====>   |        |
	 * ||   +--+ |           |        |
	 * ||        |           |        |
 	 * |+--------+           +--------+
 	 * |
	 */
	region16_clear(&region);
	if (!region16_union_rect(&region, &region, &r1))
		goto out;
	if (!region16_union_rect(&region, &region, &r6))
		goto out;
	rects = region16_rects(&region, &nbRects);
	if (!rects || nbRects != 1 || !compareRectangles(rects, &r1, nbRects))
		goto out;

	retCode = 0;
out:
	region16_uninit(&region);
	return retCode;
}


static int test_r1_r2_r4() {
	REGION16 region;
	int retCode = -1;
	const RECTANGLE_16 *rects;
	int nbRects;
	RECTANGLE_16 r1 = {  0, 101, 200, 201};
	RECTANGLE_16 r2 = {150, 301, 250, 401};
	RECTANGLE_16 r4 = {150, 251, 250, 301};
	RECTANGLE_16 r1_r2_r4[] = {
		{  0, 101, 200, 201},
		{150, 251, 250, 401}
	};

	/*
	 * +===============================================================
	 * |
	 * |+-----+                +-----+
	 * || r1  |                |     |
	 * ||     |                |     |
	 * ||     |                |     |
	 * |+-----+        ====>   +-----+
	 * |
	 * |    +--------+               +--------+
	 * |    |   r4   |               |        |
	 * |    +--------+               |        |
	 * |    | r2     |               |        |
	 * |    |        |               |        |
	 * |    +--------+               +--------+
	 *
	 */
	region16_init(&region);
	if (!region16_union_rect(&region, &region, &r1))
		goto out;
	if (!region16_union_rect(&region, &region, &r2))
		goto out;
	if (!region16_union_rect(&region, &region, &r4))
		goto out;
	rects = region16_rects(&region, &nbRects);
	if (!rects || nbRects != 2 || !compareRectangles(rects, r1_r2_r4, nbRects))
		goto out;

	retCode = 0;
out:
	region16_uninit(&region);
	return retCode;
}


static int test_r1_r7_r8() {
	REGION16 region;
	int retCode = -1;
	const RECTANGLE_16 *rects;
	int nbRects;
	RECTANGLE_16 r1 = {  0, 101, 200, 201};
	RECTANGLE_16 r7 = {300, 101, 500, 201};
	RECTANGLE_16 r8 = {150, 121, 400, 131};

	RECTANGLE_16 r1_r7_r8[] = {
		{  0, 101, 200, 121},
		{300, 101, 500, 121},
		{  0, 121, 500, 131},
		{  0, 131, 200, 201},
		{300, 131, 500, 201},
	};

	/*
	 * +===============================================================
	 * |
	 * |+--------+   +--------+           +--------+   +--------+
	 * || r1     |   | r7     |           |        |   |        |
	 * ||   +------------+    |           +--------+---+--------+
	 * ||   |   r8       |    |   =====>  |                     |
	 * ||   +------------+    |           +--------+---+--------+
	 * ||        |   |        |           |        |   |        |
 	 * |+--------+   +--------+           +--------+   +--------+
 	 * |
	 */
	region16_init(&region);
	if (!region16_union_rect(&region, &region, &r1))
		goto out;
	if (!region16_union_rect(&region, &region, &r7))
		goto out;
	if (!region16_union_rect(&region, &region, &r8))
		goto out;
	rects = region16_rects(&region, &nbRects);
	if (!rects || nbRects != 5 || !compareRectangles(rects, r1_r7_r8, nbRects))
		goto out;

	region16_clear(&region);
	if (!region16_union_rect(&region, &region, &r1))
		goto out;
	if (!region16_union_rect(&region, &region, &r8))
		goto out;
	if (!region16_union_rect(&region, &region, &r7))
		goto out;
	rects = region16_rects(&region, &nbRects);
	if (!rects || nbRects != 5 || !compareRectangles(rects, r1_r7_r8, nbRects))
		goto out;

	region16_clear(&region);
	if (!region16_union_rect(&region, &region, &r8))
		goto out;
	if (!region16_union_rect(&region, &region, &r7))
		goto out;
	if (!region16_union_rect(&region, &region, &r1))
		goto out;
	rects = region16_rects(&region, &nbRects);
	if (!rects || nbRects != 5 || !compareRectangles(rects, r1_r7_r8, nbRects))
		goto out;

	retCode = 0;
out:
	region16_uninit(&region);
	return retCode;
}


static int test_r1_r2_r3_r4() {
	REGION16 region;
	int retCode = -1;
	const RECTANGLE_16 *rects;
	int nbRects;
	RECTANGLE_16 r1 = {  0, 101, 200, 201};
	RECTANGLE_16 r2 = {150, 301, 250, 401};
	RECTANGLE_16 r3 = {150, 151, 250, 251};
	RECTANGLE_16 r4 = {150, 251, 250, 301};

	RECTANGLE_16 r1_r2_r3[] = {
		{  0, 101, 200, 151},
		{  0, 151, 250, 201},
		{150, 201, 250, 251},
		{150, 301, 250, 401}
	};

	RECTANGLE_16 r1_r2_r3_r4[] = {
		{  0, 101, 200, 151},
		{  0, 151, 250, 201},
		{150, 201, 250, 401}
	};

	region16_init(&region);
	/*
	 * +===============================================================
	 * |
	 * |+-----+                +-----+
	 * || r1  |                |     |
	 * ||   +-+------+         +-----+--------+
	 * ||   |    r3  |         |              |
	 * |+---+        | ====>   +-----+--------+
	 * |    |        |               |        |
	 * |    +--------+               +--------+
	 * |    +--------+               +--------+
	 * |    | r2     |               |        |
	 * |    |        |               |        |
	 * |    +--------+               +--------+
	 */
	if (!region16_union_rect(&region, &region, &r1))
		goto out;
	if (!region16_union_rect(&region, &region, &r2))
		goto out;
	if (!region16_union_rect(&region, &region, &r3))
		goto out;
	rects = region16_rects(&region, &nbRects);
	if (!rects || nbRects != 4 || !compareRectangles(rects, r1_r2_r3, 4))
		goto out;

	/*
	 * +===============================================================
	 * |
	 * |+-----+                 +-----+
	 * ||     |                 |     |
 	 * |+-----+--------+        +-----+--------+
	 * ||              |  ==>   |              |
	 * |+-----+--------+        +-----+--------+
	 * |  	  |        |              |        |
	 * |  	  +--------+              |        |
	 * |      |  + r4  |              |        |
	 * |  	  +--------+              |        |
	 * |  	  |        |              |        |
	 * |  	  |        |              |        |
	 * |  	  +--------+              +--------+
	 */
	if (!region16_union_rect(&region, &region, &r4))
		goto out;
	rects = region16_rects(&region, &nbRects);
	if (!rects || nbRects != 3 || !compareRectangles(rects, r1_r2_r3_r4, 3))
		goto out;

	retCode = 0;
out:
	region16_uninit(&region);
	return retCode;
}


static int test_from_weston()
{
	/*
	 * 0: 0,0 -> 640,32 (w=640 h=32)
	 * 1: 236,169 -> 268,201 (w=32 h=32)
	 * 2: 246,258 -> 278,290 (w=32 h=32)
	 */
	REGION16 region;
	int retCode = -1;
	const RECTANGLE_16 *rects;
	int nbRects;
	RECTANGLE_16 r1 = {  0,   0, 640,  32};
	RECTANGLE_16 r2 = {236, 169, 268, 201};
	RECTANGLE_16 r3 = {246, 258, 278, 290};

	RECTANGLE_16 r1_r2_r3[] = {
			{  0,   0, 640,  32},
			{236, 169, 268, 201},
			{246, 258, 278, 290}
	};

	region16_init(&region);
	/*
	 * +===============================================================
	 * |+-------------------------------------------------------------+
	 * ||              r1                                             |
	 * |+-------------------------------------------------------------+
	 * |
	 * |       +---------------+
	 * |       |     r2        |
	 * |       +---------------+
	 * |
	 * |         +---------------+
	 * |         |     r3        |
	 * |         +---------------+
	 * |
	 */
	if (!region16_union_rect(&region, &region, &r1))
		goto out;
	if (!region16_union_rect(&region, &region, &r2))
		goto out;
	if (!region16_union_rect(&region, &region, &r3))
		goto out;

	rects = region16_rects(&region, &nbRects);
	if (!rects || nbRects != 3 || !compareRectangles(rects, r1_r2_r3, 3))
		goto out;

	retCode = 0;
out:
	region16_uninit(&region);
	return retCode;
}

static int test_r1_inter_r3() {
	REGION16 region, intersection;
	int retCode = -1;
	const RECTANGLE_16 *rects;
	int nbRects;
	RECTANGLE_16 r1 = {  0, 101, 200, 201};
	RECTANGLE_16 r3 = {150, 151, 250, 251};

	RECTANGLE_16 r1_inter_r3[] = {
		{150, 151, 200, 201},
	};

	region16_init(&region);
	region16_init(&intersection);

	/*
	 * +===============================================================
	 * |
	 * |+-----+
	 * || r1  |
	 * ||   +-+------+             +-+
	 * ||   |    r3  | r1&r3       | |
	 * |+---+        | ====>       +-+
	 * |    |        |
	 * |    +--------+
	 */
	if (!region16_union_rect(&region, &region, &r1))
		goto out;
	if (!region16_intersects_rect(&region, &r3))
		goto out;

	if (!region16_intersect_rect(&intersection, &region, &r3))
		goto out;
	rects = region16_rects(&intersection, &nbRects);
	if (!rects || nbRects != 1 || !compareRectangles(rects, r1_inter_r3, nbRects))
		goto out;


	retCode = 0;
out:
	region16_uninit(&region);
	return retCode;
}

static int test_r1_r3_inter_r11() {
	REGION16 region, intersection;
	int retCode = -1;
	const RECTANGLE_16 *rects;
	int nbRects;
	RECTANGLE_16 r1 = {  0, 101, 200, 201};
	RECTANGLE_16 r3 = {150, 151, 250, 251};
	RECTANGLE_16 r11 ={170, 151, 600, 301};

	RECTANGLE_16 r1_r3_inter_r11[] = {
		{170, 151, 250, 251},
	};

	region16_init(&region);
	region16_init(&intersection);

	/*
	 * +===============================================================
	 * |
	 * |+-----+
	 * ||     |
	 * ||     +------+
	 * || r1+r3      |          (r1+r3) & r11
	 * ||     +----------------+             +--------+
	 * |+---+ |      |         |   ====>     |        |
	 * |    | |      |         |             |        |
	 * |    | |      |         |             |        |
	 * |    +-|------+         |             +--------+
	 * |      |            r11 |
	 * |      +----------------+
	 *
	 *
	 * R1+R3 is made of 3 bands, R11 overlap the second and the third band. The
	 * intersection is made of two band that must be reassembled to give only
	 * one
	 */
	if (!region16_union_rect(&region, &region, &r1))
		goto out;
	if (!region16_union_rect(&region, &region, &r3))
		goto out;

	if (!region16_intersects_rect(&region, &r11))
		goto out;

	if (!region16_intersect_rect(&intersection, &region, &r11))
		goto out;
	rects = region16_rects(&intersection, &nbRects);
	if (!rects || nbRects != 1 || !compareRectangles(rects, r1_r3_inter_r11, nbRects))
		goto out;

	retCode = 0;
out:
	region16_uninit(&intersection);
	region16_uninit(&region);
	return retCode;
}

static int test_norbert_case() {
	REGION16 region, intersection;
	int retCode = -1;
	const RECTANGLE_16 *rects;
	int nbRects, i;

	RECTANGLE_16 inRectangles[5] = {
			{1680,    0, 1920,  242},
			{ 294,  242,  971,  776},
			{1680,  242, 1920,  776},
			{1680,  776, 1920, 1036},
			{   2, 1040,   53, 1078}
	};

	RECTANGLE_16 screenRect = {
		0, 0, 1920, 1080
	};
	RECTANGLE_16 expected_inter_extents = {
		2, 0, 1920, 1078
	};

	region16_init(&region);
	region16_init(&intersection);

	/*
	 * Consider following as a screen with resolution 1920*1080
	 *      | |    |     |           |               |      |
	 *      | |2   |53   |294        |971            |1680  |
	 *      | |    |     |           |               |      |
	 *    0 +=+======================================+======+
	 *      | |                                      |      |
	 *      |                                        |  R[0]|
	 *  242 |            +-----------+               +------+
	 *      | |          |           |               |      |
	 *      |            |           |               |      |
	 *      |            |       R[1]|               |  R[2]|
	 *  776 | |          +-----------+               +------+
	 *      |                                        |      |
	 *      |                                        |  R[3]|
	 * 1036 | |                                      +------+
	 * 1040 | +----+                                        
	 *      | |R[4]|                         Union of R[0-4]|
	 * 1078 | +----+    -    -    -    -    -    -    -    -+
	 * 1080 |                                       
	 *   
	 *   
	 * The result is union of R[0] - R[4].
	 * After intersected with the full screen rect, the 
	 * result should keep the same.
	 */
	for (i = 0; i < 5; i++)
	{
		if (!region16_union_rect(&region, &region, &inRectangles[i]))
			goto out;
	}

	if (!compareRectangles(region16_extents(&region), &expected_inter_extents, 1) )
		goto out;

	if (!region16_intersect_rect(&intersection, &region, &screenRect))
		goto out;
	rects = region16_rects(&intersection, &nbRects);
	if (!rects || nbRects != 5 || !compareRectangles(rects, inRectangles, nbRects))
		goto out;

	if (!compareRectangles(region16_extents(&intersection), &expected_inter_extents, 1) )
		goto out;

	retCode = 0;
out:
	region16_uninit(&intersection);
	region16_uninit(&region);
	return retCode;
}

static int test_norbert2_case() {
	REGION16 region;
	int retCode = -1;
	const RECTANGLE_16 *rects;
	int nbRects = 0;
	RECTANGLE_16 rect1 = {  464, 696,   476,  709 };
	RECTANGLE_16 rect2 = {    0,   0,  1024,   32 };

	region16_init(&region);

	if (!region16_union_rect(&region, &region, &rect1)) {
		fprintf(stderr, "%s: Error 1 - region16_union_rect failed\n", __FUNCTION__);
		goto out;
	}

	if (!(rects = region16_rects(&region, &nbRects))) {
		fprintf(stderr, "%s: Error 2 - region16_rects failed\n", __FUNCTION__);
		goto out;
	}

	if (nbRects != 1) {
		fprintf(stderr, "%s: Error 3 - expected nbRects == 1 but got %d\n", __FUNCTION__, nbRects);
		goto out;
	}

	if (!compareRectangles(&rects[0], &rect1, 1)) {
		fprintf(stderr, "%s: Error 4 - compare failed\n", __FUNCTION__);
		goto out;
	}

	if (!region16_union_rect(&region, &region, &rect2)) {
		fprintf(stderr, "%s: Error 5 - region16_union_rect failed\n", __FUNCTION__);
		goto out;
	}

	if (!(rects = region16_rects(&region, &nbRects))) {
		fprintf(stderr, "%s: Error 6 - region16_rects failed\n", __FUNCTION__);
		goto out;
	}

	if (nbRects != 2) {
		fprintf(stderr, "%s: Error 7 - expected nbRects == 2 but got %d\n", __FUNCTION__, nbRects);
		goto out;
	}

	if (!compareRectangles(&rects[0], &rect2, 1)) {
		fprintf(stderr, "%s: Error 8 - compare failed\n", __FUNCTION__);
		goto out;
	}

	if (!compareRectangles(&rects[1], &rect1, 1)) {
		fprintf(stderr, "%s: Error 9 - compare failed\n", __FUNCTION__);
		goto out;
	}

	retCode = 0;
out:
	region16_uninit(&region);
	return retCode;
}

static int test_empty_rectangle() {
	REGION16 region, intersection;
	int retCode = -1;
	int i;

	RECTANGLE_16 emptyRectangles[3] = {
		{   0,    0,    0,    0},
		{  10,   10,   10,   11},
		{  10,   10,   11,   10}
	};

	RECTANGLE_16 firstRect = {
		0, 0, 100, 100
	};
	RECTANGLE_16 anotherRect = {
		100, 100,  200,  200
	};
	RECTANGLE_16 expected_inter_extents = {
		0, 0, 0, 0 
	};

	region16_init(&region);
	region16_init(&intersection);

	/* Check for empty rectangles */
	for (i = 0; i < 3; i++)
	{
		if (!rectangle_is_empty(&emptyRectangles[i]))
			goto out;
	}

	/* Check for non-empty rectangles */
	if (rectangle_is_empty(&firstRect))
		goto out;

	/* Intersect 2 non-intersect rectangle, result should be empty */
	if (!region16_union_rect(&region, &region, &firstRect))
		goto out;

	if (!region16_intersect_rect(&region, &region, &anotherRect))
		goto out;

	if (!compareRectangles(region16_extents(&region), &expected_inter_extents, 1) )
		goto out;

	if (!region16_is_empty(&region))
		goto out;

	if (!rectangle_is_empty(region16_extents(&intersection)))
		goto out;

	retCode = 0;
out:
	region16_uninit(&intersection);
	region16_uninit(&region);
	return retCode;
}

typedef int (*TestFunction)();
struct UnitaryTest {
	const char *name;
	TestFunction func;
};

struct UnitaryTest tests[] = {
	{"Basic trivial tests", 	test_basic},
	{"R1+R3 and R3+R1", 		test_r1_r3},
	{"R1+R5",					test_r1_r5},
	{"R1+R6",					test_r1_r6},
	{"R9+R10", 					test_r9_r10},
	{"R1+R2+R4", 				test_r1_r2_r4},
	{"R1+R7+R8 in many orders", test_r1_r7_r8},
	{"R1+R2+R3+R4",     		test_r1_r2_r3_r4},
	{"data from weston",        test_from_weston},
	{"R1 & R3",					test_r1_inter_r3},
	{"(R1+R3)&R11 (band merge)",test_r1_r3_inter_r11},
	{"norbert's case",			test_norbert_case},
	{"norbert's case 2", 		test_norbert2_case},
	{"empty rectangle case",	test_empty_rectangle},

	{NULL, NULL}
};

int TestFreeRDPRegion(int argc, char* argv[])
{
	int i, testNb = 0;
	int retCode = -1;

	for (i = 0; tests[i].func; i++)
	{
		testNb++;
		fprintf(stderr, "%d: %s\n", testNb, tests[i].name);
		retCode = tests[i].func();
		if (retCode < 0)
			break;
	}

	if (retCode < 0)
		fprintf(stderr, "failed for test %d\n", testNb);

	return retCode;
}
