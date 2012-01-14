/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Drawing Orders Unit Tests
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/freerdp.h>
#include <freerdp/utils/hexdump.h>
#include <freerdp/utils/stream.h>

#include "test_orders.h"
#include "libfreerdp-core/orders.h"
#include "libfreerdp-core/update.h"

ORDER_INFO* orderInfo;

int init_orders_suite(void)
{
	orderInfo = (ORDER_INFO*) malloc(sizeof(ORDER_INFO));
	return 0;
}

int clean_orders_suite(void)
{
	free(orderInfo);
	return 0;
}

int add_orders_suite(void)
{
	add_test_suite(orders);

	add_test_function(read_dstblt_order);
	add_test_function(read_patblt_order);
	add_test_function(read_scrblt_order);
	add_test_function(read_opaque_rect_order);
	add_test_function(read_draw_nine_grid_order);
	add_test_function(read_multi_opaque_rect_order);
	add_test_function(read_line_to_order);
	add_test_function(read_polyline_order);
	add_test_function(read_glyph_index_order);
	add_test_function(read_fast_index_order);
	add_test_function(read_fast_glyph_order);
	add_test_function(read_polygon_cb_order);

	add_test_function(read_cache_bitmap_order);
	add_test_function(read_cache_bitmap_v2_order);
	add_test_function(read_cache_bitmap_v3_order);
	add_test_function(read_cache_brush_order);

	add_test_function(read_create_offscreen_bitmap_order);
	add_test_function(read_switch_surface_order);

	add_test_function(update_recv_orders);

	return 0;
}

uint8 dstblt_order[] = "\x48\x00\x37\x01";

void test_read_dstblt_order(void)
{
	STREAM _s, *s;
	DSTBLT_ORDER dstblt;

	s = &_s;
	s->p = s->data = dstblt_order;

	memset(orderInfo, 0, sizeof(ORDER_INFO));
	orderInfo->fieldFlags = 0x0C;
	memset(&dstblt, 0, sizeof(DSTBLT_ORDER));

	update_read_dstblt_order(s, orderInfo, &dstblt);

	CU_ASSERT(dstblt.nLeftRect == 0);
	CU_ASSERT(dstblt.nTopRect == 0);
	CU_ASSERT(dstblt.nWidth == 72);
	CU_ASSERT(dstblt.nHeight == 311);
	CU_ASSERT(dstblt.bRop == 0);

	CU_ASSERT(stream_get_length(s) == (sizeof(dstblt_order) - 1));
}

uint8 patblt_order[] = "\x1a\x00\xc3\x01\x0d\x00\x0d\x00\xf0\xff\xff\x00\x5b\xef\x00\x81";

void test_read_patblt_order(void)
{
	STREAM _s, *s;
	PATBLT_ORDER patblt;

	s = &_s;
	s->p = s->data = patblt_order;

	memset(orderInfo, 0, sizeof(ORDER_INFO));
	orderInfo->fieldFlags = 0x027F;
	memset(&patblt, 0, sizeof(PATBLT_ORDER));

	update_read_patblt_order(s, orderInfo, &patblt);

	CU_ASSERT(patblt.nLeftRect == 26);
	CU_ASSERT(patblt.nTopRect == 451);
	CU_ASSERT(patblt.nWidth == 13);
	CU_ASSERT(patblt.nHeight == 13);
	CU_ASSERT(patblt.bRop == 240);
	CU_ASSERT(patblt.backColor == 0x00FFFF);
	CU_ASSERT(patblt.foreColor == 0x00EF5B);
	CU_ASSERT(patblt.brush.x == 0);
	CU_ASSERT(patblt.brush.y == 0);
	CU_ASSERT(patblt.brush.style == (BMF_1BPP | CACHED_BRUSH));

	CU_ASSERT(stream_get_length(s) == (sizeof(patblt_order) - 1));
}

uint8 scrblt_order[] = "\x07\x00\xa1\x01\xf1\x00\xcc\x2f\x01\x8e\x00";

