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

#include "message.h"

#include <winpr/crt.h>
#include <winpr/collections.h>

/* Update */

static void message_BeginPaint(rdpContext* context)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, BeginPaint), NULL, NULL);
}

static void message_EndPaint(rdpContext* context)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, EndPaint), NULL, NULL);
}

static void message_SetBounds(rdpContext* context, rdpBounds* bounds)
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

static void message_Synchronize(rdpContext* context)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, Synchronize), NULL, NULL);
}

static void message_DesktopResize(rdpContext* context)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, DesktopResize), NULL, NULL);
}

static void message_BitmapUpdate(rdpContext* context, BITMAP_UPDATE* bitmap)
{
	int index;
	BITMAP_UPDATE* wParam;

	wParam = (BITMAP_UPDATE*) malloc(sizeof(BITMAP_UPDATE));

	wParam->number = bitmap->number;
	wParam->count = wParam->number;

	wParam->rectangles = (BITMAP_DATA*) malloc(sizeof(BITMAP_DATA) * wParam->number);
	CopyMemory(wParam->rectangles, bitmap->rectangles, sizeof(BITMAP_DATA) * wParam->number);

	/* TODO: increment reference count to original stream instead of copying */

	for (index = 0; index < wParam->number; index++)
	{
		wParam->rectangles[index].bitmapDataStream = (BYTE*) malloc(wParam->rectangles[index].bitmapLength);
		CopyMemory(wParam->rectangles[index].bitmapDataStream, bitmap->rectangles[index].bitmapDataStream,
				wParam->rectangles[index].bitmapLength);
	}

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, BitmapUpdate), (void*) wParam, NULL);
}

static void message_Palette(rdpContext* context, PALETTE_UPDATE* palette)
{
	PALETTE_UPDATE* wParam;

	wParam = (PALETTE_UPDATE*) malloc(sizeof(PALETTE_UPDATE));
	CopyMemory(wParam, palette, sizeof(PALETTE_UPDATE));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, Palette), (void*) wParam, NULL);
}

static void message_PlaySound(rdpContext* context, PLAY_SOUND_UPDATE* playSound)
{
	PLAY_SOUND_UPDATE* wParam;

	wParam = (PLAY_SOUND_UPDATE*) malloc(sizeof(PLAY_SOUND_UPDATE));
	CopyMemory(wParam, playSound, sizeof(PLAY_SOUND_UPDATE));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, PlaySound), (void*) wParam, NULL);
}

static void message_RefreshRect(rdpContext* context, BYTE count, RECTANGLE_16* areas)
{
	RECTANGLE_16* lParam;

	lParam = (RECTANGLE_16*) malloc(sizeof(RECTANGLE_16) * count);
	CopyMemory(lParam, areas, sizeof(RECTANGLE_16) * count);

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, RefreshRect), (void*) (size_t) count, (void*) lParam);
}

static void message_SuppressOutput(rdpContext* context, BYTE allow, RECTANGLE_16* area)
{
	RECTANGLE_16* lParam;

	lParam = (RECTANGLE_16*) malloc(sizeof(RECTANGLE_16));
	CopyMemory(lParam, area, sizeof(RECTANGLE_16));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SuppressOutput), (void*) (size_t) allow, (void*) lParam);
}

static void message_SurfaceCommand(rdpContext* context, STREAM* s)
{
	STREAM* wParam;

	wParam = (STREAM*) malloc(sizeof(STREAM));

	wParam->size = s->size;
	wParam->data = (BYTE*) malloc(wParam->size);
	wParam->p = wParam->data;

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SurfaceCommand), (void*) wParam, NULL);
}

static void message_SurfaceBits(rdpContext* context, SURFACE_BITS_COMMAND* surfaceBitsCommand)
{
	SURFACE_BITS_COMMAND* wParam;

	wParam = (SURFACE_BITS_COMMAND*) malloc(sizeof(SURFACE_BITS_COMMAND));
	CopyMemory(wParam, surfaceBitsCommand, sizeof(SURFACE_BITS_COMMAND));

	wParam->bitmapData = (BYTE*) malloc(wParam->bitmapDataLength);
	CopyMemory(wParam->bitmapData, surfaceBitsCommand->bitmapData, wParam->bitmapDataLength);

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SurfaceBits), (void*) wParam, NULL);
}

static void message_SurfaceFrameMarker(rdpContext* context, SURFACE_FRAME_MARKER* surfaceFrameMarker)
{
	SURFACE_FRAME_MARKER* wParam;

	wParam = (SURFACE_FRAME_MARKER*) malloc(sizeof(SURFACE_FRAME_MARKER));
	CopyMemory(wParam, surfaceFrameMarker, sizeof(SURFACE_FRAME_MARKER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SurfaceFrameMarker), (void*) wParam, NULL);
}

static void message_SurfaceFrameAcknowledge(rdpContext* context, UINT32 frameId)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SurfaceFrameAcknowledge), (void*) (size_t) frameId, NULL);
}

/* Primary Update */

static void message_DstBlt(rdpContext* context, DSTBLT_ORDER* dstBlt)
{
	DSTBLT_ORDER* wParam;

	wParam = (DSTBLT_ORDER*) malloc(sizeof(DSTBLT_ORDER));
	CopyMemory(wParam, dstBlt, sizeof(DSTBLT_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, DstBlt), (void*) wParam, NULL);
}

static void message_PatBlt(rdpContext* context, PATBLT_ORDER* patBlt)
{
	PATBLT_ORDER* wParam;

	wParam = (PATBLT_ORDER*) malloc(sizeof(PATBLT_ORDER));
	CopyMemory(wParam, patBlt, sizeof(PATBLT_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, PatBlt), (void*) wParam, NULL);
}

static void message_ScrBlt(rdpContext* context, SCRBLT_ORDER* scrBlt)
{
	SCRBLT_ORDER* wParam;

	wParam = (SCRBLT_ORDER*) malloc(sizeof(SCRBLT_ORDER));
	CopyMemory(wParam, scrBlt, sizeof(SCRBLT_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, ScrBlt), (void*) wParam, NULL);
}

static void message_OpaqueRect(rdpContext* context, OPAQUE_RECT_ORDER* opaqueRect)
{
	OPAQUE_RECT_ORDER* wParam;

	wParam = (OPAQUE_RECT_ORDER*) malloc(sizeof(OPAQUE_RECT_ORDER));
	CopyMemory(wParam, opaqueRect, sizeof(OPAQUE_RECT_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, OpaqueRect), (void*) wParam, NULL);
}

