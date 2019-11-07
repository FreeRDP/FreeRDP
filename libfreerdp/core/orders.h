/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Drawing Orders
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_LIB_CORE_ORDERS_H
#define FREERDP_LIB_CORE_ORDERS_H

#include "rdp.h"

#include <freerdp/types.h>
#include <freerdp/update.h>
#include <freerdp/api.h>

#include <winpr/stream.h>

/* Order Control Flags */
#define ORDER_STANDARD 0x01
#define ORDER_SECONDARY 0x02
#define ORDER_BOUNDS 0x04
#define ORDER_TYPE_CHANGE 0x08
#define ORDER_DELTA_COORDINATES 0x10
#define ORDER_ZERO_BOUNDS_DELTAS 0x20
#define ORDER_ZERO_FIELD_BYTE_BIT0 0x40
#define ORDER_ZERO_FIELD_BYTE_BIT1 0x80

/* Bound Field Flags */
#define BOUND_LEFT 0x01
#define BOUND_TOP 0x02
#define BOUND_RIGHT 0x04
#define BOUND_BOTTOM 0x08
#define BOUND_DELTA_LEFT 0x10
#define BOUND_DELTA_TOP 0x20
#define BOUND_DELTA_RIGHT 0x40
#define BOUND_DELTA_BOTTOM 0x80

/* Field Presence Flags */
#define ORDER_FIELD_01 0x000001
#define ORDER_FIELD_02 0x000002
#define ORDER_FIELD_03 0x000004
#define ORDER_FIELD_04 0x000008
#define ORDER_FIELD_05 0x000010
#define ORDER_FIELD_06 0x000020
#define ORDER_FIELD_07 0x000040
#define ORDER_FIELD_08 0x000080
#define ORDER_FIELD_09 0x000100
#define ORDER_FIELD_10 0x000200
#define ORDER_FIELD_11 0x000400
#define ORDER_FIELD_12 0x000800
#define ORDER_FIELD_13 0x001000
#define ORDER_FIELD_14 0x002000
#define ORDER_FIELD_15 0x004000
#define ORDER_FIELD_16 0x008000
#define ORDER_FIELD_17 0x010000
#define ORDER_FIELD_18 0x020000
#define ORDER_FIELD_19 0x040000
#define ORDER_FIELD_20 0x080000
#define ORDER_FIELD_21 0x100000
#define ORDER_FIELD_22 0x200000
#define ORDER_FIELD_23 0x400000

/* Bitmap Cache Flags */
#define CBR2_8BPP 0x3
#define CBR2_16BPP 0x4
#define CBR2_24BPP 0x5
#define CBR2_32BPP 0x6

#define CBR23_8BPP 0x3
#define CBR23_16BPP 0x4
#define CBR23_24BPP 0x5
#define CBR23_32BPP 0x6

#define CBR3_IGNORABLE_FLAG 0x08
#define CBR3_DO_NOT_CACHE 0x10

/* Primary Drawing Orders */
#define ORDER_TYPE_DSTBLT 0x00
#define ORDER_TYPE_PATBLT 0x01
#define ORDER_TYPE_SCRBLT 0x02
#define ORDER_TYPE_DRAW_NINE_GRID 0x07
#define ORDER_TYPE_MULTI_DRAW_NINE_GRID 0x08
#define ORDER_TYPE_LINE_TO 0x09
#define ORDER_TYPE_OPAQUE_RECT 0x0A
#define ORDER_TYPE_SAVE_BITMAP 0x0B
#define ORDER_TYPE_MEMBLT 0x0D
#define ORDER_TYPE_MEM3BLT 0x0E
#define ORDER_TYPE_MULTI_DSTBLT 0x0F
#define ORDER_TYPE_MULTI_PATBLT 0x10
#define ORDER_TYPE_MULTI_SCRBLT 0x11
#define ORDER_TYPE_MULTI_OPAQUE_RECT 0x12
#define ORDER_TYPE_FAST_INDEX 0x13
#define ORDER_TYPE_POLYGON_SC 0x14
#define ORDER_TYPE_POLYGON_CB 0x15
#define ORDER_TYPE_POLYLINE 0x16
#define ORDER_TYPE_FAST_GLYPH 0x18
#define ORDER_TYPE_ELLIPSE_SC 0x19
#define ORDER_TYPE_ELLIPSE_CB 0x1A
#define ORDER_TYPE_GLYPH_INDEX 0x1B

