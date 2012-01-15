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

#include "window.h"
#include <freerdp/api.h>
#include <freerdp/graphics.h>
#include <freerdp/codec/bitmap.h>

#include "orders.h"

#ifdef WITH_DEBUG_ORDERS

static const char* const PRIMARY_DRAWING_ORDER_STRINGS[] =
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
	"",
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
	"",
	"FastGlyph",
	"EllipseSC",
	"EllipseCB",
	"GlyphIndex"
};

#define PRIMARY_DRAWING_ORDER_COUNT	(sizeof(PRIMARY_DRAWING_ORDER_STRINGS) / sizeof(PRIMARY_DRAWING_ORDER_STRINGS[0]))

static const char* const SECONDARY_DRAWING_ORDER_STRINGS[] =
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

#define SECONDARY_DRAWING_ORDER_COUNT	(sizeof(SECONDARY_DRAWING_ORDER_STRINGS) / sizeof(SECONDARY_DRAWING_ORDER_STRINGS[0]))

static const char* const ALTSEC_DRAWING_ORDER_STRINGS[] =
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

#define ALTSEC_DRAWING_ORDER_COUNT	(sizeof(ALTSEC_DRAWING_ORDER_STRINGS) / sizeof(ALTSEC_DRAWING_ORDER_STRINGS[0]))

#endif /* WITH_DEBUG_ORDERS */

static const uint8 PRIMARY_DRAWING_ORDER_FIELD_BYTES[] =
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
	0,
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
	0,
	FAST_GLYPH_ORDER_FIELD_BYTES,
	ELLIPSE_SC_ORDER_FIELD_BYTES,
	ELLIPSE_CB_ORDER_FIELD_BYTES,
	GLYPH_INDEX_ORDER_FIELD_BYTES
};

static const uint8 CBR2_BPP[] =
{
		0, 0, 0, 8, 16, 24, 32
};

static const uint8 CBR23_BPP[] =
{
		0, 0, 0, 8, 16, 24, 32
};

static const uint8 BMF_BPP[] =
{
		0, 1, 0, 8, 16, 24, 32
};

INLINE void update_read_coord(STREAM* s, sint32* coord, boolean delta)
{
	sint8 lsi8;
	sint16 lsi16;

	if (delta)
	{
		stream_read_uint8(s, lsi8);
		*coord += lsi8;
	}
	else
	{
		stream_read_uint16(s, lsi16);
		*coord = lsi16;
	}
}

INLINE void update_read_color(STREAM* s, uint32* color)
{
	uint8 byte;

	stream_read_uint8(s, byte);
	*color = byte;
	stream_read_uint8(s, byte);
	*color |= (byte << 8);
	stream_read_uint8(s, byte);
	*color |= (byte << 16);
}

INLINE void update_read_colorref(STREAM* s, uint32* color)
{
	uint8 byte;

	stream_read_uint8(s, byte);
	*color = byte;
	stream_read_uint8(s, byte);
	*color |= (byte << 8);
	stream_read_uint8(s, byte);
	*color |= (byte << 16);
	stream_seek_uint8(s);
}

INLINE void update_read_color_quad(STREAM* s, uint32* color)
{
	uint8 byte;

	stream_read_uint8(s, byte);
	*color = (byte << 16);
	stream_read_uint8(s, byte);
	*color |= (byte << 8);
	stream_read_uint8(s, byte);
	*color |= byte;
	stream_seek_uint8(s);
}

INLINE void update_read_2byte_unsigned(STREAM* s, uint32* value)
{
	uint8 byte;

	stream_read_uint8(s, byte);

	if (byte & 0x80)
	{
		*value = (byte & 0x7F) << 8;
		stream_read_uint8(s, byte);
		*value |= byte;
	}
	else
	{
		*value = (byte & 0x7F);
	}
}

INLINE void update_read_2byte_signed(STREAM* s, sint32* value)
{
	uint8 byte;
	boolean negative;

	stream_read_uint8(s, byte);

	negative = (byte & 0x40) ? true : false;

	*value = (byte & 0x3F);

	if (byte & 0x80)
	{
		stream_read_uint8(s, byte);
		*value = (*value << 8) | byte;
	}

	if (negative)
		*value *= -1;
}

INLINE void update_read_4byte_unsigned(STREAM* s, uint32* value)
{
	uint8 byte;
	uint8 count;

	stream_read_uint8(s, byte);

	count = (byte & 0xC0) >> 6;

	switch (count)
	{
		case 0:
			*value = (byte & 0x3F);
			break;

		case 1:
			*value = (byte & 0x3F) << 8;
			stream_read_uint8(s, byte);
			*value |= byte;
			break;

		case 2:
			*value = (byte & 0x3F) << 16;
			stream_read_uint8(s, byte);
			*value |= (byte << 8);
			stream_read_uint8(s, byte);
			*value |= byte;
			break;

		case 3:
			*value = (byte & 0x3F) << 24;
			stream_read_uint8(s, byte);
			*value |= (byte << 16);
			stream_read_uint8(s, byte);
			*value |= (byte << 8);
			stream_read_uint8(s, byte);
			*value |= byte;
			break;

		default:
			break;
	}
}

INLINE void update_read_delta(STREAM* s, sint32* value)
{
	uint8 byte;

	stream_read_uint8(s, byte);

	if (byte & 0x40)
		*value = (byte | ~0x3F);
	else
		*value = (byte & 0x3F);

	if (byte & 0x80)
	{
		stream_read_uint8(s, byte);
		*value = (*value << 8) | byte;
	}
}

INLINE void update_read_glyph_delta(STREAM* s, uint16* value)
{
	uint8 byte;

	stream_read_uint8(s, byte);

	if (byte == 0x80)
		stream_read_uint16(s, *value);
	else
		*value = (byte & 0x3F);
}

INLINE void update_seek_glyph_delta(STREAM* s)
{
	uint8 byte;

	stream_read_uint8(s, byte);

	if (byte & 0x80)
		stream_seek_uint8(s);
}

INLINE void update_read_brush(STREAM* s, rdpBrush* brush, uint8 fieldFlags)
{
	if (fieldFlags & ORDER_FIELD_01)
		stream_read_uint8(s, brush->x);

	if (fieldFlags & ORDER_FIELD_02)
		stream_read_uint8(s, brush->y);

	if (fieldFlags & ORDER_FIELD_03)
		stream_read_uint8(s, brush->style);

	if (fieldFlags & ORDER_FIELD_04)
		stream_read_uint8(s, brush->hatch);

	if (brush->style & CACHED_BRUSH)
	{
		brush->index = brush->hatch;

		brush->bpp = BMF_BPP[brush->style & 0x0F];

		if (brush->bpp == 0)
			brush->bpp = 1;
	}

	if (fieldFlags & ORDER_FIELD_05)
	{
		brush->data = (uint8*) brush->p8x8;
		stream_read_uint8(s, brush->data[7]);
		stream_read_uint8(s, brush->data[6]);
		stream_read_uint8(s, brush->data[5]);
		stream_read_uint8(s, brush->data[4]);
		stream_read_uint8(s, brush->data[3]);
		stream_read_uint8(s, brush->data[2]);
		stream_read_uint8(s, brush->data[1]);
		brush->data[0] = brush->hatch;
	}
}

INLINE void update_read_delta_rects(STREAM* s, DELTA_RECT* rectangles, int number)
{
	int i;
	uint8 flags = 0;
	uint8* zeroBits;
	int zeroBitsSize;

	if (number > 45)
		number = 45;

	zeroBitsSize = ((number + 1) / 2);

	stream_get_mark(s, zeroBits);
	stream_seek(s, zeroBitsSize);

	memset(rectangles, 0, sizeof(DELTA_RECT) * (number + 1));

	for (i = 1; i < number + 1; i++)
	{
		if ((i - 1) % 2 == 0)
			flags = zeroBits[(i - 1) / 2];

		if (~flags & 0x80)
			update_read_delta(s, &rectangles[i].left);

		if (~flags & 0x40)
			update_read_delta(s, &rectangles[i].top);

		if (~flags & 0x20)
			update_read_delta(s, &rectangles[i].width);
		else
			rectangles[i].width = rectangles[i - 1].width;

		if (~flags & 0x10)
			update_read_delta(s, &rectangles[i].height);
		else
			rectangles[i].height = rectangles[i - 1].height;

		rectangles[i].left = rectangles[i].left + rectangles[i - 1].left;
		rectangles[i].top = rectangles[i].top + rectangles[i - 1].top;

		flags <<= 4;
	}
}

