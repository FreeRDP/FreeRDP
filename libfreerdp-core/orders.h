/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Drawing Orders
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

#ifndef __ORDERS_H
#define __ORDERS_H

#include "rdp.h"
#include <freerdp/types.h>
#include <freerdp/update.h>
#include <freerdp/utils/stream.h>

/* Order Control Flags */
#define ORDER_STANDARD				0x01
#define ORDER_SECONDARY				0x02
#define ORDER_BOUNDS				0x04
#define ORDER_TYPE_CHANGE			0x08
#define ORDER_DELTA_COORDINATES			0x10
#define ORDER_ZERO_BOUNDS_DELTAS		0x20
#define ORDER_ZERO_FIELD_BYTE_BIT0		0x40
#define ORDER_ZERO_FIELD_BYTE_BIT1		0x80

/* Bound Field Flags */
#define BOUND_LEFT				0x01
#define BOUND_TOP				0x02
#define BOUND_RIGHT				0x04
#define BOUND_BOTTOM				0x08
#define BOUND_DELTA_LEFT			0x10
#define BOUND_DELTA_TOP				0x20
#define BOUND_DELTA_RIGHT			0x40
#define BOUND_DELTA_BOTTOM			0x80

/* Field Presence Flags */
#define ORDER_FIELD_01				0x000001
#define ORDER_FIELD_02				0x000002
#define ORDER_FIELD_03				0x000004
#define ORDER_FIELD_04				0x000008
#define ORDER_FIELD_05				0x000010
#define ORDER_FIELD_06				0x000020
#define ORDER_FIELD_07				0x000040
#define ORDER_FIELD_08				0x000080
#define ORDER_FIELD_09				0x000100
#define ORDER_FIELD_10				0x000200
#define ORDER_FIELD_11				0x000400
#define ORDER_FIELD_12				0x000800
#define ORDER_FIELD_13				0x001000
#define ORDER_FIELD_14				0x002000
#define ORDER_FIELD_15				0x004000
#define ORDER_FIELD_16				0x008000
#define ORDER_FIELD_17				0x010000
#define ORDER_FIELD_18				0x020000
#define ORDER_FIELD_19				0x040000
#define ORDER_FIELD_20				0x080000
#define ORDER_FIELD_21				0x100000
#define ORDER_FIELD_22				0x200000
#define ORDER_FIELD_23				0x400000

/* Bitmap Cache Flags */
#define CBR2_8BPP			0x3
#define CBR2_16BPP			0x4
#define CBR2_24BPP			0x5
#define CBR2_32BPP			0x6

#define CBR23_8BPP			0x3
#define CBR23_16BPP			0x4
#define CBR23_24BPP			0x5
#define CBR23_32BPP			0x6

#define CBR3_IGNORABLE_FLAG		0x08
#define CBR3_DO_NOT_CACHE		0x10

/* Primary Drawing Orders */
#define ORDER_TYPE_DSTBLT			0x00
#define ORDER_TYPE_PATBLT			0x01
#define ORDER_TYPE_SCRBLT			0x02
#define ORDER_TYPE_DRAW_NINE_GRID		0x07
#define ORDER_TYPE_MULTI_DRAW_NINE_GRID		0x08
#define ORDER_TYPE_LINE_TO			0x09
#define ORDER_TYPE_OPAQUE_RECT			0x0A
#define ORDER_TYPE_SAVE_BITMAP			0x0B
#define ORDER_TYPE_MEMBLT			0x0D
#define ORDER_TYPE_MEM3BLT			0x0E
#define ORDER_TYPE_MULTI_DSTBLT			0x0F
#define ORDER_TYPE_MULTI_PATBLT			0x10
#define ORDER_TYPE_MULTI_SCRBLT			0x11
#define ORDER_TYPE_MULTI_OPAQUE_RECT		0x12
#define ORDER_TYPE_FAST_INDEX			0x13
#define ORDER_TYPE_POLYGON_SC			0x14
#define ORDER_TYPE_POLYGON_CB			0x15
#define ORDER_TYPE_POLYLINE			0x16
#define ORDER_TYPE_FAST_GLYPH			0x18
#define ORDER_TYPE_ELLIPSE_SC			0x19
#define ORDER_TYPE_ELLIPSE_CB			0x1A
#define ORDER_TYPE_GLYPH_INDEX			0x1B

