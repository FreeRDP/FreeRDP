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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "window.h"

#include <winpr/wtypes.h>
#include <winpr/crt.h>

#include <freerdp/api.h>
#include <freerdp/log.h>
#include <freerdp/graphics.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/gdi/gdi.h>

#include "orders.h"

#include "../cache/glyph.h"
#include "../cache/bitmap.h"
#include "../cache/brush.h"
#include "../cache/cache.h"

#define TAG FREERDP_TAG("core.orders")

const BYTE PRIMARY_DRAWING_ORDER_FIELD_BYTES[] = { DSTBLT_ORDER_FIELD_BYTES,
	                                               PATBLT_ORDER_FIELD_BYTES,
	                                               SCRBLT_ORDER_FIELD_BYTES,
	                                               0,
	                                               0,
	                                               0,
	                                               0,
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
	                                               GLYPH_INDEX_ORDER_FIELD_BYTES };

#define PRIMARY_DRAWING_ORDER_COUNT (ARRAYSIZE(PRIMARY_DRAWING_ORDER_FIELD_BYTES))

static const BYTE CBR2_BPP[] = { 0, 0, 0, 8, 16, 24, 32 };

static const BYTE BPP_CBR2[] = { 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0,
	                             0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0 };

static const BYTE CBR23_BPP[] = { 0, 0, 0, 8, 16, 24, 32 };

static const BYTE BPP_CBR23[] = { 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0,
	                              0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0 };

static const BYTE BMF_BPP[] = { 0, 1, 0, 8, 16, 24, 32, 0 };

static const BYTE BPP_BMF[] = { 0, 1, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0,
	                            0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0 };

static BOOL check_order_activated(wLog* log, rdpSettings* settings, const char* orderName,
                                  BOOL condition)
{
	if (!condition)
	{
		if (settings->AllowUnanouncedOrdersFromServer)
		{
			WLog_Print(log, WLOG_WARN,
			           "%s - SERVER BUG: The support for this feature was not announced!",
			           orderName);
			return TRUE;
		}
		else
		{
			WLog_Print(log, WLOG_ERROR,
			           "%s - SERVER BUG: The support for this feature was not announced! Use "
			           "/relax-order-checks to ignore",
			           orderName);
			return FALSE;
		}
	}

	return TRUE;
}

static BOOL check_alt_order_supported(wLog* log, rdpSettings* settings, BYTE orderType,
                                      const char* orderName)
{
	BOOL condition = FALSE;

	switch (orderType)
	{
		case ORDER_TYPE_CREATE_OFFSCREEN_BITMAP:
		case ORDER_TYPE_SWITCH_SURFACE:
			condition = settings->OffscreenSupportLevel != 0;
			break;

		case ORDER_TYPE_CREATE_NINE_GRID_BITMAP:
			condition = settings->DrawNineGridEnabled;
			break;

		case ORDER_TYPE_FRAME_MARKER:
			condition = settings->FrameMarkerCommandEnabled;
			break;

		case ORDER_TYPE_GDIPLUS_FIRST:
		case ORDER_TYPE_GDIPLUS_NEXT:
		case ORDER_TYPE_GDIPLUS_END:
		case ORDER_TYPE_GDIPLUS_CACHE_FIRST:
		case ORDER_TYPE_GDIPLUS_CACHE_NEXT:
		case ORDER_TYPE_GDIPLUS_CACHE_END:
			condition = settings->DrawGdiPlusCacheEnabled;
			break;

		case ORDER_TYPE_WINDOW:
			condition = settings->RemoteWndSupportLevel != WINDOW_LEVEL_NOT_SUPPORTED;
			break;

		case ORDER_TYPE_STREAM_BITMAP_FIRST:
		case ORDER_TYPE_STREAM_BITMAP_NEXT:
		case ORDER_TYPE_COMPDESK_FIRST:
			condition = TRUE;
			break;

		default:
			WLog_Print(log, WLOG_WARN, "%s - Alternate Secondary Drawing Order UNKNOWN", orderName);
			condition = FALSE;
			break;
	}

	return check_order_activated(log, settings, orderName, condition);
}

static BOOL check_secondary_order_supported(wLog* log, rdpSettings* settings, BYTE orderType,
                                            const char* orderName)
{
	BOOL condition = FALSE;

	switch (orderType)
	{
		case ORDER_TYPE_BITMAP_UNCOMPRESSED:
		case ORDER_TYPE_CACHE_BITMAP_COMPRESSED:
			condition = settings->BitmapCacheEnabled;
			break;

		case ORDER_TYPE_BITMAP_UNCOMPRESSED_V2:
		case ORDER_TYPE_BITMAP_COMPRESSED_V2:
			condition = settings->BitmapCacheEnabled;
			break;

		case ORDER_TYPE_BITMAP_COMPRESSED_V3:
			condition = settings->BitmapCacheV3Enabled;
			break;

		case ORDER_TYPE_CACHE_COLOR_TABLE:
			condition = (settings->OrderSupport[NEG_MEMBLT_INDEX] ||
			             settings->OrderSupport[NEG_MEM3BLT_INDEX]);
			break;

		case ORDER_TYPE_CACHE_GLYPH:
		{
			switch (settings->GlyphSupportLevel)
			{
				case GLYPH_SUPPORT_PARTIAL:
				case GLYPH_SUPPORT_FULL:
				case GLYPH_SUPPORT_ENCODE:
					condition = TRUE;
					break;

				case GLYPH_SUPPORT_NONE:
				default:
					condition = FALSE;
					break;
			}
		}
		break;

		case ORDER_TYPE_CACHE_BRUSH:
			condition = TRUE;
			break;

		default:
			WLog_Print(log, WLOG_WARN, "SECONDARY ORDER %s not supported", orderName);
			break;
	}

	return check_order_activated(log, settings, orderName, condition);
}

static BOOL check_primary_order_supported(wLog* log, rdpSettings* settings, UINT32 orderType,
                                          const char* orderName)
{
	BOOL condition = FALSE;

	switch (orderType)
	{
		case ORDER_TYPE_DSTBLT:
			condition = settings->OrderSupport[NEG_DSTBLT_INDEX];
			break;

		case ORDER_TYPE_SCRBLT:
			condition = settings->OrderSupport[NEG_SCRBLT_INDEX];
			break;

		case ORDER_TYPE_DRAW_NINE_GRID:
			condition = settings->OrderSupport[NEG_DRAWNINEGRID_INDEX];
			break;

		case ORDER_TYPE_MULTI_DRAW_NINE_GRID:
			condition = settings->OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX];
			break;

		case ORDER_TYPE_LINE_TO:
			condition = settings->OrderSupport[NEG_LINETO_INDEX];
			break;

		/* [MS-RDPEGDI] 2.2.2.2.1.1.2.5 OpaqueRect (OPAQUERECT_ORDER)
		 * suggests that PatBlt and OpaqueRect imply each other. */
		case ORDER_TYPE_PATBLT:
		case ORDER_TYPE_OPAQUE_RECT:
			condition = settings->OrderSupport[NEG_OPAQUE_RECT_INDEX] ||
			            settings->OrderSupport[NEG_PATBLT_INDEX];
			break;

		case ORDER_TYPE_SAVE_BITMAP:
			condition = settings->OrderSupport[NEG_SAVEBITMAP_INDEX];
			break;

		case ORDER_TYPE_MEMBLT:
			condition = settings->OrderSupport[NEG_MEMBLT_INDEX];
			break;

		case ORDER_TYPE_MEM3BLT:
			condition = settings->OrderSupport[NEG_MEM3BLT_INDEX];
			break;

		case ORDER_TYPE_MULTI_DSTBLT:
			condition = settings->OrderSupport[NEG_MULTIDSTBLT_INDEX];
			break;

		case ORDER_TYPE_MULTI_PATBLT:
			condition = settings->OrderSupport[NEG_MULTIPATBLT_INDEX];
			break;

		case ORDER_TYPE_MULTI_SCRBLT:
			condition = settings->OrderSupport[NEG_MULTIDSTBLT_INDEX];
			break;

		case ORDER_TYPE_MULTI_OPAQUE_RECT:
			condition = settings->OrderSupport[NEG_MULTIOPAQUERECT_INDEX];
			break;

		case ORDER_TYPE_FAST_INDEX:
			condition = settings->OrderSupport[NEG_FAST_INDEX_INDEX];
			break;

		case ORDER_TYPE_POLYGON_SC:
			condition = settings->OrderSupport[NEG_POLYGON_SC_INDEX];
			break;

		case ORDER_TYPE_POLYGON_CB:
			condition = settings->OrderSupport[NEG_POLYGON_CB_INDEX];
			break;

		case ORDER_TYPE_POLYLINE:
			condition = settings->OrderSupport[NEG_POLYLINE_INDEX];
			break;

		case ORDER_TYPE_FAST_GLYPH:
			condition = settings->OrderSupport[NEG_FAST_GLYPH_INDEX];
			break;

		case ORDER_TYPE_ELLIPSE_SC:
			condition = settings->OrderSupport[NEG_ELLIPSE_SC_INDEX];
			break;

		case ORDER_TYPE_ELLIPSE_CB:
			condition = settings->OrderSupport[NEG_ELLIPSE_CB_INDEX];
			break;

		case ORDER_TYPE_GLYPH_INDEX:
			condition = settings->OrderSupport[NEG_GLYPH_INDEX_INDEX];
			break;

		default:
			WLog_Print(log, WLOG_WARN, "%s Primary Drawing Order not supported", orderName);
			break;
	}

	return check_order_activated(log, settings, orderName, condition);
}

static const char* primary_order_string(UINT32 orderType)
{
	const char* orders[] = { "[0x%02" PRIx8 "] DstBlt",
		                     "[0x%02" PRIx8 "] PatBlt",
		                     "[0x%02" PRIx8 "] ScrBlt",
		                     "[0x%02" PRIx8 "] UNUSED",
		                     "[0x%02" PRIx8 "] UNUSED",
		                     "[0x%02" PRIx8 "] UNUSED",
		                     "[0x%02" PRIx8 "] UNUSED",
		                     "[0x%02" PRIx8 "] DrawNineGrid",
		                     "[0x%02" PRIx8 "] MultiDrawNineGrid",
		                     "[0x%02" PRIx8 "] LineTo",
		                     "[0x%02" PRIx8 "] OpaqueRect",
		                     "[0x%02" PRIx8 "] SaveBitmap",
		                     "[0x%02" PRIx8 "] UNUSED",
		                     "[0x%02" PRIx8 "] MemBlt",
		                     "[0x%02" PRIx8 "] Mem3Blt",
		                     "[0x%02" PRIx8 "] MultiDstBlt",
		                     "[0x%02" PRIx8 "] MultiPatBlt",
		                     "[0x%02" PRIx8 "] MultiScrBlt",
		                     "[0x%02" PRIx8 "] MultiOpaqueRect",
		                     "[0x%02" PRIx8 "] FastIndex",
		                     "[0x%02" PRIx8 "] PolygonSC",
		                     "[0x%02" PRIx8 "] PolygonCB",
		                     "[0x%02" PRIx8 "] Polyline",
		                     "[0x%02" PRIx8 "] UNUSED",
		                     "[0x%02" PRIx8 "] FastGlyph",
		                     "[0x%02" PRIx8 "] EllipseSC",
		                     "[0x%02" PRIx8 "] EllipseCB",
		                     "[0x%02" PRIx8 "] GlyphIndex" };
	const char* fmt = "[0x%02" PRIx8 "] UNKNOWN";
	static char buffer[64] = { 0 };

	if (orderType < ARRAYSIZE(orders))
		fmt = orders[orderType];

	sprintf_s(buffer, ARRAYSIZE(buffer), fmt, orderType);
	return buffer;
}
static const char* secondary_order_string(UINT32 orderType)
{
	const char* orders[] = { "[0x%02" PRIx8 "] Cache Bitmap",
		                     "[0x%02" PRIx8 "] Cache Color Table",
		                     "[0x%02" PRIx8 "] Cache Bitmap (Compressed)",
		                     "[0x%02" PRIx8 "] Cache Glyph",
		                     "[0x%02" PRIx8 "] Cache Bitmap V2",
		                     "[0x%02" PRIx8 "] Cache Bitmap V2 (Compressed)",
		                     "[0x%02" PRIx8 "] UNUSED",
		                     "[0x%02" PRIx8 "] Cache Brush",
		                     "[0x%02" PRIx8 "] Cache Bitmap V3" };
	const char* fmt = "[0x%02" PRIx8 "] UNKNOWN";
	static char buffer[64] = { 0 };

	if (orderType < ARRAYSIZE(orders))
		fmt = orders[orderType];

	sprintf_s(buffer, ARRAYSIZE(buffer), fmt, orderType);
	return buffer;
}
static const char* altsec_order_string(BYTE orderType)
{
	const char* orders[] = {
		"[0x%02" PRIx8 "] Switch Surface",         "[0x%02" PRIx8 "] Create Offscreen Bitmap",
		"[0x%02" PRIx8 "] Stream Bitmap First",    "[0x%02" PRIx8 "] Stream Bitmap Next",
		"[0x%02" PRIx8 "] Create NineGrid Bitmap", "[0x%02" PRIx8 "] Draw GDI+ First",
		"[0x%02" PRIx8 "] Draw GDI+ Next",         "[0x%02" PRIx8 "] Draw GDI+ End",
		"[0x%02" PRIx8 "] Draw GDI+ Cache First",  "[0x%02" PRIx8 "] Draw GDI+ Cache Next",
		"[0x%02" PRIx8 "] Draw GDI+ Cache End",    "[0x%02" PRIx8 "] Windowing",
		"[0x%02" PRIx8 "] Desktop Composition",    "[0x%02" PRIx8 "] Frame Marker"
	};
	const char* fmt = "[0x%02" PRIx8 "] UNKNOWN";
	static char buffer[64] = { 0 };

	if (orderType < ARRAYSIZE(orders))
		fmt = orders[orderType];

	sprintf_s(buffer, ARRAYSIZE(buffer), fmt, orderType);
	return buffer;
}

