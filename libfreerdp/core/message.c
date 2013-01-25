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

void message_BeginPaint(rdpContext* context)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(Update, BeginPaint), NULL, NULL);
}

void message_EndPaint(rdpContext* context)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(Update, EndPaint), NULL, NULL);
}

void message_SetBounds(rdpContext* context, rdpBounds* bounds)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(Update, SetBounds), (void*) bounds, NULL);
}

void message_Synchronize(rdpContext* context)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(Update, Synchronize), NULL, NULL);
}

void message_DesktopResize(rdpContext* context)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(Update, DesktopResize), NULL, NULL);
}

void message_BitmapUpdate(rdpContext* context, BITMAP_UPDATE* bitmap)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(Update, BitmapUpdate), (void*) bitmap, NULL);
}

void message_Palette(rdpContext* context, PALETTE_UPDATE* palette)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(Update, Palette), (void*) palette, NULL);
}

void message_PlaySound(rdpContext* context, PLAY_SOUND_UPDATE* playSound)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(Update, PlaySound), (void*) playSound, NULL);
}

void message_RefreshRect(rdpContext* context, BYTE count, RECTANGLE_16* areas)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(Update, RefreshRect), (void*) (size_t) count, (void*) areas);
}

void message_SuppressOutput(rdpContext* context, BYTE allow, RECTANGLE_16* area)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(Update, SuppressOutput), (void*) (size_t) allow, (void*) area);
}

void message_SurfaceCommand(rdpContext* context, STREAM* s)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(Update, SurfaceCommand), (void*) s, NULL);
}

void message_SurfaceBits(rdpContext* context, SURFACE_BITS_COMMAND* surfaceBitsCommand)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(Update, SurfaceBits), (void*) surfaceBitsCommand, NULL);
}

void message_SurfaceFrameMarker(rdpContext* context, SURFACE_FRAME_MARKER* surfaceFrameMarker)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(Update, SurfaceFrameMarker), (void*) surfaceFrameMarker, NULL);
}

void message_SurfaceFrameAcknowledge(rdpContext* context, UINT32 frameId)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(Update, SurfaceFrameAcknowledge), (void*) (size_t) frameId, NULL);
}

/* Primary Update */

void message_DstBlt(rdpContext* context, DSTBLT_ORDER* dstBlt)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, DstBlt), (void*) dstBlt, NULL);
}

void message_PatBlt(rdpContext* context, PATBLT_ORDER* patBlt)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, PatBlt), (void*) patBlt, NULL);
}

void message_ScrBlt(rdpContext* context, SCRBLT_ORDER* scrBlt)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, ScrBlt), (void*) scrBlt, NULL);
}

void message_OpaqueRect(rdpContext* context, OPAQUE_RECT_ORDER* opaqueRect)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, OpaqueRect), (void*) opaqueRect, NULL);
}

void message_DrawNineGrid(rdpContext* context, DRAW_NINE_GRID_ORDER* drawNineGrid)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, DrawNineGrid), (void*) drawNineGrid, NULL);
}

void message_MultiDstBlt(rdpContext* context, MULTI_DSTBLT_ORDER* multiDstBlt)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiDstBlt), (void*) multiDstBlt, NULL);
}

void message_MultiPatBlt(rdpContext* context, MULTI_PATBLT_ORDER* multiPatBlt)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiPatBlt), (void*) multiPatBlt, NULL);
}

void message_MultiScrBlt(rdpContext* context, MULTI_SCRBLT_ORDER* multiScrBlt)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiScrBlt), (void*) multiScrBlt, NULL);
}

void message_MultiOpaqueRect(rdpContext* context, MULTI_OPAQUE_RECT_ORDER* multiOpaqueRect)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiOpaqueRect), (void*) multiOpaqueRect, NULL);
}

void message_MultiDrawNineGrid(rdpContext* context, MULTI_DRAW_NINE_GRID_ORDER* multiDrawNineGrid)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiDrawNineGrid), (void*) multiDrawNineGrid, NULL);
}

void message_LineTo(rdpContext* context, LINE_TO_ORDER* lineTo)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, LineTo), (void*) lineTo, NULL);
}

