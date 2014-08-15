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

#ifndef FREERDP_CORE_MESSAGE_PRIVATE_H
#define FREERDP_CORE_MESSAGE_PRIVATE_H

#include <freerdp/freerdp.h>
#include <freerdp/message.h>

/**
 * Update Message Queue
 */

/* Update Proxy Interface */

struct rdp_update_proxy
{
	rdpUpdate* update;

	/* Update */

	pBeginPaint BeginPaint;
	pEndPaint EndPaint;
	pSetBounds SetBounds;
	pSynchronize Synchronize;
	pDesktopResize DesktopResize;
	pBitmapUpdate BitmapUpdate;
	pPalette Palette;
	pPlaySound PlaySound;
	pRefreshRect RefreshRect;
	pSuppressOutput SuppressOutput;
	pSurfaceCommand SurfaceCommand;
	pSurfaceBits SurfaceBits;
	pSurfaceFrameMarker SurfaceFrameMarker;
	pSurfaceFrameAcknowledge SurfaceFrameAcknowledge;

	/* Primary Update */

	pDstBlt DstBlt;
	pPatBlt PatBlt;
	pScrBlt ScrBlt;
	pOpaqueRect OpaqueRect;
	pDrawNineGrid DrawNineGrid;
	pMultiDstBlt MultiDstBlt;
	pMultiPatBlt MultiPatBlt;
	pMultiScrBlt MultiScrBlt;
	pMultiOpaqueRect MultiOpaqueRect;
	pMultiDrawNineGrid MultiDrawNineGrid;
	pLineTo LineTo;
	pPolyline Polyline;
	pMemBlt MemBlt;
	pMem3Blt Mem3Blt;
	pSaveBitmap SaveBitmap;
	pGlyphIndex GlyphIndex;
	pFastIndex FastIndex;
	pFastGlyph FastGlyph;
	pPolygonSC PolygonSC;
	pPolygonCB PolygonCB;
	pEllipseSC EllipseSC;
	pEllipseCB EllipseCB;

	/* Secondary Update */

	pCacheBitmap CacheBitmap;
	pCacheBitmapV2 CacheBitmapV2;
	pCacheBitmapV3 CacheBitmapV3;
	pCacheColorTable CacheColorTable;
	pCacheGlyph CacheGlyph;
	pCacheGlyphV2 CacheGlyphV2;
	pCacheBrush CacheBrush;

	/* Alternate Secondary Update */

	pCreateOffscreenBitmap CreateOffscreenBitmap;
	pSwitchSurface SwitchSurface;
	pCreateNineGridBitmap CreateNineGridBitmap;
	pFrameMarker FrameMarker;
	pStreamBitmapFirst StreamBitmapFirst;
	pStreamBitmapNext StreamBitmapNext;
	pDrawGdiPlusFirst DrawGdiPlusFirst;
	pDrawGdiPlusNext DrawGdiPlusNext;
	pDrawGdiPlusEnd DrawGdiPlusEnd;
	pDrawGdiPlusCacheFirst DrawGdiPlusCacheFirst;
	pDrawGdiPlusCacheNext DrawGdiPlusCacheNext;
	pDrawGdiPlusCacheEnd DrawGdiPlusCacheEnd;

	/* Window Update */

	pWindowCreate WindowCreate;
	pWindowUpdate WindowUpdate;
	pWindowIcon WindowIcon;
	pWindowCachedIcon WindowCachedIcon;
	pWindowDelete WindowDelete;
	pNotifyIconCreate NotifyIconCreate;
	pNotifyIconUpdate NotifyIconUpdate;
	pNotifyIconDelete NotifyIconDelete;
	pMonitoredDesktop MonitoredDesktop;
	pNonMonitoredDesktop NonMonitoredDesktop;

	/* Pointer Update */

	pPointerPosition PointerPosition;
	pPointerSystem PointerSystem;
	pPointerColor PointerColor;
	pPointerNew PointerNew;
	pPointerCached PointerCached;

	HANDLE thread;
};

int update_message_queue_process_message(rdpUpdate* update, wMessage* message);
int update_message_queue_free_message(wMessage* message);

int update_message_queue_process_pending_messages(rdpUpdate* update);

rdpUpdateProxy* update_message_proxy_new(rdpUpdate* update);
void update_message_proxy_free(rdpUpdateProxy* message);

/**
 * Input Message Queue
 */

/* Input Proxy Interface */

struct rdp_input_proxy
{
	rdpInput* input;

	/* Input */

	pSynchronizeEvent SynchronizeEvent;
	pKeyboardEvent KeyboardEvent;
	pUnicodeKeyboardEvent UnicodeKeyboardEvent;
	pMouseEvent MouseEvent;
	pExtendedMouseEvent ExtendedMouseEvent;
};

int input_message_queue_process_message(rdpInput* input, wMessage* message);
int input_message_queue_free_message(wMessage* message);
int input_message_queue_process_pending_messages(rdpInput* input);

rdpInputProxy* input_message_proxy_new(rdpInput* input);
void input_message_proxy_free(rdpInputProxy* proxy);

#endif /* FREERDP_CORE_MESSAGE_PRIVATE_H */