/* Primary Drawing Orders Fields */
#define DSTBLT_ORDER_FIELDS 5
#define PATBLT_ORDER_FIELDS 12
#define SCRBLT_ORDER_FIELDS 7
#define DRAW_NINE_GRID_ORDER_FIELDS 5
#define MULTI_DRAW_NINE_GRID_ORDER_FIELDS 7
#define LINE_TO_ORDER_FIELDS 10
#define OPAQUE_RECT_ORDER_FIELDS 7
#define SAVE_BITMAP_ORDER_FIELDS 6
#define MEMBLT_ORDER_FIELDS 9
#define MEM3BLT_ORDER_FIELDS 16
#define MULTI_DSTBLT_ORDER_FIELDS 7
#define MULTI_PATBLT_ORDER_FIELDS 14
#define MULTI_SCRBLT_ORDER_FIELDS 9
#define MULTI_OPAQUE_RECT_ORDER_FIELDS 9
#define FAST_INDEX_ORDER_FIELDS 15
#define POLYGON_SC_ORDER_FIELDS 7
#define POLYGON_CB_ORDER_FIELDS 13
#define POLYLINE_ORDER_FIELDS 7
#define FAST_GLYPH_ORDER_FIELDS 15
#define ELLIPSE_SC_ORDER_FIELDS 7
#define ELLIPSE_CB_ORDER_FIELDS 13
#define GLYPH_INDEX_ORDER_FIELDS 22

/* Primary Drawing Orders Field Bytes */
#define DSTBLT_ORDER_FIELD_BYTES 1
#define PATBLT_ORDER_FIELD_BYTES 2
#define SCRBLT_ORDER_FIELD_BYTES 1
#define DRAW_NINE_GRID_ORDER_FIELD_BYTES 1
#define MULTI_DRAW_NINE_GRID_ORDER_FIELD_BYTES 1
#define LINE_TO_ORDER_FIELD_BYTES 2
#define OPAQUE_RECT_ORDER_FIELD_BYTES 1
#define SAVE_BITMAP_ORDER_FIELD_BYTES 1
#define MEMBLT_ORDER_FIELD_BYTES 2
#define MEM3BLT_ORDER_FIELD_BYTES 3
#define MULTI_DSTBLT_ORDER_FIELD_BYTES 1
#define MULTI_PATBLT_ORDER_FIELD_BYTES 2
#define MULTI_SCRBLT_ORDER_FIELD_BYTES 2
#define MULTI_OPAQUE_RECT_ORDER_FIELD_BYTES 2
#define FAST_INDEX_ORDER_FIELD_BYTES 2
#define POLYGON_SC_ORDER_FIELD_BYTES 1
#define POLYGON_CB_ORDER_FIELD_BYTES 2
#define POLYLINE_ORDER_FIELD_BYTES 1
#define FAST_GLYPH_ORDER_FIELD_BYTES 2
#define ELLIPSE_SC_ORDER_FIELD_BYTES 1
#define ELLIPSE_CB_ORDER_FIELD_BYTES 2
#define GLYPH_INDEX_ORDER_FIELD_BYTES 3

/* Secondary Drawing Orders */
#define ORDER_TYPE_BITMAP_UNCOMPRESSED 0x00
#define ORDER_TYPE_CACHE_COLOR_TABLE 0x01
#define ORDER_TYPE_CACHE_BITMAP_COMPRESSED 0x02
#define ORDER_TYPE_CACHE_GLYPH 0x03
#define ORDER_TYPE_BITMAP_UNCOMPRESSED_V2 0x04
#define ORDER_TYPE_BITMAP_COMPRESSED_V2 0x05
#define ORDER_TYPE_CACHE_BRUSH 0x07
#define ORDER_TYPE_BITMAP_COMPRESSED_V3 0x08