static void message_DrawNineGrid(rdpContext* context, DRAW_NINE_GRID_ORDER* drawNineGrid)
{
	DRAW_NINE_GRID_ORDER* wParam;

	wParam = (DRAW_NINE_GRID_ORDER*) malloc(sizeof(DRAW_NINE_GRID_ORDER));
	CopyMemory(wParam, drawNineGrid, sizeof(DRAW_NINE_GRID_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, DrawNineGrid), (void*) wParam, NULL);
}

static void message_MultiDstBlt(rdpContext* context, MULTI_DSTBLT_ORDER* multiDstBlt)
{
	MULTI_DSTBLT_ORDER* wParam;

	wParam = (MULTI_DSTBLT_ORDER*) malloc(sizeof(MULTI_DSTBLT_ORDER));
	CopyMemory(wParam, multiDstBlt, sizeof(MULTI_DSTBLT_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiDstBlt), (void*) wParam, NULL);
}

static void message_MultiPatBlt(rdpContext* context, MULTI_PATBLT_ORDER* multiPatBlt)
{
	MULTI_PATBLT_ORDER* wParam;

	wParam = (MULTI_PATBLT_ORDER*) malloc(sizeof(MULTI_PATBLT_ORDER));
	CopyMemory(wParam, multiPatBlt, sizeof(MULTI_PATBLT_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiPatBlt), (void*) wParam, NULL);
}

static void message_MultiScrBlt(rdpContext* context, MULTI_SCRBLT_ORDER* multiScrBlt)
{
	MULTI_SCRBLT_ORDER* wParam;

	wParam = (MULTI_SCRBLT_ORDER*) malloc(sizeof(MULTI_SCRBLT_ORDER));
	CopyMemory(wParam, multiScrBlt, sizeof(MULTI_SCRBLT_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiScrBlt), (void*) wParam, NULL);
}

static void message_MultiOpaqueRect(rdpContext* context, MULTI_OPAQUE_RECT_ORDER* multiOpaqueRect)
{
	MULTI_OPAQUE_RECT_ORDER* wParam;

	wParam = (MULTI_OPAQUE_RECT_ORDER*) malloc(sizeof(MULTI_OPAQUE_RECT_ORDER));
	CopyMemory(wParam, multiOpaqueRect, sizeof(MULTI_OPAQUE_RECT_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiOpaqueRect), (void*) wParam, NULL);
}

static void message_MultiDrawNineGrid(rdpContext* context, MULTI_DRAW_NINE_GRID_ORDER* multiDrawNineGrid)
{
	MULTI_DRAW_NINE_GRID_ORDER* wParam;

	wParam = (MULTI_DRAW_NINE_GRID_ORDER*) malloc(sizeof(MULTI_DRAW_NINE_GRID_ORDER));
	CopyMemory(wParam, multiDrawNineGrid, sizeof(MULTI_DRAW_NINE_GRID_ORDER));

	/* TODO: complete copy */

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiDrawNineGrid), (void*) wParam, NULL);
}

static void message_LineTo(rdpContext* context, LINE_TO_ORDER* lineTo)
{
	LINE_TO_ORDER* wParam;

	wParam = (LINE_TO_ORDER*) malloc(sizeof(LINE_TO_ORDER));
	CopyMemory(wParam, lineTo, sizeof(LINE_TO_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, LineTo), (void*) wParam, NULL);
}

static void message_Polyline(rdpContext* context, POLYLINE_ORDER* polyline)
{
	POLYLINE_ORDER* wParam;

	wParam = (POLYLINE_ORDER*) malloc(sizeof(POLYLINE_ORDER));
	CopyMemory(wParam, polyline, sizeof(POLYLINE_ORDER));

	wParam->points = (DELTA_POINT*) malloc(sizeof(DELTA_POINT) * wParam->numPoints);
	CopyMemory(wParam->points, polyline->points, sizeof(DELTA_POINT) * wParam->numPoints);

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, Polyline), (void*) wParam, NULL);
}

static void message_MemBlt(rdpContext* context, MEMBLT_ORDER* memBlt)
{
	MEMBLT_ORDER* wParam;

	wParam = (MEMBLT_ORDER*) malloc(sizeof(MEMBLT_ORDER));
	CopyMemory(wParam, memBlt, sizeof(MEMBLT_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MemBlt), (void*) wParam, NULL);
}

static void message_Mem3Blt(rdpContext* context, MEM3BLT_ORDER* mem3Blt)
{
	MEM3BLT_ORDER* wParam;

	wParam = (MEM3BLT_ORDER*) malloc(sizeof(MEM3BLT_ORDER));
	CopyMemory(wParam, mem3Blt, sizeof(MEM3BLT_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, Mem3Blt), (void*) wParam, NULL);
}

static void message_SaveBitmap(rdpContext* context, SAVE_BITMAP_ORDER* saveBitmap)
{
	SAVE_BITMAP_ORDER* wParam;

	wParam = (SAVE_BITMAP_ORDER*) malloc(sizeof(SAVE_BITMAP_ORDER));
	CopyMemory(wParam, saveBitmap, sizeof(SAVE_BITMAP_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, SaveBitmap), (void*) wParam, NULL);
}

static void message_GlyphIndex(rdpContext* context, GLYPH_INDEX_ORDER* glyphIndex)
{
	GLYPH_INDEX_ORDER* wParam;

	wParam = (GLYPH_INDEX_ORDER*) malloc(sizeof(GLYPH_INDEX_ORDER));
	CopyMemory(wParam, glyphIndex, sizeof(GLYPH_INDEX_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, GlyphIndex), (void*) wParam, NULL);
}

static void message_FastIndex(rdpContext* context, FAST_INDEX_ORDER* fastIndex)
{
	FAST_INDEX_ORDER* wParam;

	wParam = (FAST_INDEX_ORDER*) malloc(sizeof(FAST_INDEX_ORDER));
	CopyMemory(wParam, fastIndex, sizeof(FAST_INDEX_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, FastIndex), (void*) wParam, NULL);
}

static void message_FastGlyph(rdpContext* context, FAST_GLYPH_ORDER* fastGlyph)
{
	FAST_GLYPH_ORDER* wParam;

	wParam = (FAST_GLYPH_ORDER*) malloc(sizeof(FAST_GLYPH_ORDER));
	CopyMemory(wParam, fastGlyph, sizeof(FAST_GLYPH_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, FastGlyph), (void*) wParam, NULL);
}

static void message_PolygonSC(rdpContext* context, POLYGON_SC_ORDER* polygonSC)
{
	POLYGON_SC_ORDER* wParam;

	wParam = (POLYGON_SC_ORDER*) malloc(sizeof(POLYGON_SC_ORDER));
	CopyMemory(wParam, polygonSC, sizeof(POLYGON_SC_ORDER));

	wParam->points = (DELTA_POINT*) malloc(sizeof(DELTA_POINT) * wParam->numPoints);
	CopyMemory(wParam->points, polygonSC, sizeof(DELTA_POINT) * wParam->numPoints);

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, PolygonSC), (void*) wParam, NULL);
}

