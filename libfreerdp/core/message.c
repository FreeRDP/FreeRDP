/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Asynchronous Message Queue
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2017 Armin Novak <armin.novak@thincast.com>
 * Copyright 2017 Thincast Technologies GmbH
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

#include <freerdp/config.h>

#include <winpr/assert.h>

#include "rdp.h"
#include "message.h"
#include "transport.h"

#include <freerdp/log.h>
#include <freerdp/freerdp.h>

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

#include "../cache/pointer.h"
#include "../cache/bitmap.h"
#include "../cache/palette.h"
#include "../cache/glyph.h"
#include "../cache/brush.h"
#include "../cache/cache.h"

#define TAG FREERDP_TAG("core.message")

/* Update */

static BOOL update_message_BeginPaint(rdpContext* context)
{
	rdp_update_internal* up;

	if (!context || !context->update)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(Update, BeginPaint), NULL,
	                         NULL);
}

static BOOL update_message_EndPaint(rdpContext* context)
{
	rdp_update_internal* up;

	if (!context || !context->update)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(Update, EndPaint), NULL,
	                         NULL);
}

static BOOL update_message_SetBounds(rdpContext* context, const rdpBounds* bounds)
{
	rdpBounds* wParam = NULL;
	rdp_update_internal* up;

	if (!context || !context->update)
		return FALSE;

	if (bounds)
	{
		wParam = (rdpBounds*)malloc(sizeof(rdpBounds));

		if (!wParam)
			return FALSE;

		CopyMemory(wParam, bounds, sizeof(rdpBounds));
	}

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(Update, SetBounds),
	                         (void*)wParam, NULL);
}

static BOOL update_message_Synchronize(rdpContext* context)
{
	rdp_update_internal* up;

	if (!context || !context->update)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(Update, Synchronize), NULL,
	                         NULL);
}

static BOOL update_message_DesktopResize(rdpContext* context)
{
	rdp_update_internal* up;

	if (!context || !context->update)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(Update, DesktopResize), NULL,
	                         NULL);
}

static BOOL update_message_BitmapUpdate(rdpContext* context, const BITMAP_UPDATE* bitmap)
{
	BITMAP_UPDATE* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !bitmap)
		return FALSE;

	wParam = copy_bitmap_update(context, bitmap);

	if (!wParam)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(Update, BitmapUpdate),
	                         (void*)wParam, NULL);
}

static BOOL update_message_Palette(rdpContext* context, const PALETTE_UPDATE* palette)
{
	PALETTE_UPDATE* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !palette)
		return FALSE;

	wParam = copy_palette_update(context, palette);

	if (!wParam)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(Update, Palette),
	                         (void*)wParam, NULL);
}

static BOOL update_message_PlaySound(rdpContext* context, const PLAY_SOUND_UPDATE* playSound)
{
	PLAY_SOUND_UPDATE* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !playSound)
		return FALSE;

	wParam = (PLAY_SOUND_UPDATE*)malloc(sizeof(PLAY_SOUND_UPDATE));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, playSound, sizeof(PLAY_SOUND_UPDATE));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(Update, PlaySound),
	                         (void*)wParam, NULL);
}

static BOOL update_message_SetKeyboardIndicators(rdpContext* context, UINT16 led_flags)
{
	rdp_update_internal* up;

	if (!context || !context->update)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(Update, SetKeyboardIndicators), (void*)(size_t)led_flags,
	                         NULL);
}

static BOOL update_message_SetKeyboardImeStatus(rdpContext* context, UINT16 imeId, UINT32 imeState,
                                                UINT32 imeConvMode)
{
	rdp_update_internal* up;

	if (!context || !context->update)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(Update, SetKeyboardImeStatus),
	                         (void*)(size_t)((imeId << 16UL) | imeState),
	                         (void*)(size_t)imeConvMode);
}

static BOOL update_message_RefreshRect(rdpContext* context, BYTE count, const RECTANGLE_16* areas)
{
	RECTANGLE_16* lParam;
	rdp_update_internal* up;

	if (!context || !context->update || !areas)
		return FALSE;

	lParam = (RECTANGLE_16*)calloc(count, sizeof(RECTANGLE_16));

	if (!lParam)
		return FALSE;

	CopyMemory(lParam, areas, sizeof(RECTANGLE_16) * count);

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(Update, RefreshRect),
	                         (void*)(size_t)count, (void*)lParam);
}

static BOOL update_message_SuppressOutput(rdpContext* context, BYTE allow, const RECTANGLE_16* area)
{
	RECTANGLE_16* lParam = NULL;
	rdp_update_internal* up;

	if (!context || !context->update)
		return FALSE;

	if (area)
	{
		lParam = (RECTANGLE_16*)malloc(sizeof(RECTANGLE_16));

		if (!lParam)
			return FALSE;

		CopyMemory(lParam, area, sizeof(RECTANGLE_16));
	}

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(Update, SuppressOutput),
	                         (void*)(size_t)allow, (void*)lParam);
}

static BOOL update_message_SurfaceCommand(rdpContext* context, wStream* s)
{
	wStream* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !s)
		return FALSE;

	wParam = Stream_New(NULL, Stream_GetRemainingLength(s));

	if (!wParam)
		return FALSE;

	Stream_Copy(s, wParam, Stream_GetRemainingLength(s));
	Stream_SetPosition(wParam, 0);

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(Update, SurfaceCommand),
	                         (void*)wParam, NULL);
}

static BOOL update_message_SurfaceBits(rdpContext* context,
                                       const SURFACE_BITS_COMMAND* surfaceBitsCommand)
{
	SURFACE_BITS_COMMAND* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !surfaceBitsCommand)
		return FALSE;

	wParam = copy_surface_bits_command(context, surfaceBitsCommand);

	if (!wParam)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(Update, SurfaceBits),
	                         (void*)wParam, NULL);
}

static BOOL update_message_SurfaceFrameMarker(rdpContext* context,
                                              const SURFACE_FRAME_MARKER* surfaceFrameMarker)
{
	SURFACE_FRAME_MARKER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !surfaceFrameMarker)
		return FALSE;

	wParam = (SURFACE_FRAME_MARKER*)malloc(sizeof(SURFACE_FRAME_MARKER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, surfaceFrameMarker, sizeof(SURFACE_FRAME_MARKER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(Update, SurfaceFrameMarker),
	                         (void*)wParam, NULL);
}

static BOOL update_message_SurfaceFrameAcknowledge(rdpContext* context, UINT32 frameId)
{
	rdp_update_internal* up;

	if (!context || !context->update)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(Update, SurfaceFrameAcknowledge), (void*)(size_t)frameId,
	                         NULL);
}

/* Primary Update */

static BOOL update_message_DstBlt(rdpContext* context, const DSTBLT_ORDER* dstBlt)
{
	DSTBLT_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !dstBlt)
		return FALSE;

	wParam = (DSTBLT_ORDER*)malloc(sizeof(DSTBLT_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, dstBlt, sizeof(DSTBLT_ORDER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PrimaryUpdate, DstBlt),
	                         (void*)wParam, NULL);
}

static BOOL update_message_PatBlt(rdpContext* context, PATBLT_ORDER* patBlt)
{
	PATBLT_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !patBlt)
		return FALSE;

	wParam = (PATBLT_ORDER*)malloc(sizeof(PATBLT_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, patBlt, sizeof(PATBLT_ORDER));
	wParam->brush.data = (BYTE*)wParam->brush.p8x8;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PrimaryUpdate, PatBlt),
	                         (void*)wParam, NULL);
}

static BOOL update_message_ScrBlt(rdpContext* context, const SCRBLT_ORDER* scrBlt)
{
	SCRBLT_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !scrBlt)
		return FALSE;

	wParam = (SCRBLT_ORDER*)malloc(sizeof(SCRBLT_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, scrBlt, sizeof(SCRBLT_ORDER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PrimaryUpdate, ScrBlt),
	                         (void*)wParam, NULL);
}

static BOOL update_message_OpaqueRect(rdpContext* context, const OPAQUE_RECT_ORDER* opaqueRect)
{
	OPAQUE_RECT_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !opaqueRect)
		return FALSE;

	wParam = (OPAQUE_RECT_ORDER*)malloc(sizeof(OPAQUE_RECT_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, opaqueRect, sizeof(OPAQUE_RECT_ORDER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PrimaryUpdate, OpaqueRect),
	                         (void*)wParam, NULL);
}

static BOOL update_message_DrawNineGrid(rdpContext* context,
                                        const DRAW_NINE_GRID_ORDER* drawNineGrid)
{
	DRAW_NINE_GRID_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !drawNineGrid)
		return FALSE;

	wParam = (DRAW_NINE_GRID_ORDER*)malloc(sizeof(DRAW_NINE_GRID_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, drawNineGrid, sizeof(DRAW_NINE_GRID_ORDER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PrimaryUpdate, DrawNineGrid),
	                         (void*)wParam, NULL);
}

static BOOL update_message_MultiDstBlt(rdpContext* context, const MULTI_DSTBLT_ORDER* multiDstBlt)
{
	MULTI_DSTBLT_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !multiDstBlt)
		return FALSE;

	wParam = (MULTI_DSTBLT_ORDER*)malloc(sizeof(MULTI_DSTBLT_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, multiDstBlt, sizeof(MULTI_DSTBLT_ORDER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PrimaryUpdate, MultiDstBlt),
	                         (void*)wParam, NULL);
}

static BOOL update_message_MultiPatBlt(rdpContext* context, const MULTI_PATBLT_ORDER* multiPatBlt)
{
	MULTI_PATBLT_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !multiPatBlt)
		return FALSE;

	wParam = (MULTI_PATBLT_ORDER*)malloc(sizeof(MULTI_PATBLT_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, multiPatBlt, sizeof(MULTI_PATBLT_ORDER));
	wParam->brush.data = (BYTE*)wParam->brush.p8x8;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PrimaryUpdate, MultiPatBlt),
	                         (void*)wParam, NULL);
}

static BOOL update_message_MultiScrBlt(rdpContext* context, const MULTI_SCRBLT_ORDER* multiScrBlt)
{
	MULTI_SCRBLT_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !multiScrBlt)
		return FALSE;

	wParam = (MULTI_SCRBLT_ORDER*)malloc(sizeof(MULTI_SCRBLT_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, multiScrBlt, sizeof(MULTI_SCRBLT_ORDER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PrimaryUpdate, MultiScrBlt),
	                         (void*)wParam, NULL);
}

static BOOL update_message_MultiOpaqueRect(rdpContext* context,
                                           const MULTI_OPAQUE_RECT_ORDER* multiOpaqueRect)
{
	MULTI_OPAQUE_RECT_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !multiOpaqueRect)
		return FALSE;

	wParam = (MULTI_OPAQUE_RECT_ORDER*)malloc(sizeof(MULTI_OPAQUE_RECT_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, multiOpaqueRect, sizeof(MULTI_OPAQUE_RECT_ORDER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(PrimaryUpdate, MultiOpaqueRect), (void*)wParam, NULL);
}

static BOOL update_message_MultiDrawNineGrid(rdpContext* context,
                                             const MULTI_DRAW_NINE_GRID_ORDER* multiDrawNineGrid)
{
	MULTI_DRAW_NINE_GRID_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !multiDrawNineGrid)
		return FALSE;

	wParam = (MULTI_DRAW_NINE_GRID_ORDER*)malloc(sizeof(MULTI_DRAW_NINE_GRID_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, multiDrawNineGrid, sizeof(MULTI_DRAW_NINE_GRID_ORDER));
	/* TODO: complete copy */

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(PrimaryUpdate, MultiDrawNineGrid), (void*)wParam, NULL);
}

