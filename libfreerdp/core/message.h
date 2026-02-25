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

#ifndef FREERDP_LIB_CORE_MESSAGE_H
#define FREERDP_LIB_CORE_MESSAGE_H

#include <freerdp/freerdp.h>
#include <freerdp/message.h>
#include <freerdp/api.h>

/**
 * Update Message Queue
 */

/* Update Proxy Interface */

struct rdp_update_proxy
{
	rdpUpdate* update;

	/* Update */

	WINPR_ATTR_NODISCARD pBeginPaint BeginPaint;
	WINPR_ATTR_NODISCARD pEndPaint EndPaint;
	WINPR_ATTR_NODISCARD pSetBounds SetBounds;
	WINPR_ATTR_NODISCARD pSynchronize Synchronize;
	WINPR_ATTR_NODISCARD pDesktopResize DesktopResize;
	WINPR_ATTR_NODISCARD pBitmapUpdate BitmapUpdate;
	WINPR_ATTR_NODISCARD pPalette Palette;
	WINPR_ATTR_NODISCARD pPlaySound PlaySound;
	WINPR_ATTR_NODISCARD pSetKeyboardIndicators SetKeyboardIndicators;
	WINPR_ATTR_NODISCARD pSetKeyboardImeStatus SetKeyboardImeStatus;
	WINPR_ATTR_NODISCARD pRefreshRect RefreshRect;
	WINPR_ATTR_NODISCARD pSuppressOutput SuppressOutput;
	WINPR_ATTR_NODISCARD pSurfaceCommand SurfaceCommand;
	WINPR_ATTR_NODISCARD pSurfaceBits SurfaceBits;
	WINPR_ATTR_NODISCARD pSurfaceFrameMarker SurfaceFrameMarker;
	WINPR_ATTR_NODISCARD pSurfaceFrameAcknowledge SurfaceFrameAcknowledge;

	/* Primary Update */

	WINPR_ATTR_NODISCARD pDstBlt DstBlt;
	WINPR_ATTR_NODISCARD pPatBlt PatBlt;
	WINPR_ATTR_NODISCARD pScrBlt ScrBlt;
	WINPR_ATTR_NODISCARD pOpaqueRect OpaqueRect;
	WINPR_ATTR_NODISCARD pDrawNineGrid DrawNineGrid;
	WINPR_ATTR_NODISCARD pMultiDstBlt MultiDstBlt;
	WINPR_ATTR_NODISCARD pMultiPatBlt MultiPatBlt;
	WINPR_ATTR_NODISCARD pMultiScrBlt MultiScrBlt;
	WINPR_ATTR_NODISCARD pMultiOpaqueRect MultiOpaqueRect;
	WINPR_ATTR_NODISCARD pMultiDrawNineGrid MultiDrawNineGrid;
	WINPR_ATTR_NODISCARD pLineTo LineTo;
	WINPR_ATTR_NODISCARD pPolyline Polyline;
	WINPR_ATTR_NODISCARD pMemBlt MemBlt;
	WINPR_ATTR_NODISCARD pMem3Blt Mem3Blt;
	WINPR_ATTR_NODISCARD pSaveBitmap SaveBitmap;
	WINPR_ATTR_NODISCARD pGlyphIndex GlyphIndex;
	WINPR_ATTR_NODISCARD pFastIndex FastIndex;
	WINPR_ATTR_NODISCARD pFastGlyph FastGlyph;
	WINPR_ATTR_NODISCARD pPolygonSC PolygonSC;
	WINPR_ATTR_NODISCARD pPolygonCB PolygonCB;
	WINPR_ATTR_NODISCARD pEllipseSC EllipseSC;
	WINPR_ATTR_NODISCARD pEllipseCB EllipseCB;

	/* Secondary Update */

	WINPR_ATTR_NODISCARD pCacheBitmap CacheBitmap;
	WINPR_ATTR_NODISCARD pCacheBitmapV2 CacheBitmapV2;
	WINPR_ATTR_NODISCARD pCacheBitmapV3 CacheBitmapV3;
	WINPR_ATTR_NODISCARD pCacheColorTable CacheColorTable;
	WINPR_ATTR_NODISCARD pCacheGlyph CacheGlyph;
	WINPR_ATTR_NODISCARD pCacheGlyphV2 CacheGlyphV2;
	WINPR_ATTR_NODISCARD pCacheBrush CacheBrush;