static void message_PolygonCB(rdpContext* context, POLYGON_CB_ORDER* polygonCB)
{
	POLYGON_CB_ORDER* wParam;

	wParam = (POLYGON_CB_ORDER*) malloc(sizeof(POLYGON_CB_ORDER));
	CopyMemory(wParam, polygonCB, sizeof(POLYGON_CB_ORDER));

	wParam->points = (DELTA_POINT*) malloc(sizeof(DELTA_POINT) * wParam->numPoints);
	CopyMemory(wParam->points, polygonCB, sizeof(DELTA_POINT) * wParam->numPoints);

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, PolygonCB), (void*) wParam, NULL);
}

static void message_EllipseSC(rdpContext* context, ELLIPSE_SC_ORDER* ellipseSC)
{
	ELLIPSE_SC_ORDER* wParam;

	wParam = (ELLIPSE_SC_ORDER*) malloc(sizeof(ELLIPSE_SC_ORDER));
	CopyMemory(wParam, ellipseSC, sizeof(ELLIPSE_SC_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, EllipseSC), (void*) wParam, NULL);
}

static void message_EllipseCB(rdpContext* context, ELLIPSE_CB_ORDER* ellipseCB)
{
	ELLIPSE_CB_ORDER* wParam;

	wParam = (ELLIPSE_CB_ORDER*) malloc(sizeof(ELLIPSE_CB_ORDER));
	CopyMemory(wParam, ellipseCB, sizeof(ELLIPSE_CB_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, EllipseCB), (void*) wParam, NULL);
}

/* Secondary Update */

static void message_CacheBitmap(rdpContext* context, CACHE_BITMAP_ORDER* cacheBitmapOrder)
{
	CACHE_BITMAP_ORDER* wParam;

	wParam = (CACHE_BITMAP_ORDER*) malloc(sizeof(CACHE_BITMAP_ORDER));
	CopyMemory(wParam, cacheBitmapOrder, sizeof(CACHE_BITMAP_ORDER));

	wParam->bitmapDataStream = (BYTE*) malloc(wParam->bitmapLength);
	CopyMemory(wParam->bitmapDataStream, cacheBitmapOrder, wParam->bitmapLength);

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheBitmap), (void*) wParam, NULL);
}

static void message_CacheBitmapV2(rdpContext* context, CACHE_BITMAP_V2_ORDER* cacheBitmapV2Order)
{
	CACHE_BITMAP_V2_ORDER* wParam;

	wParam = (CACHE_BITMAP_V2_ORDER*) malloc(sizeof(CACHE_BITMAP_V2_ORDER));
	CopyMemory(wParam, cacheBitmapV2Order, sizeof(CACHE_BITMAP_V2_ORDER));

	wParam->bitmapDataStream = (BYTE*) malloc(wParam->bitmapLength);
	CopyMemory(wParam->bitmapDataStream, cacheBitmapV2Order, wParam->bitmapLength);

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheBitmapV2), (void*) wParam, NULL);
}

static void message_CacheBitmapV3(rdpContext* context, CACHE_BITMAP_V3_ORDER* cacheBitmapV3Order)
{
	CACHE_BITMAP_V3_ORDER* wParam;

	wParam = (CACHE_BITMAP_V3_ORDER*) malloc(sizeof(CACHE_BITMAP_V3_ORDER));
	CopyMemory(wParam, cacheBitmapV3Order, sizeof(CACHE_BITMAP_V3_ORDER));

	wParam->bitmapData.data = (BYTE*) malloc(wParam->bitmapData.length);
	CopyMemory(wParam->bitmapData.data, cacheBitmapV3Order->bitmapData.data, wParam->bitmapData.length);

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheBitmapV3), (void*) wParam, NULL);
}

static void message_CacheColorTable(rdpContext* context, CACHE_COLOR_TABLE_ORDER* cacheColorTableOrder)
{
	CACHE_COLOR_TABLE_ORDER* wParam;

	wParam = (CACHE_COLOR_TABLE_ORDER*) malloc(sizeof(CACHE_COLOR_TABLE_ORDER));
	CopyMemory(wParam, cacheColorTableOrder, sizeof(CACHE_COLOR_TABLE_ORDER));

	wParam->colorTable = (UINT32*) malloc(sizeof(UINT32) * wParam->numberColors);
	CopyMemory(wParam->colorTable, cacheColorTableOrder->colorTable, wParam->numberColors);

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheColorTable), (void*) wParam, NULL);
}

static void message_CacheGlyph(rdpContext* context, CACHE_GLYPH_ORDER* cacheGlyphOrder)
{
	int index;
	CACHE_GLYPH_ORDER* wParam;

	wParam = (CACHE_GLYPH_ORDER*) malloc(sizeof(CACHE_GLYPH_ORDER));
	CopyMemory(wParam, cacheGlyphOrder, sizeof(CACHE_GLYPH_ORDER));

	for (index = 0; index < wParam->cGlyphs; index++)
	{
		wParam->glyphData[index] = (GLYPH_DATA*) malloc(sizeof(GLYPH_DATA));
		CopyMemory(wParam->glyphData[index], cacheGlyphOrder->glyphData[index], sizeof(GLYPH_DATA));
	}

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheGlyph), (void*) wParam, NULL);
}

static void message_CacheGlyphV2(rdpContext* context, CACHE_GLYPH_V2_ORDER* cacheGlyphV2Order)
{
	int index;
	CACHE_GLYPH_V2_ORDER* wParam;

	wParam = (CACHE_GLYPH_V2_ORDER*) malloc(sizeof(CACHE_GLYPH_V2_ORDER));
	CopyMemory(wParam, cacheGlyphV2Order, sizeof(CACHE_GLYPH_V2_ORDER));

	for (index = 0; index < wParam->cGlyphs; index++)
	{
		wParam->glyphData[index] = (GLYPH_DATA_V2*) malloc(sizeof(GLYPH_DATA_V2));
		CopyMemory(wParam->glyphData[index], cacheGlyphV2Order->glyphData[index], sizeof(GLYPH_DATA_V2));
	}

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheGlyphV2), (void*) wParam, NULL);
}

static void message_CacheBrush(rdpContext* context, CACHE_BRUSH_ORDER* cacheBrushOrder)
{
	CACHE_BRUSH_ORDER* wParam;

	wParam = (CACHE_BRUSH_ORDER*) malloc(sizeof(CACHE_BRUSH_ORDER));
	CopyMemory(wParam, cacheBrushOrder, sizeof(CACHE_BRUSH_ORDER));

	//wParam->data = (BYTE*) malloc(wParam->length);
	//CopyMemory(wParam->data, cacheBrushOrder->data, wParam->length);

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheBrush), (void*) wParam, NULL);
}