static INLINE BOOL update_read_coord(wStream* s, INT32* coord, BOOL delta)
{
	INT8 lsi8;
	INT16 lsi16;

	if (delta)
	{
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_INT8(s, lsi8);
		*coord += lsi8;
	}
	else
	{
		if (Stream_GetRemainingLength(s) < 2)
			return FALSE;

		Stream_Read_INT16(s, lsi16);
		*coord = lsi16;
	}

	return TRUE;
}
static INLINE BOOL update_write_coord(wStream* s, INT32 coord)
{
	Stream_Write_UINT16(s, coord);
	return TRUE;
}
static INLINE BOOL update_read_color(wStream* s, UINT32* color)
{
	BYTE byte;

	if (Stream_GetRemainingLength(s) < 3)
		return FALSE;

	*color = 0;
	Stream_Read_UINT8(s, byte);
	*color = (UINT32)byte;
	Stream_Read_UINT8(s, byte);
	*color |= ((UINT32)byte << 8) & 0xFF00;
	Stream_Read_UINT8(s, byte);
	*color |= ((UINT32)byte << 16) & 0xFF0000;
	return TRUE;
}
static INLINE BOOL update_write_color(wStream* s, UINT32 color)
{
	BYTE byte;
	byte = (color & 0xFF);
	Stream_Write_UINT8(s, byte);
	byte = ((color >> 8) & 0xFF);
	Stream_Write_UINT8(s, byte);
	byte = ((color >> 16) & 0xFF);
	Stream_Write_UINT8(s, byte);
	return TRUE;
}
static INLINE BOOL update_read_colorref(wStream* s, UINT32* color)
{
	BYTE byte;

	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	*color = 0;
	Stream_Read_UINT8(s, byte);
	*color = byte;
	Stream_Read_UINT8(s, byte);
	*color |= ((UINT32)byte << 8);
	Stream_Read_UINT8(s, byte);
	*color |= ((UINT32)byte << 16);
	Stream_Seek_UINT8(s);
	return TRUE;
}
static INLINE BOOL update_read_color_quad(wStream* s, UINT32* color)
{
	return update_read_colorref(s, color);
}
static INLINE void update_write_color_quad(wStream* s, UINT32 color)
{
	BYTE byte;
	byte = (color >> 16) & 0xFF;
	Stream_Write_UINT8(s, byte);
	byte = (color >> 8) & 0xFF;
	Stream_Write_UINT8(s, byte);
	byte = color & 0xFF;
	Stream_Write_UINT8(s, byte);
}
static INLINE BOOL update_read_2byte_unsigned(wStream* s, UINT32* value)
{
	BYTE byte;

	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, byte);

	if (byte & 0x80)
	{
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		*value = (byte & 0x7F) << 8;
		Stream_Read_UINT8(s, byte);
		*value |= byte;
	}
	else
	{
		*value = (byte & 0x7F);
	}

	return TRUE;
}
static INLINE BOOL update_write_2byte_unsigned(wStream* s, UINT32 value)
{
	BYTE byte;

	if (value > 0x7FFF)
		return FALSE;

	if (value >= 0x7F)
	{
		byte = ((value & 0x7F00) >> 8);
		Stream_Write_UINT8(s, byte | 0x80);
		byte = (value & 0xFF);
		Stream_Write_UINT8(s, byte);
	}
	else
	{
		byte = (value & 0x7F);
		Stream_Write_UINT8(s, byte);
	}

	return TRUE;
}
static INLINE BOOL update_read_2byte_signed(wStream* s, INT32* value)
{
	BYTE byte;
	BOOL negative;

	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, byte);
	negative = (byte & 0x40) ? TRUE : FALSE;
	*value = (byte & 0x3F);

	if (byte & 0x80)
	{
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, byte);
		*value = (*value << 8) | byte;
	}

	if (negative)
		*value *= -1;

	return TRUE;
}
static INLINE BOOL update_write_2byte_signed(wStream* s, INT32 value)
{
	BYTE byte;
	BOOL negative = FALSE;

	if (value < 0)
	{
		negative = TRUE;
		value *= -1;
	}

	if (value > 0x3FFF)
		return FALSE;

	if (value >= 0x3F)
	{
		byte = ((value & 0x3F00) >> 8);

		if (negative)
			byte |= 0x40;

		Stream_Write_UINT8(s, byte | 0x80);
		byte = (value & 0xFF);
		Stream_Write_UINT8(s, byte);
	}
	else
	{
		byte = (value & 0x3F);

		if (negative)
			byte |= 0x40;

		Stream_Write_UINT8(s, byte);
	}

	return TRUE;
}
static INLINE BOOL update_read_4byte_unsigned(wStream* s, UINT32* value)
{
	BYTE byte;
	BYTE count;

	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, byte);
	count = (byte & 0xC0) >> 6;

	if (Stream_GetRemainingLength(s) < count)
		return FALSE;

	switch (count)
	{
		case 0:
			*value = (byte & 0x3F);
			break;

		case 1:
			*value = (byte & 0x3F) << 8;
			Stream_Read_UINT8(s, byte);
			*value |= byte;
			break;

		case 2:
			*value = (byte & 0x3F) << 16;
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 8);
			Stream_Read_UINT8(s, byte);
			*value |= byte;
			break;

		case 3:
			*value = (byte & 0x3F) << 24;
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 16);
			Stream_Read_UINT8(s, byte);
			*value |= (byte << 8);
			Stream_Read_UINT8(s, byte);
			*value |= byte;
			break;

		default:
			break;
	}

	return TRUE;
}
static INLINE BOOL update_write_4byte_unsigned(wStream* s, UINT32 value)
{
	BYTE byte;

	if (value <= 0x3F)
	{
		Stream_Write_UINT8(s, value);
	}
	else if (value <= 0x3FFF)
	{
		byte = (value >> 8) & 0x3F;
		Stream_Write_UINT8(s, byte | 0x40);
		byte = (value & 0xFF);
		Stream_Write_UINT8(s, byte);
	}
	else if (value <= 0x3FFFFF)
	{
		byte = (value >> 16) & 0x3F;
		Stream_Write_UINT8(s, byte | 0x80);
		byte = (value >> 8) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (value & 0xFF);
		Stream_Write_UINT8(s, byte);
	}
	else if (value <= 0x3FFFFFFF)
	{
		byte = (value >> 24) & 0x3F;
		Stream_Write_UINT8(s, byte | 0xC0);
		byte = (value >> 16) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (value >> 8) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (value & 0xFF);
		Stream_Write_UINT8(s, byte);
	}
	else
		return FALSE;

	return TRUE;
}
static INLINE BOOL update_read_delta(wStream* s, INT32* value)
{
	BYTE byte;

	if (Stream_GetRemainingLength(s) < 1)
	{
		WLog_ERR(TAG, "Stream_GetRemainingLength(s) < 1");
		return FALSE;
	}

	Stream_Read_UINT8(s, byte);

	if (byte & 0x40)
		*value = (byte | ~0x3F);
	else
		*value = (byte & 0x3F);

	if (byte & 0x80)
	{
		if (Stream_GetRemainingLength(s) < 1)
		{
			WLog_ERR(TAG, "Stream_GetRemainingLength(s) < 1");
			return FALSE;
		}

		Stream_Read_UINT8(s, byte);
		*value = (*value << 8) | byte;
	}

	return TRUE;
}
#if 0
static INLINE void update_read_glyph_delta(wStream* s, UINT16* value)
{
	BYTE byte;
	Stream_Read_UINT8(s, byte);

	if (byte == 0x80)
		Stream_Read_UINT16(s, *value);
	else
		*value = (byte & 0x3F);
}
static INLINE void update_seek_glyph_delta(wStream* s)
{
	BYTE byte;
	Stream_Read_UINT8(s, byte);

	if (byte & 0x80)
		Stream_Seek_UINT8(s);
}
#endif
static INLINE BOOL update_read_brush(wStream* s, rdpBrush* brush, BYTE fieldFlags)
{
	if (fieldFlags & ORDER_FIELD_01)
	{
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, brush->x);
	}

	if (fieldFlags & ORDER_FIELD_02)
	{
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, brush->y);
	}

	if (fieldFlags & ORDER_FIELD_03)
	{
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, brush->style);
	}

	if (fieldFlags & ORDER_FIELD_04)
	{
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, brush->hatch);
	}

	if (brush->style & CACHED_BRUSH)
	{
		brush->index = brush->hatch;
		brush->bpp = BMF_BPP[brush->style & 0x07];

		if (brush->bpp == 0)
			brush->bpp = 1;
	}

	if (fieldFlags & ORDER_FIELD_05)
	{
		if (Stream_GetRemainingLength(s) < 7)
			return FALSE;

		brush->data = (BYTE*)brush->p8x8;
		Stream_Read_UINT8(s, brush->data[7]);
		Stream_Read_UINT8(s, brush->data[6]);
		Stream_Read_UINT8(s, brush->data[5]);
		Stream_Read_UINT8(s, brush->data[4]);
		Stream_Read_UINT8(s, brush->data[3]);
		Stream_Read_UINT8(s, brush->data[2]);
		Stream_Read_UINT8(s, brush->data[1]);
		brush->data[0] = brush->hatch;
	}

	return TRUE;
}
static INLINE BOOL update_write_brush(wStream* s, rdpBrush* brush, BYTE fieldFlags)
{
	if (fieldFlags & ORDER_FIELD_01)
	{
		Stream_Write_UINT8(s, brush->x);
	}

	if (fieldFlags & ORDER_FIELD_02)
	{
		Stream_Write_UINT8(s, brush->y);
	}

	if (fieldFlags & ORDER_FIELD_03)
	{
		Stream_Write_UINT8(s, brush->style);
	}

	if (brush->style & CACHED_BRUSH)
	{
		brush->hatch = brush->index;
		brush->bpp = BMF_BPP[brush->style & 0x07];

		if (brush->bpp == 0)
			brush->bpp = 1;
	}

	if (fieldFlags & ORDER_FIELD_04)
	{
		Stream_Write_UINT8(s, brush->hatch);
	}

	if (fieldFlags & ORDER_FIELD_05)
	{
		brush->data = (BYTE*)brush->p8x8;
		Stream_Write_UINT8(s, brush->data[7]);
		Stream_Write_UINT8(s, brush->data[6]);
		Stream_Write_UINT8(s, brush->data[5]);
		Stream_Write_UINT8(s, brush->data[4]);
		Stream_Write_UINT8(s, brush->data[3]);
		Stream_Write_UINT8(s, brush->data[2]);
		Stream_Write_UINT8(s, brush->data[1]);
		brush->data[0] = brush->hatch;
	}

	return TRUE;
}
static INLINE BOOL update_read_delta_rects(wStream* s, DELTA_RECT* rectangles, UINT32 number)
{
	UINT32 i;
	BYTE flags = 0;
	BYTE* zeroBits;
	UINT32 zeroBitsSize;

	if (number > 45)
		number = 45;

	zeroBitsSize = ((number + 1) / 2);

	if (Stream_GetRemainingLength(s) < zeroBitsSize)
		return FALSE;

	Stream_GetPointer(s, zeroBits);
	Stream_Seek(s, zeroBitsSize);
	ZeroMemory(rectangles, sizeof(DELTA_RECT) * number);

	for (i = 0; i < number; i++)
	{
		if (i % 2 == 0)
			flags = zeroBits[i / 2];

		if ((~flags & 0x80) && !update_read_delta(s, &rectangles[i].left))
			return FALSE;

		if ((~flags & 0x40) && !update_read_delta(s, &rectangles[i].top))
			return FALSE;

		if (~flags & 0x20)
		{
			if (!update_read_delta(s, &rectangles[i].width))
				return FALSE;
		}
		else if (i > 0)
			rectangles[i].width = rectangles[i - 1].width;
		else
			rectangles[i].width = 0;

		if (~flags & 0x10)
		{
			if (!update_read_delta(s, &rectangles[i].height))
				return FALSE;
		}
		else if (i > 0)
			rectangles[i].height = rectangles[i - 1].height;
		else
			rectangles[i].height = 0;

		if (i > 0)
		{
			rectangles[i].left += rectangles[i - 1].left;
			rectangles[i].top += rectangles[i - 1].top;
		}

		flags <<= 4;
	}

	return TRUE;
}
static INLINE BOOL update_read_delta_points(wStream* s, DELTA_POINT* points, int number, INT16 x,
                                            INT16 y)
{
	int i;
	BYTE flags = 0;
	BYTE* zeroBits;
	UINT32 zeroBitsSize;
	zeroBitsSize = ((number + 3) / 4);

	if (Stream_GetRemainingLength(s) < zeroBitsSize)
	{
		WLog_ERR(TAG, "Stream_GetRemainingLength(s) < %" PRIu32 "", zeroBitsSize);
		return FALSE;
	}

	Stream_GetPointer(s, zeroBits);
	Stream_Seek(s, zeroBitsSize);
	ZeroMemory(points, sizeof(DELTA_POINT) * number);

	for (i = 0; i < number; i++)
	{
		if (i % 4 == 0)
			flags = zeroBits[i / 4];

		if ((~flags & 0x80) && !update_read_delta(s, &points[i].x))
		{
			WLog_ERR(TAG, "update_read_delta(x) failed");
			return FALSE;
		}

		if ((~flags & 0x40) && !update_read_delta(s, &points[i].y))
		{
			WLog_ERR(TAG, "update_read_delta(y) failed");
			return FALSE;
		}

		flags <<= 2;
	}

	return TRUE;
}
#define ORDER_FIELD_BYTE(NO, TARGET)                        \
	do                                                      \
	{                                                       \
		if (orderInfo->fieldFlags & (1 << (NO - 1)))        \
		{                                                   \
			if (Stream_GetRemainingLength(s) < 1)           \
			{                                               \
				WLog_ERR(TAG, "error reading %s", #TARGET); \
				return FALSE;                               \
			}                                               \
			Stream_Read_UINT8(s, TARGET);                   \
		}                                                   \
	} while (0)
#define ORDER_FIELD_2BYTE(NO, TARGET1, TARGET2)                              \
	do                                                                       \
	{                                                                        \
		if (orderInfo->fieldFlags & (1 << (NO - 1)))                         \
		{                                                                    \
			if (Stream_GetRemainingLength(s) < 2)                            \
			{                                                                \
				WLog_ERR(TAG, "error reading %s or %s", #TARGET1, #TARGET2); \
				return FALSE;                                                \
			}                                                                \
			Stream_Read_UINT8(s, TARGET1);                                   \
			Stream_Read_UINT8(s, TARGET2);                                   \
		}                                                                    \
	} while (0)
#define ORDER_FIELD_UINT16(NO, TARGET)                      \
	do                                                      \
	{                                                       \
		if (orderInfo->fieldFlags & (1 << (NO - 1)))        \
		{                                                   \
			if (Stream_GetRemainingLength(s) < 2)           \
			{                                               \
				WLog_ERR(TAG, "error reading %s", #TARGET); \
				return FALSE;                               \
			}                                               \
			Stream_Read_UINT16(s, TARGET);                  \
		}                                                   \
	} while (0)
#define ORDER_FIELD_UINT32(NO, TARGET)                      \
	do                                                      \
	{                                                       \
		if (orderInfo->fieldFlags & (1 << (NO - 1)))        \
		{                                                   \
			if (Stream_GetRemainingLength(s) < 4)           \
			{                                               \
				WLog_ERR(TAG, "error reading %s", #TARGET); \
				return FALSE;                               \
			}                                               \
			Stream_Read_UINT32(s, TARGET);                  \
		}                                                   \
	} while (0)
#define ORDER_FIELD_COORD(NO, TARGET)                                    \
	do                                                                   \
	{                                                                    \
		if ((orderInfo->fieldFlags & (1 << (NO - 1))) &&                 \
		    !update_read_coord(s, &TARGET, orderInfo->deltaCoordinates)) \
		{                                                                \
			WLog_ERR(TAG, "error reading %s", #TARGET);                  \
			return FALSE;                                                \
		}                                                                \
	} while (0)
static INLINE BOOL ORDER_FIELD_COLOR(const ORDER_INFO* orderInfo, wStream* s, UINT32 NO,
                                     UINT32* TARGET)
{
	if (!TARGET || !orderInfo)
		return FALSE;

	if ((orderInfo->fieldFlags & (1 << (NO - 1))) && !update_read_color(s, TARGET))
		return FALSE;

	return TRUE;
}
static INLINE BOOL FIELD_SKIP_BUFFER16(wStream* s, UINT32 TARGET_LEN)
{
	if (Stream_GetRemainingLength(s) < 2)
		return FALSE;

	Stream_Read_UINT16(s, TARGET_LEN);

	if (!Stream_SafeSeek(s, TARGET_LEN))
	{
		WLog_ERR(TAG, "error skipping %" PRIu32 " bytes", TARGET_LEN);
		return FALSE;
	}

	return TRUE;
}
/* Primary Drawing Orders */
static BOOL update_read_dstblt_order(wStream* s, const ORDER_INFO* orderInfo, DSTBLT_ORDER* dstblt)
{
	ORDER_FIELD_COORD(1, dstblt->nLeftRect);
	ORDER_FIELD_COORD(2, dstblt->nTopRect);
	ORDER_FIELD_COORD(3, dstblt->nWidth);
	ORDER_FIELD_COORD(4, dstblt->nHeight);
	ORDER_FIELD_BYTE(5, dstblt->bRop);
	return TRUE;
}
int update_approximate_dstblt_order(ORDER_INFO* orderInfo, const DSTBLT_ORDER* dstblt)
{
	return 32;
}
BOOL update_write_dstblt_order(wStream* s, ORDER_INFO* orderInfo, const DSTBLT_ORDER* dstblt)
{
	if (!Stream_EnsureRemainingCapacity(s, update_approximate_dstblt_order(orderInfo, dstblt)))
		return FALSE;

	orderInfo->fieldFlags = 0;
	orderInfo->fieldFlags |= ORDER_FIELD_01;
	update_write_coord(s, dstblt->nLeftRect);
	orderInfo->fieldFlags |= ORDER_FIELD_02;
	update_write_coord(s, dstblt->nTopRect);
	orderInfo->fieldFlags |= ORDER_FIELD_03;
	update_write_coord(s, dstblt->nWidth);
	orderInfo->fieldFlags |= ORDER_FIELD_04;
	update_write_coord(s, dstblt->nHeight);
	orderInfo->fieldFlags |= ORDER_FIELD_05;
	Stream_Write_UINT8(s, dstblt->bRop);
	return TRUE;
}
static BOOL update_read_patblt_order(wStream* s, const ORDER_INFO* orderInfo, PATBLT_ORDER* patblt)
{
	ORDER_FIELD_COORD(1, patblt->nLeftRect);
	ORDER_FIELD_COORD(2, patblt->nTopRect);
	ORDER_FIELD_COORD(3, patblt->nWidth);
	ORDER_FIELD_COORD(4, patblt->nHeight);
	ORDER_FIELD_BYTE(5, patblt->bRop);
	ORDER_FIELD_COLOR(orderInfo, s, 6, &patblt->backColor);
	ORDER_FIELD_COLOR(orderInfo, s, 7, &patblt->foreColor);
	return update_read_brush(s, &patblt->brush, orderInfo->fieldFlags >> 7);
}
int update_approximate_patblt_order(ORDER_INFO* orderInfo, PATBLT_ORDER* patblt)
{
	return 32;
}
BOOL update_write_patblt_order(wStream* s, ORDER_INFO* orderInfo, PATBLT_ORDER* patblt)
{
	if (!Stream_EnsureRemainingCapacity(s, update_approximate_patblt_order(orderInfo, patblt)))
		return FALSE;

	orderInfo->fieldFlags = 0;
	orderInfo->fieldFlags |= ORDER_FIELD_01;
	update_write_coord(s, patblt->nLeftRect);
	orderInfo->fieldFlags |= ORDER_FIELD_02;
	update_write_coord(s, patblt->nTopRect);
	orderInfo->fieldFlags |= ORDER_FIELD_03;
	update_write_coord(s, patblt->nWidth);
	orderInfo->fieldFlags |= ORDER_FIELD_04;
	update_write_coord(s, patblt->nHeight);
	orderInfo->fieldFlags |= ORDER_FIELD_05;
	Stream_Write_UINT8(s, patblt->bRop);
	orderInfo->fieldFlags |= ORDER_FIELD_06;
	update_write_color(s, patblt->backColor);
	orderInfo->fieldFlags |= ORDER_FIELD_07;
	update_write_color(s, patblt->foreColor);
	orderInfo->fieldFlags |= ORDER_FIELD_08;
	orderInfo->fieldFlags |= ORDER_FIELD_09;
	orderInfo->fieldFlags |= ORDER_FIELD_10;
	orderInfo->fieldFlags |= ORDER_FIELD_11;
	orderInfo->fieldFlags |= ORDER_FIELD_12;
	update_write_brush(s, &patblt->brush, orderInfo->fieldFlags >> 7);
	return TRUE;
}
static BOOL update_read_scrblt_order(wStream* s, const ORDER_INFO* orderInfo, SCRBLT_ORDER* scrblt)
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
int update_approximate_scrblt_order(ORDER_INFO* orderInfo, const SCRBLT_ORDER* scrblt)
{
	return 32;
}
BOOL update_write_scrblt_order(wStream* s, ORDER_INFO* orderInfo, const SCRBLT_ORDER* scrblt)
{
	if (!Stream_EnsureRemainingCapacity(s, update_approximate_scrblt_order(orderInfo, scrblt)))
		return FALSE;

	orderInfo->fieldFlags = 0;
	orderInfo->fieldFlags |= ORDER_FIELD_01;
	update_write_coord(s, scrblt->nLeftRect);
	orderInfo->fieldFlags |= ORDER_FIELD_02;
	update_write_coord(s, scrblt->nTopRect);
	orderInfo->fieldFlags |= ORDER_FIELD_03;
	update_write_coord(s, scrblt->nWidth);
	orderInfo->fieldFlags |= ORDER_FIELD_04;
	update_write_coord(s, scrblt->nHeight);
	orderInfo->fieldFlags |= ORDER_FIELD_05;
	Stream_Write_UINT8(s, scrblt->bRop);
	orderInfo->fieldFlags |= ORDER_FIELD_06;
	update_write_coord(s, scrblt->nXSrc);
	orderInfo->fieldFlags |= ORDER_FIELD_07;
	update_write_coord(s, scrblt->nYSrc);
	return TRUE;
}
static BOOL update_read_opaque_rect_order(wStream* s, const ORDER_INFO* orderInfo,
                                          OPAQUE_RECT_ORDER* opaque_rect)
{
	BYTE byte;
	ORDER_FIELD_COORD(1, opaque_rect->nLeftRect);
	ORDER_FIELD_COORD(2, opaque_rect->nTopRect);
	ORDER_FIELD_COORD(3, opaque_rect->nWidth);
	ORDER_FIELD_COORD(4, opaque_rect->nHeight);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
	{
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, byte);
		opaque_rect->color = (opaque_rect->color & 0x00FFFF00) | ((UINT32)byte);
	}

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
	{
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, byte);
		opaque_rect->color = (opaque_rect->color & 0x00FF00FF) | ((UINT32)byte << 8);
	}

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
	{
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, byte);
		opaque_rect->color = (opaque_rect->color & 0x0000FFFF) | ((UINT32)byte << 16);
	}

	return TRUE;
}
int update_approximate_opaque_rect_order(ORDER_INFO* orderInfo,
                                         const OPAQUE_RECT_ORDER* opaque_rect)
{
	return 32;
}
BOOL update_write_opaque_rect_order(wStream* s, ORDER_INFO* orderInfo,
                                    const OPAQUE_RECT_ORDER* opaque_rect)
{
	BYTE byte;
	int inf = update_approximate_opaque_rect_order(orderInfo, opaque_rect);

	if (!Stream_EnsureRemainingCapacity(s, inf))
		return FALSE;

	// TODO: Color format conversion
	orderInfo->fieldFlags = 0;
	orderInfo->fieldFlags |= ORDER_FIELD_01;
	update_write_coord(s, opaque_rect->nLeftRect);
	orderInfo->fieldFlags |= ORDER_FIELD_02;
	update_write_coord(s, opaque_rect->nTopRect);
	orderInfo->fieldFlags |= ORDER_FIELD_03;
	update_write_coord(s, opaque_rect->nWidth);
	orderInfo->fieldFlags |= ORDER_FIELD_04;
	update_write_coord(s, opaque_rect->nHeight);
	orderInfo->fieldFlags |= ORDER_FIELD_05;
	byte = opaque_rect->color & 0x000000FF;
	Stream_Write_UINT8(s, byte);
	orderInfo->fieldFlags |= ORDER_FIELD_06;
	byte = (opaque_rect->color & 0x0000FF00) >> 8;
	Stream_Write_UINT8(s, byte);
	orderInfo->fieldFlags |= ORDER_FIELD_07;
	byte = (opaque_rect->color & 0x00FF0000) >> 16;
	Stream_Write_UINT8(s, byte);
	return TRUE;
}
static BOOL update_read_draw_nine_grid_order(wStream* s, const ORDER_INFO* orderInfo,
                                             DRAW_NINE_GRID_ORDER* draw_nine_grid)
{
	ORDER_FIELD_COORD(1, draw_nine_grid->srcLeft);
	ORDER_FIELD_COORD(2, draw_nine_grid->srcTop);
	ORDER_FIELD_COORD(3, draw_nine_grid->srcRight);
	ORDER_FIELD_COORD(4, draw_nine_grid->srcBottom);
	ORDER_FIELD_UINT16(5, draw_nine_grid->bitmapId);
	return TRUE;
}
static BOOL update_read_multi_dstblt_order(wStream* s, const ORDER_INFO* orderInfo,
                                           MULTI_DSTBLT_ORDER* multi_dstblt)
{
	ORDER_FIELD_COORD(1, multi_dstblt->nLeftRect);
	ORDER_FIELD_COORD(2, multi_dstblt->nTopRect);
	ORDER_FIELD_COORD(3, multi_dstblt->nWidth);
	ORDER_FIELD_COORD(4, multi_dstblt->nHeight);
	ORDER_FIELD_BYTE(5, multi_dstblt->bRop);
	ORDER_FIELD_BYTE(6, multi_dstblt->numRectangles);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
	{
		if (Stream_GetRemainingLength(s) < 2)
			return FALSE;

		Stream_Read_UINT16(s, multi_dstblt->cbData);
		return update_read_delta_rects(s, multi_dstblt->rectangles, multi_dstblt->numRectangles);
	}

	return TRUE;
}
static BOOL update_read_multi_patblt_order(wStream* s, const ORDER_INFO* orderInfo,
                                           MULTI_PATBLT_ORDER* multi_patblt)
{
	ORDER_FIELD_COORD(1, multi_patblt->nLeftRect);
	ORDER_FIELD_COORD(2, multi_patblt->nTopRect);
	ORDER_FIELD_COORD(3, multi_patblt->nWidth);
	ORDER_FIELD_COORD(4, multi_patblt->nHeight);
	ORDER_FIELD_BYTE(5, multi_patblt->bRop);
	ORDER_FIELD_COLOR(orderInfo, s, 6, &multi_patblt->backColor);
	ORDER_FIELD_COLOR(orderInfo, s, 7, &multi_patblt->foreColor);

	if (!update_read_brush(s, &multi_patblt->brush, orderInfo->fieldFlags >> 7))
		return FALSE;

	ORDER_FIELD_BYTE(13, multi_patblt->numRectangles);

	if (orderInfo->fieldFlags & ORDER_FIELD_14)
	{
		if (Stream_GetRemainingLength(s) < 2)
			return FALSE;

		Stream_Read_UINT16(s, multi_patblt->cbData);

		if (!update_read_delta_rects(s, multi_patblt->rectangles, multi_patblt->numRectangles))
			return FALSE;
	}

	return TRUE;
}
static BOOL update_read_multi_scrblt_order(wStream* s, const ORDER_INFO* orderInfo,
                                           MULTI_SCRBLT_ORDER* multi_scrblt)
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
		if (Stream_GetRemainingLength(s) < 2)
			return FALSE;

		Stream_Read_UINT16(s, multi_scrblt->cbData);
		return update_read_delta_rects(s, multi_scrblt->rectangles, multi_scrblt->numRectangles);
	}

	return TRUE;
}
static BOOL update_read_multi_opaque_rect_order(wStream* s, const ORDER_INFO* orderInfo,
                                                MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect)
{
	BYTE byte;
	ORDER_FIELD_COORD(1, multi_opaque_rect->nLeftRect);
	ORDER_FIELD_COORD(2, multi_opaque_rect->nTopRect);
	ORDER_FIELD_COORD(3, multi_opaque_rect->nWidth);
	ORDER_FIELD_COORD(4, multi_opaque_rect->nHeight);

	if (orderInfo->fieldFlags & ORDER_FIELD_05)
	{
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, byte);
		multi_opaque_rect->color = (multi_opaque_rect->color & 0x00FFFF00) | ((UINT32)byte);
	}

	if (orderInfo->fieldFlags & ORDER_FIELD_06)
	{
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, byte);
		multi_opaque_rect->color = (multi_opaque_rect->color & 0x00FF00FF) | ((UINT32)byte << 8);
	}

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
	{
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, byte);
		multi_opaque_rect->color = (multi_opaque_rect->color & 0x0000FFFF) | ((UINT32)byte << 16);
	}

	ORDER_FIELD_BYTE(8, multi_opaque_rect->numRectangles);

	if (orderInfo->fieldFlags & ORDER_FIELD_09)
	{
		if (Stream_GetRemainingLength(s) < 2)
			return FALSE;

		Stream_Read_UINT16(s, multi_opaque_rect->cbData);
		return update_read_delta_rects(s, multi_opaque_rect->rectangles,
		                               multi_opaque_rect->numRectangles);
	}

	return TRUE;
}
static BOOL update_read_multi_draw_nine_grid_order(wStream* s, const ORDER_INFO* orderInfo,
                                                   MULTI_DRAW_NINE_GRID_ORDER* multi_draw_nine_grid)
{
	ORDER_FIELD_COORD(1, multi_draw_nine_grid->srcLeft);
	ORDER_FIELD_COORD(2, multi_draw_nine_grid->srcTop);
	ORDER_FIELD_COORD(3, multi_draw_nine_grid->srcRight);
	ORDER_FIELD_COORD(4, multi_draw_nine_grid->srcBottom);
	ORDER_FIELD_UINT16(5, multi_draw_nine_grid->bitmapId);
	ORDER_FIELD_BYTE(6, multi_draw_nine_grid->nDeltaEntries);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
	{
		if (Stream_GetRemainingLength(s) < 2)
			return FALSE;

		Stream_Read_UINT16(s, multi_draw_nine_grid->cbData);
		return update_read_delta_rects(s, multi_draw_nine_grid->rectangles,
		                               multi_draw_nine_grid->nDeltaEntries);
	}

	return TRUE;
}
static BOOL update_read_line_to_order(wStream* s, const ORDER_INFO* orderInfo,
                                      LINE_TO_ORDER* line_to)
{
	ORDER_FIELD_UINT16(1, line_to->backMode);
	ORDER_FIELD_COORD(2, line_to->nXStart);
	ORDER_FIELD_COORD(3, line_to->nYStart);
	ORDER_FIELD_COORD(4, line_to->nXEnd);
	ORDER_FIELD_COORD(5, line_to->nYEnd);
	ORDER_FIELD_COLOR(orderInfo, s, 6, &line_to->backColor);
	ORDER_FIELD_BYTE(7, line_to->bRop2);
	ORDER_FIELD_BYTE(8, line_to->penStyle);
	ORDER_FIELD_BYTE(9, line_to->penWidth);
	ORDER_FIELD_COLOR(orderInfo, s, 10, &line_to->penColor);
	return TRUE;
}
int update_approximate_line_to_order(ORDER_INFO* orderInfo, const LINE_TO_ORDER* line_to)
{
	return 32;
}
BOOL update_write_line_to_order(wStream* s, ORDER_INFO* orderInfo, const LINE_TO_ORDER* line_to)
{
	if (!Stream_EnsureRemainingCapacity(s, update_approximate_line_to_order(orderInfo, line_to)))
		return FALSE;

	orderInfo->fieldFlags = 0;
	orderInfo->fieldFlags |= ORDER_FIELD_01;
	Stream_Write_UINT16(s, line_to->backMode);
	orderInfo->fieldFlags |= ORDER_FIELD_02;
	update_write_coord(s, line_to->nXStart);
	orderInfo->fieldFlags |= ORDER_FIELD_03;
	update_write_coord(s, line_to->nYStart);
	orderInfo->fieldFlags |= ORDER_FIELD_04;
	update_write_coord(s, line_to->nXEnd);
	orderInfo->fieldFlags |= ORDER_FIELD_05;
	update_write_coord(s, line_to->nYEnd);
	orderInfo->fieldFlags |= ORDER_FIELD_06;
	update_write_color(s, line_to->backColor);
	orderInfo->fieldFlags |= ORDER_FIELD_07;
	Stream_Write_UINT8(s, line_to->bRop2);
	orderInfo->fieldFlags |= ORDER_FIELD_08;
	Stream_Write_UINT8(s, line_to->penStyle);
	orderInfo->fieldFlags |= ORDER_FIELD_09;
	Stream_Write_UINT8(s, line_to->penWidth);
	orderInfo->fieldFlags |= ORDER_FIELD_10;
	update_write_color(s, line_to->penColor);
	return TRUE;
}
static BOOL update_read_polyline_order(wStream* s, const ORDER_INFO* orderInfo,
                                       POLYLINE_ORDER* polyline)
{
	UINT16 word;
	UINT32 new_num = polyline->numDeltaEntries;
	ORDER_FIELD_COORD(1, polyline->xStart);
	ORDER_FIELD_COORD(2, polyline->yStart);
	ORDER_FIELD_BYTE(3, polyline->bRop2);
	ORDER_FIELD_UINT16(4, word);
	ORDER_FIELD_COLOR(orderInfo, s, 5, &polyline->penColor);
	ORDER_FIELD_BYTE(6, new_num);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
	{
		DELTA_POINT* new_points;

		if (Stream_GetRemainingLength(s) < 1)
		{
			WLog_ERR(TAG, "Stream_GetRemainingLength(s) < 1");
			return FALSE;
		}

		Stream_Read_UINT8(s, polyline->cbData);
		new_points = (DELTA_POINT*)realloc(polyline->points, sizeof(DELTA_POINT) * new_num);

		if (!new_points)
		{
			WLog_ERR(TAG, "realloc(%" PRIu32 ") failed", new_num);
			return FALSE;
		}

		polyline->points = new_points;
		polyline->numDeltaEntries = new_num;
		return update_read_delta_points(s, polyline->points, polyline->numDeltaEntries,
		                                polyline->xStart, polyline->yStart);
	}

	return TRUE;
}
static BOOL update_read_memblt_order(wStream* s, const ORDER_INFO* orderInfo, MEMBLT_ORDER* memblt)
{
	if (!s || !orderInfo || !memblt)
		return FALSE;

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
	memblt->bitmap = NULL;
	return TRUE;
}
int update_approximate_memblt_order(ORDER_INFO* orderInfo, const MEMBLT_ORDER* memblt)
{
	return 64;
}
BOOL update_write_memblt_order(wStream* s, ORDER_INFO* orderInfo, const MEMBLT_ORDER* memblt)
{
	UINT16 cacheId;

	if (!Stream_EnsureRemainingCapacity(s, update_approximate_memblt_order(orderInfo, memblt)))
		return FALSE;

	cacheId = (memblt->cacheId & 0xFF) | ((memblt->colorIndex & 0xFF) << 8);
	orderInfo->fieldFlags |= ORDER_FIELD_01;
	Stream_Write_UINT16(s, cacheId);
	orderInfo->fieldFlags |= ORDER_FIELD_02;
	update_write_coord(s, memblt->nLeftRect);
	orderInfo->fieldFlags |= ORDER_FIELD_03;
	update_write_coord(s, memblt->nTopRect);
	orderInfo->fieldFlags |= ORDER_FIELD_04;
	update_write_coord(s, memblt->nWidth);
	orderInfo->fieldFlags |= ORDER_FIELD_05;
	update_write_coord(s, memblt->nHeight);
	orderInfo->fieldFlags |= ORDER_FIELD_06;
	Stream_Write_UINT8(s, memblt->bRop);
	orderInfo->fieldFlags |= ORDER_FIELD_07;
	update_write_coord(s, memblt->nXSrc);
	orderInfo->fieldFlags |= ORDER_FIELD_08;
	update_write_coord(s, memblt->nYSrc);
	orderInfo->fieldFlags |= ORDER_FIELD_09;
	Stream_Write_UINT16(s, memblt->cacheIndex);
	return TRUE;
}
static BOOL update_read_mem3blt_order(wStream* s, const ORDER_INFO* orderInfo,
                                      MEM3BLT_ORDER* mem3blt)
{
	ORDER_FIELD_UINT16(1, mem3blt->cacheId);
	ORDER_FIELD_COORD(2, mem3blt->nLeftRect);
	ORDER_FIELD_COORD(3, mem3blt->nTopRect);
	ORDER_FIELD_COORD(4, mem3blt->nWidth);
	ORDER_FIELD_COORD(5, mem3blt->nHeight);
	ORDER_FIELD_BYTE(6, mem3blt->bRop);
	ORDER_FIELD_COORD(7, mem3blt->nXSrc);
	ORDER_FIELD_COORD(8, mem3blt->nYSrc);
	ORDER_FIELD_COLOR(orderInfo, s, 9, &mem3blt->backColor);
	ORDER_FIELD_COLOR(orderInfo, s, 10, &mem3blt->foreColor);

	if (!update_read_brush(s, &mem3blt->brush, orderInfo->fieldFlags >> 10))
		return FALSE;

	ORDER_FIELD_UINT16(16, mem3blt->cacheIndex);
	mem3blt->colorIndex = (mem3blt->cacheId >> 8);
	mem3blt->cacheId = (mem3blt->cacheId & 0xFF);
	mem3blt->bitmap = NULL;
	return TRUE;
}
static BOOL update_read_save_bitmap_order(wStream* s, const ORDER_INFO* orderInfo,
                                          SAVE_BITMAP_ORDER* save_bitmap)
{
	ORDER_FIELD_UINT32(1, save_bitmap->savedBitmapPosition);
	ORDER_FIELD_COORD(2, save_bitmap->nLeftRect);
	ORDER_FIELD_COORD(3, save_bitmap->nTopRect);
	ORDER_FIELD_COORD(4, save_bitmap->nRightRect);
	ORDER_FIELD_COORD(5, save_bitmap->nBottomRect);
	ORDER_FIELD_BYTE(6, save_bitmap->operation);
	return TRUE;
}
static BOOL update_read_glyph_index_order(wStream* s, const ORDER_INFO* orderInfo,
                                          GLYPH_INDEX_ORDER* glyph_index)
{
	ORDER_FIELD_BYTE(1, glyph_index->cacheId);
	ORDER_FIELD_BYTE(2, glyph_index->flAccel);
	ORDER_FIELD_BYTE(3, glyph_index->ulCharInc);
	ORDER_FIELD_BYTE(4, glyph_index->fOpRedundant);
	ORDER_FIELD_COLOR(orderInfo, s, 5, &glyph_index->backColor);
	ORDER_FIELD_COLOR(orderInfo, s, 6, &glyph_index->foreColor);
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
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, glyph_index->cbData);

		if (Stream_GetRemainingLength(s) < glyph_index->cbData)
			return FALSE;

		CopyMemory(glyph_index->data, Stream_Pointer(s), glyph_index->cbData);
		Stream_Seek(s, glyph_index->cbData);
	}

	return TRUE;
}
int update_approximate_glyph_index_order(ORDER_INFO* orderInfo,
                                         const GLYPH_INDEX_ORDER* glyph_index)
{
	return 64;
}
BOOL update_write_glyph_index_order(wStream* s, ORDER_INFO* orderInfo,
                                    GLYPH_INDEX_ORDER* glyph_index)
{
	int inf = update_approximate_glyph_index_order(orderInfo, glyph_index);

	if (!Stream_EnsureRemainingCapacity(s, inf))
		return FALSE;

	orderInfo->fieldFlags = 0;
	orderInfo->fieldFlags |= ORDER_FIELD_01;
	Stream_Write_UINT8(s, glyph_index->cacheId);
	orderInfo->fieldFlags |= ORDER_FIELD_02;
	Stream_Write_UINT8(s, glyph_index->flAccel);
	orderInfo->fieldFlags |= ORDER_FIELD_03;
	Stream_Write_UINT8(s, glyph_index->ulCharInc);
	orderInfo->fieldFlags |= ORDER_FIELD_04;
	Stream_Write_UINT8(s, glyph_index->fOpRedundant);
	orderInfo->fieldFlags |= ORDER_FIELD_05;
	update_write_color(s, glyph_index->backColor);
	orderInfo->fieldFlags |= ORDER_FIELD_06;
	update_write_color(s, glyph_index->foreColor);
	orderInfo->fieldFlags |= ORDER_FIELD_07;
	Stream_Write_UINT16(s, glyph_index->bkLeft);
	orderInfo->fieldFlags |= ORDER_FIELD_08;
	Stream_Write_UINT16(s, glyph_index->bkTop);
	orderInfo->fieldFlags |= ORDER_FIELD_09;
	Stream_Write_UINT16(s, glyph_index->bkRight);
	orderInfo->fieldFlags |= ORDER_FIELD_10;
	Stream_Write_UINT16(s, glyph_index->bkBottom);
	orderInfo->fieldFlags |= ORDER_FIELD_11;
	Stream_Write_UINT16(s, glyph_index->opLeft);
	orderInfo->fieldFlags |= ORDER_FIELD_12;
	Stream_Write_UINT16(s, glyph_index->opTop);
	orderInfo->fieldFlags |= ORDER_FIELD_13;
	Stream_Write_UINT16(s, glyph_index->opRight);
	orderInfo->fieldFlags |= ORDER_FIELD_14;
	Stream_Write_UINT16(s, glyph_index->opBottom);
	orderInfo->fieldFlags |= ORDER_FIELD_15;
	orderInfo->fieldFlags |= ORDER_FIELD_16;
	orderInfo->fieldFlags |= ORDER_FIELD_17;
	orderInfo->fieldFlags |= ORDER_FIELD_18;
	orderInfo->fieldFlags |= ORDER_FIELD_19;
	update_write_brush(s, &glyph_index->brush, orderInfo->fieldFlags >> 14);
	orderInfo->fieldFlags |= ORDER_FIELD_20;
	Stream_Write_UINT16(s, glyph_index->x);
	orderInfo->fieldFlags |= ORDER_FIELD_21;
	Stream_Write_UINT16(s, glyph_index->y);
	orderInfo->fieldFlags |= ORDER_FIELD_22;
	Stream_Write_UINT8(s, glyph_index->cbData);
	Stream_Write(s, glyph_index->data, glyph_index->cbData);
	return TRUE;
}
static BOOL update_read_fast_index_order(wStream* s, const ORDER_INFO* orderInfo,
                                         FAST_INDEX_ORDER* fast_index)
{
	ORDER_FIELD_BYTE(1, fast_index->cacheId);
	ORDER_FIELD_2BYTE(2, fast_index->ulCharInc, fast_index->flAccel);
	ORDER_FIELD_COLOR(orderInfo, s, 3, &fast_index->backColor);
	ORDER_FIELD_COLOR(orderInfo, s, 4, &fast_index->foreColor);
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
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, fast_index->cbData);

		if (Stream_GetRemainingLength(s) < fast_index->cbData)
			return FALSE;

		CopyMemory(fast_index->data, Stream_Pointer(s), fast_index->cbData);
		Stream_Seek(s, fast_index->cbData);
	}

	return TRUE;
}
static BOOL update_read_fast_glyph_order(wStream* s, const ORDER_INFO* orderInfo,
                                         FAST_GLYPH_ORDER* fastGlyph)
{
	BYTE* phold;
	GLYPH_DATA_V2* glyph = &fastGlyph->glyphData;
	ORDER_FIELD_BYTE(1, fastGlyph->cacheId);
	ORDER_FIELD_2BYTE(2, fastGlyph->ulCharInc, fastGlyph->flAccel);
	ORDER_FIELD_COLOR(orderInfo, s, 3, &fastGlyph->backColor);
	ORDER_FIELD_COLOR(orderInfo, s, 4, &fastGlyph->foreColor);
	ORDER_FIELD_COORD(5, fastGlyph->bkLeft);
	ORDER_FIELD_COORD(6, fastGlyph->bkTop);
	ORDER_FIELD_COORD(7, fastGlyph->bkRight);
	ORDER_FIELD_COORD(8, fastGlyph->bkBottom);
	ORDER_FIELD_COORD(9, fastGlyph->opLeft);
	ORDER_FIELD_COORD(10, fastGlyph->opTop);
	ORDER_FIELD_COORD(11, fastGlyph->opRight);
	ORDER_FIELD_COORD(12, fastGlyph->opBottom);
	ORDER_FIELD_COORD(13, fastGlyph->x);
	ORDER_FIELD_COORD(14, fastGlyph->y);

	if (orderInfo->fieldFlags & ORDER_FIELD_15)
	{
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, fastGlyph->cbData);

		if (Stream_GetRemainingLength(s) < fastGlyph->cbData)
			return FALSE;

		CopyMemory(fastGlyph->data, Stream_Pointer(s), fastGlyph->cbData);
		phold = Stream_Pointer(s);

		if (!Stream_SafeSeek(s, 1))
			return FALSE;

		if (fastGlyph->cbData > 1)
		{
			UINT32 new_cb;
			/* parse optional glyph data */
			glyph->cacheIndex = fastGlyph->data[0];

			if (!update_read_2byte_signed(s, &glyph->x) ||
			    !update_read_2byte_signed(s, &glyph->y) ||
			    !update_read_2byte_unsigned(s, &glyph->cx) ||
			    !update_read_2byte_unsigned(s, &glyph->cy))
				return FALSE;

			glyph->cb = ((glyph->cx + 7) / 8) * glyph->cy;
			glyph->cb += ((glyph->cb % 4) > 0) ? 4 - (glyph->cb % 4) : 0;
			new_cb = ((glyph->cx + 7) / 8) * glyph->cy;
			new_cb += ((new_cb % 4) > 0) ? 4 - (new_cb % 4) : 0;

			if (Stream_GetRemainingLength(s) < new_cb)
				return FALSE;

			if (new_cb)
			{
				BYTE* new_aj;
				new_aj = (BYTE*)realloc(glyph->aj, new_cb);

				if (!new_aj)
					return FALSE;

				glyph->aj = new_aj;
				glyph->cb = new_cb;
				Stream_Read(s, glyph->aj, glyph->cb);
			}
		}

		Stream_SetPointer(s, phold + fastGlyph->cbData);
	}

	return TRUE;
}
static BOOL update_read_polygon_sc_order(wStream* s, const ORDER_INFO* orderInfo,
                                         POLYGON_SC_ORDER* polygon_sc)
{
	UINT32 num = polygon_sc->numPoints;
	ORDER_FIELD_COORD(1, polygon_sc->xStart);
	ORDER_FIELD_COORD(2, polygon_sc->yStart);
	ORDER_FIELD_BYTE(3, polygon_sc->bRop2);
	ORDER_FIELD_BYTE(4, polygon_sc->fillMode);
	ORDER_FIELD_COLOR(orderInfo, s, 5, &polygon_sc->brushColor);
	ORDER_FIELD_BYTE(6, num);

	if (orderInfo->fieldFlags & ORDER_FIELD_07)
	{
		DELTA_POINT* newpoints;

		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, polygon_sc->cbData);
		newpoints = (DELTA_POINT*)realloc(polygon_sc->points, sizeof(DELTA_POINT) * num);

		if (!newpoints)
			return FALSE;

		polygon_sc->points = newpoints;
		polygon_sc->numPoints = num;
		return update_read_delta_points(s, polygon_sc->points, polygon_sc->numPoints,
		                                polygon_sc->xStart, polygon_sc->yStart);
	}

	return TRUE;
}
static BOOL update_read_polygon_cb_order(wStream* s, const ORDER_INFO* orderInfo,
                                         POLYGON_CB_ORDER* polygon_cb)
{
	UINT32 num = polygon_cb->numPoints;
	ORDER_FIELD_COORD(1, polygon_cb->xStart);
	ORDER_FIELD_COORD(2, polygon_cb->yStart);
	ORDER_FIELD_BYTE(3, polygon_cb->bRop2);
	ORDER_FIELD_BYTE(4, polygon_cb->fillMode);
	ORDER_FIELD_COLOR(orderInfo, s, 5, &polygon_cb->backColor);
	ORDER_FIELD_COLOR(orderInfo, s, 6, &polygon_cb->foreColor);

	if (!update_read_brush(s, &polygon_cb->brush, orderInfo->fieldFlags >> 6))
		return FALSE;

	ORDER_FIELD_BYTE(12, num);

	if (orderInfo->fieldFlags & ORDER_FIELD_13)
	{
		DELTA_POINT* newpoints;

		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, polygon_cb->cbData);
		newpoints = (DELTA_POINT*)realloc(polygon_cb->points, sizeof(DELTA_POINT) * num);

		if (!newpoints)
			return FALSE;

		polygon_cb->points = newpoints;
		polygon_cb->numPoints = num;

		if (!update_read_delta_points(s, polygon_cb->points, polygon_cb->numPoints,
		                              polygon_cb->xStart, polygon_cb->yStart))
			return FALSE;
	}

	polygon_cb->backMode = (polygon_cb->bRop2 & 0x80) ? BACKMODE_TRANSPARENT : BACKMODE_OPAQUE;
	polygon_cb->bRop2 = (polygon_cb->bRop2 & 0x1F);
	return TRUE;
}
static BOOL update_read_ellipse_sc_order(wStream* s, const ORDER_INFO* orderInfo,
                                         ELLIPSE_SC_ORDER* ellipse_sc)
{
	ORDER_FIELD_COORD(1, ellipse_sc->leftRect);
	ORDER_FIELD_COORD(2, ellipse_sc->topRect);
	ORDER_FIELD_COORD(3, ellipse_sc->rightRect);
	ORDER_FIELD_COORD(4, ellipse_sc->bottomRect);
	ORDER_FIELD_BYTE(5, ellipse_sc->bRop2);
	ORDER_FIELD_BYTE(6, ellipse_sc->fillMode);
	ORDER_FIELD_COLOR(orderInfo, s, 7, &ellipse_sc->color);
	return TRUE;
}
static BOOL update_read_ellipse_cb_order(wStream* s, const ORDER_INFO* orderInfo,
                                         ELLIPSE_CB_ORDER* ellipse_cb)
{
	ORDER_FIELD_COORD(1, ellipse_cb->leftRect);
	ORDER_FIELD_COORD(2, ellipse_cb->topRect);
	ORDER_FIELD_COORD(3, ellipse_cb->rightRect);
	ORDER_FIELD_COORD(4, ellipse_cb->bottomRect);
	ORDER_FIELD_BYTE(5, ellipse_cb->bRop2);
	ORDER_FIELD_BYTE(6, ellipse_cb->fillMode);
	ORDER_FIELD_COLOR(orderInfo, s, 7, &ellipse_cb->backColor);
	ORDER_FIELD_COLOR(orderInfo, s, 8, &ellipse_cb->foreColor);
	return update_read_brush(s, &ellipse_cb->brush, orderInfo->fieldFlags >> 8);
}
/* Secondary Drawing Orders */
static CACHE_BITMAP_ORDER* update_read_cache_bitmap_order(rdpUpdate* update, wStream* s,
                                                          BOOL compressed, UINT16 flags)
{
	CACHE_BITMAP_ORDER* cache_bitmap;

	if (!update || !s)
		return NULL;

	cache_bitmap = calloc(1, sizeof(CACHE_BITMAP_ORDER));

	if (!cache_bitmap)
		goto fail;

	if (Stream_GetRemainingLength(s) < 9)
		goto fail;

	Stream_Read_UINT8(s, cache_bitmap->cacheId);      /* cacheId (1 byte) */
	Stream_Seek_UINT8(s);                             /* pad1Octet (1 byte) */
	Stream_Read_UINT8(s, cache_bitmap->bitmapWidth);  /* bitmapWidth (1 byte) */
	Stream_Read_UINT8(s, cache_bitmap->bitmapHeight); /* bitmapHeight (1 byte) */
	Stream_Read_UINT8(s, cache_bitmap->bitmapBpp);    /* bitmapBpp (1 byte) */

	if ((cache_bitmap->bitmapBpp < 1) || (cache_bitmap->bitmapBpp > 32))
	{
		WLog_Print(update->log, WLOG_ERROR, "invalid bitmap bpp %" PRIu32 "",
		           cache_bitmap->bitmapBpp);
		goto fail;
	}

	Stream_Read_UINT16(s, cache_bitmap->bitmapLength); /* bitmapLength (2 bytes) */
	Stream_Read_UINT16(s, cache_bitmap->cacheIndex);   /* cacheIndex (2 bytes) */

	if (compressed)
	{
		if ((flags & NO_BITMAP_COMPRESSION_HDR) == 0)
		{
			BYTE* bitmapComprHdr = (BYTE*)&(cache_bitmap->bitmapComprHdr);

			if (Stream_GetRemainingLength(s) < 8)
				goto fail;

			Stream_Read(s, bitmapComprHdr, 8); /* bitmapComprHdr (8 bytes) */
			cache_bitmap->bitmapLength -= 8;
		}
	}

	if (Stream_GetRemainingLength(s) < cache_bitmap->bitmapLength)
		goto fail;

	cache_bitmap->bitmapDataStream = malloc(cache_bitmap->bitmapLength);

	if (!cache_bitmap->bitmapDataStream)
		goto fail;

	Stream_Read(s, cache_bitmap->bitmapDataStream, cache_bitmap->bitmapLength);
	cache_bitmap->compressed = compressed;
	return cache_bitmap;
fail:
	free_cache_bitmap_order(update->context, cache_bitmap);
	return NULL;
}
int update_approximate_cache_bitmap_order(const CACHE_BITMAP_ORDER* cache_bitmap, BOOL compressed,
                                          UINT16* flags)
{
	return 64 + cache_bitmap->bitmapLength;
}
BOOL update_write_cache_bitmap_order(wStream* s, const CACHE_BITMAP_ORDER* cache_bitmap,
                                     BOOL compressed, UINT16* flags)
{
	UINT32 bitmapLength = cache_bitmap->bitmapLength;
	int inf = update_approximate_cache_bitmap_order(cache_bitmap, compressed, flags);

	if (!Stream_EnsureRemainingCapacity(s, inf))
		return FALSE;

	*flags = NO_BITMAP_COMPRESSION_HDR;

	if ((*flags & NO_BITMAP_COMPRESSION_HDR) == 0)
		bitmapLength += 8;

	Stream_Write_UINT8(s, cache_bitmap->cacheId);      /* cacheId (1 byte) */
	Stream_Write_UINT8(s, 0);                          /* pad1Octet (1 byte) */
	Stream_Write_UINT8(s, cache_bitmap->bitmapWidth);  /* bitmapWidth (1 byte) */
	Stream_Write_UINT8(s, cache_bitmap->bitmapHeight); /* bitmapHeight (1 byte) */
	Stream_Write_UINT8(s, cache_bitmap->bitmapBpp);    /* bitmapBpp (1 byte) */
	Stream_Write_UINT16(s, bitmapLength);              /* bitmapLength (2 bytes) */
	Stream_Write_UINT16(s, cache_bitmap->cacheIndex);  /* cacheIndex (2 bytes) */

	if (compressed)
	{
		if ((*flags & NO_BITMAP_COMPRESSION_HDR) == 0)
		{
			BYTE* bitmapComprHdr = (BYTE*)&(cache_bitmap->bitmapComprHdr);
			Stream_Write(s, bitmapComprHdr, 8); /* bitmapComprHdr (8 bytes) */
			bitmapLength -= 8;
		}

		Stream_Write(s, cache_bitmap->bitmapDataStream, bitmapLength);
	}
	else
	{
		Stream_Write(s, cache_bitmap->bitmapDataStream, bitmapLength);
	}

	return TRUE;
}
static CACHE_BITMAP_V2_ORDER* update_read_cache_bitmap_v2_order(rdpUpdate* update, wStream* s,
                                                                BOOL compressed, UINT16 flags)
{
	BYTE bitsPerPixelId;
	CACHE_BITMAP_V2_ORDER* cache_bitmap_v2;

	if (!update || !s)
		return NULL;

	cache_bitmap_v2 = calloc(1, sizeof(CACHE_BITMAP_V2_ORDER));

	if (!cache_bitmap_v2)
		goto fail;

	cache_bitmap_v2->cacheId = flags & 0x0003;
	cache_bitmap_v2->flags = (flags & 0xFF80) >> 7;
	bitsPerPixelId = (flags & 0x0078) >> 3;
	cache_bitmap_v2->bitmapBpp = CBR2_BPP[bitsPerPixelId];

	if (cache_bitmap_v2->flags & CBR2_PERSISTENT_KEY_PRESENT)
	{
		if (Stream_GetRemainingLength(s) < 8)
			goto fail;

		Stream_Read_UINT32(s, cache_bitmap_v2->key1); /* key1 (4 bytes) */
		Stream_Read_UINT32(s, cache_bitmap_v2->key2); /* key2 (4 bytes) */
	}

	if (cache_bitmap_v2->flags & CBR2_HEIGHT_SAME_AS_WIDTH)
	{
		if (!update_read_2byte_unsigned(s, &cache_bitmap_v2->bitmapWidth)) /* bitmapWidth */
			goto fail;

		cache_bitmap_v2->bitmapHeight = cache_bitmap_v2->bitmapWidth;
	}
	else
	{
		if (!update_read_2byte_unsigned(s, &cache_bitmap_v2->bitmapWidth) || /* bitmapWidth */
		    !update_read_2byte_unsigned(s, &cache_bitmap_v2->bitmapHeight))  /* bitmapHeight */
			goto fail;
	}

	if (!update_read_4byte_unsigned(s, &cache_bitmap_v2->bitmapLength) || /* bitmapLength */
	    !update_read_2byte_unsigned(s, &cache_bitmap_v2->cacheIndex))     /* cacheIndex */
		goto fail;

	if (cache_bitmap_v2->flags & CBR2_DO_NOT_CACHE)
		cache_bitmap_v2->cacheIndex = BITMAP_CACHE_WAITING_LIST_INDEX;

	if (compressed)
	{
		if (!(cache_bitmap_v2->flags & CBR2_NO_BITMAP_COMPRESSION_HDR))
		{
			if (Stream_GetRemainingLength(s) < 8)
				goto fail;

			Stream_Read_UINT16(
			    s, cache_bitmap_v2->cbCompFirstRowSize); /* cbCompFirstRowSize (2 bytes) */
			Stream_Read_UINT16(
			    s, cache_bitmap_v2->cbCompMainBodySize);         /* cbCompMainBodySize (2 bytes) */
			Stream_Read_UINT16(s, cache_bitmap_v2->cbScanWidth); /* cbScanWidth (2 bytes) */
			Stream_Read_UINT16(
			    s, cache_bitmap_v2->cbUncompressedSize); /* cbUncompressedSize (2 bytes) */
			cache_bitmap_v2->bitmapLength = cache_bitmap_v2->cbCompMainBodySize;
		}
	}

	if (Stream_GetRemainingLength(s) < cache_bitmap_v2->bitmapLength)
		goto fail;

	if (cache_bitmap_v2->bitmapLength == 0)
		goto fail;

	cache_bitmap_v2->bitmapDataStream = malloc(cache_bitmap_v2->bitmapLength);

	if (!cache_bitmap_v2->bitmapDataStream)
		goto fail;

	Stream_Read(s, cache_bitmap_v2->bitmapDataStream, cache_bitmap_v2->bitmapLength);
	cache_bitmap_v2->compressed = compressed;
	return cache_bitmap_v2;
fail:
	free_cache_bitmap_v2_order(update->context, cache_bitmap_v2);
	return NULL;
}
int update_approximate_cache_bitmap_v2_order(CACHE_BITMAP_V2_ORDER* cache_bitmap_v2,
                                             BOOL compressed, UINT16* flags)
{
	return 64 + cache_bitmap_v2->bitmapLength;
}
BOOL update_write_cache_bitmap_v2_order(wStream* s, CACHE_BITMAP_V2_ORDER* cache_bitmap_v2,
                                        BOOL compressed, UINT16* flags)
{
	BYTE bitsPerPixelId;

	if (!Stream_EnsureRemainingCapacity(
	        s, update_approximate_cache_bitmap_v2_order(cache_bitmap_v2, compressed, flags)))
		return FALSE;

	bitsPerPixelId = BPP_CBR2[cache_bitmap_v2->bitmapBpp];
	*flags = (cache_bitmap_v2->cacheId & 0x0003) | (bitsPerPixelId << 3) |
	         ((cache_bitmap_v2->flags << 7) & 0xFF80);

	if (cache_bitmap_v2->flags & CBR2_PERSISTENT_KEY_PRESENT)
	{
		Stream_Write_UINT32(s, cache_bitmap_v2->key1); /* key1 (4 bytes) */
		Stream_Write_UINT32(s, cache_bitmap_v2->key2); /* key2 (4 bytes) */
	}

	if (cache_bitmap_v2->flags & CBR2_HEIGHT_SAME_AS_WIDTH)
	{
		if (!update_write_2byte_unsigned(s, cache_bitmap_v2->bitmapWidth)) /* bitmapWidth */
			return FALSE;
	}
	else
	{
		if (!update_write_2byte_unsigned(s, cache_bitmap_v2->bitmapWidth) || /* bitmapWidth */
		    !update_write_2byte_unsigned(s, cache_bitmap_v2->bitmapHeight))  /* bitmapHeight */
			return FALSE;
	}

	if (cache_bitmap_v2->flags & CBR2_DO_NOT_CACHE)
		cache_bitmap_v2->cacheIndex = BITMAP_CACHE_WAITING_LIST_INDEX;

	if (!update_write_4byte_unsigned(s, cache_bitmap_v2->bitmapLength) || /* bitmapLength */
	    !update_write_2byte_unsigned(s, cache_bitmap_v2->cacheIndex))     /* cacheIndex */
		return FALSE;

	if (compressed)
	{
		if (!(cache_bitmap_v2->flags & CBR2_NO_BITMAP_COMPRESSION_HDR))
		{
			Stream_Write_UINT16(
			    s, cache_bitmap_v2->cbCompFirstRowSize); /* cbCompFirstRowSize (2 bytes) */
			Stream_Write_UINT16(
			    s, cache_bitmap_v2->cbCompMainBodySize);          /* cbCompMainBodySize (2 bytes) */
			Stream_Write_UINT16(s, cache_bitmap_v2->cbScanWidth); /* cbScanWidth (2 bytes) */
			Stream_Write_UINT16(
			    s, cache_bitmap_v2->cbUncompressedSize); /* cbUncompressedSize (2 bytes) */
			cache_bitmap_v2->bitmapLength = cache_bitmap_v2->cbCompMainBodySize;
		}

		if (!Stream_EnsureRemainingCapacity(s, cache_bitmap_v2->bitmapLength))
			return FALSE;

		Stream_Write(s, cache_bitmap_v2->bitmapDataStream, cache_bitmap_v2->bitmapLength);
	}
	else
	{
		if (!Stream_EnsureRemainingCapacity(s, cache_bitmap_v2->bitmapLength))
			return FALSE;

		Stream_Write(s, cache_bitmap_v2->bitmapDataStream, cache_bitmap_v2->bitmapLength);
	}

	cache_bitmap_v2->compressed = compressed;
	return TRUE;
}
static CACHE_BITMAP_V3_ORDER* update_read_cache_bitmap_v3_order(rdpUpdate* update, wStream* s,
                                                                UINT16 flags)
{
	BYTE bitsPerPixelId;
	BITMAP_DATA_EX* bitmapData;
	UINT32 new_len;
	BYTE* new_data;
	CACHE_BITMAP_V3_ORDER* cache_bitmap_v3;

	if (!update || !s)
		return NULL;

	cache_bitmap_v3 = calloc(1, sizeof(CACHE_BITMAP_V3_ORDER));

	if (!cache_bitmap_v3)
		goto fail;

	cache_bitmap_v3->cacheId = flags & 0x00000003;
	cache_bitmap_v3->flags = (flags & 0x0000FF80) >> 7;
	bitsPerPixelId = (flags & 0x00000078) >> 3;
	cache_bitmap_v3->bpp = CBR23_BPP[bitsPerPixelId];

	if (Stream_GetRemainingLength(s) < 21)
		goto fail;

	Stream_Read_UINT16(s, cache_bitmap_v3->cacheIndex); /* cacheIndex (2 bytes) */
	Stream_Read_UINT32(s, cache_bitmap_v3->key1);       /* key1 (4 bytes) */
	Stream_Read_UINT32(s, cache_bitmap_v3->key2);       /* key2 (4 bytes) */
	bitmapData = &cache_bitmap_v3->bitmapData;
	Stream_Read_UINT8(s, bitmapData->bpp);

	if ((bitmapData->bpp < 1) || (bitmapData->bpp > 32))
	{
		WLog_Print(update->log, WLOG_ERROR, "invalid bpp value %" PRIu32 "", bitmapData->bpp);
		goto fail;
	}

	Stream_Seek_UINT8(s);                      /* reserved1 (1 byte) */
	Stream_Seek_UINT8(s);                      /* reserved2 (1 byte) */
	Stream_Read_UINT8(s, bitmapData->codecID); /* codecID (1 byte) */
	Stream_Read_UINT16(s, bitmapData->width);  /* width (2 bytes) */
	Stream_Read_UINT16(s, bitmapData->height); /* height (2 bytes) */
	Stream_Read_UINT32(s, new_len);            /* length (4 bytes) */

	if ((new_len == 0) || (Stream_GetRemainingLength(s) < new_len))
		goto fail;

	new_data = (BYTE*)realloc(bitmapData->data, new_len);

	if (!new_data)
		goto fail;

	bitmapData->data = new_data;
	bitmapData->length = new_len;
	Stream_Read(s, bitmapData->data, bitmapData->length);
	return cache_bitmap_v3;
fail:
	free_cache_bitmap_v3_order(update->context, cache_bitmap_v3);
	return NULL;
}
int update_approximate_cache_bitmap_v3_order(CACHE_BITMAP_V3_ORDER* cache_bitmap_v3, UINT16* flags)
{
	BITMAP_DATA_EX* bitmapData = &cache_bitmap_v3->bitmapData;
	return 64 + bitmapData->length;
}
BOOL update_write_cache_bitmap_v3_order(wStream* s, CACHE_BITMAP_V3_ORDER* cache_bitmap_v3,
                                        UINT16* flags)
{
	BYTE bitsPerPixelId;
	BITMAP_DATA_EX* bitmapData;

	if (!Stream_EnsureRemainingCapacity(
	        s, update_approximate_cache_bitmap_v3_order(cache_bitmap_v3, flags)))
		return FALSE;

	bitmapData = &cache_bitmap_v3->bitmapData;
	bitsPerPixelId = BPP_CBR23[cache_bitmap_v3->bpp];
	*flags = (cache_bitmap_v3->cacheId & 0x00000003) |
	         ((cache_bitmap_v3->flags << 7) & 0x0000FF80) | ((bitsPerPixelId << 3) & 0x00000078);
	Stream_Write_UINT16(s, cache_bitmap_v3->cacheIndex); /* cacheIndex (2 bytes) */
	Stream_Write_UINT32(s, cache_bitmap_v3->key1);       /* key1 (4 bytes) */
	Stream_Write_UINT32(s, cache_bitmap_v3->key2);       /* key2 (4 bytes) */
	Stream_Write_UINT8(s, bitmapData->bpp);
	Stream_Write_UINT8(s, 0);                   /* reserved1 (1 byte) */
	Stream_Write_UINT8(s, 0);                   /* reserved2 (1 byte) */
	Stream_Write_UINT8(s, bitmapData->codecID); /* codecID (1 byte) */
	Stream_Write_UINT16(s, bitmapData->width);  /* width (2 bytes) */
	Stream_Write_UINT16(s, bitmapData->height); /* height (2 bytes) */
	Stream_Write_UINT32(s, bitmapData->length); /* length (4 bytes) */
	Stream_Write(s, bitmapData->data, bitmapData->length);
	return TRUE;
}
static CACHE_COLOR_TABLE_ORDER* update_read_cache_color_table_order(rdpUpdate* update, wStream* s,
                                                                    UINT16 flags)
{
	int i;
	UINT32* colorTable;
	CACHE_COLOR_TABLE_ORDER* cache_color_table = calloc(1, sizeof(CACHE_COLOR_TABLE_ORDER));

	if (!cache_color_table)
		goto fail;

	if (Stream_GetRemainingLength(s) < 3)
		goto fail;

	Stream_Read_UINT8(s, cache_color_table->cacheIndex);    /* cacheIndex (1 byte) */
	Stream_Read_UINT16(s, cache_color_table->numberColors); /* numberColors (2 bytes) */

	if (cache_color_table->numberColors != 256)
	{
		/* This field MUST be set to 256 */
		goto fail;
	}

	if (Stream_GetRemainingLength(s) < cache_color_table->numberColors * 4)
		goto fail;

	colorTable = (UINT32*)&cache_color_table->colorTable;

	for (i = 0; i < (int)cache_color_table->numberColors; i++)
		update_read_color_quad(s, &colorTable[i]);

	return cache_color_table;
fail:
	free_cache_color_table_order(update->context, cache_color_table);
	return NULL;
}
int update_approximate_cache_color_table_order(const CACHE_COLOR_TABLE_ORDER* cache_color_table,
                                               UINT16* flags)
{
	return 16 + (256 * 4);
}
BOOL update_write_cache_color_table_order(wStream* s,
                                          const CACHE_COLOR_TABLE_ORDER* cache_color_table,
                                          UINT16* flags)
{
	int i, inf;
	UINT32* colorTable;

	if (cache_color_table->numberColors != 256)
		return FALSE;

	inf = update_approximate_cache_color_table_order(cache_color_table, flags);

	if (!Stream_EnsureRemainingCapacity(s, inf))
		return FALSE;

	Stream_Write_UINT8(s, cache_color_table->cacheIndex);    /* cacheIndex (1 byte) */
	Stream_Write_UINT16(s, cache_color_table->numberColors); /* numberColors (2 bytes) */
	colorTable = (UINT32*)&cache_color_table->colorTable;

	for (i = 0; i < (int)cache_color_table->numberColors; i++)
	{
		update_write_color_quad(s, colorTable[i]);
	}

	return TRUE;
}
static CACHE_GLYPH_ORDER* update_read_cache_glyph_order(rdpUpdate* update, wStream* s, UINT16 flags)
{
	UINT32 i;
	CACHE_GLYPH_ORDER* cache_glyph_order = calloc(1, sizeof(CACHE_GLYPH_ORDER));

	if (!cache_glyph_order || !update || !s)
		goto fail;

	if (Stream_GetRemainingLength(s) < 2)
		goto fail;

	Stream_Read_UINT8(s, cache_glyph_order->cacheId); /* cacheId (1 byte) */
	Stream_Read_UINT8(s, cache_glyph_order->cGlyphs); /* cGlyphs (1 byte) */

	for (i = 0; i < cache_glyph_order->cGlyphs; i++)
	{
		GLYPH_DATA* glyph = &cache_glyph_order->glyphData[i];

		if (Stream_GetRemainingLength(s) < 10)
			goto fail;

		Stream_Read_UINT16(s, glyph->cacheIndex);
		Stream_Read_INT16(s, glyph->x);
		Stream_Read_INT16(s, glyph->y);
		Stream_Read_UINT16(s, glyph->cx);
		Stream_Read_UINT16(s, glyph->cy);
		glyph->cb = ((glyph->cx + 7) / 8) * glyph->cy;
		glyph->cb += ((glyph->cb % 4) > 0) ? 4 - (glyph->cb % 4) : 0;

		if (Stream_GetRemainingLength(s) < glyph->cb)
			goto fail;

		glyph->aj = (BYTE*)malloc(glyph->cb);

		if (!glyph->aj)
			goto fail;

		Stream_Read(s, glyph->aj, glyph->cb);
	}

	if ((flags & CG_GLYPH_UNICODE_PRESENT) && (cache_glyph_order->cGlyphs > 0))
	{
		cache_glyph_order->unicodeCharacters = calloc(cache_glyph_order->cGlyphs, sizeof(WCHAR));

		if (!cache_glyph_order->unicodeCharacters)
			goto fail;

		if (Stream_GetRemainingLength(s) < sizeof(WCHAR) * cache_glyph_order->cGlyphs)
			goto fail;

		Stream_Read_UTF16_String(s, cache_glyph_order->unicodeCharacters,
		                         cache_glyph_order->cGlyphs);
	}

	return cache_glyph_order;
fail:
	free_cache_glyph_order(update->context, cache_glyph_order);
	return NULL;
}
int update_approximate_cache_glyph_order(const CACHE_GLYPH_ORDER* cache_glyph, UINT16* flags)
{
	return 2 + cache_glyph->cGlyphs * 32;
}
BOOL update_write_cache_glyph_order(wStream* s, const CACHE_GLYPH_ORDER* cache_glyph, UINT16* flags)
{
	int i, inf;
	INT16 lsi16;
	const GLYPH_DATA* glyph;
	inf = update_approximate_cache_glyph_order(cache_glyph, flags);

	if (!Stream_EnsureRemainingCapacity(s, inf))
		return FALSE;

	Stream_Write_UINT8(s, cache_glyph->cacheId); /* cacheId (1 byte) */
	Stream_Write_UINT8(s, cache_glyph->cGlyphs); /* cGlyphs (1 byte) */

	for (i = 0; i < (int)cache_glyph->cGlyphs; i++)
	{
		UINT32 cb;
		glyph = &cache_glyph->glyphData[i];
		Stream_Write_UINT16(s, glyph->cacheIndex); /* cacheIndex (2 bytes) */
		lsi16 = glyph->x;
		Stream_Write_UINT16(s, lsi16); /* x (2 bytes) */
		lsi16 = glyph->y;
		Stream_Write_UINT16(s, lsi16);     /* y (2 bytes) */
		Stream_Write_UINT16(s, glyph->cx); /* cx (2 bytes) */
		Stream_Write_UINT16(s, glyph->cy); /* cy (2 bytes) */
		cb = ((glyph->cx + 7) / 8) * glyph->cy;
		cb += ((cb % 4) > 0) ? 4 - (cb % 4) : 0;
		Stream_Write(s, glyph->aj, cb);
	}

	if (*flags & CG_GLYPH_UNICODE_PRESENT)
	{
		Stream_Zero(s, cache_glyph->cGlyphs * 2);
	}

	return TRUE;
}
static CACHE_GLYPH_V2_ORDER* update_read_cache_glyph_v2_order(rdpUpdate* update, wStream* s,
                                                              UINT16 flags)
{
	UINT32 i;
	CACHE_GLYPH_V2_ORDER* cache_glyph_v2 = calloc(1, sizeof(CACHE_GLYPH_V2_ORDER));

	if (!cache_glyph_v2)
		goto fail;

	cache_glyph_v2->cacheId = (flags & 0x000F);
	cache_glyph_v2->flags = (flags & 0x00F0) >> 4;
	cache_glyph_v2->cGlyphs = (flags & 0xFF00) >> 8;

	for (i = 0; i < cache_glyph_v2->cGlyphs; i++)
	{
		GLYPH_DATA_V2* glyph = &cache_glyph_v2->glyphData[i];

		if (Stream_GetRemainingLength(s) < 1)
			goto fail;

		Stream_Read_UINT8(s, glyph->cacheIndex);

		if (!update_read_2byte_signed(s, &glyph->x) || !update_read_2byte_signed(s, &glyph->y) ||
		    !update_read_2byte_unsigned(s, &glyph->cx) ||
		    !update_read_2byte_unsigned(s, &glyph->cy))
		{
			goto fail;
		}

		glyph->cb = ((glyph->cx + 7) / 8) * glyph->cy;
		glyph->cb += ((glyph->cb % 4) > 0) ? 4 - (glyph->cb % 4) : 0;

		if (Stream_GetRemainingLength(s) < glyph->cb)
			goto fail;

		glyph->aj = (BYTE*)malloc(glyph->cb);

		if (!glyph->aj)
			goto fail;

		Stream_Read(s, glyph->aj, glyph->cb);
	}

	if ((flags & CG_GLYPH_UNICODE_PRESENT) && (cache_glyph_v2->cGlyphs > 0))
	{
		cache_glyph_v2->unicodeCharacters = calloc(cache_glyph_v2->cGlyphs, sizeof(WCHAR));

		if (!cache_glyph_v2->unicodeCharacters)
			goto fail;

		if (Stream_GetRemainingLength(s) < sizeof(WCHAR) * cache_glyph_v2->cGlyphs)
			goto fail;

		Stream_Read_UTF16_String(s, cache_glyph_v2->unicodeCharacters, cache_glyph_v2->cGlyphs);
	}

	return cache_glyph_v2;
fail:
	free_cache_glyph_v2_order(update->context, cache_glyph_v2);
	return NULL;
}
int update_approximate_cache_glyph_v2_order(const CACHE_GLYPH_V2_ORDER* cache_glyph_v2,
                                            UINT16* flags)
{
	return 8 + cache_glyph_v2->cGlyphs * 32;
}
BOOL update_write_cache_glyph_v2_order(wStream* s, const CACHE_GLYPH_V2_ORDER* cache_glyph_v2,
                                       UINT16* flags)
{
	UINT32 i, inf;
	inf = update_approximate_cache_glyph_v2_order(cache_glyph_v2, flags);

	if (!Stream_EnsureRemainingCapacity(s, inf))
		return FALSE;

	*flags = (cache_glyph_v2->cacheId & 0x000F) | ((cache_glyph_v2->flags & 0x000F) << 4) |
	         ((cache_glyph_v2->cGlyphs & 0x00FF) << 8);

	for (i = 0; i < cache_glyph_v2->cGlyphs; i++)
	{
		UINT32 cb;
		const GLYPH_DATA_V2* glyph = &cache_glyph_v2->glyphData[i];
		Stream_Write_UINT8(s, glyph->cacheIndex);

		if (!update_write_2byte_signed(s, glyph->x) || !update_write_2byte_signed(s, glyph->y) ||
		    !update_write_2byte_unsigned(s, glyph->cx) ||
		    !update_write_2byte_unsigned(s, glyph->cy))
		{
			return FALSE;
		}

		cb = ((glyph->cx + 7) / 8) * glyph->cy;
		cb += ((cb % 4) > 0) ? 4 - (cb % 4) : 0;
		Stream_Write(s, glyph->aj, cb);
	}

	if (*flags & CG_GLYPH_UNICODE_PRESENT)
	{
		Stream_Zero(s, cache_glyph_v2->cGlyphs * 2);
	}

	return TRUE;
}
static BOOL update_decompress_brush(wStream* s, BYTE* output, BYTE bpp)
{
	int index;
	int x, y, k;
	BYTE byte = 0;
	BYTE* palette;
	int bytesPerPixel;
	palette = Stream_Pointer(s) + 16;
	bytesPerPixel = ((bpp + 1) / 8);

	if (Stream_GetRemainingLength(s) < 16) // 64 / 4
		return FALSE;

	for (y = 7; y >= 0; y--)
	{
		for (x = 0; x < 8; x++)
		{
			if ((x % 4) == 0)
				Stream_Read_UINT8(s, byte);

			index = ((byte >> ((3 - (x % 4)) * 2)) & 0x03);

			for (k = 0; k < bytesPerPixel; k++)
			{
				output[((y * 8 + x) * bytesPerPixel) + k] = palette[(index * bytesPerPixel) + k];
			}
		}
	}

	return TRUE;
}
static BOOL update_compress_brush(wStream* s, const BYTE* input, BYTE bpp)
{
	return FALSE;
}
static CACHE_BRUSH_ORDER* update_read_cache_brush_order(rdpUpdate* update, wStream* s, UINT16 flags)
{
	int i;
	BYTE iBitmapFormat;
	BOOL compressed = FALSE;
	CACHE_BRUSH_ORDER* cache_brush = calloc(1, sizeof(CACHE_BRUSH_ORDER));

	if (!cache_brush)
		goto fail;

	if (Stream_GetRemainingLength(s) < 6)
		goto fail;

	Stream_Read_UINT8(s, cache_brush->index); /* cacheEntry (1 byte) */
	Stream_Read_UINT8(s, iBitmapFormat);      /* iBitmapFormat (1 byte) */

	if (iBitmapFormat > ARRAYSIZE(BMF_BPP))
		goto fail;

	cache_brush->bpp = BMF_BPP[iBitmapFormat];
	Stream_Read_UINT8(s, cache_brush->cx);     /* cx (1 byte) */
	Stream_Read_UINT8(s, cache_brush->cy);     /* cy (1 byte) */
	Stream_Read_UINT8(s, cache_brush->style);  /* style (1 byte) */
	Stream_Read_UINT8(s, cache_brush->length); /* iBytes (1 byte) */

	if ((cache_brush->cx == 8) && (cache_brush->cy == 8))
	{
		if (cache_brush->bpp == 1)
		{
			if (cache_brush->length != 8)
			{
				WLog_Print(update->log, WLOG_ERROR, "incompatible 1bpp brush of length:%" PRIu32 "",
				           cache_brush->length);
				goto fail;
			}

			/* rows are encoded in reverse order */
			if (Stream_GetRemainingLength(s) < 8)
				goto fail;

			for (i = 7; i >= 0; i--)
			{
				Stream_Read_UINT8(s, cache_brush->data[i]);
			}
		}
		else
		{
			if ((iBitmapFormat == BMF_8BPP) && (cache_brush->length == 20))
				compressed = TRUE;
			else if ((iBitmapFormat == BMF_16BPP) && (cache_brush->length == 24))
				compressed = TRUE;
			else if ((iBitmapFormat == BMF_32BPP) && (cache_brush->length == 32))
				compressed = TRUE;

			if (compressed != FALSE)
			{
				/* compressed brush */
				if (!update_decompress_brush(s, cache_brush->data, cache_brush->bpp))
					goto fail;
			}
			else
			{
				/* uncompressed brush */
				UINT32 scanline = (cache_brush->bpp / 8) * 8;

				if (Stream_GetRemainingLength(s) < scanline * 8)
					goto fail;

				for (i = 7; i >= 0; i--)
				{
					Stream_Read(s, &cache_brush->data[i * scanline], scanline);
				}
			}
		}
	}

	return cache_brush;
fail:
	free_cache_brush_order(update->context, cache_brush);
	return NULL;
}
int update_approximate_cache_brush_order(const CACHE_BRUSH_ORDER* cache_brush, UINT16* flags)
{
	return 64;
}
BOOL update_write_cache_brush_order(wStream* s, const CACHE_BRUSH_ORDER* cache_brush, UINT16* flags)
{
	int i;
	BYTE iBitmapFormat;
	BOOL compressed = FALSE;

	if (!Stream_EnsureRemainingCapacity(s,
	                                    update_approximate_cache_brush_order(cache_brush, flags)))
		return FALSE;

	iBitmapFormat = BPP_BMF[cache_brush->bpp];
	Stream_Write_UINT8(s, cache_brush->index);  /* cacheEntry (1 byte) */
	Stream_Write_UINT8(s, iBitmapFormat);       /* iBitmapFormat (1 byte) */
	Stream_Write_UINT8(s, cache_brush->cx);     /* cx (1 byte) */
	Stream_Write_UINT8(s, cache_brush->cy);     /* cy (1 byte) */
	Stream_Write_UINT8(s, cache_brush->style);  /* style (1 byte) */
	Stream_Write_UINT8(s, cache_brush->length); /* iBytes (1 byte) */

	if ((cache_brush->cx == 8) && (cache_brush->cy == 8))
	{
		if (cache_brush->bpp == 1)
		{
			if (cache_brush->length != 8)
			{
				WLog_ERR(TAG, "incompatible 1bpp brush of length:%" PRIu32 "", cache_brush->length);
				return FALSE;
			}

			for (i = 7; i >= 0; i--)
			{
				Stream_Write_UINT8(s, cache_brush->data[i]);
			}
		}
		else
		{
			if ((iBitmapFormat == BMF_8BPP) && (cache_brush->length == 20))
				compressed = TRUE;
			else if ((iBitmapFormat == BMF_16BPP) && (cache_brush->length == 24))
				compressed = TRUE;
			else if ((iBitmapFormat == BMF_32BPP) && (cache_brush->length == 32))
				compressed = TRUE;

			if (compressed != FALSE)
			{
				/* compressed brush */
				if (!update_compress_brush(s, cache_brush->data, cache_brush->bpp))
					return FALSE;
			}
			else
			{
				/* uncompressed brush */
				int scanline = (cache_brush->bpp / 8) * 8;

				for (i = 7; i >= 0; i--)
				{
					Stream_Write(s, &cache_brush->data[i * scanline], scanline);
				}
			}
		}
	}

	return TRUE;
}
/* Alternate Secondary Drawing Orders */
static BOOL
update_read_create_offscreen_bitmap_order(wStream* s,
                                          CREATE_OFFSCREEN_BITMAP_ORDER* create_offscreen_bitmap)
{
	UINT16 flags;
	BOOL deleteListPresent;
	OFFSCREEN_DELETE_LIST* deleteList;

	if (Stream_GetRemainingLength(s) < 6)
		return FALSE;

	Stream_Read_UINT16(s, flags); /* flags (2 bytes) */
	create_offscreen_bitmap->id = flags & 0x7FFF;
	deleteListPresent = (flags & 0x8000) ? TRUE : FALSE;
	Stream_Read_UINT16(s, create_offscreen_bitmap->cx); /* cx (2 bytes) */
	Stream_Read_UINT16(s, create_offscreen_bitmap->cy); /* cy (2 bytes) */
	deleteList = &(create_offscreen_bitmap->deleteList);

	if (deleteListPresent)
	{
		UINT32 i;

		if (Stream_GetRemainingLength(s) < 2)
			return FALSE;

		Stream_Read_UINT16(s, deleteList->cIndices);

		if (deleteList->cIndices > deleteList->sIndices)
		{
			UINT16* new_indices;
			new_indices = (UINT16*)realloc(deleteList->indices, deleteList->cIndices * 2);

			if (!new_indices)
				return FALSE;

			deleteList->sIndices = deleteList->cIndices;
			deleteList->indices = new_indices;
		}

		if (Stream_GetRemainingLength(s) < 2 * deleteList->cIndices)
			return FALSE;

		for (i = 0; i < deleteList->cIndices; i++)
		{
			Stream_Read_UINT16(s, deleteList->indices[i]);
		}
	}
	else
	{
		deleteList->cIndices = 0;
	}

	return TRUE;
}
int update_approximate_create_offscreen_bitmap_order(
    const CREATE_OFFSCREEN_BITMAP_ORDER* create_offscreen_bitmap)
{
	const OFFSCREEN_DELETE_LIST* deleteList = &(create_offscreen_bitmap->deleteList);
	return 32 + deleteList->cIndices * 2;
}
BOOL update_write_create_offscreen_bitmap_order(
    wStream* s, const CREATE_OFFSCREEN_BITMAP_ORDER* create_offscreen_bitmap)
{
	UINT16 flags;
	BOOL deleteListPresent;
	const OFFSCREEN_DELETE_LIST* deleteList;

	if (!Stream_EnsureRemainingCapacity(
	        s, update_approximate_create_offscreen_bitmap_order(create_offscreen_bitmap)))
		return FALSE;

	deleteList = &(create_offscreen_bitmap->deleteList);
	flags = create_offscreen_bitmap->id & 0x7FFF;
	deleteListPresent = (deleteList->cIndices > 0) ? TRUE : FALSE;

	if (deleteListPresent)
		flags |= 0x8000;

	Stream_Write_UINT16(s, flags);                       /* flags (2 bytes) */
	Stream_Write_UINT16(s, create_offscreen_bitmap->cx); /* cx (2 bytes) */
	Stream_Write_UINT16(s, create_offscreen_bitmap->cy); /* cy (2 bytes) */

	if (deleteListPresent)
	{
		int i;
		Stream_Write_UINT16(s, deleteList->cIndices);

		for (i = 0; i < (int)deleteList->cIndices; i++)
		{
			Stream_Write_UINT16(s, deleteList->indices[i]);
		}
	}

	return TRUE;
}
static BOOL update_read_switch_surface_order(wStream* s, SWITCH_SURFACE_ORDER* switch_surface)
{
	if (Stream_GetRemainingLength(s) < 2)
		return FALSE;

	Stream_Read_UINT16(s, switch_surface->bitmapId); /* bitmapId (2 bytes) */
	return TRUE;
}
int update_approximate_switch_surface_order(const SWITCH_SURFACE_ORDER* switch_surface)
{
	return 2;
}
BOOL update_write_switch_surface_order(wStream* s, const SWITCH_SURFACE_ORDER* switch_surface)
{
	int inf = update_approximate_switch_surface_order(switch_surface);

	if (!Stream_EnsureRemainingCapacity(s, inf))
		return FALSE;

	Stream_Write_UINT16(s, switch_surface->bitmapId); /* bitmapId (2 bytes) */
	return TRUE;
}
static BOOL
update_read_create_nine_grid_bitmap_order(wStream* s,
                                          CREATE_NINE_GRID_BITMAP_ORDER* create_nine_grid_bitmap)
{
	NINE_GRID_BITMAP_INFO* nineGridInfo;

	if (Stream_GetRemainingLength(s) < 19)
		return FALSE;

	Stream_Read_UINT8(s, create_nine_grid_bitmap->bitmapBpp); /* bitmapBpp (1 byte) */

	if ((create_nine_grid_bitmap->bitmapBpp < 1) || (create_nine_grid_bitmap->bitmapBpp > 32))
	{
		WLog_ERR(TAG, "invalid bpp value %" PRIu32 "", create_nine_grid_bitmap->bitmapBpp);
		return FALSE;
	}

	Stream_Read_UINT16(s, create_nine_grid_bitmap->bitmapId); /* bitmapId (2 bytes) */
	nineGridInfo = &(create_nine_grid_bitmap->nineGridInfo);
	Stream_Read_UINT32(s, nineGridInfo->flFlags);          /* flFlags (4 bytes) */
	Stream_Read_UINT16(s, nineGridInfo->ulLeftWidth);      /* ulLeftWidth (2 bytes) */
	Stream_Read_UINT16(s, nineGridInfo->ulRightWidth);     /* ulRightWidth (2 bytes) */
	Stream_Read_UINT16(s, nineGridInfo->ulTopHeight);      /* ulTopHeight (2 bytes) */
	Stream_Read_UINT16(s, nineGridInfo->ulBottomHeight);   /* ulBottomHeight (2 bytes) */
	update_read_colorref(s, &nineGridInfo->crTransparent); /* crTransparent (4 bytes) */
	return TRUE;
}
static BOOL update_read_frame_marker_order(wStream* s, FRAME_MARKER_ORDER* frame_marker)
{
	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT32(s, frame_marker->action); /* action (4 bytes) */
	return TRUE;
}
static BOOL update_read_stream_bitmap_first_order(wStream* s,
                                                  STREAM_BITMAP_FIRST_ORDER* stream_bitmap_first)
{
	if (Stream_GetRemainingLength(s) < 10) // 8 + 2 at least
		return FALSE;

	Stream_Read_UINT8(s, stream_bitmap_first->bitmapFlags); /* bitmapFlags (1 byte) */
	Stream_Read_UINT8(s, stream_bitmap_first->bitmapBpp);   /* bitmapBpp (1 byte) */

	if ((stream_bitmap_first->bitmapBpp < 1) || (stream_bitmap_first->bitmapBpp > 32))
	{
		WLog_ERR(TAG, "invalid bpp value %" PRIu32 "", stream_bitmap_first->bitmapBpp);
		return FALSE;
	}

	Stream_Read_UINT16(s, stream_bitmap_first->bitmapType);   /* bitmapType (2 bytes) */
	Stream_Read_UINT16(s, stream_bitmap_first->bitmapWidth);  /* bitmapWidth (2 bytes) */
	Stream_Read_UINT16(s, stream_bitmap_first->bitmapHeight); /* bitmapHeigth (2 bytes) */

	if (stream_bitmap_first->bitmapFlags & STREAM_BITMAP_V2)
	{
		if (Stream_GetRemainingLength(s) < 4)
			return FALSE;

		Stream_Read_UINT32(s, stream_bitmap_first->bitmapSize); /* bitmapSize (4 bytes) */
	}
	else
	{
		if (Stream_GetRemainingLength(s) < 2)
			return FALSE;

		Stream_Read_UINT16(s, stream_bitmap_first->bitmapSize); /* bitmapSize (2 bytes) */
	}

	FIELD_SKIP_BUFFER16(
	    s, stream_bitmap_first->bitmapBlockSize); /* bitmapBlockSize(2 bytes) + bitmapBlock */
	return TRUE;
}
static BOOL update_read_stream_bitmap_next_order(wStream* s,
                                                 STREAM_BITMAP_NEXT_ORDER* stream_bitmap_next)
{
	if (Stream_GetRemainingLength(s) < 5)
		return FALSE;

	Stream_Read_UINT8(s, stream_bitmap_next->bitmapFlags); /* bitmapFlags (1 byte) */
	Stream_Read_UINT16(s, stream_bitmap_next->bitmapType); /* bitmapType (2 bytes) */
	FIELD_SKIP_BUFFER16(
	    s, stream_bitmap_next->bitmapBlockSize); /* bitmapBlockSize(2 bytes) + bitmapBlock */
	return TRUE;
}
static BOOL update_read_draw_gdiplus_first_order(wStream* s,
                                                 DRAW_GDIPLUS_FIRST_ORDER* draw_gdiplus_first)
{
	if (Stream_GetRemainingLength(s) < 11)
		return FALSE;

	Stream_Seek_UINT8(s);                                      /* pad1Octet (1 byte) */
	Stream_Read_UINT16(s, draw_gdiplus_first->cbSize);         /* cbSize (2 bytes) */
	Stream_Read_UINT32(s, draw_gdiplus_first->cbTotalSize);    /* cbTotalSize (4 bytes) */
	Stream_Read_UINT32(s, draw_gdiplus_first->cbTotalEmfSize); /* cbTotalEmfSize (4 bytes) */
	return Stream_SafeSeek(s, draw_gdiplus_first->cbSize);     /* emfRecords */
}
static BOOL update_read_draw_gdiplus_next_order(wStream* s,
                                                DRAW_GDIPLUS_NEXT_ORDER* draw_gdiplus_next)
{
	if (Stream_GetRemainingLength(s) < 3)
		return FALSE;

	Stream_Seek_UINT8(s);                              /* pad1Octet (1 byte) */
	FIELD_SKIP_BUFFER16(s, draw_gdiplus_next->cbSize); /* cbSize(2 bytes) + emfRecords */
	return TRUE;
}
static BOOL update_read_draw_gdiplus_end_order(wStream* s, DRAW_GDIPLUS_END_ORDER* draw_gdiplus_end)
{
	if (Stream_GetRemainingLength(s) < 11)
		return FALSE;

	Stream_Seek_UINT8(s);                                    /* pad1Octet (1 byte) */
	Stream_Read_UINT16(s, draw_gdiplus_end->cbSize);         /* cbSize (2 bytes) */
	Stream_Read_UINT32(s, draw_gdiplus_end->cbTotalSize);    /* cbTotalSize (4 bytes) */
	Stream_Read_UINT32(s, draw_gdiplus_end->cbTotalEmfSize); /* cbTotalEmfSize (4 bytes) */
	return Stream_SafeSeek(s, draw_gdiplus_end->cbSize);     /* emfRecords */
}
static BOOL
update_read_draw_gdiplus_cache_first_order(wStream* s,
                                           DRAW_GDIPLUS_CACHE_FIRST_ORDER* draw_gdiplus_cache_first)
{
	if (Stream_GetRemainingLength(s) < 11)
		return FALSE;

	Stream_Read_UINT8(s, draw_gdiplus_cache_first->flags);        /* flags (1 byte) */
	Stream_Read_UINT16(s, draw_gdiplus_cache_first->cacheType);   /* cacheType (2 bytes) */
	Stream_Read_UINT16(s, draw_gdiplus_cache_first->cacheIndex);  /* cacheIndex (2 bytes) */
	Stream_Read_UINT16(s, draw_gdiplus_cache_first->cbSize);      /* cbSize (2 bytes) */
	Stream_Read_UINT32(s, draw_gdiplus_cache_first->cbTotalSize); /* cbTotalSize (4 bytes) */
	return Stream_SafeSeek(s, draw_gdiplus_cache_first->cbSize);  /* emfRecords */
}
static BOOL
update_read_draw_gdiplus_cache_next_order(wStream* s,
                                          DRAW_GDIPLUS_CACHE_NEXT_ORDER* draw_gdiplus_cache_next)
{
	if (Stream_GetRemainingLength(s) < 7)
		return FALSE;

	Stream_Read_UINT8(s, draw_gdiplus_cache_next->flags);       /* flags (1 byte) */
	Stream_Read_UINT16(s, draw_gdiplus_cache_next->cacheType);  /* cacheType (2 bytes) */
	Stream_Read_UINT16(s, draw_gdiplus_cache_next->cacheIndex); /* cacheIndex (2 bytes) */
	FIELD_SKIP_BUFFER16(s, draw_gdiplus_cache_next->cbSize);    /* cbSize(2 bytes) + emfRecords */
	return TRUE;
}
static BOOL
update_read_draw_gdiplus_cache_end_order(wStream* s,
                                         DRAW_GDIPLUS_CACHE_END_ORDER* draw_gdiplus_cache_end)
{
	if (Stream_GetRemainingLength(s) < 11)
		return FALSE;

	Stream_Read_UINT8(s, draw_gdiplus_cache_end->flags);        /* flags (1 byte) */
	Stream_Read_UINT16(s, draw_gdiplus_cache_end->cacheType);   /* cacheType (2 bytes) */
	Stream_Read_UINT16(s, draw_gdiplus_cache_end->cacheIndex);  /* cacheIndex (2 bytes) */
	Stream_Read_UINT16(s, draw_gdiplus_cache_end->cbSize);      /* cbSize (2 bytes) */
	Stream_Read_UINT32(s, draw_gdiplus_cache_end->cbTotalSize); /* cbTotalSize (4 bytes) */
	return Stream_SafeSeek(s, draw_gdiplus_cache_end->cbSize);  /* emfRecords */
}
static BOOL update_read_field_flags(wStream* s, UINT32* fieldFlags, BYTE flags, BYTE fieldBytes)
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

	if (Stream_GetRemainingLength(s) < fieldBytes)
		return FALSE;

	*fieldFlags = 0;

	for (i = 0; i < fieldBytes; i++)
	{
		Stream_Read_UINT8(s, byte);
		*fieldFlags |= byte << (i * 8);
	}

	return TRUE;
}
BOOL update_write_field_flags(wStream* s, UINT32 fieldFlags, BYTE flags, BYTE fieldBytes)
{
	BYTE byte;

	if (fieldBytes == 1)
	{
		byte = fieldFlags & 0xFF;
		Stream_Write_UINT8(s, byte);
	}
	else if (fieldBytes == 2)
	{
		byte = fieldFlags & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (fieldFlags >> 8) & 0xFF;
		Stream_Write_UINT8(s, byte);
	}
	else if (fieldBytes == 3)
	{
		byte = fieldFlags & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (fieldFlags >> 8) & 0xFF;
		Stream_Write_UINT8(s, byte);
		byte = (fieldFlags >> 16) & 0xFF;
		Stream_Write_UINT8(s, byte);
	}
	else
	{
		return FALSE;
	}

	return TRUE;
}
static BOOL update_read_bounds(wStream* s, rdpBounds* bounds)
{
	BYTE flags;

	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, flags); /* field flags */

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
BOOL update_write_bounds(wStream* s, ORDER_INFO* orderInfo)
{
	if (!(orderInfo->controlFlags & ORDER_BOUNDS))
		return TRUE;

	if (orderInfo->controlFlags & ORDER_ZERO_BOUNDS_DELTAS)
		return TRUE;

	Stream_Write_UINT8(s, orderInfo->boundsFlags); /* field flags */

	if (orderInfo->boundsFlags & BOUND_LEFT)
	{
		if (!update_write_coord(s, orderInfo->bounds.left))
			return FALSE;
	}
	else if (orderInfo->boundsFlags & BOUND_DELTA_LEFT)
	{
	}

	if (orderInfo->boundsFlags & BOUND_TOP)
	{
		if (!update_write_coord(s, orderInfo->bounds.top))
			return FALSE;
	}
	else if (orderInfo->boundsFlags & BOUND_DELTA_TOP)
	{
	}

	if (orderInfo->boundsFlags & BOUND_RIGHT)
	{
		if (!update_write_coord(s, orderInfo->bounds.right))
			return FALSE;
	}
	else if (orderInfo->boundsFlags & BOUND_DELTA_RIGHT)
	{
	}

	if (orderInfo->boundsFlags & BOUND_BOTTOM)
	{
		if (!update_write_coord(s, orderInfo->bounds.bottom))
			return FALSE;
	}
	else if (orderInfo->boundsFlags & BOUND_DELTA_BOTTOM)
	{
	}

	return TRUE;
}