static BOOL update_message_LineTo(rdpContext* context, const LINE_TO_ORDER* lineTo)
{
	LINE_TO_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !lineTo)
		return FALSE;

	wParam = (LINE_TO_ORDER*)malloc(sizeof(LINE_TO_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, lineTo, sizeof(LINE_TO_ORDER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PrimaryUpdate, LineTo),
	                         (void*)wParam, NULL);
}

static BOOL update_message_Polyline(rdpContext* context, const POLYLINE_ORDER* polyline)
{
	POLYLINE_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !polyline)
		return FALSE;

	wParam = (POLYLINE_ORDER*)malloc(sizeof(POLYLINE_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, polyline, sizeof(POLYLINE_ORDER));
	wParam->points = (DELTA_POINT*)calloc(wParam->numDeltaEntries, sizeof(DELTA_POINT));

	if (!wParam->points)
	{
		free(wParam);
		return FALSE;
	}

	CopyMemory(wParam->points, polyline->points, sizeof(DELTA_POINT) * wParam->numDeltaEntries);

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PrimaryUpdate, Polyline),
	                         (void*)wParam, NULL);
}

static BOOL update_message_MemBlt(rdpContext* context, MEMBLT_ORDER* memBlt)
{
	MEMBLT_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !memBlt)
		return FALSE;

	wParam = (MEMBLT_ORDER*)malloc(sizeof(MEMBLT_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, memBlt, sizeof(MEMBLT_ORDER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PrimaryUpdate, MemBlt),
	                         (void*)wParam, NULL);
}

static BOOL update_message_Mem3Blt(rdpContext* context, MEM3BLT_ORDER* mem3Blt)
{
	MEM3BLT_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !mem3Blt)
		return FALSE;

	wParam = (MEM3BLT_ORDER*)malloc(sizeof(MEM3BLT_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, mem3Blt, sizeof(MEM3BLT_ORDER));
	wParam->brush.data = (BYTE*)wParam->brush.p8x8;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PrimaryUpdate, Mem3Blt),
	                         (void*)wParam, NULL);
}

static BOOL update_message_SaveBitmap(rdpContext* context, const SAVE_BITMAP_ORDER* saveBitmap)
{
	SAVE_BITMAP_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !saveBitmap)
		return FALSE;

	wParam = (SAVE_BITMAP_ORDER*)malloc(sizeof(SAVE_BITMAP_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, saveBitmap, sizeof(SAVE_BITMAP_ORDER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PrimaryUpdate, SaveBitmap),
	                         (void*)wParam, NULL);
}

static BOOL update_message_GlyphIndex(rdpContext* context, GLYPH_INDEX_ORDER* glyphIndex)
{
	GLYPH_INDEX_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !glyphIndex)
		return FALSE;

	wParam = (GLYPH_INDEX_ORDER*)malloc(sizeof(GLYPH_INDEX_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, glyphIndex, sizeof(GLYPH_INDEX_ORDER));
	wParam->brush.data = (BYTE*)wParam->brush.p8x8;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PrimaryUpdate, GlyphIndex),
	                         (void*)wParam, NULL);
}

static BOOL update_message_FastIndex(rdpContext* context, const FAST_INDEX_ORDER* fastIndex)
{
	FAST_INDEX_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !fastIndex)
		return FALSE;

	wParam = (FAST_INDEX_ORDER*)malloc(sizeof(FAST_INDEX_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, fastIndex, sizeof(FAST_INDEX_ORDER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PrimaryUpdate, FastIndex),
	                         (void*)wParam, NULL);
}

static BOOL update_message_FastGlyph(rdpContext* context, const FAST_GLYPH_ORDER* fastGlyph)
{
	FAST_GLYPH_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !fastGlyph)
		return FALSE;

	wParam = (FAST_GLYPH_ORDER*)malloc(sizeof(FAST_GLYPH_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, fastGlyph, sizeof(FAST_GLYPH_ORDER));

	if (wParam->cbData > 1)
	{
		wParam->glyphData.aj = (BYTE*)malloc(fastGlyph->glyphData.cb);

		if (!wParam->glyphData.aj)
		{
			free(wParam);
			return FALSE;
		}

		CopyMemory(wParam->glyphData.aj, fastGlyph->glyphData.aj, fastGlyph->glyphData.cb);
	}
	else
	{
		wParam->glyphData.aj = NULL;
	}

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PrimaryUpdate, FastGlyph),
	                         (void*)wParam, NULL);
}

static BOOL update_message_PolygonSC(rdpContext* context, const POLYGON_SC_ORDER* polygonSC)
{
	POLYGON_SC_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !polygonSC)
		return FALSE;

	wParam = (POLYGON_SC_ORDER*)malloc(sizeof(POLYGON_SC_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, polygonSC, sizeof(POLYGON_SC_ORDER));
	wParam->points = (DELTA_POINT*)calloc(wParam->numPoints, sizeof(DELTA_POINT));

	if (!wParam->points)
	{
		free(wParam);
		return FALSE;
	}

	CopyMemory(wParam->points, polygonSC, sizeof(DELTA_POINT) * wParam->numPoints);

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PrimaryUpdate, PolygonSC),
	                         (void*)wParam, NULL);
}

static BOOL update_message_PolygonCB(rdpContext* context, POLYGON_CB_ORDER* polygonCB)
{
	POLYGON_CB_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !polygonCB)
		return FALSE;

	wParam = (POLYGON_CB_ORDER*)malloc(sizeof(POLYGON_CB_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, polygonCB, sizeof(POLYGON_CB_ORDER));
	wParam->points = (DELTA_POINT*)calloc(wParam->numPoints, sizeof(DELTA_POINT));

	if (!wParam->points)
	{
		free(wParam);
		return FALSE;
	}

	CopyMemory(wParam->points, polygonCB, sizeof(DELTA_POINT) * wParam->numPoints);
	wParam->brush.data = (BYTE*)wParam->brush.p8x8;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PrimaryUpdate, PolygonCB),
	                         (void*)wParam, NULL);
}

static BOOL update_message_EllipseSC(rdpContext* context, const ELLIPSE_SC_ORDER* ellipseSC)
{
	ELLIPSE_SC_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !ellipseSC)
		return FALSE;

	wParam = (ELLIPSE_SC_ORDER*)malloc(sizeof(ELLIPSE_SC_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, ellipseSC, sizeof(ELLIPSE_SC_ORDER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PrimaryUpdate, EllipseSC),
	                         (void*)wParam, NULL);
}

static BOOL update_message_EllipseCB(rdpContext* context, const ELLIPSE_CB_ORDER* ellipseCB)
{
	ELLIPSE_CB_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !ellipseCB)
		return FALSE;

	wParam = (ELLIPSE_CB_ORDER*)malloc(sizeof(ELLIPSE_CB_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, ellipseCB, sizeof(ELLIPSE_CB_ORDER));
	wParam->brush.data = (BYTE*)wParam->brush.p8x8;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PrimaryUpdate, EllipseCB),
	                         (void*)wParam, NULL);
}

/* Secondary Update */

static BOOL update_message_CacheBitmap(rdpContext* context,
                                       const CACHE_BITMAP_ORDER* cacheBitmapOrder)
{
	CACHE_BITMAP_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !cacheBitmapOrder)
		return FALSE;

	wParam = copy_cache_bitmap_order(context, cacheBitmapOrder);

	if (!wParam)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(SecondaryUpdate, CacheBitmap),
	                         (void*)wParam, NULL);
}

static BOOL update_message_CacheBitmapV2(rdpContext* context,
                                         CACHE_BITMAP_V2_ORDER* cacheBitmapV2Order)
{
	CACHE_BITMAP_V2_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !cacheBitmapV2Order)
		return FALSE;

	wParam = copy_cache_bitmap_v2_order(context, cacheBitmapV2Order);

	if (!wParam)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(SecondaryUpdate, CacheBitmapV2), (void*)wParam, NULL);
}

static BOOL update_message_CacheBitmapV3(rdpContext* context,
                                         CACHE_BITMAP_V3_ORDER* cacheBitmapV3Order)
{
	CACHE_BITMAP_V3_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !cacheBitmapV3Order)
		return FALSE;

	wParam = copy_cache_bitmap_v3_order(context, cacheBitmapV3Order);

	if (!wParam)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(SecondaryUpdate, CacheBitmapV3), (void*)wParam, NULL);
}

static BOOL update_message_CacheColorTable(rdpContext* context,
                                           const CACHE_COLOR_TABLE_ORDER* cacheColorTableOrder)
{
	CACHE_COLOR_TABLE_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !cacheColorTableOrder)
		return FALSE;

	wParam = copy_cache_color_table_order(context, cacheColorTableOrder);

	if (!wParam)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(SecondaryUpdate, CacheColorTable), (void*)wParam, NULL);
}

static BOOL update_message_CacheGlyph(rdpContext* context, const CACHE_GLYPH_ORDER* cacheGlyphOrder)
{
	CACHE_GLYPH_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !cacheGlyphOrder)
		return FALSE;

	wParam = copy_cache_glyph_order(context, cacheGlyphOrder);

	if (!wParam)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(SecondaryUpdate, CacheGlyph),
	                         (void*)wParam, NULL);
}

static BOOL update_message_CacheGlyphV2(rdpContext* context,
                                        const CACHE_GLYPH_V2_ORDER* cacheGlyphV2Order)
{
	CACHE_GLYPH_V2_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !cacheGlyphV2Order)
		return FALSE;

	wParam = copy_cache_glyph_v2_order(context, cacheGlyphV2Order);

	if (!wParam)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(SecondaryUpdate, CacheGlyphV2), (void*)wParam, NULL);
}

static BOOL update_message_CacheBrush(rdpContext* context, const CACHE_BRUSH_ORDER* cacheBrushOrder)
{
	CACHE_BRUSH_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !cacheBrushOrder)
		return FALSE;

	wParam = copy_cache_brush_order(context, cacheBrushOrder);

	if (!wParam)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(SecondaryUpdate, CacheBrush),
	                         (void*)wParam, NULL);
}

/* Alternate Secondary Update */

static BOOL
update_message_CreateOffscreenBitmap(rdpContext* context,
                                     const CREATE_OFFSCREEN_BITMAP_ORDER* createOffscreenBitmap)
{
	CREATE_OFFSCREEN_BITMAP_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !createOffscreenBitmap)
		return FALSE;

	wParam = (CREATE_OFFSCREEN_BITMAP_ORDER*)malloc(sizeof(CREATE_OFFSCREEN_BITMAP_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, createOffscreenBitmap, sizeof(CREATE_OFFSCREEN_BITMAP_ORDER));
	wParam->deleteList.cIndices = createOffscreenBitmap->deleteList.cIndices;
	wParam->deleteList.sIndices = wParam->deleteList.cIndices;
	wParam->deleteList.indices = (UINT16*)calloc(wParam->deleteList.cIndices, sizeof(UINT16));

	if (!wParam->deleteList.indices)
	{
		free(wParam);
		return FALSE;
	}

	CopyMemory(wParam->deleteList.indices, createOffscreenBitmap->deleteList.indices,
	           wParam->deleteList.cIndices);

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(AltSecUpdate, CreateOffscreenBitmap), (void*)wParam,
	                         NULL);
}