/* Alternate Secondary Update */

static void message_CreateOffscreenBitmap(rdpContext* context, CREATE_OFFSCREEN_BITMAP_ORDER* createOffscreenBitmap)
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

static void message_SwitchSurface(rdpContext* context, SWITCH_SURFACE_ORDER* switchSurface)
{
	SWITCH_SURFACE_ORDER* wParam;

	wParam = (SWITCH_SURFACE_ORDER*) malloc(sizeof(SWITCH_SURFACE_ORDER));
	CopyMemory(wParam, switchSurface, sizeof(SWITCH_SURFACE_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, SwitchSurface), (void*) wParam, NULL);
}

static void message_CreateNineGridBitmap(rdpContext* context, CREATE_NINE_GRID_BITMAP_ORDER* createNineGridBitmap)
{
	CREATE_NINE_GRID_BITMAP_ORDER* wParam;

	wParam = (CREATE_NINE_GRID_BITMAP_ORDER*) malloc(sizeof(CREATE_NINE_GRID_BITMAP_ORDER));
	CopyMemory(wParam, createNineGridBitmap, sizeof(CREATE_NINE_GRID_BITMAP_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, CreateNineGridBitmap), (void*) wParam, NULL);
}

static void message_FrameMarker(rdpContext* context, FRAME_MARKER_ORDER* frameMarker)
{
	FRAME_MARKER_ORDER* wParam;

	wParam = (FRAME_MARKER_ORDER*) malloc(sizeof(FRAME_MARKER_ORDER));
	CopyMemory(wParam, frameMarker, sizeof(FRAME_MARKER_ORDER));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, FrameMarker), (void*) wParam, NULL);
}

static void message_StreamBitmapFirst(rdpContext* context, STREAM_BITMAP_FIRST_ORDER* streamBitmapFirst)
{
	STREAM_BITMAP_FIRST_ORDER* wParam;

	wParam = (STREAM_BITMAP_FIRST_ORDER*) malloc(sizeof(STREAM_BITMAP_FIRST_ORDER));
	CopyMemory(wParam, streamBitmapFirst, sizeof(STREAM_BITMAP_FIRST_ORDER));

	/* TODO: complete copy */

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, StreamBitmapFirst), (void*) wParam, NULL);
}

static void message_StreamBitmapNext(rdpContext* context, STREAM_BITMAP_NEXT_ORDER* streamBitmapNext)
{
	STREAM_BITMAP_NEXT_ORDER* wParam;

	wParam = (STREAM_BITMAP_NEXT_ORDER*) malloc(sizeof(STREAM_BITMAP_NEXT_ORDER));
	CopyMemory(wParam, streamBitmapNext, sizeof(STREAM_BITMAP_NEXT_ORDER));

	/* TODO: complete copy */

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, StreamBitmapNext), (void*) wParam, NULL);
}

static void message_DrawGdiPlusFirst(rdpContext* context, DRAW_GDIPLUS_FIRST_ORDER* drawGdiPlusFirst)
{
	DRAW_GDIPLUS_FIRST_ORDER* wParam;

	wParam = (DRAW_GDIPLUS_FIRST_ORDER*) malloc(sizeof(DRAW_GDIPLUS_FIRST_ORDER));
	CopyMemory(wParam, drawGdiPlusFirst, sizeof(DRAW_GDIPLUS_FIRST_ORDER));

	/* TODO: complete copy */

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusFirst), (void*) wParam, NULL);
}

static void message_DrawGdiPlusNext(rdpContext* context, DRAW_GDIPLUS_NEXT_ORDER* drawGdiPlusNext)
{
	DRAW_GDIPLUS_NEXT_ORDER* wParam;

	wParam = (DRAW_GDIPLUS_NEXT_ORDER*) malloc(sizeof(DRAW_GDIPLUS_NEXT_ORDER));
	CopyMemory(wParam, drawGdiPlusNext, sizeof(DRAW_GDIPLUS_NEXT_ORDER));

	/* TODO: complete copy */

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusNext), (void*) wParam, NULL);
}

static void message_DrawGdiPlusEnd(rdpContext* context, DRAW_GDIPLUS_END_ORDER* drawGdiPlusEnd)
{
	DRAW_GDIPLUS_END_ORDER* wParam;

	wParam = (DRAW_GDIPLUS_END_ORDER*) malloc(sizeof(DRAW_GDIPLUS_END_ORDER));
	CopyMemory(wParam, drawGdiPlusEnd, sizeof(DRAW_GDIPLUS_END_ORDER));

	/* TODO: complete copy */

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusEnd), (void*) wParam, NULL);
}

static void message_DrawGdiPlusCacheFirst(rdpContext* context, DRAW_GDIPLUS_CACHE_FIRST_ORDER* drawGdiPlusCacheFirst)
{
	DRAW_GDIPLUS_CACHE_FIRST_ORDER* wParam;

	wParam = (DRAW_GDIPLUS_CACHE_FIRST_ORDER*) malloc(sizeof(DRAW_GDIPLUS_CACHE_FIRST_ORDER));
	CopyMemory(wParam, drawGdiPlusCacheFirst, sizeof(DRAW_GDIPLUS_CACHE_FIRST_ORDER));

	/* TODO: complete copy */

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusCacheFirst), (void*) wParam, NULL);
}

static void message_DrawGdiPlusCacheNext(rdpContext* context, DRAW_GDIPLUS_CACHE_NEXT_ORDER* drawGdiPlusCacheNext)
{
	DRAW_GDIPLUS_CACHE_NEXT_ORDER* wParam;

	wParam = (DRAW_GDIPLUS_CACHE_NEXT_ORDER*) malloc(sizeof(DRAW_GDIPLUS_CACHE_NEXT_ORDER));
	CopyMemory(wParam, drawGdiPlusCacheNext, sizeof(DRAW_GDIPLUS_CACHE_NEXT_ORDER));

	/* TODO: complete copy */

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusCacheNext), (void*) wParam, NULL);
}

static void message_DrawGdiPlusCacheEnd(rdpContext* context, DRAW_GDIPLUS_CACHE_END_ORDER* drawGdiPlusCacheEnd)
{
	DRAW_GDIPLUS_CACHE_END_ORDER* wParam;

	wParam = (DRAW_GDIPLUS_CACHE_END_ORDER*) malloc(sizeof(DRAW_GDIPLUS_CACHE_END_ORDER));
	CopyMemory(wParam, drawGdiPlusCacheEnd, sizeof(DRAW_GDIPLUS_CACHE_END_ORDER));

	/* TODO: complete copy */

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusCacheEnd), (void*) wParam, NULL);
}