/* Alternate Secondary Drawing Orders */
#define ORDER_TYPE_SWITCH_SURFACE 0x00
#define ORDER_TYPE_CREATE_OFFSCREEN_BITMAP 0x01
#define ORDER_TYPE_STREAM_BITMAP_FIRST 0x02
#define ORDER_TYPE_STREAM_BITMAP_NEXT 0x03
#define ORDER_TYPE_CREATE_NINE_GRID_BITMAP 0x04
#define ORDER_TYPE_GDIPLUS_FIRST 0x05
#define ORDER_TYPE_GDIPLUS_NEXT 0x06
#define ORDER_TYPE_GDIPLUS_END 0x07
#define ORDER_TYPE_GDIPLUS_CACHE_FIRST 0x08
#define ORDER_TYPE_GDIPLUS_CACHE_NEXT 0x09
#define ORDER_TYPE_GDIPLUS_CACHE_END 0x0A
#define ORDER_TYPE_WINDOW 0x0B
#define ORDER_TYPE_COMPDESK_FIRST 0x0C
#define ORDER_TYPE_FRAME_MARKER 0x0D

#define CG_GLYPH_UNICODE_PRESENT 0x0010

FREERDP_LOCAL extern const BYTE PRIMARY_DRAWING_ORDER_FIELD_BYTES[];

FREERDP_LOCAL BOOL update_recv_order(rdpUpdate* update, wStream* s);

FREERDP_LOCAL BOOL update_write_field_flags(wStream* s, UINT32 fieldFlags, BYTE flags,
                                            BYTE fieldBytes);

FREERDP_LOCAL BOOL update_write_bounds(wStream* s, ORDER_INFO* orderInfo);

FREERDP_LOCAL int update_approximate_dstblt_order(ORDER_INFO* orderInfo,
                                                  const DSTBLT_ORDER* dstblt);
FREERDP_LOCAL BOOL update_write_dstblt_order(wStream* s, ORDER_INFO* orderInfo,
                                             const DSTBLT_ORDER* dstblt);

FREERDP_LOCAL int update_approximate_patblt_order(ORDER_INFO* orderInfo, PATBLT_ORDER* patblt);
FREERDP_LOCAL BOOL update_write_patblt_order(wStream* s, ORDER_INFO* orderInfo,
                                             PATBLT_ORDER* patblt);

FREERDP_LOCAL int update_approximate_scrblt_order(ORDER_INFO* orderInfo,
                                                  const SCRBLT_ORDER* scrblt);
FREERDP_LOCAL BOOL update_write_scrblt_order(wStream* s, ORDER_INFO* orderInfo,
                                             const SCRBLT_ORDER* scrblt);

FREERDP_LOCAL int update_approximate_opaque_rect_order(ORDER_INFO* orderInfo,
                                                       const OPAQUE_RECT_ORDER* opaque_rect);
FREERDP_LOCAL BOOL update_write_opaque_rect_order(wStream* s, ORDER_INFO* orderInfo,
                                                  const OPAQUE_RECT_ORDER* opaque_rect);

FREERDP_LOCAL int update_approximate_line_to_order(ORDER_INFO* orderInfo,
                                                   const LINE_TO_ORDER* line_to);
FREERDP_LOCAL BOOL update_write_line_to_order(wStream* s, ORDER_INFO* orderInfo,
                                              const LINE_TO_ORDER* line_to);

FREERDP_LOCAL int update_approximate_memblt_order(ORDER_INFO* orderInfo,
                                                  const MEMBLT_ORDER* memblt);
FREERDP_LOCAL BOOL update_write_memblt_order(wStream* s, ORDER_INFO* orderInfo,
                                             const MEMBLT_ORDER* memblt);