void test_read_scrblt_order(void)
{
	STREAM _s, *s;
	SCRBLT_ORDER scrblt;

	s = &_s;
	s->p = s->data = scrblt_order;

	memset(orderInfo, 0, sizeof(ORDER_INFO));
	orderInfo->fieldFlags = 0x7D;
	memset(&scrblt, 0, sizeof(SCRBLT_ORDER));

	update_read_scrblt_order(s, orderInfo, &scrblt);

	CU_ASSERT(scrblt.nLeftRect == 7);
	CU_ASSERT(scrblt.nTopRect == 0);
	CU_ASSERT(scrblt.nWidth == 417);
	CU_ASSERT(scrblt.nHeight == 241);
	CU_ASSERT(scrblt.bRop == 204);
	CU_ASSERT(scrblt.nXSrc == 303);
	CU_ASSERT(scrblt.nYSrc == 142);

	CU_ASSERT(stream_get_length(s) == (sizeof(scrblt_order) - 1));
}

uint8 opaque_rect_order[] = "\x00\x04\x00\x03\x73\x02\x06";

void test_read_opaque_rect_order(void)
{
	STREAM _s, *s;
	OPAQUE_RECT_ORDER opaque_rect;

	s = &_s;
	s->p = s->data = opaque_rect_order;

	memset(orderInfo, 0, sizeof(ORDER_INFO));
	orderInfo->fieldFlags = 0x7C;
	memset(&opaque_rect, 0, sizeof(OPAQUE_RECT_ORDER));

	update_read_opaque_rect_order(s, orderInfo, &opaque_rect);

	CU_ASSERT(opaque_rect.nLeftRect == 0);
	CU_ASSERT(opaque_rect.nTopRect == 0);
	CU_ASSERT(opaque_rect.nWidth == 1024);
	CU_ASSERT(opaque_rect.nHeight == 768);
	CU_ASSERT(opaque_rect.color == 0x00060273);

	CU_ASSERT(stream_get_length(s) == (sizeof(opaque_rect_order) - 1));
}

uint8 draw_nine_grid_order[] = "\xfb\xf9\x0d\x00";

void test_read_draw_nine_grid_order(void)
{
	STREAM _s, *s;
	DRAW_NINE_GRID_ORDER draw_nine_grid;

	s = &_s;
	s->p = s->data = draw_nine_grid_order;

	memset(orderInfo, 0, sizeof(ORDER_INFO));
	orderInfo->fieldFlags = 0x1C;
	orderInfo->deltaCoordinates = true;

	memset(&draw_nine_grid, 0, sizeof(DRAW_NINE_GRID_ORDER));
	draw_nine_grid.srcRight = 38;
	draw_nine_grid.srcBottom = 40;

	update_read_draw_nine_grid_order(s, orderInfo, &draw_nine_grid);

	CU_ASSERT(draw_nine_grid.srcLeft == 0);
	CU_ASSERT(draw_nine_grid.srcTop == 0);
	CU_ASSERT(draw_nine_grid.srcRight == 33);
	CU_ASSERT(draw_nine_grid.srcBottom == 33);
	CU_ASSERT(draw_nine_grid.bitmapId == 13);

	CU_ASSERT(stream_get_length(s) == (sizeof(draw_nine_grid_order) - 1));
}


uint8 multi_opaque_rect_order[] =
	"\x87\x01\x1c\x01\xf1\x00\x12\x00\x5c\xef\x04\x16\x00\x08\x40\x81"
	"\x87\x81\x1c\x80\xf1\x01\x01\x01\x10\x80\xf0\x01\x10\xff\x10\x10"
	"\x80\xf1\x01";

void test_read_multi_opaque_rect_order(void)
{
	STREAM _s, *s;
	MULTI_OPAQUE_RECT_ORDER multi_opaque_rect;

	s = &_s;
	s->p = s->data = multi_opaque_rect_order;

	memset(orderInfo, 0, sizeof(ORDER_INFO));
	orderInfo->fieldFlags = 0x01BF;
	memset(&multi_opaque_rect, 0, sizeof(MULTI_OPAQUE_RECT_ORDER));

	update_read_multi_opaque_rect_order(s, orderInfo, &multi_opaque_rect);

	CU_ASSERT(multi_opaque_rect.nLeftRect == 391);
	CU_ASSERT(multi_opaque_rect.nTopRect == 284);
	CU_ASSERT(multi_opaque_rect.nWidth == 241);
	CU_ASSERT(multi_opaque_rect.nHeight == 18);
	CU_ASSERT(multi_opaque_rect.color == 0x0000EF5C);
	CU_ASSERT(multi_opaque_rect.cbData == 22);
	CU_ASSERT(multi_opaque_rect.numRectangles == 4);

	CU_ASSERT(multi_opaque_rect.rectangles[1].left == 391);
	CU_ASSERT(multi_opaque_rect.rectangles[1].top == 284);
	CU_ASSERT(multi_opaque_rect.rectangles[1].width == 241);
	CU_ASSERT(multi_opaque_rect.rectangles[1].height == 1);

	CU_ASSERT(multi_opaque_rect.rectangles[2].left == 391);
	CU_ASSERT(multi_opaque_rect.rectangles[2].top == 285);
	CU_ASSERT(multi_opaque_rect.rectangles[2].width == 1);
	CU_ASSERT(multi_opaque_rect.rectangles[2].height == 16);

	CU_ASSERT(multi_opaque_rect.rectangles[3].left == 631);
	CU_ASSERT(multi_opaque_rect.rectangles[3].top == 285);
	CU_ASSERT(multi_opaque_rect.rectangles[3].width == 1);
	CU_ASSERT(multi_opaque_rect.rectangles[3].height == 16);

	CU_ASSERT(multi_opaque_rect.rectangles[4].left == 391);
	CU_ASSERT(multi_opaque_rect.rectangles[4].top == 301);
	CU_ASSERT(multi_opaque_rect.rectangles[4].width == 241);
	CU_ASSERT(multi_opaque_rect.rectangles[4].height == 1);

	CU_ASSERT(stream_get_length(s) == (sizeof(multi_opaque_rect_order) - 1));
}

