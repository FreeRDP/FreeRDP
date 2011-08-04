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

#define PRIMARY_DRAWING_ORDER_COUNT	(sizeof(PRIMARY_DRAWING_ORDER_STRINGS) / sizeof(PRIMARY_DRAWING_ORDER_STRINGS[0]))

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

#define SECONDARY_DRAWING_ORDER_COUNT	(sizeof(SECONDARY_DRAWING_ORDER_STRINGS) / sizeof(SECONDARY_DRAWING_ORDER_STRINGS[0]))

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

#define ALTSEC_DRAWING_ORDER_COUNT	(sizeof(ALTSEC_DRAWING_ORDER_STRINGS) / sizeof(ALTSEC_DRAWING_ORDER_STRINGS[0]))

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

uint8 CBR2_BPP[] =
{
		0, 0, 0, 8, 16, 24, 32
};

uint8 CBR23_BPP[] =
{
		0, 0, 0, 8, 16, 24, 32
};

uint8 BMF_BPP[] =
{
		0, 1, 0, 8, 16, 24, 32
};

void update_read_coord(STREAM* s, sint16* coord, boolean delta)
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

void update_read_color(STREAM* s, uint32* color)
{
	uint8 byte;

	stream_read_uint8(s, byte);
	*color = byte;
	stream_read_uint8(s, byte);
	*color |= (byte << 8);
	stream_read_uint8(s, byte);
	*color |= (byte << 16);
}

void update_read_colorref(STREAM* s, uint32* color)
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

void update_read_color_quad(STREAM* s, uint32* color)
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

void update_read_2byte_unsigned(STREAM* s, uint16* value)
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

void update_read_2byte_signed(STREAM* s, sint16* value)
{
	uint8 byte;
	boolean negative;

	stream_read_uint8(s, byte);

	negative = (byte & 0x40) ? True : False;

	*value = (byte & 0x3F);

	if (byte & 0x80)
	{
		stream_read_uint8(s, byte);
		*value = (*value << 8) | byte;
	}

	if (negative)
		*value *= -1;
}

void update_read_4byte_unsigned(STREAM* s, uint32* value)
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

void update_read_delta(STREAM* s, sint16* value)
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

