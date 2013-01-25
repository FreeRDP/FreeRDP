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
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SetBounds), (void*) bounds, NULL);
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
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, BitmapUpdate), (void*) bitmap, NULL);
}

static void message_Palette(rdpContext* context, PALETTE_UPDATE* palette)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, Palette), (void*) palette, NULL);
}

static void message_PlaySound(rdpContext* context, PLAY_SOUND_UPDATE* playSound)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, PlaySound), (void*) playSound, NULL);
}

static void message_RefreshRect(rdpContext* context, BYTE count, RECTANGLE_16* areas)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, RefreshRect), (void*) (size_t) count, (void*) areas);
}

static void message_SuppressOutput(rdpContext* context, BYTE allow, RECTANGLE_16* area)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SuppressOutput), (void*) (size_t) allow, (void*) area);
}

static void message_SurfaceCommand(rdpContext* context, STREAM* s)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SurfaceCommand), (void*) s, NULL);
}

static void message_SurfaceBits(rdpContext* context, SURFACE_BITS_COMMAND* surfaceBitsCommand)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SurfaceBits), (void*) surfaceBitsCommand, NULL);
}

static void message_SurfaceFrameMarker(rdpContext* context, SURFACE_FRAME_MARKER* surfaceFrameMarker)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SurfaceFrameMarker), (void*) surfaceFrameMarker, NULL);
}

static void message_SurfaceFrameAcknowledge(rdpContext* context, UINT32 frameId)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SurfaceFrameAcknowledge), (void*) (size_t) frameId, NULL);
}

/* Primary Update */

static void message_DstBlt(rdpContext* context, DSTBLT_ORDER* dstBlt)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, DstBlt), (void*) dstBlt, NULL);
}

static void message_PatBlt(rdpContext* context, PATBLT_ORDER* patBlt)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, PatBlt), (void*) patBlt, NULL);
}

static void message_ScrBlt(rdpContext* context, SCRBLT_ORDER* scrBlt)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, ScrBlt), (void*) scrBlt, NULL);
}

static void message_OpaqueRect(rdpContext* context, OPAQUE_RECT_ORDER* opaqueRect)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, OpaqueRect), (void*) opaqueRect, NULL);
}

static void message_DrawNineGrid(rdpContext* context, DRAW_NINE_GRID_ORDER* drawNineGrid)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, DrawNineGrid), (void*) drawNineGrid, NULL);
}

static void message_MultiDstBlt(rdpContext* context, MULTI_DSTBLT_ORDER* multiDstBlt)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiDstBlt), (void*) multiDstBlt, NULL);
}

static void message_MultiPatBlt(rdpContext* context, MULTI_PATBLT_ORDER* multiPatBlt)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiPatBlt), (void*) multiPatBlt, NULL);
}

static void message_MultiScrBlt(rdpContext* context, MULTI_SCRBLT_ORDER* multiScrBlt)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiScrBlt), (void*) multiScrBlt, NULL);
}

static void message_MultiOpaqueRect(rdpContext* context, MULTI_OPAQUE_RECT_ORDER* multiOpaqueRect)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiOpaqueRect), (void*) multiOpaqueRect, NULL);
}

static void message_MultiDrawNineGrid(rdpContext* context, MULTI_DRAW_NINE_GRID_ORDER* multiDrawNineGrid)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiDrawNineGrid), (void*) multiDrawNineGrid, NULL);
}

static void message_LineTo(rdpContext* context, LINE_TO_ORDER* lineTo)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, LineTo), (void*) lineTo, NULL);
}

static void message_Polyline(rdpContext* context, POLYLINE_ORDER* polyline)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, Polyline), (void*) polyline, NULL);
}

static void message_MemBlt(rdpContext* context, MEMBLT_ORDER* memBlt)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MemBlt), (void*) memBlt, NULL);
}

static void message_Mem3Blt(rdpContext* context, MEM3BLT_ORDER* mem3Blt)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, Mem3Blt), (void*) mem3Blt, NULL);
}

static void message_SaveBitmap(rdpContext* context, SAVE_BITMAP_ORDER* saveBitmap)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, SaveBitmap), (void*) saveBitmap, NULL);
}

