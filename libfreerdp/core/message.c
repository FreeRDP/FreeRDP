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
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, BeginPaint), NULL, NULL);
}

void message_EndPaint(rdpContext* context)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, EndPaint), NULL, NULL);
}

void message_SetBounds(rdpContext* context, rdpBounds* bounds)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SetBounds), (void*) bounds, NULL);
}

void message_Synchronize(rdpContext* context)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, Synchronize), NULL, NULL);
}

void message_DesktopResize(rdpContext* context)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, DesktopResize), NULL, NULL);
}

void message_BitmapUpdate(rdpContext* context, BITMAP_UPDATE* bitmap)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, BitmapUpdate), (void*) bitmap, NULL);
}

void message_Palette(rdpContext* context, PALETTE_UPDATE* palette)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, Palette), (void*) palette, NULL);
}

void message_PlaySound(rdpContext* context, PLAY_SOUND_UPDATE* playSound)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, PlaySound), (void*) playSound, NULL);
}

void message_RefreshRect(rdpContext* context, BYTE count, RECTANGLE_16* areas)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, RefreshRect), (void*) (size_t) count, (void*) areas);
}

void message_SuppressOutput(rdpContext* context, BYTE allow, RECTANGLE_16* area)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SuppressOutput), (void*) (size_t) allow, (void*) area);
}

void message_SurfaceCommand(rdpContext* context, STREAM* s)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SurfaceCommand), (void*) s, NULL);
}

void message_SurfaceBits(rdpContext* context, SURFACE_BITS_COMMAND* surfaceBitsCommand)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SurfaceBits), (void*) surfaceBitsCommand, NULL);
}

void message_SurfaceFrameMarker(rdpContext* context, SURFACE_FRAME_MARKER* surfaceFrameMarker)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SurfaceFrameMarker), (void*) surfaceFrameMarker, NULL);
}

void message_SurfaceFrameAcknowledge(rdpContext* context, UINT32 frameId)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(Update, SurfaceFrameAcknowledge), (void*) (size_t) frameId, NULL);
}

/* Primary Update */

void message_DstBlt(rdpContext* context, DSTBLT_ORDER* dstBlt)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, DstBlt), (void*) dstBlt, NULL);
}

void message_PatBlt(rdpContext* context, PATBLT_ORDER* patBlt)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, PatBlt), (void*) patBlt, NULL);
}

void message_ScrBlt(rdpContext* context, SCRBLT_ORDER* scrBlt)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, ScrBlt), (void*) scrBlt, NULL);
}

void message_OpaqueRect(rdpContext* context, OPAQUE_RECT_ORDER* opaqueRect)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, OpaqueRect), (void*) opaqueRect, NULL);
}

void message_DrawNineGrid(rdpContext* context, DRAW_NINE_GRID_ORDER* drawNineGrid)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, DrawNineGrid), (void*) drawNineGrid, NULL);
}

void message_MultiDstBlt(rdpContext* context, MULTI_DSTBLT_ORDER* multiDstBlt)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiDstBlt), (void*) multiDstBlt, NULL);
}

void message_MultiPatBlt(rdpContext* context, MULTI_PATBLT_ORDER* multiPatBlt)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiPatBlt), (void*) multiPatBlt, NULL);
}

void message_MultiScrBlt(rdpContext* context, MULTI_SCRBLT_ORDER* multiScrBlt)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiScrBlt), (void*) multiScrBlt, NULL);
}

void message_MultiOpaqueRect(rdpContext* context, MULTI_OPAQUE_RECT_ORDER* multiOpaqueRect)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiOpaqueRect), (void*) multiOpaqueRect, NULL);
}

void message_MultiDrawNineGrid(rdpContext* context, MULTI_DRAW_NINE_GRID_ORDER* multiDrawNineGrid)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MultiDrawNineGrid), (void*) multiDrawNineGrid, NULL);
}

void message_LineTo(rdpContext* context, LINE_TO_ORDER* lineTo)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, LineTo), (void*) lineTo, NULL);
}

void message_Polyline(rdpContext* context, POLYLINE_ORDER* polyline)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, Polyline), (void*) polyline, NULL);
}

void message_MemBlt(rdpContext* context, MEMBLT_ORDER* memBlt)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, MemBlt), (void*) memBlt, NULL);
}

void message_Mem3Blt(rdpContext* context, MEM3BLT_ORDER* mem3Blt)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, Mem3Blt), (void*) mem3Blt, NULL);
}

void message_SaveBitmap(rdpContext* context, SAVE_BITMAP_ORDER* saveBitmap)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, SaveBitmap), (void*) saveBitmap, NULL);
}