uint8 line_to_order[] = "\x03\xb1\x0e\xa6\x5b\xef\x00";

void test_read_line_to_order(void)
{
	STREAM _s, *s;
	LINE_TO_ORDER line_to;

	s = &_s;
	s->p = s->data = line_to_order;

	memset(orderInfo, 0, sizeof(ORDER_INFO));
	orderInfo->fieldFlags = 0x021E;
	orderInfo->deltaCoordinates = true;

	memset(&line_to, 0, sizeof(LINE_TO_ORDER));
	line_to.nXStart = 826;
	line_to.nYStart = 350;
	line_to.nXEnd = 829;
	line_to.nYEnd = 347;

	update_read_line_to_order(s, orderInfo, &line_to);

	CU_ASSERT(line_to.nXStart == 829);
	CU_ASSERT(line_to.nYStart == 271);
	CU_ASSERT(line_to.nXEnd == 843);
	CU_ASSERT(line_to.nYEnd == 257);
	CU_ASSERT(line_to.backColor == 0);
	CU_ASSERT(line_to.bRop2 == 0);
	CU_ASSERT(line_to.penStyle == 0);
	CU_ASSERT(line_to.penWidth == 0);
	CU_ASSERT(line_to.penColor == 0x00EF5B);

	CU_ASSERT(stream_get_length(s) == (sizeof(line_to_order) - 1));
}

uint8 polyline_order[] =
	"\xf8\x01\xb8\x02\x00\xc0\x00\x20\x6c\x00\x00\x00\x00\x00\x04\x00"
	"\x00\xff\x7e\x76\xff\x41\x6c\xff\x24\x62\xff\x2b\x59\xff\x55\x51"
	"\xff\x9c\x49\x73\x43\x80\x4d\xff\xbe\x80\x99\xff\xba\x80\xcd\xff"
	"\xb7\x80\xde\xff\xb6\x80\xca\xff\xb6\x80\x96\xff\xb7\x80\x48\xff"
	"\xba\x6f\xff\xbe\xff\x97\x43\xff\x52\x4a\xff\x2b\x51\xff\x24\x59"
	"\xff\x44\x63\xff\x81\x6c\x56\x76\x2f\x80\x82\x0a\x80\xbf\x14\x80"
	"\xdd\x1e\x80\xd4\x27\x80\xab\x2f\x80\x64\x37\x0d\x3d\xff\xb3\x80"
	"\x42\xff\x67\x80\x46";