static BOOL read_primary_order(wLog* log, const char* orderName, wStream* s,
                               const ORDER_INFO* orderInfo, rdpPrimaryUpdate* primary)
{
	BOOL rc = FALSE;

	if (!s || !orderInfo || !primary || !orderName)
		return FALSE;

	switch (orderInfo->orderType)
	{
		case ORDER_TYPE_DSTBLT:
			rc = update_read_dstblt_order(s, orderInfo, &(primary->dstblt));
			break;

		case ORDER_TYPE_PATBLT:
			rc = update_read_patblt_order(s, orderInfo, &(primary->patblt));
			break;

		case ORDER_TYPE_SCRBLT:
			rc = update_read_scrblt_order(s, orderInfo, &(primary->scrblt));
			break;

		case ORDER_TYPE_OPAQUE_RECT:
			rc = update_read_opaque_rect_order(s, orderInfo, &(primary->opaque_rect));
			break;

		case ORDER_TYPE_DRAW_NINE_GRID:
			rc = update_read_draw_nine_grid_order(s, orderInfo, &(primary->draw_nine_grid));
			break;

		case ORDER_TYPE_MULTI_DSTBLT:
			rc = update_read_multi_dstblt_order(s, orderInfo, &(primary->multi_dstblt));
			break;

		case ORDER_TYPE_MULTI_PATBLT:
			rc = update_read_multi_patblt_order(s, orderInfo, &(primary->multi_patblt));
			break;

		case ORDER_TYPE_MULTI_SCRBLT:
			rc = update_read_multi_scrblt_order(s, orderInfo, &(primary->multi_scrblt));
			break;

		case ORDER_TYPE_MULTI_OPAQUE_RECT:
			rc = update_read_multi_opaque_rect_order(s, orderInfo, &(primary->multi_opaque_rect));
			break;

		case ORDER_TYPE_MULTI_DRAW_NINE_GRID:
			rc = update_read_multi_draw_nine_grid_order(s, orderInfo,
			                                            &(primary->multi_draw_nine_grid));
			break;

		case ORDER_TYPE_LINE_TO:
			rc = update_read_line_to_order(s, orderInfo, &(primary->line_to));
			break;

		case ORDER_TYPE_POLYLINE:
			rc = update_read_polyline_order(s, orderInfo, &(primary->polyline));
			break;

		case ORDER_TYPE_MEMBLT:
			rc = update_read_memblt_order(s, orderInfo, &(primary->memblt));
			break;

		case ORDER_TYPE_MEM3BLT:
			rc = update_read_mem3blt_order(s, orderInfo, &(primary->mem3blt));
			break;

		case ORDER_TYPE_SAVE_BITMAP:
			rc = update_read_save_bitmap_order(s, orderInfo, &(primary->save_bitmap));
			break;

		case ORDER_TYPE_GLYPH_INDEX:
			rc = update_read_glyph_index_order(s, orderInfo, &(primary->glyph_index));
			break;

		case ORDER_TYPE_FAST_INDEX:
			rc = update_read_fast_index_order(s, orderInfo, &(primary->fast_index));
			break;

		case ORDER_TYPE_FAST_GLYPH:
			rc = update_read_fast_glyph_order(s, orderInfo, &(primary->fast_glyph));
			break;

		case ORDER_TYPE_POLYGON_SC:
			rc = update_read_polygon_sc_order(s, orderInfo, &(primary->polygon_sc));
			break;

		case ORDER_TYPE_POLYGON_CB:
			rc = update_read_polygon_cb_order(s, orderInfo, &(primary->polygon_cb));
			break;

		case ORDER_TYPE_ELLIPSE_SC:
			rc = update_read_ellipse_sc_order(s, orderInfo, &(primary->ellipse_sc));
			break;

		case ORDER_TYPE_ELLIPSE_CB:
			rc = update_read_ellipse_cb_order(s, orderInfo, &(primary->ellipse_cb));
			break;

		default:
			WLog_Print(log, WLOG_WARN, "Primary Drawing Order %s not supported, ignoring",
			           orderName);
			rc = TRUE;
			break;
	}

	if (!rc)
	{
		WLog_Print(log, WLOG_ERROR, "%s - update_read_dstblt_order() failed", orderName);
		return FALSE;
	}

	return TRUE;
}