void message_Polyline(rdpContext* context, POLYLINE_ORDER* polyline)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, Polyline), (void*) polyline, NULL);
}

void message_MemBlt(rdpContext* context, MEMBLT_ORDER* memBlt)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MemBlt), (void*) memBlt, NULL);
}

void message_Mem3Blt(rdpContext* context, MEM3BLT_ORDER* mem3Blt)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, Mem3Blt), (void*) mem3Blt, NULL);
}

void message_SaveBitmap(rdpContext* context, SAVE_BITMAP_ORDER* saveBitmap)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, SaveBitmap), (void*) saveBitmap, NULL);
}

void message_GlyphIndex(rdpContext* context, GLYPH_INDEX_ORDER* glyphIndex)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, GlyphIndex), (void*) glyphIndex, NULL);
}

void message_FastIndex(rdpContext* context, FAST_INDEX_ORDER* fastIndex)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, FastIndex), (void*) fastIndex, NULL);
}

void message_FastGlyph(rdpContext* context, FAST_GLYPH_ORDER* fastGlyph)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, FastGlyph), (void*) fastGlyph, NULL);
}

void message_PolygonSC(rdpContext* context, POLYGON_SC_ORDER* polygonSC)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, PolygonSC), (void*) polygonSC, NULL);
}

void message_PolygonCB(rdpContext* context, POLYGON_CB_ORDER* polygonCB)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, PolygonCB), (void*) polygonCB, NULL);
}

void message_EllipseSC(rdpContext* context, ELLIPSE_SC_ORDER* ellipseSC)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, EllipseSC), (void*) ellipseSC, NULL);
}

void message_EllipseCB(rdpContext* context, ELLIPSE_CB_ORDER* ellipseCB)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, EllipseCB), (void*) ellipseCB, NULL);
}

/* Secondary Update */

void message_CacheBitmap(rdpContext* context, CACHE_BITMAP_ORDER* cacheBitmapOrder)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheBitmap), (void*) cacheBitmapOrder, NULL);
}

void message_CacheBitmapV2(rdpContext* context, CACHE_BITMAP_V2_ORDER* cacheBitmapV2Order)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheBitmapV2), (void*) cacheBitmapV2Order, NULL);
}

void message_CacheBitmapV3(rdpContext* context, CACHE_BITMAP_V3_ORDER* cacheBitmapV3Order)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheBitmapV3), (void*) cacheBitmapV3Order, NULL);
}

void message_CacheColorTable(rdpContext* context, CACHE_COLOR_TABLE_ORDER* cacheColorTableOrder)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheColorTable), (void*) cacheColorTableOrder, NULL);
}

void message_CacheGlyph(rdpContext* context, CACHE_GLYPH_ORDER* cacheGlyphOrder)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheGlyph), (void*) cacheGlyphOrder, NULL);
}

void message_CacheGlyphV2(rdpContext* context, CACHE_GLYPH_V2_ORDER* cacheGlyphV2Order)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheGlyphV2), (void*) cacheGlyphV2Order, NULL);
}

void message_CacheBrush(rdpContext* context, CACHE_BRUSH_ORDER* cacheBrushOrder)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheBrush), (void*) cacheBrushOrder, NULL);
}

/* Alternate Secondary Update */

void message_CreateOffscreenBitmap(rdpContext* context, CREATE_OFFSCREEN_BITMAP_ORDER* createOffscreenBitmap)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(AltSecUpdate, CreateOffscreenBitmap), (void*) createOffscreenBitmap, NULL);
}

void message_SwitchSurface(rdpContext* context, SWITCH_SURFACE_ORDER* switchSurface)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(AltSecUpdate, SwitchSurface), (void*) switchSurface, NULL);
}

void message_CreateNineGridBitmap(rdpContext* context, CREATE_NINE_GRID_BITMAP_ORDER* createNineGridBitmap)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(AltSecUpdate, CreateNineGridBitmap), (void*) createNineGridBitmap, NULL);
}

void message_FrameMarker(rdpContext* context, FRAME_MARKER_ORDER* frameMarker)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(AltSecUpdate, FrameMarker), (void*) frameMarker, NULL);
}