void message_GlyphIndex(rdpContext* context, GLYPH_INDEX_ORDER* glyphIndex)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, GlyphIndex), (void*) glyphIndex, NULL);
}

void message_FastIndex(rdpContext* context, FAST_INDEX_ORDER* fastIndex)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, FastIndex), (void*) fastIndex, NULL);
}

void message_FastGlyph(rdpContext* context, FAST_GLYPH_ORDER* fastGlyph)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, FastGlyph), (void*) fastGlyph, NULL);
}

void message_PolygonSC(rdpContext* context, POLYGON_SC_ORDER* polygonSC)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, PolygonSC), (void*) polygonSC, NULL);
}

void message_PolygonCB(rdpContext* context, POLYGON_CB_ORDER* polygonCB)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, PolygonCB), (void*) polygonCB, NULL);
}

void message_EllipseSC(rdpContext* context, ELLIPSE_SC_ORDER* ellipseSC)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, EllipseSC), (void*) ellipseSC, NULL);
}

void message_EllipseCB(rdpContext* context, ELLIPSE_CB_ORDER* ellipseCB)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PrimaryUpdate, EllipseCB), (void*) ellipseCB, NULL);
}

/* Secondary Update */

void message_CacheBitmap(rdpContext* context, CACHE_BITMAP_ORDER* cacheBitmapOrder)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheBitmap), (void*) cacheBitmapOrder, NULL);
}

void message_CacheBitmapV2(rdpContext* context, CACHE_BITMAP_V2_ORDER* cacheBitmapV2Order)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheBitmapV2), (void*) cacheBitmapV2Order, NULL);
}

void message_CacheBitmapV3(rdpContext* context, CACHE_BITMAP_V3_ORDER* cacheBitmapV3Order)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheBitmapV3), (void*) cacheBitmapV3Order, NULL);
}

void message_CacheColorTable(rdpContext* context, CACHE_COLOR_TABLE_ORDER* cacheColorTableOrder)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheColorTable), (void*) cacheColorTableOrder, NULL);
}

void message_CacheGlyph(rdpContext* context, CACHE_GLYPH_ORDER* cacheGlyphOrder)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheGlyph), (void*) cacheGlyphOrder, NULL);
}

void message_CacheGlyphV2(rdpContext* context, CACHE_GLYPH_V2_ORDER* cacheGlyphV2Order)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheGlyphV2), (void*) cacheGlyphV2Order, NULL);
}

void message_CacheBrush(rdpContext* context, CACHE_BRUSH_ORDER* cacheBrushOrder)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(SecondaryUpdate, CacheBrush), (void*) cacheBrushOrder, NULL);
}

/* Alternate Secondary Update */

void message_CreateOffscreenBitmap(rdpContext* context, CREATE_OFFSCREEN_BITMAP_ORDER* createOffscreenBitmap)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, CreateOffscreenBitmap), (void*) createOffscreenBitmap, NULL);
}

void message_SwitchSurface(rdpContext* context, SWITCH_SURFACE_ORDER* switchSurface)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, SwitchSurface), (void*) switchSurface, NULL);
}

void message_CreateNineGridBitmap(rdpContext* context, CREATE_NINE_GRID_BITMAP_ORDER* createNineGridBitmap)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, CreateNineGridBitmap), (void*) createNineGridBitmap, NULL);
}

void message_FrameMarker(rdpContext* context, FRAME_MARKER_ORDER* frameMarker)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, FrameMarker), (void*) frameMarker, NULL);
}

void message_StreamBitmapFirst(rdpContext* context, STREAM_BITMAP_FIRST_ORDER* streamBitmapFirst)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, StreamBitmapFirst), (void*) streamBitmapFirst, NULL);
}

void message_StreamBitmapNext(rdpContext* context, STREAM_BITMAP_NEXT_ORDER* streamBitmapNext)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, StreamBitmapNext), (void*) streamBitmapNext, NULL);
}

void message_DrawGdiPlusFirst(rdpContext* context, DRAW_GDIPLUS_FIRST_ORDER* drawGdiPlusFirst)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusFirst), (void*) drawGdiPlusFirst, NULL);
}

void message_DrawGdiPlusNext(rdpContext* context, DRAW_GDIPLUS_NEXT_ORDER* drawGdiPlusNext)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusNext), (void*) drawGdiPlusNext, NULL);
}

void message_DrawGdiPlusEnd(rdpContext* context, DRAW_GDIPLUS_END_ORDER* drawGdiPlusEnd)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusEnd), (void*) drawGdiPlusEnd, NULL);
}