static void message_GlyphIndex(rdpContext* context, GLYPH_INDEX_ORDER* glyphIndex)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, GlyphIndex), (void*) glyphIndex, NULL);
}

static void message_FastIndex(rdpContext* context, FAST_INDEX_ORDER* fastIndex)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, FastIndex), (void*) fastIndex, NULL);
}

static void message_FastGlyph(rdpContext* context, FAST_GLYPH_ORDER* fastGlyph)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, FastGlyph), (void*) fastGlyph, NULL);
}

static void message_PolygonSC(rdpContext* context, POLYGON_SC_ORDER* polygonSC)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, PolygonSC), (void*) polygonSC, NULL);
}

static void message_PolygonCB(rdpContext* context, POLYGON_CB_ORDER* polygonCB)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, PolygonCB), (void*) polygonCB, NULL);
}

static void message_EllipseSC(rdpContext* context, ELLIPSE_SC_ORDER* ellipseSC)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, EllipseSC), (void*) ellipseSC, NULL);
}

static void message_EllipseCB(rdpContext* context, ELLIPSE_CB_ORDER* ellipseCB)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, EllipseCB), (void*) ellipseCB, NULL);
}

/* Secondary Update */

static void message_CacheBitmap(rdpContext* context, CACHE_BITMAP_ORDER* cacheBitmapOrder)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheBitmap), (void*) cacheBitmapOrder, NULL);
}

static void message_CacheBitmapV2(rdpContext* context, CACHE_BITMAP_V2_ORDER* cacheBitmapV2Order)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheBitmapV2), (void*) cacheBitmapV2Order, NULL);
}

static void message_CacheBitmapV3(rdpContext* context, CACHE_BITMAP_V3_ORDER* cacheBitmapV3Order)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheBitmapV3), (void*) cacheBitmapV3Order, NULL);
}

static void message_CacheColorTable(rdpContext* context, CACHE_COLOR_TABLE_ORDER* cacheColorTableOrder)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheColorTable), (void*) cacheColorTableOrder, NULL);
}

static void message_CacheGlyph(rdpContext* context, CACHE_GLYPH_ORDER* cacheGlyphOrder)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheGlyph), (void*) cacheGlyphOrder, NULL);
}

static void message_CacheGlyphV2(rdpContext* context, CACHE_GLYPH_V2_ORDER* cacheGlyphV2Order)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheGlyphV2), (void*) cacheGlyphV2Order, NULL);
}

static void message_CacheBrush(rdpContext* context, CACHE_BRUSH_ORDER* cacheBrushOrder)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheBrush), (void*) cacheBrushOrder, NULL);
}

/* Alternate Secondary Update */

static void message_CreateOffscreenBitmap(rdpContext* context, CREATE_OFFSCREEN_BITMAP_ORDER* createOffscreenBitmap)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, CreateOffscreenBitmap), (void*) createOffscreenBitmap, NULL);
}

static void message_SwitchSurface(rdpContext* context, SWITCH_SURFACE_ORDER* switchSurface)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, SwitchSurface), (void*) switchSurface, NULL);
}

static void message_CreateNineGridBitmap(rdpContext* context, CREATE_NINE_GRID_BITMAP_ORDER* createNineGridBitmap)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, CreateNineGridBitmap), (void*) createNineGridBitmap, NULL);
}

static void message_FrameMarker(rdpContext* context, FRAME_MARKER_ORDER* frameMarker)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, FrameMarker), (void*) frameMarker, NULL);
}

static void message_StreamBitmapFirst(rdpContext* context, STREAM_BITMAP_FIRST_ORDER* streamBitmapFirst)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, StreamBitmapFirst), (void*) streamBitmapFirst, NULL);
}

static void message_StreamBitmapNext(rdpContext* context, STREAM_BITMAP_NEXT_ORDER* streamBitmapNext)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, StreamBitmapNext), (void*) streamBitmapNext, NULL);
}

static void message_DrawGdiPlusFirst(rdpContext* context, DRAW_GDIPLUS_FIRST_ORDER* drawGdiPlusFirst)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusFirst), (void*) drawGdiPlusFirst, NULL);
}