static BOOL update_recv_primary_order(rdpUpdate* update, wStream* s, BYTE flags)
{
	BOOL rc = FALSE;
	rdpContext* context = update->context;
	rdpPrimaryUpdate* primary = update->primary;
	ORDER_INFO* orderInfo = &(primary->order_info);
	rdpSettings* settings = context->settings;
	const char* orderName;

	if (flags & ORDER_TYPE_CHANGE)
	{
		if (Stream_GetRemainingLength(s) < 1)
		{
			WLog_Print(update->log, WLOG_ERROR, "Stream_GetRemainingLength(s) < 1");
			return FALSE;
		}

		Stream_Read_UINT8(s, orderInfo->orderType); /* orderType (1 byte) */
	}

	orderName = primary_order_string(orderInfo->orderType);

	if (!check_primary_order_supported(update->log, settings, orderInfo->orderType, orderName))
		return FALSE;

	if (!update_read_field_flags(s, &(orderInfo->fieldFlags), flags,
	                             PRIMARY_DRAWING_ORDER_FIELD_BYTES[orderInfo->orderType]))
	{
		WLog_Print(update->log, WLOG_ERROR, "update_read_field_flags() failed");
		return FALSE;
	}

	if (flags & ORDER_BOUNDS)
	{
		if (!(flags & ORDER_ZERO_BOUNDS_DELTAS))
		{
			if (!update_read_bounds(s, &orderInfo->bounds))
			{
				WLog_Print(update->log, WLOG_ERROR, "update_read_bounds() failed");
				return FALSE;
			}
		}

		rc = IFCALLRESULT(FALSE, update->SetBounds, context, &orderInfo->bounds);

		if (!rc)
			return FALSE;
	}

	orderInfo->deltaCoordinates = (flags & ORDER_DELTA_COORDINATES) ? TRUE : FALSE;

	if (!read_primary_order(update->log, orderName, s, orderInfo, primary))
		return FALSE;

	switch (orderInfo->orderType)
	{
		case ORDER_TYPE_DSTBLT:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s rop=%s [0x%08" PRIx32 "]",
			           orderName, gdi_rop3_code_string(primary->dstblt.bRop),
			           gdi_rop3_code(primary->dstblt.bRop));
			rc = IFCALLRESULT(FALSE, primary->DstBlt, context, &primary->dstblt);
		}
		break;

		case ORDER_TYPE_PATBLT:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s rop=%s [0x%08" PRIx32 "]",
			           orderName, gdi_rop3_code_string(primary->patblt.bRop),
			           gdi_rop3_code(primary->patblt.bRop));
			rc = IFCALLRESULT(FALSE, primary->PatBlt, context, &primary->patblt);
		}
		break;

		case ORDER_TYPE_SCRBLT:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s rop=%s [0x%08" PRIx32 "]",
			           orderName, gdi_rop3_code_string(primary->scrblt.bRop),
			           gdi_rop3_code(primary->scrblt.bRop));
			rc = IFCALLRESULT(FALSE, primary->ScrBlt, context, &primary->scrblt);
		}
		break;

		case ORDER_TYPE_OPAQUE_RECT:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s", orderName);
			rc = IFCALLRESULT(FALSE, primary->OpaqueRect, context, &primary->opaque_rect);
		}
		break;

		case ORDER_TYPE_DRAW_NINE_GRID:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s", orderName);
			rc = IFCALLRESULT(FALSE, primary->DrawNineGrid, context, &primary->draw_nine_grid);
		}
		break;

		case ORDER_TYPE_MULTI_DSTBLT:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s rop=%s [0x%08" PRIx32 "]",
			           orderName, gdi_rop3_code_string(primary->multi_dstblt.bRop),
			           gdi_rop3_code(primary->multi_dstblt.bRop));
			rc = IFCALLRESULT(FALSE, primary->MultiDstBlt, context, &primary->multi_dstblt);
		}
		break;

		case ORDER_TYPE_MULTI_PATBLT:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s rop=%s [0x%08" PRIx32 "]",
			           orderName, gdi_rop3_code_string(primary->multi_patblt.bRop),
			           gdi_rop3_code(primary->multi_patblt.bRop));
			rc = IFCALLRESULT(FALSE, primary->MultiPatBlt, context, &primary->multi_patblt);
		}
		break;

		case ORDER_TYPE_MULTI_SCRBLT:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s rop=%s [0x%08" PRIx32 "]",
			           orderName, gdi_rop3_code_string(primary->multi_scrblt.bRop),
			           gdi_rop3_code(primary->multi_scrblt.bRop));
			rc = IFCALLRESULT(FALSE, primary->MultiScrBlt, context, &primary->multi_scrblt);
		}
		break;

		case ORDER_TYPE_MULTI_OPAQUE_RECT:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s", orderName);
			rc =
			    IFCALLRESULT(FALSE, primary->MultiOpaqueRect, context, &primary->multi_opaque_rect);
		}
		break;

		case ORDER_TYPE_MULTI_DRAW_NINE_GRID:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s", orderName);
			rc = IFCALLRESULT(FALSE, primary->MultiDrawNineGrid, context,
			                  &primary->multi_draw_nine_grid);
		}
		break;

		case ORDER_TYPE_LINE_TO:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s", orderName);
			rc = IFCALLRESULT(FALSE, primary->LineTo, context, &primary->line_to);
		}
		break;

		case ORDER_TYPE_POLYLINE:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s", orderName);
			rc = IFCALLRESULT(FALSE, primary->Polyline, context, &primary->polyline);
		}
		break;

		case ORDER_TYPE_MEMBLT:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s rop=%s [0x%08" PRIx32 "]",
			           orderName, gdi_rop3_code_string(primary->memblt.bRop),
			           gdi_rop3_code(primary->memblt.bRop));
			rc = IFCALLRESULT(FALSE, primary->MemBlt, context, &primary->memblt);
		}
		break;

		case ORDER_TYPE_MEM3BLT:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s rop=%s [0x%08" PRIx32 "]",
			           orderName, gdi_rop3_code_string(primary->mem3blt.bRop),
			           gdi_rop3_code(primary->mem3blt.bRop));
			rc = IFCALLRESULT(FALSE, primary->Mem3Blt, context, &primary->mem3blt);
		}
		break;

		case ORDER_TYPE_SAVE_BITMAP:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s", orderName);
			rc = IFCALLRESULT(FALSE, primary->SaveBitmap, context, &primary->save_bitmap);
		}
		break;

		case ORDER_TYPE_GLYPH_INDEX:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s", orderName);
			rc = IFCALLRESULT(FALSE, primary->GlyphIndex, context, &primary->glyph_index);
		}
		break;

		case ORDER_TYPE_FAST_INDEX:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s", orderName);
			rc = IFCALLRESULT(FALSE, primary->FastIndex, context, &primary->fast_index);
		}
		break;

		case ORDER_TYPE_FAST_GLYPH:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s", orderName);
			rc = IFCALLRESULT(FALSE, primary->FastGlyph, context, &primary->fast_glyph);
		}
		break;

		case ORDER_TYPE_POLYGON_SC:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s", orderName);
			rc = IFCALLRESULT(FALSE, primary->PolygonSC, context, &primary->polygon_sc);
		}
		break;

		case ORDER_TYPE_POLYGON_CB:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s", orderName);
			rc = IFCALLRESULT(FALSE, primary->PolygonCB, context, &primary->polygon_cb);
		}
		break;

		case ORDER_TYPE_ELLIPSE_SC:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s", orderName);
			rc = IFCALLRESULT(FALSE, primary->EllipseSC, context, &primary->ellipse_sc);
		}
		break;

		case ORDER_TYPE_ELLIPSE_CB:
		{
			WLog_Print(update->log, WLOG_DEBUG, "Primary Drawing Order %s", orderName);
			rc = IFCALLRESULT(FALSE, primary->EllipseCB, context, &primary->ellipse_cb);
		}
		break;

		default:
			WLog_Print(update->log, WLOG_WARN, "Primary Drawing Order %s not supported", orderName);
			break;
	}

	if (!rc)
	{
		WLog_Print(update->log, WLOG_WARN, "Primary Drawing Order %s failed", orderName);
		return FALSE;
	}

	if (flags & ORDER_BOUNDS)
	{
		rc = IFCALLRESULT(FALSE, update->SetBounds, context, NULL);
	}

	return rc;
}

