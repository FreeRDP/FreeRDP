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

}

void message_EndPaint(rdpContext* context)
{

}

void message_SetBounds(rdpContext* context, rdpBounds* bounds)
{

}

void message_Synchronize(rdpContext* context)
{

}

void message_DesktopResize(rdpContext* context)
{

}

void message_BitmapUpdate(rdpContext* context, BITMAP_UPDATE* bitmap)
{

}

void message_Palette(rdpContext* context, PALETTE_UPDATE* palette)
{

}

void message_PlaySound(rdpContext* context, PLAY_SOUND_UPDATE* playSound)
{

}

void message_RefreshRect(rdpContext* context, BYTE count, RECTANGLE_16* areas)
{

}

void message_SuppressOutput(rdpContext* context, BYTE allow, RECTANGLE_16* area)
{

}

void message_SurfaceCommand(rdpContext* context, STREAM* s)
{

}

void message_SurfaceBits(rdpContext* context, SURFACE_BITS_COMMAND* surfaceBitsCommand)
{

}

void message_SurfaceFrameMarker(rdpContext* context, SURFACE_FRAME_MARKER* surfaceFrameMarker)
{

}

void message_SurfaceFrameAcknowledge(rdpContext* context, UINT32 frameId)
{

}

/* Primary Update */

void message_DstBlt(rdpContext* context, DSTBLT_ORDER* dstBlt)
{

}

void message_PatBlt(rdpContext* context, PATBLT_ORDER* patBlt)
{

}

void message_ScrBlt(rdpContext* context, SCRBLT_ORDER* scrBlt)
{

}

void message_OpaqueRect(rdpContext* context, OPAQUE_RECT_ORDER* opaqueRect)
{

}

void message_DrawNineGrid(rdpContext* context, DRAW_NINE_GRID_ORDER* drawNineGrid)
{

}

void message_MultiDstBlt(rdpContext* context, MULTI_DSTBLT_ORDER* multiDstBlt)
{

}

void message_MultiPatBlt(rdpContext* context, MULTI_PATBLT_ORDER* multiPatBlt)
{

}

void message_MultiScrBlt(rdpContext* context, MULTI_SCRBLT_ORDER* multiScrBlt)
{

}

void message_MultiOpaqueRect(rdpContext* context, MULTI_OPAQUE_RECT_ORDER* multiOpaqueRect)
{

}

void message_MultiDrawNineGrid(rdpContext* context, MULTI_DRAW_NINE_GRID_ORDER* multiDrawNineGrid)
{

}

void message_LineTo(rdpContext* context, LINE_TO_ORDER* lineTo)
{

}

void message_Polyline(rdpContext* context, POLYLINE_ORDER* polyline)
{

}

void message_MemBlt(rdpContext* context, MEMBLT_ORDER* memBlt)
{

}

void message_Mem3Blt(rdpContext* context, MEM3BLT_ORDER* mem3Blt)
{

}

void message_SaveBitmap(rdpContext* context, SAVE_BITMAP_ORDER* saveBitmap)
{

}

void message_GlyphIndex(rdpContext* context, GLYPH_INDEX_ORDER* glyphIndex)
{

}

void message_FastIndex(rdpContext* context, FAST_INDEX_ORDER* fastIndex)
{

}

void message_FastGlyph(rdpContext* context, FAST_GLYPH_ORDER* fastGlyph)
{

}

void message_PolygonSC(rdpContext* context, POLYGON_SC_ORDER* polygonSC)
{

}

void message_PolygonCB(rdpContext* context, POLYGON_CB_ORDER* polygonCB)
{

}

void message_EllipseSC(rdpContext* context, ELLIPSE_SC_ORDER* ellipseSC)
{

}

void message_EllipseCB(rdpContext* context, ELLIPSE_CB_ORDER* ellipseCB)
{

}

/* Secondary Update */

void message_CacheBitmap(rdpContext* context, CACHE_BITMAP_ORDER* cacheBitmapOrder)
{

}