void test_read_polyline_order(void)
{
	STREAM _s, *s;
	POLYLINE_ORDER polyline;

	s = &_s;
	s->p = s->data = polyline_order;

	memset(orderInfo, 0, sizeof(ORDER_INFO));
	orderInfo->fieldFlags = 0x73;

	memset(&polyline, 0, sizeof(POLYLINE_ORDER));

	update_read_polyline_order(s, orderInfo, &polyline);

	CU_ASSERT(polyline.xStart == 504);
	CU_ASSERT(polyline.yStart == 696);
	CU_ASSERT(polyline.bRop2 == 0);
	CU_ASSERT(polyline.penColor == 0x0000C000);
	CU_ASSERT(polyline.numPoints == 32);
	CU_ASSERT(polyline.cbData == 108);

	CU_ASSERT(polyline.points[0].x == -130);
	CU_ASSERT(polyline.points[1].x == -191);
	CU_ASSERT(polyline.points[2].x == -220);
	CU_ASSERT(polyline.points[3].x == -213);
	CU_ASSERT(polyline.points[4].x == -171);
	CU_ASSERT(polyline.points[5].x == -100);
	CU_ASSERT(polyline.points[6].x == -13);
	CU_ASSERT(polyline.points[7].x == 77);
	CU_ASSERT(polyline.points[8].x == 153);
	CU_ASSERT(polyline.points[9].x == 205);
	CU_ASSERT(polyline.points[10].x == 222);
	CU_ASSERT(polyline.points[11].x == 202);
	CU_ASSERT(polyline.points[12].x == 150);
	CU_ASSERT(polyline.points[13].x == 72);
	CU_ASSERT(polyline.points[14].x == -17);
	CU_ASSERT(polyline.points[15].x == -105);
	CU_ASSERT(polyline.points[16].x == -174);
	CU_ASSERT(polyline.points[17].x == -213);
	CU_ASSERT(polyline.points[18].x == -220);
	CU_ASSERT(polyline.points[19].x == -188);
	CU_ASSERT(polyline.points[20].x == -127);
	CU_ASSERT(polyline.points[21].x == -42);
	CU_ASSERT(polyline.points[22].x == 47);
	CU_ASSERT(polyline.points[23].x == 130);
	CU_ASSERT(polyline.points[24].x == 191);
	CU_ASSERT(polyline.points[25].x == 221);
	CU_ASSERT(polyline.points[26].x == 212);
	CU_ASSERT(polyline.points[27].x == 171);
	CU_ASSERT(polyline.points[28].x == 100);
	CU_ASSERT(polyline.points[29].x == 13);
	CU_ASSERT(polyline.points[30].x == -77);
	CU_ASSERT(polyline.points[31].x == -153);
	CU_ASSERT(polyline.points[32].x == 0);

	CU_ASSERT(stream_get_length(s) == (sizeof(polyline_order) - 1));
}

uint8 glyph_index_order_1[] =
	"\x6a\x02\x27\x38\x00\x39\x07\x3a\x06\x3b\x07\x3c\x06\x3d\x06\x18"
	"\x04\x1f\x06\x17\x02\x14\x04\x1b\x06\x19\x06\x45\x05\x18\x06\x1f"
	"\x06\x1f\x02\x14\x02\x46\x06\xff\x15\x24";

uint8 glyph_index_order_2[] =
	"\x00\xff\xff\xff\x0c\x02\x6e\x01\x4d\x02\x7b\x01\x09\x02\x6e\x01"
	"\xf6\x02\x7b\x01\x0c\x02\x79\x01\x03\xfe\x04\x00";

void test_read_glyph_index_order(void)
{
	STREAM _s, *s;
	GLYPH_INDEX_ORDER glyph_index;

	s = &_s;
	s->p = s->data = glyph_index_order_1;

	memset(orderInfo, 0, sizeof(ORDER_INFO));
	orderInfo->fieldFlags = 0x200100;
	orderInfo->deltaCoordinates = true;

	memset(&glyph_index, 0, sizeof(GLYPH_INDEX_ORDER));

	update_read_glyph_index_order(s, orderInfo, &glyph_index);

	CU_ASSERT(glyph_index.bkRight == 618);

	CU_ASSERT(stream_get_length(s) == (sizeof(glyph_index_order_1) - 1));

	s->p = s->data = glyph_index_order_2;

	memset(orderInfo, 0, sizeof(ORDER_INFO));
	orderInfo->fieldFlags = 0x383FE8;
	orderInfo->deltaCoordinates = true;

	memset(&glyph_index, 0, sizeof(GLYPH_INDEX_ORDER));

	update_read_glyph_index_order(s, orderInfo, &glyph_index);

	CU_ASSERT(glyph_index.fOpRedundant == 0);
	CU_ASSERT(glyph_index.foreColor == 0x00FFFFFF);
	CU_ASSERT(glyph_index.bkLeft == 524);
	CU_ASSERT(glyph_index.bkTop == 366);
	CU_ASSERT(glyph_index.bkRight == 589);
	CU_ASSERT(glyph_index.bkBottom == 379);
	CU_ASSERT(glyph_index.opLeft == 521);
	CU_ASSERT(glyph_index.opTop == 366);
	CU_ASSERT(glyph_index.opRight == 758);
	CU_ASSERT(glyph_index.opBottom == 379);
	CU_ASSERT(glyph_index.x == 524);
	CU_ASSERT(glyph_index.y == 377);

	CU_ASSERT(stream_get_length(s) == (sizeof(glyph_index_order_2) - 1));
}