void message_DrawGdiPlusCacheFirst(rdpContext* context, DRAW_GDIPLUS_CACHE_FIRST_ORDER* drawGdiPlusCacheFirst)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusCacheFirst), (void*) drawGdiPlusCacheFirst, NULL);
}

void message_DrawGdiPlusCacheNext(rdpContext* context, DRAW_GDIPLUS_CACHE_NEXT_ORDER* drawGdiPlusCacheNext)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusCacheNext), (void*) drawGdiPlusCacheNext, NULL);
}

void message_DrawGdiPlusCacheEnd(rdpContext* context, DRAW_GDIPLUS_CACHE_END_ORDER* drawGdiplusCacheEnd)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(AltSecUpdate, DrawGdiPlusCacheEnd), (void*) drawGdiplusCacheEnd, NULL);
}

/* Window Update */

void message_WindowCreate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* windowState)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowCreate), (void*) orderInfo, (void*) windowState);
}

void message_WindowUpdate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* windowState)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowUpdate), (void*) orderInfo, (void*) windowState);
}

void message_WindowIcon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_ICON_ORDER* windowIcon)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowIcon), (void*) orderInfo, (void*) windowIcon);
}

void message_WindowCachedIcon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_CACHED_ICON_ORDER* windowCachedIcon)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowCachedIcon), (void*) orderInfo, (void*) windowCachedIcon);
}

void message_WindowDelete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, WindowDelete), (void*) orderInfo, NULL);
}

void message_NotifyIconCreate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, NotifyIconCreate), (void*) orderInfo, (void*) notifyIconState);
}

void message_NotifyIconUpdate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, NotifyIconUpdate), (void*) orderInfo, (void*) notifyIconState);
}

void message_NotifyIconDelete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, NotifyIconDelete), (void*) orderInfo, NULL);
}

void message_MonitoredDesktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, MONITORED_DESKTOP_ORDER* monitoredDesktop)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, MonitoredDesktop), (void*) orderInfo, (void*) monitoredDesktop);
}

void message_NonMonitoredDesktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(WindowUpdate, NonMonitoredDesktop), (void*) orderInfo, NULL);
}

/* Pointer Update */

void message_PointerPosition(rdpContext* context, POINTER_POSITION_UPDATE* pointerPosition)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerPosition), (void*) pointerPosition, NULL);
}

void message_PointerSystem(rdpContext* context, POINTER_SYSTEM_UPDATE* pointerSystem)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerSystem), (void*) pointerSystem, NULL);
}

void message_PointerColor(rdpContext* context, POINTER_COLOR_UPDATE* pointerColor)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerColor), (void*) pointerColor, NULL);
}

void message_PointerNew(rdpContext* context, POINTER_NEW_UPDATE* pointerNew)
{
	MessageQueue_Post(context->update->queue, (void*) context,
			MakeMessageId(PointerUpdate, PointerNew), (void*) pointerNew, NULL);
}