/* Primary Drawing Orders Fields */
#define DSTBLT_ORDER_FIELDS			5
#define PATBLT_ORDER_FIELDS			12
#define SCRBLT_ORDER_FIELDS			7
#define DRAW_NINE_GRID_ORDER_FIELDS		5
#define MULTI_DRAW_NINE_GRID_ORDER_FIELDS	7
#define LINE_TO_ORDER_FIELDS			10
#define OPAQUE_RECT_ORDER_FIELDS		7
#define SAVE_BITMAP_ORDER_FIELDS		6
#define MEMBLT_ORDER_FIELDS			9
#define MEM3BLT_ORDER_FIELDS			16
#define MULTI_DSTBLT_ORDER_FIELDS		7
#define MULTI_PATBLT_ORDER_FIELDS		14
#define MULTI_SCRBLT_ORDER_FIELDS		9
#define MULTI_OPAQUE_RECT_ORDER_FIELDS		9
#define FAST_INDEX_ORDER_FIELDS			15
#define POLYGON_SC_ORDER_FIELDS			7
#define POLYGON_CB_ORDER_FIELDS			13
#define POLYLINE_ORDER_FIELDS			7
#define FAST_GLYPH_ORDER_FIELDS			15
#define ELLIPSE_SC_ORDER_FIELDS			7
#define ELLIPSE_CB_ORDER_FIELDS			13
#define GLYPH_INDEX_ORDER_FIELDS		22

/* Primary Drawing Orders Field Bytes */
#define DSTBLT_ORDER_FIELD_BYTES		1
#define PATBLT_ORDER_FIELD_BYTES		2
#define SCRBLT_ORDER_FIELD_BYTES		1
#define DRAW_NINE_GRID_ORDER_FIELD_BYTES	1
#define MULTI_DRAW_NINE_GRID_ORDER_FIELD_BYTES	1
#define LINE_TO_ORDER_FIELD_BYTES		2
#define OPAQUE_RECT_ORDER_FIELD_BYTES		1
#define SAVE_BITMAP_ORDER_FIELD_BYTES		1
#define MEMBLT_ORDER_FIELD_BYTES		2
#define MEM3BLT_ORDER_FIELD_BYTES		3
#define MULTI_DSTBLT_ORDER_FIELD_BYTES		1
#define MULTI_PATBLT_ORDER_FIELD_BYTES		2
#define MULTI_SCRBLT_ORDER_FIELD_BYTES		2
#define MULTI_OPAQUE_RECT_ORDER_FIELD_BYTES	2
#define FAST_INDEX_ORDER_FIELD_BYTES		2
#define POLYGON_SC_ORDER_FIELD_BYTES		1
#define POLYGON_CB_ORDER_FIELD_BYTES		2
#define POLYLINE_ORDER_FIELD_BYTES		1
#define FAST_GLYPH_ORDER_FIELD_BYTES		2
#define ELLIPSE_SC_ORDER_FIELD_BYTES		1
#define ELLIPSE_CB_ORDER_FIELD_BYTES		2
#define GLYPH_INDEX_ORDER_FIELD_BYTES		3