static BOOL update_message_SwitchSurface(rdpContext* context,
                                         const SWITCH_SURFACE_ORDER* switchSurface)
{
	SWITCH_SURFACE_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !switchSurface)
		return FALSE;

	wParam = (SWITCH_SURFACE_ORDER*)malloc(sizeof(SWITCH_SURFACE_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, switchSurface, sizeof(SWITCH_SURFACE_ORDER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(AltSecUpdate, SwitchSurface),
	                         (void*)wParam, NULL);
}

static BOOL
update_message_CreateNineGridBitmap(rdpContext* context,
                                    const CREATE_NINE_GRID_BITMAP_ORDER* createNineGridBitmap)
{
	CREATE_NINE_GRID_BITMAP_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !createNineGridBitmap)
		return FALSE;

	wParam = (CREATE_NINE_GRID_BITMAP_ORDER*)malloc(sizeof(CREATE_NINE_GRID_BITMAP_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, createNineGridBitmap, sizeof(CREATE_NINE_GRID_BITMAP_ORDER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(AltSecUpdate, CreateNineGridBitmap), (void*)wParam,
	                         NULL);
}

static BOOL update_message_FrameMarker(rdpContext* context, const FRAME_MARKER_ORDER* frameMarker)
{
	FRAME_MARKER_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !frameMarker)
		return FALSE;

	wParam = (FRAME_MARKER_ORDER*)malloc(sizeof(FRAME_MARKER_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, frameMarker, sizeof(FRAME_MARKER_ORDER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(AltSecUpdate, FrameMarker),
	                         (void*)wParam, NULL);
}

static BOOL update_message_StreamBitmapFirst(rdpContext* context,
                                             const STREAM_BITMAP_FIRST_ORDER* streamBitmapFirst)
{
	STREAM_BITMAP_FIRST_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !streamBitmapFirst)
		return FALSE;

	wParam = (STREAM_BITMAP_FIRST_ORDER*)malloc(sizeof(STREAM_BITMAP_FIRST_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, streamBitmapFirst, sizeof(STREAM_BITMAP_FIRST_ORDER));
	/* TODO: complete copy */

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(AltSecUpdate, StreamBitmapFirst), (void*)wParam, NULL);
}

static BOOL update_message_StreamBitmapNext(rdpContext* context,
                                            const STREAM_BITMAP_NEXT_ORDER* streamBitmapNext)
{
	STREAM_BITMAP_NEXT_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !streamBitmapNext)
		return FALSE;

	wParam = (STREAM_BITMAP_NEXT_ORDER*)malloc(sizeof(STREAM_BITMAP_NEXT_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, streamBitmapNext, sizeof(STREAM_BITMAP_NEXT_ORDER));
	/* TODO: complete copy */

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(AltSecUpdate, StreamBitmapNext), (void*)wParam, NULL);
}

static BOOL update_message_DrawGdiPlusFirst(rdpContext* context,
                                            const DRAW_GDIPLUS_FIRST_ORDER* drawGdiPlusFirst)
{
	DRAW_GDIPLUS_FIRST_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !drawGdiPlusFirst)
		return FALSE;

	wParam = (DRAW_GDIPLUS_FIRST_ORDER*)malloc(sizeof(DRAW_GDIPLUS_FIRST_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, drawGdiPlusFirst, sizeof(DRAW_GDIPLUS_FIRST_ORDER));
	/* TODO: complete copy */
	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(AltSecUpdate, DrawGdiPlusFirst), (void*)wParam, NULL);
}

static BOOL update_message_DrawGdiPlusNext(rdpContext* context,
                                           const DRAW_GDIPLUS_NEXT_ORDER* drawGdiPlusNext)
{
	DRAW_GDIPLUS_NEXT_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !drawGdiPlusNext)
		return FALSE;

	wParam = (DRAW_GDIPLUS_NEXT_ORDER*)malloc(sizeof(DRAW_GDIPLUS_NEXT_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, drawGdiPlusNext, sizeof(DRAW_GDIPLUS_NEXT_ORDER));
	/* TODO: complete copy */

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(AltSecUpdate, DrawGdiPlusNext), (void*)wParam, NULL);
}

static BOOL update_message_DrawGdiPlusEnd(rdpContext* context,
                                          const DRAW_GDIPLUS_END_ORDER* drawGdiPlusEnd)
{
	DRAW_GDIPLUS_END_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !drawGdiPlusEnd)
		return FALSE;

	wParam = (DRAW_GDIPLUS_END_ORDER*)malloc(sizeof(DRAW_GDIPLUS_END_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, drawGdiPlusEnd, sizeof(DRAW_GDIPLUS_END_ORDER));
	/* TODO: complete copy */

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(AltSecUpdate, DrawGdiPlusEnd),
	                         (void*)wParam, NULL);
}

static BOOL
update_message_DrawGdiPlusCacheFirst(rdpContext* context,
                                     const DRAW_GDIPLUS_CACHE_FIRST_ORDER* drawGdiPlusCacheFirst)
{
	DRAW_GDIPLUS_CACHE_FIRST_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !drawGdiPlusCacheFirst)
		return FALSE;

	wParam = (DRAW_GDIPLUS_CACHE_FIRST_ORDER*)malloc(sizeof(DRAW_GDIPLUS_CACHE_FIRST_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, drawGdiPlusCacheFirst, sizeof(DRAW_GDIPLUS_CACHE_FIRST_ORDER));
	/* TODO: complete copy */

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(AltSecUpdate, DrawGdiPlusCacheFirst), (void*)wParam,
	                         NULL);
}

static BOOL
update_message_DrawGdiPlusCacheNext(rdpContext* context,
                                    const DRAW_GDIPLUS_CACHE_NEXT_ORDER* drawGdiPlusCacheNext)
{
	DRAW_GDIPLUS_CACHE_NEXT_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !drawGdiPlusCacheNext)
		return FALSE;

	wParam = (DRAW_GDIPLUS_CACHE_NEXT_ORDER*)malloc(sizeof(DRAW_GDIPLUS_CACHE_NEXT_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, drawGdiPlusCacheNext, sizeof(DRAW_GDIPLUS_CACHE_NEXT_ORDER));
	/* TODO: complete copy */

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(AltSecUpdate, DrawGdiPlusCacheNext), (void*)wParam,
	                         NULL);
}

static BOOL
update_message_DrawGdiPlusCacheEnd(rdpContext* context,
                                   const DRAW_GDIPLUS_CACHE_END_ORDER* drawGdiPlusCacheEnd)
{
	DRAW_GDIPLUS_CACHE_END_ORDER* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !drawGdiPlusCacheEnd)
		return FALSE;

	wParam = (DRAW_GDIPLUS_CACHE_END_ORDER*)malloc(sizeof(DRAW_GDIPLUS_CACHE_END_ORDER));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, drawGdiPlusCacheEnd, sizeof(DRAW_GDIPLUS_CACHE_END_ORDER));
	/* TODO: complete copy */

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(AltSecUpdate, DrawGdiPlusCacheEnd), (void*)wParam, NULL);
}

/* Window Update */

static BOOL update_message_WindowCreate(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                        const WINDOW_STATE_ORDER* windowState)
{
	WINDOW_ORDER_INFO* wParam;
	WINDOW_STATE_ORDER* lParam;
	rdp_update_internal* up;

	if (!context || !context->update || !orderInfo || !windowState)
		return FALSE;

	wParam = (WINDOW_ORDER_INFO*)malloc(sizeof(WINDOW_ORDER_INFO));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));
	lParam = (WINDOW_STATE_ORDER*)malloc(sizeof(WINDOW_STATE_ORDER));

	if (!lParam)
	{
		free(wParam);
		return FALSE;
	}

	CopyMemory(lParam, windowState, sizeof(WINDOW_STATE_ORDER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(WindowUpdate, WindowCreate),
	                         (void*)wParam, (void*)lParam);
}

static BOOL update_message_WindowUpdate(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                        const WINDOW_STATE_ORDER* windowState)
{
	WINDOW_ORDER_INFO* wParam;
	WINDOW_STATE_ORDER* lParam;
	rdp_update_internal* up;

	if (!context || !context->update || !orderInfo || !windowState)
		return FALSE;

	wParam = (WINDOW_ORDER_INFO*)malloc(sizeof(WINDOW_ORDER_INFO));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));
	lParam = (WINDOW_STATE_ORDER*)malloc(sizeof(WINDOW_STATE_ORDER));

	if (!lParam)
	{
		free(wParam);
		return FALSE;
	}

	CopyMemory(lParam, windowState, sizeof(WINDOW_STATE_ORDER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(WindowUpdate, WindowUpdate),
	                         (void*)wParam, (void*)lParam);
}

static BOOL update_message_WindowIcon(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                      const WINDOW_ICON_ORDER* windowIcon)
{
	WINDOW_ORDER_INFO* wParam;
	WINDOW_ICON_ORDER* lParam;
	rdp_update_internal* up;

	if (!context || !context->update || !orderInfo || !windowIcon)
		return FALSE;

	wParam = (WINDOW_ORDER_INFO*)malloc(sizeof(WINDOW_ORDER_INFO));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));
	lParam = (WINDOW_ICON_ORDER*)calloc(1, sizeof(WINDOW_ICON_ORDER));

	if (!lParam)
		goto out_fail;

	lParam->iconInfo = calloc(1, sizeof(ICON_INFO));

	if (!lParam->iconInfo)
		goto out_fail;

	CopyMemory(lParam, windowIcon, sizeof(WINDOW_ICON_ORDER));
	WLog_VRB(TAG, "update_message_WindowIcon");

	if (windowIcon->iconInfo->cbBitsColor > 0)
	{
		lParam->iconInfo->bitsColor = (BYTE*)malloc(windowIcon->iconInfo->cbBitsColor);

		if (!lParam->iconInfo->bitsColor)
			goto out_fail;

		CopyMemory(lParam->iconInfo->bitsColor, windowIcon->iconInfo->bitsColor,
		           windowIcon->iconInfo->cbBitsColor);
	}

	if (windowIcon->iconInfo->cbBitsMask > 0)
	{
		lParam->iconInfo->bitsMask = (BYTE*)malloc(windowIcon->iconInfo->cbBitsMask);

		if (!lParam->iconInfo->bitsMask)
			goto out_fail;

		CopyMemory(lParam->iconInfo->bitsMask, windowIcon->iconInfo->bitsMask,
		           windowIcon->iconInfo->cbBitsMask);
	}

	if (windowIcon->iconInfo->cbColorTable > 0)
	{
		lParam->iconInfo->colorTable = (BYTE*)malloc(windowIcon->iconInfo->cbColorTable);

		if (!lParam->iconInfo->colorTable)
			goto out_fail;

		CopyMemory(lParam->iconInfo->colorTable, windowIcon->iconInfo->colorTable,
		           windowIcon->iconInfo->cbColorTable);
	}

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(WindowUpdate, WindowIcon),
	                         (void*)wParam, (void*)lParam);