uint8 fast_index_order[] =
	"\x07\x00\x03\xff\xff\x00\x74\x3b\x00\x0e\x00\x71\x00\x42\x00\x7e"
	"\x00\x00\x80\x7c\x00\x15\x00\x00\x01\x06\x02\x04\x03\x08\x05\x09"
	"\x06\x06\x06\x06\x07\x06\x08\x02\xff\x00\x12";

void test_read_fast_index_order(void)
{
	STREAM _s, *s;
	FAST_INDEX_ORDER fast_index;

	s = &_s;
	s->p = s->data = fast_index_order;

	memset(orderInfo, 0, sizeof(ORDER_INFO));
	orderInfo->fieldFlags = 0x70FF;

	memset(&fast_index, 0, sizeof(FAST_INDEX_ORDER));
	update_read_fast_index_order(s, orderInfo, &fast_index);

	CU_ASSERT(fast_index.cacheId == 7);
	CU_ASSERT(fast_index.flAccel == 3);
	CU_ASSERT(fast_index.ulCharInc == 0);
	CU_ASSERT(fast_index.backColor == 0x0000FFFF);
	CU_ASSERT(fast_index.foreColor == 0x00003B74);
	CU_ASSERT(fast_index.bkLeft == 14);
	CU_ASSERT(fast_index.bkTop == 113);
	CU_ASSERT(fast_index.bkRight == 66);
	CU_ASSERT(fast_index.bkBottom == 126);
	CU_ASSERT(fast_index.opLeft == 0);
	CU_ASSERT(fast_index.opTop == 0);
	CU_ASSERT(fast_index.opRight == 0);
	CU_ASSERT(fast_index.opBottom == 0);
	CU_ASSERT(fast_index.x == -32768);
	CU_ASSERT(fast_index.y == 124);

	CU_ASSERT(stream_get_length(s) == (sizeof(fast_index_order) - 1));
}

uint8 fast_glyph_order[] =
	"\x06\x00\x03\xff\xff\x00\x8b\x00\xb1\x00\x93\x00\xbe\x00\x0d\x00"
	"\xfe\x7f\x00\x80\x00\x80\xbb\x00\x13\x00\x01\x4a\x06\x0a\x80\x80"
	"\x80\xb8\xc4\x84\x84\x84\x84\x84\x00\x00\x68\x00";

void test_read_fast_glyph_order(void)
{
	STREAM _s, *s;
	FAST_GLYPH_ORDER fast_glyph;

	s = &_s;
	s->p = s->data = fast_glyph_order;

	memset(orderInfo, 0, sizeof(ORDER_INFO));
	orderInfo->fieldFlags = 0x7EFB;

	memset(&fast_glyph, 0, sizeof(FAST_GLYPH_ORDER));

	update_read_fast_glyph_order(s, orderInfo, &fast_glyph);

	CU_ASSERT(fast_glyph.backColor == 0);
	CU_ASSERT(fast_glyph.foreColor == 0x0000FFFF);
	CU_ASSERT(fast_glyph.bkLeft == 139);
	CU_ASSERT(fast_glyph.bkTop == 177);
	CU_ASSERT(fast_glyph.bkRight == 147);
	CU_ASSERT(fast_glyph.bkBottom == 190);
	CU_ASSERT(fast_glyph.opLeft == 0);
	CU_ASSERT(fast_glyph.opTop == 13);
	CU_ASSERT(fast_glyph.opRight == 32766);
	CU_ASSERT(fast_glyph.opBottom == -32768);
	CU_ASSERT(fast_glyph.x == -32768);
	CU_ASSERT(fast_glyph.y == 187);

	CU_ASSERT(stream_get_length(s) == (sizeof(fast_glyph_order) - 1));
}

uint8 polygon_cb_order[] =
	"\xea\x00\x46\x01\x0d\x01\x08\x00\x00\x04\x03\x81\x08\x03\x05\x88"
	"\x09\x26\x09\x77";

