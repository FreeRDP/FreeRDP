/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "window.h"

#include <winpr/crt.h>

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

#define SECONDARY_DRAWING_ORDER_COUNT	(ARRAYSIZE(SECONDARY_DRAWING_ORDER_STRINGS))

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

#define ALTSEC_DRAWING_ORDER_COUNT	(ARRAYSIZE(ALTSEC_DRAWING_ORDER_STRINGS))

#endif /* WITH_DEBUG_ORDERS */

static const BYTE PRIMARY_DRAWING_ORDER_FIELD_BYTES[] =
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

#define PRIMARY_DRAWING_ORDER_COUNT	(ARRAYSIZE(PRIMARY_DRAWING_ORDER_FIELD_BYTES))

static const BYTE CBR2_BPP[] =
{
		0, 0, 0, 8, 16, 24, 32
};

static const BYTE CBR23_BPP[] =
{
		0, 0, 0, 8, 16, 24, 32
};

static const BYTE BMF_BPP[] =
{
		0, 1, 0, 8, 16, 24, 32
};

static INLINE BOOL update_read_coord(wStream* s, INT32* coord, BOOL delta)
{
	INT8 lsi8;
	INT16 lsi16;

	if (delta)
	{
		if (stream_get_left(s) < 1)
			return FALSE;
		stream_read_BYTE(s, lsi8);
		*coord += lsi8;
	}
	else
	{
		if (stream_get_left(s) < 2)
			return FALSE;
		stream_read_UINT16(s, lsi16);
		*coord = lsi16;
	}
	return TRUE;
}

static INLINE BOOL update_read_color(wStream* s, UINT32* color)
{
	BYTE byte;

	if (stream_get_left(s) < 3)
		return FALSE;
	stream_read_BYTE(s, byte);
	*color = byte;
	stream_read_BYTE(s, byte);
	*color |= (byte << 8);
	stream_read_BYTE(s, byte);
	*color |= (byte << 16);
	return TRUE;
}

static INLINE void update_read_colorref(wStream* s, UINT32* color)
{
	BYTE byte;

	stream_read_BYTE(s, byte);
	*color = byte;
	stream_read_BYTE(s, byte);
	*color |= (byte << 8);
	stream_read_BYTE(s, byte);
	*color |= (byte << 16);
	stream_seek_BYTE(s);
}

static INLINE void update_read_color_quad(wStream* s, UINT32* color)
{
	BYTE byte;

	stream_read_BYTE(s, byte);
	*color = (byte << 16);
	stream_read_BYTE(s, byte);
	*color |= (byte << 8);
	stream_read_BYTE(s, byte);
	*color |= byte;
	stream_seek_BYTE(s);
}

static INLINE BOOL update_read_2byte_unsigned(wStream* s, UINT32* value)
{
	BYTE byte;

	if (stream_get_left(s) < 1)
		return FALSE;
	stream_read_BYTE(s, byte);

	if (byte & 0x80)
	{
		if (stream_get_left(s) < 1)
			return FALSE;

		*value = (byte & 0x7F) << 8;
		stream_read_BYTE(s, byte);
		*value |= byte;
	}
	else
	{
		*value = (byte & 0x7F);
	}
	return TRUE;
}

static INLINE BOOL update_read_2byte_signed(wStream* s, INT32* value)
{
	BYTE byte;
	BOOL negative;

	if (stream_get_left(s) < 1)
		return FALSE;

	stream_read_BYTE(s, byte);

	negative = (byte & 0x40) ? TRUE : FALSE;

	*value = (byte & 0x3F);

	if (byte & 0x80)
	{
		if (stream_get_left(s) < 1)
			return FALSE;
		stream_read_BYTE(s, byte);
		*value = (*value << 8) | byte;
	}

	if (negative)
		*value *= -1;
	return TRUE;
}

static INLINE BOOL update_read_4byte_unsigned(wStream* s, UINT32* value)
{
	BYTE byte;
	BYTE count;

	if (stream_get_left(s) < 1)
		return FALSE;
	stream_read_BYTE(s, byte);

	count = (byte & 0xC0) >> 6;
	if (stream_get_left(s) < count)
		return FALSE;

	switch (count)
	{
		case 0:
			*value = (byte & 0x3F);
			break;

		case 1:
			*value = (byte & 0x3F) << 8;
			stream_read_BYTE(s, byte);
			*value |= byte;
			break;

		case 2:
			*value = (byte & 0x3F) << 16;
			stream_read_BYTE(s, byte);
			*value |= (byte << 8);
			stream_read_BYTE(s, byte);
			*value |= byte;
			break;

		case 3:
			*value = (byte & 0x3F) << 24;
			stream_read_BYTE(s, byte);
			*value |= (byte << 16);
			stream_read_BYTE(s, byte);
			*value |= (byte << 8);
			stream_read_BYTE(s, byte);
			*value |= byte;
			break;

		default:
			break;
	}
	return TRUE;
}

static INLINE BOOL update_read_delta(wStream* s, INT32* value)
{
	BYTE byte;

	if (stream_get_left(s) < 1)
		return FALSE;
	stream_read_BYTE(s, byte);

	if (byte & 0x40)
		*value = (byte | ~0x3F);
	else
		*value = (byte & 0x3F);

	if (byte & 0x80)
	{
		if (stream_get_left(s) < 1)
			return FALSE;
		stream_read_BYTE(s, byte);
		*value = (*value << 8) | byte;
	}
	return TRUE;
}

static INLINE void update_read_glyph_delta(wStream* s, UINT16* value)
{
	BYTE byte;

	stream_read_BYTE(s, byte);

	if (byte == 0x80)
		stream_read_UINT16(s, *value);
	else
		*value = (byte & 0x3F);
}

static INLINE void update_seek_glyph_delta(wStream* s)
{
	BYTE byte;

	stream_read_BYTE(s, byte);

	if (byte & 0x80)
		stream_seek_BYTE(s);
}

static INLINE BOOL update_read_brush(wStream* s, rdpBrush* brush, BYTE fieldFlags)
{
	if (fieldFlags & ORDER_FIELD_01)
	{
		if (stream_get_left(s) < 1)
			return FALSE;
		stream_read_BYTE(s, brush->x);
	}

	if (fieldFlags & ORDER_FIELD_02)
	{
		if (stream_get_left(s) < 1)
			return FALSE;
		stream_read_BYTE(s, brush->y);
	}

	if (fieldFlags & ORDER_FIELD_03)
	{
		if (stream_get_left(s) < 1)
			return FALSE;
		stream_read_BYTE(s, brush->style);
	}

	if (fieldFlags & ORDER_FIELD_04)
	{
		if (stream_get_left(s) < 1)
			return FALSE;
		stream_read_BYTE(s, brush->hatch);
	}

	if (brush->style & CACHED_BRUSH)
	{
		brush->index = brush->hatch;

		brush->bpp = BMF_BPP[brush->style & 0x0F];

		if (brush->bpp == 0)
			brush->bpp = 1;
	}

	if (fieldFlags & ORDER_FIELD_05)
	{
		if (stream_get_left(s) < 7)
			return FALSE;
		brush->data = (BYTE*) brush->p8x8;
		stream_read_BYTE(s, brush->data[7]);
		stream_read_BYTE(s, brush->data[6]);
		stream_read_BYTE(s, brush->data[5]);
		stream_read_BYTE(s, brush->data[4]);
		stream_read_BYTE(s, brush->data[3]);
		stream_read_BYTE(s, brush->data[2]);
		stream_read_BYTE(s, brush->data[1]);
		brush->data[0] = brush->hatch;
	}
	return TRUE;
}

static INLINE BOOL update_read_delta_rects(wStream* s, DELTA_RECT* rectangles, int number)
{
	int i;
	BYTE flags = 0;
	BYTE* zeroBits;
	int zeroBitsSize;

	if (number > 45)
		number = 45;

	zeroBitsSize = ((number + 1) / 2);

	if (stream_get_left(s) < zeroBitsSize)
		return FALSE;
	stream_get_mark(s, zeroBits);
	stream_seek(s, zeroBitsSize);

	memset(rectangles, 0, sizeof(DELTA_RECT) * (number + 1));

	for (i = 1; i < number + 1; i++)
	{
		if ((i - 1) % 2 == 0)
			flags = zeroBits[(i - 1) / 2];

		if ((~flags & 0x80) && !update_read_delta(s, &rectangles[i].left))
			return FALSE;

		if ((~flags & 0x40) && !update_read_delta(s, &rectangles[i].top))
			return FALSE;

		if (~flags & 0x20)
		{
			if (!update_read_delta(s, &rectangles[i].width))
				return FALSE;
		}
		else
			rectangles[i].width = rectangles[i - 1].width;

		if (~flags & 0x10)
		{
			if (!update_read_delta(s, &rectangles[i].height))
				return FALSE;
		}
		else
			rectangles[i].height = rectangles[i - 1].height;

		rectangles[i].left = rectangles[i].left + rectangles[i - 1].left;
		rectangles[i].top = rectangles[i].top + rectangles[i - 1].top;

		flags <<= 4;
	}
	return TRUE;
}

static INLINE BOOL update_read_delta_points(wStream* s, DELTA_POINT* points, int number, INT16 x, INT16 y)
{
	int i;
	BYTE flags = 0;
	BYTE* zeroBits;
	int zeroBitsSize;

	zeroBitsSize = ((number + 3) / 4);

	if (stream_get_left(s) < zeroBitsSize)
		return FALSE;
	stream_get_mark(s, zeroBits);
	stream_seek(s, zeroBitsSize);

	memset(points, 0, sizeof(DELTA_POINT) * number);

	for (i = 0; i < number; i++)
	{
		if (i % 4 == 0)
			flags = zeroBits[i / 4];

		if ((~flags & 0x80) && !update_read_delta(s, &points[i].x))
			return FALSE;

		if ((~flags & 0x40) && !update_read_delta(s, &points[i].y))
			return FALSE;

		flags <<= 2;
	}
	return TRUE;
}