out_fail:

	if (lParam && lParam->iconInfo)
	{
		free(lParam->iconInfo->bitsColor);
		free(lParam->iconInfo->bitsMask);
		free(lParam->iconInfo->colorTable);
		free(lParam->iconInfo);
	}

	free(lParam);
	free(wParam);
	return FALSE;
}

static BOOL update_message_WindowCachedIcon(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                            const WINDOW_CACHED_ICON_ORDER* windowCachedIcon)
{
	WINDOW_ORDER_INFO* wParam;
	WINDOW_CACHED_ICON_ORDER* lParam;
	rdp_update_internal* up;

	if (!context || !context->update || !orderInfo || !windowCachedIcon)
		return FALSE;

	wParam = (WINDOW_ORDER_INFO*)malloc(sizeof(WINDOW_ORDER_INFO));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));
	lParam = (WINDOW_CACHED_ICON_ORDER*)malloc(sizeof(WINDOW_CACHED_ICON_ORDER));

	if (!lParam)
	{
		free(wParam);
		return FALSE;
	}

	CopyMemory(lParam, windowCachedIcon, sizeof(WINDOW_CACHED_ICON_ORDER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(WindowUpdate, WindowCachedIcon), (void*)wParam,
	                         (void*)lParam);
}

static BOOL update_message_WindowDelete(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo)
{
	WINDOW_ORDER_INFO* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !orderInfo)
		return FALSE;

	wParam = (WINDOW_ORDER_INFO*)malloc(sizeof(WINDOW_ORDER_INFO));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(WindowUpdate, WindowDelete),
	                         (void*)wParam, NULL);
}

static BOOL update_message_NotifyIconCreate(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                            const NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	WINDOW_ORDER_INFO* wParam;
	NOTIFY_ICON_STATE_ORDER* lParam;
	rdp_update_internal* up;

	if (!context || !context->update || !orderInfo || !notifyIconState)
		return FALSE;

	wParam = (WINDOW_ORDER_INFO*)malloc(sizeof(WINDOW_ORDER_INFO));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));
	lParam = (NOTIFY_ICON_STATE_ORDER*)malloc(sizeof(NOTIFY_ICON_STATE_ORDER));

	if (!lParam)
	{
		free(wParam);
		return FALSE;
	}

	CopyMemory(lParam, notifyIconState, sizeof(NOTIFY_ICON_STATE_ORDER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(WindowUpdate, NotifyIconCreate), (void*)wParam,
	                         (void*)lParam);
}

static BOOL update_message_NotifyIconUpdate(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                            const NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	WINDOW_ORDER_INFO* wParam;
	NOTIFY_ICON_STATE_ORDER* lParam;
	rdp_update_internal* up;

	if (!context || !context->update || !orderInfo || !notifyIconState)
		return FALSE;

	wParam = (WINDOW_ORDER_INFO*)malloc(sizeof(WINDOW_ORDER_INFO));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));
	lParam = (NOTIFY_ICON_STATE_ORDER*)malloc(sizeof(NOTIFY_ICON_STATE_ORDER));

	if (!lParam)
	{
		free(wParam);
		return FALSE;
	}

	CopyMemory(lParam, notifyIconState, sizeof(NOTIFY_ICON_STATE_ORDER));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(WindowUpdate, NotifyIconUpdate), (void*)wParam,
	                         (void*)lParam);
}

static BOOL update_message_NotifyIconDelete(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo)
{
	WINDOW_ORDER_INFO* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !orderInfo)
		return FALSE;

	wParam = (WINDOW_ORDER_INFO*)malloc(sizeof(WINDOW_ORDER_INFO));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(WindowUpdate, NotifyIconDelete), (void*)wParam, NULL);
}

static BOOL update_message_MonitoredDesktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                            const MONITORED_DESKTOP_ORDER* monitoredDesktop)
{
	WINDOW_ORDER_INFO* wParam;
	MONITORED_DESKTOP_ORDER* lParam;
	rdp_update_internal* up;

	if (!context || !context->update || !orderInfo || !monitoredDesktop)
		return FALSE;

	wParam = (WINDOW_ORDER_INFO*)malloc(sizeof(WINDOW_ORDER_INFO));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));
	lParam = (MONITORED_DESKTOP_ORDER*)malloc(sizeof(MONITORED_DESKTOP_ORDER));

	if (!lParam)
	{
		free(wParam);
		return FALSE;
	}

	CopyMemory(lParam, monitoredDesktop, sizeof(MONITORED_DESKTOP_ORDER));
	lParam->windowIds = NULL;

	if (lParam->numWindowIds)
	{
		lParam->windowIds = (UINT32*)calloc(lParam->numWindowIds, sizeof(UINT32));
		CopyMemory(lParam->windowIds, monitoredDesktop->windowIds, lParam->numWindowIds);
	}

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(WindowUpdate, MonitoredDesktop), (void*)wParam,
	                         (void*)lParam);
}

static BOOL update_message_NonMonitoredDesktop(rdpContext* context,
                                               const WINDOW_ORDER_INFO* orderInfo)
{
	WINDOW_ORDER_INFO* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !orderInfo)
		return FALSE;

	wParam = (WINDOW_ORDER_INFO*)malloc(sizeof(WINDOW_ORDER_INFO));

	if (!wParam)
		return FALSE;

	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(WindowUpdate, NonMonitoredDesktop), (void*)wParam, NULL);
}

/* Pointer Update */

static BOOL update_message_PointerPosition(rdpContext* context,
                                           const POINTER_POSITION_UPDATE* pointerPosition)
{
	POINTER_POSITION_UPDATE* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !pointerPosition)
		return FALSE;

	wParam = copy_pointer_position_update(context, pointerPosition);

	if (!wParam)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context,
	                         MakeMessageId(PointerUpdate, PointerPosition), (void*)wParam, NULL);
}

static BOOL update_message_PointerSystem(rdpContext* context,
                                         const POINTER_SYSTEM_UPDATE* pointerSystem)
{
	POINTER_SYSTEM_UPDATE* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !pointerSystem)
		return FALSE;

	wParam = copy_pointer_system_update(context, pointerSystem);

	if (!wParam)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PointerUpdate, PointerSystem),
	                         (void*)wParam, NULL);
}

static BOOL update_message_PointerColor(rdpContext* context,
                                        const POINTER_COLOR_UPDATE* pointerColor)
{
	POINTER_COLOR_UPDATE* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !pointerColor)
		return FALSE;

	wParam = copy_pointer_color_update(context, pointerColor);

	if (!wParam)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PointerUpdate, PointerColor),
	                         (void*)wParam, NULL);
}

static BOOL update_message_PointerLarge(rdpContext* context, const POINTER_LARGE_UPDATE* pointer)
{
	POINTER_LARGE_UPDATE* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !pointer)
		return FALSE;

	wParam = copy_pointer_large_update(context, pointer);

	if (!wParam)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PointerUpdate, PointerLarge),
	                         (void*)wParam, NULL);
}

static BOOL update_message_PointerNew(rdpContext* context, const POINTER_NEW_UPDATE* pointerNew)
{
	POINTER_NEW_UPDATE* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !pointerNew)
		return FALSE;

	wParam = copy_pointer_new_update(context, pointerNew);

	if (!wParam)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PointerUpdate, PointerNew),
	                         (void*)wParam, NULL);
}

static BOOL update_message_PointerCached(rdpContext* context,
                                         const POINTER_CACHED_UPDATE* pointerCached)
{
	POINTER_CACHED_UPDATE* wParam;
	rdp_update_internal* up;

	if (!context || !context->update || !pointerCached)
		return FALSE;

	wParam = copy_pointer_cached_update(context, pointerCached);

	if (!wParam)
		return FALSE;

	up = update_cast(context->update);
	return MessageQueue_Post(up->queue, (void*)context, MakeMessageId(PointerUpdate, PointerCached),
	                         (void*)wParam, NULL);
}

/* Message Queue */
static BOOL update_message_free_update_class(wMessage* msg, int type)
{
	rdpContext* context;

	if (!msg)
		return FALSE;

	context = (rdpContext*)msg->context;

	switch (type)
	{
		case Update_BeginPaint:
			break;

		case Update_EndPaint:
			break;

		case Update_SetBounds:
			free(msg->wParam);
			break;

		case Update_Synchronize:
			break;

		case Update_DesktopResize:
			break;

		case Update_BitmapUpdate:
		{
			BITMAP_UPDATE* wParam = (BITMAP_UPDATE*)msg->wParam;
			free_bitmap_update(context, wParam);
		}
		break;

		case Update_Palette:
		{
			PALETTE_UPDATE* palette = (PALETTE_UPDATE*)msg->wParam;
			free_palette_update(context, palette);
		}
		break;

		case Update_PlaySound:
			free(msg->wParam);
			break;

		case Update_RefreshRect:
			free(msg->lParam);
			break;

		case Update_SuppressOutput:
			free(msg->lParam);
			break;

		case Update_SurfaceCommand:
		{
			wStream* s = (wStream*)msg->wParam;
			Stream_Free(s, TRUE);
		}
		break;

		case Update_SurfaceBits:
		{
			SURFACE_BITS_COMMAND* wParam = (SURFACE_BITS_COMMAND*)msg->wParam;
			free_surface_bits_command(context, wParam);
		}
		break;

		case Update_SurfaceFrameMarker:
			free(msg->wParam);
			break;

		case Update_SurfaceFrameAcknowledge:
		case Update_SetKeyboardIndicators:
		case Update_SetKeyboardImeStatus:
			break;

		default:
			return FALSE;
	}

	return TRUE;
}