void test_read_polygon_cb_order(void)
{
	STREAM _s, *s;
	POLYGON_CB_ORDER polygon_cb;

	s = &_s;
	s->p = s->data = polygon_cb_order;

	memset(orderInfo, 0, sizeof(ORDER_INFO));
	orderInfo->fieldFlags = 0x1BEF;

	memset(&polygon_cb, 0, sizeof(POLYGON_CB_ORDER));

	update_read_polygon_cb_order(s, orderInfo, &polygon_cb);

	CU_ASSERT(polygon_cb.xStart == 234);
	CU_ASSERT(polygon_cb.yStart == 326);
	CU_ASSERT(polygon_cb.bRop2 == 0x0D);
	CU_ASSERT(polygon_cb.fillMode == 1);
	CU_ASSERT(polygon_cb.backColor == 0);
	CU_ASSERT(polygon_cb.foreColor == 0x00000008);
	CU_ASSERT(polygon_cb.brush.x == 4);
	CU_ASSERT(polygon_cb.brush.y == 3);
	CU_ASSERT(polygon_cb.brush.style == 0x81);
	CU_ASSERT(polygon_cb.nDeltaEntries == 3);
	CU_ASSERT(polygon_cb.cbData == 5);

	CU_ASSERT(stream_get_length(s) == (sizeof(polygon_cb_order) - 1));
}

uint8 cache_bitmap_order[] = "\x00\x00\x10\x01\x08\x01\x00\x00\x00\x10";

void test_read_cache_bitmap_order(void)
{
	STREAM _s, *s;
	uint16 extraFlags;
	CACHE_BITMAP_ORDER cache_bitmap;

	s = &_s;
	extraFlags = 0x0400;
	s->p = s->data = cache_bitmap_order;

	memset(&cache_bitmap, 0, sizeof(CACHE_BITMAP_ORDER));

	update_read_cache_bitmap_order(s, &cache_bitmap, true, extraFlags);

	CU_ASSERT(cache_bitmap.cacheId == 0);
	CU_ASSERT(cache_bitmap.bitmapWidth == 16);
	CU_ASSERT(cache_bitmap.bitmapHeight == 1);
	CU_ASSERT(cache_bitmap.bitmapBpp == 8);
	CU_ASSERT(cache_bitmap.bitmapLength == 1);
	CU_ASSERT(cache_bitmap.cacheIndex == 0);

	CU_ASSERT(stream_get_length(s) == (sizeof(cache_bitmap_order) - 1));
}

uint8 cache_bitmap_v2_order[] =
	"\x20\x40\xdc\xff\xff\x85\xff\xff\x99\xd6\x99\xd6\x99\xd6\x99\xd6"
	"\x06\x8b\x99\xd6\x99\xd6\x99\xd6\x10\x84\x08\x42\x08\x42\x10\x84"
	"\x99\xd6\x99\xd6\x99\xd6\x99\xd6\x06\x84\x99\xd6\x99\xd6\x99\xd6"
	"\xff\xff\x16\x69\x99\xd6\x06\x69\x99\xd6\x04\xcc\x89\x52\x03\x6e"
	"\xff\xff\x02\x6e\x08\x42\x01\x70\x08\x42\x71\xff\xff\xce\x18\xc6"
	"\x01\x81\x08\x42\xce\x66\x29\x02\xcd\x89\x52\x03\x88\x10\x84\x99"
	"\xd6\x99\xd6\x99\xd6\x00\x00\x00\x00\x00\x00\x00\x00\xd8\x99\xd6"
	"\x03\xf8\x01\x00\x00\x00\x00\xf0\x66\x99\xd6\x05\x6a\x99\xd6\x00"
	"\xc4\xcc\x89\x52\x03\x6e\xff\xff\x02\x6e\x08\x42\x01\x70\x08\x42"
	"\x71\xff\xff\xce\x18\xc6\x01\x81\x08\x42\xce\x66\x29\x02\xcd\x89"
	"\x52\x03\x00\x04\xd6\x99\xd6\xc3\x80\x61\x00\xa5\x80\x40\xec\x52"
	"\x00\x5a\x00\x2d\x00\x24\x00\x12\x00\x24\x00\x12\x00\x5a\x00\x2d"
	"\x00\xa5\x80\x52\x00\xc3\x80\x61\x00\x00\x00\x00\x00\xcc\x89\x52"
	"\x03\x6e\xff\xff\x02\xcb\x18\xc6\x84\x08\x42\x08\x42\x08\x42\xff"
	"\xff";

