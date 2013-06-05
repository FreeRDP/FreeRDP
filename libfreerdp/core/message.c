/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Asynchronous Message Queue
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "rdp.h"
#include "message.h"
#include "transport.h"

#include <freerdp/freerdp.h>

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

#define WITH_STREAM_POOL	1

/* Update */

static void update_message_BeginPaint(rdpContext* context)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, BeginPaint), NULL, NULL);
}

static void update_message_EndPaint(rdpContext* context)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, EndPaint), NULL, NULL);
}

static void update_message_SetBounds(rdpContext* context, rdpBounds* bounds)
{
	rdpBounds* wParam = NULL;

	if (bounds)
	{
		wParam = (rdpBounds*) malloc(sizeof(rdpBounds));
		CopyMemory(wParam, bounds, sizeof(rdpBounds));
	}

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SetBounds), (void*) wParam, NULL);
}

static void update_message_Synchronize(rdpContext* context)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, Synchronize), NULL, NULL);
}

static void update_message_DesktopResize(rdpContext* context)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, DesktopResize), NULL, NULL);
}

static void update_message_BitmapUpdate(rdpContext* context, BITMAP_UPDATE* bitmap)
{
	int index;
	BITMAP_UPDATE* wParam;

	wParam = (BITMAP_UPDATE*) malloc(sizeof(BITMAP_UPDATE));

	wParam->number = bitmap->number;
	wParam->count = wParam->number;

	wParam->rectangles = (BITMAP_DATA*) malloc(sizeof(BITMAP_DATA) * wParam->number);
	CopyMemory(wParam->rectangles, bitmap->rectangles, sizeof(BITMAP_DATA) * wParam->number);

	for (index = 0; index < wParam->number; index++)
	{
#ifdef WITH_STREAM_POOL
		StreamPool_AddRef(context->rdp->transport->ReceivePool, bitmap->rectangles[index].bitmapDataStream);
#else
		wParam->rectangles[index].bitmapDataStream = (BYTE*) malloc(wParam->rectangles[index].bitmapLength);
		CopyMemory(wParam->rectangles[index].bitmapDataStream, bitmap->rectangles[index].bitmapDataStream,
				wParam->rectangles[index].bitmapLength);
#endif
	}

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, BitmapUpdate), (void*) wParam, NULL);
}

static void update_message_Palette(rdpContext* context, PALETTE_UPDATE* palette)
{
	PALETTE_UPDATE* wParam;

	wParam = (PALETTE_UPDATE*) malloc(sizeof(PALETTE_UPDATE));
	CopyMemory(wParam, palette, sizeof(PALETTE_UPDATE));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, Palette), (void*) wParam, NULL);
}

static void update_message_PlaySound(rdpContext* context, PLAY_SOUND_UPDATE* playSound)
{
	PLAY_SOUND_UPDATE* wParam;

	wParam = (PLAY_SOUND_UPDATE*) malloc(sizeof(PLAY_SOUND_UPDATE));
	CopyMemory(wParam, playSound, sizeof(PLAY_SOUND_UPDATE));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, PlaySound), (void*) wParam, NULL);
}

static void update_message_RefreshRect(rdpContext* context, BYTE count, RECTANGLE_16* areas)
{
	RECTANGLE_16* lParam;

	lParam = (RECTANGLE_16*) malloc(sizeof(RECTANGLE_16) * count);
	CopyMemory(lParam, areas, sizeof(RECTANGLE_16) * count);

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, RefreshRect), (void*) (size_t) count, (void*) lParam);
}

static void update_message_SuppressOutput(rdpContext* context, BYTE allow, RECTANGLE_16* area)
{
	RECTANGLE_16* lParam = NULL;

	if (area)
	{
		lParam = (RECTANGLE_16*) malloc(sizeof(RECTANGLE_16));
		CopyMemory(lParam, area, sizeof(RECTANGLE_16));
	}

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SuppressOutput), (void*) (size_t) allow, (void*) lParam);
}

static void update_message_SurfaceCommand(rdpContext* context, wStream* s)
{
	wStream* wParam;

	wParam = (wStream*) malloc(sizeof(wStream));

	wParam->capacity = Stream_Capacity(s);
	wParam->buffer = (BYTE*) malloc(wParam->capacity);
	wParam->pointer = wParam->buffer;

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SurfaceCommand), (void*) wParam, NULL);
}

static void update_message_SurfaceBits(rdpContext* context, SURFACE_BITS_COMMAND* surfaceBitsCommand)
{
	SURFACE_BITS_COMMAND* wParam;

	wParam = (SURFACE_BITS_COMMAND*) malloc(sizeof(SURFACE_BITS_COMMAND));
	CopyMemory(wParam, surfaceBitsCommand, sizeof(SURFACE_BITS_COMMAND));

#ifdef WITH_STREAM_POOL
	StreamPool_AddRef(context->rdp->transport->ReceivePool, surfaceBitsCommand->bitmapData);
#else
	wParam->bitmapData = (BYTE*) malloc(wParam->bitmapDataLength);
	CopyMemory(wParam->bitmapData, surfaceBitsCommand->bitmapData, wParam->bitmapDataLength);
#endif

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SurfaceBits), (void*) wParam, NULL);
}

static void update_message_SurfaceFrameMarker(rdpContext* context, SURFACE_FRAME_MARKER* surfaceFrameMarker)
{
	SURFACE_FRAME_MARKER* wParam;

	wParam = (SURFACE_FRAME_MARKER*) malloc(sizeof(SURFACE_FRAME_MARKER));
	CopyMemory(wParam, surfaceFrameMarker, sizeof(SURFACE_FRAME_MARKER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SurfaceFrameMarker), (void*) wParam, NULL);
}

static void update_message_SurfaceFrameAcknowledge(rdpContext* context, UINT32 frameId)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SurfaceFrameAcknowledge), (void*) (size_t) frameId, NULL);
}

/* Primary Update */

static void update_message_DstBlt(rdpContext* context, DSTBLT_ORDER* dstBlt)
{
	DSTBLT_ORDER* wParam;

	wParam = (DSTBLT_ORDER*) malloc(sizeof(DSTBLT_ORDER));
	CopyMemory(wParam, dstBlt, sizeof(DSTBLT_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, DstBlt), (void*) wParam, NULL);
}

static void update_message_PatBlt(rdpContext* context, PATBLT_ORDER* patBlt)
{
	PATBLT_ORDER* wParam;

	wParam = (PATBLT_ORDER*) malloc(sizeof(PATBLT_ORDER));
	CopyMemory(wParam, patBlt, sizeof(PATBLT_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, PatBlt), (void*) wParam, NULL);
}

static void update_message_ScrBlt(rdpContext* context, SCRBLT_ORDER* scrBlt)
{
	SCRBLT_ORDER* wParam;

	wParam = (SCRBLT_ORDER*) malloc(sizeof(SCRBLT_ORDER));
	CopyMemory(wParam, scrBlt, sizeof(SCRBLT_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, ScrBlt), (void*) wParam, NULL);
}

static void update_message_OpaqueRect(rdpContext* context, OPAQUE_RECT_ORDER* opaqueRect)
{
	OPAQUE_RECT_ORDER* wParam;

	wParam = (OPAQUE_RECT_ORDER*) malloc(sizeof(OPAQUE_RECT_ORDER));
	CopyMemory(wParam, opaqueRect, sizeof(OPAQUE_RECT_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, OpaqueRect), (void*) wParam, NULL);
}

static void update_message_DrawNineGrid(rdpContext* context, DRAW_NINE_GRID_ORDER* drawNineGrid)
{
	DRAW_NINE_GRID_ORDER* wParam;

	wParam = (DRAW_NINE_GRID_ORDER*) malloc(sizeof(DRAW_NINE_GRID_ORDER));
	CopyMemory(wParam, drawNineGrid, sizeof(DRAW_NINE_GRID_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, DrawNineGrid), (void*) wParam, NULL);
}