static BOOL update_message_process_update_class(rdpUpdateProxy* proxy, wMessage* msg, int type)
{
	BOOL rc = FALSE;

	if (!proxy || !msg)
		return FALSE;

	switch (type)
	{
		case Update_BeginPaint:
			rc = IFCALLRESULT(TRUE, proxy->BeginPaint, msg->context);
			break;

		case Update_EndPaint:
			rc = IFCALLRESULT(TRUE, proxy->EndPaint, msg->context);
			break;

		case Update_SetBounds:
			rc = IFCALLRESULT(TRUE, proxy->SetBounds, msg->context, (rdpBounds*)msg->wParam);
			break;

		case Update_Synchronize:
			rc = IFCALLRESULT(TRUE, proxy->Synchronize, msg->context);
			break;

		case Update_DesktopResize:
			rc = IFCALLRESULT(TRUE, proxy->DesktopResize, msg->context);
			break;

		case Update_BitmapUpdate:
			rc = IFCALLRESULT(TRUE, proxy->BitmapUpdate, msg->context, (BITMAP_UPDATE*)msg->wParam);
			break;

		case Update_Palette:
			rc = IFCALLRESULT(TRUE, proxy->Palette, msg->context, (PALETTE_UPDATE*)msg->wParam);
			break;

		case Update_PlaySound:
			rc =
			    IFCALLRESULT(TRUE, proxy->PlaySound, msg->context, (PLAY_SOUND_UPDATE*)msg->wParam);
			break;

		case Update_RefreshRect:
			rc = IFCALLRESULT(TRUE, proxy->RefreshRect, msg->context, (BYTE)(size_t)msg->wParam,
			                  (RECTANGLE_16*)msg->lParam);
			break;

		case Update_SuppressOutput:
			rc = IFCALLRESULT(TRUE, proxy->SuppressOutput, msg->context, (BYTE)(size_t)msg->wParam,
			                  (RECTANGLE_16*)msg->lParam);
			break;

		case Update_SurfaceCommand:
			rc = IFCALLRESULT(TRUE, proxy->SurfaceCommand, msg->context, (wStream*)msg->wParam);
			break;

		case Update_SurfaceBits:
			rc = IFCALLRESULT(TRUE, proxy->SurfaceBits, msg->context,
			                  (SURFACE_BITS_COMMAND*)msg->wParam);
			break;

		case Update_SurfaceFrameMarker:
			rc = IFCALLRESULT(TRUE, proxy->SurfaceFrameMarker, msg->context,
			                  (SURFACE_FRAME_MARKER*)msg->wParam);
			break;

		case Update_SurfaceFrameAcknowledge:
			rc = IFCALLRESULT(TRUE, proxy->SurfaceFrameAcknowledge, msg->context,
			                  (UINT32)(size_t)msg->wParam);
			break;

		case Update_SetKeyboardIndicators:
			rc = IFCALLRESULT(TRUE, proxy->SetKeyboardIndicators, msg->context,
			                  (UINT16)(size_t)msg->wParam);
			break;

		case Update_SetKeyboardImeStatus:
		{
			const UINT16 imeId = ((size_t)msg->wParam) >> 16 & 0xFFFF;
			const UINT32 imeState = ((size_t)msg->wParam) & 0xFFFF;
			const UINT32 imeConvMode = ((size_t)msg->lParam);
			rc = IFCALLRESULT(TRUE, proxy->SetKeyboardImeStatus, msg->context, imeId, imeState,
			                  imeConvMode);
		}
		break;

		default:
			break;
	}

	return rc;
}

static BOOL update_message_free_primary_update_class(wMessage* msg, int type)
{
	if (!msg)
		return FALSE;

	switch (type)
	{
		case PrimaryUpdate_DstBlt:
			free(msg->wParam);
			break;

		case PrimaryUpdate_PatBlt:
			free(msg->wParam);
			break;

		case PrimaryUpdate_ScrBlt:
			free(msg->wParam);
			break;

		case PrimaryUpdate_OpaqueRect:
			free(msg->wParam);
			break;

		case PrimaryUpdate_DrawNineGrid:
			free(msg->wParam);
			break;

		case PrimaryUpdate_MultiDstBlt:
			free(msg->wParam);
			break;

		case PrimaryUpdate_MultiPatBlt:
			free(msg->wParam);
			break;

		case PrimaryUpdate_MultiScrBlt:
			free(msg->wParam);
			break;

		case PrimaryUpdate_MultiOpaqueRect:
			free(msg->wParam);
			break;

		case PrimaryUpdate_MultiDrawNineGrid:
			free(msg->wParam);
			break;

		case PrimaryUpdate_LineTo:
			free(msg->wParam);
			break;

		case PrimaryUpdate_Polyline:
		{
			POLYLINE_ORDER* wParam = (POLYLINE_ORDER*)msg->wParam;
			free(wParam->points);
			free(wParam);
		}
		break;

		case PrimaryUpdate_MemBlt:
			free(msg->wParam);
			break;

		case PrimaryUpdate_Mem3Blt:
			free(msg->wParam);
			break;

		case PrimaryUpdate_SaveBitmap:
			free(msg->wParam);
			break;

		case PrimaryUpdate_GlyphIndex:
			free(msg->wParam);
			break;

		case PrimaryUpdate_FastIndex:
			free(msg->wParam);
			break;

		case PrimaryUpdate_FastGlyph:
		{
			FAST_GLYPH_ORDER* wParam = (FAST_GLYPH_ORDER*)msg->wParam;
			free(wParam->glyphData.aj);
			free(wParam);
		}
		break;

		case PrimaryUpdate_PolygonSC:
		{
			POLYGON_SC_ORDER* wParam = (POLYGON_SC_ORDER*)msg->wParam;
			free(wParam->points);
			free(wParam);
		}
		break;

		case PrimaryUpdate_PolygonCB:
		{
			POLYGON_CB_ORDER* wParam = (POLYGON_CB_ORDER*)msg->wParam;
			free(wParam->points);
			free(wParam);
		}
		break;

		case PrimaryUpdate_EllipseSC:
			free(msg->wParam);
			break;

		case PrimaryUpdate_EllipseCB:
			free(msg->wParam);
			break;

		default:
			return FALSE;
	}

	return TRUE;
}

static BOOL update_message_process_primary_update_class(rdpUpdateProxy* proxy, wMessage* msg,
                                                        int type)
{
	if (!proxy || !msg)
		return FALSE;

	switch (type)
	{
		case PrimaryUpdate_DstBlt:
			return IFCALLRESULT(TRUE, proxy->DstBlt, msg->context, (DSTBLT_ORDER*)msg->wParam);

		case PrimaryUpdate_PatBlt:
			return IFCALLRESULT(TRUE, proxy->PatBlt, msg->context, (PATBLT_ORDER*)msg->wParam);

		case PrimaryUpdate_ScrBlt:
			return IFCALLRESULT(TRUE, proxy->ScrBlt, msg->context, (SCRBLT_ORDER*)msg->wParam);

		case PrimaryUpdate_OpaqueRect:
			return IFCALLRESULT(TRUE, proxy->OpaqueRect, msg->context,
			                    (OPAQUE_RECT_ORDER*)msg->wParam);

		case PrimaryUpdate_DrawNineGrid:
			return IFCALLRESULT(TRUE, proxy->DrawNineGrid, msg->context,
			                    (DRAW_NINE_GRID_ORDER*)msg->wParam);

		case PrimaryUpdate_MultiDstBlt:
			return IFCALLRESULT(TRUE, proxy->MultiDstBlt, msg->context,
			                    (MULTI_DSTBLT_ORDER*)msg->wParam);

		case PrimaryUpdate_MultiPatBlt:
			return IFCALLRESULT(TRUE, proxy->MultiPatBlt, msg->context,
			                    (MULTI_PATBLT_ORDER*)msg->wParam);

		case PrimaryUpdate_MultiScrBlt:
			return IFCALLRESULT(TRUE, proxy->MultiScrBlt, msg->context,
			                    (MULTI_SCRBLT_ORDER*)msg->wParam);

		case PrimaryUpdate_MultiOpaqueRect:
			return IFCALLRESULT(TRUE, proxy->MultiOpaqueRect, msg->context,
			                    (MULTI_OPAQUE_RECT_ORDER*)msg->wParam);

		case PrimaryUpdate_MultiDrawNineGrid:
			return IFCALLRESULT(TRUE, proxy->MultiDrawNineGrid, msg->context,
			                    (MULTI_DRAW_NINE_GRID_ORDER*)msg->wParam);

		case PrimaryUpdate_LineTo:
			return IFCALLRESULT(TRUE, proxy->LineTo, msg->context, (LINE_TO_ORDER*)msg->wParam);

		case PrimaryUpdate_Polyline:
			return IFCALLRESULT(TRUE, proxy->Polyline, msg->context, (POLYLINE_ORDER*)msg->wParam);

		case PrimaryUpdate_MemBlt:
			return IFCALLRESULT(TRUE, proxy->MemBlt, msg->context, (MEMBLT_ORDER*)msg->wParam);

		case PrimaryUpdate_Mem3Blt:
			return IFCALLRESULT(TRUE, proxy->Mem3Blt, msg->context, (MEM3BLT_ORDER*)msg->wParam);

		case PrimaryUpdate_SaveBitmap:
			return IFCALLRESULT(TRUE, proxy->SaveBitmap, msg->context,
			                    (SAVE_BITMAP_ORDER*)msg->wParam);

		case PrimaryUpdate_GlyphIndex:
			return IFCALLRESULT(TRUE, proxy->GlyphIndex, msg->context,
			                    (GLYPH_INDEX_ORDER*)msg->wParam);

		case PrimaryUpdate_FastIndex:
			return IFCALLRESULT(TRUE, proxy->FastIndex, msg->context,
			                    (FAST_INDEX_ORDER*)msg->wParam);

		case PrimaryUpdate_FastGlyph:
			return IFCALLRESULT(TRUE, proxy->FastGlyph, msg->context,
			                    (FAST_GLYPH_ORDER*)msg->wParam);

		case PrimaryUpdate_PolygonSC:
			return IFCALLRESULT(TRUE, proxy->PolygonSC, msg->context,
			                    (POLYGON_SC_ORDER*)msg->wParam);

		case PrimaryUpdate_PolygonCB:
			return IFCALLRESULT(TRUE, proxy->PolygonCB, msg->context,
			                    (POLYGON_CB_ORDER*)msg->wParam);

		case PrimaryUpdate_EllipseSC:
			return IFCALLRESULT(TRUE, proxy->EllipseSC, msg->context,
			                    (ELLIPSE_SC_ORDER*)msg->wParam);

		case PrimaryUpdate_EllipseCB:
			return IFCALLRESULT(TRUE, proxy->EllipseCB, msg->context,
			                    (ELLIPSE_CB_ORDER*)msg->wParam);

		default:
			return FALSE;
	}
}

static BOOL update_message_free_secondary_update_class(wMessage* msg, int type)
{
	rdpContext* context;

	if (!msg)
		return FALSE;

	context = msg->context;

	switch (type)
	{
		case SecondaryUpdate_CacheBitmap:
		{
			CACHE_BITMAP_ORDER* wParam = (CACHE_BITMAP_ORDER*)msg->wParam;
			free_cache_bitmap_order(context, wParam);
		}
		break;

		case SecondaryUpdate_CacheBitmapV2:
		{
			CACHE_BITMAP_V2_ORDER* wParam = (CACHE_BITMAP_V2_ORDER*)msg->wParam;
			free_cache_bitmap_v2_order(context, wParam);
		}
		break;

		case SecondaryUpdate_CacheBitmapV3:
		{
			CACHE_BITMAP_V3_ORDER* wParam = (CACHE_BITMAP_V3_ORDER*)msg->wParam;
			free_cache_bitmap_v3_order(context, wParam);
		}
		break;

		case SecondaryUpdate_CacheColorTable:
		{
			CACHE_COLOR_TABLE_ORDER* wParam = (CACHE_COLOR_TABLE_ORDER*)msg->wParam;
			free_cache_color_table_order(context, wParam);
		}
		break;

		case SecondaryUpdate_CacheGlyph:
		{
			CACHE_GLYPH_ORDER* wParam = (CACHE_GLYPH_ORDER*)msg->wParam;
			free_cache_glyph_order(context, wParam);
		}
		break;

		case SecondaryUpdate_CacheGlyphV2:
		{
			CACHE_GLYPH_V2_ORDER* wParam = (CACHE_GLYPH_V2_ORDER*)msg->wParam;
			free_cache_glyph_v2_order(context, wParam);
		}
		break;

		case SecondaryUpdate_CacheBrush:
		{
			CACHE_BRUSH_ORDER* wParam = (CACHE_BRUSH_ORDER*)msg->wParam;
			free_cache_brush_order(context, wParam);
		}
		break;

		default:
			return FALSE;
	}

	return TRUE;
}