/* Secondary Drawing Orders */
#define ORDER_TYPE_BITMAP_UNCOMPRESSED		0x00
#define ORDER_TYPE_CACHE_COLOR_TABLE		0x01
#define ORDER_TYPE_CACHE_BITMAP_COMPRESSED	0x02
#define ORDER_TYPE_CACHE_GLYPH			0x03
#define ORDER_TYPE_BITMAP_UNCOMPRESSED_V2	0x04
#define ORDER_TYPE_BITMAP_COMPRESSED_V2		0x05
#define ORDER_TYPE_CACHE_BRUSH			0x07
#define ORDER_TYPE_BITMAP_COMPRESSED_V3		0x08

/* Alternate Secondary Drawing Orders */
#define ORDER_TYPE_SWITCH_SURFACE		0x00
#define ORDER_TYPE_CREATE_OFFSCREEN_BITMAP	0x01
#define ORDER_TYPE_STREAM_BITMAP_FIRST		0x02
#define ORDER_TYPE_STREAM_BITMAP_NEXT		0x03
#define ORDER_TYPE_CREATE_NINE_GRID_BITMAP	0x04
#define ORDER_TYPE_GDIPLUS_FIRST		0x05
#define ORDER_TYPE_GDIPLUS_NEXT			0x06
#define ORDER_TYPE_GDIPLUS_END			0x07
#define ORDER_TYPE_GDIPLUS_CACHE_FIRST		0x08
#define ORDER_TYPE_GDIPLUS_CACHE_NEXT		0x09
#define ORDER_TYPE_GDIPLUS_CACHE_END		0x0A
#define ORDER_TYPE_WINDOW			0x0B
#define ORDER_TYPE_COMPDESK_FIRST		0x0C
#define ORDER_TYPE_FRAME_MARKER			0x0D

#define CG_GLYPH_UNICODE_PRESENT		0x0010

void update_recv_order(rdpUpdate* update, STREAM* s);

void update_read_dstblt_order(STREAM* s, ORDER_INFO* orderInfo, DSTBLT_ORDER* dstblt);
void update_read_patblt_order(STREAM* s, ORDER_INFO* orderInfo, PATBLT_ORDER* patblt);
void update_read_scrblt_order(STREAM* s, ORDER_INFO* orderInfo, SCRBLT_ORDER* scrblt);
void update_read_opaque_rect_order(STREAM* s, ORDER_INFO* orderInfo, OPAQUE_RECT_ORDER* opaque_rect);
void update_read_draw_nine_grid_order(STREAM* s, ORDER_INFO* orderInfo, DRAW_NINE_GRID_ORDER* draw_nine_grid);
void update_read_multi_dstblt_order(STREAM* s, ORDER_INFO* orderInfo, MULTI_DSTBLT_ORDER* multi_dstblt);
void update_read_multi_patblt_order(STREAM* s, ORDER_INFO* orderInfo, MULTI_PATBLT_ORDER* multi_patblt);
void update_read_multi_scrblt_order(STREAM* s, ORDER_INFO* orderInfo, MULTI_SCRBLT_ORDER* multi_scrblt);
void update_read_multi_opaque_rect_order(STREAM* s, ORDER_INFO* orderInfo, MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect);
void update_read_multi_draw_nine_grid_order(STREAM* s, ORDER_INFO* orderInfo, MULTI_DRAW_NINE_GRID_ORDER* multi_draw_nine_grid);
void update_read_line_to_order(STREAM* s, ORDER_INFO* orderInfo, LINE_TO_ORDER* line_to);
void update_read_polyline_order(STREAM* s, ORDER_INFO* orderInfo, POLYLINE_ORDER* polyline);
void update_read_memblt_order(STREAM* s, ORDER_INFO* orderInfo, MEMBLT_ORDER* memblt);
void update_read_mem3blt_order(STREAM* s, ORDER_INFO* orderInfo, MEM3BLT_ORDER* mem3blt);
void update_read_save_bitmap_order(STREAM* s, ORDER_INFO* orderInfo, SAVE_BITMAP_ORDER* save_bitmap);
void update_read_glyph_index_order(STREAM* s, ORDER_INFO* orderInfo, GLYPH_INDEX_ORDER* glyph_index);
void update_read_fast_index_order(STREAM* s, ORDER_INFO* orderInfo, FAST_INDEX_ORDER* fast_index);
void update_read_fast_glyph_order(STREAM* s, ORDER_INFO* orderInfo, FAST_GLYPH_ORDER* fast_glyph);
void update_read_polygon_sc_order(STREAM* s, ORDER_INFO* orderInfo, POLYGON_SC_ORDER* polygon_sc);
void update_read_polygon_cb_order(STREAM* s, ORDER_INFO* orderInfo, POLYGON_CB_ORDER* polygon_cb);
void update_read_ellipse_sc_order(STREAM* s, ORDER_INFO* orderInfo, ELLIPSE_SC_ORDER* ellipse_sc);
void update_read_ellipse_cb_order(STREAM* s, ORDER_INFO* orderInfo, ELLIPSE_CB_ORDER* ellipse_cb);