static void message_DrawGdiPlusNext(rdpContext* context, DRAW_GDIPLUS_NEXT_ORDER* drawGdiPlusNext)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusNext), (void*) drawGdiPlusNext, NULL);
}

static void message_DrawGdiPlusEnd(rdpContext* context, DRAW_GDIPLUS_END_ORDER* drawGdiPlusEnd)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusEnd), (void*) drawGdiPlusEnd, NULL);
}

static void message_DrawGdiPlusCacheFirst(rdpContext* context, DRAW_GDIPLUS_CACHE_FIRST_ORDER* drawGdiPlusCacheFirst)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusCacheFirst), (void*) drawGdiPlusCacheFirst, NULL);
}

static void message_DrawGdiPlusCacheNext(rdpContext* context, DRAW_GDIPLUS_CACHE_NEXT_ORDER* drawGdiPlusCacheNext)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusCacheNext), (void*) drawGdiPlusCacheNext, NULL);
}

static void message_DrawGdiPlusCacheEnd(rdpContext* context, DRAW_GDIPLUS_CACHE_END_ORDER* drawGdiplusCacheEnd)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusCacheEnd), (void*) drawGdiplusCacheEnd, NULL);
}

/* Window Update */

static void message_WindowCreate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* windowState)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowCreate), (void*) orderInfo, (void*) windowState);
}

static void message_WindowUpdate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* windowState)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowUpdate), (void*) orderInfo, (void*) windowState);
}

static void message_WindowIcon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_ICON_ORDER* windowIcon)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowIcon), (void*) orderInfo, (void*) windowIcon);
}

static void message_WindowCachedIcon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_CACHED_ICON_ORDER* windowCachedIcon)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowCachedIcon), (void*) orderInfo, (void*) windowCachedIcon);
}

static void message_WindowDelete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowDelete), (void*) orderInfo, NULL);
}

static void message_NotifyIconCreate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, NotifyIconCreate), (void*) orderInfo, (void*) notifyIconState);
}

static void message_NotifyIconUpdate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, NotifyIconUpdate), (void*) orderInfo, (void*) notifyIconState);
}

static void message_NotifyIconDelete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, NotifyIconDelete), (void*) orderInfo, NULL);
}

static void message_MonitoredDesktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, MONITORED_DESKTOP_ORDER* monitoredDesktop)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, MonitoredDesktop), (void*) orderInfo, (void*) monitoredDesktop);
}

static void message_NonMonitoredDesktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, NonMonitoredDesktop), (void*) orderInfo, NULL);
}

/* Pointer Update */

static void message_PointerPosition(rdpContext* context, POINTER_POSITION_UPDATE* pointerPosition)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerPosition), (void*) pointerPosition, NULL);
}

static void message_PointerSystem(rdpContext* context, POINTER_SYSTEM_UPDATE* pointerSystem)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerSystem), (void*) pointerSystem, NULL);
}

static void message_PointerColor(rdpContext* context, POINTER_COLOR_UPDATE* pointerColor)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerColor), (void*) pointerColor, NULL);
}

static void message_PointerNew(rdpContext* context, POINTER_NEW_UPDATE* pointerNew)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerNew), (void*) pointerNew, NULL);
}

static void message_PointerCached(rdpContext* context, POINTER_CACHED_UPDATE* pointerCached)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerCached), (void*) pointerCached, NULL);
}

