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

#include "orders.h"

uint8 PRIMARY_DRAWING_ORDER_STRINGS[][20] =
{
	"DstBlt",
	"PatBlt",
	"ScrBlt",
	"", "", "", "",
	"DrawNineGrid",
	"MultiDrawNineGrid",
	"LineTo",
	"OpaqueRect",
	"SaveBitmap",
	"MemBlt",
	"Mem3Blt",
	"MultiDstBlt",
	"MultiPatBlt",
	"MultiScrBlt",
	"MultiOpaqueRect",
	"FastIndex",
	"PolygonSC",
	"PolygonCB",
	"Polyline",
	"FastGlyph",
	"EllipseSC",
	"EllipseCB",
	"GlyphIndex"
};

uint8 SECONDARY_DRAWING_ORDER_STRINGS[][32] =
{
	"Cache Bitmap",
	"Cache Color Table",
	"Cache Bitmap (Compressed)",
	"Cache Glyph",
	"Cache Bitmap V2",
	"Cache Bitmap V2 (Compressed)",
	"",
	"Cache Brush",
	"Cache Bitmap V3"
};

uint8 ALTSEC_DRAWING_ORDER_STRINGS[][32] =
{
	"Switch Surface",
	"Create Offscreen Bitmap",
	"Stream Bitmap First",
	"Stream Bitmap Next",
	"Create NineGrid Bitmap",
	"Draw GDI+ First",
	"Draw GDI+ Next",
	"Draw GDI+ End",
	"Draw GDI+ Cache First",
	"Draw GDI+ Cache Next",
	"Draw GDI+ Cache End",
	"Windowing",
	"Desktop Composition",
	"Frame Marker"
};

/* Primary Drawing Orders */

void rdp_recv_dstblt_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_patblt_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_scrblt_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_draw_nine_grid_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_multi_draw_nine_grid_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_line_to_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_opaque_rect_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_save_bitmap_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_memblt_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_mem3blt_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_multi_dstblt_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_multi_patblt_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_multi_scrblt_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_multi_opaque_rect_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_fast_index_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_polygon_sc_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_polygon_cb_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_polyline_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_fast_glyph_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_ellipse_sc_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_ellipse_cb_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_glyph_index_order(rdpRdp* rdp, STREAM* s)
{

}

/* Secondary Drawing Orders */

void rdp_recv_cache_bitmap_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_cache_color_table_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_cache_bitmap_compressed_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_cache_glyph_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_cache_bitmap_v2_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_cache_bitmap_v2_compressed_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_cache_brush_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_cache_bitmap_v3_order(rdpRdp* rdp, STREAM* s)
{

}

/* Alternate Secondary Drawing Orders */

void rdp_recv_switch_surface_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_create_offscreen_bitmap_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_stream_bitmap_first_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_stream_bitmap_next_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_create_nine_grid_bitmap_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_draw_gdiplus_first_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_draw_gdiplus_next_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_draw_gdiplus_end_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_draw_gdiplus_cache_first_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_draw_gdiplus_cache_next_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_draw_gdiplus_cache_end_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_windowing_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_desktop_composition_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_frame_marker_order(rdpRdp* rdp, STREAM* s)
{

}

void rdp_recv_primary_order(rdpRdp* rdp, STREAM* s, uint8 flags)
{
	uint8 orderType;

	stream_read_uint8(s, orderType); /* orderType (1 byte) */

	printf("%s Primary Drawing Order\n", PRIMARY_DRAWING_ORDER_STRINGS[orderType]);

	switch (orderType)
	{
		case ORDER_TYPE_DSTBLT:
			rdp_recv_dstblt_order(rdp, s);
			break;

		case ORDER_TYPE_PATBLT:
			rdp_recv_patblt_order(rdp, s);
			break;

		case ORDER_TYPE_SCRBLT:
			rdp_recv_scrblt_order(rdp, s);
			break;

		case ORDER_TYPE_DRAW_NINE_GRID:
			rdp_recv_draw_nine_grid_order(rdp, s);
			break;

		case ORDER_TYPE_MULTI_DRAW_NINE_GRID:
			rdp_recv_multi_draw_nine_grid_order(rdp, s);
			break;

		case ORDER_TYPE_LINE_TO:
			rdp_recv_line_to_order(rdp, s);
			break;

		case ORDER_TYPE_OPAQUE_RECT:
			rdp_recv_opaque_rect_order(rdp, s);
			break;

		case ORDER_TYPE_SAVE_BITMAP:
			rdp_recv_save_bitmap_order(rdp, s);
			break;

		case ORDER_TYPE_MEMBLT:
			rdp_recv_memblt_order(rdp, s);
			break;

		case ORDER_TYPE_MEM3BLT:
			rdp_recv_mem3blt_order(rdp, s);
			break;

		case ORDER_TYPE_MULTI_DSTBLT:
			rdp_recv_multi_dstblt_order(rdp, s);
			break;

		case ORDER_TYPE_MULTI_PATBLT:
			rdp_recv_multi_patblt_order(rdp, s);
			break;

		case ORDER_TYPE_MULTI_SCRBLT:
			rdp_recv_multi_scrblt_order(rdp, s);
			break;

		case ORDER_TYPE_MULTI_OPAQUE_RECT:
			rdp_recv_multi_opaque_rect_order(rdp, s);
			break;

		case ORDER_TYPE_FAST_INDEX:
			rdp_recv_fast_index_order(rdp, s);
			break;

		case ORDER_TYPE_POLYGON_SC:
			rdp_recv_polygon_sc_order(rdp, s);
			break;

		case ORDER_TYPE_POLYGON_CB:
			rdp_recv_polygon_cb_order(rdp, s);
			break;

		case ORDER_TYPE_POLYLINE:
			rdp_recv_polyline_order(rdp, s);
			break;

		case ORDER_TYPE_FAST_GLYPH:
			rdp_recv_fast_glyph_order(rdp, s);
			break;

		case ORDER_TYPE_ELLIPSE_SC:
			rdp_recv_ellipse_sc_order(rdp, s);
			break;

		case ORDER_TYPE_ELLIPSE_CB:
			rdp_recv_ellipse_cb_order(rdp, s);
			break;

		case ORDER_TYPE_GLYPH_INDEX:
			rdp_recv_glyph_index_order(rdp, s);
			break;

		default:
			break;
	}
}