void message_StreamBitmapFirst(rdpContext* context, STREAM_BITMAP_FIRST_ORDER* streamBitmapFirst)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(AltSecUpdate, StreamBitmapFirst), (void*) streamBitmapFirst, NULL);
}

void message_StreamBitmapNext(rdpContext* context, STREAM_BITMAP_NEXT_ORDER* streamBitmapNext)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(AltSecUpdate, StreamBitmapNext), (void*) streamBitmapNext, NULL);
}

void message_DrawGdiPlusFirst(rdpContext* context, DRAW_GDIPLUS_FIRST_ORDER* drawGdiPlusFirst)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusFirst), (void*) drawGdiPlusFirst, NULL);
}

void message_DrawGdiPlusNext(rdpContext* context, DRAW_GDIPLUS_NEXT_ORDER* drawGdiPlusNext)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusNext), (void*) drawGdiPlusNext, NULL);
}

void message_DrawGdiPlusEnd(rdpContext* context, DRAW_GDIPLUS_END_ORDER* drawGdiPlusEnd)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusEnd), (void*) drawGdiPlusEnd, NULL);
}

void message_DrawGdiPlusCacheFirst(rdpContext* context, DRAW_GDIPLUS_CACHE_FIRST_ORDER* drawGdiPlusCacheFirst)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusCacheFirst), (void*) drawGdiPlusCacheFirst, NULL);
}

void message_DrawGdiPlusCacheNext(rdpContext* context, DRAW_GDIPLUS_CACHE_NEXT_ORDER* drawGdiPlusCacheNext)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusCacheNext), (void*) drawGdiPlusCacheNext, NULL);
}

void message_DrawGdiPlusCacheEnd(rdpContext* context, DRAW_GDIPLUS_CACHE_END_ORDER* drawGdiplusCacheEnd)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusCacheEnd), (void*) drawGdiplusCacheEnd, NULL);
}

/* Window Update */

void message_WindowCreate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* windowState)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowCreate), (void*) orderInfo, (void*) windowState);
}

void message_WindowUpdate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* windowState)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowUpdate), (void*) orderInfo, (void*) windowState);
}

void message_WindowIcon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_ICON_ORDER* windowIcon)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowIcon), (void*) orderInfo, (void*) windowIcon);
}

void message_WindowCachedIcon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_CACHED_ICON_ORDER* windowCachedIcon)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowCachedIcon), (void*) orderInfo, (void*) windowCachedIcon);
}

void message_WindowDelete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowDelete), (void*) orderInfo, NULL);
}

void message_NotifyIconCreate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(WindowUpdate, NotifyIconCreate), (void*) orderInfo, (void*) notifyIconState);
}

void message_NotifyIconUpdate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(WindowUpdate, NotifyIconUpdate), (void*) orderInfo, (void*) notifyIconState);
}

void message_NotifyIconDelete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(WindowUpdate, NotifyIconDelete), (void*) orderInfo, NULL);
}

void message_MonitoredDesktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, MONITORED_DESKTOP_ORDER* monitoredDesktop)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(WindowUpdate, MonitoredDesktop), (void*) orderInfo, (void*) monitoredDesktop);
}

void message_NonMonitoredDesktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(WindowUpdate, NonMonitoredDesktop), (void*) orderInfo, NULL);
}

/* Pointer Update */

void message_PointerPosition(rdpContext* context, POINTER_POSITION_UPDATE* pointerPosition)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerPosition), (void*) pointerPosition, NULL);
}

void message_PointerSystem(rdpContext* context, POINTER_SYSTEM_UPDATE* pointerSystem)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerSystem), (void*) pointerSystem, NULL);
}

void message_PointerColor(rdpContext* context, POINTER_COLOR_UPDATE* pointerColor)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerColor), (void*) pointerColor, NULL);
}

void message_PointerNew(rdpContext* context, POINTER_NEW_UPDATE* pointerNew)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerNew), (void*) pointerNew, NULL);
}

void message_PointerCached(rdpContext* context, POINTER_CACHED_UPDATE* pointerCached)
{
	MessageQueue_Post((wMessageQueue*) context->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerCached), (void*) pointerCached, NULL);
}

/* Message Queue */