void test_read_cache_bitmap_v2_order(void)
{
	STREAM _s, *s;
	uint16 extraFlags;
	CACHE_BITMAP_V2_ORDER cache_bitmap_v2;

	s = &_s;
	extraFlags = 0x0CA1;
	s->p = s->data = cache_bitmap_v2_order;

	memset(&cache_bitmap_v2, 0, sizeof(CACHE_BITMAP_V2_ORDER));

	update_read_cache_bitmap_v2_order(s, &cache_bitmap_v2, true, extraFlags);

	CU_ASSERT(cache_bitmap_v2.cacheId == 1);
	CU_ASSERT(cache_bitmap_v2.bitmapBpp == 16);
	CU_ASSERT(cache_bitmap_v2.flags == 0x19);
	CU_ASSERT(cache_bitmap_v2.bitmapWidth == 32);
	CU_ASSERT(cache_bitmap_v2.bitmapHeight == 32);
	CU_ASSERT(cache_bitmap_v2.bitmapLength == 220);
	CU_ASSERT(cache_bitmap_v2.cacheIndex == 32767);

	CU_ASSERT(stream_get_length(s) == (sizeof(cache_bitmap_v2_order) - 1));
}

uint8 cache_bitmap_v3_order[] =
	"\xff\x7f\x35\x50\xec\xbc\x74\x52\x65\xb7\x20\x00\x00\x00\x05\x00"
	"\x02\x00\x28\x00\x00\x00\x5b\x4f\x45\xff\x5b\x4f\x45\xff\x5b\x4f"
	"\x45\xff\x5b\x4f\x45\xff\x5b\x4f\x45\xff\x5b\x50\x45\xff\x5b\x50"
	"\x45\xff\x5b\x50\x45\xff\x5b\x50\x45\xff\x5b\x50\x45\xff";

void test_read_cache_bitmap_v3_order(void)
{
	STREAM _s, *s;
	uint16 extraFlags;
	CACHE_BITMAP_V3_ORDER cache_bitmap_v3;

	s = &_s;
	extraFlags = 0x0C30;
	s->p = s->data = cache_bitmap_v3_order;

	memset(&cache_bitmap_v3, 0, sizeof(CACHE_BITMAP_V3_ORDER));

	update_read_cache_bitmap_v3_order(s, &cache_bitmap_v3, true, extraFlags);

	CU_ASSERT(cache_bitmap_v3.cacheIndex == 32767);
	CU_ASSERT(cache_bitmap_v3.key1 == 0xBCEC5035);
	CU_ASSERT(cache_bitmap_v3.key2 == 0xB7655274);
	CU_ASSERT(cache_bitmap_v3.bpp == 32);

	CU_ASSERT(cache_bitmap_v3.bitmapData.bpp == 32);
	CU_ASSERT(cache_bitmap_v3.bitmapData.codecID == 0);
	CU_ASSERT(cache_bitmap_v3.bitmapData.width == 5);
	CU_ASSERT(cache_bitmap_v3.bitmapData.height == 2);
	CU_ASSERT(cache_bitmap_v3.bitmapData.length == 40);

	CU_ASSERT(stream_get_length(s) == (sizeof(cache_bitmap_v3_order) - 1));
}

uint8 cache_brush_order[] = "\x00\x01\x08\x08\x81\x08\xaa\x55\xaa\x55\xaa\x55\xaa\x55";

void test_read_cache_brush_order(void)
{
	STREAM _s, *s;
	CACHE_BRUSH_ORDER cache_brush;

	s = &_s;
	s->p = s->data = cache_brush_order;

	memset(&cache_brush, 0, sizeof(CACHE_BRUSH_ORDER));

	update_read_cache_brush_order(s, &cache_brush, 0);

	CU_ASSERT(cache_brush.index == 0);
	CU_ASSERT(cache_brush.bpp == 1);
	CU_ASSERT(cache_brush.cx == 8);
	CU_ASSERT(cache_brush.cy == 8);
	CU_ASSERT(cache_brush.style == 0x81);
	CU_ASSERT(cache_brush.length == 8);

	CU_ASSERT(stream_get_length(s) == (sizeof(cache_brush_order) - 1));
}

uint8 create_offscreen_bitmap_order[] = "\x00\x80\x60\x01\x10\x00\x01\x00\x02\x00";

