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

void rdp_read_coord(STREAM* s, sint16* coord, boolean delta)
{
	sint8 byte;

	if (delta)
	{
		stream_read_uint8(s, byte);
		*coord += byte;
	}
	else
	{
		stream_read_uint16(s, *coord);
	}
}

void rdp_read_color(STREAM* s, uint32* color)
{
	uint8 byte;

	stream_read_uint8(s, byte);
	*color = byte;
	stream_read_uint8(s, byte);
	*color |= (byte << 8);
	stream_read_uint8(s, byte);
	*color |= (byte << 16);
}

/* Primary Drawing Orders */

void rdp_read_dstblt_order(STREAM* s, ORDER_INFO* orderInfo, DSTBLT_ORDER* dstblt)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		rdp_read_coord(s, &dstblt->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		rdp_read_coord(s, &dstblt->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		rdp_read_coord(s, &dstblt->nWidth, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		rdp_read_coord(s, &dstblt->nHeight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		stream_read_uint8(s, dstblt->bRop);
}

void rdp_read_patblt_order(STREAM* s, ORDER_INFO* orderInfo, PATBLT_ORDER* patblt)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		rdp_read_coord(s, &patblt->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		rdp_read_coord(s, &patblt->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		rdp_read_coord(s, &patblt->nWidth, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		rdp_read_coord(s, &patblt->nHeight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		stream_read_uint8(s, patblt->bRop);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		rdp_read_color(s, &patblt->backColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		rdp_read_color(s, &patblt->foreColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		stream_read_uint8(s, patblt->brushOrgX);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
		stream_read_uint8(s, patblt->brushOrgY);

	if (orderInfo->fieldFlags & ORDER_FIELD_10)
		stream_read_uint8(s, patblt->brushStyle);

	if (orderInfo->fieldFlags & ORDER_FIELD_11)
		stream_read_uint8(s, patblt->brushHatch);

	if (orderInfo->fieldFlags & ORDER_FIELD_12)
		stream_read(s, patblt->brushExtra, 7);
}

void rdp_read_scrblt_order(STREAM* s, ORDER_INFO* orderInfo, SCRBLT_ORDER* scrblt)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		rdp_read_coord(s, &scrblt->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		rdp_read_coord(s, &scrblt->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		rdp_read_coord(s, &scrblt->nWidth, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		rdp_read_coord(s, &scrblt->nHeight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		stream_read_uint8(s, scrblt->bRop);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		rdp_read_coord(s, &scrblt->nXSrc, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		rdp_read_coord(s, &scrblt->nYSrc, orderInfo->deltaCoordinates);
}

void rdp_read_draw_nine_grid_order(STREAM* s, ORDER_INFO* orderInfo, DRAW_NINE_GRID_ORDER* draw_nine_grid)
{

}

void rdp_read_multi_draw_nine_grid_order(STREAM* s, ORDER_INFO* orderInfo, MULTI_DRAW_NINE_GRID_ORDER* multi_draw_nine_grid)
{

}

void rdp_read_line_to_order(STREAM* s, ORDER_INFO* orderInfo, LINE_TO_ORDER* line_to)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		stream_read_uint16(s, line_to->backMode);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		rdp_read_coord(s, &line_to->nXStart, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		rdp_read_coord(s, &line_to->nYStart, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		rdp_read_coord(s, &line_to->nXEnd, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		rdp_read_coord(s, &line_to->nYEnd, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		rdp_read_color(s, &line_to->backColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		stream_read_uint8(s, line_to->bRop2);

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		stream_read_uint8(s, line_to->penStyle);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
		stream_read_uint8(s, line_to->penWidth);

	if (orderInfo->fieldFlags & ORDER_FIELD_10)
		rdp_read_color(s, &line_to->penColor);
}

void rdp_read_opaque_rect_order(STREAM* s, ORDER_INFO* orderInfo, OPAQUE_RECT_ORDER* opaque_rect)
{
	uint8 byte;

	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		rdp_read_coord(s, &opaque_rect->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		rdp_read_coord(s, &opaque_rect->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		rdp_read_coord(s, &opaque_rect->nWidth, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		rdp_read_coord(s, &opaque_rect->nHeight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
	{
		stream_read_uint8(s, byte);
		opaque_rect->color = (opaque_rect->color & 0xFFFFFF00) | byte;
	}

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
	{
		stream_read_uint8(s, byte);
		opaque_rect->color = (opaque_rect->color & 0xFFFF00FF) | (byte << 8);
	}

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
	{
		stream_read_uint8(s, byte);
		opaque_rect->color = (opaque_rect->color & 0xFF00FFFF) | (byte << 16);
	}
}

void rdp_read_save_bitmap_order(STREAM* s, ORDER_INFO* orderInfo, SAVE_BITMAP_ORDER* save_bitmap)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		stream_read_uint32(s, save_bitmap->savedBitmapPosition);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		rdp_read_coord(s, &save_bitmap->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		rdp_read_coord(s, &save_bitmap->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		rdp_read_coord(s, &save_bitmap->nRightRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		rdp_read_coord(s, &save_bitmap->nBottomRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		stream_read_uint8(s, save_bitmap->operation);
}

void rdp_read_memblt_order(STREAM* s, ORDER_INFO* orderInfo, MEMBLT_ORDER* memblt)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		stream_read_uint16(s, memblt->cacheId);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		rdp_read_coord(s, &memblt->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		rdp_read_coord(s, &memblt->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		rdp_read_coord(s, &memblt->nWidth, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		rdp_read_coord(s, &memblt->nHeight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		stream_read_uint8(s, memblt->bRop);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		rdp_read_coord(s, &memblt->nXSrc, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		rdp_read_coord(s, &memblt->nYSrc, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
		stream_read_uint16(s, memblt->cacheIndex);
}

void rdp_read_mem3blt_order(STREAM* s, ORDER_INFO* orderInfo, MEM3BLT_ORDER* mem3blt)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		stream_read_uint16(s, mem3blt->cacheId);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		rdp_read_coord(s, &mem3blt->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		rdp_read_coord(s, &mem3blt->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		rdp_read_coord(s, &mem3blt->nWidth, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		rdp_read_coord(s, &mem3blt->nHeight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		stream_read_uint8(s, mem3blt->bRop);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		rdp_read_coord(s, &mem3blt->nXSrc, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		rdp_read_coord(s, &mem3blt->nYSrc, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
		rdp_read_color(s, &mem3blt->backColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_10)
		rdp_read_color(s, &mem3blt->foreColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_11)
		stream_read_uint8(s, mem3blt->brushOrgX);

	if (orderInfo->fieldFlags & ORDER_FIELD_12)
		stream_read_uint8(s, mem3blt->brushOrgY);

	if (orderInfo->fieldFlags & ORDER_FIELD_13)
		stream_read_uint8(s, mem3blt->brushStyle);

	if (orderInfo->fieldFlags & ORDER_FIELD_14)
		stream_read_uint8(s, mem3blt->brushHatch);

	if (orderInfo->fieldFlags & ORDER_FIELD_15)
		stream_read(s, mem3blt->brushExtra, 7);

	if (orderInfo->fieldFlags & ORDER_FIELD_16)
		stream_read_uint16(s, mem3blt->cacheIndex);
}

void rdp_read_multi_dstblt_order(STREAM* s, ORDER_INFO* orderInfo, MULTI_DSTBLT_ORDER* multi_dstblt)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		rdp_read_coord(s, &multi_dstblt->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		rdp_read_coord(s, &multi_dstblt->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		rdp_read_coord(s, &multi_dstblt->nWidth, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		rdp_read_coord(s, &multi_dstblt->nHeight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		stream_read_uint8(s, multi_dstblt->bRop);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		stream_read_uint8(s, multi_dstblt->nDeltaEntries);

	/* codeDeltaList */
}

void rdp_read_multi_patblt_order(STREAM* s, ORDER_INFO* orderInfo, MULTI_PATBLT_ORDER* multi_patblt)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		rdp_read_coord(s, &multi_patblt->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		rdp_read_coord(s, &multi_patblt->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		rdp_read_coord(s, &multi_patblt->nWidth, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		rdp_read_coord(s, &multi_patblt->nHeight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		stream_read_uint8(s, multi_patblt->bRop);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		rdp_read_color(s, &multi_patblt->backColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		rdp_read_color(s, &multi_patblt->foreColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		stream_read_uint8(s, multi_patblt->brushOrgX);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
		stream_read_uint8(s, multi_patblt->brushOrgY);

	if (orderInfo->fieldFlags & ORDER_FIELD_10)
		stream_read_uint8(s, multi_patblt->brushStyle);

	if (orderInfo->fieldFlags & ORDER_FIELD_11)
		stream_read_uint8(s, multi_patblt->brushHatch);

	if (orderInfo->fieldFlags & ORDER_FIELD_12)
		stream_read(s, multi_patblt->brushExtra, 7);

	if (orderInfo->fieldFlags & ORDER_FIELD_13)
		stream_read_uint8(s, multi_patblt->nDeltaEntries);

	/* codeDeltaList */
}

void rdp_read_multi_scrblt_order(STREAM* s, ORDER_INFO* orderInfo, MULTI_SCRBLT_ORDER* multi_scrblt)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		rdp_read_coord(s, &multi_scrblt->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		rdp_read_coord(s, &multi_scrblt->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		rdp_read_coord(s, &multi_scrblt->nWidth, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		rdp_read_coord(s, &multi_scrblt->nHeight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		stream_read_uint8(s, multi_scrblt->bRop);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		rdp_read_coord(s, &multi_scrblt->nXSrc, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		rdp_read_coord(s, &multi_scrblt->nYSrc, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		stream_read_uint8(s, multi_scrblt->nDeltaEntries);

	/* codeDeltaList */
}

void rdp_read_multi_opaque_rect_order(STREAM* s, ORDER_INFO* orderInfo, MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect)
{
	uint8 byte;

	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		rdp_read_coord(s, &multi_opaque_rect->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		rdp_read_coord(s, &multi_opaque_rect->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		rdp_read_coord(s, &multi_opaque_rect->nWidth, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		rdp_read_coord(s, &multi_opaque_rect->nHeight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
	{
		stream_read_uint8(s, byte);
		multi_opaque_rect->color = (multi_opaque_rect->color & 0xFFFFFF00) | byte;
	}

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
	{
		stream_read_uint8(s, byte);
		multi_opaque_rect->color = (multi_opaque_rect->color & 0xFFFF00FF) | (byte << 8);
	}

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
	{
		stream_read_uint8(s, byte);
		multi_opaque_rect->color = (multi_opaque_rect->color & 0xFF00FFFF) | (byte << 16);
	}

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		stream_read_uint8(s, multi_opaque_rect->nDeltaEntries);

	/* codeDeltaList */
}

void rdp_read_fast_index_order(STREAM* s, ORDER_INFO* orderInfo, FAST_INDEX_ORDER* fast_index)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		stream_read_uint8(s, fast_index->cacheId);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		stream_read_uint16(s, fast_index->fDrawing);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		rdp_read_color(s, &fast_index->backColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		rdp_read_color(s, &fast_index->foreColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		rdp_read_coord(s, &fast_index->bkLeft, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		rdp_read_coord(s, &fast_index->bkTop, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		rdp_read_coord(s, &fast_index->bkRight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		rdp_read_coord(s, &fast_index->bkBottom, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
		rdp_read_coord(s, &fast_index->opLeft, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_10)
		rdp_read_coord(s, &fast_index->opTop, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_11)
		rdp_read_coord(s, &fast_index->opRight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_12)
		rdp_read_coord(s, &fast_index->opBottom, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_13)
		rdp_read_coord(s, &fast_index->x, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_14)
		rdp_read_coord(s, &fast_index->y, orderInfo->deltaCoordinates);

	/* bytes */
}

void rdp_read_polygon_sc_order(STREAM* s, ORDER_INFO* orderInfo, POLYGON_SC_ORDER* polygon_sc)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		rdp_read_coord(s, &polygon_sc->xStart, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		rdp_read_coord(s, &polygon_sc->yStart, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		stream_read_uint8(s, polygon_sc->bRop2);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		stream_read_uint8(s, polygon_sc->fillMode);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		rdp_read_color(s, &polygon_sc->brushColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		stream_read_uint8(s, polygon_sc->nDeltaEntries);

	/* codeDeltaList */
}

void rdp_read_polygon_cb_order(STREAM* s, ORDER_INFO* orderInfo, POLYGON_CB_ORDER* polygon_cb)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		rdp_read_coord(s, &polygon_cb->xStart, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		rdp_read_coord(s, &polygon_cb->yStart, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		stream_read_uint8(s, polygon_cb->bRop2);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		stream_read_uint8(s, polygon_cb->fillMode);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		rdp_read_color(s, &polygon_cb->backColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		rdp_read_color(s, &polygon_cb->foreColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		stream_read_uint8(s, polygon_cb->brushOrgX);

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		stream_read_uint8(s, polygon_cb->brushOrgY);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
		stream_read_uint8(s, polygon_cb->brushStyle);

	if (orderInfo->fieldFlags & ORDER_FIELD_10)
		stream_read_uint8(s, polygon_cb->brushHatch);

	if (orderInfo->fieldFlags & ORDER_FIELD_12)
		stream_read(s, polygon_cb->brushExtra, 7);

	if (orderInfo->fieldFlags & ORDER_FIELD_13)
		stream_read_uint8(s, polygon_cb->nDeltaEntries);

	/* codeDeltaList */
}

void rdp_read_polyline_order(STREAM* s, ORDER_INFO* orderInfo, POLYLINE_ORDER* polyline)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		rdp_read_coord(s, &polyline->xStart, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		rdp_read_coord(s, &polyline->yStart, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		stream_read_uint8(s, polyline->bRop2);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		rdp_read_color(s, &polyline->penColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		stream_read_uint8(s, polyline->nDeltaEntries);

	/* codeDeltaList */
}

void rdp_read_fast_glyph_order(STREAM* s, ORDER_INFO* orderInfo, FAST_GLYPH_ORDER* fast_glyph)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		stream_read_uint8(s, fast_glyph->cacheId);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		stream_read_uint16(s, fast_glyph->fDrawing);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		rdp_read_color(s, &fast_glyph->backColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		rdp_read_color(s, &fast_glyph->foreColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		rdp_read_coord(s, &fast_glyph->bkLeft, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		rdp_read_coord(s, &fast_glyph->bkTop, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		rdp_read_coord(s, &fast_glyph->bkRight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		rdp_read_coord(s, &fast_glyph->bkBottom, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
		rdp_read_coord(s, &fast_glyph->opLeft, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_10)
		rdp_read_coord(s, &fast_glyph->opTop, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_11)
		rdp_read_coord(s, &fast_glyph->opRight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_12)
		rdp_read_coord(s, &fast_glyph->opBottom, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_13)
		rdp_read_coord(s, &fast_glyph->x, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_14)
		rdp_read_coord(s, &fast_glyph->y, orderInfo->deltaCoordinates);

	/* bytes */
}

void rdp_read_ellipse_sc_order(STREAM* s, ORDER_INFO* orderInfo, ELLIPSE_SC_ORDER* ellipse_sc)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		rdp_read_coord(s, &ellipse_sc->leftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		rdp_read_coord(s, &ellipse_sc->topRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		rdp_read_coord(s, &ellipse_sc->rightRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		rdp_read_coord(s, &ellipse_sc->bottomRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		stream_read_uint8(s, ellipse_sc->bRop2);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		stream_read_uint8(s, ellipse_sc->fillMode);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		rdp_read_color(s, &ellipse_sc->color);
}

void rdp_read_ellipse_cb_order(STREAM* s, ORDER_INFO* orderInfo, ELLIPSE_CB_ORDER* ellipse_cb)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		rdp_read_coord(s, &ellipse_cb->leftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		rdp_read_coord(s, &ellipse_cb->topRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		rdp_read_coord(s, &ellipse_cb->rightRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		rdp_read_coord(s, &ellipse_cb->bottomRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		stream_read_uint8(s, ellipse_cb->bRop2);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		stream_read_uint8(s, ellipse_cb->fillMode);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		rdp_read_color(s, &ellipse_cb->backColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		rdp_read_color(s, &ellipse_cb->foreColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
		stream_read_uint8(s, ellipse_cb->brushOrgX);

	if (orderInfo->fieldFlags & ORDER_FIELD_10)
		stream_read_uint8(s, ellipse_cb->brushOrgY);

	if (orderInfo->fieldFlags & ORDER_FIELD_11)
		stream_read_uint8(s, ellipse_cb->brushStyle);

	if (orderInfo->fieldFlags & ORDER_FIELD_12)
		stream_read_uint8(s, ellipse_cb->brushHatch);

	if (orderInfo->fieldFlags & ORDER_FIELD_13)
		stream_read(s, ellipse_cb->brushExtra, 7);
}

void rdp_read_glyph_index_order(STREAM* s, ORDER_INFO* orderInfo, GLYPH_INDEX_ORDER* glyph_index)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		stream_read_uint8(s, glyph_index->cacheId);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		stream_read_uint8(s, glyph_index->flAccel);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		stream_read_uint8(s, glyph_index->ulCharInc);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		stream_read_uint8(s, glyph_index->fOpRedundant);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		rdp_read_color(s, &glyph_index->backColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		rdp_read_color(s, &glyph_index->foreColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		rdp_read_coord(s, &glyph_index->bkLeft, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		rdp_read_coord(s, &glyph_index->bkTop, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
		rdp_read_coord(s, &glyph_index->bkRight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_10)
		rdp_read_coord(s, &glyph_index->bkBottom, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_10)
		rdp_read_coord(s, &glyph_index->opLeft, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_11)
		rdp_read_coord(s, &glyph_index->opTop, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_12)
		rdp_read_coord(s, &glyph_index->opRight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_13)
		rdp_read_coord(s, &glyph_index->opBottom, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_14)
		stream_read_uint8(s, glyph_index->brushOrgX);

	if (orderInfo->fieldFlags & ORDER_FIELD_15)
		stream_read_uint8(s, glyph_index->brushOrgY);

	if (orderInfo->fieldFlags & ORDER_FIELD_16)
		stream_read_uint8(s, glyph_index->brushStyle);

	if (orderInfo->fieldFlags & ORDER_FIELD_17)
		stream_read_uint8(s, glyph_index->brushHatch);

	if (orderInfo->fieldFlags & ORDER_FIELD_18)
		stream_read(s, glyph_index->brushExtra, 7);

	if (orderInfo->fieldFlags & ORDER_FIELD_19)
		rdp_read_coord(s, &glyph_index->x, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_20)
		rdp_read_coord(s, &glyph_index->y, orderInfo->deltaCoordinates);

	/* bytes */
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

void rdp_recv_primary_order(rdpOrders* orders, STREAM* s, uint8 flags)
{
	ORDER_INFO* orderInfo = &(orders->order_info);

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
			rdp_read_dstblt_order(s, orderInfo, &(orders->dstblt));
			break;

		case ORDER_TYPE_PATBLT:
			rdp_read_patblt_order(s, orderInfo, &(orders->patblt));
			break;

		case ORDER_TYPE_SCRBLT:
			rdp_read_scrblt_order(s, orderInfo, &(orders->scrblt));
			break;

		case ORDER_TYPE_DRAW_NINE_GRID:
			rdp_read_draw_nine_grid_order(s, orderInfo, &(orders->draw_nine_grid));
			break;

		case ORDER_TYPE_MULTI_DRAW_NINE_GRID:
			rdp_read_multi_draw_nine_grid_order(s, orderInfo, &(orders->multi_draw_nine_grid));
			break;

		case ORDER_TYPE_LINE_TO:
			rdp_read_line_to_order(s, orderInfo, &(orders->line_to));
			break;

		case ORDER_TYPE_OPAQUE_RECT:
			rdp_read_opaque_rect_order(s, orderInfo, &(orders->opaque_rect));
			break;

		case ORDER_TYPE_SAVE_BITMAP:
			rdp_read_save_bitmap_order(s, orderInfo, &(orders->save_bitmap));
			break;

		case ORDER_TYPE_MEMBLT:
			rdp_read_memblt_order(s, orderInfo, &(orders->memblt));
			break;

		case ORDER_TYPE_MEM3BLT:
			rdp_read_mem3blt_order(s, orderInfo, &(orders->mem3blt));
			break;

		case ORDER_TYPE_MULTI_DSTBLT:
			rdp_read_multi_dstblt_order(s, orderInfo, &(orders->multi_dstblt));
			break;

		case ORDER_TYPE_MULTI_PATBLT:
			rdp_read_multi_patblt_order(s, orderInfo, &(orders->multi_patblt));
			break;

		case ORDER_TYPE_MULTI_SCRBLT:
			rdp_read_multi_scrblt_order(s, orderInfo, &(orders->multi_scrblt));
			break;

		case ORDER_TYPE_MULTI_OPAQUE_RECT:
			rdp_read_multi_opaque_rect_order(s, orderInfo, &(orders->multi_opaque_rect));
			break;

		case ORDER_TYPE_FAST_INDEX:
			rdp_read_fast_index_order(s, orderInfo, &(orders->fast_index));
			break;

		case ORDER_TYPE_POLYGON_SC:
			rdp_read_polygon_sc_order(s, orderInfo, &(orders->polygon_sc));
			break;

		case ORDER_TYPE_POLYGON_CB:
			rdp_read_polygon_cb_order(s, orderInfo, &(orders->polygon_cb));
			break;

		case ORDER_TYPE_POLYLINE:
			rdp_read_polyline_order(s, orderInfo, &(orders->polyline));
			break;

		case ORDER_TYPE_FAST_GLYPH:
			rdp_read_fast_glyph_order(s, orderInfo, &(orders->fast_glyph));
			break;

		case ORDER_TYPE_ELLIPSE_SC:
			rdp_read_ellipse_sc_order(s, orderInfo, &(orders->ellipse_sc));
			break;

		case ORDER_TYPE_ELLIPSE_CB:
			rdp_read_ellipse_cb_order(s, orderInfo, &(orders->ellipse_cb));
			break;

		case ORDER_TYPE_GLYPH_INDEX:
			rdp_read_glyph_index_order(s, orderInfo, &(orders->glyph_index));
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
			rdp_recv_primary_order(rdp->orders, s, controlFlags);
			break;

		case ORDER_SECONDARY_CLASS:
			rdp_recv_secondary_order(rdp, s, controlFlags);
			break;

		case ORDER_ALTSEC_CLASS:
			rdp_recv_altsec_order(rdp, s, controlFlags);
			break;
	}
}

rdpOrders* orders_new()
{
	rdpOrders* orders;

	orders = (rdpOrders*) xzalloc(sizeof(rdpOrders));

	if (orders != NULL)
	{

	}

	return orders;
}

void orders_free(rdpOrders* orders)
{
	if (orders != NULL)
	{
		xfree(orders);
	}
}