static BOOL update_recv_secondary_order(rdpUpdate* update, wStream* s, BYTE flags)
{
	BOOL rc = FALSE;
	BYTE* next;
	BYTE orderType;
	UINT16 extraFlags;
	UINT16 orderLength;
	rdpContext* context = update->context;
	rdpSettings* settings = context->settings;
	rdpSecondaryUpdate* secondary = update->secondary;
	const char* name;

	if (Stream_GetRemainingLength(s) < 5)
	{
		WLog_Print(update->log, WLOG_ERROR, "Stream_GetRemainingLength(s) < 5");
		return FALSE;
	}

	Stream_Read_UINT16(s, orderLength); /* orderLength (2 bytes) */
	Stream_Read_UINT16(s, extraFlags);  /* extraFlags (2 bytes) */
	Stream_Read_UINT8(s, orderType);    /* orderType (1 byte) */
	next = Stream_Pointer(s) + ((INT16)orderLength) + 7;
	name = secondary_order_string(orderType);
	WLog_Print(update->log, WLOG_DEBUG, "Secondary Drawing Order %s", name);

	if (!check_secondary_order_supported(update->log, settings, orderType, name))
		return FALSE;

	switch (orderType)
	{
		case ORDER_TYPE_BITMAP_UNCOMPRESSED:
		case ORDER_TYPE_CACHE_BITMAP_COMPRESSED:
		{
			const BOOL compressed = (orderType == ORDER_TYPE_CACHE_BITMAP_COMPRESSED);
			CACHE_BITMAP_ORDER* order =
			    update_read_cache_bitmap_order(update, s, compressed, extraFlags);

			if (order)
			{
				rc = IFCALLRESULT(FALSE, secondary->CacheBitmap, context, order);
				free_cache_bitmap_order(context, order);
			}
		}
		break;

		case ORDER_TYPE_BITMAP_UNCOMPRESSED_V2:
		case ORDER_TYPE_BITMAP_COMPRESSED_V2:
		{
			const BOOL compressed = (orderType == ORDER_TYPE_BITMAP_COMPRESSED_V2);
			CACHE_BITMAP_V2_ORDER* order =
			    update_read_cache_bitmap_v2_order(update, s, compressed, extraFlags);

			if (order)
			{
				rc = IFCALLRESULT(FALSE, secondary->CacheBitmapV2, context, order);
				free_cache_bitmap_v2_order(context, order);
			}
		}
		break;

		case ORDER_TYPE_BITMAP_COMPRESSED_V3:
		{
			CACHE_BITMAP_V3_ORDER* order = update_read_cache_bitmap_v3_order(update, s, extraFlags);

			if (order)
			{
				rc = IFCALLRESULT(FALSE, secondary->CacheBitmapV3, context, order);
				free_cache_bitmap_v3_order(context, order);
			}
		}
		break;

		case ORDER_TYPE_CACHE_COLOR_TABLE:
		{
			CACHE_COLOR_TABLE_ORDER* order =
			    update_read_cache_color_table_order(update, s, extraFlags);

			if (order)
			{
				rc = IFCALLRESULT(FALSE, secondary->CacheColorTable, context, order);
				free_cache_color_table_order(context, order);
			}
		}
		break;

		case ORDER_TYPE_CACHE_GLYPH:
		{
			switch (settings->GlyphSupportLevel)
			{
				case GLYPH_SUPPORT_PARTIAL:
				case GLYPH_SUPPORT_FULL:
				{
					CACHE_GLYPH_ORDER* order = update_read_cache_glyph_order(update, s, extraFlags);

					if (order)
					{
						rc = IFCALLRESULT(FALSE, secondary->CacheGlyph, context, order);
						free_cache_glyph_order(context, order);
					}
				}
				break;

				case GLYPH_SUPPORT_ENCODE:
				{
					CACHE_GLYPH_V2_ORDER* order =
					    update_read_cache_glyph_v2_order(update, s, extraFlags);

					if (order)
					{
						rc = IFCALLRESULT(FALSE, secondary->CacheGlyphV2, context, order);
						free_cache_glyph_v2_order(context, order);
					}
				}
				break;

				case GLYPH_SUPPORT_NONE:
				default:
					break;
			}
		}
		break;

		case ORDER_TYPE_CACHE_BRUSH:
			/* [MS-RDPEGDI] 2.2.2.2.1.2.7 Cache Brush (CACHE_BRUSH_ORDER) */
			{
				CACHE_BRUSH_ORDER* order = update_read_cache_brush_order(update, s, extraFlags);

				if (order)
				{
					rc = IFCALLRESULT(FALSE, secondary->CacheBrush, context, order);
					free_cache_brush_order(context, order);
				}
			}
			break;

		default:
			WLog_Print(update->log, WLOG_WARN, "SECONDARY ORDER %s not supported", name);
			break;
	}

	if (!rc)
	{
		WLog_Print(update->log, WLOG_ERROR, "SECONDARY ORDER %s failed", name);
	}

	Stream_SetPointer(s, next);
	return rc;
}

