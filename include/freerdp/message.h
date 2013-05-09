/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Asynchronous Message Interface
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

#ifndef FREERDP_CORE_MESSAGE_H
#define FREERDP_CORE_MESSAGE_H

#define GetMessageType(_id) 		(_id & 0xFF)
#define GetMessageClass(_id)		((_id >> 16) & 0xFF)

#define GetMessageId(_class, _type)	((_class << 16) | _type)

#define MakeMessageId(_class, _type) \
	(((_class ##_Class) << 16) | (_class ## _ ## _type))

/**
 * Update Message Queue
 */

#define FREERDP_UPDATE_MESSAGE_QUEUE				1

#define Update_Base						0

/* Update */

#define Update_Class						(Update_Base + 1)

#define Update_BeginPaint					1
#define Update_EndPaint						2
#define Update_SetBounds					3
#define Update_Synchronize					4
#define Update_DesktopResize					5
#define Update_BitmapUpdate					6
#define Update_Palette						7
#define Update_PlaySound					8
#define Update_RefreshRect					9
#define Update_SuppressOutput					10
#define Update_SurfaceCommand					11
#define Update_SurfaceBits					12
#define Update_SurfaceFrameMarker				13
#define Update_SurfaceFrameAcknowledge				14

#define FREERDP_UPDATE_BEGIN_PAINT				MakeMessageId(Update, BeginPaint)
#define FREERDP_UPDATE_	END_PAINT				MakeMessageId(Update, EndPaint)
#define FREERDP_UPDATE_SET_BOUNDS				MakeMessageId(Update, SetBounds)
#define FREERDP_UPDATE_SYNCHRONIZE				MakeMessageId(Update, Synchronize)
#define FREERDP_UPDATE_DESKTOP_RESIZE				MakeMessageId(Update, DesktopResize)
#define FREERDP_UPDATE_BITMAP_UPDATE				MakeMessageId(Update, BitmapUpdate)
#define FREERDP_UPDATE_PALETTE					MakeMessageId(Update, Palette)
#define FREERDP_UPDATE_PLAY_SOUND				MakeMessageId(Update, PlaySound)
#define FREERDP_UPDATE_REFRESH_RECT				MakeMessageId(Update, RefreshRect)
#define FREERDP_UPDATE_SUPPRESS_OUTPUT				MakeMessageId(Update, SuppressOutput)
#define FREERDP_UPDATE_SURFACE_COMMAND				MakeMessageId(Update, SurfaceCommand)
#define FREERDP_UPDATE_SURFACE_BITS				MakeMessageId(Update, SurfaceBits)
#define FREERDP_UPDATE_SURFACE_FRAME_MARKER			MakeMessageId(Update, SurfaceFrameMarker)
#define FREERDP_UPDATE_SURFACE_FRAME_ACKNOWLEDGE		MakeMessageId(Update, SurfaceFrameAcknowledge)

/* Primary Update */

#define PrimaryUpdate_Class					(Update_Base + 2)

#define PrimaryUpdate_DstBlt					1
#define PrimaryUpdate_PatBlt					2
#define PrimaryUpdate_ScrBlt					3
#define PrimaryUpdate_OpaqueRect				4
#define PrimaryUpdate_DrawNineGrid				5
#define PrimaryUpdate_MultiDstBlt				6
#define PrimaryUpdate_MultiPatBlt				7
#define PrimaryUpdate_MultiScrBlt				8
#define PrimaryUpdate_MultiOpaqueRect				9
#define PrimaryUpdate_MultiDrawNineGrid				10
#define PrimaryUpdate_LineTo					11
#define PrimaryUpdate_Polyline					12
#define PrimaryUpdate_MemBlt					13
#define PrimaryUpdate_Mem3Blt					14
#define PrimaryUpdate_SaveBitmap				15
#define PrimaryUpdate_GlyphIndex				16
#define PrimaryUpdate_FastIndex					17
#define PrimaryUpdate_FastGlyph					18
#define PrimaryUpdate_PolygonSC					19
#define PrimaryUpdate_PolygonCB					20
#define PrimaryUpdate_EllipseSC					21
#define PrimaryUpdate_EllipseCB					22

#define FREERDP_PRIMARY_UPDATE_DSTBLT				MakeMessageId(PrimaryUpdate, DstBlt)
#define FREERDP_PRIMARY_UPDATE_PATBLT				MakeMessageId(PrimaryUpdate, PatBlt)
#define FREERDP_PRIMARY_UPDATE_SCRBLT				MakeMessageId(PrimaryUpdate, ScrBlt)
#define FREERDP_PRIMARY_UPDATE_OPAQUE_RECT			MakeMessageId(PrimaryUpdate, OpaqueRect)
#define FREERDP_PRIMARY_UPDATE_DRAW_NINE_GRID			MakeMessageId(PrimaryUpdate, DrawNineGrid)
#define FREERDP_PRIMARY_UPDATE_MULTI_DSTBLT			MakeMessageId(PrimaryUpdate, MultiDstBlt)
#define FREERDP_PRIMARY_UPDATE_MULTI_PATBLT			MakeMessageId(PrimaryUpdate, MultiPatBlt)
#define FREERDP_PRIMARY_UPDATE_MULTI_SCRBLT			MakeMessageId(PrimaryUpdate, MultiScrBlt)
#define FREERDP_PRIMARY_UPDATE_MULTI_OPAQUE_RECT		MakeMessageId(PrimaryUpdate, MultiOpaqueRect)
#define FREERDP_PRIMARY_UPDATE_MULTI_DRAW_NINE_GRID		MakeMessageId(PrimaryUpdate, MultiDrawNineGrid)
#define FREERDP_PRIMARY_UPDATE_LINE_TO				MakeMessageId(PrimaryUpdate, LineTo)
#define FREERDP_PRIMARY_UPDATE_POLYLINE				MakeMessageId(PrimaryUpdate, Polyline)
#define FREERDP_PRIMARY_UPDATE_MEMBLT				MakeMessageId(PrimaryUpdate, MemBlt)
#define FREERDP_PRIMARY_UPDATE_MEM3BLT				MakeMessageId(PrimaryUpdate, Mem3Blt)
#define FREERDP_PRIMARY_UPDATE_SAVE_BITMAP			MakeMessageId(PrimaryUpdate, SaveBitmap)
#define FREERDP_PRIMARY_UPDATE_GLYPH_INDEX			MakeMessageId(PrimaryUpdate, GlyphIndex)
#define FREERDP_PRIMARY_UPDATE_FAST_INDEX			MakeMessageId(PrimaryUpdate, FastIndex)
#define FREERDP_PRIMARY_UPDATE_FAST_GLYPH			MakeMessageId(PrimaryUpdate, FastGlyph)
#define FREERDP_PRIMARY_UPDATE_POLYGON_SC			MakeMessageId(PrimaryUpdate, PolygonSC)
#define FREERDP_PRIMARY_UPDATE_POLYGON_CB			MakeMessageId(PrimaryUpdate, PolygonCB)
#define FREERDP_PRIMARY_UPDATE_ELLIPSE_SC			MakeMessageId(PrimaryUpdate, EllipseSC)
#define FREERDP_PRIMARY_UPDATE_ELLIPSE_CB			MakeMessageId(PrimaryUpdate, EllipseCB)

/* Secondary Update */

#define SecondaryUpdate_Class					(Update_Base + 3)

#define SecondaryUpdate_CacheBitmap				1
#define SecondaryUpdate_CacheBitmapV2				2
#define SecondaryUpdate_CacheBitmapV3				3
#define SecondaryUpdate_CacheColorTable				4
#define SecondaryUpdate_CacheGlyph				5
#define SecondaryUpdate_CacheGlyphV2				6
#define SecondaryUpdate_CacheBrush				7

#define FREERDP_SECONDARY_UPDATE_CACHE_BITMAP			MakeMessageId(SecondaryUpdate, CacheBitmap)
#define FREERDP_SECONDARY_UPDATE_CACHE_BITMAP_V2		MakeMessageId(SecondaryUpdate, CacheBitmapV2)
#define FREERDP_SECONDARY_UPDATE_CACHE_BITMAP_V3		MakeMessageId(SecondaryUpdate, CacheBitmapV3)
#define FREERDP_SECONDARY_UPDATE_CACHE_COLOR_TABLE		MakeMessageId(SecondaryUpdate, CacheColorTable)
#define FREERDP_SECONDARY_UPDATE_CACHE_GLYPH			MakeMessageId(SecondaryUpdate, CacheGlyph)
#define FREERDP_SECONDARY_UPDATE_CACHE_GLYPH_V2			MakeMessageId(SecondaryUpdate, CacheGlyphV2)
#define FREERDP_SECONDARY_UPDATE_CACHE_BRUSH			MakeMessageId(SecondaryUpdate, CacheBrush)

/* Alternate Secondary Update */

#define AltSecUpdate_Class					(Update_Base + 4)

#define AltSecUpdate_CreateOffscreenBitmap			1
#define AltSecUpdate_SwitchSurface				2
#define AltSecUpdate_CreateNineGridBitmap			3
#define AltSecUpdate_FrameMarker				4
#define AltSecUpdate_StreamBitmapFirst				5
#define AltSecUpdate_StreamBitmapNext				6
#define AltSecUpdate_DrawGdiPlusFirst				7
#define AltSecUpdate_DrawGdiPlusNext				8
#define AltSecUpdate_DrawGdiPlusEnd				9
#define AltSecUpdate_DrawGdiPlusCacheFirst			10
#define AltSecUpdate_DrawGdiPlusCacheNext			11
#define AltSecUpdate_DrawGdiPlusCacheEnd			12

#define FREERDP_ALTSEC_UPDATE_CREATE_OFFSCREEN_BITMAP		MakeMessageId(AltSecUpdate, CreateOffscreenBitmap)
#define FREERDP_ALTSEC_UPDATE_SWITCH_SURFACE			MakeMessageId(AltSecUpdate, SwitchSurface)
#define FREERDP_ALTSEC_UPDATE_CREATE_NINE_GRID_BITMAP		MakeMessageId(AltSecUpdate, CreateNineGridBitmap)
#define FREERDP_ALTSEC_UPDATE_FRAME_MARKER			MakeMessageId(AltSecUpdate, FrameMarker)
#define FREERDP_ALTSEC_UPDATE_STREAM_BITMAP_FIRST		MakeMessageId(AltSecUpdate, StreamBitmapFirst)
#define FREERDP_ALTSEC_UPDATE_STREAM_BITMAP_NEXT		MakeMessageId(AltSecUpdate, StreamBitmapNext)
#define FREERDP_ALTSEC_UPDATE_DRAW_GDI_PLUS_FIRST		MakeMessageId(AltSecUpdate, DrawGdiPlusFirst)
#define FREERDP_ALTSEC_UPDATE_DRAW_GDI_PLUS_NEXT		MakeMessageId(AltSecUpdate, DrawGdiPlusNext)
#define FREERDP_ALTSEC_UPDATE_DRAW_GDI_PLUS_END			MakeMessageId(AltSecUpdate, DrawGdiPlusEnd)
#define FREERDP_ALTSEC_UPDATE_DRAW_GDI_PLUS_CACHE_FIRST		MakeMessageId(AltSecUpdate, DrawGdiPlusCacheFirst)
#define FREERDP_ALTSEC_UPDATE_DRAW_GDI_PLUS_CACHE_NEXT		MakeMessageId(AltSecUpdate, DrawGdiPlusCacheNext)
#define FREERDP_ALTSEC_UPDATE_DRAW_GDI_PLUS_CACHE_END		MakeMessageId(AltSecUpdate, DrawGdiPlusCacheEnd)

/* Window Update */

#define WindowUpdate_Class					(Update_Base + 5)

#define WindowUpdate_WindowCreate				1
#define WindowUpdate_WindowUpdate				2
#define WindowUpdate_WindowIcon					3
#define WindowUpdate_WindowCachedIcon				4
#define WindowUpdate_WindowDelete				5
#define WindowUpdate_NotifyIconCreate				6
#define WindowUpdate_NotifyIconUpdate				7
#define WindowUpdate_NotifyIconDelete				8
#define WindowUpdate_MonitoredDesktop				9
#define WindowUpdate_NonMonitoredDesktop			10

#define FREERDP_WINDOW_UPDATE_WINDOW_CREATE			MakeMessageId(WindowUpdate, WindowCreate)
#define FREERDP_WINDOW_UPDATE_WINDOW_UPDATE			MakeMessageId(WindowUpdate, WindowUpdate)
#define FREERDP_WINDOW_UPDATE_WINDOW_ICON			MakeMessageId(WindowUpdate, WindowIcon)
#define FREERDP_WINDOW_UPDATE_WINDOW_CACHED_ICON		MakeMessageId(WindowUpdate, WindowCachedIcon)
#define FREERDP_WINDOW_UPDATE_WINDOW_DELETE			MakeMessageId(WindowUpdate, WindowDelete)
#define FREERDP_WINDOW_UPDATE_NOTIFY_ICON_CREATE		MakeMessageId(WindowUpdate, NotifyIconCreate)
#define FREERDP_WINDOW_UPDATE_NOTIFY_ICON_UPDATE		MakeMessageId(WindowUpdate, NotifyIconUpdate)
#define FREERDP_WINDOW_UPDATE_NOTIFY_ICON_DELETE		MakeMessageId(WindowUpdate, NotifyIconDelete)
#define FREERDP_WINDOW_UPDATE_MONITORED_DESKTOP			MakeMessageId(WindowUpdate, MonitoredDesktop)
#define FREERDP_WINDOW_UPDATE_NON_MONITORED_DESKTOP		MakeMessageId(WindowUpdate, NonMonitoredDesktop)

/* Pointer Update */

#define PointerUpdate_Class					(Update_Base + 6)

#define PointerUpdate_PointerPosition				1
#define PointerUpdate_PointerSystem				2
#define PointerUpdate_PointerColor				3
#define PointerUpdate_PointerNew				4
#define PointerUpdate_PointerCached				5

#define FREERDP_POINTER_UPDATE_	POINTER_POSITION		MakeMessageId(PointerUpdate, PointerPosition)
#define FREERDP_POINTER_UPDATE_POINTER_SYSTEM			MakeMessageId(PointerUpdate, PointerSystem)
#define FREERDP_POINTER_UPDATE_POINTER_COLOR			MakeMessageId(PointerUpdate, PointerColor)
#define FREERDP_POINTER_UPDATE_POINTER_NEW			MakeMessageId(PointerUpdate, PointerNew)
#define FREERDP_POINTER_UPDATE_POINTER_CACHED			MakeMessageId(PointerUpdate, PointerCached)

/**
 * Input Message Queue
 */

#define FREERDP_INPUT_MESSAGE_QUEUE				2

#define Input_Base						16

/* Input */

#define Input_Class						(Input_Base + 1)

#define Input_SynchronizeEvent					1
#define Input_KeyboardEvent					2
#define Input_UnicodeKeyboardEvent				3
#define Input_MouseEvent					4
#define Input_ExtendedMouseEvent				5

#define FREERDP_INPUT_SYNCHRONIZE_EVENT				MakeMessageId(Input, SynchronizeEvent)
#define FREERDP_INPUT_KEYBOARD_EVENT				MakeMessageId(Input, KeyboardEvent)
#define FREERDP_INPUT_UNICODE_KEYBOARD_EVENT			MakeMessageId(Input, UnicodeKeyboardEvent)
#define FREERDP_INPUT_MOUSE_EVENT				MakeMessageId(Input, MouseEvent)
#define FREERDP_INPUT_EXTENDED_MOUSE_EVENT			MakeMessageId(Input, ExtendedMouseEvent)

/**
 * Static Channel Message Queues
 */

#define FREERDP_CHANNEL_MESSAGE_QUEUE				3

#define Channel_Base						20

/**
 * Debug Channel
 */

#define DebugChannel_Class					(Channel_Base + 1)

/**
 * Clipboard Channel
 */

#define CliprdrChannel_Class					(Channel_Base + 2)

#define CliprdrChannel_MonitorReady				1
#define CliprdrChannel_FormatList				2
#define CliprdrChannel_DataRequest				3
#define CliprdrChannel_DataResponse				4

#define FREERDP_CLIPRDR_CHANNEL_MONITOR_READY			MakeMessageId(CliprdrChannel, MonitorReady)
#define FREERDP_CLIPRDR_CHANNEL_FORMAT_LIST			MakeMessageId(CliprdrChannel, FormatList)
#define FREERDP_CLIPRDR_CHANNEL_DATA_REQUEST			MakeMessageId(CliprdrChannel, DataRequest)
#define FREERDP_CLIPRDR_CHANNEL_DATA_RESPONSE			MakeMessageId(CliprdrChannel, DataResponse)

/**
 * Multimedia Redirection Channel
 */

#define TsmfChannel_Class					(Channel_Base + 3)

#define TsmfChannel_VideoFrame					1
#define TsmfChannel_Redraw					2

#define FREERDP_TSMF_CHANNEL_VIDEO_FRAME			MakeMessageId(TsmfChannel, VideoFrame)
#define FREERDP_TSMF_CHANNEL_REDRAW				MakeMessageId(TsmfChannel, Redraw)

/**
 * RemoteApp Channel
 */

#define RailChannel_Class					(Channel_Base + 4)

#define RailChannel_ClientExecute				1
#define RailChannel_ClientActivate				2
#define RailChannel_GetSystemParam				3
#define RailChannel_ClientSystemParam				4
#define RailChannel_ServerSystemParam				5
#define RailChannel_ClientSystemCommand				6
#define RailChannel_ClientHandshake				7
#define RailChannel_ServerHandshake				8
#define RailChannel_ClientNotifyEvent				9
#define RailChannel_ClientWindowMove				10
#define RailChannel_ServerLocalMoveSize				11
#define RailChannel_ServerMinMaxInfo				12
#define RailChannel_ClientInformation				13
#define RailChannel_ClientSystemMenu				14
#define RailChannel_ClientLanguageBarInfo			15
#define RailChannel_ServerLanguageBarInfo			16
#define RailChannel_ServerExecuteResult				17
#define RailChannel_ClientGetAppIdRequest			18
#define RailChannel_ServerGetAppIdResponse			19

#define FREERDP_RAIL_CHANNEL_CLIENT_EXECUTE			MakeMessageId(RailChannel, ClientExecute)
#define FREERDP_RAIL_CHANNEL_CLIENT_ACTIVATE			MakeMessageId(RailChannel, ClientActivate)
#define FREERDP_RAIL_CHANNEL_GET_SYSTEM_PARAM			MakeMessageId(RailChannel, GetSystemParam)
#define FREERDP_RAIL_CHANNEL_CLIENT_SYSTEM_PARAM		MakeMessageId(RailChannel, ClientSystemParam)
#define FREERDP_RAIL_CHANNEL_SERVER_SYSTEM_PARAM		MakeMessageId(RailChannel, ClientSystemParam)
#define FREERDP_RAIL_CHANNEL_CLIENT_SYSTEM_COMMAND		MakeMessageId(RailChannel, ClientSystemCommand)
#define FREERDP_RAIL_CHANNEL_CLIENT_HANDSHAKE			MakeMessageId(RailChannel, ClientHandshake)
#define FREERDP_RAIL_CHANNEL_SERVER_HANDSHAKE			MakeMessageId(RailChannel, ServerHandshake)
#define FREERDP_RAIL_CHANNEL_CLIENT_NOTIFY_EVENT		MakeMessageId(RailChannel, ClientNotifyEvent)
#define FREERDP_RAIL_CHANNEL_CLIENT_WINDOW_MOVE			MakeMessageId(RailChannel, ClientWindowMove)
#define FREERDP_RAIL_CHANNEL_SERVER_LOCAL_MOVE_SIZE		MakeMessageId(RailChannel, ServerLocalMoveSize)
#define FREERDP_RAIL_CHANNEL_SERVER_MIN_MAX_INFO		MakeMessageId(RailChannel, ServerMinMaxInfo)
#define FREERDP_RAIL_CHANNEL_CLIENT_INFORMATION			MakeMessageId(RailChannel, ClientInformation)
#define FREERDP_RAIL_CHANNEL_CLIENT_SYSTEM_MENU			MakeMessageId(RailChannel, ClientSystemMenu)
#define FREERDP_RAIL_CHANNEL_CLIENT_LANGUAGE_BAR_INFO		MakeMessageId(RailChannel, ClientLanguageBarInfo)
#define FREERDP_RAIL_CHANNEL_SERVER_LANGUAGE_BAR_INFO		MakeMessageId(RailChannel, ServerLanguageBarInfo)
#define FREERDP_RAIL_CHANNEL_SERVER_EXECUTE_RESULT		MakeMessageId(RailChannel, ServerExecuteResult)
#define FREERDP_RAIL_CHANNEL_CLIENT_GET_APP_ID_REQUEST		MakeMessageId(RailChannel, ClientGetAppIdRequest)
#define FREERDP_RAIL_CHANNEL_SERVER_GET_APP_ID_RESPONSE		MakeMessageId(RailChannel, ServerGetAppIdResponse)

/**
 * MultiTouch Input Channel Extension (MS-RDPEDI)
 */

#define RdpeiChannel_Class					(Channel_Base + 5)

#define RdpeiChannel_ServerReady				1
#define RdpeiChannel_ClientReady				2
#define RdpeiChannel_TouchEvent					3
#define RdpeiChannel_SuspendTouch				4
#define RdpeiChannel_ResumeTouch				5
#define RdpeiChannel_DismissHoveringContact			6

#define FREERDP_RDPEI_CHANNEL_SERVER_READY			MakeMessageId(RdpeiChannel, ServerReady)
#define FREERDP_RDPEI_CHANNEL_CLIENT_READY			MakeMessageId(RdpeiChannel, ClientReady)
#define FREERDP_RDPEI_CHANNEL_TOUCH_EVENT			MakeMessageId(RdpeiChannel, TouchEvent)
#define FREERDP_RDPEI_CHANNEL_SUSPEND_TOUCH			MakeMessageId(RdpeiChannel, SuspendTouch)
#define FREERDP_RDPEI_CHANNEL_RESUME_TOUCH			MakeMessageId(RdpeiChannel, ResumeTouch)
#define FREERDP_RDPEI_CHANNEL_DISMISS_HOVERING_CONTACT		MakeMessageId(RdpeiChannel, DismissHoveringContact)

#endif /* FREERDP_CORE_MESSAGE_H */

