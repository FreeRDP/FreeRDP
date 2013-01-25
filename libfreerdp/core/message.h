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

#define GetMessageType(_id) 	(_id & 0xF)
#define GetMessageClass(_id)	((_id >> 8) & 0xF)

#define MakeMessageId(_class, _type) \
	(((_class ##_Class) << 8) | (_class ## _ ## _type))

/* Update */

#define Update_Class					1

#define Update_BeginPaint				1
#define Update_EndPaint					2
#define Update_SetBounds				3
#define Update_Synchronize				4
#define Update_DesktopResize				5
#define Update_BitmapUpdate				6
#define Update_Palette					7
#define Update_PlaySound				8
#define Update_RefreshRect				9
#define Update_SuppressOutput				10
#define Update_SurfaceCommand				11
#define Update_SurfaceBits				12
#define Update_SurfaceFrameMarker			13
#define Update_SurfaceFrameAcknowledge			14

/* Primary Update */

#define PrimaryUpdate_Class				2

#define PrimaryUpdate_DstBlt				1
#define PrimaryUpdate_PatBlt				2
#define PrimaryUpdate_ScrBlt				3
#define PrimaryUpdate_OpaqueRect			4
#define PrimaryUpdate_DrawNineGrid			5
#define PrimaryUpdate_MultiDstBlt			6
#define PrimaryUpdate_MultiPatBlt			7
#define PrimaryUpdate_MultiScrBlt			8
#define PrimaryUpdate_MultiOpaqueRect			9
#define PrimaryUpdate_MultiDrawNineGrid			10
#define PrimaryUpdate_LineTo				11
#define PrimaryUpdate_Polyline				12
#define PrimaryUpdate_MemBlt				13
#define PrimaryUpdate_Mem3Blt				14
#define PrimaryUpdate_SaveBitmap			15
#define PrimaryUpdate_GlyphIndex			16
#define PrimaryUpdate_FastIndex				17
#define PrimaryUpdate_FastGlyph				18
#define PrimaryUpdate_PolygonSC				19
#define PrimaryUpdate_PolygonCB				20
#define PrimaryUpdate_EllipseSC				21
#define PrimaryUpdate_EllipseCB				22

/* Secondary Update */

#define SecondaryUpdate_Class				3

#define SecondaryUpdate_CacheBitmap			1
#define SecondaryUpdate_CacheBitmapV2			2
#define SecondaryUpdate_CacheBitmapV3			3
#define SecondaryUpdate_CacheColorTable			4
#define SecondaryUpdate_CacheGlyph			5
#define SecondaryUpdate_CacheGlyphV2			6
#define SecondaryUpdate_CacheBrush			7


/* Alternate Secondary Update */

#define AltSecUpdate_Class				4

#define AltSecUpdate_CreateOffscreenBitmap		1
#define AltSecUpdate_SwitchSurface			2
#define AltSecUpdate_CreateNineGridBitmap		3
#define AltSecUpdate_FrameMarker			4
#define AltSecUpdate_StreamBitmapFirst			5
#define AltSecUpdate_StreamBitmapNext			6
#define AltSecUpdate_DrawGdiPlusFirst			7
#define AltSecUpdate_DrawGdiPlusNext			8
#define AltSecUpdate_DrawGdiPlusEnd			9
#define AltSecUpdate_DrawGdiPlusCacheFirst		10
#define AltSecUpdate_DrawGdiPlusCacheNext		11
#define AltSecUpdate_DrawGdiPlusCacheEnd		12

/* Window Update */

#define WindowUpdate_Class				5

#define WindowUpdate_WindowCreate			1
#define WindowUpdate_WindowUpdate			2
#define WindowUpdate_WindowIcon				3
#define WindowUpdate_WindowCachedIcon			4
#define WindowUpdate_WindowDelete			5
#define WindowUpdate_NotifyIconCreate			6
#define WindowUpdate_NotifyIconUpdate			7
#define WindowUpdate_NotifyIconDelete			8
#define WindowUpdate_MonitoredDesktop			9
#define WindowUpdate_NonMonitoredDesktop		10

/* Pointer Update */

#define PointerUpdate_Class				6

#define PointerUpdate_PointerPosition			1
#define PointerUpdate_PointerSystem			2
#define PointerUpdate_PointerColor			3
#define PointerUpdate_PointerNew			4
#define PointerUpdate_PointerCached			5

#endif /* FREERDP_CORE_MESSAGE_PRIVATE_H */