/* Message Queue */

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
			break;

		case Update_Synchronize:
			IFCALL(update->Synchronize, msg->context);
			break;

		case Update_DesktopResize:
			IFCALL(update->DesktopResize, msg->context);
			break;

		case Update_BitmapUpdate:
			IFCALL(update->BitmapUpdate, msg->context, (BITMAP_UPDATE*) msg->wParam);
			break;

		case Update_Palette:
			IFCALL(update->Palette, msg->context, (PALETTE_UPDATE*) msg->wParam);
			break;

		case Update_PlaySound:
			IFCALL(update->PlaySound, msg->context, (PLAY_SOUND_UPDATE*) msg->wParam);
			break;

		case Update_RefreshRect:
			IFCALL(update->RefreshRect, msg->context,
					(BYTE) (size_t) msg->wParam, (RECTANGLE_16*) msg->lParam);
			break;

		case Update_SuppressOutput:
			IFCALL(update->SuppressOutput, msg->context,
					(BYTE) (size_t) msg->wParam, (RECTANGLE_16*) msg->lParam);
			break;

		case Update_SurfaceCommand:
			IFCALL(update->SurfaceCommand, msg->context, (STREAM*) msg->wParam);
			break;

		case Update_SurfaceBits:
			IFCALL(update->SurfaceBits, msg->context, (SURFACE_BITS_COMMAND*) msg->wParam);
			break;

		case Update_SurfaceFrameMarker:
			IFCALL(update->SurfaceFrameMarker, msg->context, (SURFACE_FRAME_MARKER*) msg->wParam);
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
			break;

		case PrimaryUpdate_PatBlt:
			IFCALL(update->PatBlt, msg->context, (PATBLT_ORDER*) msg->wParam);
			break;

		case PrimaryUpdate_ScrBlt:
			IFCALL(update->ScrBlt, msg->context, (SCRBLT_ORDER*) msg->wParam);
			break;

		case PrimaryUpdate_OpaqueRect:
			IFCALL(update->OpaqueRect, msg->context, (OPAQUE_RECT_ORDER*) msg->wParam);
			break;

		case PrimaryUpdate_DrawNineGrid:
			IFCALL(update->DrawNineGrid, msg->context, (DRAW_NINE_GRID_ORDER*) msg->wParam);
			break;

		case PrimaryUpdate_MultiDstBlt:
			IFCALL(update->MultiDstBlt, msg->context, (MULTI_DSTBLT_ORDER*) msg->wParam);
			break;

		case PrimaryUpdate_MultiPatBlt:
			IFCALL(update->MultiPatBlt, msg->context, (MULTI_PATBLT_ORDER*) msg->wParam);
			break;

		case PrimaryUpdate_MultiScrBlt:
			IFCALL(update->MultiScrBlt, msg->context, (MULTI_SCRBLT_ORDER*) msg->wParam);
			break;

		case PrimaryUpdate_MultiOpaqueRect:
			IFCALL(update->MultiOpaqueRect, msg->context, (MULTI_OPAQUE_RECT_ORDER*) msg->wParam);
			break;

		case PrimaryUpdate_MultiDrawNineGrid:
			IFCALL(update->MultiDrawNineGrid, msg->context, (MULTI_DRAW_NINE_GRID_ORDER*) msg->wParam);
			break;

		case PrimaryUpdate_LineTo:
			IFCALL(update->LineTo, msg->context, (LINE_TO_ORDER*) msg->wParam);
			break;

		case PrimaryUpdate_Polyline:
			IFCALL(update->Polyline, msg->context, (POLYLINE_ORDER*) msg->wParam);
			break;

		case PrimaryUpdate_MemBlt:
			IFCALL(update->MemBlt, msg->context, (MEMBLT_ORDER*) msg->wParam);
			break;

		case PrimaryUpdate_Mem3Blt:
			IFCALL(update->Mem3Blt, msg->context, (MEM3BLT_ORDER*) msg->wParam);
			break;

		case PrimaryUpdate_SaveBitmap:
			IFCALL(update->SaveBitmap, msg->context, (SAVE_BITMAP_ORDER*) msg->wParam);
			break;

		case PrimaryUpdate_GlyphIndex:
			IFCALL(update->GlyphIndex, msg->context, (GLYPH_INDEX_ORDER*) msg->wParam);
			break;

		case PrimaryUpdate_FastIndex:
			IFCALL(update->FastIndex, msg->context, (FAST_INDEX_ORDER*) msg->wParam);
			break;

		case PrimaryUpdate_FastGlyph:
			IFCALL(update->FastGlyph, msg->context, (FAST_GLYPH_ORDER*) msg->wParam);
			break;

		case PrimaryUpdate_PolygonSC:
			IFCALL(update->PolygonSC, msg->context, (POLYGON_SC_ORDER*) msg->wParam);
			break;

		case PrimaryUpdate_PolygonCB:
			IFCALL(update->PolygonCB, msg->context, (POLYGON_CB_ORDER*) msg->wParam);
			break;

		case PrimaryUpdate_EllipseSC:
			IFCALL(update->EllipseSC, msg->context, (ELLIPSE_SC_ORDER*) msg->wParam);
			break;

		case PrimaryUpdate_EllipseCB:
			IFCALL(update->EllipseCB, msg->context, (ELLIPSE_CB_ORDER*) msg->wParam);
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
			break;

		case SecondaryUpdate_CacheBitmapV2:
			IFCALL(update->CacheBitmapV2, msg->context, (CACHE_BITMAP_V2_ORDER*) msg->wParam);
			break;

		case SecondaryUpdate_CacheBitmapV3:
			IFCALL(update->CacheBitmapV3, msg->context, (CACHE_BITMAP_V3_ORDER*) msg->wParam);
			break;

		case SecondaryUpdate_CacheColorTable:
			IFCALL(update->CacheColorTable, msg->context, (CACHE_COLOR_TABLE_ORDER*) msg->wParam);
			break;

		case SecondaryUpdate_CacheGlyph:
			IFCALL(update->CacheGlyph, msg->context, (CACHE_GLYPH_ORDER*) msg->wParam);
			break;

		case SecondaryUpdate_CacheGlyphV2:
			IFCALL(update->CacheGlyphV2, msg->context, (CACHE_GLYPH_V2_ORDER*) msg->wParam);
			break;

		case SecondaryUpdate_CacheBrush:
			IFCALL(update->CacheBrush, msg->context, (CACHE_BRUSH_ORDER*) msg->wParam);
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
			break;

		case AltSecUpdate_SwitchSurface:
			IFCALL(update->SwitchSurface, msg->context, (SWITCH_SURFACE_ORDER*) msg->wParam);
			break;

		case AltSecUpdate_CreateNineGridBitmap:
			IFCALL(update->CreateNineGridBitmap, msg->context, (CREATE_NINE_GRID_BITMAP_ORDER*) msg->wParam);
			break;

		case AltSecUpdate_FrameMarker:
			IFCALL(update->FrameMarker, msg->context, (FRAME_MARKER_ORDER*) msg->wParam);
			break;

		case AltSecUpdate_StreamBitmapFirst:
			IFCALL(update->StreamBitmapFirst, msg->context, (STREAM_BITMAP_FIRST_ORDER*) msg->wParam);
			break;

		case AltSecUpdate_StreamBitmapNext:
			IFCALL(update->StreamBitmapNext, msg->context, (STREAM_BITMAP_NEXT_ORDER*) msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusFirst:
			IFCALL(update->DrawGdiPlusFirst, msg->context, (DRAW_GDIPLUS_FIRST_ORDER*) msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusNext:
			IFCALL(update->DrawGdiPlusNext, msg->context, (DRAW_GDIPLUS_NEXT_ORDER*) msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusEnd:
			IFCALL(update->DrawGdiPlusEnd, msg->context, (DRAW_GDIPLUS_END_ORDER*) msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusCacheFirst:
			IFCALL(update->DrawGdiPlusCacheFirst, msg->context, (DRAW_GDIPLUS_CACHE_FIRST_ORDER*) msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusCacheNext:
			IFCALL(update->DrawGdiPlusCacheNext, msg->context, (DRAW_GDIPLUS_CACHE_NEXT_ORDER*) msg->wParam);
			break;

		case AltSecUpdate_DrawGdiPlusCacheEnd:
			IFCALL(update->DrawGdiPlusCacheEnd, msg->context, (DRAW_GDIPLUS_CACHE_END_ORDER*) msg->wParam);
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
			break;

		case WindowUpdate_WindowUpdate:
			IFCALL(update->WindowCreate, msg->context, (WINDOW_ORDER_INFO*) msg->wParam,
					(WINDOW_STATE_ORDER*) msg->lParam);
			break;

		case WindowUpdate_WindowIcon:
			IFCALL(update->WindowIcon, msg->context, (WINDOW_ORDER_INFO*) msg->wParam,
					(WINDOW_ICON_ORDER*) msg->lParam);
			break;

		case WindowUpdate_WindowCachedIcon:
			IFCALL(update->WindowCachedIcon, msg->context, (WINDOW_ORDER_INFO*) msg->wParam,
					(WINDOW_CACHED_ICON_ORDER*) msg->lParam);
			break;

		case WindowUpdate_WindowDelete:
			IFCALL(update->WindowDelete, msg->context, (WINDOW_ORDER_INFO*) msg->wParam);
			break;

		case WindowUpdate_NotifyIconCreate:
			IFCALL(update->NotifyIconCreate, msg->context, (WINDOW_ORDER_INFO*) msg->wParam,
					(NOTIFY_ICON_STATE_ORDER*) msg->lParam);
			break;

		case WindowUpdate_NotifyIconUpdate:
			IFCALL(update->NotifyIconUpdate, msg->context, (WINDOW_ORDER_INFO*) msg->wParam,
					(NOTIFY_ICON_STATE_ORDER*) msg->lParam);
			break;

		case WindowUpdate_NotifyIconDelete:
			IFCALL(update->NotifyIconDelete, msg->context, (WINDOW_ORDER_INFO*) msg->wParam);
			break;

		case WindowUpdate_MonitoredDesktop:
			IFCALL(update->MonitoredDesktop, msg->context, (WINDOW_ORDER_INFO*) msg->wParam,
					(MONITORED_DESKTOP_ORDER*) msg->lParam);
			break;

		case WindowUpdate_NonMonitoredDesktop:
			IFCALL(update->NonMonitoredDesktop, msg->context, (WINDOW_ORDER_INFO*) msg->wParam);
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
			break;

		case PointerUpdate_PointerSystem:
			IFCALL(update->PointerSystem, msg->context, (POINTER_SYSTEM_UPDATE*) msg->wParam);
			break;

		case PointerUpdate_PointerColor:
			IFCALL(update->PointerColor, msg->context, (POINTER_COLOR_UPDATE*) msg->wParam);
			break;

		case PointerUpdate_PointerNew:
			IFCALL(update->PointerNew, msg->context, (POINTER_NEW_UPDATE*) msg->wParam);
			break;

		case PointerUpdate_PointerCached:
			IFCALL(update->PointerCached, msg->context, (POINTER_CACHED_UPDATE*) msg->wParam);
			break;

		default:
			status = -1;
			break;
	}

	return status;
}

void* message_update_thread(void* arg)
{
	int status;
	int msgClass;
	int msgType;
	wMessage message;
	wMessageQueue* queue;
	rdpUpdate* update = (rdpUpdate*) arg;

	queue = update->queue;

	while (MessageQueue_Wait(queue))
	{
		status = MessageQueue_Peek(queue, &message, TRUE);

		if (!status)
			continue;

		if (message.type == WMQ_QUIT)
			break;

		status = -1;
		msgClass = GetMessageClass(message.type);
		msgType = GetMessageType(message.type);

		printf("Message Class: %d Type: %d\n", msgClass, msgType);

		switch (msgClass)
		{
			case Update_Class:
				status = message_process_update_class(update->message, &message, msgType);
				break;

			case PrimaryUpdate_Class:
				status = message_process_primary_update_class(update->message, &message, msgType);
				break;

			case SecondaryUpdate_Class:
				status = message_process_secondary_update_class(update->message, &message, msgType);
				break;

			case AltSecUpdate_Class:
				status = message_process_altsec_update_class(update->message, &message, msgType);
				break;

			case WindowUpdate_Class:
				status = message_process_window_update_class(update->message, &message, msgType);
				break;

			case PointerUpdate_Class:
				status = message_process_pointer_update_class(update->message, &message, msgType);
				break;
		}

		if (status < 0)
			printf("Unknown message: class: %d type: %d\n", msgClass, msgType);
	}

	return NULL;
}

rdpMessage* message_new()
{
	rdpMessage* message;

	message = (rdpMessage*) malloc(sizeof(rdpMessage));

	return message;
}

void message_free(rdpMessage* message)
{
	if (message)
	{
		free(message);
	}
}