void message_CacheBitmapV2(rdpContext* context, CACHE_BITMAP_V2_ORDER* cacheBitmapV2Order)
{

}

void message_CacheBitmapV3(rdpContext* context, CACHE_BITMAP_V3_ORDER* cacheBitmapV3Order)
{

}

void message_CacheColorTable(rdpContext* context, CACHE_COLOR_TABLE_ORDER* cacheColorTableOrder)
{

}

void message_CacheGlyph(rdpContext* context, CACHE_GLYPH_ORDER* cacheGlyphOrder)
{

}

void message_CacheGlyphV2(rdpContext* context, CACHE_GLYPH_V2_ORDER* cacheGlyphV2Order)
{

}

void message_CacheBrush(rdpContext* context, CACHE_BRUSH_ORDER* cacheBrushOrder)
{

}

/* Alternate Secondary Update */

void message_CreateOffscreenBitmap(rdpContext* context, CREATE_OFFSCREEN_BITMAP_ORDER* createOffscreenBitmap)
{

}

void message_SwitchSurface(rdpContext* context, SWITCH_SURFACE_ORDER* switchSurface)
{

}

void message_CreateNineGridBitmap(rdpContext* context, CREATE_NINE_GRID_BITMAP_ORDER* createNineGridBitmap)
{

}

void message_FrameMarker(rdpContext* context, FRAME_MARKER_ORDER* frameMarker)
{

}

void message_StreamBitmapFirst(rdpContext* context, STREAM_BITMAP_FIRST_ORDER* streamBitmapFirst)
{

}

void message_StreamBitmapNext(rdpContext* context, STREAM_BITMAP_NEXT_ORDER* streamBitmapNext)
{

}

void message_DrawGdiPlusFirst(rdpContext* context, DRAW_GDIPLUS_FIRST_ORDER* drawGdiPlusFirst)
{

}

void message_DrawGdiPlusNext(rdpContext* context, DRAW_GDIPLUS_NEXT_ORDER* drawGdiPlusNext)
{

}

void message_DrawGdiPlusEnd(rdpContext* context, DRAW_GDIPLUS_END_ORDER* drawGdiPlusEnd)
{

}

void message_DrawGdiPlusCacheFirst(rdpContext* context, DRAW_GDIPLUS_CACHE_FIRST_ORDER* drawGdiPlusCacheFirst)
{

}

void message_DrawGdiPlusCacheNext(rdpContext* context, DRAW_GDIPLUS_CACHE_NEXT_ORDER* drawGdiPlusCacheNext)
{

}

void message_DrawGdiPlusCacheEnd(rdpContext* context, DRAW_GDIPLUS_CACHE_END_ORDER* drawGdiplusCacheEnd)
{

}

/* Window Update */

void message_WindowCreate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* windowState)
{

}

void message_WindowUpdate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* windowState)
{

}

void message_WindowIcon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_ICON_ORDER* windowIcon)
{

}

void message_WindowCachedIcon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_CACHED_ICON_ORDER* windowCachedIcon)
{

}

void message_WindowDelete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{

}

void message_NotifyIconCreate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
{

}

void message_NotifyIconUpdate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
{

}

void message_NotifyIconDelete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{

}

void message_MonitoredDesktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, MONITORED_DESKTOP_ORDER* monitoredDesktop)
{

}

void message_NonMonitoredDesktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{

}

/* Pointer Update */

void message_PointerPosition(rdpContext* context, POINTER_POSITION_UPDATE* pointerPosition)
{

}

void message_PointerSystem(rdpContext* context, POINTER_SYSTEM_UPDATE* pointerSystem)
{

}

void message_PointerColor(rdpContext* context, POINTER_COLOR_UPDATE* pointerColor)
{

}

void message_PointerNew(rdpContext* context, POINTER_NEW_UPDATE* pointerNew)
{

}

void message_PointerCached(rdpContext* context, POINTER_CACHED_UPDATE* pointerCached)
{

}