void update_read_delta_rects(STREAM* s, DELTA_RECT* rectangles, int number)
{
	int i;
	uint8 flags;
	uint8* zeroBits;
	int zeroBitsSize;

	if (number > 45)
		number = 45;

	zeroBitsSize = ((number + 1) / 2);

	stream_get_mark(s, zeroBits);
	stream_seek(s, zeroBitsSize);

	memset(rectangles, 0, sizeof(DELTA_RECT) * number);

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

void update_read_delta_points(STREAM* s, DELTA_POINT* points, int number, sint16 x, sint16 y)
{
	int i;
	uint8 flags;
	uint8* zeroBits;
	int zeroBitsSize;

	zeroBitsSize = ((number + 3) / 4);

	stream_get_mark(s, zeroBits);
	stream_seek(s, zeroBitsSize);

	memset(points, 0, sizeof(DELTA_POINT) * number);

	for (i = 1; i < number + 1; i++)
	{
		if ((i - 1) % 4 == 0)
			flags = zeroBits[(i - 1) / 4];

		if (~flags & 0x80)
			update_read_delta(s, &points[i].x);

		if (~flags & 0x40)
			update_read_delta(s, &points[i].y);

		points[i].x = points[i].x + points[i - 1].x;
		points[i].y = points[i].y + points[i - 1].y;

		points[i - 1].x += x;
		points[i - 1].y += y;

		flags <<= 2;
	}

	points[i - 1].x += x;
	points[i - 1].y += y;
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

	opaque_rect->color = 0;

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
	{
		stream_read_uint8(s, byte);
		opaque_rect->color = byte;
	}

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
	{
		stream_read_uint8(s, byte);
		opaque_rect->color |= (byte << 8);
	}

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
	{
		stream_read_uint8(s, byte);
		opaque_rect->color |= (byte << 16);
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
		stream_read_uint8(s, multi_dstblt->nDeltaEntries);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
	{
		stream_read_uint16(s, multi_dstblt->cbData);
		stream_seek(s, multi_dstblt->cbData);
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

	if (orderInfo->fieldFlags & ORDER_FIELD_14)
	{
		stream_read_uint16(s, multi_patblt->cbData);
		stream_seek(s, multi_patblt->cbData);
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
		stream_read_uint8(s, multi_scrblt->nDeltaEntries);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
	{
		stream_read_uint16(s, multi_scrblt->cbData);
		stream_seek(s, multi_scrblt->cbData);
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
			polyline->points = (DELTA_POINT*) xmalloc(polyline->cbData);
		else
			polyline->points = (DELTA_POINT*) xrealloc(polyline->points, polyline->cbData);

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

	if (orderInfo->fieldFlags & ORDER_FIELD_15)
		stream_read_uint8(s, glyph_index->brushOrgX);

	if (orderInfo->fieldFlags & ORDER_FIELD_16)
		stream_read_uint8(s, glyph_index->brushOrgY);

	if (orderInfo->fieldFlags & ORDER_FIELD_17)
		stream_read_uint8(s, glyph_index->brushStyle);

	if (orderInfo->fieldFlags & ORDER_FIELD_18)
		stream_read_uint8(s, glyph_index->brushHatch);

	if (orderInfo->fieldFlags & ORDER_FIELD_19)
		stream_read(s, glyph_index->brushExtra, 7);

	if (orderInfo->fieldFlags & ORDER_FIELD_20)
		stream_read_uint16(s, glyph_index->x);

	if (orderInfo->fieldFlags & ORDER_FIELD_21)
		stream_read_uint16(s, glyph_index->y);

	if (orderInfo->fieldFlags & ORDER_FIELD_22)
	{
		stream_read_uint8(s, glyph_index->cbData);
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
		stream_seek(s, fast_index->cbData);
	}
}

void update_read_fast_glyph_order(STREAM* s, ORDER_INFO* orderInfo, FAST_GLYPH_ORDER* fast_glyph)
{
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
		stream_seek(s, fast_glyph->cbData);
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

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
		stream_read_uint8(s, polygon_cb->brushOrgX);

	if (orderInfo->fieldFlags & ORDER_FIELD_08)
		stream_read_uint8(s, polygon_cb->brushOrgY);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
		stream_read_uint8(s, polygon_cb->brushStyle);

	if (orderInfo->fieldFlags & ORDER_FIELD_10)
		stream_read_uint8(s, polygon_cb->brushHatch);

	if (orderInfo->fieldFlags & ORDER_FIELD_11)
		stream_read(s, polygon_cb->brushExtra, 7);

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
		if (flags & NO_BITMAP_COMPRESSION_HDR)
		{
			stream_seek(s, cache_bitmap_order->bitmapLength); /* bitmapDataStream */
		}
		else
		{
			uint8* bitmapComprHdr = (uint8*) &(cache_bitmap_order->bitmapComprHdr);
			stream_read(s, bitmapComprHdr, 8); /* bitmapComprHdr (8 bytes) */
			stream_seek(s, cache_bitmap_order->bitmapLength); /* bitmapDataStream */
		}
	}
	else
	{
		stream_seek(s, cache_bitmap_order->bitmapLength); /* bitmapDataStream */
	}
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

	if (compressed)
	{
		if (cache_bitmap_v2_order->flags & CBR2_NO_BITMAP_COMPRESSION_HDR)
		{
			stream_seek(s, cache_bitmap_v2_order->bitmapLength); /* bitmapDataStream */
		}
		else
		{
			uint8* bitmapComprHdr = (uint8*) &(cache_bitmap_v2_order->bitmapComprHdr);
			stream_read(s, bitmapComprHdr, 8); /* bitmapComprHdr (8 bytes) */
			stream_seek(s, cache_bitmap_v2_order->bitmapLength); /* bitmapDataStream */
		}
	}
	else
	{
		stream_seek(s, cache_bitmap_v2_order->bitmapLength); /* bitmapDataStream */
	}
}

void update_read_cache_bitmap_v3_order(STREAM* s, CACHE_BITMAP_V3_ORDER* cache_bitmap_v3_order, boolean compressed, uint16 flags)
{
	uint32 bitmapLength;
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

	for (i = 0; i < cache_color_table_order->numberColors; i++)
	{
		update_read_color_quad(s, &colorTable[i]);
	}

	cache_color_table_order->colorTable = colorTable;
}

void update_read_cache_glyph_order(STREAM* s, CACHE_GLYPH_ORDER* cache_glyph_order, uint16 flags)
{
	int i;
	int size;
	GLYPH_DATA* glyph;

	stream_read_uint8(s, cache_glyph_order->cacheId); /* cacheId (1 byte) */
	stream_read_uint8(s, cache_glyph_order->cGlyphs); /* cGlyphs (1 byte) */

	size = cache_glyph_order->cGlyphs * sizeof(GLYPH_DATA);

	if (cache_glyph_order->glyphData == NULL)
		cache_glyph_order->glyphData = (GLYPH_DATA*) xmalloc(size);
	else
		cache_glyph_order->glyphData = (GLYPH_DATA*) xrealloc(cache_glyph_order->glyphData, size);

	for (i = 0; i < cache_glyph_order->cGlyphs; i++)
	{
		glyph = &cache_glyph_order->glyphData[i];

		stream_read_uint16(s, glyph->cacheIndex);
		stream_read_uint16(s, glyph->x);
		stream_read_uint16(s, glyph->y);
		stream_read_uint16(s, glyph->cx);
		stream_read_uint16(s, glyph->cy);

		glyph->cb = ((glyph->cx + 7) / 8) * glyph->cy;
		glyph->cb += glyph->cb % 4;

		if (glyph->aj == NULL)
			glyph->aj = (uint8*) xmalloc(glyph->cb);
		else
			glyph->aj = (uint8*) xrealloc(glyph->aj, glyph->cb);

		stream_read(s, glyph->aj, glyph->cb);
	}
}

void update_read_cache_glyph_v2_order(STREAM* s, CACHE_GLYPH_V2_ORDER* cache_glyph_v2_order, uint16 flags)
{
	int i;
	int size;
	GLYPH_DATA_V2* glyph;

	cache_glyph_v2_order->cacheId = (flags & 0x000F);
	cache_glyph_v2_order->flags = (flags & 0x00F0) >> 4;
	cache_glyph_v2_order->cGlyphs = (flags & 0xFF00) >> 8;

	size = cache_glyph_v2_order->cGlyphs * sizeof(GLYPH_DATA_V2);

	if (cache_glyph_v2_order->glyphData == NULL)
		cache_glyph_v2_order->glyphData = (GLYPH_DATA_V2*) xmalloc(size);
	else
		cache_glyph_v2_order->glyphData = (GLYPH_DATA_V2*) xrealloc(cache_glyph_v2_order->glyphData, size);

	for (i = 0; i < cache_glyph_v2_order->cGlyphs; i++)
	{
		glyph = &cache_glyph_v2_order->glyphData[i];

		stream_read_uint16(s, glyph->cacheIndex);
		update_read_2byte_signed(s, &glyph->x);
		update_read_2byte_signed(s, &glyph->y);
		update_read_2byte_unsigned(s, &glyph->cx);
		update_read_2byte_unsigned(s, &glyph->cy);

		glyph->cb = ((glyph->cx + 7) / 8) * glyph->cy;
		glyph->cb += glyph->cb % 4;

		if (glyph->aj == NULL)
			glyph->aj = (uint8*) xmalloc(glyph->cb);
		else
			glyph->aj = (uint8*) xrealloc(glyph->aj, glyph->cb);

		stream_read(s, glyph->aj, glyph->cb);
	}
}

void update_read_cache_brush_order(STREAM* s, CACHE_BRUSH_ORDER* cache_brush_order, uint16 flags)
{
	uint8 iBitmapFormat;

	stream_read_uint8(s, cache_brush_order->cacheEntry); /* cacheEntry (1 byte) */

	stream_read_uint8(s, iBitmapFormat); /* iBitmapFormat (1 byte) */
	cache_brush_order->bpp = BMF_BPP[iBitmapFormat];

	stream_read_uint8(s, cache_brush_order->cx); /* cx (1 byte) */
	stream_read_uint8(s, cache_brush_order->cy); /* cy (1 byte) */
	stream_read_uint8(s, cache_brush_order->style); /* style (1 byte) */
	stream_read_uint8(s, cache_brush_order->length); /* iBytes (1 byte) */

	if (cache_brush_order->brushData == NULL)
		cache_brush_order->brushData = (uint8*) xmalloc(cache_brush_order->length);
	else
		cache_brush_order->brushData = (uint8*) xrealloc(cache_brush_order->brushData, cache_brush_order->length);

	stream_read(s, cache_brush_order->brushData, cache_brush_order->length);
}

/* Alternate Secondary Drawing Orders */

void update_read_create_offscreen_bitmap_order(STREAM* s, CREATE_OFFSCREEN_BITMAP_ORDER* create_offscreen_bitmap)
{
	uint16 flags;
	boolean deleteListPresent;

	stream_read_uint16(s, flags); /* flags (2 bytes) */
	create_offscreen_bitmap->id = flags & 0x7FFF;
	deleteListPresent = (flags & 0x8000) ? True : False;

	stream_read_uint16(s, create_offscreen_bitmap->cx); /* cx (2 bytes) */
	stream_read_uint16(s, create_offscreen_bitmap->cy); /* cy (2 bytes) */

	if (deleteListPresent)
	{
		int i;
		OFFSCREEN_DELETE_LIST* deleteList;

		deleteList = &(create_offscreen_bitmap->deleteList);

		stream_read_uint16(s, deleteList->cIndices);

		if (deleteList->indices == NULL)
			deleteList->indices = xmalloc(deleteList->cIndices * 2);
		else
			deleteList->indices = xrealloc(deleteList->indices, deleteList->cIndices * 2);

		for (i = 0; i < deleteList->cIndices; i++)
		{
			stream_read_uint16(s, deleteList->indices[i]);
		}
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

void update_read_bounds(STREAM* s, ORDER_INFO* orderInfo)
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

void update_recv_primary_order(rdpUpdate* update, STREAM* s, uint8 flags)
{
	BOUNDS bounds;
	ORDER_INFO* orderInfo = &(update->order_info);

	if (flags & ORDER_TYPE_CHANGE)
		stream_read_uint8(s, orderInfo->orderType); /* orderType (1 byte) */

	update_read_field_flags(s, &(orderInfo->fieldFlags), flags,
			PRIMARY_DRAWING_ORDER_FIELD_BYTES[orderInfo->orderType]);

	if (flags & ORDER_BOUNDS)
	{
		if (!(flags & ORDER_ZERO_BOUNDS_DELTAS))
			update_read_bounds(s, orderInfo);

		bounds.left = orderInfo->boundLeft;
		bounds.top = orderInfo->boundTop;
		bounds.right = orderInfo->boundRight;
		bounds.bottom = orderInfo->boundBottom;

		IFCALL(update->SetBounds, update, &bounds);
	}

	orderInfo->deltaCoordinates = (flags & ORDER_DELTA_COORDINATES) ? True : False;

	if (orderInfo->orderType < PRIMARY_DRAWING_ORDER_COUNT)
		printf("%s Primary Drawing Order (0x%02X)\n", PRIMARY_DRAWING_ORDER_STRINGS[orderInfo->orderType], orderInfo->orderType);
	else
		printf("Unknown Primary Drawing Order (0x%02X)\n", orderInfo->orderType);

	switch (orderInfo->orderType)
	{
		case ORDER_TYPE_DSTBLT:
			update_read_dstblt_order(s, orderInfo, &(update->dstblt));
			IFCALL(update->DstBlt, update, &update->dstblt);
			break;

		case ORDER_TYPE_PATBLT:
			update_read_patblt_order(s, orderInfo, &(update->patblt));
			IFCALL(update->PatBlt, update, &update->patblt);
			break;

		case ORDER_TYPE_SCRBLT:
			update_read_scrblt_order(s, orderInfo, &(update->scrblt));
			IFCALL(update->ScrBlt, update, &update->scrblt);
			break;

		case ORDER_TYPE_OPAQUE_RECT:
			update_read_opaque_rect_order(s, orderInfo, &(update->opaque_rect));
			IFCALL(update->OpaqueRect, update, &update->opaque_rect);
			break;

		case ORDER_TYPE_DRAW_NINE_GRID:
			update_read_draw_nine_grid_order(s, orderInfo, &(update->draw_nine_grid));
			IFCALL(update->DrawNineGrid, update, &update->draw_nine_grid);
			break;

		case ORDER_TYPE_MULTI_DSTBLT:
			update_read_multi_dstblt_order(s, orderInfo, &(update->multi_dstblt));
			IFCALL(update->MultiDstBlt, update, &update->multi_dstblt);
			break;

		case ORDER_TYPE_MULTI_PATBLT:
			update_read_multi_patblt_order(s, orderInfo, &(update->multi_patblt));
			IFCALL(update->MultiPatBlt, update, &update->multi_patblt);
			break;

		case ORDER_TYPE_MULTI_SCRBLT:
			update_read_multi_scrblt_order(s, orderInfo, &(update->multi_scrblt));
			IFCALL(update->MultiScrBlt, update, &update->multi_scrblt);
			break;

		case ORDER_TYPE_MULTI_OPAQUE_RECT:
			update_read_multi_opaque_rect_order(s, orderInfo, &(update->multi_opaque_rect));
			IFCALL(update->MultiOpaqueRect, update, &update->multi_opaque_rect);
			break;

		case ORDER_TYPE_MULTI_DRAW_NINE_GRID:
			update_read_multi_draw_nine_grid_order(s, orderInfo, &(update->multi_draw_nine_grid));
			IFCALL(update->MultiDrawNineGrid, update, &update->multi_draw_nine_grid);
			break;

		case ORDER_TYPE_LINE_TO:
			update_read_line_to_order(s, orderInfo, &(update->line_to));
			IFCALL(update->LineTo, update, &update->line_to);
			break;

		case ORDER_TYPE_POLYLINE:
			update_read_polyline_order(s, orderInfo, &(update->polyline));
			IFCALL(update->Polyline, update, &update->polyline);
			break;

		case ORDER_TYPE_MEMBLT:
			update_read_memblt_order(s, orderInfo, &(update->memblt));
			IFCALL(update->MemBlt, update, &update->memblt);
			break;

		case ORDER_TYPE_MEM3BLT:
			update_read_mem3blt_order(s, orderInfo, &(update->mem3blt));
			IFCALL(update->Mem3Blt, update, &update->mem3blt);
			break;

		case ORDER_TYPE_SAVE_BITMAP:
			update_read_save_bitmap_order(s, orderInfo, &(update->save_bitmap));
			IFCALL(update->SaveBitmap, update, &update->save_bitmap);
			break;

		case ORDER_TYPE_GLYPH_INDEX:
			update_read_glyph_index_order(s, orderInfo, &(update->glyph_index));
			IFCALL(update->GlyphIndex, update, &update->glyph_index);
			break;

		case ORDER_TYPE_FAST_INDEX:
			update_read_fast_index_order(s, orderInfo, &(update->fast_index));
			IFCALL(update->FastIndex, update, &update->fast_index);
			break;

		case ORDER_TYPE_FAST_GLYPH:
			update_read_fast_glyph_order(s, orderInfo, &(update->fast_glyph));
			IFCALL(update->FastGlyph, update, &update->fast_glyph);
			break;

		case ORDER_TYPE_POLYGON_SC:
			update_read_polygon_sc_order(s, orderInfo, &(update->polygon_sc));
			IFCALL(update->PolygonSC, update, &update->polygon_sc);
			break;

		case ORDER_TYPE_POLYGON_CB:
			update_read_polygon_cb_order(s, orderInfo, &(update->polygon_cb));
			IFCALL(update->PolygonCB, update, &update->polygon_cb);
			break;

		case ORDER_TYPE_ELLIPSE_SC:
			update_read_ellipse_sc_order(s, orderInfo, &(update->ellipse_sc));
			IFCALL(update->EllipseSC, update, &update->ellipse_sc);
			break;

		case ORDER_TYPE_ELLIPSE_CB:
			update_read_ellipse_cb_order(s, orderInfo, &(update->ellipse_cb));
			IFCALL(update->EllipseCB, update, &update->ellipse_cb);
			break;

		default:
			break;
	}

	if (flags & ORDER_BOUNDS)
		IFCALL(update->SetBounds, update, NULL);
}

void update_recv_secondary_order(rdpUpdate* update, STREAM* s, uint8 flags)
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

	if (orderType < SECONDARY_DRAWING_ORDER_COUNT)
		printf("%s Secondary Drawing Order (0x%02X)\n", SECONDARY_DRAWING_ORDER_STRINGS[orderType], orderType);
	else
		printf("Unknown Secondary Drawing Order (0x%02X)\n", orderType);

	switch (orderType)
	{
		case ORDER_TYPE_BITMAP_UNCOMPRESSED:
			update_read_cache_bitmap_order(s, &(update->cache_bitmap_order), False, extraFlags);
			IFCALL(update->CacheBitmap, update, &(update->cache_bitmap_order));
			break;

		case ORDER_TYPE_CACHE_BITMAP_COMPRESSED:
			update_read_cache_bitmap_order(s, &(update->cache_bitmap_order), True, extraFlags);
			IFCALL(update->CacheBitmap, update, &(update->cache_bitmap_order));
			break;

		case ORDER_TYPE_BITMAP_UNCOMPRESSED_V2:
			update_read_cache_bitmap_v2_order(s, &(update->cache_bitmap_v2_order), False, extraFlags);
			IFCALL(update->CacheBitmapV2, update, &(update->cache_bitmap_v2_order));
			break;

		case ORDER_TYPE_BITMAP_COMPRESSED_V2:
			update_read_cache_bitmap_v2_order(s, &(update->cache_bitmap_v2_order), True, extraFlags);
			IFCALL(update->CacheBitmapV2, update, &(update->cache_bitmap_v2_order));
			break;

		case ORDER_TYPE_BITMAP_COMPRESSED_V3:
			update_read_cache_bitmap_v3_order(s, &(update->cache_bitmap_v3_order), True, extraFlags);
			IFCALL(update->CacheBitmapV3, update, &(update->cache_bitmap_v3_order));
			break;

		case ORDER_TYPE_CACHE_COLOR_TABLE:
			update_read_cache_color_table_order(s, &(update->cache_color_table_order), extraFlags);
			IFCALL(update->CacheColorTable, update, &(update->cache_color_table_order));
			break;

		case ORDER_TYPE_CACHE_GLYPH:
			if (update->glyph_v2)
			{
				update_read_cache_glyph_v2_order(s, &(update->cache_glyph_v2_order), extraFlags);
				IFCALL(update->CacheGlyph, update, &(update->cache_glyph_order));
			}
			else
			{
				update_read_cache_glyph_order(s, &(update->cache_glyph_order), extraFlags);
				IFCALL(update->CacheGlyphV2, update, &(update->cache_glyph_v2_order));
			}
			break;

		case ORDER_TYPE_CACHE_BRUSH:
			update_read_cache_brush_order(s, &(update->cache_brush_order), extraFlags);
			IFCALL(update->CacheBrush, update, &(update->cache_brush_order));
			break;

		default:
			break;
	}

	stream_set_mark(s, next);
}

void update_recv_altsec_order(rdpUpdate* update, STREAM* s, uint8 flags)
{
	uint8 orderType;

	orderType = flags >>= 2; /* orderType is in higher 6 bits of flags field */

	if (orderType < ALTSEC_DRAWING_ORDER_COUNT)
		printf("%s Alternate Secondary Drawing Order (0x%02X)\n", ALTSEC_DRAWING_ORDER_STRINGS[orderType], orderType);
	else
		printf("Unknown Alternate Secondary Drawing Order: 0x%02X\n", orderType);

	switch (orderType)
	{
		case ORDER_TYPE_CREATE_OFFSCREEN_BITMAP:
			update_read_create_offscreen_bitmap_order(s, &(update->create_offscreen_bitmap));
			IFCALL(update->CreateOffscreenBitmap, update, &(update->create_offscreen_bitmap));
			break;

		case ORDER_TYPE_SWITCH_SURFACE:
			update_read_switch_surface_order(s, &(update->switch_surface));
			IFCALL(update->SwitchSurface, update, &(update->switch_surface));
			break;

		case ORDER_TYPE_CREATE_NINE_GRID_BITMAP:
			update_read_create_nine_grid_bitmap_order(s, &(update->create_nine_grid_bitmap));
			IFCALL(update->CreateNineGridBitmap, update, &(update->create_nine_grid_bitmap));
			break;

		case ORDER_TYPE_FRAME_MARKER:
			update_read_frame_marker_order(s, &(update->frame_marker));
			IFCALL(update->FrameMarker, update, &(update->frame_marker));
			break;

		case ORDER_TYPE_STREAM_BITMAP_FIRST:
			update_read_stream_bitmap_first_order(s, &(update->stream_bitmap_first));
			IFCALL(update->StreamBitmapFirst, update, &(update->stream_bitmap_first));
			break;

		case ORDER_TYPE_STREAM_BITMAP_NEXT:
			update_read_stream_bitmap_next_order(s, &(update->stream_bitmap_next));
			IFCALL(update->StreamBitmapNext, update, &(update->stream_bitmap_next));
			break;

		case ORDER_TYPE_GDIPLUS_FIRST:
			update_read_draw_gdiplus_first_order(s, &(update->draw_gdiplus_first));
			IFCALL(update->DrawGdiPlusFirst, update, &(update->draw_gdiplus_first));
			break;

		case ORDER_TYPE_GDIPLUS_NEXT:
			update_read_draw_gdiplus_next_order(s, &(update->draw_gdiplus_next));
			IFCALL(update->DrawGdiPlusNext, update, &(update->draw_gdiplus_next));
			break;

		case ORDER_TYPE_GDIPLUS_END:
			update_read_draw_gdiplus_end_order(s, &(update->draw_gdiplus_end));
			IFCALL(update->DrawGdiPlusEnd, update, &(update->draw_gdiplus_end));
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_FIRST:
			update_read_draw_gdiplus_cache_first_order(s, &(update->draw_gdiplus_cache_first));
			IFCALL(update->DrawGdiPlusCacheFirst, update, &(update->draw_gdiplus_cache_first));
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_NEXT:
			update_read_draw_gdiplus_cache_next_order(s, &(update->draw_gdiplus_cache_next));
			IFCALL(update->DrawGdiPlusCacheNext, update, &(update->draw_gdiplus_cache_next));
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_END:
			update_read_draw_gdiplus_cache_end_order(s, &(update->draw_gdiplus_cache_end));
			IFCALL(update->DrawGdiPlusCacheEnd, update, &(update->draw_gdiplus_cache_end));
			break;

		case ORDER_TYPE_WINDOW:
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

	switch (controlFlags & ORDER_CLASS_MASK)
	{
		case ORDER_PRIMARY_CLASS:
			update_recv_primary_order(update, s, controlFlags);
			break;

		case ORDER_SECONDARY_CLASS:
			update_recv_secondary_order(update, s, controlFlags);
			break;

		case ORDER_ALTSEC_CLASS:
			update_recv_altsec_order(update, s, controlFlags);
			break;
	}
}

