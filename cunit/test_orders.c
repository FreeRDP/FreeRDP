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

ORDER_INFO* orderInfo;

int init_orders_suite(void)
{
	orderInfo = (ORDER_INFO*) malloc(sizeof(orderInfo));
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
	add_test_function(read_line_to_order);
	add_test_function(read_glyph_index_order);

	return 0;
}

uint8 dstblt_order[] = "\x48\x00\x37\x01";

void test_read_dstblt_order(void)
{
	STREAM* s;
	DSTBLT_ORDER dstblt;

	s = stream_new(0);
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
}

uint8 patblt_order[] = "\x1a\x00\xc3\x01\x0d\x00\x0d\x00\xf0\xff\xff\x00\x5b\xef\x00\x81";

void test_read_patblt_order(void)
{
	STREAM* s;
	PATBLT_ORDER patblt;

	s = stream_new(0);
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
	CU_ASSERT(patblt.brushOrgX == 0);
	CU_ASSERT(patblt.brushOrgY == 0);
	CU_ASSERT(patblt.brushStyle == (BMF_1BPP | CACHED_BRUSH));
}

uint8 scrblt_order[] = "\x07\x00\xa1\x01\xf1\x00\xcc\x2f\x01\x8e";

void test_read_scrblt_order(void)
{
	STREAM* s;
	SCRBLT_ORDER scrblt;

	s = stream_new(0);
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
}

uint8 opaque_rect_order[] = "\x00\x04\x00\x03\x73\x02\x06";

void test_read_opaque_rect_order(void)
{
	STREAM* s;
	OPAQUE_RECT_ORDER opaque_rect;

	s = stream_new(0);
	s->p = s->data = opaque_rect_order;

	memset(orderInfo, 0, sizeof(ORDER_INFO));
	orderInfo->fieldFlags = 0x3C;
	memset(&opaque_rect, 0, sizeof(OPAQUE_RECT_ORDER));

	update_read_opaque_rect_order(s, orderInfo, &opaque_rect);

	CU_ASSERT(opaque_rect.nLeftRect == 0);
	CU_ASSERT(opaque_rect.nTopRect == 0);
	CU_ASSERT(opaque_rect.nWidth == 1024);
	CU_ASSERT(opaque_rect.nHeight == 768);
}

uint8 draw_nine_grid_order[] = "\xfb\xf9\x0d";

void test_read_draw_nine_grid_order(void)
{
	STREAM* s;
	DRAW_NINE_GRID_ORDER draw_nine_grid;

	s = stream_new(0);
	s->p = s->data = draw_nine_grid_order;

	memset(orderInfo, 0, sizeof(ORDER_INFO));
	orderInfo->fieldFlags = 0x1C;
	orderInfo->boundLeft = 925;
	orderInfo->boundTop = 134;
	orderInfo->boundRight = 846;
	orderInfo->boundBottom = 155;
	orderInfo->deltaCoordinates = True;

	memset(&draw_nine_grid, 0, sizeof(DRAW_NINE_GRID_ORDER));
	draw_nine_grid.srcRight = 38;
	draw_nine_grid.srcBottom = 40;

	update_read_draw_nine_grid_order(s, orderInfo, &draw_nine_grid);

	CU_ASSERT(draw_nine_grid.srcLeft == 0);
	CU_ASSERT(draw_nine_grid.srcTop == 0);
	CU_ASSERT(draw_nine_grid.srcRight == 33);
	CU_ASSERT(draw_nine_grid.srcBottom == 33);
	CU_ASSERT(draw_nine_grid.bitmapId == 13);
}

uint8 line_to_order[] = "\x03\xb1\x0e\xa6\x5b\xef\x00";

void test_read_line_to_order(void)
{
	STREAM* s;
	LINE_TO_ORDER line_to;

	s = stream_new(0);
	s->p = s->data = line_to_order;

	memset(orderInfo, 0, sizeof(ORDER_INFO));
	orderInfo->fieldFlags = 0x021E;
	orderInfo->boundLeft = 829;
	orderInfo->boundTop = 257;
	orderInfo->boundRight = 842;
	orderInfo->boundBottom = 270;
	orderInfo->deltaCoordinates = True;

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
	STREAM* s;
	GLYPH_INDEX_ORDER glyph_index;

	s = stream_new(0);
	s->p = s->data = glyph_index_order_1;

	memset(orderInfo, 0, sizeof(ORDER_INFO));
	orderInfo->fieldFlags = 0x200100;
	orderInfo->deltaCoordinates = True;

	memset(&glyph_index, 0, sizeof(GLYPH_INDEX_ORDER));

	update_read_glyph_index_order(s, orderInfo, &glyph_index);

	CU_ASSERT(glyph_index.bkRight == 618);

	s->p = s->data = glyph_index_order_2;

	memset(orderInfo, 0, sizeof(ORDER_INFO));
	orderInfo->fieldFlags = 0x383FE8;
	orderInfo->deltaCoordinates = True;

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
}
