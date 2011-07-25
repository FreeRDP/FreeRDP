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

void rdp_recv_primary_order(rdpRdp* rdp, STREAM* s, uint8 flags)
{
	uint8 orderType;

	stream_read_uint8(s, orderType); /* orderType (1 byte) */

	printf("%s Primary Drawing Order\n", PRIMARY_DRAWING_ORDER_STRINGS[orderType]);

	switch (orderType)
	{
		case ORDER_TYPE_DSTBLT:
			break;

		case ORDER_TYPE_PATBLT:
			break;

		case ORDER_TYPE_SCRBLT:
			break;

		case ORDER_TYPE_DRAW_NINEGRID:
			break;

		case ORDER_TYPE_MULTI_DRAW_NINEGRID:
			break;

		case ORDER_TYPE_LINETO:
			break;

		case ORDER_TYPE_OPAQUE_RECT:
			break;

		case ORDER_TYPE_SAVE_BITMAP:
			break;

		case ORDER_TYPE_MEMBLT:
			break;

		case ORDER_TYPE_MEM3BLT:
			break;

		case ORDER_TYPE_MULTI_DSTBLT:
			break;

		case ORDER_TYPE_MULTI_PATBLT:
			break;

		case ORDER_TYPE_MULTI_SCRBLT:
			break;

		case ORDER_TYPE_MULTI_OPAQUE_RECT:
			break;

		case ORDER_TYPE_FAST_INDEX:
			break;

		case ORDER_TYPE_POLYGON_SC:
			break;

		case ORDER_TYPE_POLYGON_CB:
			break;

		case ORDER_TYPE_POLYLINE:
			break;

		case ORDER_TYPE_FAST_GLYPH:
			break;

		case ORDER_TYPE_ELLIPSE_SC:
			break;

		case ORDER_TYPE_ELLIPSE_CB:
			break;

		case ORDER_TYPE_INDEX_ORDER:
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
			break;

		case ORDER_TYPE_CACHE_COLOR_TABLE:
			break;

		case ORDER_TYPE_CACHE_BITMAP_COMPRESSED:
			break;

		case ORDER_TYPE_CACHE_GLYPH:
			break;

		case ORDER_TYPE_BITMAP_UNCOMPRESSED_V2:
			break;

		case ORDER_TYPE_BITMAP_COMPRESSED_V2:
			break;

		case ORDER_TYPE_CACHE_BRUSH:
			break;

		case ORDER_TYPE_BITMAP_COMPRESSED_V3:
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
			break;

		case ORDER_TYPE_CREATE_OFFSCR_BITMAP:
			break;

		case ORDER_TYPE_STREAM_BITMAP_FIRST:
			break;

		case ORDER_TYPE_STREAM_BITMAP_NEXT:
			break;

		case ORDER_TYPE_CREATE_NINEGRID_BITMAP:
			break;

		case ORDER_TYPE_GDIPLUS_FIRST:
			break;

		case ORDER_TYPE_GDIPLUS_NEXT:
			break;

		case ORDER_TYPE_GDIPLUS_END:
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_FIRST:
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_NEXT:
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_END:
			break;

		case ORDER_TYPE_WINDOW:
			break;

		case ORDER_TYPE_COMPDESK_FIRST:
			break;

		case ORDER_TYPE_FRAME_MARKER:
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
			//rdp_recv_primary_order(rdp, s, controlFlags);
			break;

		case ORDER_SECONDARY_CLASS:
			//rdp_recv_secondary_order(rdp, s, controlFlags);
			break;

		case ORDER_ALTSEC_CLASS:
			//rdp_recv_altsec_order(rdp, s, controlFlags);
			break;
	}
}