static BOOL update_message_process_secondary_update_class(rdpUpdateProxy* proxy, wMessage* msg,
                                                          int type)
{
	if (!proxy || !msg)
		return FALSE;

	switch (type)
	{
		case SecondaryUpdate_CacheBitmap:
			return IFCALLRESULT(TRUE, proxy->CacheBitmap, msg->context,
			                    (CACHE_BITMAP_ORDER*)msg->wParam);

		case SecondaryUpdate_CacheBitmapV2:
			return IFCALLRESULT(TRUE, proxy->CacheBitmapV2, msg->context,
			                    (CACHE_BITMAP_V2_ORDER*)msg->wParam);

		case SecondaryUpdate_CacheBitmapV3:
			return IFCALLRESULT(TRUE, proxy->CacheBitmapV3, msg->context,
			                    (CACHE_BITMAP_V3_ORDER*)msg->wParam);

		case SecondaryUpdate_CacheColorTable:
			return IFCALLRESULT(TRUE, proxy->CacheColorTable, msg->context,
			                    (CACHE_COLOR_TABLE_ORDER*)msg->wParam);

		case SecondaryUpdate_CacheGlyph:
			return IFCALLRESULT(TRUE, proxy->CacheGlyph, msg->context,
			                    (CACHE_GLYPH_ORDER*)msg->wParam);

		case SecondaryUpdate_CacheGlyphV2:
			return IFCALLRESULT(TRUE, proxy->CacheGlyphV2, msg->context,
			                    (CACHE_GLYPH_V2_ORDER*)msg->wParam);

		case SecondaryUpdate_CacheBrush:
			return IFCALLRESULT(TRUE, proxy->CacheBrush, msg->context,
			                    (CACHE_BRUSH_ORDER*)msg->wParam);

		default:
			return FALSE;
	}
}