static void update_message_MultiDstBlt(rdpContext* context, MULTI_DSTBLT_ORDER* multiDstBlt)
{
	MULTI_DSTBLT_ORDER* wParam;

	wParam = (MULTI_DSTBLT_ORDER*) malloc(sizeof(MULTI_DSTBLT_ORDER));
	CopyMemory(wParam, multiDstBlt, sizeof(MULTI_DSTBLT_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiDstBlt), (void*) wParam, NULL);
}

static void update_message_MultiPatBlt(rdpContext* context, MULTI_PATBLT_ORDER* multiPatBlt)
{
	MULTI_PATBLT_ORDER* wParam;

	wParam = (MULTI_PATBLT_ORDER*) malloc(sizeof(MULTI_PATBLT_ORDER));
	CopyMemory(wParam, multiPatBlt, sizeof(MULTI_PATBLT_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiPatBlt), (void*) wParam, NULL);
}

static void update_message_MultiScrBlt(rdpContext* context, MULTI_SCRBLT_ORDER* multiScrBlt)
{
	MULTI_SCRBLT_ORDER* wParam;

	wParam = (MULTI_SCRBLT_ORDER*) malloc(sizeof(MULTI_SCRBLT_ORDER));
	CopyMemory(wParam, multiScrBlt, sizeof(MULTI_SCRBLT_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiScrBlt), (void*) wParam, NULL);
}

static void update_message_MultiOpaqueRect(rdpContext* context, MULTI_OPAQUE_RECT_ORDER* multiOpaqueRect)
{
	MULTI_OPAQUE_RECT_ORDER* wParam;

	wParam = (MULTI_OPAQUE_RECT_ORDER*) malloc(sizeof(MULTI_OPAQUE_RECT_ORDER));
	CopyMemory(wParam, multiOpaqueRect, sizeof(MULTI_OPAQUE_RECT_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiOpaqueRect), (void*) wParam, NULL);
}

static void update_message_MultiDrawNineGrid(rdpContext* context, MULTI_DRAW_NINE_GRID_ORDER* multiDrawNineGrid)
{
	MULTI_DRAW_NINE_GRID_ORDER* wParam;

	wParam = (MULTI_DRAW_NINE_GRID_ORDER*) malloc(sizeof(MULTI_DRAW_NINE_GRID_ORDER));
	CopyMemory(wParam, multiDrawNineGrid, sizeof(MULTI_DRAW_NINE_GRID_ORDER));

	/* TODO: complete copy */

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiDrawNineGrid), (void*) wParam, NULL);
}

static void update_message_LineTo(rdpContext* context, LINE_TO_ORDER* lineTo)
{
	LINE_TO_ORDER* wParam;

	wParam = (LINE_TO_ORDER*) malloc(sizeof(LINE_TO_ORDER));
	CopyMemory(wParam, lineTo, sizeof(LINE_TO_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, LineTo), (void*) wParam, NULL);
}

static void update_message_Polyline(rdpContext* context, POLYLINE_ORDER* polyline)
{
	POLYLINE_ORDER* wParam;

	wParam = (POLYLINE_ORDER*) malloc(sizeof(POLYLINE_ORDER));
	CopyMemory(wParam, polyline, sizeof(POLYLINE_ORDER));

	wParam->points = (DELTA_POINT*) malloc(sizeof(DELTA_POINT) * wParam->numPoints);
	CopyMemory(wParam->points, polyline->points, sizeof(DELTA_POINT) * wParam->numPoints);

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, Polyline), (void*) wParam, NULL);
}

static void update_message_MemBlt(rdpContext* context, MEMBLT_ORDER* memBlt)
{
	MEMBLT_ORDER* wParam;

	wParam = (MEMBLT_ORDER*) malloc(sizeof(MEMBLT_ORDER));
	CopyMemory(wParam, memBlt, sizeof(MEMBLT_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MemBlt), (void*) wParam, NULL);
}

static void update_message_Mem3Blt(rdpContext* context, MEM3BLT_ORDER* mem3Blt)
{
	MEM3BLT_ORDER* wParam;

	wParam = (MEM3BLT_ORDER*) malloc(sizeof(MEM3BLT_ORDER));
	CopyMemory(wParam, mem3Blt, sizeof(MEM3BLT_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, Mem3Blt), (void*) wParam, NULL);
}

static void update_message_SaveBitmap(rdpContext* context, SAVE_BITMAP_ORDER* saveBitmap)
{
	SAVE_BITMAP_ORDER* wParam;

	wParam = (SAVE_BITMAP_ORDER*) malloc(sizeof(SAVE_BITMAP_ORDER));
	CopyMemory(wParam, saveBitmap, sizeof(SAVE_BITMAP_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, SaveBitmap), (void*) wParam, NULL);
}

static void update_message_GlyphIndex(rdpContext* context, GLYPH_INDEX_ORDER* glyphIndex)
{
	GLYPH_INDEX_ORDER* wParam;

	wParam = (GLYPH_INDEX_ORDER*) malloc(sizeof(GLYPH_INDEX_ORDER));
	CopyMemory(wParam, glyphIndex, sizeof(GLYPH_INDEX_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, GlyphIndex), (void*) wParam, NULL);
}

static void update_message_FastIndex(rdpContext* context, FAST_INDEX_ORDER* fastIndex)
{
	FAST_INDEX_ORDER* wParam;

	wParam = (FAST_INDEX_ORDER*) malloc(sizeof(FAST_INDEX_ORDER));
	CopyMemory(wParam, fastIndex, sizeof(FAST_INDEX_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, FastIndex), (void*) wParam, NULL);
}

static void update_message_FastGlyph(rdpContext* context, FAST_GLYPH_ORDER* fastGlyph)
{
	FAST_GLYPH_ORDER* wParam;

	wParam = (FAST_GLYPH_ORDER*) malloc(sizeof(FAST_GLYPH_ORDER));
	CopyMemory(wParam, fastGlyph, sizeof(FAST_GLYPH_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, FastGlyph), (void*) wParam, NULL);
}

static void update_message_PolygonSC(rdpContext* context, POLYGON_SC_ORDER* polygonSC)
{
	POLYGON_SC_ORDER* wParam;

	wParam = (POLYGON_SC_ORDER*) malloc(sizeof(POLYGON_SC_ORDER));
	CopyMemory(wParam, polygonSC, sizeof(POLYGON_SC_ORDER));

	wParam->points = (DELTA_POINT*) malloc(sizeof(DELTA_POINT) * wParam->numPoints);
	CopyMemory(wParam->points, polygonSC, sizeof(DELTA_POINT) * wParam->numPoints);

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, PolygonSC), (void*) wParam, NULL);
}

static void update_message_PolygonCB(rdpContext* context, POLYGON_CB_ORDER* polygonCB)
{
	POLYGON_CB_ORDER* wParam;

	wParam = (POLYGON_CB_ORDER*) malloc(sizeof(POLYGON_CB_ORDER));
	CopyMemory(wParam, polygonCB, sizeof(POLYGON_CB_ORDER));

	wParam->points = (DELTA_POINT*) malloc(sizeof(DELTA_POINT) * wParam->numPoints);
	CopyMemory(wParam->points, polygonCB, sizeof(DELTA_POINT) * wParam->numPoints);

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, PolygonCB), (void*) wParam, NULL);
}

static void update_message_EllipseSC(rdpContext* context, ELLIPSE_SC_ORDER* ellipseSC)
{
	ELLIPSE_SC_ORDER* wParam;

	wParam = (ELLIPSE_SC_ORDER*) malloc(sizeof(ELLIPSE_SC_ORDER));
	CopyMemory(wParam, ellipseSC, sizeof(ELLIPSE_SC_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, EllipseSC), (void*) wParam, NULL);
}