void rdp_recv_secondary_order(rdpRdp* rdp, STREAM* s, uint8 flags)
{
	uint8 orderType;

	stream_read_uint8(s, orderType); /* orderType (1 byte) */

	printf("%s Secondary Drawing Order\n", SECONDARY_DRAWING_ORDER_STRINGS[orderType]);

	switch (orderType)
	{
		case ORDER_TYPE_BITMAP_UNCOMPRESSED:
			rdp_recv_cache_bitmap_order(rdp, s);
			break;

		case ORDER_TYPE_CACHE_COLOR_TABLE:
			rdp_recv_cache_color_table_order(rdp, s);
			break;

		case ORDER_TYPE_CACHE_BITMAP_COMPRESSED:
			rdp_recv_cache_bitmap_compressed_order(rdp, s);
			break;

		case ORDER_TYPE_CACHE_GLYPH:
			rdp_recv_cache_glyph_order(rdp, s);
			break;

		case ORDER_TYPE_BITMAP_UNCOMPRESSED_V2:
			rdp_recv_cache_bitmap_v2_order(rdp, s);
			break;

		case ORDER_TYPE_BITMAP_COMPRESSED_V2:
			rdp_recv_cache_bitmap_v2_compressed_order(rdp, s);
			break;

		case ORDER_TYPE_CACHE_BRUSH:
			rdp_recv_cache_brush_order(rdp, s);
			break;

		case ORDER_TYPE_BITMAP_COMPRESSED_V3:
			rdp_recv_cache_bitmap_v3_order(rdp, s);
			break;

		default:
			break;
	}
}

void rdp_recv_altsec_order(rdpRdp* rdp, STREAM* s, uint8 flags)
{
	uint8 orderType;

	stream_read_uint8(s, orderType); /* orderType (1 byte) */

	printf("%s Alternate Secondary Drawing Order\n", ALTSEC_DRAWING_ORDER_STRINGS[orderType]);

	switch (orderType)
	{
		case ORDER_TYPE_SWITCH_SURFACE:
			rdp_recv_switch_surface_order(rdp, s);
			break;

		case ORDER_TYPE_CREATE_OFFSCR_BITMAP:
			rdp_recv_create_offscreen_bitmap_order(rdp, s);
			break;

		case ORDER_TYPE_STREAM_BITMAP_FIRST:
			rdp_recv_stream_bitmap_first_order(rdp, s);
			break;

		case ORDER_TYPE_STREAM_BITMAP_NEXT:
			rdp_recv_stream_bitmap_next_order(rdp, s);
			break;

		case ORDER_TYPE_CREATE_NINE_GRID_BITMAP:
			rdp_recv_create_nine_grid_bitmap_order(rdp, s);
			break;

		case ORDER_TYPE_GDIPLUS_FIRST:
			rdp_recv_draw_gdiplus_first_order(rdp, s);
			break;

		case ORDER_TYPE_GDIPLUS_NEXT:
			rdp_recv_draw_gdiplus_next_order(rdp, s);
			break;

		case ORDER_TYPE_GDIPLUS_END:
			rdp_recv_draw_gdiplus_end_order(rdp, s);
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_FIRST:
			rdp_recv_draw_gdiplus_cache_first_order(rdp, s);
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_NEXT:
			rdp_recv_draw_gdiplus_cache_next_order(rdp, s);
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_END:
			rdp_recv_draw_gdiplus_cache_end_order(rdp, s);
			break;

		case ORDER_TYPE_WINDOW:
			rdp_recv_windowing_order(rdp, s);
			break;

		case ORDER_TYPE_COMPDESK_FIRST:
			rdp_recv_desktop_composition_order(rdp, s);
			break;

		case ORDER_TYPE_FRAME_MARKER:
			rdp_recv_frame_marker_order(rdp, s);
			break;

		default:
			break;
	}
}

void rdp_recv_order(rdpRdp* rdp, STREAM* s)
{
	uint8 controlFlags;

	stream_read_uint8(s, controlFlags); /* controlFlags (1 byte) */

	switch (controlFlags & ORDER_CLASS_MASK)
	{
		case ORDER_PRIMARY_CLASS:
			rdp_recv_primary_order(rdp, s, controlFlags);
			break;

		case ORDER_SECONDARY_CLASS:
			rdp_recv_secondary_order(rdp, s, controlFlags);
			break;

		case ORDER_ALTSEC_CLASS:
			rdp_recv_altsec_order(rdp, s, controlFlags);
			break;
	}
}