void test_read_create_offscreen_bitmap_order(void)
{
	STREAM _s, *s;
	OFFSCREEN_DELETE_LIST* deleteList;
	CREATE_OFFSCREEN_BITMAP_ORDER create_offscreen_bitmap;

	s = &_s;
	s->p = s->data = create_offscreen_bitmap_order;

	memset(&create_offscreen_bitmap, 0, sizeof(CREATE_OFFSCREEN_BITMAP_ORDER));

	deleteList = &(create_offscreen_bitmap.deleteList);
	deleteList->cIndices = 0;
	deleteList->sIndices = 16;
	deleteList->indices = malloc(sizeof(uint16) * deleteList->sIndices);

	update_read_create_offscreen_bitmap_order(s, &create_offscreen_bitmap);

	CU_ASSERT(create_offscreen_bitmap.id == 0);
	CU_ASSERT(create_offscreen_bitmap.cx == 352);
	CU_ASSERT(create_offscreen_bitmap.cy == 16);
	CU_ASSERT(create_offscreen_bitmap.deleteList.cIndices == 1);

	CU_ASSERT(stream_get_length(s) == (sizeof(create_offscreen_bitmap_order) - 1));
}

uint8 switch_surface_order[] = "\xff\xff";

void test_read_switch_surface_order(void)
{
	STREAM _s, *s;
	SWITCH_SURFACE_ORDER switch_surface;

	s = &_s;
	s->p = s->data = switch_surface_order;

	memset(&switch_surface, 0, sizeof(SWITCH_SURFACE_ORDER));

	update_read_switch_surface_order(s, &switch_surface);

	CU_ASSERT(switch_surface.bitmapId == 0xFFFF);

	CU_ASSERT(stream_get_length(s) == (sizeof(switch_surface_order) - 1));
}

int opaque_rect_count;
int polyline_count;
int patblt_count;

uint8 orders_update_1[] =
	"\x00\x00\x33\xd0\x07\x00\x80\xba\x0d\x0a\x7f\x1e\x2c\x4d\x00\x36"
	"\x02\xd3\x00\x47\x00\x4d\x00\xf0\x01\x87\x00\xc2\xdc\xff\x05\x7f"
	"\x0f\x67\x01\x90\x01\x8e\x01\xa5\x01\x67\x01\x90\x01\x28\x00\x16"
	"\x00\xf0\xf0\xf0\x15\x0f\xf0\x2d\x01\x19\xfe\x2d\x01\xec\xfd\x0d"
	"\x16\x77\xf0\xff\xff\x01\x01\xa8\x01\x90\x01\x0d\xf0\xf0\xf0\x04"
	"\x05\x66\x6b\x14\x15\x6c\x1d\x0a\x0f\xd0\x16\x64\x01\x15\xff\x50"
	"\x03\x15\x0f\xf0\x65\x01\x15\xfe\x65\x01\xb0\xfd\x1d\x16\x01\xf0"
	"\xff\xff\x01\x01\x7a";

uint8 orders_update_2[] =
	"\x00\x00\x45\x62\x03\x00\x93\x14\x55\x01\x50\xff\xff\xff\x55\x01"
	"\x50\x01\x01\x01\x55\x01\x50\xff\xff\xff\x16\x00\x17\x00\xea\x03"
	"\xea\x03\x02\x00\x85\x02\x16\x00\x02\x00\x00\x00\x03\x00\x14\xb2";

void test_opaque_rect(rdpContext* context, OPAQUE_RECT_ORDER* opaque_rect)
{
	opaque_rect_count++;
}

void test_polyline(rdpContext* context, POLYLINE_ORDER* polyline)
{
	polyline_count++;
}

void test_patblt(rdpContext* context, PATBLT_ORDER* patblt)
{
	patblt_count++;
}

void test_update_recv_orders(void)
{
	rdpRdp* rdp;
	STREAM _s, *s;
	rdpUpdate* update;

	s = &_s;
	rdp = rdp_new(NULL);
	update = update_new(rdp);

	update->context = malloc(sizeof(rdpContext));
	update->context->rdp = rdp;

	opaque_rect_count = 0;
	polyline_count = 0;
	patblt_count = 0;

	update->primary->OpaqueRect = test_opaque_rect;
	update->primary->Polyline = test_polyline;
	update->primary->PatBlt = test_patblt;

	s->p = s->data = orders_update_1;
	s->size = sizeof(orders_update_1);

	update_recv(update, s);

	CU_ASSERT(opaque_rect_count == 5);
	CU_ASSERT(polyline_count == 2);

	update->primary->order_info.orderType = ORDER_TYPE_PATBLT;
	s->p = s->data = orders_update_2;
	s->size = sizeof(orders_update_2);

	update_recv(update, s);

	CU_ASSERT(patblt_count == 3);

	free(update->context);
}