INLINE void update_read_delta_points(STREAM* s, DELTA_POINT* points, int number, sint16 x, sint16 y)
{
	int i;
	uint8 flags = 0;
	uint8* zeroBits;
	int zeroBitsSize;

	zeroBitsSize = ((number + 3) / 4);

	stream_get_mark(s, zeroBits);
	stream_seek(s, zeroBitsSize);

	memset(points, 0, sizeof(DELTA_POINT) * number);

	for (i = 0; i < number; i++)
	{
		if (i % 4 == 0)
			flags = zeroBits[i / 4];

		if (~flags & 0x80)
			update_read_delta(s, &points[i].x);

		if (~flags & 0x40)
			update_read_delta(s, &points[i].y);

		flags <<= 2;
	}
}

/* Primary Drawing Orders */

void update_read_dstblt_order(STREAM* s, ORDER_INFO* orderInfo, DSTBLT_ORDER* dstblt)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		update_read_coord(s, &dstblt->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		update_read_coord(s, &dstblt->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		update_read_coord(s, &dstblt->nWidth, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		update_read_coord(s, &dstblt->nHeight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		stream_read_uint8(s, dstblt->bRop);
}

void update_read_patblt_order(STREAM* s, ORDER_INFO* orderInfo, PATBLT_ORDER* patblt)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		update_read_coord(s, &patblt->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		update_read_coord(s, &patblt->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		update_read_coord(s, &patblt->nWidth, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		update_read_coord(s, &patblt->nHeight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		stream_read_uint8(s, patblt->bRop);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		update_read_color(s, &patblt->backColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		update_read_color(s, &patblt->foreColor);

	update_read_brush(s, &patblt->brush, orderInfo->fieldFlags >> 7);
}

void update_read_scrblt_order(STREAM* s, ORDER_INFO* orderInfo, SCRBLT_ORDER* scrblt)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		update_read_coord(s, &scrblt->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		update_read_coord(s, &scrblt->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		update_read_coord(s, &scrblt->nWidth, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		update_read_coord(s, &scrblt->nHeight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		stream_read_uint8(s, scrblt->bRop);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		update_read_coord(s, &scrblt->nXSrc, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		update_read_coord(s, &scrblt->nYSrc, orderInfo->deltaCoordinates);
}

void update_read_opaque_rect_order(STREAM* s, ORDER_INFO* orderInfo, OPAQUE_RECT_ORDER* opaque_rect)
{
	uint8 byte;

	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		update_read_coord(s, &opaque_rect->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		update_read_coord(s, &opaque_rect->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		update_read_coord(s, &opaque_rect->nWidth, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		update_read_coord(s, &opaque_rect->nHeight, orderInfo->deltaCoordinates);

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

void update_read_draw_nine_grid_order(STREAM* s, ORDER_INFO* orderInfo, DRAW_NINE_GRID_ORDER* draw_nine_grid)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		update_read_coord(s, &draw_nine_grid->srcLeft, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		update_read_coord(s, &draw_nine_grid->srcTop, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		update_read_coord(s, &draw_nine_grid->srcRight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		update_read_coord(s, &draw_nine_grid->srcBottom, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		stream_read_uint16(s, draw_nine_grid->bitmapId);
}

void update_read_multi_dstblt_order(STREAM* s, ORDER_INFO* orderInfo, MULTI_DSTBLT_ORDER* multi_dstblt)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		update_read_coord(s, &multi_dstblt->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		update_read_coord(s, &multi_dstblt->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		update_read_coord(s, &multi_dstblt->nWidth, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		update_read_coord(s, &multi_dstblt->nHeight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		stream_read_uint8(s, multi_dstblt->bRop);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		stream_read_uint8(s, multi_dstblt->numRectangles);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
	{
		stream_read_uint16(s, multi_dstblt->cbData);
		update_read_delta_rects(s, multi_dstblt->rectangles, multi_dstblt->numRectangles);
	}
}

void update_read_multi_patblt_order(STREAM* s, ORDER_INFO* orderInfo, MULTI_PATBLT_ORDER* multi_patblt)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		update_read_coord(s, &multi_patblt->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		update_read_coord(s, &multi_patblt->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		update_read_coord(s, &multi_patblt->nWidth, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		update_read_coord(s, &multi_patblt->nHeight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		stream_read_uint8(s, multi_patblt->bRop);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		update_read_color(s, &multi_patblt->backColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		update_read_color(s, &multi_patblt->foreColor);

	update_read_brush(s, &multi_patblt->brush, orderInfo->fieldFlags >> 7);

	if (orderInfo->fieldFlags & ORDER_FIELD_13)
		stream_read_uint8(s, multi_patblt->numRectangles);

	if (orderInfo->fieldFlags & ORDER_FIELD_14)
	{
		stream_read_uint16(s, multi_patblt->cbData);
		update_read_delta_rects(s, multi_patblt->rectangles, multi_patblt->numRectangles);
	}
}

void update_read_multi_scrblt_order(STREAM* s, ORDER_INFO* orderInfo, MULTI_SCRBLT_ORDER* multi_scrblt)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		update_read_coord(s, &multi_scrblt->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		update_read_coord(s, &multi_scrblt->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		update_read_coord(s, &multi_scrblt->nWidth, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		update_read_coord(s, &multi_scrblt->nHeight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		stream_read_uint8(s, multi_scrblt->bRop);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		update_read_coord(s, &multi_scrblt->nXSrc, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		update_read_coord(s, &multi_scrblt->nYSrc, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		stream_read_uint8(s, multi_scrblt->numRectangles);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
	{
		stream_read_uint16(s, multi_scrblt->cbData);
		update_read_delta_rects(s, multi_scrblt->rectangles, multi_scrblt->numRectangles);
	}
}

void update_read_multi_opaque_rect_order(STREAM* s, ORDER_INFO* orderInfo, MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect)
{
	uint8 byte;

	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		update_read_coord(s, &multi_opaque_rect->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		update_read_coord(s, &multi_opaque_rect->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		update_read_coord(s, &multi_opaque_rect->nWidth, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		update_read_coord(s, &multi_opaque_rect->nHeight, orderInfo->deltaCoordinates);

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
		stream_read_uint8(s, multi_opaque_rect->numRectangles);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
	{
		stream_read_uint16(s, multi_opaque_rect->cbData);
		update_read_delta_rects(s, multi_opaque_rect->rectangles, multi_opaque_rect->numRectangles);
	}
}

void update_read_multi_draw_nine_grid_order(STREAM* s, ORDER_INFO* orderInfo, MULTI_DRAW_NINE_GRID_ORDER* multi_draw_nine_grid)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		update_read_coord(s, &multi_draw_nine_grid->srcLeft, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		update_read_coord(s, &multi_draw_nine_grid->srcTop, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		update_read_coord(s, &multi_draw_nine_grid->srcRight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		update_read_coord(s, &multi_draw_nine_grid->srcBottom, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		stream_read_uint16(s, multi_draw_nine_grid->bitmapId);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		stream_read_uint8(s, multi_draw_nine_grid->nDeltaEntries);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
	{
		stream_read_uint16(s, multi_draw_nine_grid->cbData);
		stream_seek(s, multi_draw_nine_grid->cbData);
	}
}

void update_read_line_to_order(STREAM* s, ORDER_INFO* orderInfo, LINE_TO_ORDER* line_to)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		stream_read_uint16(s, line_to->backMode);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		update_read_coord(s, &line_to->nXStart, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		update_read_coord(s, &line_to->nYStart, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		update_read_coord(s, &line_to->nXEnd, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		update_read_coord(s, &line_to->nYEnd, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		update_read_color(s, &line_to->backColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		stream_read_uint8(s, line_to->bRop2);

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		stream_read_uint8(s, line_to->penStyle);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
		stream_read_uint8(s, line_to->penWidth);

	if (orderInfo->fieldFlags & ORDER_FIELD_10)
		update_read_color(s, &line_to->penColor);
}

void update_read_polyline_order(STREAM* s, ORDER_INFO* orderInfo, POLYLINE_ORDER* polyline)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		update_read_coord(s, &polyline->xStart, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		update_read_coord(s, &polyline->yStart, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		stream_read_uint8(s, polyline->bRop2);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		stream_seek_uint16(s);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		update_read_color(s, &polyline->penColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		stream_read_uint8(s, polyline->numPoints);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
	{
		stream_read_uint8(s, polyline->cbData);

		if (polyline->points == NULL)
			polyline->points = (DELTA_POINT*) xmalloc(sizeof(DELTA_POINT) * polyline->numPoints);
		else
			polyline->points = (DELTA_POINT*) xrealloc(polyline->points, sizeof(DELTA_POINT) * polyline->numPoints);

		update_read_delta_points(s, polyline->points, polyline->numPoints, polyline->xStart, polyline->yStart);
	}
}

void update_read_memblt_order(STREAM* s, ORDER_INFO* orderInfo, MEMBLT_ORDER* memblt)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		stream_read_uint16(s, memblt->cacheId);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		update_read_coord(s, &memblt->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		update_read_coord(s, &memblt->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		update_read_coord(s, &memblt->nWidth, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		update_read_coord(s, &memblt->nHeight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		stream_read_uint8(s, memblt->bRop);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		update_read_coord(s, &memblt->nXSrc, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		update_read_coord(s, &memblt->nYSrc, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
		stream_read_uint16(s, memblt->cacheIndex);

	memblt->colorIndex = (memblt->cacheId >> 8);
	memblt->cacheId = (memblt->cacheId & 0xFF);
}

void update_read_mem3blt_order(STREAM* s, ORDER_INFO* orderInfo, MEM3BLT_ORDER* mem3blt)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		stream_read_uint16(s, mem3blt->cacheId);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		update_read_coord(s, &mem3blt->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		update_read_coord(s, &mem3blt->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		update_read_coord(s, &mem3blt->nWidth, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		update_read_coord(s, &mem3blt->nHeight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		stream_read_uint8(s, mem3blt->bRop);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		update_read_coord(s, &mem3blt->nXSrc, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		update_read_coord(s, &mem3blt->nYSrc, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
		update_read_color(s, &mem3blt->backColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_10)
		update_read_color(s, &mem3blt->foreColor);

	update_read_brush(s, &mem3blt->brush, orderInfo->fieldFlags >> 10);

	if (orderInfo->fieldFlags & ORDER_FIELD_16)
		stream_read_uint16(s, mem3blt->cacheIndex);

	mem3blt->colorIndex = (mem3blt->cacheId >> 8);
	mem3blt->cacheId = (mem3blt->cacheId & 0xFF);
}

void update_read_save_bitmap_order(STREAM* s, ORDER_INFO* orderInfo, SAVE_BITMAP_ORDER* save_bitmap)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		stream_read_uint32(s, save_bitmap->savedBitmapPosition);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		update_read_coord(s, &save_bitmap->nLeftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		update_read_coord(s, &save_bitmap->nTopRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		update_read_coord(s, &save_bitmap->nRightRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		update_read_coord(s, &save_bitmap->nBottomRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		stream_read_uint8(s, save_bitmap->operation);
}

void update_read_glyph_index_order(STREAM* s, ORDER_INFO* orderInfo, GLYPH_INDEX_ORDER* glyph_index)
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
		update_read_color(s, &glyph_index->backColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		update_read_color(s, &glyph_index->foreColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		stream_read_uint16(s, glyph_index->bkLeft);

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		stream_read_uint16(s, glyph_index->bkTop);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
		stream_read_uint16(s, glyph_index->bkRight);

	if (orderInfo->fieldFlags & ORDER_FIELD_10)
		stream_read_uint16(s, glyph_index->bkBottom);

	if (orderInfo->fieldFlags & ORDER_FIELD_11)
		stream_read_uint16(s, glyph_index->opLeft);

	if (orderInfo->fieldFlags & ORDER_FIELD_12)
		stream_read_uint16(s, glyph_index->opTop);

	if (orderInfo->fieldFlags & ORDER_FIELD_13)
		stream_read_uint16(s, glyph_index->opRight);

	if (orderInfo->fieldFlags & ORDER_FIELD_14)
		stream_read_uint16(s, glyph_index->opBottom);

	update_read_brush(s, &glyph_index->brush, orderInfo->fieldFlags >> 14);

	if (orderInfo->fieldFlags & ORDER_FIELD_20)
		stream_read_uint16(s, glyph_index->x);

	if (orderInfo->fieldFlags & ORDER_FIELD_21)
		stream_read_uint16(s, glyph_index->y);

	if (orderInfo->fieldFlags & ORDER_FIELD_22)
	{
		stream_read_uint8(s, glyph_index->cbData);
		memcpy(glyph_index->data, s->p, glyph_index->cbData);
		stream_seek(s, glyph_index->cbData);
	}
}

void update_read_fast_index_order(STREAM* s, ORDER_INFO* orderInfo, FAST_INDEX_ORDER* fast_index)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		stream_read_uint8(s, fast_index->cacheId);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
	{
		stream_read_uint8(s, fast_index->ulCharInc);
		stream_read_uint8(s, fast_index->flAccel);
	}

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		update_read_color(s, &fast_index->backColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		update_read_color(s, &fast_index->foreColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		update_read_coord(s, &fast_index->bkLeft, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		update_read_coord(s, &fast_index->bkTop, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		update_read_coord(s, &fast_index->bkRight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		update_read_coord(s, &fast_index->bkBottom, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
		update_read_coord(s, &fast_index->opLeft, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_10)
		update_read_coord(s, &fast_index->opTop, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_11)
		update_read_coord(s, &fast_index->opRight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_12)
		update_read_coord(s, &fast_index->opBottom, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_13)
		update_read_coord(s, &fast_index->x, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_14)
		update_read_coord(s, &fast_index->y, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_15)
	{
		stream_read_uint8(s, fast_index->cbData);
		memcpy(fast_index->data, s->p, fast_index->cbData);
		stream_seek(s, fast_index->cbData);
	}
}

void update_read_fast_glyph_order(STREAM* s, ORDER_INFO* orderInfo, FAST_GLYPH_ORDER* fast_glyph)
{
	GLYPH_DATA_V2* glyph;
	uint8* phold;

	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		stream_read_uint8(s, fast_glyph->cacheId);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
	{
		stream_read_uint8(s, fast_glyph->ulCharInc);
		stream_read_uint8(s, fast_glyph->flAccel);
	}

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		update_read_color(s, &fast_glyph->backColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		update_read_color(s, &fast_glyph->foreColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		update_read_coord(s, &fast_glyph->bkLeft, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		update_read_coord(s, &fast_glyph->bkTop, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		update_read_coord(s, &fast_glyph->bkRight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		update_read_coord(s, &fast_glyph->bkBottom, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
		update_read_coord(s, &fast_glyph->opLeft, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_10)
		update_read_coord(s, &fast_glyph->opTop, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_11)
		update_read_coord(s, &fast_glyph->opRight, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_12)
		update_read_coord(s, &fast_glyph->opBottom, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_13)
		update_read_coord(s, &fast_glyph->x, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_14)
		update_read_coord(s, &fast_glyph->y, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_15)
	{
		stream_read_uint8(s, fast_glyph->cbData);
		memcpy(fast_glyph->data, s->p, fast_glyph->cbData);
		phold = s->p;
		stream_seek(s, 1);
		if ((fast_glyph->cbData > 1) && (fast_glyph->glyph_data == NULL))
		{
			/* parse optional glyph data */
			glyph = (GLYPH_DATA_V2*) xmalloc(sizeof(GLYPH_DATA_V2));
			glyph->cacheIndex = fast_glyph->data[0];
			update_read_2byte_signed(s, &glyph->x);
			update_read_2byte_signed(s, &glyph->y);
			update_read_2byte_unsigned(s, &glyph->cx);
			update_read_2byte_unsigned(s, &glyph->cy);
			glyph->cb = ((glyph->cx + 7) / 8) * glyph->cy;
			glyph->cb += ((glyph->cb % 4) > 0) ? 4 - (glyph->cb % 4) : 0;
			glyph->aj = (uint8*) xmalloc(glyph->cb);
			stream_read(s, glyph->aj, glyph->cb);
			fast_glyph->glyph_data = glyph;
		}
		s->p = phold + fast_glyph->cbData;
	}
}

void update_read_polygon_sc_order(STREAM* s, ORDER_INFO* orderInfo, POLYGON_SC_ORDER* polygon_sc)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		update_read_coord(s, &polygon_sc->xStart, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		update_read_coord(s, &polygon_sc->yStart, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		stream_read_uint8(s, polygon_sc->bRop2);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		stream_read_uint8(s, polygon_sc->fillMode);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		update_read_color(s, &polygon_sc->brushColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		stream_read_uint8(s, polygon_sc->nDeltaEntries);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
	{
		stream_read_uint8(s, polygon_sc->cbData);
		stream_seek(s, polygon_sc->cbData);
	}
}

void update_read_polygon_cb_order(STREAM* s, ORDER_INFO* orderInfo, POLYGON_CB_ORDER* polygon_cb)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		update_read_coord(s, &polygon_cb->xStart, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		update_read_coord(s, &polygon_cb->yStart, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		stream_read_uint8(s, polygon_cb->bRop2);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		stream_read_uint8(s, polygon_cb->fillMode);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		update_read_color(s, &polygon_cb->backColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		update_read_color(s, &polygon_cb->foreColor);

	update_read_brush(s, &polygon_cb->brush, orderInfo->fieldFlags >> 6);

	if (orderInfo->fieldFlags & ORDER_FIELD_12)
		stream_read_uint8(s, polygon_cb->nDeltaEntries);

	if (orderInfo->fieldFlags & ORDER_FIELD_13)
	{
		stream_read_uint8(s, polygon_cb->cbData);
		stream_seek(s, polygon_cb->cbData);
	}
}

void update_read_ellipse_sc_order(STREAM* s, ORDER_INFO* orderInfo, ELLIPSE_SC_ORDER* ellipse_sc)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		update_read_coord(s, &ellipse_sc->leftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		update_read_coord(s, &ellipse_sc->topRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		update_read_coord(s, &ellipse_sc->rightRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		update_read_coord(s, &ellipse_sc->bottomRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		stream_read_uint8(s, ellipse_sc->bRop2);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		stream_read_uint8(s, ellipse_sc->fillMode);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		update_read_color(s, &ellipse_sc->color);
}

void update_read_ellipse_cb_order(STREAM* s, ORDER_INFO* orderInfo, ELLIPSE_CB_ORDER* ellipse_cb)
{
	if (orderInfo->fieldFlags & ORDER_FIELD_01)
		update_read_coord(s, &ellipse_cb->leftRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_02)
		update_read_coord(s, &ellipse_cb->topRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_03)
		update_read_coord(s, &ellipse_cb->rightRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_04)
		update_read_coord(s, &ellipse_cb->bottomRect, orderInfo->deltaCoordinates);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
		stream_read_uint8(s, ellipse_cb->bRop2);

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
		stream_read_uint8(s, ellipse_cb->fillMode);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		update_read_color(s, &ellipse_cb->backColor);

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		update_read_color(s, &ellipse_cb->foreColor);

	update_read_brush(s, &ellipse_cb->brush, orderInfo->fieldFlags >> 8);
}

/* Secondary Drawing Orders */

void update_read_cache_bitmap_order(STREAM* s, CACHE_BITMAP_ORDER* cache_bitmap_order, boolean compressed, uint16 flags)
{
	stream_read_uint8(s, cache_bitmap_order->cacheId); /* cacheId (1 byte) */
	stream_seek_uint8(s); /* pad1Octet (1 byte) */
	stream_read_uint8(s, cache_bitmap_order->bitmapWidth); /* bitmapWidth (1 byte) */
	stream_read_uint8(s, cache_bitmap_order->bitmapHeight); /* bitmapHeight (1 byte) */
	stream_read_uint8(s, cache_bitmap_order->bitmapBpp); /* bitmapBpp (1 byte) */
	stream_read_uint16(s, cache_bitmap_order->bitmapLength); /* bitmapLength (2 bytes) */
	stream_read_uint16(s, cache_bitmap_order->cacheIndex); /* cacheIndex (2 bytes) */

	if (compressed)
	{
		if ((flags & NO_BITMAP_COMPRESSION_HDR) == 0)
		{
			uint8* bitmapComprHdr = (uint8*) &(cache_bitmap_order->bitmapComprHdr);
			stream_read(s, bitmapComprHdr, 8); /* bitmapComprHdr (8 bytes) */
			cache_bitmap_order->bitmapLength -= 8;
		}
		stream_get_mark(s, cache_bitmap_order->bitmapDataStream);
		stream_seek(s, cache_bitmap_order->bitmapLength);
	}
	else
	{
		stream_get_mark(s, cache_bitmap_order->bitmapDataStream);
		stream_seek(s, cache_bitmap_order->bitmapLength); /* bitmapDataStream */
	}
	cache_bitmap_order->compressed = compressed;
}

void update_read_cache_bitmap_v2_order(STREAM* s, CACHE_BITMAP_V2_ORDER* cache_bitmap_v2_order, boolean compressed, uint16 flags)
{
	uint8 bitsPerPixelId;

	cache_bitmap_v2_order->cacheId = flags & 0x0003;
	cache_bitmap_v2_order->flags = (flags & 0xFF80) >> 7;

	bitsPerPixelId = (flags & 0x0078) >> 3;
	cache_bitmap_v2_order->bitmapBpp = CBR2_BPP[bitsPerPixelId];

	if (cache_bitmap_v2_order->flags & CBR2_PERSISTENT_KEY_PRESENT)
	{
		stream_read_uint32(s, cache_bitmap_v2_order->key1); /* key1 (4 bytes) */
		stream_read_uint32(s, cache_bitmap_v2_order->key2); /* key2 (4 bytes) */
	}

	if (cache_bitmap_v2_order->flags & CBR2_HEIGHT_SAME_AS_WIDTH)
	{
		update_read_2byte_unsigned(s, &cache_bitmap_v2_order->bitmapWidth); /* bitmapWidth */
		cache_bitmap_v2_order->bitmapHeight = cache_bitmap_v2_order->bitmapWidth;
	}
	else
	{
		update_read_2byte_unsigned(s, &cache_bitmap_v2_order->bitmapWidth); /* bitmapWidth */
		update_read_2byte_unsigned(s, &cache_bitmap_v2_order->bitmapHeight); /* bitmapHeight */
	}

	update_read_4byte_unsigned(s, &cache_bitmap_v2_order->bitmapLength); /* bitmapLength */
	update_read_2byte_unsigned(s, &cache_bitmap_v2_order->cacheIndex); /* cacheIndex */

	if (cache_bitmap_v2_order->flags & CBR2_DO_NOT_CACHE)
		cache_bitmap_v2_order->cacheIndex = BITMAP_CACHE_WAITING_LIST_INDEX;

	if (compressed)
	{
		if (!(cache_bitmap_v2_order->flags & CBR2_NO_BITMAP_COMPRESSION_HDR))
		{
			stream_read_uint16(s, cache_bitmap_v2_order->cbCompFirstRowSize); /* cbCompFirstRowSize (2 bytes) */
			stream_read_uint16(s, cache_bitmap_v2_order->cbCompMainBodySize); /* cbCompMainBodySize (2 bytes) */
			stream_read_uint16(s, cache_bitmap_v2_order->cbScanWidth); /* cbScanWidth (2 bytes) */
			stream_read_uint16(s, cache_bitmap_v2_order->cbUncompressedSize); /* cbUncompressedSize (2 bytes) */
			cache_bitmap_v2_order->bitmapLength = cache_bitmap_v2_order->cbCompMainBodySize;
		}

		stream_get_mark(s, cache_bitmap_v2_order->bitmapDataStream);
		stream_seek(s, cache_bitmap_v2_order->bitmapLength);
	}
	else
	{
		stream_get_mark(s, cache_bitmap_v2_order->bitmapDataStream);
		stream_seek(s, cache_bitmap_v2_order->bitmapLength);
	}
	cache_bitmap_v2_order->compressed = compressed;
}

void update_read_cache_bitmap_v3_order(STREAM* s, CACHE_BITMAP_V3_ORDER* cache_bitmap_v3_order, boolean compressed, uint16 flags)
{
	uint8 bitsPerPixelId;
	BITMAP_DATA_EX* bitmapData;

	cache_bitmap_v3_order->cacheId = flags & 0x00000003;
	cache_bitmap_v3_order->flags = (flags & 0x0000FF80) >> 7;

	bitsPerPixelId = (flags & 0x00000078) >> 3;
	cache_bitmap_v3_order->bpp = CBR23_BPP[bitsPerPixelId];

	stream_read_uint16(s, cache_bitmap_v3_order->cacheIndex); /* cacheIndex (2 bytes) */
	stream_read_uint32(s, cache_bitmap_v3_order->key1); /* key1 (4 bytes) */
	stream_read_uint32(s, cache_bitmap_v3_order->key2); /* key2 (4 bytes) */

	bitmapData = &cache_bitmap_v3_order->bitmapData;

	stream_read_uint8(s, bitmapData->bpp);
	stream_seek_uint8(s); /* reserved1 (1 byte) */
	stream_seek_uint8(s); /* reserved2 (1 byte) */
	stream_read_uint8(s, bitmapData->codecID); /* codecID (1 byte) */
	stream_read_uint16(s, bitmapData->width); /* width (2 bytes) */
	stream_read_uint16(s, bitmapData->height); /* height (2 bytes) */
	stream_read_uint32(s, bitmapData->length); /* length (4 bytes) */

	if (bitmapData->data == NULL)
		bitmapData->data = (uint8*) xmalloc(bitmapData->length);
	else
		bitmapData->data = (uint8*) xrealloc(bitmapData->data, bitmapData->length);

	stream_read(s, bitmapData->data, bitmapData->length);
}

void update_read_cache_color_table_order(STREAM* s, CACHE_COLOR_TABLE_ORDER* cache_color_table_order, uint16 flags)
{
	int i;
	uint32* colorTable;

	stream_read_uint8(s, cache_color_table_order->cacheIndex); /* cacheIndex (1 byte) */
	stream_read_uint8(s, cache_color_table_order->numberColors); /* numberColors (2 bytes) */

	colorTable = cache_color_table_order->colorTable;

	if (colorTable == NULL)
		colorTable = (uint32*) xmalloc(cache_color_table_order->numberColors * 4);
	else
		colorTable = (uint32*) xrealloc(colorTable, cache_color_table_order->numberColors * 4);

	for (i = 0; i < (int) cache_color_table_order->numberColors; i++)
	{
		update_read_color_quad(s, &colorTable[i]);
	}

	cache_color_table_order->colorTable = colorTable;
}

void update_read_cache_glyph_order(STREAM* s, CACHE_GLYPH_ORDER* cache_glyph_order, uint16 flags)
{
	int i;
	sint16 lsi16;
	GLYPH_DATA* glyph;

	stream_read_uint8(s, cache_glyph_order->cacheId); /* cacheId (1 byte) */
	stream_read_uint8(s, cache_glyph_order->cGlyphs); /* cGlyphs (1 byte) */

	for (i = 0; i < (int) cache_glyph_order->cGlyphs; i++)
	{
		if (cache_glyph_order->glyphData[i] == NULL)
		{
			cache_glyph_order->glyphData[i] = (GLYPH_DATA*) xmalloc(sizeof(GLYPH_DATA));
		}
		glyph = cache_glyph_order->glyphData[i];

		stream_read_uint16(s, glyph->cacheIndex);
		stream_read_uint16(s, lsi16);
		glyph->x = lsi16;
		stream_read_uint16(s, lsi16);
		glyph->y = lsi16;
		stream_read_uint16(s, glyph->cx);
		stream_read_uint16(s, glyph->cy);

		glyph->cb = ((glyph->cx + 7) / 8) * glyph->cy;
		glyph->cb += ((glyph->cb % 4) > 0) ? 4 - (glyph->cb % 4) : 0;

		glyph->aj = (uint8*) xmalloc(glyph->cb);

		stream_read(s, glyph->aj, glyph->cb);
	}

	if (flags & CG_GLYPH_UNICODE_PRESENT)
		stream_seek(s, cache_glyph_order->cGlyphs * 2);
}

void update_read_cache_glyph_v2_order(STREAM* s, CACHE_GLYPH_V2_ORDER* cache_glyph_v2_order, uint16 flags)
{
	int i;
	GLYPH_DATA_V2* glyph;

	cache_glyph_v2_order->cacheId = (flags & 0x000F);
	cache_glyph_v2_order->flags = (flags & 0x00F0) >> 4;
	cache_glyph_v2_order->cGlyphs = (flags & 0xFF00) >> 8;

	for (i = 0; i < (int) cache_glyph_v2_order->cGlyphs; i++)
	{
		if (cache_glyph_v2_order->glyphData[i] == NULL)
		{
			cache_glyph_v2_order->glyphData[i] = (GLYPH_DATA_V2*) xmalloc(sizeof(GLYPH_DATA_V2));
		}
		glyph = cache_glyph_v2_order->glyphData[i];

		stream_read_uint8(s, glyph->cacheIndex);
		update_read_2byte_signed(s, &glyph->x);
		update_read_2byte_signed(s, &glyph->y);
		update_read_2byte_unsigned(s, &glyph->cx);
		update_read_2byte_unsigned(s, &glyph->cy);

		glyph->cb = ((glyph->cx + 7) / 8) * glyph->cy;
		glyph->cb += ((glyph->cb % 4) > 0) ? 4 - (glyph->cb % 4) : 0;

		glyph->aj = (uint8*) xmalloc(glyph->cb);

		stream_read(s, glyph->aj, glyph->cb);
	}

	if (flags & CG_GLYPH_UNICODE_PRESENT)
		stream_seek(s, cache_glyph_v2_order->cGlyphs * 2);
}

void update_decompress_brush(STREAM* s, uint8* output, uint8 bpp)
{
	int index;
	int x, y, k;
	uint8 byte = 0;
	uint8* palette;
	int bytesPerPixel;

	palette = s->p + 16;
	bytesPerPixel = ((bpp + 1) / 8);

	for (y = 7; y >= 0; y--)
	{
		for (x = 0; x < 8; x++)
		{
			if ((x % 4) == 0)
				stream_read_uint8(s, byte);

			index = ((byte >> ((3 - (x % 4)) * 2)) & 0x03);

			for (k = 0; k < bytesPerPixel; k++)
			{
				output[((y * 8 + x) * bytesPerPixel) + k] = palette[(index * bytesPerPixel) + k];
			}
		}
	}
}

void update_read_cache_brush_order(STREAM* s, CACHE_BRUSH_ORDER* cache_brush_order, uint16 flags)
{
	int i;
	int size;
	uint8 iBitmapFormat;
	boolean compressed = false;

	stream_read_uint8(s, cache_brush_order->index); /* cacheEntry (1 byte) */

	stream_read_uint8(s, iBitmapFormat); /* iBitmapFormat (1 byte) */
	cache_brush_order->bpp = BMF_BPP[iBitmapFormat];

	stream_read_uint8(s, cache_brush_order->cx); /* cx (1 byte) */
	stream_read_uint8(s, cache_brush_order->cy); /* cy (1 byte) */
	stream_read_uint8(s, cache_brush_order->style); /* style (1 byte) */
	stream_read_uint8(s, cache_brush_order->length); /* iBytes (1 byte) */

	if ((cache_brush_order->cx == 8) && (cache_brush_order->cy == 8))
	{
		size = (cache_brush_order->bpp == 1) ? 8 : 8 * 8 * cache_brush_order->bpp;

		cache_brush_order->data = (uint8*) xmalloc(size);

		if (cache_brush_order->bpp == 1)
		{
			if (cache_brush_order->length != 8)
			{
				printf("incompatible 1bpp brush of length:%d\n", cache_brush_order->length);
				return;
			}

			/* rows are encoded in reverse order */

			for (i = 7; i >= 0; i--)
			{
				stream_read_uint8(s, cache_brush_order->data[i]);
			}
		}
		else
		{
			if ((iBitmapFormat == BMF_8BPP) && (cache_brush_order->length == 20))
				compressed = true;
			else if ((iBitmapFormat == BMF_16BPP) && (cache_brush_order->length == 24))
				compressed = true;
			else if ((iBitmapFormat == BMF_32BPP) && (cache_brush_order->length == 32))
				compressed = true;

			if (compressed != false)
			{
				/* compressed brush */
				update_decompress_brush(s, cache_brush_order->data, cache_brush_order->bpp);
			}
			else
			{
				/* uncompressed brush */
				int scanline = (cache_brush_order->bpp / 8) * 8;

				for (i = 7; i >= 0; i--)
				{
					stream_read(s, &cache_brush_order->data[i * scanline], scanline);
				}
			}
		}
	}
}

/* Alternate Secondary Drawing Orders */

void update_read_create_offscreen_bitmap_order(STREAM* s, CREATE_OFFSCREEN_BITMAP_ORDER* create_offscreen_bitmap)
{
	uint16 flags;
	boolean deleteListPresent;
	OFFSCREEN_DELETE_LIST* deleteList;

	stream_read_uint16(s, flags); /* flags (2 bytes) */
	create_offscreen_bitmap->id = flags & 0x7FFF;
	deleteListPresent = (flags & 0x8000) ? true : false;

	stream_read_uint16(s, create_offscreen_bitmap->cx); /* cx (2 bytes) */
	stream_read_uint16(s, create_offscreen_bitmap->cy); /* cy (2 bytes) */

	deleteList = &(create_offscreen_bitmap->deleteList);
	if (deleteListPresent)
	{
		int i;

		stream_read_uint16(s, deleteList->cIndices);

		if (deleteList->cIndices > deleteList->sIndices)
		{
			deleteList->sIndices = deleteList->cIndices;
			deleteList->indices = xrealloc(deleteList->indices, deleteList->sIndices * 2);
		}

		for (i = 0; i < (int) deleteList->cIndices; i++)
		{
			stream_read_uint16(s, deleteList->indices[i]);
		}
	}
	else
	{
		deleteList->cIndices = 0;
	}
}

void update_read_switch_surface_order(STREAM* s, SWITCH_SURFACE_ORDER* switch_surface)
{
	stream_read_uint16(s, switch_surface->bitmapId); /* bitmapId (2 bytes) */
}

void update_read_create_nine_grid_bitmap_order(STREAM* s, CREATE_NINE_GRID_BITMAP_ORDER* create_nine_grid_bitmap)
{
	NINE_GRID_BITMAP_INFO* nineGridInfo;

	stream_read_uint8(s, create_nine_grid_bitmap->bitmapBpp); /* bitmapBpp (1 byte) */
	stream_read_uint16(s, create_nine_grid_bitmap->bitmapId); /* bitmapId (2 bytes) */

	nineGridInfo = &(create_nine_grid_bitmap->nineGridInfo);
	stream_read_uint32(s, nineGridInfo->flFlags); /* flFlags (4 bytes) */
	stream_read_uint16(s, nineGridInfo->ulLeftWidth); /* ulLeftWidth (2 bytes) */
	stream_read_uint16(s, nineGridInfo->ulRightWidth); /* ulRightWidth (2 bytes) */
	stream_read_uint16(s, nineGridInfo->ulTopHeight); /* ulTopHeight (2 bytes) */
	stream_read_uint16(s, nineGridInfo->ulBottomHeight); /* ulBottomHeight (2 bytes) */
	update_read_colorref(s, &nineGridInfo->crTransparent); /* crTransparent (4 bytes) */
}

void update_read_frame_marker_order(STREAM* s, FRAME_MARKER_ORDER* frame_marker)
{
	stream_read_uint32(s, frame_marker->action); /* action (4 bytes) */
}

void update_read_stream_bitmap_first_order(STREAM* s, STREAM_BITMAP_FIRST_ORDER* stream_bitmap_first)
{
	stream_read_uint8(s, stream_bitmap_first->bitmapFlags); /* bitmapFlags (1 byte) */
	stream_read_uint8(s, stream_bitmap_first->bitmapBpp); /* bitmapBpp (1 byte) */
	stream_read_uint16(s, stream_bitmap_first->bitmapType); /* bitmapType (2 bytes) */
	stream_read_uint16(s, stream_bitmap_first->bitmapWidth); /* bitmapWidth (2 bytes) */
	stream_read_uint16(s, stream_bitmap_first->bitmapHeight); /* bitmapHeigth (2 bytes) */

	if (stream_bitmap_first->bitmapFlags & STREAM_BITMAP_V2)
		stream_read_uint32(s, stream_bitmap_first->bitmapSize); /* bitmapSize (4 bytes) */
	else
		stream_read_uint16(s, stream_bitmap_first->bitmapSize); /* bitmapSize (2 bytes) */

	stream_read_uint16(s, stream_bitmap_first->bitmapBlockSize); /* bitmapBlockSize (2 bytes) */
	stream_seek(s, stream_bitmap_first->bitmapBlockSize); /* bitmapBlock */
}

void update_read_stream_bitmap_next_order(STREAM* s, STREAM_BITMAP_FIRST_ORDER* stream_bitmap_next)
{
	stream_read_uint8(s, stream_bitmap_next->bitmapFlags); /* bitmapFlags (1 byte) */
	stream_read_uint16(s, stream_bitmap_next->bitmapType); /* bitmapType (2 bytes) */
	stream_read_uint16(s, stream_bitmap_next->bitmapBlockSize); /* bitmapBlockSize (2 bytes) */
	stream_seek(s, stream_bitmap_next->bitmapBlockSize); /* bitmapBlock */
}

void update_read_draw_gdiplus_first_order(STREAM* s, DRAW_GDIPLUS_FIRST_ORDER* draw_gdiplus_first)
{
	stream_seek_uint8(s); /* pad1Octet (1 byte) */
	stream_read_uint16(s, draw_gdiplus_first->cbSize); /* cbSize (2 bytes) */
	stream_read_uint32(s, draw_gdiplus_first->cbTotalSize); /* cbTotalSize (4 bytes) */
	stream_read_uint32(s, draw_gdiplus_first->cbTotalEmfSize); /* cbTotalEmfSize (4 bytes) */
	stream_seek(s, draw_gdiplus_first->cbSize); /* emfRecords */
}

void update_read_draw_gdiplus_next_order(STREAM* s, DRAW_GDIPLUS_NEXT_ORDER* draw_gdiplus_next)
{
	stream_seek_uint8(s); /* pad1Octet (1 byte) */
	stream_read_uint16(s, draw_gdiplus_next->cbSize); /* cbSize (2 bytes) */
	stream_seek(s, draw_gdiplus_next->cbSize); /* emfRecords */
}

void update_read_draw_gdiplus_end_order(STREAM* s, DRAW_GDIPLUS_END_ORDER* draw_gdiplus_end)
{
	stream_seek_uint8(s); /* pad1Octet (1 byte) */
	stream_read_uint16(s, draw_gdiplus_end->cbSize); /* cbSize (2 bytes) */
	stream_read_uint32(s, draw_gdiplus_end->cbTotalSize); /* cbTotalSize (4 bytes) */
	stream_read_uint32(s, draw_gdiplus_end->cbTotalEmfSize); /* cbTotalEmfSize (4 bytes) */
	stream_seek(s, draw_gdiplus_end->cbSize); /* emfRecords */
}

void update_read_draw_gdiplus_cache_first_order(STREAM* s, DRAW_GDIPLUS_CACHE_FIRST_ORDER* draw_gdiplus_cache_first)
{
	stream_read_uint8(s, draw_gdiplus_cache_first->flags); /* flags (1 byte) */
	stream_read_uint16(s, draw_gdiplus_cache_first->cacheType); /* cacheType (2 bytes) */
	stream_read_uint16(s, draw_gdiplus_cache_first->cacheIndex); /* cacheIndex (2 bytes) */
	stream_read_uint16(s, draw_gdiplus_cache_first->cbSize); /* cbSize (2 bytes) */
	stream_read_uint32(s, draw_gdiplus_cache_first->cbTotalSize); /* cbTotalSize (4 bytes) */
	stream_seek(s, draw_gdiplus_cache_first->cbSize); /* emfRecords */
}

void update_read_draw_gdiplus_cache_next_order(STREAM* s, DRAW_GDIPLUS_CACHE_NEXT_ORDER* draw_gdiplus_cache_next)
{
	stream_read_uint8(s, draw_gdiplus_cache_next->flags); /* flags (1 byte) */
	stream_read_uint16(s, draw_gdiplus_cache_next->cacheType); /* cacheType (2 bytes) */
	stream_read_uint16(s, draw_gdiplus_cache_next->cacheIndex); /* cacheIndex (2 bytes) */
	stream_read_uint16(s, draw_gdiplus_cache_next->cbSize); /* cbSize (2 bytes) */
	stream_seek(s, draw_gdiplus_cache_next->cbSize); /* emfRecords */
}

void update_read_draw_gdiplus_cache_end_order(STREAM* s, DRAW_GDIPLUS_CACHE_END_ORDER* draw_gdiplus_cache_end)
{
	stream_read_uint8(s, draw_gdiplus_cache_end->flags); /* flags (1 byte) */
	stream_read_uint16(s, draw_gdiplus_cache_end->cacheType); /* cacheType (2 bytes) */
	stream_read_uint16(s, draw_gdiplus_cache_end->cacheIndex); /* cacheIndex (2 bytes) */
	stream_read_uint16(s, draw_gdiplus_cache_end->cbSize); /* cbSize (2 bytes) */
	stream_read_uint32(s, draw_gdiplus_cache_end->cbTotalSize); /* cbTotalSize (4 bytes) */
	stream_seek(s, draw_gdiplus_cache_end->cbSize); /* emfRecords */
}

void update_read_field_flags(STREAM* s, uint32* fieldFlags, uint8 flags, uint8 fieldBytes)
{
	int i;
	uint8 byte;

	if (flags & ORDER_ZERO_FIELD_BYTE_BIT0)
		fieldBytes--;

	if (flags & ORDER_ZERO_FIELD_BYTE_BIT1)
	{
		if (fieldBytes > 1)
			fieldBytes -= 2;
		else
			fieldBytes = 0;
	}

	*fieldFlags = 0;
	for (i = 0; i < fieldBytes; i++)
	{
		stream_read_uint8(s, byte);
		*fieldFlags |= byte << (i * 8);
	}
}

void update_read_bounds(STREAM* s, rdpBounds* bounds)
{
	uint8 flags;

	stream_read_uint8(s, flags); /* field flags */

	if (flags & BOUND_LEFT)
		update_read_coord(s, &bounds->left, false);
	else if (flags & BOUND_DELTA_LEFT)
		update_read_coord(s, &bounds->left, true);

	if (flags & BOUND_TOP)
		update_read_coord(s, &bounds->top, false);
	else if (flags & BOUND_DELTA_TOP)
		update_read_coord(s, &bounds->top, true);

	if (flags & BOUND_RIGHT)
		update_read_coord(s, &bounds->right, false);
	else if (flags & BOUND_DELTA_RIGHT)
		update_read_coord(s, &bounds->right, true);

	if (flags & BOUND_BOTTOM)
		update_read_coord(s, &bounds->bottom, false);
	else if (flags & BOUND_DELTA_BOTTOM)
		update_read_coord(s, &bounds->bottom, true);
}

void update_recv_primary_order(rdpUpdate* update, STREAM* s, uint8 flags)
{
	ORDER_INFO* orderInfo;
	rdpContext* context = update->context;
	rdpPrimaryUpdate* primary = update->primary;

	orderInfo = &(primary->order_info);

	if (flags & ORDER_TYPE_CHANGE)
		stream_read_uint8(s, orderInfo->orderType); /* orderType (1 byte) */

	update_read_field_flags(s, &(orderInfo->fieldFlags), flags,
			PRIMARY_DRAWING_ORDER_FIELD_BYTES[orderInfo->orderType]);

	if (flags & ORDER_BOUNDS)
	{
		if (!(flags & ORDER_ZERO_BOUNDS_DELTAS))
			update_read_bounds(s, &orderInfo->bounds);

		IFCALL(update->SetBounds, context, &orderInfo->bounds);
	}

	orderInfo->deltaCoordinates = (flags & ORDER_DELTA_COORDINATES) ? true : false;

#ifdef WITH_DEBUG_ORDERS
	if (orderInfo->orderType < PRIMARY_DRAWING_ORDER_COUNT)
		printf("%s Primary Drawing Order (0x%02X)\n", PRIMARY_DRAWING_ORDER_STRINGS[orderInfo->orderType], orderInfo->orderType);
	else
		printf("Unknown Primary Drawing Order (0x%02X)\n", orderInfo->orderType);
#endif

	switch (orderInfo->orderType)
	{
		case ORDER_TYPE_DSTBLT:
			update_read_dstblt_order(s, orderInfo, &(primary->dstblt));
			IFCALL(primary->DstBlt, context, &primary->dstblt);
			break;

		case ORDER_TYPE_PATBLT:
			update_read_patblt_order(s, orderInfo, &(primary->patblt));
			IFCALL(primary->PatBlt, context, &primary->patblt);
			break;

		case ORDER_TYPE_SCRBLT:
			update_read_scrblt_order(s, orderInfo, &(primary->scrblt));
			IFCALL(primary->ScrBlt, context, &primary->scrblt);
			break;

		case ORDER_TYPE_OPAQUE_RECT:
			update_read_opaque_rect_order(s, orderInfo, &(primary->opaque_rect));
			IFCALL(primary->OpaqueRect, context, &primary->opaque_rect);
			break;

		case ORDER_TYPE_DRAW_NINE_GRID:
			update_read_draw_nine_grid_order(s, orderInfo, &(primary->draw_nine_grid));
			IFCALL(primary->DrawNineGrid, context, &primary->draw_nine_grid);
			break;

		case ORDER_TYPE_MULTI_DSTBLT:
			update_read_multi_dstblt_order(s, orderInfo, &(primary->multi_dstblt));
			IFCALL(primary->MultiDstBlt, context, &primary->multi_dstblt);
			break;

		case ORDER_TYPE_MULTI_PATBLT:
			update_read_multi_patblt_order(s, orderInfo, &(primary->multi_patblt));
			IFCALL(primary->MultiPatBlt, context, &primary->multi_patblt);
			break;

		case ORDER_TYPE_MULTI_SCRBLT:
			update_read_multi_scrblt_order(s, orderInfo, &(primary->multi_scrblt));
			IFCALL(primary->MultiScrBlt, context, &primary->multi_scrblt);
			break;

		case ORDER_TYPE_MULTI_OPAQUE_RECT:
			update_read_multi_opaque_rect_order(s, orderInfo, &(primary->multi_opaque_rect));
			IFCALL(primary->MultiOpaqueRect, context, &primary->multi_opaque_rect);
			break;

		case ORDER_TYPE_MULTI_DRAW_NINE_GRID:
			update_read_multi_draw_nine_grid_order(s, orderInfo, &(primary->multi_draw_nine_grid));
			IFCALL(primary->MultiDrawNineGrid, context, &primary->multi_draw_nine_grid);
			break;

		case ORDER_TYPE_LINE_TO:
			update_read_line_to_order(s, orderInfo, &(primary->line_to));
			IFCALL(primary->LineTo, context, &primary->line_to);
			break;

		case ORDER_TYPE_POLYLINE:
			update_read_polyline_order(s, orderInfo, &(primary->polyline));
			IFCALL(primary->Polyline, context, &primary->polyline);
			break;

		case ORDER_TYPE_MEMBLT:
			update_read_memblt_order(s, orderInfo, &(primary->memblt));
			IFCALL(primary->MemBlt, context, &primary->memblt);
			break;

		case ORDER_TYPE_MEM3BLT:
			update_read_mem3blt_order(s, orderInfo, &(primary->mem3blt));
			IFCALL(primary->Mem3Blt, context, &primary->mem3blt);
			break;

		case ORDER_TYPE_SAVE_BITMAP:
			update_read_save_bitmap_order(s, orderInfo, &(primary->save_bitmap));
			IFCALL(primary->SaveBitmap, context, &primary->save_bitmap);
			break;

		case ORDER_TYPE_GLYPH_INDEX:
			update_read_glyph_index_order(s, orderInfo, &(primary->glyph_index));
			IFCALL(primary->GlyphIndex, context, &primary->glyph_index);
			break;

		case ORDER_TYPE_FAST_INDEX:
			update_read_fast_index_order(s, orderInfo, &(primary->fast_index));
			IFCALL(primary->FastIndex, context, &primary->fast_index);
			break;

		case ORDER_TYPE_FAST_GLYPH:
			update_read_fast_glyph_order(s, orderInfo, &(primary->fast_glyph));
			IFCALL(primary->FastGlyph, context, &primary->fast_glyph);
			break;

		case ORDER_TYPE_POLYGON_SC:
			update_read_polygon_sc_order(s, orderInfo, &(primary->polygon_sc));
			IFCALL(primary->PolygonSC, context, &primary->polygon_sc);
			break;

		case ORDER_TYPE_POLYGON_CB:
			update_read_polygon_cb_order(s, orderInfo, &(primary->polygon_cb));
			IFCALL(primary->PolygonCB, context, &primary->polygon_cb);
			break;

		case ORDER_TYPE_ELLIPSE_SC:
			update_read_ellipse_sc_order(s, orderInfo, &(primary->ellipse_sc));
			IFCALL(primary->EllipseSC, context, &primary->ellipse_sc);
			break;

		case ORDER_TYPE_ELLIPSE_CB:
			update_read_ellipse_cb_order(s, orderInfo, &(primary->ellipse_cb));
			IFCALL(primary->EllipseCB, context, &primary->ellipse_cb);
			break;

		default:
			break;
	}

	if (flags & ORDER_BOUNDS)
	{
		IFCALL(update->SetBounds, context, NULL);
	}
}

void update_recv_secondary_order(rdpUpdate* update, STREAM* s, uint8 flags)
{
	uint8* next;
	uint8 orderType;
	uint16 extraFlags;
	uint16 orderLength;
	rdpContext* context = update->context;
	rdpSecondaryUpdate* secondary = update->secondary;

	stream_read_uint16(s, orderLength); /* orderLength (2 bytes) */
	stream_read_uint16(s, extraFlags); /* extraFlags (2 bytes) */
	stream_read_uint8(s, orderType); /* orderType (1 byte) */

	next = s->p + ((sint16) orderLength) + 7;

#ifdef WITH_DEBUG_ORDERS
	if (orderType < SECONDARY_DRAWING_ORDER_COUNT)
		printf("%s Secondary Drawing Order (0x%02X)\n", SECONDARY_DRAWING_ORDER_STRINGS[orderType], orderType);
	else
		printf("Unknown Secondary Drawing Order (0x%02X)\n", orderType);
#endif

	switch (orderType)
	{
		case ORDER_TYPE_BITMAP_UNCOMPRESSED:
			update_read_cache_bitmap_order(s, &(secondary->cache_bitmap_order), false, extraFlags);
			IFCALL(secondary->CacheBitmap, context, &(secondary->cache_bitmap_order));
			break;

		case ORDER_TYPE_CACHE_BITMAP_COMPRESSED:
			update_read_cache_bitmap_order(s, &(secondary->cache_bitmap_order), true, extraFlags);
			IFCALL(secondary->CacheBitmap, context, &(secondary->cache_bitmap_order));
			break;

		case ORDER_TYPE_BITMAP_UNCOMPRESSED_V2:
			update_read_cache_bitmap_v2_order(s, &(secondary->cache_bitmap_v2_order), false, extraFlags);
			IFCALL(secondary->CacheBitmapV2, context, &(secondary->cache_bitmap_v2_order));
			break;

		case ORDER_TYPE_BITMAP_COMPRESSED_V2:
			update_read_cache_bitmap_v2_order(s, &(secondary->cache_bitmap_v2_order), true, extraFlags);
			IFCALL(secondary->CacheBitmapV2, context, &(secondary->cache_bitmap_v2_order));
			break;

		case ORDER_TYPE_BITMAP_COMPRESSED_V3:
			update_read_cache_bitmap_v3_order(s, &(secondary->cache_bitmap_v3_order), true, extraFlags);
			IFCALL(secondary->CacheBitmapV3, context, &(secondary->cache_bitmap_v3_order));
			break;

		case ORDER_TYPE_CACHE_COLOR_TABLE:
			update_read_cache_color_table_order(s, &(secondary->cache_color_table_order), extraFlags);
			IFCALL(secondary->CacheColorTable, context, &(secondary->cache_color_table_order));
			break;

		case ORDER_TYPE_CACHE_GLYPH:
			if (secondary->glyph_v2)
			{
				update_read_cache_glyph_v2_order(s, &(secondary->cache_glyph_v2_order), extraFlags);
				IFCALL(secondary->CacheGlyphV2, context, &(secondary->cache_glyph_v2_order));
			}
			else
			{
				update_read_cache_glyph_order(s, &(secondary->cache_glyph_order), extraFlags);
				IFCALL(secondary->CacheGlyph, context, &(secondary->cache_glyph_order));
			}
			break;

		case ORDER_TYPE_CACHE_BRUSH:
			update_read_cache_brush_order(s, &(secondary->cache_brush_order), extraFlags);
			IFCALL(secondary->CacheBrush, context, &(secondary->cache_brush_order));
			break;

		default:
			break;
	}

	s->p = next;
}

void update_recv_altsec_order(rdpUpdate* update, STREAM* s, uint8 flags)
{
	uint8 orderType;
	rdpContext* context = update->context;
	rdpAltSecUpdate* altsec = update->altsec;

	orderType = flags >>= 2; /* orderType is in higher 6 bits of flags field */

#ifdef WITH_DEBUG_ORDERS
	if (orderType < ALTSEC_DRAWING_ORDER_COUNT)
		printf("%s Alternate Secondary Drawing Order (0x%02X)\n", ALTSEC_DRAWING_ORDER_STRINGS[orderType], orderType);
	else
		printf("Unknown Alternate Secondary Drawing Order: 0x%02X\n", orderType);
#endif

	switch (orderType)
	{
		case ORDER_TYPE_CREATE_OFFSCREEN_BITMAP:
			update_read_create_offscreen_bitmap_order(s, &(altsec->create_offscreen_bitmap));
			IFCALL(altsec->CreateOffscreenBitmap, context, &(altsec->create_offscreen_bitmap));
			break;

		case ORDER_TYPE_SWITCH_SURFACE:
			update_read_switch_surface_order(s, &(altsec->switch_surface));
			IFCALL(altsec->SwitchSurface, context, &(altsec->switch_surface));
			break;

		case ORDER_TYPE_CREATE_NINE_GRID_BITMAP:
			update_read_create_nine_grid_bitmap_order(s, &(altsec->create_nine_grid_bitmap));
			IFCALL(altsec->CreateNineGridBitmap, context, &(altsec->create_nine_grid_bitmap));
			break;

		case ORDER_TYPE_FRAME_MARKER:
			update_read_frame_marker_order(s, &(altsec->frame_marker));
			IFCALL(altsec->FrameMarker, context, &(altsec->frame_marker));
			break;

		case ORDER_TYPE_STREAM_BITMAP_FIRST:
			update_read_stream_bitmap_first_order(s, &(altsec->stream_bitmap_first));
			IFCALL(altsec->StreamBitmapFirst, context, &(altsec->stream_bitmap_first));
			break;

		case ORDER_TYPE_STREAM_BITMAP_NEXT:
			update_read_stream_bitmap_next_order(s, &(altsec->stream_bitmap_next));
			IFCALL(altsec->StreamBitmapNext, context, &(altsec->stream_bitmap_next));
			break;

		case ORDER_TYPE_GDIPLUS_FIRST:
			update_read_draw_gdiplus_first_order(s, &(altsec->draw_gdiplus_first));
			IFCALL(altsec->DrawGdiPlusFirst, context, &(altsec->draw_gdiplus_first));
			break;

		case ORDER_TYPE_GDIPLUS_NEXT:
			update_read_draw_gdiplus_next_order(s, &(altsec->draw_gdiplus_next));
			IFCALL(altsec->DrawGdiPlusNext, context, &(altsec->draw_gdiplus_next));
			break;

		case ORDER_TYPE_GDIPLUS_END:
			update_read_draw_gdiplus_end_order(s, &(altsec->draw_gdiplus_end));
			IFCALL(altsec->DrawGdiPlusEnd, context, &(altsec->draw_gdiplus_end));
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_FIRST:
			update_read_draw_gdiplus_cache_first_order(s, &(altsec->draw_gdiplus_cache_first));
			IFCALL(altsec->DrawGdiPlusCacheFirst, context, &(altsec->draw_gdiplus_cache_first));
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_NEXT:
			update_read_draw_gdiplus_cache_next_order(s, &(altsec->draw_gdiplus_cache_next));
			IFCALL(altsec->DrawGdiPlusCacheNext, context, &(altsec->draw_gdiplus_cache_next));
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_END:
			update_read_draw_gdiplus_cache_end_order(s, &(altsec->draw_gdiplus_cache_end));
			IFCALL(altsec->DrawGdiPlusCacheEnd, context, &(altsec->draw_gdiplus_cache_end));
			break;

		case ORDER_TYPE_WINDOW:
			update_recv_altsec_window_order(update, s);
			break;

		case ORDER_TYPE_COMPDESK_FIRST:
			break;

		default:
			break;
	}
}

void update_recv_order(rdpUpdate* update, STREAM* s)
{
	uint8 controlFlags;

	stream_read_uint8(s, controlFlags); /* controlFlags (1 byte) */

	if (!(controlFlags & ORDER_STANDARD))
		update_recv_altsec_order(update, s, controlFlags);
	else if (controlFlags & ORDER_SECONDARY)
		update_recv_secondary_order(update, s, controlFlags);
	else
		update_recv_primary_order(update, s, controlFlags);
}