void update_read_cache_bitmap_order(STREAM* s, CACHE_BITMAP_ORDER* cache_bitmap_order, boolean compressed, uint16 flags);
void update_read_cache_bitmap_v2_order(STREAM* s, CACHE_BITMAP_V2_ORDER* cache_bitmap_v2_order, boolean compressed, uint16 flags);
void update_read_cache_bitmap_v3_order(STREAM* s, CACHE_BITMAP_V3_ORDER* cache_bitmap_v3_order, boolean compressed, uint16 flags);
void update_read_cache_color_table_order(STREAM* s, CACHE_COLOR_TABLE_ORDER* cache_color_table_order, uint16 flags);
void update_read_cache_glyph_order(STREAM* s, CACHE_GLYPH_ORDER* cache_glyph_order, uint16 flags);
void update_read_cache_glyph_v2_order(STREAM* s, CACHE_GLYPH_V2_ORDER* cache_glyph_v2_order, uint16 flags);
void update_read_cache_brush_order(STREAM* s, CACHE_BRUSH_ORDER* cache_brush_order, uint16 flags);

void update_read_create_offscreen_bitmap_order(STREAM* s, CREATE_OFFSCREEN_BITMAP_ORDER* create_offscreen_bitmap);
void update_read_switch_surface_order(STREAM* s, SWITCH_SURFACE_ORDER* switch_surface);
void update_read_create_nine_grid_bitmap_order(STREAM* s, CREATE_NINE_GRID_BITMAP_ORDER* create_nine_grid_bitmap);
void update_read_frame_marker_order(STREAM* s, FRAME_MARKER_ORDER* frame_marker);
void update_read_stream_bitmap_first_order(STREAM* s, STREAM_BITMAP_FIRST_ORDER* stream_bitmap_first);
void update_read_stream_bitmap_next_order(STREAM* s, STREAM_BITMAP_FIRST_ORDER* stream_bitmap_next);
void update_read_draw_gdiplus_first_order(STREAM* s, DRAW_GDIPLUS_FIRST_ORDER* draw_gdiplus_first);
void update_read_draw_gdiplus_next_order(STREAM* s, DRAW_GDIPLUS_NEXT_ORDER* draw_gdiplus_next);
void update_read_draw_gdiplus_end_order(STREAM* s, DRAW_GDIPLUS_END_ORDER* draw_gdiplus_end);
void update_read_draw_gdiplus_cache_first_order(STREAM* s, DRAW_GDIPLUS_CACHE_FIRST_ORDER* draw_gdiplus_cache_first);
void update_read_draw_gdiplus_cache_next_order(STREAM* s, DRAW_GDIPLUS_CACHE_NEXT_ORDER* draw_gdiplus_cache_next);
void update_read_draw_gdiplus_cache_end_order(STREAM* s, DRAW_GDIPLUS_CACHE_END_ORDER* draw_gdiplus_cache_end);

#endif /* __ORDERS_H */