static BOOL read_altsec_order(wStream* s, BYTE orderType, rdpAltSecUpdate* altsec)
{
	BOOL rc = FALSE;

	switch (orderType)
	{
		case ORDER_TYPE_CREATE_OFFSCREEN_BITMAP:
			rc = update_read_create_offscreen_bitmap_order(s, &(altsec->create_offscreen_bitmap));
			break;

		case ORDER_TYPE_SWITCH_SURFACE:
			rc = update_read_switch_surface_order(s, &(altsec->switch_surface));
			break;

		case ORDER_TYPE_CREATE_NINE_GRID_BITMAP:
			rc = update_read_create_nine_grid_bitmap_order(s, &(altsec->create_nine_grid_bitmap));
			break;

		case ORDER_TYPE_FRAME_MARKER:
			rc = update_read_frame_marker_order(s, &(altsec->frame_marker));
			break;

		case ORDER_TYPE_STREAM_BITMAP_FIRST:
			rc = update_read_stream_bitmap_first_order(s, &(altsec->stream_bitmap_first));
			break;

		case ORDER_TYPE_STREAM_BITMAP_NEXT:
			rc = update_read_stream_bitmap_next_order(s, &(altsec->stream_bitmap_next));
			break;

		case ORDER_TYPE_GDIPLUS_FIRST:
			rc = update_read_draw_gdiplus_first_order(s, &(altsec->draw_gdiplus_first));
			break;

		case ORDER_TYPE_GDIPLUS_NEXT:
			rc = update_read_draw_gdiplus_next_order(s, &(altsec->draw_gdiplus_next));
			break;

		case ORDER_TYPE_GDIPLUS_END:
			rc = update_read_draw_gdiplus_end_order(s, &(altsec->draw_gdiplus_end));
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_FIRST:
			rc = update_read_draw_gdiplus_cache_first_order(s, &(altsec->draw_gdiplus_cache_first));
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_NEXT:
			rc = update_read_draw_gdiplus_cache_next_order(s, &(altsec->draw_gdiplus_cache_next));
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_END:
			rc = update_read_draw_gdiplus_cache_end_order(s, &(altsec->draw_gdiplus_cache_end));
			break;

		case ORDER_TYPE_WINDOW:
			/* This order is handled elsewhere. */
			rc = TRUE;
			break;

		case ORDER_TYPE_COMPDESK_FIRST:
			rc = TRUE;
			break;

		default:
			break;
	}

	return rc;
}