#define ORDER_FIELD_BYTE(NO, TARGET) \
	do {\
		if (orderInfo->fieldFlags & (1 << (NO-1))) \
		{ \
			if (stream_get_left(s) < 1) {\
				fprintf(stderr, "%s: error reading %s\n", __FUNCTION__, #TARGET); \
				return FALSE; \
			} \
			stream_read_BYTE(s, TARGET); \
		} \
	} while(0)

#define ORDER_FIELD_2BYTE(NO, TARGET1, TARGET2) \
	do {\
		if (orderInfo->fieldFlags & (1 << (NO-1))) \
		{ \
			if (stream_get_left(s) < 2) { \
				fprintf(stderr, "%s: error reading %s or %s\n", __FUNCTION__, #TARGET1, #TARGET2); \
				return FALSE; \
			} \
			stream_read_BYTE(s, TARGET1); \
			stream_read_BYTE(s, TARGET2); \
		} \
	} while(0)

#define ORDER_FIELD_UINT16(NO, TARGET) \
	do {\
		if (orderInfo->fieldFlags & (1 << (NO-1))) \
		{ \
			if (stream_get_left(s) < 2) { \
				fprintf(stderr, "%s: error reading %s\n", __FUNCTION__, #TARGET); \
				return FALSE; \
			} \
			stream_read_UINT16(s, TARGET); \
		} \
	} while(0)
#define ORDER_FIELD_UINT32(NO, TARGET) \
	do {\
		if (orderInfo->fieldFlags & (1 << (NO-1))) \
		{ \
			if (stream_get_left(s) < 4) { \
				fprintf(stderr, "%s: error reading %s\n", __FUNCTION__, #TARGET); \
				return FALSE; \
			} \
			stream_read_UINT32(s, TARGET); \
		} \
	} while(0)

#define ORDER_FIELD_COORD(NO, TARGET) \
	do { \
		if ((orderInfo->fieldFlags & (1 << (NO-1))) && !update_read_coord(s, &TARGET, orderInfo->deltaCoordinates)) { \
			fprintf(stderr, "%s: error reading %s\n", __FUNCTION__, #TARGET); \
			return FALSE; \
		} \
	} while(0)
#define ORDER_FIELD_COLOR(NO, TARGET) \
	do { \
		if ((orderInfo->fieldFlags & (1 << (NO-1))) && !update_read_color(s, &TARGET)) { \
			fprintf(stderr, "%s: error reading %s\n", __FUNCTION__, #TARGET); \
			return FALSE; \
		} \
	} while(0)


#define FIELD_SKIP_BUFFER16(s, TARGET_LEN) \
	do { \
		if (stream_get_left(s) < 2) {\
			fprintf(stderr, "%s: error reading length %s\n", __FUNCTION__, #TARGET_LEN); \
			return FALSE; \
		}\
		stream_read_UINT16(s, TARGET_LEN); \
		if (!stream_skip(s, TARGET_LEN)) { \
			fprintf(stderr, "%s: error skipping %d bytes\n", __FUNCTION__, TARGET_LEN); \
			return FALSE; \
		} \
	} while(0)

/* Primary Drawing Orders */

BOOL update_read_dstblt_order(wStream* s, ORDER_INFO* orderInfo, DSTBLT_ORDER* dstblt)
{
	ORDER_FIELD_COORD(1, dstblt->nLeftRect);
	ORDER_FIELD_COORD(2, dstblt->nTopRect);
	ORDER_FIELD_COORD(3, dstblt->nWidth);
	ORDER_FIELD_COORD(4, dstblt->nHeight);
	ORDER_FIELD_BYTE(5, dstblt->bRop);
	return TRUE;
}

BOOL update_read_patblt_order(wStream* s, ORDER_INFO* orderInfo, PATBLT_ORDER* patblt)
{
	ORDER_FIELD_COORD(1, patblt->nLeftRect);
	ORDER_FIELD_COORD(2, patblt->nTopRect);
	ORDER_FIELD_COORD(3, patblt->nWidth);
	ORDER_FIELD_COORD(4, patblt->nHeight);
	ORDER_FIELD_BYTE(5, patblt->bRop);
	ORDER_FIELD_COLOR(6, patblt->backColor);
	ORDER_FIELD_COLOR(7, patblt->foreColor);
	return update_read_brush(s, &patblt->brush, orderInfo->fieldFlags >> 7);
}

BOOL update_read_scrblt_order(wStream* s, ORDER_INFO* orderInfo, SCRBLT_ORDER* scrblt)
{
	ORDER_FIELD_COORD(1, scrblt->nLeftRect);
	ORDER_FIELD_COORD(2, scrblt->nTopRect);
	ORDER_FIELD_COORD(3, scrblt->nWidth);
	ORDER_FIELD_COORD(4, scrblt->nHeight);
	ORDER_FIELD_BYTE(5, scrblt->bRop);
	ORDER_FIELD_COORD(6, scrblt->nXSrc);
	ORDER_FIELD_COORD(7, scrblt->nYSrc);
	return TRUE;
}

BOOL update_read_opaque_rect_order(wStream* s, ORDER_INFO* orderInfo, OPAQUE_RECT_ORDER* opaque_rect)
{
	BYTE byte;

	ORDER_FIELD_COORD(1, opaque_rect->nLeftRect);
	ORDER_FIELD_COORD(2, opaque_rect->nTopRect);
	ORDER_FIELD_COORD(3, opaque_rect->nWidth);
	ORDER_FIELD_COORD(4, opaque_rect->nHeight);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
	{
		if (stream_get_left(s) < 1)
			return FALSE;
		stream_read_BYTE(s, byte);
		opaque_rect->color = (opaque_rect->color & 0xFFFFFF00) | byte;
	}

	if (orderInfo->fieldFlags & ORDER_FIELD_06) {
		if (stream_get_left(s) < 1)
			return FALSE;
		stream_read_BYTE(s, byte);
		opaque_rect->color = (opaque_rect->color & 0xFFFF00FF) | (byte << 8);
	}

	if (orderInfo->fieldFlags & ORDER_FIELD_07) {
		if (stream_get_left(s) < 1)
			return FALSE;
		stream_read_BYTE(s, byte);
		opaque_rect->color = (opaque_rect->color & 0xFF00FFFF) | (byte << 16);
	}
	return TRUE;
}

BOOL update_read_draw_nine_grid_order(wStream* s, ORDER_INFO* orderInfo, DRAW_NINE_GRID_ORDER* draw_nine_grid)
{
	ORDER_FIELD_COORD(1, draw_nine_grid->srcLeft);
	ORDER_FIELD_COORD(2, draw_nine_grid->srcTop);
	ORDER_FIELD_COORD(3, draw_nine_grid->srcRight);
	ORDER_FIELD_COORD(4, draw_nine_grid->srcBottom);
	ORDER_FIELD_UINT16(5, draw_nine_grid->bitmapId);
	return TRUE;
}

BOOL update_read_multi_dstblt_order(wStream* s, ORDER_INFO* orderInfo, MULTI_DSTBLT_ORDER* multi_dstblt)
{
	ORDER_FIELD_COORD(1, multi_dstblt->nLeftRect);
	ORDER_FIELD_COORD(2, multi_dstblt->nTopRect);
	ORDER_FIELD_COORD(3, multi_dstblt->nWidth);
	ORDER_FIELD_COORD(4, multi_dstblt->nHeight);
	ORDER_FIELD_BYTE(5, multi_dstblt->bRop);
	ORDER_FIELD_BYTE(6, multi_dstblt->numRectangles);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
	{
		if (stream_get_left(s) < 2)
			return FALSE;
		stream_read_UINT16(s, multi_dstblt->cbData);
		return update_read_delta_rects(s, multi_dstblt->rectangles, multi_dstblt->numRectangles);
	}
	return TRUE;
}

BOOL update_read_multi_patblt_order(wStream* s, ORDER_INFO* orderInfo, MULTI_PATBLT_ORDER* multi_patblt)
{
	ORDER_FIELD_COORD(1, multi_patblt->nLeftRect);
	ORDER_FIELD_COORD(2, multi_patblt->nTopRect);
	ORDER_FIELD_COORD(3, multi_patblt->nWidth);
	ORDER_FIELD_COORD(4, multi_patblt->nHeight);
	ORDER_FIELD_BYTE(5, multi_patblt->bRop);
	ORDER_FIELD_COLOR(6, multi_patblt->backColor);
	ORDER_FIELD_COLOR(7, multi_patblt->foreColor);

	if (!update_read_brush(s, &multi_patblt->brush, orderInfo->fieldFlags >> 7))
		return FALSE;

	ORDER_FIELD_BYTE(13, multi_patblt->numRectangles);

	if (orderInfo->fieldFlags & ORDER_FIELD_14)
	{
		if (stream_get_left(s) < 2)
			return FALSE;
		stream_read_UINT16(s, multi_patblt->cbData);
		if (!update_read_delta_rects(s, multi_patblt->rectangles, multi_patblt->numRectangles))
			return FALSE;
	}
	return TRUE;
}

BOOL update_read_multi_scrblt_order(wStream* s, ORDER_INFO* orderInfo, MULTI_SCRBLT_ORDER* multi_scrblt)
{
	ORDER_FIELD_COORD(1, multi_scrblt->nLeftRect);
	ORDER_FIELD_COORD(2, multi_scrblt->nTopRect);
	ORDER_FIELD_COORD(3, multi_scrblt->nWidth);
	ORDER_FIELD_COORD(4, multi_scrblt->nHeight);
	ORDER_FIELD_BYTE(5, multi_scrblt->bRop);
	ORDER_FIELD_COORD(6, multi_scrblt->nXSrc);
	ORDER_FIELD_COORD(7, multi_scrblt->nYSrc);
	ORDER_FIELD_BYTE(8, multi_scrblt->numRectangles);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
	{
		if (stream_get_left(s) < 2)
			return FALSE;
		stream_read_UINT16(s, multi_scrblt->cbData);
		return update_read_delta_rects(s, multi_scrblt->rectangles, multi_scrblt->numRectangles);
	}
	return TRUE;
}

BOOL update_read_multi_opaque_rect_order(wStream* s, ORDER_INFO* orderInfo, MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect)
{
	BYTE byte;
	ORDER_FIELD_COORD(1, multi_opaque_rect->nLeftRect);
	ORDER_FIELD_COORD(2, multi_opaque_rect->nTopRect);
	ORDER_FIELD_COORD(3, multi_opaque_rect->nWidth);
	ORDER_FIELD_COORD(4, multi_opaque_rect->nHeight);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
	{
		if (stream_get_left(s) < 1)
			return FALSE;
		stream_read_BYTE(s, byte);
		multi_opaque_rect->color = (multi_opaque_rect->color & 0xFFFFFF00) | byte;
	}

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
	{
		if (stream_get_left(s) < 1)
			return FALSE;
		stream_read_BYTE(s, byte);
		multi_opaque_rect->color = (multi_opaque_rect->color & 0xFFFF00FF) | (byte << 8);
	}

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
	{
		if (stream_get_left(s) < 1)
			return FALSE;
		stream_read_BYTE(s, byte);
		multi_opaque_rect->color = (multi_opaque_rect->color & 0xFF00FFFF) | (byte << 16);
	}

	ORDER_FIELD_BYTE(8, multi_opaque_rect->numRectangles);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
	{
		if (stream_get_left(s) < 2)
			return FALSE;
		stream_read_UINT16(s, multi_opaque_rect->cbData);
		return update_read_delta_rects(s, multi_opaque_rect->rectangles, multi_opaque_rect->numRectangles);
	}
	return TRUE;
}


BOOL update_read_multi_draw_nine_grid_order(wStream* s, ORDER_INFO* orderInfo, MULTI_DRAW_NINE_GRID_ORDER* multi_draw_nine_grid)
{
	ORDER_FIELD_COORD(1, multi_draw_nine_grid->srcLeft);
	ORDER_FIELD_COORD(2, multi_draw_nine_grid->srcTop);
	ORDER_FIELD_COORD(3, multi_draw_nine_grid->srcRight);
	ORDER_FIELD_COORD(4, multi_draw_nine_grid->srcBottom);
	ORDER_FIELD_UINT16(5, multi_draw_nine_grid->bitmapId);
	ORDER_FIELD_BYTE(6, multi_draw_nine_grid->nDeltaEntries);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
	{
		FIELD_SKIP_BUFFER16(s, multi_draw_nine_grid->cbData);
	}
	return TRUE;
}

BOOL update_read_line_to_order(wStream* s, ORDER_INFO* orderInfo, LINE_TO_ORDER* line_to)
{
	ORDER_FIELD_UINT16(1, line_to->backMode);
	ORDER_FIELD_COORD(2, line_to->nXStart);
	ORDER_FIELD_COORD(3, line_to->nYStart);
	ORDER_FIELD_COORD(4, line_to->nXEnd);
	ORDER_FIELD_COORD(5, line_to->nYEnd);
	ORDER_FIELD_COLOR(6, line_to->backColor);
	ORDER_FIELD_BYTE(7, line_to->bRop2);
	ORDER_FIELD_BYTE(8, line_to->penStyle);
	ORDER_FIELD_BYTE(9, line_to->penWidth);
	ORDER_FIELD_COLOR(10, line_to->penColor);
	return TRUE;
}

BOOL update_read_polyline_order(wStream* s, ORDER_INFO* orderInfo, POLYLINE_ORDER* polyline)
{
	UINT16 word;

	ORDER_FIELD_COORD(1, polyline->xStart);
	ORDER_FIELD_COORD(2, polyline->yStart);
	ORDER_FIELD_BYTE(3, polyline->bRop2);
	ORDER_FIELD_UINT16(4, word);
	ORDER_FIELD_COLOR(5, polyline->penColor);
	ORDER_FIELD_BYTE(6, polyline->numPoints);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
	{
		if (stream_get_left(s) < 1)
			return FALSE;
		stream_read_BYTE(s, polyline->cbData);

		if (polyline->points == NULL)
			polyline->points = (DELTA_POINT*) malloc(sizeof(DELTA_POINT) * polyline->numPoints);
		else
			polyline->points = (DELTA_POINT*) realloc(polyline->points, sizeof(DELTA_POINT) * polyline->numPoints);

		return update_read_delta_points(s, polyline->points, polyline->numPoints, polyline->xStart, polyline->yStart);
	}
	return TRUE;
}

BOOL update_read_memblt_order(wStream* s, ORDER_INFO* orderInfo, MEMBLT_ORDER* memblt)
{
	ORDER_FIELD_UINT16(1, memblt->cacheId);
	ORDER_FIELD_COORD(2, memblt->nLeftRect);
	ORDER_FIELD_COORD(3, memblt->nTopRect);
	ORDER_FIELD_COORD(4, memblt->nWidth);
	ORDER_FIELD_COORD(5, memblt->nHeight);
	ORDER_FIELD_BYTE(6, memblt->bRop);
	ORDER_FIELD_COORD(7, memblt->nXSrc);
	ORDER_FIELD_COORD(8, memblt->nYSrc);
	ORDER_FIELD_UINT16(9, memblt->cacheIndex);

	memblt->colorIndex = (memblt->cacheId >> 8);
	memblt->cacheId = (memblt->cacheId & 0xFF);
	return TRUE;
}

BOOL update_read_mem3blt_order(wStream* s, ORDER_INFO* orderInfo, MEM3BLT_ORDER* mem3blt)
{
	ORDER_FIELD_UINT16(1, mem3blt->cacheId);
	ORDER_FIELD_COORD(2, mem3blt->nLeftRect);
	ORDER_FIELD_COORD(3, mem3blt->nTopRect);
	ORDER_FIELD_COORD(4, mem3blt->nWidth);
	ORDER_FIELD_COORD(5, mem3blt->nHeight);
	ORDER_FIELD_BYTE(6, mem3blt->bRop);
	ORDER_FIELD_COORD(7, mem3blt->nXSrc);
	ORDER_FIELD_COORD(8, mem3blt->nYSrc);
	ORDER_FIELD_COLOR(9, mem3blt->backColor);
	ORDER_FIELD_COLOR(10, mem3blt->foreColor);

	if (!update_read_brush(s, &mem3blt->brush, orderInfo->fieldFlags >> 10))
		return FALSE;

	ORDER_FIELD_UINT16(16, mem3blt->cacheIndex);
	mem3blt->colorIndex = (mem3blt->cacheId >> 8);
	mem3blt->cacheId = (mem3blt->cacheId & 0xFF);
	return TRUE;
}

BOOL update_read_save_bitmap_order(wStream* s, ORDER_INFO* orderInfo, SAVE_BITMAP_ORDER* save_bitmap)
{
	ORDER_FIELD_UINT32(1, save_bitmap->savedBitmapPosition);
	ORDER_FIELD_COORD(2, save_bitmap->nLeftRect);
	ORDER_FIELD_COORD(3, save_bitmap->nTopRect);
	ORDER_FIELD_COORD(4, save_bitmap->nRightRect);
	ORDER_FIELD_COORD(5, save_bitmap->nBottomRect);
	ORDER_FIELD_BYTE(6, save_bitmap->operation);
	return TRUE;
}

BOOL update_read_glyph_index_order(wStream* s, ORDER_INFO* orderInfo, GLYPH_INDEX_ORDER* glyph_index)
{
	ORDER_FIELD_BYTE(1, glyph_index->cacheId);
	ORDER_FIELD_BYTE(2, glyph_index->flAccel);
	ORDER_FIELD_BYTE(3, glyph_index->ulCharInc);
	ORDER_FIELD_BYTE(4, glyph_index->fOpRedundant);
	ORDER_FIELD_COLOR(5, glyph_index->backColor);
	ORDER_FIELD_COLOR(6, glyph_index->foreColor);
	ORDER_FIELD_UINT16(7, glyph_index->bkLeft);
	ORDER_FIELD_UINT16(8, glyph_index->bkTop);
	ORDER_FIELD_UINT16(9, glyph_index->bkRight);
	ORDER_FIELD_UINT16(10, glyph_index->bkBottom);
	ORDER_FIELD_UINT16(11, glyph_index->opLeft);
	ORDER_FIELD_UINT16(12, glyph_index->opTop);
	ORDER_FIELD_UINT16(13, glyph_index->opRight);
	ORDER_FIELD_UINT16(14, glyph_index->opBottom);

	if (!update_read_brush(s, &glyph_index->brush, orderInfo->fieldFlags >> 14))
		return FALSE;

	ORDER_FIELD_UINT16(20, glyph_index->x);
	ORDER_FIELD_UINT16(21, glyph_index->y);

	if (orderInfo->fieldFlags & ORDER_FIELD_22)
	{
		if (stream_get_left(s) < 1)
			return FALSE;
		stream_read_BYTE(s, glyph_index->cbData);

		if (stream_get_left(s) < glyph_index->cbData)
			return FALSE;
		memcpy(glyph_index->data, s->pointer, glyph_index->cbData);
		stream_seek(s, glyph_index->cbData);
	}
	return TRUE;
}

BOOL update_read_fast_index_order(wStream* s, ORDER_INFO* orderInfo, FAST_INDEX_ORDER* fast_index)
{
	ORDER_FIELD_BYTE(1, fast_index->cacheId);
	ORDER_FIELD_2BYTE(2, fast_index->ulCharInc, fast_index->flAccel);
	ORDER_FIELD_COLOR(3, fast_index->backColor);
	ORDER_FIELD_COLOR(4, fast_index->foreColor);
	ORDER_FIELD_COORD(5, fast_index->bkLeft);
	ORDER_FIELD_COORD(6, fast_index->bkTop);
	ORDER_FIELD_COORD(7, fast_index->bkRight);
	ORDER_FIELD_COORD(8, fast_index->bkBottom);
	ORDER_FIELD_COORD(9, fast_index->opLeft);
	ORDER_FIELD_COORD(10, fast_index->opTop);
	ORDER_FIELD_COORD(11, fast_index->opRight);
	ORDER_FIELD_COORD(12, fast_index->opBottom);
	ORDER_FIELD_COORD(13, fast_index->x);
	ORDER_FIELD_COORD(14, fast_index->y);

	if (orderInfo->fieldFlags & ORDER_FIELD_15)
	{
		if (stream_get_left(s) < 1)
			return FALSE;
		stream_read_BYTE(s, fast_index->cbData);

		if (stream_get_left(s) < fast_index->cbData)
			return FALSE;
		memcpy(fast_index->data, s->pointer, fast_index->cbData);
		stream_seek(s, fast_index->cbData);
	}
	return TRUE;
}

BOOL update_read_fast_glyph_order(wStream* s, ORDER_INFO* orderInfo, FAST_GLYPH_ORDER* fast_glyph)
{
	BYTE* phold;
	GLYPH_DATA_V2* glyph;

	ORDER_FIELD_BYTE(1, fast_glyph->cacheId);
	ORDER_FIELD_2BYTE(2, fast_glyph->ulCharInc, fast_glyph->flAccel);
	ORDER_FIELD_COLOR(3, fast_glyph->backColor);
	ORDER_FIELD_COLOR(4, fast_glyph->foreColor);
	ORDER_FIELD_COORD(5, fast_glyph->bkLeft);
	ORDER_FIELD_COORD(6, fast_glyph->bkTop);
	ORDER_FIELD_COORD(7, fast_glyph->bkRight);
	ORDER_FIELD_COORD(8, fast_glyph->bkBottom);
	ORDER_FIELD_COORD(9, fast_glyph->opLeft);
	ORDER_FIELD_COORD(10, fast_glyph->opTop);
	ORDER_FIELD_COORD(11, fast_glyph->opRight);
	ORDER_FIELD_COORD(12, fast_glyph->opBottom);
	ORDER_FIELD_COORD(13, fast_glyph->x);
	ORDER_FIELD_COORD(14, fast_glyph->y);

	if (orderInfo->fieldFlags & ORDER_FIELD_15)
	{
		if (stream_get_left(s) < 1)
			return FALSE;

		stream_read_BYTE(s, fast_glyph->cbData);

		if (stream_get_left(s) < fast_glyph->cbData)
			return FALSE;

		memcpy(fast_glyph->data, s->pointer, fast_glyph->cbData);
		phold = s->pointer;

		if (!stream_skip(s, 1))
			return FALSE;

		if (fast_glyph->cbData > 1)
		{
			/* parse optional glyph data */
			glyph = &fast_glyph->glyphData;
			glyph->cacheIndex = fast_glyph->data[0];

			if (!update_read_2byte_signed(s, &glyph->x) ||
				!update_read_2byte_signed(s, &glyph->y) ||
				!update_read_2byte_unsigned(s, &glyph->cx) ||
				!update_read_2byte_unsigned(s, &glyph->cy))
				return FALSE;

			glyph->cb = ((glyph->cx + 7) / 8) * glyph->cy;
			glyph->cb += ((glyph->cb % 4) > 0) ? 4 - (glyph->cb % 4) : 0;

			if (stream_get_left(s) < glyph->cb)
				return FALSE;

			glyph->aj = (BYTE*) malloc(glyph->cb);
			stream_read(s, glyph->aj, glyph->cb);
		}

		s->pointer = phold + fast_glyph->cbData;
	}
	return TRUE;
}

BOOL update_read_polygon_sc_order(wStream* s, ORDER_INFO* orderInfo, POLYGON_SC_ORDER* polygon_sc)
{
	ORDER_FIELD_COORD(1, polygon_sc->xStart);
	ORDER_FIELD_COORD(2, polygon_sc->yStart);
	ORDER_FIELD_BYTE(3, polygon_sc->bRop2);
	ORDER_FIELD_BYTE(4, polygon_sc->fillMode);
	ORDER_FIELD_COLOR(5, polygon_sc->brushColor);
	ORDER_FIELD_BYTE(6, polygon_sc->numPoints);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
	{
		if (stream_get_left(s) < 1)
			return FALSE;
		stream_read_BYTE(s, polygon_sc->cbData);

		if (polygon_sc->points == NULL)
			polygon_sc->points = (DELTA_POINT*) malloc(sizeof(DELTA_POINT) * polygon_sc->numPoints);
		else
			polygon_sc->points = (DELTA_POINT*) realloc(polygon_sc->points, sizeof(DELTA_POINT) * polygon_sc->numPoints);

		return update_read_delta_points(s, polygon_sc->points, polygon_sc->numPoints, polygon_sc->xStart, polygon_sc->yStart);
	}
	return TRUE;
}

BOOL update_read_polygon_cb_order(wStream* s, ORDER_INFO* orderInfo, POLYGON_CB_ORDER* polygon_cb)
{
	ORDER_FIELD_COORD(1, polygon_cb->xStart);
	ORDER_FIELD_COORD(2, polygon_cb->yStart);
	ORDER_FIELD_BYTE(3, polygon_cb->bRop2);
	ORDER_FIELD_BYTE(4, polygon_cb->fillMode);
	ORDER_FIELD_COLOR(5, polygon_cb->backColor);
	ORDER_FIELD_COLOR(6, polygon_cb->foreColor);

	if (!update_read_brush(s, &polygon_cb->brush, orderInfo->fieldFlags >> 6))
		return FALSE;

	ORDER_FIELD_BYTE(12, polygon_cb->numPoints);

	if (orderInfo->fieldFlags & ORDER_FIELD_13)
	{
		if (stream_get_left(s) < 1)
			return FALSE;
		stream_read_BYTE(s, polygon_cb->cbData);

		if (polygon_cb->points == NULL)
			polygon_cb->points = (DELTA_POINT*) malloc(sizeof(DELTA_POINT) * polygon_cb->numPoints);
		else
			polygon_cb->points = (DELTA_POINT*) realloc(polygon_cb->points, sizeof(DELTA_POINT) * polygon_cb->numPoints);

		if (!update_read_delta_points(s, polygon_cb->points, polygon_cb->numPoints, polygon_cb->xStart, polygon_cb->yStart))
			return FALSE;
	}

	polygon_cb->backMode = (polygon_cb->bRop2 & 0x80) ? BACKMODE_TRANSPARENT : BACKMODE_OPAQUE;
	polygon_cb->bRop2 = (polygon_cb->bRop2 & 0x1F);
	return TRUE;
}

BOOL update_read_ellipse_sc_order(wStream* s, ORDER_INFO* orderInfo, ELLIPSE_SC_ORDER* ellipse_sc)
{
	ORDER_FIELD_COORD(1, ellipse_sc->leftRect);
	ORDER_FIELD_COORD(2, ellipse_sc->topRect);
	ORDER_FIELD_COORD(3, ellipse_sc->rightRect);
	ORDER_FIELD_COORD(4, ellipse_sc->bottomRect);
	ORDER_FIELD_BYTE(5, ellipse_sc->bRop2);
	ORDER_FIELD_BYTE(6, ellipse_sc->fillMode);
	ORDER_FIELD_COLOR(7, ellipse_sc->color);
	return TRUE;
}

BOOL update_read_ellipse_cb_order(wStream* s, ORDER_INFO* orderInfo, ELLIPSE_CB_ORDER* ellipse_cb)
{
	ORDER_FIELD_COORD(1, ellipse_cb->leftRect);
	ORDER_FIELD_COORD(2, ellipse_cb->topRect);
	ORDER_FIELD_COORD(3, ellipse_cb->rightRect);
	ORDER_FIELD_COORD(4, ellipse_cb->bottomRect);
	ORDER_FIELD_BYTE(5, ellipse_cb->bRop2);
	ORDER_FIELD_BYTE(6, ellipse_cb->fillMode);
	ORDER_FIELD_COLOR(7, ellipse_cb->backColor);
	ORDER_FIELD_COLOR(8, ellipse_cb->foreColor);
	return update_read_brush(s, &ellipse_cb->brush, orderInfo->fieldFlags >> 8);
}

/* Secondary Drawing Orders */

BOOL update_read_cache_bitmap_order(wStream* s, CACHE_BITMAP_ORDER* cache_bitmap_order, BOOL compressed, UINT16 flags)
{
	if (stream_get_left(s) < 9)
		return FALSE;
	stream_read_BYTE(s, cache_bitmap_order->cacheId); /* cacheId (1 byte) */
	stream_seek_BYTE(s); /* pad1Octet (1 byte) */
	stream_read_BYTE(s, cache_bitmap_order->bitmapWidth); /* bitmapWidth (1 byte) */
	stream_read_BYTE(s, cache_bitmap_order->bitmapHeight); /* bitmapHeight (1 byte) */
	stream_read_BYTE(s, cache_bitmap_order->bitmapBpp); /* bitmapBpp (1 byte) */
	stream_read_UINT16(s, cache_bitmap_order->bitmapLength); /* bitmapLength (2 bytes) */
	stream_read_UINT16(s, cache_bitmap_order->cacheIndex); /* cacheIndex (2 bytes) */

	if (compressed)
	{
		if ((flags & NO_BITMAP_COMPRESSION_HDR) == 0)
		{
			BYTE* bitmapComprHdr = (BYTE*) &(cache_bitmap_order->bitmapComprHdr);
			if (stream_get_left(s) < 8)
				return FALSE;
			stream_read(s, bitmapComprHdr, 8); /* bitmapComprHdr (8 bytes) */
			cache_bitmap_order->bitmapLength -= 8;
		}

		if (stream_get_left(s) < cache_bitmap_order->bitmapLength)
			return FALSE;

		stream_get_mark(s, cache_bitmap_order->bitmapDataStream);
		stream_seek(s, cache_bitmap_order->bitmapLength);
	}
	else
	{
		if (stream_get_left(s) < cache_bitmap_order->bitmapLength)
			return FALSE;

		stream_get_mark(s, cache_bitmap_order->bitmapDataStream);
		stream_seek(s, cache_bitmap_order->bitmapLength); /* bitmapDataStream */
	}
	cache_bitmap_order->compressed = compressed;
	return TRUE;
}

BOOL update_read_cache_bitmap_v2_order(wStream* s, CACHE_BITMAP_V2_ORDER* cache_bitmap_v2_order, BOOL compressed, UINT16 flags)
{
	BYTE bitsPerPixelId;

	cache_bitmap_v2_order->cacheId = flags & 0x0003;
	cache_bitmap_v2_order->flags = (flags & 0xFF80) >> 7;

	bitsPerPixelId = (flags & 0x0078) >> 3;
	cache_bitmap_v2_order->bitmapBpp = CBR2_BPP[bitsPerPixelId];

	if (cache_bitmap_v2_order->flags & CBR2_PERSISTENT_KEY_PRESENT)
	{
		if (stream_get_left(s) < 8)
			return FALSE;
		stream_read_UINT32(s, cache_bitmap_v2_order->key1); /* key1 (4 bytes) */
		stream_read_UINT32(s, cache_bitmap_v2_order->key2); /* key2 (4 bytes) */
	}

	if (cache_bitmap_v2_order->flags & CBR2_HEIGHT_SAME_AS_WIDTH)
	{
		if (!update_read_2byte_unsigned(s, &cache_bitmap_v2_order->bitmapWidth)) /* bitmapWidth */
			return FALSE;
		cache_bitmap_v2_order->bitmapHeight = cache_bitmap_v2_order->bitmapWidth;
	}
	else
	{
		if (!update_read_2byte_unsigned(s, &cache_bitmap_v2_order->bitmapWidth) || /* bitmapWidth */
		   !update_read_2byte_unsigned(s, &cache_bitmap_v2_order->bitmapHeight)) /* bitmapHeight */
			return FALSE;
	}

	if (!update_read_4byte_unsigned(s, &cache_bitmap_v2_order->bitmapLength) || /* bitmapLength */
		!update_read_2byte_unsigned(s, &cache_bitmap_v2_order->cacheIndex)) /* cacheIndex */
		return FALSE;

	if (cache_bitmap_v2_order->flags & CBR2_DO_NOT_CACHE)
		cache_bitmap_v2_order->cacheIndex = BITMAP_CACHE_WAITING_LIST_INDEX;

	if (compressed)
	{
		if (!(cache_bitmap_v2_order->flags & CBR2_NO_BITMAP_COMPRESSION_HDR))
		{
			if (stream_get_left(s) < 8)
				return FALSE;

			stream_read_UINT16(s, cache_bitmap_v2_order->cbCompFirstRowSize); /* cbCompFirstRowSize (2 bytes) */
			stream_read_UINT16(s, cache_bitmap_v2_order->cbCompMainBodySize); /* cbCompMainBodySize (2 bytes) */
			stream_read_UINT16(s, cache_bitmap_v2_order->cbScanWidth); /* cbScanWidth (2 bytes) */
			stream_read_UINT16(s, cache_bitmap_v2_order->cbUncompressedSize); /* cbUncompressedSize (2 bytes) */
			cache_bitmap_v2_order->bitmapLength = cache_bitmap_v2_order->cbCompMainBodySize;
		}

		if (stream_get_left(s) < cache_bitmap_v2_order->bitmapLength)
			return FALSE;
		stream_get_mark(s, cache_bitmap_v2_order->bitmapDataStream);
		stream_seek(s, cache_bitmap_v2_order->bitmapLength);
	}
	else
	{
		if (stream_get_left(s) < cache_bitmap_v2_order->bitmapLength)
			return FALSE;
		stream_get_mark(s, cache_bitmap_v2_order->bitmapDataStream);
		stream_seek(s, cache_bitmap_v2_order->bitmapLength);
	}
	cache_bitmap_v2_order->compressed = compressed;
	return TRUE;
}

BOOL update_read_cache_bitmap_v3_order(wStream* s, CACHE_BITMAP_V3_ORDER* cache_bitmap_v3_order, BOOL compressed, UINT16 flags)
{
	BYTE bitsPerPixelId;
	BITMAP_DATA_EX* bitmapData;

	cache_bitmap_v3_order->cacheId = flags & 0x00000003;
	cache_bitmap_v3_order->flags = (flags & 0x0000FF80) >> 7;

	bitsPerPixelId = (flags & 0x00000078) >> 3;
	cache_bitmap_v3_order->bpp = CBR23_BPP[bitsPerPixelId];

	if (stream_get_left(s) < 21)
		return FALSE;
	stream_read_UINT16(s, cache_bitmap_v3_order->cacheIndex); /* cacheIndex (2 bytes) */
	stream_read_UINT32(s, cache_bitmap_v3_order->key1); /* key1 (4 bytes) */
	stream_read_UINT32(s, cache_bitmap_v3_order->key2); /* key2 (4 bytes) */

	bitmapData = &cache_bitmap_v3_order->bitmapData;

	stream_read_BYTE(s, bitmapData->bpp);
	stream_seek_BYTE(s); /* reserved1 (1 byte) */
	stream_seek_BYTE(s); /* reserved2 (1 byte) */
	stream_read_BYTE(s, bitmapData->codecID); /* codecID (1 byte) */
	stream_read_UINT16(s, bitmapData->width); /* width (2 bytes) */
	stream_read_UINT16(s, bitmapData->height); /* height (2 bytes) */
	stream_read_UINT32(s, bitmapData->length); /* length (4 bytes) */

	if (stream_get_left(s) < bitmapData->length)
		return FALSE;
	if (bitmapData->data == NULL)
		bitmapData->data = (BYTE*) malloc(bitmapData->length);
	else
		bitmapData->data = (BYTE*) realloc(bitmapData->data, bitmapData->length);

	stream_read(s, bitmapData->data, bitmapData->length);
	return TRUE;
}

BOOL update_read_cache_color_table_order(wStream* s, CACHE_COLOR_TABLE_ORDER* cache_color_table_order, UINT16 flags)
{
	int i;
	UINT32* colorTable;

	if (stream_get_left(s) < 3)
		return FALSE;

	stream_read_BYTE(s, cache_color_table_order->cacheIndex); /* cacheIndex (1 byte) */
	stream_read_UINT16(s, cache_color_table_order->numberColors); /* numberColors (2 bytes) */

	if (cache_color_table_order->numberColors != 256)
	{
		/* This field MUST be set to 256 */
		return FALSE;
	}

	if (stream_get_left(s) < cache_color_table_order->numberColors * 4)
		return FALSE;

	colorTable = (UINT32*) &cache_color_table_order->colorTable;

	for (i = 0; i < (int) cache_color_table_order->numberColors; i++)
	{
		update_read_color_quad(s, &colorTable[i]);
	}

	return TRUE;
}

BOOL update_read_cache_glyph_order(wStream* s, CACHE_GLYPH_ORDER* cache_glyph_order, UINT16 flags)
{
	int i;
	INT16 lsi16;
	GLYPH_DATA* glyph;

	if (stream_get_left(s) < 2)
		return FALSE;

	stream_read_BYTE(s, cache_glyph_order->cacheId); /* cacheId (1 byte) */
	stream_read_BYTE(s, cache_glyph_order->cGlyphs); /* cGlyphs (1 byte) */

	for (i = 0; i < (int) cache_glyph_order->cGlyphs; i++)
	{
		glyph = &cache_glyph_order->glyphData[i];

		if (stream_get_left(s) < 10)
			return FALSE;

		stream_read_UINT16(s, glyph->cacheIndex);
		stream_read_UINT16(s, lsi16);
		glyph->x = lsi16;
		stream_read_UINT16(s, lsi16);
		glyph->y = lsi16;
		stream_read_UINT16(s, glyph->cx);
		stream_read_UINT16(s, glyph->cy);

		glyph->cb = ((glyph->cx + 7) / 8) * glyph->cy;
		glyph->cb += ((glyph->cb % 4) > 0) ? 4 - (glyph->cb % 4) : 0;

		if (stream_get_left(s) < glyph->cb)
			return FALSE;

		glyph->aj = (BYTE*) malloc(glyph->cb);
		stream_read(s, glyph->aj, glyph->cb);
	}

	if (flags & CG_GLYPH_UNICODE_PRESENT)
	{
		return stream_skip(s, cache_glyph_order->cGlyphs * 2);
	}

	return TRUE;
}

BOOL update_read_cache_glyph_v2_order(wStream* s, CACHE_GLYPH_V2_ORDER* cache_glyph_v2_order, UINT16 flags)
{
	int i;
	GLYPH_DATA_V2* glyph;

	cache_glyph_v2_order->cacheId = (flags & 0x000F);
	cache_glyph_v2_order->flags = (flags & 0x00F0) >> 4;
	cache_glyph_v2_order->cGlyphs = (flags & 0xFF00) >> 8;

	for (i = 0; i < (int) cache_glyph_v2_order->cGlyphs; i++)
	{
		glyph = &cache_glyph_v2_order->glyphData[i];

		if (stream_get_left(s) < 1)
			return FALSE;

		stream_read_BYTE(s, glyph->cacheIndex);

		if (!update_read_2byte_signed(s, &glyph->x) ||
				!update_read_2byte_signed(s, &glyph->y) ||
				!update_read_2byte_unsigned(s, &glyph->cx) ||
				!update_read_2byte_unsigned(s, &glyph->cy))
		{
			return FALSE;
		}

		glyph->cb = ((glyph->cx + 7) / 8) * glyph->cy;
		glyph->cb += ((glyph->cb % 4) > 0) ? 4 - (glyph->cb % 4) : 0;

		if (stream_get_left(s) < glyph->cb)
			return FALSE;

		glyph->aj = (BYTE*) malloc(glyph->cb);
		stream_read(s, glyph->aj, glyph->cb);
	}

	if (flags & CG_GLYPH_UNICODE_PRESENT)
	{
		return stream_skip(s, cache_glyph_v2_order->cGlyphs * 2);
	}

	return TRUE;
}

BOOL update_decompress_brush(wStream* s, BYTE* output, BYTE bpp)
{
	int index;
	int x, y, k;
	BYTE byte = 0;
	BYTE* palette;
	int bytesPerPixel;

	palette = s->pointer + 16;
	bytesPerPixel = ((bpp + 1) / 8);

	if (stream_get_left(s) < 16) // 64 / 4
		return FALSE;

	for (y = 7; y >= 0; y--)
	{
		for (x = 0; x < 8; x++)
		{
			if ((x % 4) == 0)
				stream_read_BYTE(s, byte);

			index = ((byte >> ((3 - (x % 4)) * 2)) & 0x03);

			for (k = 0; k < bytesPerPixel; k++)
			{
				output[((y * 8 + x) * bytesPerPixel) + k] = palette[(index * bytesPerPixel) + k];
			}
		}
	}
	return TRUE;
}

BOOL update_read_cache_brush_order(wStream* s, CACHE_BRUSH_ORDER* cache_brush_order, UINT16 flags)
{
	int i;
	int size;
	BYTE iBitmapFormat;
	BOOL compressed = FALSE;

	if (stream_get_left(s) < 6)
		return FALSE;

	stream_read_BYTE(s, cache_brush_order->index); /* cacheEntry (1 byte) */

	stream_read_BYTE(s, iBitmapFormat); /* iBitmapFormat (1 byte) */
	cache_brush_order->bpp = BMF_BPP[iBitmapFormat];

	stream_read_BYTE(s, cache_brush_order->cx); /* cx (1 byte) */
	stream_read_BYTE(s, cache_brush_order->cy); /* cy (1 byte) */
	stream_read_BYTE(s, cache_brush_order->style); /* style (1 byte) */
	stream_read_BYTE(s, cache_brush_order->length); /* iBytes (1 byte) */

	if ((cache_brush_order->cx == 8) && (cache_brush_order->cy == 8))
	{
		size = (cache_brush_order->bpp == 1) ? 8 : 8 * 8 * cache_brush_order->bpp;

		if (cache_brush_order->bpp == 1)
		{
			if (cache_brush_order->length != 8)
			{
				fprintf(stderr, "incompatible 1bpp brush of length:%d\n", cache_brush_order->length);
				return TRUE; // should be FALSE ?
			}

			/* rows are encoded in reverse order */
			if (stream_get_left(s) < 8)
				return FALSE;

			for (i = 7; i >= 0; i--)
			{
				stream_read_BYTE(s, cache_brush_order->data[i]);
			}
		}
		else
		{
			if ((iBitmapFormat == BMF_8BPP) && (cache_brush_order->length == 20))
				compressed = TRUE;
			else if ((iBitmapFormat == BMF_16BPP) && (cache_brush_order->length == 24))
				compressed = TRUE;
			else if ((iBitmapFormat == BMF_32BPP) && (cache_brush_order->length == 32))
				compressed = TRUE;

			if (compressed != FALSE)
			{
				/* compressed brush */
				if (!update_decompress_brush(s, cache_brush_order->data, cache_brush_order->bpp))
					return FALSE;
			}
			else
			{
				/* uncompressed brush */
				int scanline = (cache_brush_order->bpp / 8) * 8;

				if (stream_get_left(s) < scanline * 8)
					return FALSE;

				for (i = 7; i >= 0; i--)
				{
					stream_read(s, &cache_brush_order->data[i * scanline], scanline);
				}
			}
		}
	}

	return TRUE;
}

/* Alternate Secondary Drawing Orders */

BOOL update_read_create_offscreen_bitmap_order(wStream* s, CREATE_OFFSCREEN_BITMAP_ORDER* create_offscreen_bitmap)
{
	UINT16 flags;
	BOOL deleteListPresent;
	OFFSCREEN_DELETE_LIST* deleteList;

	if (stream_get_left(s) < 6)
		return FALSE;
	stream_read_UINT16(s, flags); /* flags (2 bytes) */
	create_offscreen_bitmap->id = flags & 0x7FFF;
	deleteListPresent = (flags & 0x8000) ? TRUE : FALSE;

	stream_read_UINT16(s, create_offscreen_bitmap->cx); /* cx (2 bytes) */
	stream_read_UINT16(s, create_offscreen_bitmap->cy); /* cy (2 bytes) */

	deleteList = &(create_offscreen_bitmap->deleteList);
	if (deleteListPresent)
	{
		int i;
		if (stream_get_left(s) < 2)
			return FALSE;
		stream_read_UINT16(s, deleteList->cIndices);

		if (deleteList->cIndices > deleteList->sIndices)
		{
			deleteList->sIndices = deleteList->cIndices;
			deleteList->indices = realloc(deleteList->indices, deleteList->sIndices * 2);
		}

		if (stream_get_left(s) < 2 * deleteList->cIndices)
			return FALSE;

		for (i = 0; i < (int) deleteList->cIndices; i++)
		{
			stream_read_UINT16(s, deleteList->indices[i]);
		}
	}
	else
	{
		deleteList->cIndices = 0;
	}
	return TRUE;
}

BOOL update_read_switch_surface_order(wStream* s, SWITCH_SURFACE_ORDER* switch_surface)
{
	if (stream_get_left(s) < 2)
		return FALSE;
	stream_read_UINT16(s, switch_surface->bitmapId); /* bitmapId (2 bytes) */
	return TRUE;
}

BOOL update_read_create_nine_grid_bitmap_order(wStream* s, CREATE_NINE_GRID_BITMAP_ORDER* create_nine_grid_bitmap)
{
	NINE_GRID_BITMAP_INFO* nineGridInfo;

	if (stream_get_left(s) < 19)
		return FALSE;
	stream_read_BYTE(s, create_nine_grid_bitmap->bitmapBpp); /* bitmapBpp (1 byte) */
	stream_read_UINT16(s, create_nine_grid_bitmap->bitmapId); /* bitmapId (2 bytes) */

	nineGridInfo = &(create_nine_grid_bitmap->nineGridInfo);
	stream_read_UINT32(s, nineGridInfo->flFlags); /* flFlags (4 bytes) */
	stream_read_UINT16(s, nineGridInfo->ulLeftWidth); /* ulLeftWidth (2 bytes) */
	stream_read_UINT16(s, nineGridInfo->ulRightWidth); /* ulRightWidth (2 bytes) */
	stream_read_UINT16(s, nineGridInfo->ulTopHeight); /* ulTopHeight (2 bytes) */
	stream_read_UINT16(s, nineGridInfo->ulBottomHeight); /* ulBottomHeight (2 bytes) */
	update_read_colorref(s, &nineGridInfo->crTransparent); /* crTransparent (4 bytes) */
	return TRUE;
}

BOOL update_read_frame_marker_order(wStream* s, FRAME_MARKER_ORDER* frame_marker)
{
	if (stream_get_left(s) < 4)
		return FALSE;
	stream_read_UINT32(s, frame_marker->action); /* action (4 bytes) */
	return TRUE;
}

BOOL update_read_stream_bitmap_first_order(wStream* s, STREAM_BITMAP_FIRST_ORDER* stream_bitmap_first)
{
	if (stream_get_left(s) < 10)	// 8 + 2 at least
		return FALSE;
	stream_read_BYTE(s, stream_bitmap_first->bitmapFlags); /* bitmapFlags (1 byte) */
	stream_read_BYTE(s, stream_bitmap_first->bitmapBpp); /* bitmapBpp (1 byte) */
	stream_read_UINT16(s, stream_bitmap_first->bitmapType); /* bitmapType (2 bytes) */
	stream_read_UINT16(s, stream_bitmap_first->bitmapWidth); /* bitmapWidth (2 bytes) */
	stream_read_UINT16(s, stream_bitmap_first->bitmapHeight); /* bitmapHeigth (2 bytes) */

	if (stream_bitmap_first->bitmapFlags & STREAM_BITMAP_V2) {
		if (stream_get_left(s) < 4)
			return FALSE;
		stream_read_UINT32(s, stream_bitmap_first->bitmapSize); /* bitmapSize (4 bytes) */
	} else {
		if (stream_get_left(s) < 2)
			return FALSE;
		stream_read_UINT16(s, stream_bitmap_first->bitmapSize); /* bitmapSize (2 bytes) */
	}

	FIELD_SKIP_BUFFER16(s, stream_bitmap_first->bitmapBlockSize); /* bitmapBlockSize(2 bytes) + bitmapBlock */
	return TRUE;
}

BOOL update_read_stream_bitmap_next_order(wStream* s, STREAM_BITMAP_NEXT_ORDER* stream_bitmap_next)
{
	if (stream_get_left(s) < 5)
		return FALSE;
	stream_read_BYTE(s, stream_bitmap_next->bitmapFlags); /* bitmapFlags (1 byte) */
	stream_read_UINT16(s, stream_bitmap_next->bitmapType); /* bitmapType (2 bytes) */
	FIELD_SKIP_BUFFER16(s, stream_bitmap_next->bitmapBlockSize); /* bitmapBlockSize(2 bytes) + bitmapBlock */
	return TRUE;
}

BOOL update_read_draw_gdiplus_first_order(wStream* s, DRAW_GDIPLUS_FIRST_ORDER* draw_gdiplus_first)
{
	if (stream_get_left(s) < 11)
		return FALSE;
	stream_seek_BYTE(s); /* pad1Octet (1 byte) */
	stream_read_UINT16(s, draw_gdiplus_first->cbSize); /* cbSize (2 bytes) */
	stream_read_UINT32(s, draw_gdiplus_first->cbTotalSize); /* cbTotalSize (4 bytes) */
	stream_read_UINT32(s, draw_gdiplus_first->cbTotalEmfSize); /* cbTotalEmfSize (4 bytes) */

	return stream_skip(s, draw_gdiplus_first->cbSize); /* emfRecords */
}

BOOL update_read_draw_gdiplus_next_order(wStream* s, DRAW_GDIPLUS_NEXT_ORDER* draw_gdiplus_next)
{
	if (stream_get_left(s) < 3)
		return FALSE;
	stream_seek_BYTE(s); /* pad1Octet (1 byte) */
	FIELD_SKIP_BUFFER16(s, draw_gdiplus_next->cbSize); /* cbSize(2 bytes) + emfRecords */
	return TRUE;
}

BOOL update_read_draw_gdiplus_end_order(wStream* s, DRAW_GDIPLUS_END_ORDER* draw_gdiplus_end)
{
	if (stream_get_left(s) < 11)
		return FALSE;
	stream_seek_BYTE(s); /* pad1Octet (1 byte) */
	stream_read_UINT16(s, draw_gdiplus_end->cbSize); /* cbSize (2 bytes) */
	stream_read_UINT32(s, draw_gdiplus_end->cbTotalSize); /* cbTotalSize (4 bytes) */
	stream_read_UINT32(s, draw_gdiplus_end->cbTotalEmfSize); /* cbTotalEmfSize (4 bytes) */

	return stream_skip(s, draw_gdiplus_end->cbSize); /* emfRecords */
}

BOOL update_read_draw_gdiplus_cache_first_order(wStream* s, DRAW_GDIPLUS_CACHE_FIRST_ORDER* draw_gdiplus_cache_first)
{
	if (stream_get_left(s) < 11)
		return FALSE;
	stream_read_BYTE(s, draw_gdiplus_cache_first->flags); /* flags (1 byte) */
	stream_read_UINT16(s, draw_gdiplus_cache_first->cacheType); /* cacheType (2 bytes) */
	stream_read_UINT16(s, draw_gdiplus_cache_first->cacheIndex); /* cacheIndex (2 bytes) */
	stream_read_UINT16(s, draw_gdiplus_cache_first->cbSize); /* cbSize (2 bytes) */
	stream_read_UINT32(s, draw_gdiplus_cache_first->cbTotalSize); /* cbTotalSize (4 bytes) */

	return stream_skip(s, draw_gdiplus_cache_first->cbSize); /* emfRecords */
}

BOOL update_read_draw_gdiplus_cache_next_order(wStream* s, DRAW_GDIPLUS_CACHE_NEXT_ORDER* draw_gdiplus_cache_next)
{
	if (stream_get_left(s) < 7)
		return FALSE;
	stream_read_BYTE(s, draw_gdiplus_cache_next->flags); /* flags (1 byte) */
	stream_read_UINT16(s, draw_gdiplus_cache_next->cacheType); /* cacheType (2 bytes) */
	stream_read_UINT16(s, draw_gdiplus_cache_next->cacheIndex); /* cacheIndex (2 bytes) */
	FIELD_SKIP_BUFFER16(s, draw_gdiplus_cache_next->cbSize); /* cbSize(2 bytes) + emfRecords */
	return TRUE;

}

BOOL update_read_draw_gdiplus_cache_end_order(wStream* s, DRAW_GDIPLUS_CACHE_END_ORDER* draw_gdiplus_cache_end)
{
	if (stream_get_left(s) < 11)
		return FALSE;
	stream_read_BYTE(s, draw_gdiplus_cache_end->flags); /* flags (1 byte) */
	stream_read_UINT16(s, draw_gdiplus_cache_end->cacheType); /* cacheType (2 bytes) */
	stream_read_UINT16(s, draw_gdiplus_cache_end->cacheIndex); /* cacheIndex (2 bytes) */
	stream_read_UINT16(s, draw_gdiplus_cache_end->cbSize); /* cbSize (2 bytes) */
	stream_read_UINT32(s, draw_gdiplus_cache_end->cbTotalSize); /* cbTotalSize (4 bytes) */

	return stream_skip(s, draw_gdiplus_cache_end->cbSize); /* emfRecords */
}

BOOL update_read_field_flags(wStream* s, UINT32* fieldFlags, BYTE flags, BYTE fieldBytes)
{
	int i;
	BYTE byte;

	if (flags & ORDER_ZERO_FIELD_BYTE_BIT0)
		fieldBytes--;

	if (flags & ORDER_ZERO_FIELD_BYTE_BIT1)
	{
		if (fieldBytes > 1)
			fieldBytes -= 2;
		else
			fieldBytes = 0;
	}

	if (stream_get_left(s) < fieldBytes)
		return FALSE;

	*fieldFlags = 0;
	for (i = 0; i < fieldBytes; i++)
	{
		stream_read_BYTE(s, byte);
		*fieldFlags |= byte << (i * 8);
	}
	return TRUE;
}

BOOL update_read_bounds(wStream* s, rdpBounds* bounds)
{
	BYTE flags;

	if (stream_get_left(s) < 1)
		return FALSE;
	stream_read_BYTE(s, flags); /* field flags */

	if (flags & BOUND_LEFT)
	{
		if (!update_read_coord(s, &bounds->left, FALSE))
			return FALSE;
	}
	else if (flags & BOUND_DELTA_LEFT)
	{
		if (!update_read_coord(s, &bounds->left, TRUE))
			return FALSE;
	}

	if (flags & BOUND_TOP)
	{
		if (!update_read_coord(s, &bounds->top, FALSE))
			return FALSE;
	}
	else if (flags & BOUND_DELTA_TOP)
	{
		if (!update_read_coord(s, &bounds->top, TRUE))
			return FALSE;
	}

	if (flags & BOUND_RIGHT)
	{
		if (!update_read_coord(s, &bounds->right, FALSE))
			return FALSE;
	}
	else if (flags & BOUND_DELTA_RIGHT)
	{
		if (!update_read_coord(s, &bounds->right, TRUE))
			return FALSE;
	}

	if (flags & BOUND_BOTTOM)
	{
		if (!update_read_coord(s, &bounds->bottom, FALSE))
			return FALSE;
	}
	else if (flags & BOUND_DELTA_BOTTOM)
	{
		if (!update_read_coord(s, &bounds->bottom, TRUE))
			return FALSE;
	}
	return TRUE;
}

BOOL update_recv_primary_order(rdpUpdate* update, wStream* s, BYTE flags)
{
	ORDER_INFO* orderInfo;
	rdpContext* context = update->context;
	rdpPrimaryUpdate* primary = update->primary;

	orderInfo = &(primary->order_info);

	if (flags & ORDER_TYPE_CHANGE)
		stream_read_BYTE(s, orderInfo->orderType); /* orderType (1 byte) */

	if (orderInfo->orderType >= PRIMARY_DRAWING_ORDER_COUNT)
	{
		fprintf(stderr, "Invalid Primary Drawing Order (0x%02X)\n", orderInfo->orderType);
		return FALSE;
	}

	if (!update_read_field_flags(s, &(orderInfo->fieldFlags), flags,
				PRIMARY_DRAWING_ORDER_FIELD_BYTES[orderInfo->orderType]))
		return FALSE;

	if (flags & ORDER_BOUNDS)
	{
		if (!(flags & ORDER_ZERO_BOUNDS_DELTAS))
		{
			if (!update_read_bounds(s, &orderInfo->bounds))
				return FALSE;
		}

		IFCALL(update->SetBounds, context, &orderInfo->bounds);
	}

	orderInfo->deltaCoordinates = (flags & ORDER_DELTA_COORDINATES) ? TRUE : FALSE;

#ifdef WITH_DEBUG_ORDERS
	fprintf(stderr, "%s Primary Drawing Order (0x%02X)\n", PRIMARY_DRAWING_ORDER_STRINGS[orderInfo->orderType], orderInfo->orderType);
#endif

	switch (orderInfo->orderType)
	{
		case ORDER_TYPE_DSTBLT:
			if (!update_read_dstblt_order(s, orderInfo, &(primary->dstblt)))
				return FALSE;
			IFCALL(primary->DstBlt, context, &primary->dstblt);
			break;

		case ORDER_TYPE_PATBLT:
			if (!update_read_patblt_order(s, orderInfo, &(primary->patblt)))
				return FALSE;
			IFCALL(primary->PatBlt, context, &primary->patblt);
			break;

		case ORDER_TYPE_SCRBLT:
			if (!update_read_scrblt_order(s, orderInfo, &(primary->scrblt)))
				return FALSE;
			IFCALL(primary->ScrBlt, context, &primary->scrblt);
			break;

		case ORDER_TYPE_OPAQUE_RECT:
			if (!update_read_opaque_rect_order(s, orderInfo, &(primary->opaque_rect)))
				return FALSE;
			IFCALL(primary->OpaqueRect, context, &primary->opaque_rect);
			break;

		case ORDER_TYPE_DRAW_NINE_GRID:
			if (!update_read_draw_nine_grid_order(s, orderInfo, &(primary->draw_nine_grid)))
				return FALSE;
			IFCALL(primary->DrawNineGrid, context, &primary->draw_nine_grid);
			break;

		case ORDER_TYPE_MULTI_DSTBLT:
			if (!update_read_multi_dstblt_order(s, orderInfo, &(primary->multi_dstblt)))
				return FALSE;
			IFCALL(primary->MultiDstBlt, context, &primary->multi_dstblt);
			break;

		case ORDER_TYPE_MULTI_PATBLT:
			if (!update_read_multi_patblt_order(s, orderInfo, &(primary->multi_patblt)))
				return FALSE;
			IFCALL(primary->MultiPatBlt, context, &primary->multi_patblt);
			break;

		case ORDER_TYPE_MULTI_SCRBLT:
			if (!update_read_multi_scrblt_order(s, orderInfo, &(primary->multi_scrblt)))
				return FALSE;
			IFCALL(primary->MultiScrBlt, context, &primary->multi_scrblt);
			break;

		case ORDER_TYPE_MULTI_OPAQUE_RECT:
			if (!update_read_multi_opaque_rect_order(s, orderInfo, &(primary->multi_opaque_rect)))
				return FALSE;
			IFCALL(primary->MultiOpaqueRect, context, &primary->multi_opaque_rect);
			break;

		case ORDER_TYPE_MULTI_DRAW_NINE_GRID:
			if (!update_read_multi_draw_nine_grid_order(s, orderInfo, &(primary->multi_draw_nine_grid)))
				return FALSE;
			IFCALL(primary->MultiDrawNineGrid, context, &primary->multi_draw_nine_grid);
			break;

		case ORDER_TYPE_LINE_TO:
			if (!update_read_line_to_order(s, orderInfo, &(primary->line_to)))
				return FALSE;
			IFCALL(primary->LineTo, context, &primary->line_to);
			break;

		case ORDER_TYPE_POLYLINE:
			if (!update_read_polyline_order(s, orderInfo, &(primary->polyline)))
				return FALSE;
			IFCALL(primary->Polyline, context, &primary->polyline);
			break;

		case ORDER_TYPE_MEMBLT:
			if (!update_read_memblt_order(s, orderInfo, &(primary->memblt)))
				return FALSE;
			IFCALL(primary->MemBlt, context, &primary->memblt);
			break;

		case ORDER_TYPE_MEM3BLT:
			if (!update_read_mem3blt_order(s, orderInfo, &(primary->mem3blt)))
				return FALSE;
			IFCALL(primary->Mem3Blt, context, &primary->mem3blt);
			break;

		case ORDER_TYPE_SAVE_BITMAP:
			if (!update_read_save_bitmap_order(s, orderInfo, &(primary->save_bitmap)))
				return FALSE;
			IFCALL(primary->SaveBitmap, context, &primary->save_bitmap);
			break;

		case ORDER_TYPE_GLYPH_INDEX:
			if (!update_read_glyph_index_order(s, orderInfo, &(primary->glyph_index)))
				return FALSE;
			IFCALL(primary->GlyphIndex, context, &primary->glyph_index);
			break;

		case ORDER_TYPE_FAST_INDEX:
			if (!update_read_fast_index_order(s, orderInfo, &(primary->fast_index)))
				return FALSE;
			IFCALL(primary->FastIndex, context, &primary->fast_index);
			break;

		case ORDER_TYPE_FAST_GLYPH:
			if (!update_read_fast_glyph_order(s, orderInfo, &(primary->fast_glyph)))
				return FALSE;
			IFCALL(primary->FastGlyph, context, &primary->fast_glyph);
			break;

		case ORDER_TYPE_POLYGON_SC:
			if (!update_read_polygon_sc_order(s, orderInfo, &(primary->polygon_sc)))
				return FALSE;
			IFCALL(primary->PolygonSC, context, &primary->polygon_sc);
			break;

		case ORDER_TYPE_POLYGON_CB:
			if (!update_read_polygon_cb_order(s, orderInfo, &(primary->polygon_cb)))
				return FALSE;
			IFCALL(primary->PolygonCB, context, &primary->polygon_cb);
			break;

		case ORDER_TYPE_ELLIPSE_SC:
			if (!update_read_ellipse_sc_order(s, orderInfo, &(primary->ellipse_sc)))
				return FALSE;
			IFCALL(primary->EllipseSC, context, &primary->ellipse_sc);
			break;

		case ORDER_TYPE_ELLIPSE_CB:
			if (!update_read_ellipse_cb_order(s, orderInfo, &(primary->ellipse_cb)))
				return FALSE;
			IFCALL(primary->EllipseCB, context, &primary->ellipse_cb);
			break;

		default:
			break;
	}

	if (flags & ORDER_BOUNDS)
	{
		IFCALL(update->SetBounds, context, NULL);
	}

	return TRUE;
}

BOOL update_recv_secondary_order(rdpUpdate* update, wStream* s, BYTE flags)
{
	BYTE* next;
	BYTE orderType;
	UINT16 extraFlags;
	UINT16 orderLength;
	rdpContext* context = update->context;
	rdpSecondaryUpdate* secondary = update->secondary;

	if (stream_get_left(s) < 5)
		return FALSE;
	stream_read_UINT16(s, orderLength); /* orderLength (2 bytes) */
	stream_read_UINT16(s, extraFlags); /* extraFlags (2 bytes) */
	stream_read_BYTE(s, orderType); /* orderType (1 byte) */

	next = s->pointer + ((INT16) orderLength) + 7;

#ifdef WITH_DEBUG_ORDERS
	if (orderType < SECONDARY_DRAWING_ORDER_COUNT)
		fprintf(stderr, "%s Secondary Drawing Order (0x%02X)\n", SECONDARY_DRAWING_ORDER_STRINGS[orderType], orderType);
	else
		fprintf(stderr, "Unknown Secondary Drawing Order (0x%02X)\n", orderType);
#endif

	switch (orderType)
	{
		case ORDER_TYPE_BITMAP_UNCOMPRESSED:
			if (!update_read_cache_bitmap_order(s, &(secondary->cache_bitmap_order), FALSE, extraFlags))
				return FALSE;
			IFCALL(secondary->CacheBitmap, context, &(secondary->cache_bitmap_order));
			break;

		case ORDER_TYPE_CACHE_BITMAP_COMPRESSED:
			if (!update_read_cache_bitmap_order(s, &(secondary->cache_bitmap_order), TRUE, extraFlags))
				return FALSE;
			IFCALL(secondary->CacheBitmap, context, &(secondary->cache_bitmap_order));
			break;

		case ORDER_TYPE_BITMAP_UNCOMPRESSED_V2:
			if (!update_read_cache_bitmap_v2_order(s, &(secondary->cache_bitmap_v2_order), FALSE, extraFlags))
				return FALSE;
			IFCALL(secondary->CacheBitmapV2, context, &(secondary->cache_bitmap_v2_order));
			break;

		case ORDER_TYPE_BITMAP_COMPRESSED_V2:
			if (!update_read_cache_bitmap_v2_order(s, &(secondary->cache_bitmap_v2_order), TRUE, extraFlags))
				return FALSE;
			IFCALL(secondary->CacheBitmapV2, context, &(secondary->cache_bitmap_v2_order));
			break;

		case ORDER_TYPE_BITMAP_COMPRESSED_V3:
			if (!update_read_cache_bitmap_v3_order(s, &(secondary->cache_bitmap_v3_order), TRUE, extraFlags))
				return FALSE;
			IFCALL(secondary->CacheBitmapV3, context, &(secondary->cache_bitmap_v3_order));
			break;

		case ORDER_TYPE_CACHE_COLOR_TABLE:
			if (!update_read_cache_color_table_order(s, &(secondary->cache_color_table_order), extraFlags))
				return FALSE;
			IFCALL(secondary->CacheColorTable, context, &(secondary->cache_color_table_order));
			break;

		case ORDER_TYPE_CACHE_GLYPH:
			if (secondary->glyph_v2)
			{
				if (!update_read_cache_glyph_v2_order(s, &(secondary->cache_glyph_v2_order), extraFlags))
					return FALSE;
				IFCALL(secondary->CacheGlyphV2, context, &(secondary->cache_glyph_v2_order));
			}
			else
			{
				if (!update_read_cache_glyph_order(s, &(secondary->cache_glyph_order), extraFlags))
					return FALSE;
				IFCALL(secondary->CacheGlyph, context, &(secondary->cache_glyph_order));
			}
			break;

		case ORDER_TYPE_CACHE_BRUSH:
			if (!update_read_cache_brush_order(s, &(secondary->cache_brush_order), extraFlags))
				return FALSE;
			IFCALL(secondary->CacheBrush, context, &(secondary->cache_brush_order));
			break;

		default:
			break;
	}

	s->pointer = next;
	return TRUE;
}

BOOL update_recv_altsec_order(rdpUpdate* update, wStream* s, BYTE flags)
{
	BYTE orderType;
	rdpContext* context = update->context;
	rdpAltSecUpdate* altsec = update->altsec;

	orderType = flags >>= 2; /* orderType is in higher 6 bits of flags field */

#ifdef WITH_DEBUG_ORDERS
	if (orderType < ALTSEC_DRAWING_ORDER_COUNT)
		fprintf(stderr, "%s Alternate Secondary Drawing Order (0x%02X)\n", ALTSEC_DRAWING_ORDER_STRINGS[orderType], orderType);
	else
		fprintf(stderr, "Unknown Alternate Secondary Drawing Order: 0x%02X\n", orderType);
#endif

	switch (orderType)
	{
		case ORDER_TYPE_CREATE_OFFSCREEN_BITMAP:
			if (!update_read_create_offscreen_bitmap_order(s, &(altsec->create_offscreen_bitmap)))
				return FALSE;
			IFCALL(altsec->CreateOffscreenBitmap, context, &(altsec->create_offscreen_bitmap));
			break;

		case ORDER_TYPE_SWITCH_SURFACE:
			if (!update_read_switch_surface_order(s, &(altsec->switch_surface)))
				return FALSE;
			IFCALL(altsec->SwitchSurface, context, &(altsec->switch_surface));
			break;

		case ORDER_TYPE_CREATE_NINE_GRID_BITMAP:
			if (!update_read_create_nine_grid_bitmap_order(s, &(altsec->create_nine_grid_bitmap)))
				return FALSE;
			IFCALL(altsec->CreateNineGridBitmap, context, &(altsec->create_nine_grid_bitmap));
			break;

		case ORDER_TYPE_FRAME_MARKER:
			if (!update_read_frame_marker_order(s, &(altsec->frame_marker)))
				return FALSE;
			IFCALL(altsec->FrameMarker, context, &(altsec->frame_marker));
			break;

		case ORDER_TYPE_STREAM_BITMAP_FIRST:
			if (!update_read_stream_bitmap_first_order(s, &(altsec->stream_bitmap_first)))
				return FALSE;
			IFCALL(altsec->StreamBitmapFirst, context, &(altsec->stream_bitmap_first));
			break;

		case ORDER_TYPE_STREAM_BITMAP_NEXT:
			if (!update_read_stream_bitmap_next_order(s, &(altsec->stream_bitmap_next)))
				return FALSE;
			IFCALL(altsec->StreamBitmapNext, context, &(altsec->stream_bitmap_next));
			break;

		case ORDER_TYPE_GDIPLUS_FIRST:
			if (!update_read_draw_gdiplus_first_order(s, &(altsec->draw_gdiplus_first)))
				return FALSE;
			IFCALL(altsec->DrawGdiPlusFirst, context, &(altsec->draw_gdiplus_first));
			break;

		case ORDER_TYPE_GDIPLUS_NEXT:
			if (!update_read_draw_gdiplus_next_order(s, &(altsec->draw_gdiplus_next)))
				return FALSE;
			IFCALL(altsec->DrawGdiPlusNext, context, &(altsec->draw_gdiplus_next));
			break;

		case ORDER_TYPE_GDIPLUS_END:
			if (update_read_draw_gdiplus_end_order(s, &(altsec->draw_gdiplus_end)))
				return FALSE;
			IFCALL(altsec->DrawGdiPlusEnd, context, &(altsec->draw_gdiplus_end));
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_FIRST:
			if (!update_read_draw_gdiplus_cache_first_order(s, &(altsec->draw_gdiplus_cache_first)))
				return FALSE;
			IFCALL(altsec->DrawGdiPlusCacheFirst, context, &(altsec->draw_gdiplus_cache_first));
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_NEXT:
			if (!update_read_draw_gdiplus_cache_next_order(s, &(altsec->draw_gdiplus_cache_next)))
				return FALSE;
			IFCALL(altsec->DrawGdiPlusCacheNext, context, &(altsec->draw_gdiplus_cache_next));
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_END:
			if (!update_read_draw_gdiplus_cache_end_order(s, &(altsec->draw_gdiplus_cache_end)))
				return FALSE;
			IFCALL(altsec->DrawGdiPlusCacheEnd, context, &(altsec->draw_gdiplus_cache_end));
			break;

		case ORDER_TYPE_WINDOW:
			return update_recv_altsec_window_order(update, s);
			break;

		case ORDER_TYPE_COMPDESK_FIRST:
			break;

		default:
			break;
	}
	return TRUE;
}

BOOL update_recv_order(rdpUpdate* update, wStream* s)
{
	BYTE controlFlags;

	if (stream_get_left(s) < 1)
		return FALSE;

	stream_read_BYTE(s, controlFlags); /* controlFlags (1 byte) */

	if (!(controlFlags & ORDER_STANDARD))
		return update_recv_altsec_order(update, s, controlFlags);
	else if (controlFlags & ORDER_SECONDARY)
		return update_recv_secondary_order(update, s, controlFlags);
	else
		return update_recv_primary_order(update, s, controlFlags);

	return TRUE;
}