static void update_message_EllipseCB(rdpContext* context, ELLIPSE_CB_ORDER* ellipseCB)
{
	ELLIPSE_CB_ORDER* wParam;

	wParam = (ELLIPSE_CB_ORDER*) malloc(sizeof(ELLIPSE_CB_ORDER));
	CopyMemory(wParam, ellipseCB, sizeof(ELLIPSE_CB_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, EllipseCB), (void*) wParam, NULL);
}

/* Secondary Update */

static void update_message_CacheBitmap(rdpContext* context, CACHE_BITMAP_ORDER* cacheBitmapOrder)
{
	CACHE_BITMAP_ORDER* wParam;

	wParam = (CACHE_BITMAP_ORDER*) malloc(sizeof(CACHE_BITMAP_ORDER));
	CopyMemory(wParam, cacheBitmapOrder, sizeof(CACHE_BITMAP_ORDER));

	wParam->bitmapDataStream = (BYTE*) malloc(wParam->bitmapLength);
	CopyMemory(wParam->bitmapDataStream, cacheBitmapOrder, wParam->bitmapLength);

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheBitmap), (void*) wParam, NULL);
}

static void update_message_CacheBitmapV2(rdpContext* context, CACHE_BITMAP_V2_ORDER* cacheBitmapV2Order)
{
	CACHE_BITMAP_V2_ORDER* wParam;

	wParam = (CACHE_BITMAP_V2_ORDER*) malloc(sizeof(CACHE_BITMAP_V2_ORDER));
	CopyMemory(wParam, cacheBitmapV2Order, sizeof(CACHE_BITMAP_V2_ORDER));

	wParam->bitmapDataStream = (BYTE*) malloc(wParam->bitmapLength);
	CopyMemory(wParam->bitmapDataStream, cacheBitmapV2Order->bitmapDataStream, wParam->bitmapLength);

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheBitmapV2), (void*) wParam, NULL);
}

static void update_message_CacheBitmapV3(rdpContext* context, CACHE_BITMAP_V3_ORDER* cacheBitmapV3Order)
{
	CACHE_BITMAP_V3_ORDER* wParam;

	wParam = (CACHE_BITMAP_V3_ORDER*) malloc(sizeof(CACHE_BITMAP_V3_ORDER));
	CopyMemory(wParam, cacheBitmapV3Order, sizeof(CACHE_BITMAP_V3_ORDER));

	wParam->bitmapData.data = (BYTE*) malloc(wParam->bitmapData.length);
	CopyMemory(wParam->bitmapData.data, cacheBitmapV3Order->bitmapData.data, wParam->bitmapData.length);

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheBitmapV3), (void*) wParam, NULL);
}

static void update_message_CacheColorTable(rdpContext* context, CACHE_COLOR_TABLE_ORDER* cacheColorTableOrder)
{
	CACHE_COLOR_TABLE_ORDER* wParam;

	wParam = (CACHE_COLOR_TABLE_ORDER*) malloc(sizeof(CACHE_COLOR_TABLE_ORDER));
	CopyMemory(wParam, cacheColorTableOrder, sizeof(CACHE_COLOR_TABLE_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheColorTable), (void*) wParam, NULL);
}

static void update_message_CacheGlyph(rdpContext* context, CACHE_GLYPH_ORDER* cacheGlyphOrder)
{
	CACHE_GLYPH_ORDER* wParam;

	wParam = (CACHE_GLYPH_ORDER*) malloc(sizeof(CACHE_GLYPH_ORDER));
	CopyMemory(wParam, cacheGlyphOrder, sizeof(CACHE_GLYPH_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheGlyph), (void*) wParam, NULL);
}

static void update_message_CacheGlyphV2(rdpContext* context, CACHE_GLYPH_V2_ORDER* cacheGlyphV2Order)
{
	CACHE_GLYPH_V2_ORDER* wParam;

	wParam = (CACHE_GLYPH_V2_ORDER*) malloc(sizeof(CACHE_GLYPH_V2_ORDER));
	CopyMemory(wParam, cacheGlyphV2Order, sizeof(CACHE_GLYPH_V2_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheGlyphV2), (void*) wParam, NULL);
}

static void update_message_CacheBrush(rdpContext* context, CACHE_BRUSH_ORDER* cacheBrushOrder)
{
	CACHE_BRUSH_ORDER* wParam;

	wParam = (CACHE_BRUSH_ORDER*) malloc(sizeof(CACHE_BRUSH_ORDER));
	CopyMemory(wParam, cacheBrushOrder, sizeof(CACHE_BRUSH_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheBrush), (void*) wParam, NULL);
}

/* Alternate Secondary Update */

static void update_message_CreateOffscreenBitmap(rdpContext* context, CREATE_OFFSCREEN_BITMAP_ORDER* createOffscreenBitmap)
{
	CREATE_OFFSCREEN_BITMAP_ORDER* wParam;

	wParam = (CREATE_OFFSCREEN_BITMAP_ORDER*) malloc(sizeof(CREATE_OFFSCREEN_BITMAP_ORDER));
	CopyMemory(wParam, createOffscreenBitmap, sizeof(CREATE_OFFSCREEN_BITMAP_ORDER));

	wParam->deleteList.cIndices = createOffscreenBitmap->deleteList.cIndices;
	wParam->deleteList.sIndices = wParam->deleteList.cIndices;
	wParam->deleteList.indices = (UINT16*) malloc(sizeof(UINT16) * wParam->deleteList.cIndices);
	CopyMemory(wParam->deleteList.indices, createOffscreenBitmap->deleteList.indices, wParam->deleteList.cIndices);

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, CreateOffscreenBitmap), (void*) wParam, NULL);
}

static void update_message_SwitchSurface(rdpContext* context, SWITCH_SURFACE_ORDER* switchSurface)
{
	SWITCH_SURFACE_ORDER* wParam;

	wParam = (SWITCH_SURFACE_ORDER*) malloc(sizeof(SWITCH_SURFACE_ORDER));
	CopyMemory(wParam, switchSurface, sizeof(SWITCH_SURFACE_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, SwitchSurface), (void*) wParam, NULL);
}

static void update_message_CreateNineGridBitmap(rdpContext* context, CREATE_NINE_GRID_BITMAP_ORDER* createNineGridBitmap)
{
	CREATE_NINE_GRID_BITMAP_ORDER* wParam;

	wParam = (CREATE_NINE_GRID_BITMAP_ORDER*) malloc(sizeof(CREATE_NINE_GRID_BITMAP_ORDER));
	CopyMemory(wParam, createNineGridBitmap, sizeof(CREATE_NINE_GRID_BITMAP_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, CreateNineGridBitmap), (void*) wParam, NULL);
}

static void update_message_FrameMarker(rdpContext* context, FRAME_MARKER_ORDER* frameMarker)
{
	FRAME_MARKER_ORDER* wParam;

	wParam = (FRAME_MARKER_ORDER*) malloc(sizeof(FRAME_MARKER_ORDER));
	CopyMemory(wParam, frameMarker, sizeof(FRAME_MARKER_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, FrameMarker), (void*) wParam, NULL);
}

static void update_message_StreamBitmapFirst(rdpContext* context, STREAM_BITMAP_FIRST_ORDER* streamBitmapFirst)
{
	STREAM_BITMAP_FIRST_ORDER* wParam;

	wParam = (STREAM_BITMAP_FIRST_ORDER*) malloc(sizeof(STREAM_BITMAP_FIRST_ORDER));
	CopyMemory(wParam, streamBitmapFirst, sizeof(STREAM_BITMAP_FIRST_ORDER));

	/* TODO: complete copy */

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, StreamBitmapFirst), (void*) wParam, NULL);
}