static BOOL update_recv_altsec_order(rdpUpdate* update, wStream* s, BYTE flags)
{
	BYTE orderType = flags >>= 2; /* orderType is in higher 6 bits of flags field */
	BOOL rc = FALSE;
	rdpContext* context = update->context;
	rdpSettings* settings = context->settings;
	rdpAltSecUpdate* altsec = update->altsec;
	const char* orderName = altsec_order_string(orderType);
	WLog_Print(update->log, WLOG_DEBUG, "Alternate Secondary Drawing Order %s", orderName);

	if (!check_alt_order_supported(update->log, settings, orderType, orderName))
		return FALSE;

	if (!read_altsec_order(s, orderType, altsec))
		return FALSE;

	switch (orderType)
	{
		case ORDER_TYPE_CREATE_OFFSCREEN_BITMAP:
			IFCALLRET(altsec->CreateOffscreenBitmap, rc, context,
			          &(altsec->create_offscreen_bitmap));
			break;

		case ORDER_TYPE_SWITCH_SURFACE:
			IFCALLRET(altsec->SwitchSurface, rc, context, &(altsec->switch_surface));
			break;

		case ORDER_TYPE_CREATE_NINE_GRID_BITMAP:
			IFCALLRET(altsec->CreateNineGridBitmap, rc, context,
			          &(altsec->create_nine_grid_bitmap));
			break;

		case ORDER_TYPE_FRAME_MARKER:
			IFCALLRET(altsec->FrameMarker, rc, context, &(altsec->frame_marker));
			break;

		case ORDER_TYPE_STREAM_BITMAP_FIRST:
			IFCALLRET(altsec->StreamBitmapFirst, rc, context, &(altsec->stream_bitmap_first));
			break;

		case ORDER_TYPE_STREAM_BITMAP_NEXT:
			IFCALLRET(altsec->StreamBitmapNext, rc, context, &(altsec->stream_bitmap_next));
			break;

		case ORDER_TYPE_GDIPLUS_FIRST:
			IFCALLRET(altsec->DrawGdiPlusFirst, rc, context, &(altsec->draw_gdiplus_first));
			break;

		case ORDER_TYPE_GDIPLUS_NEXT:
			IFCALLRET(altsec->DrawGdiPlusNext, rc, context, &(altsec->draw_gdiplus_next));
			break;

		case ORDER_TYPE_GDIPLUS_END:
			IFCALLRET(altsec->DrawGdiPlusEnd, rc, context, &(altsec->draw_gdiplus_end));
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_FIRST:
			IFCALLRET(altsec->DrawGdiPlusCacheFirst, rc, context,
			          &(altsec->draw_gdiplus_cache_first));
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_NEXT:
			IFCALLRET(altsec->DrawGdiPlusCacheNext, rc, context,
			          &(altsec->draw_gdiplus_cache_next));
			break;

		case ORDER_TYPE_GDIPLUS_CACHE_END:
			IFCALLRET(altsec->DrawGdiPlusCacheEnd, rc, context, &(altsec->draw_gdiplus_cache_end));
			break;

		case ORDER_TYPE_WINDOW:
			rc = update_recv_altsec_window_order(update, s);
			break;

		case ORDER_TYPE_COMPDESK_FIRST:
			rc = TRUE;
			break;

		default:
			break;
	}

	if (!rc)
	{
		WLog_Print(update->log, WLOG_WARN, "Alternate Secondary Drawing Order %s failed",
		           orderName);
	}

	return rc;
}
BOOL update_recv_order(rdpUpdate* update, wStream* s)
{
	BOOL rc;
	BYTE controlFlags;

	if (Stream_GetRemainingLength(s) < 1)
	{
		WLog_Print(update->log, WLOG_ERROR, "Stream_GetRemainingLength(s) < 1");
		return FALSE;
	}

	Stream_Read_UINT8(s, controlFlags); /* controlFlags (1 byte) */

	if (!(controlFlags & ORDER_STANDARD))
		rc = update_recv_altsec_order(update, s, controlFlags);
	else if (controlFlags & ORDER_SECONDARY)
		rc = update_recv_secondary_order(update, s, controlFlags);
	else
		rc = update_recv_primary_order(update, s, controlFlags);

	if (!rc)
		WLog_Print(update->log, WLOG_ERROR, "order flags %02" PRIx8 " failed", controlFlags);

	return rc;
}