	/* Alternate Secondary Update */

	WINPR_ATTR_NODISCARD pCreateOffscreenBitmap CreateOffscreenBitmap;
	WINPR_ATTR_NODISCARD pSwitchSurface SwitchSurface;
	WINPR_ATTR_NODISCARD pCreateNineGridBitmap CreateNineGridBitmap;
	WINPR_ATTR_NODISCARD pFrameMarker FrameMarker;
	WINPR_ATTR_NODISCARD pStreamBitmapFirst StreamBitmapFirst;
	WINPR_ATTR_NODISCARD pStreamBitmapNext StreamBitmapNext;
	WINPR_ATTR_NODISCARD pDrawGdiPlusFirst DrawGdiPlusFirst;
	WINPR_ATTR_NODISCARD pDrawGdiPlusNext DrawGdiPlusNext;
	WINPR_ATTR_NODISCARD pDrawGdiPlusEnd DrawGdiPlusEnd;
	WINPR_ATTR_NODISCARD pDrawGdiPlusCacheFirst DrawGdiPlusCacheFirst;
	WINPR_ATTR_NODISCARD pDrawGdiPlusCacheNext DrawGdiPlusCacheNext;
	WINPR_ATTR_NODISCARD pDrawGdiPlusCacheEnd DrawGdiPlusCacheEnd;

	/* Window Update */

	WINPR_ATTR_NODISCARD pWindowCreate WindowCreate;
	WINPR_ATTR_NODISCARD pWindowUpdate WindowUpdate;
	WINPR_ATTR_NODISCARD pWindowIcon WindowIcon;
	WINPR_ATTR_NODISCARD pWindowCachedIcon WindowCachedIcon;
	WINPR_ATTR_NODISCARD pWindowDelete WindowDelete;
	WINPR_ATTR_NODISCARD pNotifyIconCreate NotifyIconCreate;
	WINPR_ATTR_NODISCARD pNotifyIconUpdate NotifyIconUpdate;
	WINPR_ATTR_NODISCARD pNotifyIconDelete NotifyIconDelete;
	WINPR_ATTR_NODISCARD pMonitoredDesktop MonitoredDesktop;
	WINPR_ATTR_NODISCARD pNonMonitoredDesktop NonMonitoredDesktop;

	/* Pointer Update */

	WINPR_ATTR_NODISCARD pPointerPosition PointerPosition;
	WINPR_ATTR_NODISCARD pPointerSystem PointerSystem;
	WINPR_ATTR_NODISCARD pPointerColor PointerColor;
	WINPR_ATTR_NODISCARD pPointerNew PointerNew;
	WINPR_ATTR_NODISCARD pPointerCached PointerCached;
	WINPR_ATTR_NODISCARD pPointerLarge PointerLarge;

	HANDLE thread;
};

WINPR_ATTR_NODISCARD
FREERDP_LOCAL int update_message_queue_process_message(rdpUpdate* update, wMessage* message);

FREERDP_LOCAL int update_message_queue_free_message(wMessage* message);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL int update_message_queue_process_pending_messages(rdpUpdate* update);

FREERDP_LOCAL void update_message_proxy_free(rdpUpdateProxy* message);

WINPR_ATTR_MALLOC(update_message_proxy_free, 1)
WINPR_ATTR_NODISCARD
FREERDP_LOCAL rdpUpdateProxy* update_message_proxy_new(rdpUpdate* update);

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
	pFocusInEvent FocusInEvent;
	pKeyboardPauseEvent KeyboardPauseEvent;
};

FREERDP_LOCAL int input_message_queue_process_message(rdpInput* input, wMessage* message);
FREERDP_LOCAL int input_message_queue_free_message(wMessage* message);
FREERDP_LOCAL int input_message_queue_process_pending_messages(rdpInput* input);

#endif /* FREERDP_LIB_CORE_MESSAGE_H */