static void update_message_StreamBitmapNext(rdpContext* context, STREAM_BITMAP_NEXT_ORDER* streamBitmapNext)
{
	STREAM_BITMAP_NEXT_ORDER* wParam;

	wParam = (STREAM_BITMAP_NEXT_ORDER*) malloc(sizeof(STREAM_BITMAP_NEXT_ORDER));
	CopyMemory(wParam, streamBitmapNext, sizeof(STREAM_BITMAP_NEXT_ORDER));

	/* TODO: complete copy */

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, StreamBitmapNext), (void*) wParam, NULL);
}

static void update_message_DrawGdiPlusFirst(rdpContext* context, DRAW_GDIPLUS_FIRST_ORDER* drawGdiPlusFirst)
{
	DRAW_GDIPLUS_FIRST_ORDER* wParam;

	wParam = (DRAW_GDIPLUS_FIRST_ORDER*) malloc(sizeof(DRAW_GDIPLUS_FIRST_ORDER));
	CopyMemory(wParam, drawGdiPlusFirst, sizeof(DRAW_GDIPLUS_FIRST_ORDER));

	/* TODO: complete copy */

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusFirst), (void*) wParam, NULL);
}

static void update_message_DrawGdiPlusNext(rdpContext* context, DRAW_GDIPLUS_NEXT_ORDER* drawGdiPlusNext)
{
	DRAW_GDIPLUS_NEXT_ORDER* wParam;

	wParam = (DRAW_GDIPLUS_NEXT_ORDER*) malloc(sizeof(DRAW_GDIPLUS_NEXT_ORDER));
	CopyMemory(wParam, drawGdiPlusNext, sizeof(DRAW_GDIPLUS_NEXT_ORDER));

	/* TODO: complete copy */

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusNext), (void*) wParam, NULL);
}

static void update_message_DrawGdiPlusEnd(rdpContext* context, DRAW_GDIPLUS_END_ORDER* drawGdiPlusEnd)
{
	DRAW_GDIPLUS_END_ORDER* wParam;

	wParam = (DRAW_GDIPLUS_END_ORDER*) malloc(sizeof(DRAW_GDIPLUS_END_ORDER));
	CopyMemory(wParam, drawGdiPlusEnd, sizeof(DRAW_GDIPLUS_END_ORDER));

	/* TODO: complete copy */

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusEnd), (void*) wParam, NULL);
}

static void update_message_DrawGdiPlusCacheFirst(rdpContext* context, DRAW_GDIPLUS_CACHE_FIRST_ORDER* drawGdiPlusCacheFirst)
{
	DRAW_GDIPLUS_CACHE_FIRST_ORDER* wParam;

	wParam = (DRAW_GDIPLUS_CACHE_FIRST_ORDER*) malloc(sizeof(DRAW_GDIPLUS_CACHE_FIRST_ORDER));
	CopyMemory(wParam, drawGdiPlusCacheFirst, sizeof(DRAW_GDIPLUS_CACHE_FIRST_ORDER));

	/* TODO: complete copy */

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusCacheFirst), (void*) wParam, NULL);
}

static void update_message_DrawGdiPlusCacheNext(rdpContext* context, DRAW_GDIPLUS_CACHE_NEXT_ORDER* drawGdiPlusCacheNext)
{
	DRAW_GDIPLUS_CACHE_NEXT_ORDER* wParam;

	wParam = (DRAW_GDIPLUS_CACHE_NEXT_ORDER*) malloc(sizeof(DRAW_GDIPLUS_CACHE_NEXT_ORDER));
	CopyMemory(wParam, drawGdiPlusCacheNext, sizeof(DRAW_GDIPLUS_CACHE_NEXT_ORDER));

	/* TODO: complete copy */

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusCacheNext), (void*) wParam, NULL);
}

static void update_message_DrawGdiPlusCacheEnd(rdpContext* context, DRAW_GDIPLUS_CACHE_END_ORDER* drawGdiPlusCacheEnd)
{
	DRAW_GDIPLUS_CACHE_END_ORDER* wParam;

	wParam = (DRAW_GDIPLUS_CACHE_END_ORDER*) malloc(sizeof(DRAW_GDIPLUS_CACHE_END_ORDER));
	CopyMemory(wParam, drawGdiPlusCacheEnd, sizeof(DRAW_GDIPLUS_CACHE_END_ORDER));

	/* TODO: complete copy */

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusCacheEnd), (void*) wParam, NULL);
}

/* Window Update */

static void update_message_WindowCreate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* windowState)
{
	WINDOW_ORDER_INFO* wParam;
	WINDOW_STATE_ORDER* lParam;

	wParam = (WINDOW_ORDER_INFO*) malloc(sizeof(WINDOW_ORDER_INFO));
	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));

	lParam = (WINDOW_STATE_ORDER*) malloc(sizeof(WINDOW_STATE_ORDER));
	CopyMemory(lParam, windowState, sizeof(WINDOW_STATE_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowCreate), (void*) wParam, (void*) lParam);
}

static void update_message_WindowUpdate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* windowState)
{
	WINDOW_ORDER_INFO* wParam;
	WINDOW_STATE_ORDER* lParam;

	wParam = (WINDOW_ORDER_INFO*) malloc(sizeof(WINDOW_ORDER_INFO));
	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));

	lParam = (WINDOW_STATE_ORDER*) malloc(sizeof(WINDOW_STATE_ORDER));
	CopyMemory(lParam, windowState, sizeof(WINDOW_STATE_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowUpdate), (void*) wParam, (void*) lParam);
}

static void update_message_WindowIcon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_ICON_ORDER* windowIcon)
{
	WINDOW_ORDER_INFO* wParam;
	WINDOW_ICON_ORDER* lParam;

	wParam = (WINDOW_ORDER_INFO*) malloc(sizeof(WINDOW_ORDER_INFO));
	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));

	lParam = (WINDOW_ICON_ORDER*) malloc(sizeof(WINDOW_ICON_ORDER));
	CopyMemory(lParam, windowIcon, sizeof(WINDOW_ICON_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowIcon), (void*) wParam, (void*) lParam);
}

static void update_message_WindowCachedIcon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_CACHED_ICON_ORDER* windowCachedIcon)
{
	WINDOW_ORDER_INFO* wParam;
	WINDOW_CACHED_ICON_ORDER* lParam;

	wParam = (WINDOW_ORDER_INFO*) malloc(sizeof(WINDOW_ORDER_INFO));
	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));

	lParam = (WINDOW_CACHED_ICON_ORDER*) malloc(sizeof(WINDOW_CACHED_ICON_ORDER));
	CopyMemory(lParam, windowCachedIcon, sizeof(WINDOW_CACHED_ICON_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowCachedIcon), (void*) wParam, (void*) lParam);
}

static void update_message_WindowDelete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	WINDOW_ORDER_INFO* wParam;

	wParam = (WINDOW_ORDER_INFO*) malloc(sizeof(WINDOW_ORDER_INFO));
	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowDelete), (void*) wParam, NULL);
}

static void update_message_NotifyIconCreate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	WINDOW_ORDER_INFO* wParam;
	NOTIFY_ICON_STATE_ORDER* lParam;

	wParam = (WINDOW_ORDER_INFO*) malloc(sizeof(WINDOW_ORDER_INFO));
	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));

	lParam = (NOTIFY_ICON_STATE_ORDER*) malloc(sizeof(NOTIFY_ICON_STATE_ORDER));
	CopyMemory(lParam, notifyIconState, sizeof(NOTIFY_ICON_STATE_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, NotifyIconCreate), (void*) wParam, (void*) lParam);
}