static BOOL update_message_free_altsec_update_class(wMessage* msg, int type)
{
	if (!msg)
		return FALSE;

	switch (type)
	{
		case AltSecUpdate_CreateOffscreenBitmap:
		{
			CREATE_OFFSCREEN_BITMAP_ORDER* wParam = (CREATE_OFFSCREEN_BITMAP_ORDER*)msg->wParam;
			free(wParam->deleteList.indices);
			free(wParam);
		}
		break;

		case AltSecUpdate_SwitchSurface:
			free(msg->wParam);
			break;

		case AltSecUpdate_CreateNineGridBitmap:
			free(msg->wParam);
			break;

		case AltSecUpdate_FrameMarker:
			free(msg->wParam);
			break;

		case AltSecUpdate_StreamBitmapFirst:
			free(msg->wParam);
			break;

		case AltSecUpdate_StreamBitmapNext:
			free(msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusFirst:
			free(msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusNext:
			free(msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusEnd:
			free(msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusCacheFirst:
			free(msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusCacheNext:
			free(msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusCacheEnd:
			free(msg->wParam);
			break;

		default:
			return FALSE;
	}

	return TRUE;
}

static BOOL update_message_process_altsec_update_class(rdpUpdateProxy* proxy, wMessage* msg,
                                                       int type)
{
	if (!proxy || !msg)
		return FALSE;

	switch (type)
	{
		case AltSecUpdate_CreateOffscreenBitmap:
			return IFCALLRESULT(TRUE, proxy->CreateOffscreenBitmap, msg->context,
			                    (CREATE_OFFSCREEN_BITMAP_ORDER*)msg->wParam);

		case AltSecUpdate_SwitchSurface:
			return IFCALLRESULT(TRUE, proxy->SwitchSurface, msg->context,
			                    (SWITCH_SURFACE_ORDER*)msg->wParam);

		case AltSecUpdate_CreateNineGridBitmap:
			return IFCALLRESULT(TRUE, proxy->CreateNineGridBitmap, msg->context,
			                    (CREATE_NINE_GRID_BITMAP_ORDER*)msg->wParam);

		case AltSecUpdate_FrameMarker:
			return IFCALLRESULT(TRUE, proxy->FrameMarker, msg->context,
			                    (FRAME_MARKER_ORDER*)msg->wParam);

		case AltSecUpdate_StreamBitmapFirst:
			return IFCALLRESULT(TRUE, proxy->StreamBitmapFirst, msg->context,
			                    (STREAM_BITMAP_FIRST_ORDER*)msg->wParam);

		case AltSecUpdate_StreamBitmapNext:
			return IFCALLRESULT(TRUE, proxy->StreamBitmapNext, msg->context,
			                    (STREAM_BITMAP_NEXT_ORDER*)msg->wParam);

		case AltSecUpdate_DrawGdiPlusFirst:
			return IFCALLRESULT(TRUE, proxy->DrawGdiPlusFirst, msg->context,
			                    (DRAW_GDIPLUS_FIRST_ORDER*)msg->wParam);

		case AltSecUpdate_DrawGdiPlusNext:
			return IFCALLRESULT(TRUE, proxy->DrawGdiPlusNext, msg->context,
			                    (DRAW_GDIPLUS_NEXT_ORDER*)msg->wParam);

		case AltSecUpdate_DrawGdiPlusEnd:
			return IFCALLRESULT(TRUE, proxy->DrawGdiPlusEnd, msg->context,
			                    (DRAW_GDIPLUS_END_ORDER*)msg->wParam);

		case AltSecUpdate_DrawGdiPlusCacheFirst:
			return IFCALLRESULT(TRUE, proxy->DrawGdiPlusCacheFirst, msg->context,
			                    (DRAW_GDIPLUS_CACHE_FIRST_ORDER*)msg->wParam);

		case AltSecUpdate_DrawGdiPlusCacheNext:
			return IFCALLRESULT(TRUE, proxy->DrawGdiPlusCacheNext, msg->context,
			                    (DRAW_GDIPLUS_CACHE_NEXT_ORDER*)msg->wParam);

		case AltSecUpdate_DrawGdiPlusCacheEnd:
			return IFCALLRESULT(TRUE, proxy->DrawGdiPlusCacheEnd, msg->context,
			                    (DRAW_GDIPLUS_CACHE_END_ORDER*)msg->wParam);

		default:
			return FALSE;
	}
}

static BOOL update_message_free_window_update_class(wMessage* msg, int type)
{
	if (!msg)
		return FALSE;

	switch (type)
	{
		case WindowUpdate_WindowCreate:
			free(msg->wParam);
			free(msg->lParam);
			break;

		case WindowUpdate_WindowUpdate:
			free(msg->wParam);
			free(msg->lParam);
			break;

		case WindowUpdate_WindowIcon:
		{
			WINDOW_ORDER_INFO* orderInfo = (WINDOW_ORDER_INFO*)msg->wParam;
			WINDOW_ICON_ORDER* windowIcon = (WINDOW_ICON_ORDER*)msg->lParam;

			if (windowIcon->iconInfo->cbBitsColor > 0)
			{
				free(windowIcon->iconInfo->bitsColor);
			}

			if (windowIcon->iconInfo->cbBitsMask > 0)
			{
				free(windowIcon->iconInfo->bitsMask);
			}

			if (windowIcon->iconInfo->cbColorTable > 0)
			{
				free(windowIcon->iconInfo->colorTable);
			}

			free(orderInfo);
			free(windowIcon->iconInfo);
			free(windowIcon);
		}
		break;

		case WindowUpdate_WindowCachedIcon:
			free(msg->wParam);
			free(msg->lParam);
			break;

		case WindowUpdate_WindowDelete:
			free(msg->wParam);
			break;

		case WindowUpdate_NotifyIconCreate:
			free(msg->wParam);
			free(msg->lParam);
			break;

		case WindowUpdate_NotifyIconUpdate:
			free(msg->wParam);
			free(msg->lParam);
			break;

		case WindowUpdate_NotifyIconDelete:
			free(msg->wParam);
			break;

		case WindowUpdate_MonitoredDesktop:
		{
			MONITORED_DESKTOP_ORDER* lParam = (MONITORED_DESKTOP_ORDER*)msg->lParam;
			free(msg->wParam);
			free(lParam->windowIds);
			free(lParam);
		}
		break;

		case WindowUpdate_NonMonitoredDesktop:
			free(msg->wParam);
			break;

		default:
			return FALSE;
	}

	return TRUE;
}

static BOOL update_message_process_window_update_class(rdpUpdateProxy* proxy, wMessage* msg,
                                                       int type)
{
	if (!proxy || !msg)
		return FALSE;

	switch (type)
	{
		case WindowUpdate_WindowCreate:
			return IFCALLRESULT(TRUE, proxy->WindowCreate, msg->context,
			                    (WINDOW_ORDER_INFO*)msg->wParam, (WINDOW_STATE_ORDER*)msg->lParam);

		case WindowUpdate_WindowUpdate:
			return IFCALLRESULT(TRUE, proxy->WindowCreate, msg->context,
			                    (WINDOW_ORDER_INFO*)msg->wParam, (WINDOW_STATE_ORDER*)msg->lParam);

		case WindowUpdate_WindowIcon:
		{
			WINDOW_ORDER_INFO* orderInfo = (WINDOW_ORDER_INFO*)msg->wParam;
			WINDOW_ICON_ORDER* windowIcon = (WINDOW_ICON_ORDER*)msg->lParam;
			return IFCALLRESULT(TRUE, proxy->WindowIcon, msg->context, orderInfo, windowIcon);
		}

		case WindowUpdate_WindowCachedIcon:
			return IFCALLRESULT(TRUE, proxy->WindowCachedIcon, msg->context,
			                    (WINDOW_ORDER_INFO*)msg->wParam,
			                    (WINDOW_CACHED_ICON_ORDER*)msg->lParam);

		case WindowUpdate_WindowDelete:
			return IFCALLRESULT(TRUE, proxy->WindowDelete, msg->context,
			                    (WINDOW_ORDER_INFO*)msg->wParam);

		case WindowUpdate_NotifyIconCreate:
			return IFCALLRESULT(TRUE, proxy->NotifyIconCreate, msg->context,
			                    (WINDOW_ORDER_INFO*)msg->wParam,
			                    (NOTIFY_ICON_STATE_ORDER*)msg->lParam);

		case WindowUpdate_NotifyIconUpdate:
			return IFCALLRESULT(TRUE, proxy->NotifyIconUpdate, msg->context,
			                    (WINDOW_ORDER_INFO*)msg->wParam,
			                    (NOTIFY_ICON_STATE_ORDER*)msg->lParam);

		case WindowUpdate_NotifyIconDelete:
			return IFCALLRESULT(TRUE, proxy->NotifyIconDelete, msg->context,
			                    (WINDOW_ORDER_INFO*)msg->wParam);

		case WindowUpdate_MonitoredDesktop:
			return IFCALLRESULT(TRUE, proxy->MonitoredDesktop, msg->context,
			                    (WINDOW_ORDER_INFO*)msg->wParam,
			                    (MONITORED_DESKTOP_ORDER*)msg->lParam);

		case WindowUpdate_NonMonitoredDesktop:
			return IFCALLRESULT(TRUE, proxy->NonMonitoredDesktop, msg->context,
			                    (WINDOW_ORDER_INFO*)msg->wParam);

		default:
			return FALSE;
	}
}

static BOOL update_message_free_pointer_update_class(wMessage* msg, int type)
{
	rdpContext* context;

	if (!msg)
		return FALSE;

	context = msg->context;

	switch (type)
	{
		case PointerUpdate_PointerPosition:
		{
			POINTER_POSITION_UPDATE* wParam = (POINTER_POSITION_UPDATE*)msg->wParam;
			free_pointer_position_update(context, wParam);
		}
		break;

		case PointerUpdate_PointerSystem:
		{
			POINTER_SYSTEM_UPDATE* wParam = (POINTER_SYSTEM_UPDATE*)msg->wParam;
			free_pointer_system_update(context, wParam);
		}
		break;

		case PointerUpdate_PointerCached:
		{
			POINTER_CACHED_UPDATE* wParam = (POINTER_CACHED_UPDATE*)msg->wParam;
			free_pointer_cached_update(context, wParam);
		}
		break;

		case PointerUpdate_PointerColor:
		{
			POINTER_COLOR_UPDATE* wParam = (POINTER_COLOR_UPDATE*)msg->wParam;
			free_pointer_color_update(context, wParam);
		}
		break;

		case PointerUpdate_PointerNew:
		{
			POINTER_NEW_UPDATE* wParam = (POINTER_NEW_UPDATE*)msg->wParam;
			free_pointer_new_update(context, wParam);
		}
		break;

		default:
			return FALSE;
	}

	return TRUE;
}

static BOOL update_message_process_pointer_update_class(rdpUpdateProxy* proxy, wMessage* msg,
                                                        int type)
{
	if (!proxy || !msg)
		return FALSE;

	switch (type)
	{
		case PointerUpdate_PointerPosition:
			return IFCALLRESULT(TRUE, proxy->PointerPosition, msg->context,
			                    (POINTER_POSITION_UPDATE*)msg->wParam);

		case PointerUpdate_PointerSystem:
			return IFCALLRESULT(TRUE, proxy->PointerSystem, msg->context,
			                    (POINTER_SYSTEM_UPDATE*)msg->wParam);

		case PointerUpdate_PointerColor:
			return IFCALLRESULT(TRUE, proxy->PointerColor, msg->context,
			                    (POINTER_COLOR_UPDATE*)msg->wParam);

		case PointerUpdate_PointerNew:
			return IFCALLRESULT(TRUE, proxy->PointerNew, msg->context,
			                    (POINTER_NEW_UPDATE*)msg->wParam);

		case PointerUpdate_PointerCached:
			return IFCALLRESULT(TRUE, proxy->PointerCached, msg->context,
			                    (POINTER_CACHED_UPDATE*)msg->wParam);

		default:
			return FALSE;
	}
}

static BOOL update_message_free_class(wMessage* msg, int msgClass, int msgType)
{
	BOOL status = FALSE;

	switch (msgClass)
	{
		case Update_Class:
			status = update_message_free_update_class(msg, msgType);
			break;

		case PrimaryUpdate_Class:
			status = update_message_free_primary_update_class(msg, msgType);
			break;

		case SecondaryUpdate_Class:
			status = update_message_free_secondary_update_class(msg, msgType);
			break;

		case AltSecUpdate_Class:
			status = update_message_free_altsec_update_class(msg, msgType);
			break;

		case WindowUpdate_Class:
			status = update_message_free_window_update_class(msg, msgType);
			break;

		case PointerUpdate_Class:
			status = update_message_free_pointer_update_class(msg, msgType);
			break;

		default:
			break;
	}

	if (!status)
		WLog_ERR(TAG, "Unknown message: class: %d type: %d", msgClass, msgType);

	return status;
}

static int update_message_process_class(rdpUpdateProxy* proxy, wMessage* msg, int msgClass,
                                        int msgType)
{
	BOOL status = FALSE;

	switch (msgClass)
	{
		case Update_Class:
			status = update_message_process_update_class(proxy, msg, msgType);
			break;

		case PrimaryUpdate_Class:
			status = update_message_process_primary_update_class(proxy, msg, msgType);
			break;

		case SecondaryUpdate_Class:
			status = update_message_process_secondary_update_class(proxy, msg, msgType);
			break;

		case AltSecUpdate_Class:
			status = update_message_process_altsec_update_class(proxy, msg, msgType);
			break;

		case WindowUpdate_Class:
			status = update_message_process_window_update_class(proxy, msg, msgType);
			break;

		case PointerUpdate_Class:
			status = update_message_process_pointer_update_class(proxy, msg, msgType);
			break;

		default:
			status = FALSE;
			break;
	}

	if (!status)
	{
		WLog_ERR(TAG, "message: class: %d type: %d failed", msgClass, msgType);
		return -1;
	}

	return 0;
}

int update_message_queue_process_message(rdpUpdate* update, wMessage* message)
{
	int status;
	int msgClass;
	int msgType;
	rdp_update_internal* up = update_cast(update);

	if (!update || !message)
		return -1;

	if (message->id == WMQ_QUIT)
		return 0;

	msgClass = GetMessageClass(message->id);
	msgType = GetMessageType(message->id);
	status = update_message_process_class(up->proxy, message, msgClass, msgType);
	update_message_free_class(message, msgClass, msgType);

	if (status < 0)
		return -1;

	return 1;
}

int update_message_queue_free_message(wMessage* message)
{
	int msgClass;
	int msgType;

	if (!message)
		return -1;

	if (message->id == WMQ_QUIT)
		return 0;

	msgClass = GetMessageClass(message->id);
	msgType = GetMessageType(message->id);
	return update_message_free_class(message, msgClass, msgType);
}

int update_message_queue_process_pending_messages(rdpUpdate* update)
{
	int status;
	wMessage message;
	wMessageQueue* queue;
	rdp_update_internal* up = update_cast(update);

	status = 1;
	queue = up->queue;

	while (MessageQueue_Peek(queue, &message, TRUE))
	{
		status = update_message_queue_process_message(update, &message);

		if (!status)
			break;
	}

	return status;
}

static BOOL update_message_register_interface(rdpUpdateProxy* message, rdpUpdate* update)
{
	rdpPrimaryUpdate* primary;
	rdpSecondaryUpdate* secondary;
	rdpAltSecUpdate* altsec;
	rdpWindowUpdate* window;
	rdpPointerUpdate* pointer;

	if (!message || !update)
		return FALSE;

	primary = update->primary;
	secondary = update->secondary;
	altsec = update->altsec;
	window = update->window;
	pointer = update->pointer;

	if (!primary || !secondary || !altsec || !window || !pointer)
		return FALSE;

	/* Update */
	message->BeginPaint = update->BeginPaint;
	message->EndPaint = update->EndPaint;
	message->SetBounds = update->SetBounds;
	message->Synchronize = update->Synchronize;
	message->DesktopResize = update->DesktopResize;
	message->BitmapUpdate = update->BitmapUpdate;
	message->Palette = update->Palette;
	message->PlaySound = update->PlaySound;
	message->SetKeyboardIndicators = update->SetKeyboardIndicators;
	message->SetKeyboardImeStatus = update->SetKeyboardImeStatus;
	message->RefreshRect = update->RefreshRect;
	message->SuppressOutput = update->SuppressOutput;
	message->SurfaceCommand = update->SurfaceCommand;
	message->SurfaceBits = update->SurfaceBits;
	message->SurfaceFrameMarker = update->SurfaceFrameMarker;
	message->SurfaceFrameAcknowledge = update->SurfaceFrameAcknowledge;
	update->BeginPaint = update_message_BeginPaint;
	update->EndPaint = update_message_EndPaint;
	update->SetBounds = update_message_SetBounds;
	update->Synchronize = update_message_Synchronize;
	update->DesktopResize = update_message_DesktopResize;
	update->BitmapUpdate = update_message_BitmapUpdate;
	update->Palette = update_message_Palette;
	update->PlaySound = update_message_PlaySound;
	update->SetKeyboardIndicators = update_message_SetKeyboardIndicators;
	update->SetKeyboardImeStatus = update_message_SetKeyboardImeStatus;
	update->RefreshRect = update_message_RefreshRect;
	update->SuppressOutput = update_message_SuppressOutput;
	update->SurfaceCommand = update_message_SurfaceCommand;
	update->SurfaceBits = update_message_SurfaceBits;
	update->SurfaceFrameMarker = update_message_SurfaceFrameMarker;
	update->SurfaceFrameAcknowledge = update_message_SurfaceFrameAcknowledge;
	/* Primary Update */
	message->DstBlt = primary->DstBlt;
	message->PatBlt = primary->PatBlt;
	message->ScrBlt = primary->ScrBlt;
	message->OpaqueRect = primary->OpaqueRect;
	message->DrawNineGrid = primary->DrawNineGrid;
	message->MultiDstBlt = primary->MultiDstBlt;
	message->MultiPatBlt = primary->MultiPatBlt;
	message->MultiScrBlt = primary->MultiScrBlt;
	message->MultiOpaqueRect = primary->MultiOpaqueRect;
	message->MultiDrawNineGrid = primary->MultiDrawNineGrid;
	message->LineTo = primary->LineTo;
	message->Polyline = primary->Polyline;
	message->MemBlt = primary->MemBlt;
	message->Mem3Blt = primary->Mem3Blt;
	message->SaveBitmap = primary->SaveBitmap;
	message->GlyphIndex = primary->GlyphIndex;
	message->FastIndex = primary->FastIndex;
	message->FastGlyph = primary->FastGlyph;
	message->PolygonSC = primary->PolygonSC;
	message->PolygonCB = primary->PolygonCB;
	message->EllipseSC = primary->EllipseSC;
	message->EllipseCB = primary->EllipseCB;
	primary->DstBlt = update_message_DstBlt;
	primary->PatBlt = update_message_PatBlt;
	primary->ScrBlt = update_message_ScrBlt;
	primary->OpaqueRect = update_message_OpaqueRect;
	primary->DrawNineGrid = update_message_DrawNineGrid;
	primary->MultiDstBlt = update_message_MultiDstBlt;
	primary->MultiPatBlt = update_message_MultiPatBlt;
	primary->MultiScrBlt = update_message_MultiScrBlt;
	primary->MultiOpaqueRect = update_message_MultiOpaqueRect;
	primary->MultiDrawNineGrid = update_message_MultiDrawNineGrid;
	primary->LineTo = update_message_LineTo;
	primary->Polyline = update_message_Polyline;
	primary->MemBlt = update_message_MemBlt;
	primary->Mem3Blt = update_message_Mem3Blt;
	primary->SaveBitmap = update_message_SaveBitmap;
	primary->GlyphIndex = update_message_GlyphIndex;
	primary->FastIndex = update_message_FastIndex;
	primary->FastGlyph = update_message_FastGlyph;
	primary->PolygonSC = update_message_PolygonSC;
	primary->PolygonCB = update_message_PolygonCB;
	primary->EllipseSC = update_message_EllipseSC;
	primary->EllipseCB = update_message_EllipseCB;
	/* Secondary Update */
	message->CacheBitmap = secondary->CacheBitmap;
	message->CacheBitmapV2 = secondary->CacheBitmapV2;
	message->CacheBitmapV3 = secondary->CacheBitmapV3;
	message->CacheColorTable = secondary->CacheColorTable;
	message->CacheGlyph = secondary->CacheGlyph;
	message->CacheGlyphV2 = secondary->CacheGlyphV2;
	message->CacheBrush = secondary->CacheBrush;
	secondary->CacheBitmap = update_message_CacheBitmap;
	secondary->CacheBitmapV2 = update_message_CacheBitmapV2;
	secondary->CacheBitmapV3 = update_message_CacheBitmapV3;
	secondary->CacheColorTable = update_message_CacheColorTable;
	secondary->CacheGlyph = update_message_CacheGlyph;
	secondary->CacheGlyphV2 = update_message_CacheGlyphV2;
	secondary->CacheBrush = update_message_CacheBrush;
	/* Alternate Secondary Update */
	message->CreateOffscreenBitmap = altsec->CreateOffscreenBitmap;
	message->SwitchSurface = altsec->SwitchSurface;
	message->CreateNineGridBitmap = altsec->CreateNineGridBitmap;
	message->FrameMarker = altsec->FrameMarker;
	message->StreamBitmapFirst = altsec->StreamBitmapFirst;
	message->StreamBitmapNext = altsec->StreamBitmapNext;
	message->DrawGdiPlusFirst = altsec->DrawGdiPlusFirst;
	message->DrawGdiPlusNext = altsec->DrawGdiPlusNext;
	message->DrawGdiPlusEnd = altsec->DrawGdiPlusEnd;
	message->DrawGdiPlusCacheFirst = altsec->DrawGdiPlusCacheFirst;
	message->DrawGdiPlusCacheNext = altsec->DrawGdiPlusCacheNext;
	message->DrawGdiPlusCacheEnd = altsec->DrawGdiPlusCacheEnd;
	altsec->CreateOffscreenBitmap = update_message_CreateOffscreenBitmap;
	altsec->SwitchSurface = update_message_SwitchSurface;
	altsec->CreateNineGridBitmap = update_message_CreateNineGridBitmap;
	altsec->FrameMarker = update_message_FrameMarker;
	altsec->StreamBitmapFirst = update_message_StreamBitmapFirst;
	altsec->StreamBitmapNext = update_message_StreamBitmapNext;
	altsec->DrawGdiPlusFirst = update_message_DrawGdiPlusFirst;
	altsec->DrawGdiPlusNext = update_message_DrawGdiPlusNext;
	altsec->DrawGdiPlusEnd = update_message_DrawGdiPlusEnd;
	altsec->DrawGdiPlusCacheFirst = update_message_DrawGdiPlusCacheFirst;
	altsec->DrawGdiPlusCacheNext = update_message_DrawGdiPlusCacheNext;
	altsec->DrawGdiPlusCacheEnd = update_message_DrawGdiPlusCacheEnd;
	/* Window Update */
	message->WindowCreate = window->WindowCreate;
	message->WindowUpdate = window->WindowUpdate;
	message->WindowIcon = window->WindowIcon;
	message->WindowCachedIcon = window->WindowCachedIcon;
	message->WindowDelete = window->WindowDelete;
	message->NotifyIconCreate = window->NotifyIconCreate;
	message->NotifyIconUpdate = window->NotifyIconUpdate;
	message->NotifyIconDelete = window->NotifyIconDelete;
	message->MonitoredDesktop = window->MonitoredDesktop;
	message->NonMonitoredDesktop = window->NonMonitoredDesktop;
	window->WindowCreate = update_message_WindowCreate;
	window->WindowUpdate = update_message_WindowUpdate;
	window->WindowIcon = update_message_WindowIcon;
	window->WindowCachedIcon = update_message_WindowCachedIcon;
	window->WindowDelete = update_message_WindowDelete;
	window->NotifyIconCreate = update_message_NotifyIconCreate;
	window->NotifyIconUpdate = update_message_NotifyIconUpdate;
	window->NotifyIconDelete = update_message_NotifyIconDelete;
	window->MonitoredDesktop = update_message_MonitoredDesktop;
	window->NonMonitoredDesktop = update_message_NonMonitoredDesktop;
	/* Pointer Update */
	message->PointerPosition = pointer->PointerPosition;
	message->PointerSystem = pointer->PointerSystem;
	message->PointerColor = pointer->PointerColor;
	message->PointerLarge = pointer->PointerLarge;
	message->PointerNew = pointer->PointerNew;
	message->PointerCached = pointer->PointerCached;
	pointer->PointerPosition = update_message_PointerPosition;
	pointer->PointerSystem = update_message_PointerSystem;
	pointer->PointerColor = update_message_PointerColor;
	pointer->PointerLarge = update_message_PointerLarge;
	pointer->PointerNew = update_message_PointerNew;
	pointer->PointerCached = update_message_PointerCached;
	return TRUE;
}

static DWORD WINAPI update_message_proxy_thread(LPVOID arg)
{
	rdpUpdate* update = (rdpUpdate*)arg;
	wMessage message;
	rdp_update_internal* up = update_cast(update);

	while (MessageQueue_Wait(up->queue))
	{
		int status = 0;

		if (MessageQueue_Peek(up->queue, &message, TRUE))
			status = update_message_queue_process_message(update, &message);

		if (!status)
			break;
	}

	ExitThread(0);
	return 0;
}

rdpUpdateProxy* update_message_proxy_new(rdpUpdate* update)
{
	rdpUpdateProxy* message;

	if (!update)
		return NULL;

	if (!(message = (rdpUpdateProxy*)calloc(1, sizeof(rdpUpdateProxy))))
		return NULL;

	message->update = update;
	update_message_register_interface(message, update);

	if (!(message->thread = CreateThread(NULL, 0, update_message_proxy_thread, update, 0, NULL)))
	{
		WLog_ERR(TAG, "Failed to create proxy thread");
		free(message);
		return NULL;
	}

	return message;
}

void update_message_proxy_free(rdpUpdateProxy* message)
{
	if (message)
	{
		rdp_update_internal* up = update_cast(message->update);
		if (MessageQueue_PostQuit(up->queue, 0))
			WaitForSingleObject(message->thread, INFINITE);

		CloseHandle(message->thread);
		free(message);
	}
}

/* Event Queue */
static int input_message_free_input_class(wMessage* msg, int type)
{
	int status = 0;

	WINPR_UNUSED(msg);

	switch (type)
	{
		case Input_SynchronizeEvent:
			break;

		case Input_KeyboardEvent:
			break;

		case Input_UnicodeKeyboardEvent:
			break;

		case Input_MouseEvent:
			break;

		case Input_ExtendedMouseEvent:
			break;

		case Input_FocusInEvent:
			break;

		case Input_KeyboardPauseEvent:
			break;

		default:
			status = -1;
			break;
	}

	return status;
}

static int input_message_process_input_class(rdpInputProxy* proxy, wMessage* msg, int type)
{
	int status = 0;

	if (!proxy || !msg)
		return -1;

	switch (type)
	{
		case Input_SynchronizeEvent:
			IFCALL(proxy->SynchronizeEvent, msg->context, (UINT32)(size_t)msg->wParam);
			break;

		case Input_KeyboardEvent:
			IFCALL(proxy->KeyboardEvent, msg->context, (UINT16)(size_t)msg->wParam,
			       (UINT8)(size_t)msg->lParam);
			break;

		case Input_UnicodeKeyboardEvent:
			IFCALL(proxy->UnicodeKeyboardEvent, msg->context, (UINT16)(size_t)msg->wParam,
			       (UINT16)(size_t)msg->lParam);
			break;

		case Input_MouseEvent:
		{
			UINT32 pos;
			UINT16 x, y;
			pos = (UINT32)(size_t)msg->lParam;
			x = ((pos & 0xFFFF0000) >> 16);
			y = (pos & 0x0000FFFF);
			IFCALL(proxy->MouseEvent, msg->context, (UINT16)(size_t)msg->wParam, x, y);
		}
		break;

		case Input_ExtendedMouseEvent:
		{
			UINT32 pos;
			UINT16 x, y;
			pos = (UINT32)(size_t)msg->lParam;
			x = ((pos & 0xFFFF0000) >> 16);
			y = (pos & 0x0000FFFF);
			IFCALL(proxy->ExtendedMouseEvent, msg->context, (UINT16)(size_t)msg->wParam, x, y);
		}
		break;

		case Input_FocusInEvent:
			IFCALL(proxy->FocusInEvent, msg->context, (UINT16)(size_t)msg->wParam);
			break;

		case Input_KeyboardPauseEvent:
			IFCALL(proxy->KeyboardPauseEvent, msg->context);
			break;

		default:
			status = -1;
			break;
	}

	return status;
}

static int input_message_free_class(wMessage* msg, int msgClass, int msgType)
{
	int status = 0;

	switch (msgClass)
	{
		case Input_Class:
			status = input_message_free_input_class(msg, msgType);
			break;

		default:
			status = -1;
			break;
	}

	if (status < 0)
		WLog_ERR(TAG, "Unknown event: class: %d type: %d", msgClass, msgType);

	return status;
}

static int input_message_process_class(rdpInputProxy* proxy, wMessage* msg, int msgClass,
                                       int msgType)
{
	int status = 0;

	switch (msgClass)
	{
		case Input_Class:
			status = input_message_process_input_class(proxy, msg, msgType);
			break;

		default:
			status = -1;
			break;
	}

	if (status < 0)
		WLog_ERR(TAG, "Unknown event: class: %d type: %d", msgClass, msgType);

	return status;
}

int input_message_queue_free_message(wMessage* message)
{
	int status;
	int msgClass;
	int msgType;

	if (!message)
		return -1;

	if (message->id == WMQ_QUIT)
		return 0;

	msgClass = GetMessageClass(message->id);
	msgType = GetMessageType(message->id);
	status = input_message_free_class(message, msgClass, msgType);

	if (status < 0)
		return -1;

	return 1;
}

int input_message_queue_process_message(rdpInput* input, wMessage* message)
{
	int status;
	int msgClass;
	int msgType;
	rdp_input_internal* in = input_cast(input);

	if (!message)
		return -1;

	if (message->id == WMQ_QUIT)
		return 0;

	msgClass = GetMessageClass(message->id);
	msgType = GetMessageType(message->id);
	status = input_message_process_class(in->proxy, message, msgClass, msgType);
	input_message_free_class(message, msgClass, msgType);

	if (status < 0)
		return -1;

	return 1;
}

int input_message_queue_process_pending_messages(rdpInput* input)
{
	int count;
	int status;
	wMessage message;
	wMessageQueue* queue;
	rdp_input_internal* in = input_cast(input);

	if (!in->queue)
		return -1;

	count = 0;
	status = 1;
	queue = in->queue;

	while (MessageQueue_Peek(queue, &message, TRUE))
	{
		status = input_message_queue_process_message(input, &message);

		if (!status)
			break;

		count++;
	}

	return status;
}