/* Window Update */

static void message_WindowCreate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* windowState)
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

static void message_WindowUpdate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* windowState)
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

static void message_WindowIcon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_ICON_ORDER* windowIcon)
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

static void message_WindowCachedIcon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_CACHED_ICON_ORDER* windowCachedIcon)
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

static void message_WindowDelete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	WINDOW_ORDER_INFO* wParam;

	wParam = (WINDOW_ORDER_INFO*) malloc(sizeof(WINDOW_ORDER_INFO));
	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowDelete), (void*) wParam, NULL);
}

static void message_NotifyIconCreate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
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

static void message_NotifyIconUpdate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
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

static void message_NotifyIconDelete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	WINDOW_ORDER_INFO* wParam;

	wParam = (WINDOW_ORDER_INFO*) malloc(sizeof(WINDOW_ORDER_INFO));
	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, NotifyIconDelete), (void*) wParam, NULL);
}

static void message_MonitoredDesktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, MONITORED_DESKTOP_ORDER* monitoredDesktop)
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

static void message_NonMonitoredDesktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	WINDOW_ORDER_INFO* wParam;

	wParam = (WINDOW_ORDER_INFO*) malloc(sizeof(WINDOW_ORDER_INFO));
	CopyMemory(wParam, orderInfo, sizeof(WINDOW_ORDER_INFO));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, NonMonitoredDesktop), (void*) wParam, NULL);
}

/* Pointer Update */

static void message_PointerPosition(rdpContext* context, POINTER_POSITION_UPDATE* pointerPosition)
{
	POINTER_POSITION_UPDATE* wParam;

	wParam = (POINTER_POSITION_UPDATE*) malloc(sizeof(POINTER_POSITION_UPDATE));
	CopyMemory(wParam, pointerPosition, sizeof(POINTER_POSITION_UPDATE));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerPosition), (void*) wParam, NULL);
}

static void message_PointerSystem(rdpContext* context, POINTER_SYSTEM_UPDATE* pointerSystem)
{
	POINTER_SYSTEM_UPDATE* wParam;

	wParam = (POINTER_SYSTEM_UPDATE*) malloc(sizeof(POINTER_SYSTEM_UPDATE));
	CopyMemory(wParam, pointerSystem, sizeof(POINTER_SYSTEM_UPDATE));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerSystem), (void*) wParam, NULL);
}

static void message_PointerColor(rdpContext* context, POINTER_COLOR_UPDATE* pointerColor)
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

static void message_PointerNew(rdpContext* context, POINTER_NEW_UPDATE* pointerNew)
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

static void message_PointerCached(rdpContext* context, POINTER_CACHED_UPDATE* pointerCached)
{
	POINTER_CACHED_UPDATE* wParam;

	wParam = (POINTER_CACHED_UPDATE*) malloc(sizeof(POINTER_CACHED_UPDATE));
	CopyMemory(wParam, pointerCached, sizeof(POINTER_CACHED_UPDATE));

	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerCached), (void*) wParam, NULL);
}

/* Message Queue */

int message_process_update_class(rdpMessage* update, wMessage* msg, int type)
{
	int status = 0;

	switch (type)
	{
		case Update_BeginPaint:
			IFCALL(update->BeginPaint, msg->context);
			break;

		case Update_EndPaint:
			IFCALL(update->EndPaint, msg->context);
			break;

		case Update_SetBounds:
			IFCALL(update->SetBounds, msg->context, (rdpBounds*) msg->wParam);
			if (msg->wParam)
				free(msg->wParam);
			break;

		case Update_Synchronize:
			IFCALL(update->Synchronize, msg->context);
			break;

		case Update_DesktopResize:
			IFCALL(update->DesktopResize, msg->context);
			break;

		case Update_BitmapUpdate:
			IFCALL(update->BitmapUpdate, msg->context, (BITMAP_UPDATE*) msg->wParam);
			{
				int index;
				BITMAP_UPDATE* wParam = (BITMAP_UPDATE*) msg->wParam;

				for (index = 0; index < wParam->number; index++)
					free(wParam->rectangles[index].bitmapDataStream);

				free(wParam);
			}
			break;

		case Update_Palette:
			IFCALL(update->Palette, msg->context, (PALETTE_UPDATE*) msg->wParam);
			free(msg->wParam);
			break;

		case Update_PlaySound:
			IFCALL(update->PlaySound, msg->context, (PLAY_SOUND_UPDATE*) msg->wParam);
			free(msg->wParam);
			break;

		case Update_RefreshRect:
			IFCALL(update->RefreshRect, msg->context,
					(BYTE) (size_t) msg->wParam, (RECTANGLE_16*) msg->lParam);
			free(msg->lParam);
			break;

		case Update_SuppressOutput:
			IFCALL(update->SuppressOutput, msg->context,
					(BYTE) (size_t) msg->wParam, (RECTANGLE_16*) msg->lParam);
			free(msg->lParam);
			break;

		case Update_SurfaceCommand:
			IFCALL(update->SurfaceCommand, msg->context, (STREAM*) msg->wParam);
			{
				STREAM* s = (STREAM*) msg->wParam;
				free(s->data);
				free(s);
			}
			break;

		case Update_SurfaceBits:
			IFCALL(update->SurfaceBits, msg->context, (SURFACE_BITS_COMMAND*) msg->wParam);
			{
				SURFACE_BITS_COMMAND* wParam = (SURFACE_BITS_COMMAND*) msg->wParam;
				free(wParam->bitmapData);
				free(wParam);
			}
			break;

		case Update_SurfaceFrameMarker:
			IFCALL(update->SurfaceFrameMarker, msg->context, (SURFACE_FRAME_MARKER*) msg->wParam);
			free(msg->wParam);
			break;

		case Update_SurfaceFrameAcknowledge:
			IFCALL(update->SurfaceFrameAcknowledge, msg->context, (UINT32) (size_t) msg->wParam);
			break;

		default:
			status = -1;
			break;
	}

	return status;
}