void message_PointerCached(rdpContext* context, POINTER_CACHED_UPDATE* pointerCached)
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

	update->BeginPaint = message->BeginPaint;
	update->EndPaint = message->EndPaint;
	update->SetBounds = message->SetBounds;
	update->Synchronize = message->Synchronize;
	update->DesktopResize = message->DesktopResize;
	update->BitmapUpdate = message->BitmapUpdate;
	update->Palette = message->Palette;
	update->PlaySound = message->PlaySound;
	update->RefreshRect = message->RefreshRect;
	update->SuppressOutput = message->SuppressOutput;
	update->SurfaceCommand = message->SurfaceCommand;
	update->SurfaceBits = message->SurfaceBits;
	update->SurfaceFrameMarker = message->SurfaceFrameMarker;
	update->SurfaceFrameAcknowledge = message->SurfaceFrameAcknowledge;

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

	primary->DstBlt = message->DstBlt;
	primary->PatBlt = message->PatBlt;
	primary->ScrBlt = message->ScrBlt;
	primary->OpaqueRect = message->OpaqueRect;
	primary->DrawNineGrid = message->DrawNineGrid;
	primary->MultiDstBlt = message->MultiDstBlt;
	primary->MultiPatBlt = message->MultiPatBlt;
	primary->MultiScrBlt = message->MultiScrBlt;
	primary->MultiOpaqueRect = message->MultiOpaqueRect;
	primary->MultiDrawNineGrid = message->MultiDrawNineGrid;
	primary->LineTo = message->LineTo;
	primary->Polyline = message->Polyline;
	primary->MemBlt = message->MemBlt;
	primary->Mem3Blt = message->Mem3Blt;
	primary->SaveBitmap = message->SaveBitmap;
	primary->GlyphIndex = message->GlyphIndex;
	primary->FastIndex = message->FastIndex;
	primary->FastGlyph = message->FastGlyph;
	primary->PolygonSC = message->PolygonSC;
	primary->PolygonCB = message->PolygonCB;
	primary->EllipseSC = message->EllipseSC;
	primary->EllipseCB = message->EllipseCB;

	/* Secondary Update */

	message->CacheBitmap = secondary->CacheBitmap;
	message->CacheBitmapV2 = secondary->CacheBitmapV2;
	message->CacheBitmapV3 = secondary->CacheBitmapV3;
	message->CacheColorTable = secondary->CacheColorTable;
	message->CacheGlyph = secondary->CacheGlyph;
	message->CacheGlyphV2 = secondary->CacheGlyphV2;
	message->CacheBrush = secondary->CacheBrush;

	secondary->CacheBitmap = message->CacheBitmap;
	secondary->CacheBitmapV2 = message->CacheBitmapV2;
	secondary->CacheBitmapV3 = message->CacheBitmapV3;
	secondary->CacheColorTable = message->CacheColorTable;
	secondary->CacheGlyph = message->CacheGlyph;
	secondary->CacheGlyphV2 = message->CacheGlyphV2;
	secondary->CacheBrush = message->CacheBrush;

	/* Alternate Secondary Update */

	message->CreateOffscreenBitmap = altsec->CreateOffscreenBitmap;
	message->SwitchSurface = altsec->SwitchSurface;
	message->CreateNineGridBitmap = altsec->CreateNineGridBitmap;
	message->FrameMarker = altsec->FrameMarker;
	message->StreamBitmapFirst = altsec->StreamBitmapFirst;
	message->DrawGdiPlusFirst = altsec->DrawGdiPlusFirst;
	message->DrawGdiPlusNext = altsec->DrawGdiPlusNext;
	message->DrawGdiPlusEnd = altsec->DrawGdiPlusEnd;
	message->DrawGdiPlusCacheFirst = altsec->DrawGdiPlusCacheFirst;
	message->DrawGdiPlusCacheNext = altsec->DrawGdiPlusCacheNext;
	message->DrawGdiPlusCacheEnd = altsec->DrawGdiPlusCacheEnd;

	altsec->CreateOffscreenBitmap = message->CreateOffscreenBitmap;
	altsec->SwitchSurface = message->SwitchSurface;
	altsec->CreateNineGridBitmap = message->CreateNineGridBitmap;
	altsec->FrameMarker = message->FrameMarker;
	altsec->StreamBitmapFirst = message->StreamBitmapFirst;
	altsec->DrawGdiPlusFirst = message->DrawGdiPlusFirst;
	altsec->DrawGdiPlusNext = message->DrawGdiPlusNext;
	altsec->DrawGdiPlusEnd = message->DrawGdiPlusEnd;
	altsec->DrawGdiPlusCacheFirst = message->DrawGdiPlusCacheFirst;
	altsec->DrawGdiPlusCacheNext = message->DrawGdiPlusCacheNext;
	altsec->DrawGdiPlusCacheEnd = message->DrawGdiPlusCacheEnd;

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

	window->WindowCreate = message->WindowCreate;
	window->WindowUpdate = message->WindowUpdate;
	window->WindowIcon = message->WindowIcon;
	window->WindowCachedIcon = message->WindowCachedIcon;
	window->WindowDelete = message->WindowDelete;
	window->NotifyIconCreate = message->NotifyIconCreate;
	window->NotifyIconUpdate = message->NotifyIconUpdate;
	window->NotifyIconDelete = message->NotifyIconDelete;
	window->MonitoredDesktop = message->MonitoredDesktop;
	window->NonMonitoredDesktop = message->NonMonitoredDesktop;

	/* Pointer Update */

	message->PointerPosition = pointer->PointerPosition;
	message->PointerSystem = pointer->PointerSystem;
	message->PointerColor = pointer->PointerColor;
	message->PointerNew = pointer->PointerNew;
	message->PointerCached = pointer->PointerCached;

	pointer->PointerPosition = message->PointerPosition;
	pointer->PointerSystem = message->PointerSystem;
	pointer->PointerColor = message->PointerColor;
	pointer->PointerNew = message->PointerNew;
	pointer->PointerCached = message->PointerCached;
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
