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

uint8 PRIMARY_DRAWING_ORDER_FIELD_BYTES[] =
{
	DSTBLT_ORDER_FIELD_BYTES,
	PATBLT_ORDER_FIELD_BYTES,
	SCRBLT_ORDER_FIELD_BYTES,
	0, 0, 0, 0,
	DRAW_NINE_GRID_ORDER_FIELD_BYTES,
	MULTI_DRAW_NINE_GRID_ORDER_FIELD_BYTES,
	LINE_TO_ORDER_FIELD_BYTES,
	OPAQUE_RECT_ORDER_FIELD_BYTES,
	SAVE_BITMAP_ORDER_FIELD_BYTES,
	MEMBLT_ORDER_FIELD_BYTES,
	MEM3BLT_ORDER_FIELD_BYTES,
	MULTI_DSTBLT_ORDER_FIELD_BYTES,
	MULTI_PATBLT_ORDER_FIELD_BYTES,
	MULTI_SCRBLT_ORDER_FIELD_BYTES,
	MULTI_OPAQUE_RECT_ORDER_FIELD_BYTES,
	FAST_INDEX_ORDER_FIELD_BYTES,
	POLYGON_SC_ORDER_FIELD_BYTES,
	POLYGON_CB_ORDER_FIELD_BYTES,
	POLYLINE_ORDER_FIELD_BYTES,
	FAST_GLYPH_ORDER_FIELD_BYTES,
	ELLIPSE_SC_ORDER_FIELD_BYTES,
	ELLIPSE_CB_ORDER_FIELD_BYTES,
	GLYPH_INDEX_ORDER_FIELD_BYTES
};

/* Primary Drawing Orders */

void rdp_recv_dstblt_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_patblt_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_scrblt_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_draw_nine_grid_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_multi_draw_nine_grid_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_line_to_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_opaque_rect_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_save_bitmap_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_memblt_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_mem3blt_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_multi_dstblt_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_multi_patblt_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_multi_scrblt_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_multi_opaque_rect_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_fast_index_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_polygon_sc_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_polygon_cb_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_polyline_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_fast_glyph_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_ellipse_sc_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_ellipse_cb_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
{

}

void rdp_recv_glyph_index_order(rdpRdp* rdp, STREAM* s, ORDER_INFO* orderInfo)
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

void rdp_read_field_flags(STREAM* s, uint32* fieldFlags, uint8 flags, uint8 fieldBytes)
{
	int i;
	uint8 byte;

	if (flags & ORDER_ZERO_FIELD_BYTE_BIT0)
		fieldBytes--;

	if (flags & ORDER_ZERO_FIELD_BYTE_BIT1)
	{
		if (fieldBytes < 2)
			fieldBytes = 0;
		else
			fieldBytes -= 2;
	}

	*fieldFlags = 0;
	for (i = 0; i < fieldBytes; i++)
	{
		stream_read_uint8(s, byte);
		*fieldFlags |= byte << (i * 8);
	}
}

void rdp_read_bounds(STREAM* s, ORDER_INFO* orderInfo)
{
	uint8 flags;

	stream_read_uint8(s, flags); /* field flags */

	if (flags & BOUND_DELTA_LEFT)
		stream_read_uint8(s, orderInfo->deltaBoundLeft);
	else if (flags & BOUND_LEFT)
		stream_read_uint16(s, orderInfo->boundLeft);

	if (flags & BOUND_DELTA_TOP)
		stream_read_uint8(s, orderInfo->deltaBoundTop);
	else if (flags & BOUND_TOP)
		stream_read_uint16(s, orderInfo->boundTop);

	if (flags & BOUND_DELTA_RIGHT)
		stream_read_uint8(s, orderInfo->deltaBoundRight);
	else if (flags & BOUND_RIGHT)
		stream_read_uint16(s, orderInfo->boundRight);

	if (flags & BOUND_DELTA_BOTTOM)
		stream_read_uint8(s, orderInfo->deltaBoundBottom);
	else if (flags & BOUND_BOTTOM)
		stream_read_uint16(s, orderInfo->boundBottom);
}

void rdp_recv_primary_order(rdpRdp* rdp, STREAM* s, uint8 flags)
{
	ORDER_INFO* orderInfo = &(rdp->order_info);

	if (flags & ORDER_TYPE_CHANGE)
		stream_read_uint8(s, orderInfo->orderType); /* orderType (1 byte) */

	rdp_read_field_flags(s, &(orderInfo->fieldFlags), flags,
			PRIMARY_DRAWING_ORDER_FIELD_BYTES[orderInfo->orderType]);

	if (flags & ORDER_BOUNDS)
	{
		if (!(flags & ORDER_ZERO_BOUNDS_DELTAS))
		{
			rdp_read_bounds(s, orderInfo);
		}
	}

	orderInfo->deltaCoordinates = (flags & ORDER_DELTA_COORDINATES) ? True : False;

	printf("%s Primary Drawing Order\n", PRIMARY_DRAWING_ORDER_STRINGS[orderInfo->orderType]);

	switch (orderInfo->orderType)
	{
		case ORDER_TYPE_DSTBLT:
			rdp_recv_dstblt_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_PATBLT:
			rdp_recv_patblt_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_SCRBLT:
			rdp_recv_scrblt_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_DRAW_NINE_GRID:
			rdp_recv_draw_nine_grid_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_MULTI_DRAW_NINE_GRID:
			rdp_recv_multi_draw_nine_grid_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_LINE_TO:
			rdp_recv_line_to_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_OPAQUE_RECT:
			rdp_recv_opaque_rect_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_SAVE_BITMAP:
			rdp_recv_save_bitmap_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_MEMBLT:
			rdp_recv_memblt_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_MEM3BLT:
			rdp_recv_mem3blt_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_MULTI_DSTBLT:
			rdp_recv_multi_dstblt_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_MULTI_PATBLT:
			rdp_recv_multi_patblt_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_MULTI_SCRBLT:
			rdp_recv_multi_scrblt_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_MULTI_OPAQUE_RECT:
			rdp_recv_multi_opaque_rect_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_FAST_INDEX:
			rdp_recv_fast_index_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_POLYGON_SC:
			rdp_recv_polygon_sc_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_POLYGON_CB:
			rdp_recv_polygon_cb_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_POLYLINE:
			rdp_recv_polyline_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_FAST_GLYPH:
			rdp_recv_fast_glyph_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_ELLIPSE_SC:
			rdp_recv_ellipse_sc_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_ELLIPSE_CB:
			rdp_recv_ellipse_cb_order(rdp, s, orderInfo);
			break;

		case ORDER_TYPE_GLYPH_INDEX:
			rdp_recv_glyph_index_order(rdp, s, orderInfo);
			break;

		default:
			break;
	}
}

void rdp_recv_secondary_order(rdpRdp* rdp, STREAM* s, uint8 flags)
{
	uint8* next;
	uint8 orderType;
	uint16 extraFlags;
	uint16 orderLength;

	stream_get_mark(s, next);
	stream_read_uint16(s, orderLength); /* orderLength (2 bytes) */
	stream_read_uint16(s, extraFlags); /* extraFlags (2 bytes) */
	stream_read_uint8(s, orderType); /* orderType (1 byte) */

	orderLength += 13; /* adjust length (13 bytes less than actual length) */
	next += orderLength;

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

	stream_set_mark(s, next);
}

void rdp_recv_altsec_order(rdpRdp* rdp, STREAM* s, uint8 flags)
{
	uint8 orderType;

	orderType = flags >>= 2; /* orderType is in higher 6 bits of flags field */

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