FREERDP_LOCAL int update_approximate_glyph_index_order(ORDER_INFO* orderInfo,
                                                       const GLYPH_INDEX_ORDER* glyph_index);
FREERDP_LOCAL BOOL update_write_glyph_index_order(wStream* s, ORDER_INFO* orderInfo,
                                                  GLYPH_INDEX_ORDER* glyph_index);

FREERDP_LOCAL int update_approximate_cache_bitmap_order(const CACHE_BITMAP_ORDER* cache_bitmap,
                                                        BOOL compressed, UINT16* flags);
FREERDP_LOCAL BOOL update_write_cache_bitmap_order(wStream* s,
                                                   const CACHE_BITMAP_ORDER* cache_bitmap_order,
                                                   BOOL compressed, UINT16* flags);

FREERDP_LOCAL int update_approximate_cache_bitmap_v2_order(CACHE_BITMAP_V2_ORDER* cache_bitmap_v2,
                                                           BOOL compressed, UINT16* flags);
FREERDP_LOCAL BOOL update_write_cache_bitmap_v2_order(wStream* s,
                                                      CACHE_BITMAP_V2_ORDER* cache_bitmap_v2_order,
                                                      BOOL compressed, UINT16* flags);

FREERDP_LOCAL int update_approximate_cache_bitmap_v3_order(CACHE_BITMAP_V3_ORDER* cache_bitmap_v3,
                                                           UINT16* flags);
FREERDP_LOCAL BOOL update_write_cache_bitmap_v3_order(wStream* s,
                                                      CACHE_BITMAP_V3_ORDER* cache_bitmap_v3_order,
                                                      UINT16* flags);

FREERDP_LOCAL int
update_approximate_cache_color_table_order(const CACHE_COLOR_TABLE_ORDER* cache_color_table,
                                           UINT16* flags);
FREERDP_LOCAL BOOL update_write_cache_color_table_order(
    wStream* s, const CACHE_COLOR_TABLE_ORDER* cache_color_table_order, UINT16* flags);

FREERDP_LOCAL int update_approximate_cache_glyph_order(const CACHE_GLYPH_ORDER* cache_glyph,
                                                       UINT16* flags);
FREERDP_LOCAL BOOL update_write_cache_glyph_order(wStream* s,
                                                  const CACHE_GLYPH_ORDER* cache_glyph_order,
                                                  UINT16* flags);

FREERDP_LOCAL int
update_approximate_cache_glyph_v2_order(const CACHE_GLYPH_V2_ORDER* cache_glyph_v2, UINT16* flags);
FREERDP_LOCAL BOOL update_write_cache_glyph_v2_order(wStream* s,
                                                     const CACHE_GLYPH_V2_ORDER* cache_glyph_v2,
                                                     UINT16* flags);

FREERDP_LOCAL int update_approximate_cache_brush_order(const CACHE_BRUSH_ORDER* cache_brush,
                                                       UINT16* flags);
FREERDP_LOCAL BOOL update_write_cache_brush_order(wStream* s,
                                                  const CACHE_BRUSH_ORDER* cache_brush_order,
                                                  UINT16* flags);

FREERDP_LOCAL int update_approximate_create_offscreen_bitmap_order(
    const CREATE_OFFSCREEN_BITMAP_ORDER* create_offscreen_bitmap);
FREERDP_LOCAL BOOL update_write_create_offscreen_bitmap_order(
    wStream* s, const CREATE_OFFSCREEN_BITMAP_ORDER* create_offscreen_bitmap);

FREERDP_LOCAL int
update_approximate_switch_surface_order(const SWITCH_SURFACE_ORDER* switch_surface);
FREERDP_LOCAL BOOL update_write_switch_surface_order(wStream* s,
                                                     const SWITCH_SURFACE_ORDER* switch_surface);

#endif /* FREERDP_LIB_CORE_ORDERS_H */