int message_process_primary_update_class(rdpMessage* update, wMessage* msg, int type)
{
	int status = 0;

	switch (type)
	{
		case PrimaryUpdate_DstBlt:
			IFCALL(update->DstBlt, msg->context, (DSTBLT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_PatBlt:
			IFCALL(update->PatBlt, msg->context, (PATBLT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_ScrBlt:
			IFCALL(update->ScrBlt, msg->context, (SCRBLT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_OpaqueRect:
			IFCALL(update->OpaqueRect, msg->context, (OPAQUE_RECT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_DrawNineGrid:
			IFCALL(update->DrawNineGrid, msg->context, (DRAW_NINE_GRID_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_MultiDstBlt:
			IFCALL(update->MultiDstBlt, msg->context, (MULTI_DSTBLT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_MultiPatBlt:
			IFCALL(update->MultiPatBlt, msg->context, (MULTI_PATBLT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_MultiScrBlt:
			IFCALL(update->MultiScrBlt, msg->context, (MULTI_SCRBLT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_MultiOpaqueRect:
			IFCALL(update->MultiOpaqueRect, msg->context, (MULTI_OPAQUE_RECT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_MultiDrawNineGrid:
			IFCALL(update->MultiDrawNineGrid, msg->context, (MULTI_DRAW_NINE_GRID_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_LineTo:
			IFCALL(update->LineTo, msg->context, (LINE_TO_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_Polyline:
			IFCALL(update->Polyline, msg->context, (POLYLINE_ORDER*) msg->wParam);
			{
				POLYLINE_ORDER* wParam = (POLYLINE_ORDER*) msg->wParam;

				free(wParam->points);
				free(wParam);
			}
			break;

		case PrimaryUpdate_MemBlt:
			IFCALL(update->MemBlt, msg->context, (MEMBLT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_Mem3Blt:
			IFCALL(update->Mem3Blt, msg->context, (MEM3BLT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_SaveBitmap:
			IFCALL(update->SaveBitmap, msg->context, (SAVE_BITMAP_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_GlyphIndex:
			IFCALL(update->GlyphIndex, msg->context, (GLYPH_INDEX_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_FastIndex:
			IFCALL(update->FastIndex, msg->context, (FAST_INDEX_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_FastGlyph:
			IFCALL(update->FastGlyph, msg->context, (FAST_GLYPH_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_PolygonSC:
			IFCALL(update->PolygonSC, msg->context, (POLYGON_SC_ORDER*) msg->wParam);
			{
				POLYGON_SC_ORDER* wParam = (POLYGON_SC_ORDER*) msg->wParam;

				free(wParam->points);
				free(wParam);
			}
			break;

		case PrimaryUpdate_PolygonCB:
			IFCALL(update->PolygonCB, msg->context, (POLYGON_CB_ORDER*) msg->wParam);
			{
				POLYGON_CB_ORDER* wParam = (POLYGON_CB_ORDER*) msg->wParam;

				free(wParam->points);
				free(wParam);
			}
			break;

		case PrimaryUpdate_EllipseSC:
			IFCALL(update->EllipseSC, msg->context, (ELLIPSE_SC_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case PrimaryUpdate_EllipseCB:
			IFCALL(update->EllipseCB, msg->context, (ELLIPSE_CB_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		default:
			status = -1;
			break;
	}

	return status;
}

int message_process_secondary_update_class(rdpMessage* update, wMessage* msg, int type)
{
	int status = 0;

	switch (type)
	{
		case SecondaryUpdate_CacheBitmap:
			IFCALL(update->CacheBitmap, msg->context, (CACHE_BITMAP_ORDER*) msg->wParam);
			{
				CACHE_BITMAP_ORDER* wParam = (CACHE_BITMAP_ORDER*) msg->wParam;

				free(wParam->bitmapDataStream);
				free(wParam);
			}
			break;

		case SecondaryUpdate_CacheBitmapV2:
			IFCALL(update->CacheBitmapV2, msg->context, (CACHE_BITMAP_V2_ORDER*) msg->wParam);
			{
				CACHE_BITMAP_V2_ORDER* wParam = (CACHE_BITMAP_V2_ORDER*) msg->wParam;

				//free(wParam->bitmapDataStream);
				free(wParam);
			}
			break;

		case SecondaryUpdate_CacheBitmapV3:
			IFCALL(update->CacheBitmapV3, msg->context, (CACHE_BITMAP_V3_ORDER*) msg->wParam);
			{
				CACHE_BITMAP_V3_ORDER* wParam = (CACHE_BITMAP_V3_ORDER*) msg->wParam;

				free(wParam->bitmapData.data);
				free(wParam);
			}
			break;

		case SecondaryUpdate_CacheColorTable:
			IFCALL(update->CacheColorTable, msg->context, (CACHE_COLOR_TABLE_ORDER*) msg->wParam);
			{
				CACHE_COLOR_TABLE_ORDER* wParam = (CACHE_COLOR_TABLE_ORDER*) msg->wParam;

				free(wParam->colorTable);
				free(wParam);
			}
			break;

		case SecondaryUpdate_CacheGlyph:
			IFCALL(update->CacheGlyph, msg->context, (CACHE_GLYPH_ORDER*) msg->wParam);
			{
				int index;
				CACHE_GLYPH_ORDER* wParam = (CACHE_GLYPH_ORDER*) msg->wParam;

				for (index = 0; index < wParam->cGlyphs; index++)
					free(wParam->glyphData[index]);

				free(wParam);
			}
			break;

		case SecondaryUpdate_CacheGlyphV2:
			IFCALL(update->CacheGlyphV2, msg->context, (CACHE_GLYPH_V2_ORDER*) msg->wParam);
			{
				int index;
				CACHE_GLYPH_V2_ORDER* wParam = (CACHE_GLYPH_V2_ORDER*) msg->wParam;

				for (index = 0; index < wParam->cGlyphs; index++)
					free(wParam->glyphData[index]);

				free(wParam);
			}
			break;

		case SecondaryUpdate_CacheBrush:
			IFCALL(update->CacheBrush, msg->context, (CACHE_BRUSH_ORDER*) msg->wParam);
			{
				CACHE_BRUSH_ORDER* wParam = (CACHE_BRUSH_ORDER*) msg->wParam;

				//free(wParam->data);
				free(wParam);
			}
			break;

		default:
			status = -1;
			break;
	}

	return status;
}

int message_process_altsec_update_class(rdpMessage* update, wMessage* msg, int type)
{
	int status = 0;

	switch (type)
	{
		case AltSecUpdate_CreateOffscreenBitmap:
			IFCALL(update->CreateOffscreenBitmap, msg->context, (CREATE_OFFSCREEN_BITMAP_ORDER*) msg->wParam);
			{
				CREATE_OFFSCREEN_BITMAP_ORDER* wParam = (CREATE_OFFSCREEN_BITMAP_ORDER*) msg->wParam;

				free(wParam->deleteList.indices);
				free(wParam);
			}
			break;

		case AltSecUpdate_SwitchSurface:
			IFCALL(update->SwitchSurface, msg->context, (SWITCH_SURFACE_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case AltSecUpdate_CreateNineGridBitmap:
			IFCALL(update->CreateNineGridBitmap, msg->context, (CREATE_NINE_GRID_BITMAP_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case AltSecUpdate_FrameMarker:
			IFCALL(update->FrameMarker, msg->context, (FRAME_MARKER_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case AltSecUpdate_StreamBitmapFirst:
			IFCALL(update->StreamBitmapFirst, msg->context, (STREAM_BITMAP_FIRST_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case AltSecUpdate_StreamBitmapNext:
			IFCALL(update->StreamBitmapNext, msg->context, (STREAM_BITMAP_NEXT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusFirst:
			IFCALL(update->DrawGdiPlusFirst, msg->context, (DRAW_GDIPLUS_FIRST_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusNext:
			IFCALL(update->DrawGdiPlusNext, msg->context, (DRAW_GDIPLUS_NEXT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusEnd:
			IFCALL(update->DrawGdiPlusEnd, msg->context, (DRAW_GDIPLUS_END_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusCacheFirst:
			IFCALL(update->DrawGdiPlusCacheFirst, msg->context, (DRAW_GDIPLUS_CACHE_FIRST_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusCacheNext:
			IFCALL(update->DrawGdiPlusCacheNext, msg->context, (DRAW_GDIPLUS_CACHE_NEXT_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusCacheEnd:
			IFCALL(update->DrawGdiPlusCacheEnd, msg->context, (DRAW_GDIPLUS_CACHE_END_ORDER*) msg->wParam);
			free(msg->wParam);
			break;

		default:
			status = -1;
			break;
	}

	return status;
}

int message_process_window_update_class(rdpMessage* update, wMessage* msg, int type)
{
	int status = 0;

	switch (type)
	{
		case WindowUpdate_WindowCreate:
			IFCALL(update->WindowCreate, msg->context, (WINDOW_ORDER_INFO*) msg->wParam,
					(WINDOW_STATE_ORDER*) msg->lParam);
			free(msg->wParam);
			free(msg->lParam);
			break;

		case WindowUpdate_WindowUpdate:
			IFCALL(update->WindowCreate, msg->context, (WINDOW_ORDER_INFO*) msg->wParam,
					(WINDOW_STATE_ORDER*) msg->lParam);
			free(msg->wParam);
			free(msg->lParam);
			break;

		case WindowUpdate_WindowIcon:
			IFCALL(update->WindowIcon, msg->context, (WINDOW_ORDER_INFO*) msg->wParam,
					(WINDOW_ICON_ORDER*) msg->lParam);
			free(msg->wParam);
			free(msg->lParam);
			break;

		case WindowUpdate_WindowCachedIcon:
			IFCALL(update->WindowCachedIcon, msg->context, (WINDOW_ORDER_INFO*) msg->wParam,
					(WINDOW_CACHED_ICON_ORDER*) msg->lParam);
			free(msg->wParam);
			free(msg->lParam);
			break;

		case WindowUpdate_WindowDelete:
			IFCALL(update->WindowDelete, msg->context, (WINDOW_ORDER_INFO*) msg->wParam);
			free(msg->wParam);
			break;

		case WindowUpdate_NotifyIconCreate:
			IFCALL(update->NotifyIconCreate, msg->context, (WINDOW_ORDER_INFO*) msg->wParam,
					(NOTIFY_ICON_STATE_ORDER*) msg->lParam);
			free(msg->wParam);
			free(msg->lParam);
			break;

		case WindowUpdate_NotifyIconUpdate:
			IFCALL(update->NotifyIconUpdate, msg->context, (WINDOW_ORDER_INFO*) msg->wParam,
					(NOTIFY_ICON_STATE_ORDER*) msg->lParam);
			free(msg->wParam);
			free(msg->lParam);
			break;

		case WindowUpdate_NotifyIconDelete:
			IFCALL(update->NotifyIconDelete, msg->context, (WINDOW_ORDER_INFO*) msg->wParam);
			free(msg->wParam);
			break;

		case WindowUpdate_MonitoredDesktop:
			IFCALL(update->MonitoredDesktop, msg->context, (WINDOW_ORDER_INFO*) msg->wParam,
					(MONITORED_DESKTOP_ORDER*) msg->lParam);
			{
				MONITORED_DESKTOP_ORDER* lParam = (MONITORED_DESKTOP_ORDER*) msg->lParam;

				free(msg->wParam);

				free(lParam->windowIds);
				free(lParam);
			}
			break;

		case WindowUpdate_NonMonitoredDesktop:
			IFCALL(update->NonMonitoredDesktop, msg->context, (WINDOW_ORDER_INFO*) msg->wParam);
			free(msg->wParam);
			break;

		default:
			status = -1;
			break;
	}

	return status;
}

int message_process_pointer_update_class(rdpMessage* update, wMessage* msg, int type)
{
	int status = 0;

	switch (type)
	{
		case PointerUpdate_PointerPosition:
			IFCALL(update->PointerPosition, msg->context, (POINTER_POSITION_UPDATE*) msg->wParam);
			free(msg->wParam);
			break;

		case PointerUpdate_PointerSystem:
			IFCALL(update->PointerSystem, msg->context, (POINTER_SYSTEM_UPDATE*) msg->wParam);
			free(msg->wParam);
			break;

		case PointerUpdate_PointerColor:
			IFCALL(update->PointerColor, msg->context, (POINTER_COLOR_UPDATE*) msg->wParam);
			{
				POINTER_COLOR_UPDATE* wParam = (POINTER_COLOR_UPDATE*) msg->wParam;

				//free(wParam->andMaskData);
				//free(wParam->xorMaskData);
				free(wParam);
			}
			break;

		case PointerUpdate_PointerNew:
			IFCALL(update->PointerNew, msg->context, (POINTER_NEW_UPDATE*) msg->wParam);
			{
				POINTER_NEW_UPDATE* wParam = (POINTER_NEW_UPDATE*) msg->wParam;

				//free(wParam->colorPtrAttr.andMaskData);
				//free(wParam->colorPtrAttr.xorMaskData);
				free(wParam);
			}
			break;

		case PointerUpdate_PointerCached:
			IFCALL(update->PointerCached, msg->context, (POINTER_CACHED_UPDATE*) msg->wParam);
			free(msg->wParam);
			break;

		default:
			status = -1;
			break;
	}

	return status;
}

int message_process_class(rdpMessage* update, wMessage* msg, int msgClass, int msgType)
{
	int status = 0;

	switch (msgClass)
	{
		case Update_Class:
			status = message_process_update_class(update, msg, msgType);
			break;

		case PrimaryUpdate_Class:
			status = message_process_primary_update_class(update, msg, msgType);
			break;

		case SecondaryUpdate_Class:
			status = message_process_secondary_update_class(update, msg, msgType);
			break;

		case AltSecUpdate_Class:
			status = message_process_altsec_update_class(update, msg, msgType);
			break;

		case WindowUpdate_Class:
			status = message_process_window_update_class(update, msg, msgType);
			break;

		case PointerUpdate_Class:
			status = message_process_pointer_update_class(update, msg, msgType);
			break;

		default:
			status = -1;
			break;
	}

	if (status < 0)
		printf("Unknown message: class: %d type: %d\n", msgClass, msgType);

	return status;
}

int message_process_pending_updates(rdpUpdate* update)
{
	int status;
	int msgClass;
	int msgType;
	wMessage message;
	wMessageQueue* queue;

	queue = update->queue;

	while (1)
	{
		status = MessageQueue_Peek(queue, &message, TRUE);

		if (!status)
			break;

		if (message.type == WMQ_QUIT)
			break;

		msgClass = GetMessageClass(message.type);
		msgType = GetMessageType(message.type);

		status = message_process_class(update->message, &message, msgClass, msgType);
	}

	return 0;
}

void message_register_interface(rdpMessage* message, rdpUpdate* update)
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

	update->BeginPaint = message_BeginPaint;
	update->EndPaint = message_EndPaint;
	update->SetBounds = message_SetBounds;
	update->Synchronize = message_Synchronize;
	update->DesktopResize = message_DesktopResize;
	update->BitmapUpdate = message_BitmapUpdate;
	update->Palette = message_Palette;
	update->PlaySound = message_PlaySound;
	update->RefreshRect = message_RefreshRect;
	update->SuppressOutput = message_SuppressOutput;
	update->SurfaceCommand = message_SurfaceCommand;
	update->SurfaceBits = message_SurfaceBits;
	update->SurfaceFrameMarker = message_SurfaceFrameMarker;
	update->SurfaceFrameAcknowledge = message_SurfaceFrameAcknowledge;

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

	primary->DstBlt = message_DstBlt;
	primary->PatBlt = message_PatBlt;
	primary->ScrBlt = message_ScrBlt;
	primary->OpaqueRect = message_OpaqueRect;
	primary->DrawNineGrid = message_DrawNineGrid;
	primary->MultiDstBlt = message_MultiDstBlt;
	primary->MultiPatBlt = message_MultiPatBlt;
	primary->MultiScrBlt = message_MultiScrBlt;
	primary->MultiOpaqueRect = message_MultiOpaqueRect;
	primary->MultiDrawNineGrid = message_MultiDrawNineGrid;
	primary->LineTo = message_LineTo;
	primary->Polyline = message_Polyline;
	primary->MemBlt = message_MemBlt;
	primary->Mem3Blt = message_Mem3Blt;
	primary->SaveBitmap = message_SaveBitmap;
	primary->GlyphIndex = message_GlyphIndex;
	primary->FastIndex = message_FastIndex;
	primary->FastGlyph = message_FastGlyph;
	primary->PolygonSC = message_PolygonSC;
	primary->PolygonCB = message_PolygonCB;
	primary->EllipseSC = message_EllipseSC;
	primary->EllipseCB = message_EllipseCB;

	/* Secondary Update */

	message->CacheBitmap = secondary->CacheBitmap;
	message->CacheBitmapV2 = secondary->CacheBitmapV2;
	message->CacheBitmapV3 = secondary->CacheBitmapV3;
	message->CacheColorTable = secondary->CacheColorTable;
	message->CacheGlyph = secondary->CacheGlyph;
	message->CacheGlyphV2 = secondary->CacheGlyphV2;
	message->CacheBrush = secondary->CacheBrush;

	secondary->CacheBitmap = message_CacheBitmap;
	secondary->CacheBitmapV2 = message_CacheBitmapV2;
	secondary->CacheBitmapV3 = message_CacheBitmapV3;
	secondary->CacheColorTable = message_CacheColorTable;
	secondary->CacheGlyph = message_CacheGlyph;
	secondary->CacheGlyphV2 = message_CacheGlyphV2;
	secondary->CacheBrush = message_CacheBrush;

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

	altsec->CreateOffscreenBitmap = message_CreateOffscreenBitmap;
	altsec->SwitchSurface = message_SwitchSurface;
	altsec->CreateNineGridBitmap = message_CreateNineGridBitmap;
	altsec->FrameMarker = message_FrameMarker;
	altsec->StreamBitmapFirst = message_StreamBitmapFirst;
	altsec->StreamBitmapNext = message_StreamBitmapNext;
	altsec->DrawGdiPlusFirst = message_DrawGdiPlusFirst;
	altsec->DrawGdiPlusNext = message_DrawGdiPlusNext;
	altsec->DrawGdiPlusEnd = message_DrawGdiPlusEnd;
	altsec->DrawGdiPlusCacheFirst = message_DrawGdiPlusCacheFirst;
	altsec->DrawGdiPlusCacheNext = message_DrawGdiPlusCacheNext;
	altsec->DrawGdiPlusCacheEnd = message_DrawGdiPlusCacheEnd;

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

	window->WindowCreate = message_WindowCreate;
	window->WindowUpdate = message_WindowUpdate;
	window->WindowIcon = message_WindowIcon;
	window->WindowCachedIcon = message_WindowCachedIcon;
	window->WindowDelete = message_WindowDelete;
	window->NotifyIconCreate = message_NotifyIconCreate;
	window->NotifyIconUpdate = message_NotifyIconUpdate;
	window->NotifyIconDelete = message_NotifyIconDelete;
	window->MonitoredDesktop = message_MonitoredDesktop;
	window->NonMonitoredDesktop = message_NonMonitoredDesktop;

	/* Pointer Update */

	message->PointerPosition = pointer->PointerPosition;
	message->PointerSystem = pointer->PointerSystem;
	message->PointerColor = pointer->PointerColor;
	message->PointerNew = pointer->PointerNew;
	message->PointerCached = pointer->PointerCached;

	pointer->PointerPosition = message_PointerPosition;
	pointer->PointerSystem = message_PointerSystem;
	pointer->PointerColor = message_PointerColor;
	pointer->PointerNew = message_PointerNew;
	pointer->PointerCached = message_PointerCached;
}

rdpMessage* message_new(rdpUpdate* update)
{
	rdpMessage* message;

	message = (rdpMessage*) malloc(sizeof(rdpMessage));

	if (message)
	{
		message->update = update;
		update->queue = MessageQueue_New();
		message_register_interface(message, update);
	}

	return message;
}

void message_free(rdpMessage* message)
{
	if (message)
	{
		MessageQueue_Free(message->update->queue);
		free(message);
	}
}