static void update_message_NotifyIconUpdate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	WINDOW_ORDER_INFO* wParam;
	NOTIFY_ICON_STATE_ORDER* lParam;

	wParam = (WINDOW_ORDER_INFO*) malloc(sizeof(WINDOW_ORDER_INFO));
	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));

	lParam = (NOTIFY_ICON_STATE_ORDER*) malloc(sizeof(NOTIFY_ICON_STATE_ORDER));
	CopyMemory(lParam, notifyIconState, sizeof(NOTIFY_ICON_STATE_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, NotifyIconUpdate), (void*) wParam, (void*) lParam);
}

static void update_message_NotifyIconDelete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	WINDOW_ORDER_INFO* wParam;

	wParam = (WINDOW_ORDER_INFO*) malloc(sizeof(WINDOW_ORDER_INFO));
	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, NotifyIconDelete), (void*) wParam, NULL);
}

static void update_message_MonitoredDesktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, MONITORED_DESKTOP_ORDER* monitoredDesktop)
{
	WINDOW_ORDER_INFO* wParam;
	MONITORED_DESKTOP_ORDER* lParam;

	wParam = (WINDOW_ORDER_INFO*) malloc(sizeof(WINDOW_ORDER_INFO));
	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));

	lParam = (MONITORED_DESKTOP_ORDER*) malloc(sizeof(MONITORED_DESKTOP_ORDER));
	CopyMemory(lParam, monitoredDesktop, sizeof(MONITORED_DESKTOP_ORDER));

	lParam->windowIds = NULL;

	if (lParam->numWindowIds)
	{
		lParam->windowIds = (UINT32*) malloc(sizeof(UINT32) * lParam->numWindowIds);
		CopyMemory(lParam->windowIds, monitoredDesktop->windowIds, lParam->numWindowIds);
	}

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, MonitoredDesktop), (void*) wParam, (void*) lParam);
}

static void update_message_NonMonitoredDesktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	WINDOW_ORDER_INFO* wParam;

	wParam = (WINDOW_ORDER_INFO*) malloc(sizeof(WINDOW_ORDER_INFO));
	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, NonMonitoredDesktop), (void*) wParam, NULL);
}

/* Pointer Update */

static void update_message_PointerPosition(rdpContext* context, POINTER_POSITION_UPDATE* pointerPosition)
{
	POINTER_POSITION_UPDATE* wParam;

	wParam = (POINTER_POSITION_UPDATE*) malloc(sizeof(POINTER_POSITION_UPDATE));
	CopyMemory(wParam, pointerPosition, sizeof(POINTER_POSITION_UPDATE));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerPosition), (void*) wParam, NULL);
}

static void update_message_PointerSystem(rdpContext* context, POINTER_SYSTEM_UPDATE* pointerSystem)
{
	POINTER_SYSTEM_UPDATE* wParam;

	wParam = (POINTER_SYSTEM_UPDATE*) malloc(sizeof(POINTER_SYSTEM_UPDATE));
	CopyMemory(wParam, pointerSystem, sizeof(POINTER_SYSTEM_UPDATE));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerSystem), (void*) wParam, NULL);
}

static void update_message_PointerColor(rdpContext* context, POINTER_COLOR_UPDATE* pointerColor)
{
	POINTER_COLOR_UPDATE* wParam;

	wParam = (POINTER_COLOR_UPDATE*) malloc(sizeof(POINTER_COLOR_UPDATE));
	CopyMemory(wParam, pointerColor, sizeof(POINTER_COLOR_UPDATE));

	wParam->andMaskData = wParam->xorMaskData = NULL;

	if (wParam->lengthAndMask)
	{
		wParam->andMaskData = (BYTE*) malloc(wParam->lengthAndMask);
		CopyMemory(wParam->andMaskData, pointerColor->andMaskData, wParam->lengthAndMask);
	}

	if (wParam->lengthXorMask)
	{
		wParam->xorMaskData = (BYTE*) malloc(wParam->lengthXorMask);
		CopyMemory(wParam->xorMaskData, pointerColor->xorMaskData, wParam->lengthXorMask);
	}

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerColor), (void*) wParam, NULL);
}

static void update_message_PointerNew(rdpContext* context, POINTER_NEW_UPDATE* pointerNew)
{
	POINTER_NEW_UPDATE* wParam;

	wParam = (POINTER_NEW_UPDATE*) malloc(sizeof(POINTER_NEW_UPDATE));
	CopyMemory(wParam, pointerNew, sizeof(POINTER_NEW_UPDATE));

	wParam->colorPtrAttr.andMaskData = wParam->colorPtrAttr.xorMaskData = NULL;

	if (wParam->colorPtrAttr.lengthAndMask)
	{
		wParam->colorPtrAttr.andMaskData = (BYTE*) malloc(wParam->colorPtrAttr.lengthAndMask);
		CopyMemory(wParam->colorPtrAttr.andMaskData, pointerNew->colorPtrAttr.andMaskData, wParam->colorPtrAttr.lengthAndMask);
	}

	if (wParam->colorPtrAttr.lengthXorMask)
	{
		wParam->colorPtrAttr.xorMaskData = (BYTE*) malloc(wParam->colorPtrAttr.lengthXorMask);
		CopyMemory(wParam->colorPtrAttr.xorMaskData, pointerNew->colorPtrAttr.xorMaskData, wParam->colorPtrAttr.lengthXorMask);
	}

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerNew), (void*) wParam, NULL);
}

static void update_message_PointerCached(rdpContext* context, POINTER_CACHED_UPDATE* pointerCached)
{
	POINTER_CACHED_UPDATE* wParam;

	wParam = (POINTER_CACHED_UPDATE*) malloc(sizeof(POINTER_CACHED_UPDATE));
	CopyMemory(wParam, pointerCached, sizeof(POINTER_CACHED_UPDATE));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerCached), (void*) wParam, NULL);
}

/* Message Queue */

int update_message_process_update_class(rdpUpdateProxy* proxy, wMessage* msg, int type)
{
	int status = 0;

	switch (type)
	{
		case Update_BeginPaint:
			IFCALL(proxy->BeginPaint, msg->context);
			break;

		case Update_EndPaint:
			IFCALL(proxy->EndPaint, msg->context);
			break;

		case Update_SetBounds:
			IFCALL(proxy->SetBounds, msg->context, (rdpBounds*) msg->wParam);
			if (msg->wParam)
				free(msg->wParam);
			break;

		case Update_Synchronize:
			IFCALL(proxy->Synchronize, msg->context);
			break;

		case Update_DesktopResize:
			IFCALL(proxy->DesktopResize, msg->context);
			break;

		case Update_BitmapUpdate:
			IFCALL(proxy->BitmapUpdate, msg->context, (BITMAP_UPDATE*) msg->wParam);
			{
				int index;
				BITMAP_UPDATE* wParam = (BITMAP_UPDATE*) msg->wParam;

				for (index = 0; index < wParam->number; index++)
				{
#ifdef WITH_STREAM_POOL
					rdpContext* context = (rdpContext*) msg->context;
					StreamPool_Release(context->rdp->transport->ReceivePool, wParam->rectangles[index].bitmapDataStream);
#else
					free(wParam->rectangles[index].bitmapDataStream);
#endif
				}

				free(wParam);
			}
			break;

		case Update_Palette:
			IFCALL(proxy->Palette, msg->context, (PALETTE_UPDATE*) msg->wParam);
			free(msg->wParam);
			break;

		case Update_PlaySound:
			IFCALL(proxy->PlaySound, msg->context, (PLAY_SOUND_UPDATE*) msg->wParam);
			free(msg->wParam);
			break;

		case Update_RefreshRect:
			IFCALL(proxy->RefreshRect, msg->context,
					(BYTE) (size_t) msg->wParam, (RECTANGLE_16*) msg->lParam);
			free(msg->lParam);
			break;

		case Update_SuppressOutput:
			IFCALL(proxy->SuppressOutput, msg->context,
					(BYTE) (size_t) msg->wParam, (RECTANGLE_16*) msg->lParam);
			if (msg->lParam)
				free(msg->lParam);
			break;

		case Update_SurfaceCommand:
			IFCALL(proxy->SurfaceCommand, msg->context, (wStream*) msg->wParam);
			{
				wStream* s = (wStream*) msg->wParam;
				Stream_Free(s, TRUE);
			}
			break;

		case Update_SurfaceBits:
			IFCALL(proxy->SurfaceBits, msg->context, (SURFACE_BITS_COMMAND*) msg->wParam);
			{
#ifdef WITH_STREAM_POOL
				rdpContext* context = (rdpContext*) msg->context;
				SURFACE_BITS_COMMAND* wParam = (SURFACE_BITS_COMMAND*) msg->wParam;
				StreamPool_Release(context->rdp->transport->ReceivePool, wParam->bitmapData);
#else
				SURFACE_BITS_COMMAND* wParam = (SURFACE_BITS_COMMAND*) msg->wParam;
				free(wParam->bitmapData);
				free(wParam);
#endif
			}
			break;

		case Update_SurfaceFrameMarker:
			IFCALL(proxy->SurfaceFrameMarker, msg->context, (SURFACE_FRAME_MARKER*) msg->wParam);
			free(msg->wParam);
			break;

		case Update_SurfaceFrameAcknowledge:
			IFCALL(proxy->SurfaceFrameAcknowledge, msg->context, (UINT32) (size_t) msg->wParam);
			break;

		default:
			status = -1;
			break;
	}

	return status;
}

int update_message_process_primary_update_class(rdpUpdateProxy* proxy, wMessage* msg, int type)
{
	int status = 0;

	switch (type)
	{
		case PrimaryUpdate_DstBlt:
			IFCALL(proxy->DstBlt, msg->context, (DSTBLT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_PatBlt:
			IFCALL(proxy->PatBlt, msg->context, (PATBLT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_ScrBlt:
			IFCALL(proxy->ScrBlt, msg->context, (SCRBLT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_OpaqueRect:
			IFCALL(proxy->OpaqueRect, msg->context, (OPAQUE_RECT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_DrawNineGrid:
			IFCALL(proxy->DrawNineGrid, msg->context, (DRAW_NINE_GRID_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_MultiDstBlt:
			IFCALL(proxy->MultiDstBlt, msg->context, (MULTI_DSTBLT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_MultiPatBlt:
			IFCALL(proxy->MultiPatBlt, msg->context, (MULTI_PATBLT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_MultiScrBlt:
			IFCALL(proxy->MultiScrBlt, msg->context, (MULTI_SCRBLT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_MultiOpaqueRect:
			IFCALL(proxy->MultiOpaqueRect, msg->context, (MULTI_OPAQUE_RECT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_MultiDrawNineGrid:
			IFCALL(proxy->MultiDrawNineGrid, msg->context, (MULTI_DRAW_NINE_GRID_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_LineTo:
			IFCALL(proxy->LineTo, msg->context, (LINE_TO_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_Polyline:
			IFCALL(proxy->Polyline, msg->context, (POLYLINE_ORDER*) msg->wParam);
			{
				POLYLINE_ORDER* wParam = (POLYLINE_ORDER*) msg->wParam;

				free(wParam->points);
				free(wParam);
			}
			break;

		case PrimaryUpdate_MemBlt:
			IFCALL(proxy->MemBlt, msg->context, (MEMBLT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_Mem3Blt:
			IFCALL(proxy->Mem3Blt, msg->context, (MEM3BLT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_SaveBitmap:
			IFCALL(proxy->SaveBitmap, msg->context, (SAVE_BITMAP_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_GlyphIndex:
			IFCALL(proxy->GlyphIndex, msg->context, (GLYPH_INDEX_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_FastIndex:
			IFCALL(proxy->FastIndex, msg->context, (FAST_INDEX_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_FastGlyph:
			IFCALL(proxy->FastGlyph, msg->context, (FAST_GLYPH_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_PolygonSC:
			IFCALL(proxy->PolygonSC, msg->context, (POLYGON_SC_ORDER*) msg->wParam);
			{
				POLYGON_SC_ORDER* wParam = (POLYGON_SC_ORDER*) msg->wParam;

				free(wParam->points);
				free(wParam);
			}
			break;

		case PrimaryUpdate_PolygonCB:
			IFCALL(proxy->PolygonCB, msg->context, (POLYGON_CB_ORDER*) msg->wParam);
			{
				POLYGON_CB_ORDER* wParam = (POLYGON_CB_ORDER*) msg->wParam;

				free(wParam->points);
				free(wParam);
			}
			break;

		case PrimaryUpdate_EllipseSC:
			IFCALL(proxy->EllipseSC, msg->context, (ELLIPSE_SC_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_EllipseCB:
			IFCALL(proxy->EllipseCB, msg->context, (ELLIPSE_CB_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		default:
			status = -1;
			break;
	}

	return status;
}

int update_message_process_secondary_update_class(rdpUpdateProxy* proxy, wMessage* msg, int type)
{
	int status = 0;

	switch (type)
	{
		case SecondaryUpdate_CacheBitmap:
			IFCALL(proxy->CacheBitmap, msg->context, (CACHE_BITMAP_ORDER*) msg->wParam);
			{
				CACHE_BITMAP_ORDER* wParam = (CACHE_BITMAP_ORDER*) msg->wParam;

				free(wParam->bitmapDataStream);
				free(wParam);
			}
			break;

		case SecondaryUpdate_CacheBitmapV2:
			IFCALL(proxy->CacheBitmapV2, msg->context, (CACHE_BITMAP_V2_ORDER*) msg->wParam);
			{
				CACHE_BITMAP_V2_ORDER* wParam = (CACHE_BITMAP_V2_ORDER*) msg->wParam;

				free(wParam->bitmapDataStream);
				free(wParam);
			}
			break;

		case SecondaryUpdate_CacheBitmapV3:
			IFCALL(proxy->CacheBitmapV3, msg->context, (CACHE_BITMAP_V3_ORDER*) msg->wParam);
			{
				CACHE_BITMAP_V3_ORDER* wParam = (CACHE_BITMAP_V3_ORDER*) msg->wParam;

				free(wParam->bitmapData.data);
				free(wParam);
			}
			break;

		case SecondaryUpdate_CacheColorTable:
			IFCALL(proxy->CacheColorTable, msg->context, (CACHE_COLOR_TABLE_ORDER*) msg->wParam);
			{
				CACHE_COLOR_TABLE_ORDER* wParam = (CACHE_COLOR_TABLE_ORDER*) msg->wParam;
				free(wParam);
			}
			break;

		case SecondaryUpdate_CacheGlyph:
			IFCALL(proxy->CacheGlyph, msg->context, (CACHE_GLYPH_ORDER*) msg->wParam);
			{
				CACHE_GLYPH_ORDER* wParam = (CACHE_GLYPH_ORDER*) msg->wParam;
				free(wParam);
			}
			break;

		case SecondaryUpdate_CacheGlyphV2:
			IFCALL(proxy->CacheGlyphV2, msg->context, (CACHE_GLYPH_V2_ORDER*) msg->wParam);
			{
				CACHE_GLYPH_V2_ORDER* wParam = (CACHE_GLYPH_V2_ORDER*) msg->wParam;
				free(wParam);
			}
			break;

		case SecondaryUpdate_CacheBrush:
			IFCALL(proxy->CacheBrush, msg->context, (CACHE_BRUSH_ORDER*) msg->wParam);
			{
				CACHE_BRUSH_ORDER* wParam = (CACHE_BRUSH_ORDER*) msg->wParam;
				free(wParam);
			}
			break;

		default:
			status = -1;
			break;
	}

	return status;
}

int update_message_process_altsec_update_class(rdpUpdateProxy* proxy, wMessage* msg, int type)
{
	int status = 0;

	switch (type)
	{
		case AltSecUpdate_CreateOffscreenBitmap:
			IFCALL(proxy->CreateOffscreenBitmap, msg->context, (CREATE_OFFSCREEN_BITMAP_ORDER*) msg->wParam);
			{
				CREATE_OFFSCREEN_BITMAP_ORDER* wParam = (CREATE_OFFSCREEN_BITMAP_ORDER*) msg->wParam;

				free(wParam->deleteList.indices);
				free(wParam);
			}
			break;

		case AltSecUpdate_SwitchSurface:
			IFCALL(proxy->SwitchSurface, msg->context, (SWITCH_SURFACE_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case AltSecUpdate_CreateNineGridBitmap:
			IFCALL(proxy->CreateNineGridBitmap, msg->context, (CREATE_NINE_GRID_BITMAP_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case AltSecUpdate_FrameMarker:
			IFCALL(proxy->FrameMarker, msg->context, (FRAME_MARKER_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case AltSecUpdate_StreamBitmapFirst:
			IFCALL(proxy->StreamBitmapFirst, msg->context, (STREAM_BITMAP_FIRST_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case AltSecUpdate_StreamBitmapNext:
			IFCALL(proxy->StreamBitmapNext, msg->context, (STREAM_BITMAP_NEXT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusFirst:
			IFCALL(proxy->DrawGdiPlusFirst, msg->context, (DRAW_GDIPLUS_FIRST_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusNext:
			IFCALL(proxy->DrawGdiPlusNext, msg->context, (DRAW_GDIPLUS_NEXT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusEnd:
			IFCALL(proxy->DrawGdiPlusEnd, msg->context, (DRAW_GDIPLUS_END_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusCacheFirst:
			IFCALL(proxy->DrawGdiPlusCacheFirst, msg->context, (DRAW_GDIPLUS_CACHE_FIRST_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusCacheNext:
			IFCALL(proxy->DrawGdiPlusCacheNext, msg->context, (DRAW_GDIPLUS_CACHE_NEXT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusCacheEnd:
			IFCALL(proxy->DrawGdiPlusCacheEnd, msg->context, (DRAW_GDIPLUS_CACHE_END_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		default:
			status = -1;
			break;
	}

	return status;
}

int update_message_process_window_update_class(rdpUpdateProxy* proxy, wMessage* msg, int type)
{
	int status = 0;

	switch (type)
	{
		case WindowUpdate_WindowCreate:
			IFCALL(proxy->WindowCreate, msg->context, (WINDOW_ORDER_INFO*) msg->wParam,
					(WINDOW_STATE_ORDER*) msg->lParam);
			free(msg->wParam);
			free(msg->lParam);
			break;

		case WindowUpdate_WindowUpdate:
			IFCALL(proxy->WindowCreate, msg->context, (WINDOW_ORDER_INFO*) msg->wParam,
					(WINDOW_STATE_ORDER*) msg->lParam);
			free(msg->wParam);
			free(msg->lParam);
			break;

		case WindowUpdate_WindowIcon:
			IFCALL(proxy->WindowIcon, msg->context, (WINDOW_ORDER_INFO*) msg->wParam,
					(WINDOW_ICON_ORDER*) msg->lParam);
			free(msg->wParam);
			free(msg->lParam);
			break;

		case WindowUpdate_WindowCachedIcon:
			IFCALL(proxy->WindowCachedIcon, msg->context, (WINDOW_ORDER_INFO*) msg->wParam,
					(WINDOW_CACHED_ICON_ORDER*) msg->lParam);
			free(msg->wParam);
			free(msg->lParam);
			break;

		case WindowUpdate_WindowDelete:
			IFCALL(proxy->WindowDelete, msg->context, (WINDOW_ORDER_INFO*) msg->wParam);
			free(msg->wParam);
			break;

		case WindowUpdate_NotifyIconCreate:
			IFCALL(proxy->NotifyIconCreate, msg->context, (WINDOW_ORDER_INFO*) msg->wParam,
					(NOTIFY_ICON_STATE_ORDER*) msg->lParam);
			free(msg->wParam);
			free(msg->lParam);
			break;

		case WindowUpdate_NotifyIconUpdate:
			IFCALL(proxy->NotifyIconUpdate, msg->context, (WINDOW_ORDER_INFO*) msg->wParam,
					(NOTIFY_ICON_STATE_ORDER*) msg->lParam);
			free(msg->wParam);
			free(msg->lParam);
			break;

		case WindowUpdate_NotifyIconDelete:
			IFCALL(proxy->NotifyIconDelete, msg->context, (WINDOW_ORDER_INFO*) msg->wParam);
			free(msg->wParam);
			break;

		case WindowUpdate_MonitoredDesktop:
			IFCALL(proxy->MonitoredDesktop, msg->context, (WINDOW_ORDER_INFO*) msg->wParam,
					(MONITORED_DESKTOP_ORDER*) msg->lParam);
			{
				MONITORED_DESKTOP_ORDER* lParam = (MONITORED_DESKTOP_ORDER*) msg->lParam;

				free(msg->wParam);

				free(lParam->windowIds);
				free(lParam);
			}
			break;

		case WindowUpdate_NonMonitoredDesktop:
			IFCALL(proxy->NonMonitoredDesktop, msg->context, (WINDOW_ORDER_INFO*) msg->wParam);
			free(msg->wParam);
			break;

		default:
			status = -1;
			break;
	}

	return status;
}

int update_message_process_pointer_update_class(rdpUpdateProxy* proxy, wMessage* msg, int type)
{
	int status = 0;

	switch (type)
	{
		case PointerUpdate_PointerPosition:
			IFCALL(proxy->PointerPosition, msg->context, (POINTER_POSITION_UPDATE*) msg->wParam);
			free(msg->wParam);
			break;

		case PointerUpdate_PointerSystem:
			IFCALL(proxy->PointerSystem, msg->context, (POINTER_SYSTEM_UPDATE*) msg->wParam);
			free(msg->wParam);
			break;

		case PointerUpdate_PointerColor:
			IFCALL(proxy->PointerColor, msg->context, (POINTER_COLOR_UPDATE*) msg->wParam);
			{
				POINTER_COLOR_UPDATE* wParam = (POINTER_COLOR_UPDATE*) msg->wParam;

				free(wParam->andMaskData);
				free(wParam->xorMaskData);
				free(wParam);
			}
			break;

		case PointerUpdate_PointerNew:
			IFCALL(proxy->PointerNew, msg->context, (POINTER_NEW_UPDATE*) msg->wParam);
			{
				POINTER_NEW_UPDATE* wParam = (POINTER_NEW_UPDATE*) msg->wParam;

				free(wParam->colorPtrAttr.andMaskData);
				free(wParam->colorPtrAttr.xorMaskData);
				free(wParam);
			}
			break;

		case PointerUpdate_PointerCached:
			IFCALL(proxy->PointerCached, msg->context, (POINTER_CACHED_UPDATE*) msg->wParam);
			free(msg->wParam);
			break;

		default:
			status = -1;
			break;
	}

	return status;
}

int update_message_process_class(rdpUpdateProxy* proxy, wMessage* msg, int msgClass, int msgType)
{
	int status = 0;

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
			status = -1;
			break;
	}

	if (status < 0)
		fprintf(stderr, "Unknown message: class: %d type: %d\n", msgClass, msgType);

	return status;
}

int update_message_queue_process_message(rdpUpdate* update, wMessage* message)
{
	int status;
	int msgClass;
	int msgType;

	if (message->id == WMQ_QUIT)
		return 0;

	msgClass = GetMessageClass(message->id);
	msgType = GetMessageType(message->id);

	status = update_message_process_class(update->proxy, message, msgClass, msgType);

	if (status < 0)
		status = -1;

	return 1;
}

int update_message_queue_process_pending_messages(rdpUpdate* update)
{
	int status;
	wMessage message;
	wMessageQueue* queue;

	status = 1;
	queue = update->queue;

	while (MessageQueue_Peek(queue, &message, TRUE))
	{
		status = update_message_queue_process_message(update, &message);

		if (!status)
			break;
	}

	return status;
}

void update_message_register_interface(rdpUpdateProxy* message, rdpUpdate* update)
{
	rdpPrimaryUpdate* primary;
	rdpSecondaryUpdate* secondary;
	rdpAltSecUpdate* altsec;
	rdpWindowUpdate* window;
	rdpPointerUpdate* pointer;

	primary = update->primary;
	secondary = update->secondary;
	altsec = update->altsec;
	window = update->window;
	pointer = update->pointer;

	/* Update */

	message->BeginPaint = update->BeginPaint;
	message->EndPaint = update->EndPaint;
	message->SetBounds = update->SetBounds;
	message->Synchronize = update->Synchronize;
	message->DesktopResize = update->DesktopResize;
	message->BitmapUpdate = update->BitmapUpdate;
	message->Palette = update->Palette;
	message->PlaySound = update->PlaySound;
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
	message->PointerNew = pointer->PointerNew;
	message->PointerCached = pointer->PointerCached;

	pointer->PointerPosition = update_message_PointerPosition;
	pointer->PointerSystem = update_message_PointerSystem;
	pointer->PointerColor = update_message_PointerColor;
	pointer->PointerNew = update_message_PointerNew;
	pointer->PointerCached = update_message_PointerCached;
}

rdpUpdateProxy* update_message_proxy_new(rdpUpdate* update)
{
	rdpUpdateProxy* message;

	message = (rdpUpdateProxy*) malloc(sizeof(rdpUpdateProxy));

	if (message)
	{
		ZeroMemory(message, sizeof(rdpUpdateProxy));

		message->update = update;
		update->queue = MessageQueue_New();
		update_message_register_interface(message, update);
	}

	return message;
}

void update_message_proxy_free(rdpUpdateProxy* message)
{
	if (message)
	{
		MessageQueue_Free(message->update->queue);
		free(message);
	}
}

/* Input */

static void input_message_SynchronizeEvent(rdpInput* input, UINT32 flags)
{
	MessageQueue_Post(input->queue, (void*) input,
			MakeMessageId(Input, SynchronizeEvent), (void*) (size_t) flags, NULL);
}

static void input_message_KeyboardEvent(rdpInput* input, UINT16 flags, UINT16 code)
{
	MessageQueue_Post(input->queue, (void*) input,
			MakeMessageId(Input, KeyboardEvent), (void*) (size_t) flags, (void*) (size_t) code);
}

static void input_message_UnicodeKeyboardEvent(rdpInput* input, UINT16 flags, UINT16 code)
{
	MessageQueue_Post(input->queue, (void*) input,
			MakeMessageId(Input, UnicodeKeyboardEvent), (void*) (size_t) flags, (void*) (size_t) code);
}

static void input_message_MouseEvent(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	UINT32 pos = (x << 16) | y;

	MessageQueue_Post(input->queue, (void*) input,
			MakeMessageId(Input, MouseEvent), (void*) (size_t) flags, (void*) (size_t) pos);
}

static void input_message_ExtendedMouseEvent(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	UINT32 pos = (x << 16) | y;

	MessageQueue_Post(input->queue, (void*) input,
			MakeMessageId(Input, ExtendedMouseEvent), (void*) (size_t) flags, (void*) (size_t) pos);
}

/* Event Queue */

int input_message_process_input_class(rdpInputProxy* proxy, wMessage* msg, int type)
{
	int status = 0;

	switch (type)
	{
		case Input_SynchronizeEvent:
			IFCALL(proxy->SynchronizeEvent, msg->context, (UINT32) (size_t) msg->wParam);
			break;

		case Input_KeyboardEvent:
			IFCALL(proxy->KeyboardEvent, msg->context, (UINT16) (size_t) msg->wParam, (UINT16) (size_t) msg->lParam);
			break;

		case Input_UnicodeKeyboardEvent:
			IFCALL(proxy->UnicodeKeyboardEvent, msg->context, (UINT16) (size_t) msg->wParam, (UINT16) (size_t) msg->lParam);
			break;

		case Input_MouseEvent:
			{
				UINT32 pos;
				UINT16 x, y;

				pos = (UINT32) (size_t) msg->lParam;
				x = ((pos & 0xFFFF0000) >> 16);
				y = (pos & 0x0000FFFF);

				IFCALL(proxy->MouseEvent, msg->context, (UINT16) (size_t) msg->wParam, x, y);
			}
			break;

		case Input_ExtendedMouseEvent:
			{
				UINT32 pos;
				UINT16 x, y;

				pos = (UINT32) (size_t) msg->lParam;
				x = ((pos & 0xFFFF0000) >> 16);
				y = (pos & 0x0000FFFF);

				IFCALL(proxy->ExtendedMouseEvent, msg->context, (UINT16) (size_t) msg->wParam, x, y);
			}
			break;

		default:
			status = -1;
			break;
	}

	return status;
}

int input_message_process_class(rdpInputProxy* proxy, wMessage* msg, int msgClass, int msgType)
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
		fprintf(stderr, "Unknown event: class: %d type: %d\n", msgClass, msgType);

	return status;
}

int input_message_queue_process_message(rdpInput* input, wMessage* message)
{
	int status;
	int msgClass;
	int msgType;

	if (message->id == WMQ_QUIT)
		return 0;

	msgClass = GetMessageClass(message->id);
	msgType = GetMessageType(message->id);

	status = input_message_process_class(input->proxy, message, msgClass, msgType);

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

	count = 0;
	status = 1;
	queue = input->queue;

	while (MessageQueue_Peek(queue, &message, TRUE))
	{
		status = input_message_queue_process_message(input, &message);

		if (!status)
			break;

		count++;
	}

	return status;
}

void input_message_proxy_register(rdpInputProxy* proxy, rdpInput* input)
{
	/* Input */

	proxy->SynchronizeEvent = input->SynchronizeEvent;
	proxy->KeyboardEvent = input->KeyboardEvent;
	proxy->UnicodeKeyboardEvent = input->UnicodeKeyboardEvent;
	proxy->MouseEvent = input->MouseEvent;
	proxy->ExtendedMouseEvent = input->ExtendedMouseEvent;

	input->SynchronizeEvent = input_message_SynchronizeEvent;
	input->KeyboardEvent = input_message_KeyboardEvent;
	input->UnicodeKeyboardEvent = input_message_UnicodeKeyboardEvent;
	input->MouseEvent = input_message_MouseEvent;
	input->ExtendedMouseEvent = input_message_ExtendedMouseEvent;
}

rdpInputProxy* input_message_proxy_new(rdpInput* input)
{
	rdpInputProxy* proxy;

	proxy = (rdpInputProxy*) malloc(sizeof(rdpInputProxy));

	if (proxy)
	{
		ZeroMemory(proxy, sizeof(rdpInputProxy));

		proxy->input = input;
		input->queue = MessageQueue_New();
		input_message_proxy_register(proxy, input);
	}

	return proxy;
}

void input_message_proxy_free(rdpInputProxy* proxy)
{
	if (proxy)
	{
		MessageQueue_Free(proxy->input->queue);
		free(proxy);
	}
}
