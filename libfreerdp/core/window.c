/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windowing Alternate Secondary Orders
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2011 Roman Barabanov <romanbarabanov@gmail.com>
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

#include <winpr/crt.h>

#include <freerdp/log.h>

#include "window.h"

#define TAG FREERDP_TAG("core.window")

BOOL rail_read_unicode_string(wStream* s, RAIL_UNICODE_STRING* unicode_string)
{
	UINT16 new_len;
	BYTE *new_str;

	if (Stream_GetRemainingLength(s) < 2)
		return FALSE;

	Stream_Read_UINT16(s, new_len); /* cbString (2 bytes) */

	if (Stream_GetRemainingLength(s) < new_len)
		return FALSE;

	if (!new_len)
	{
		free(unicode_string->string);
		unicode_string->string = NULL;
		unicode_string->length = 0;
		return TRUE;
	}

	new_str = (BYTE*) realloc(unicode_string->string, new_len);
	if (!new_str)
	{
		free (unicode_string->string);
		unicode_string->string = NULL;
		return FALSE;
	}

	unicode_string->string = new_str;
	unicode_string->length = new_len;
	Stream_Read(s, unicode_string->string, unicode_string->length);

	return TRUE;
}

BOOL update_read_icon_info(wStream* s, ICON_INFO* iconInfo)
{
	BYTE* newBitMask;

	if (Stream_GetRemainingLength(s) < 8)
		return FALSE;

	Stream_Read_UINT16(s, iconInfo->cacheEntry); /* cacheEntry (2 bytes) */
	Stream_Read_UINT8(s, iconInfo->cacheId); /* cacheId (1 byte) */
	Stream_Read_UINT8(s, iconInfo->bpp); /* bpp (1 byte) */

	if ((iconInfo->bpp < 1) || (iconInfo->bpp > 32))
	{
		WLog_ERR(TAG, "invalid bpp value %d", iconInfo->bpp);
		return FALSE;
	}

	Stream_Read_UINT16(s, iconInfo->width); /* width (2 bytes) */
	Stream_Read_UINT16(s, iconInfo->height); /* height (2 bytes) */

	/* cbColorTable is only present when bpp is 1, 2 or 4 */
	if (iconInfo->bpp == 1 || iconInfo->bpp == 2 || iconInfo->bpp == 4)
	{
		if (Stream_GetRemainingLength(s) < 2)
			return FALSE;

		Stream_Read_UINT16(s, iconInfo->cbColorTable); /* cbColorTable (2 bytes) */
	}
	else
	{
		iconInfo->cbColorTable = 0;
	}

	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT16(s, iconInfo->cbBitsMask); /* cbBitsMask (2 bytes) */
	Stream_Read_UINT16(s, iconInfo->cbBitsColor); /* cbBitsColor (2 bytes) */

	if (Stream_GetRemainingLength(s) < iconInfo->cbBitsMask + iconInfo->cbBitsColor)
		return FALSE;

	/* bitsMask */
	newBitMask = (BYTE*) realloc(iconInfo->bitsMask, iconInfo->cbBitsMask);
	if (!newBitMask)
	{
		free (iconInfo->bitsMask);
		iconInfo->bitsMask = NULL;
		return FALSE;
	}
	iconInfo->bitsMask = newBitMask;

	Stream_Read(s, iconInfo->bitsMask, iconInfo->cbBitsMask);

	/* colorTable */
	if (iconInfo->colorTable == NULL)
	{
		if (iconInfo->cbColorTable)
		{
			iconInfo->colorTable = (BYTE*) malloc(iconInfo->cbColorTable);
			if (!iconInfo->colorTable)
				return FALSE;
		}
	}
	else if (iconInfo->cbColorTable)
	{
		BYTE *new_tab;

		new_tab = (BYTE*) realloc(iconInfo->colorTable, iconInfo->cbColorTable);
		if (!new_tab)
		{
			free (iconInfo->colorTable);
			iconInfo->colorTable = NULL;
			return FALSE;
		}
		iconInfo->colorTable = new_tab;
	}
	else
	{
		free(iconInfo->colorTable);
		iconInfo->colorTable = NULL;
	}

	if (iconInfo->colorTable)
		Stream_Read(s, iconInfo->colorTable, iconInfo->cbColorTable);

	/* bitsColor */
	newBitMask = (BYTE *)realloc(iconInfo->bitsColor, iconInfo->cbBitsColor);
	if (!newBitMask)
	{
		free (iconInfo->bitsColor);
		iconInfo->bitsColor = NULL;
		return FALSE;
	}
	iconInfo->bitsColor = newBitMask;

	Stream_Read(s, iconInfo->bitsColor, iconInfo->cbBitsColor);

	return TRUE;
}

BOOL update_read_cached_icon_info(wStream* s, CACHED_ICON_INFO* cachedIconInfo)
{
	if (Stream_GetRemainingLength(s) < 3)
		return FALSE;

	Stream_Read_UINT16(s, cachedIconInfo->cacheEntry); /* cacheEntry (2 bytes) */
	Stream_Read_UINT8(s, cachedIconInfo->cacheId); /* cacheId (1 byte) */

	return TRUE;
}

BOOL update_read_notify_icon_infotip(wStream* s, NOTIFY_ICON_INFOTIP* notifyIconInfoTip)
{
	if (Stream_GetRemainingLength(s) < 8)
		return FALSE;

	Stream_Read_UINT32(s, notifyIconInfoTip->timeout); /* timeout (4 bytes) */
	Stream_Read_UINT32(s, notifyIconInfoTip->flags); /* infoFlags (4 bytes) */

	return rail_read_unicode_string(s, &notifyIconInfoTip->text) && /* infoTipText */
			rail_read_unicode_string(s, &notifyIconInfoTip->title); /* title */
}

BOOL update_read_window_state_order(wStream* s, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* windowState)
{
	int i;
	int size;

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_OWNER)
	{
		if (Stream_GetRemainingLength(s) < 4)
			return FALSE;

		Stream_Read_UINT32(s, windowState->ownerWindowId); /* ownerWindowId (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_STYLE)
	{
		if (Stream_GetRemainingLength(s) < 8)
			return FALSE;

		Stream_Read_UINT32(s, windowState->style); /* style (4 bytes) */
		Stream_Read_UINT32(s, windowState->extendedStyle); /* extendedStyle (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_SHOW)
	{
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, windowState->showState); /* showState (1 byte) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_TITLE)
	{
		if (!rail_read_unicode_string(s, &windowState->titleInfo)) /* titleInfo */
			return FALSE;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET)
	{
		if (Stream_GetRemainingLength(s) < 4)
			return FALSE;

		Stream_Read_UINT32(s, windowState->clientOffsetX); /* clientOffsetX (4 bytes) */
		Stream_Read_UINT32(s, windowState->clientOffsetY); /* clientOffsetY (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE)
	{
		if (Stream_GetRemainingLength(s) < 4)
			return FALSE;

		Stream_Read_UINT32(s, windowState->clientAreaWidth); /* clientAreaWidth (4 bytes) */
		Stream_Read_UINT32(s, windowState->clientAreaHeight); /* clientAreaHeight (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_RP_CONTENT)
	{
		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, windowState->RPContent); /* RPContent (1 byte) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ROOT_PARENT)
	{
		if (Stream_GetRemainingLength(s) < 4)
			return FALSE;

		Stream_Read_UINT32(s, windowState->rootParentHandle);/* rootParentHandle (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_OFFSET)
	{
		if (Stream_GetRemainingLength(s) < 8)
			return FALSE;

		Stream_Read_UINT32(s, windowState->windowOffsetX); /* windowOffsetX (4 bytes) */
		Stream_Read_UINT32(s, windowState->windowOffsetY); /* windowOffsetY (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_CLIENT_DELTA)
	{
		if (Stream_GetRemainingLength(s) < 8)
			return FALSE;

		Stream_Read_UINT32(s, windowState->windowClientDeltaX); /* windowClientDeltaX (4 bytes) */
		Stream_Read_UINT32(s, windowState->windowClientDeltaY); /* windowClientDeltaY (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE)
	{
		if (Stream_GetRemainingLength(s) < 8)
			return FALSE;

		Stream_Read_UINT32(s, windowState->windowWidth); /* windowWidth (4 bytes) */
		Stream_Read_UINT32(s, windowState->windowHeight); /* windowHeight (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_RECTS)
	{
		if (Stream_GetRemainingLength(s) < 2)
			return FALSE;

		Stream_Read_UINT16(s, windowState->numWindowRects); /* numWindowRects (2 bytes) */

		size = sizeof(RECTANGLE_16) * windowState->numWindowRects;
		windowState->windowRects = (RECTANGLE_16*) malloc(size);
		if (!windowState->windowRects)
			return FALSE;

		if (Stream_GetRemainingLength(s) < 8 * windowState->numWindowRects)
			return FALSE;

		/* windowRects */
		for (i = 0; i < (int) windowState->numWindowRects; i++)
		{
			Stream_Read_UINT16(s, windowState->windowRects[i].left); /* left (2 bytes) */
			Stream_Read_UINT16(s, windowState->windowRects[i].top); /* top (2 bytes) */
			Stream_Read_UINT16(s, windowState->windowRects[i].right); /* right (2 bytes) */
			Stream_Read_UINT16(s, windowState->windowRects[i].bottom); /* bottom (2 bytes) */
		}
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VIS_OFFSET)
	{
		if (Stream_GetRemainingLength(s) < 4)
			return FALSE;

		Stream_Read_UINT32(s, windowState->visibleOffsetX); /* visibleOffsetX (4 bytes) */
		Stream_Read_UINT32(s, windowState->visibleOffsetY); /* visibleOffsetY (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY)
	{
		if (Stream_GetRemainingLength(s) < 2)
			return FALSE;

		Stream_Read_UINT16(s, windowState->numVisibilityRects); /* numVisibilityRects (2 bytes) */

		size = sizeof(RECTANGLE_16) * windowState->numVisibilityRects;
		windowState->visibilityRects = (RECTANGLE_16*) malloc(size);
		if (!windowState->visibilityRects)
			return FALSE;

		if (Stream_GetRemainingLength(s) < windowState->numVisibilityRects * 8)
			return FALSE;

		/* visibilityRects */
		for (i = 0; i < (int) windowState->numVisibilityRects; i++)
		{
			Stream_Read_UINT16(s, windowState->visibilityRects[i].left); /* left (2 bytes) */
			Stream_Read_UINT16(s, windowState->visibilityRects[i].top); /* top (2 bytes) */
			Stream_Read_UINT16(s, windowState->visibilityRects[i].right); /* right (2 bytes) */
			Stream_Read_UINT16(s, windowState->visibilityRects[i].bottom); /* bottom (2 bytes) */
		}
	}
	return TRUE;
}

BOOL update_read_window_icon_order(wStream* s, WINDOW_ORDER_INFO* orderInfo, WINDOW_ICON_ORDER* window_icon)
{
	window_icon->iconInfo = (ICON_INFO*) calloc(1, sizeof(ICON_INFO));
	if (!window_icon->iconInfo)
		return FALSE;

	return update_read_icon_info(s, window_icon->iconInfo); /* iconInfo (ICON_INFO) */
}

BOOL update_read_window_cached_icon_order(wStream* s, WINDOW_ORDER_INFO* orderInfo, WINDOW_CACHED_ICON_ORDER* window_cached_icon)
{
	return update_read_cached_icon_info(s, &window_cached_icon->cachedIcon); /* cachedIcon (CACHED_ICON_INFO) */
}

void update_read_window_delete_order(wStream* s, WINDOW_ORDER_INFO* orderInfo)
{
	/* window deletion event */
}

BOOL update_recv_window_info_order(rdpUpdate* update, wStream* s, WINDOW_ORDER_INFO* orderInfo)
{
	rdpContext* context = update->context;
	rdpWindowUpdate* window = update->window;
	BOOL result = TRUE;

	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT32(s, orderInfo->windowId); /* windowId (4 bytes) */

	if (orderInfo->fieldFlags & WINDOW_ORDER_ICON)
	{
		if (!update_read_window_icon_order(s, orderInfo, &window->window_icon))
			return FALSE;
		WLog_Print(update->log, WLOG_DEBUG, "WindowIcon");
		IFCALLRET(window->WindowIcon, result, context, orderInfo, &window->window_icon);
	}
	else if (orderInfo->fieldFlags & WINDOW_ORDER_CACHED_ICON)
	{
		if (!update_read_window_cached_icon_order(s, orderInfo, &window->window_cached_icon))
			return FALSE;
		WLog_Print(update->log, WLOG_DEBUG, "WindowCachedIcon");
		IFCALLRET(window->WindowCachedIcon, result, context, orderInfo, &window->window_cached_icon);
	}
	else if (orderInfo->fieldFlags & WINDOW_ORDER_STATE_DELETED)
	{
		update_read_window_delete_order(s, orderInfo);
		WLog_Print(update->log, WLOG_DEBUG, "WindowDelete");
		IFCALLRET(window->WindowDelete, result, context, orderInfo);
	}
	else
	{
		if (!update_read_window_state_order(s, orderInfo, &window->window_state))
			return FALSE;

		if (orderInfo->fieldFlags & WINDOW_ORDER_STATE_NEW)
		{
			WLog_Print(update->log, WLOG_DEBUG, "WindowCreate");
			IFCALLRET(window->WindowCreate, result, context, orderInfo, &window->window_state);
		}
		else
		{
			WLog_Print(update->log, WLOG_DEBUG, "WindowUpdate");
			IFCALLRET(window->WindowUpdate, result, context, orderInfo, &window->window_state);
		}
	}

	return result;
}

BOOL update_read_notification_icon_state_order(wStream* s, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notify_icon_state)
{
	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_VERSION)
	{
		if (Stream_GetRemainingLength(s) < 4)
			return FALSE;
		Stream_Read_UINT32(s, notify_icon_state->version); /* version (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_TIP)
	{
		if (!rail_read_unicode_string(s, &notify_icon_state->toolTip)) /* toolTip (UNICODE_STRING) */
			return FALSE;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_INFO_TIP)
	{
		if (!update_read_notify_icon_infotip(s, &notify_icon_state->infoTip)) /* infoTip (NOTIFY_ICON_INFOTIP) */
			return FALSE;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_STATE)
	{
		if (Stream_GetRemainingLength(s) < 4)
			return FALSE;

		Stream_Read_UINT32(s, notify_icon_state->state); /* state (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_ICON)
	{
		if (!update_read_icon_info(s, &notify_icon_state->icon)) /* icon (ICON_INFO) */
			return FALSE;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_CACHED_ICON)
	{
		if (!update_read_cached_icon_info(s, &notify_icon_state->cachedIcon)) /* cachedIcon (CACHED_ICON_INFO) */
			return FALSE;
	}

	return TRUE;
}

void update_read_notification_icon_delete_order(wStream* s, WINDOW_ORDER_INFO* orderInfo)
{
	/* notification icon deletion event */
}

BOOL update_recv_notification_icon_info_order(rdpUpdate* update, wStream* s, WINDOW_ORDER_INFO* orderInfo)
{
	rdpContext* context = update->context;
	rdpWindowUpdate* window = update->window;
	BOOL result = TRUE;

	if (Stream_GetRemainingLength(s) < 8)
		return FALSE;

	Stream_Read_UINT32(s, orderInfo->windowId); /* windowId (4 bytes) */
	Stream_Read_UINT32(s, orderInfo->notifyIconId); /* notifyIconId (4 bytes) */

	if (orderInfo->fieldFlags & WINDOW_ORDER_STATE_DELETED)
	{
		update_read_notification_icon_delete_order(s, orderInfo);
		WLog_Print(update->log, WLOG_DEBUG, "NotifyIconDelete");
		IFCALLRET(window->NotifyIconDelete, result, context, orderInfo);
	}
	else
	{
		if (!update_read_notification_icon_state_order(s, orderInfo, &window->notify_icon_state))
			return FALSE;

		if (orderInfo->fieldFlags & WINDOW_ORDER_STATE_NEW)
		{
			WLog_Print(update->log, WLOG_DEBUG, "NotifyIconCreate");
			IFCALLRET(window->NotifyIconCreate, result, context, orderInfo, &window->notify_icon_state);
		}
		else
		{
			WLog_Print(update->log, WLOG_DEBUG, "NotifyIconUpdate");
			IFCALLRET(window->NotifyIconUpdate, result, context, orderInfo, &window->notify_icon_state);
		}
	}

	return result;
}

BOOL update_read_desktop_actively_monitored_order(wStream* s, WINDOW_ORDER_INFO* orderInfo, MONITORED_DESKTOP_ORDER* monitored_desktop)
{
	int i;
	int size;

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_DESKTOP_ACTIVE_WND)
	{
		if (Stream_GetRemainingLength(s) < 4)
			return FALSE;

		Stream_Read_UINT32(s, monitored_desktop->activeWindowId); /* activeWindowId (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_DESKTOP_ZORDER)
	{
		UINT32 *newid;

		if (Stream_GetRemainingLength(s) < 1)
			return FALSE;

		Stream_Read_UINT8(s, monitored_desktop->numWindowIds); /* numWindowIds (1 byte) */

		if (Stream_GetRemainingLength(s) < 4 * monitored_desktop->numWindowIds)
			return FALSE;

		if (monitored_desktop->numWindowIds > 0) {
			size = sizeof(UINT32) * monitored_desktop->numWindowIds;

			newid = (UINT32*)realloc(monitored_desktop->windowIds, size);
			if (!newid)
			{
				free(monitored_desktop->windowIds);
				monitored_desktop->windowIds = NULL;
				return FALSE;
			}
			monitored_desktop->windowIds = newid;

			/* windowIds */
			for (i = 0; i < (int)monitored_desktop->numWindowIds; i++)
			{
				Stream_Read_UINT32(s, monitored_desktop->windowIds[i]);
			}
		}
	}

	return TRUE;
}

void update_read_desktop_non_monitored_order(wStream* s, WINDOW_ORDER_INFO* orderInfo)
{
	/* non-monitored desktop notification event */
}

BOOL update_recv_desktop_info_order(rdpUpdate* update, wStream* s, WINDOW_ORDER_INFO* orderInfo)
{
	rdpContext* context = update->context;
	rdpWindowUpdate* window = update->window;
	BOOL result = TRUE;

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_DESKTOP_NONE)
	{
		update_read_desktop_non_monitored_order(s, orderInfo);
		WLog_Print(update->log, WLOG_DEBUG, "NonMonitoredDesktop");
		IFCALLRET(window->NonMonitoredDesktop, result, context, orderInfo);
	}
	else
	{
		if (!update_read_desktop_actively_monitored_order(s, orderInfo, &window->monitored_desktop))
			return FALSE;
		WLog_Print(update->log, WLOG_DEBUG, "ActivelyMonitoredDesktop");
		IFCALLRET(window->MonitoredDesktop, result, context, orderInfo, &window->monitored_desktop);
	}

	return result;
}

BOOL update_recv_altsec_window_order(rdpUpdate* update, wStream* s)
{
	UINT16 orderSize;
	rdpWindowUpdate* window = update->window;

	if (Stream_GetRemainingLength(s) < 6)
		return FALSE;

	Stream_Read_UINT16(s, orderSize); /* orderSize (2 bytes) */
	Stream_Read_UINT32(s, window->orderInfo.fieldFlags); /* FieldsPresentFlags (4 bytes) */

	if (window->orderInfo.fieldFlags & WINDOW_ORDER_TYPE_WINDOW)
		return update_recv_window_info_order(update, s, &window->orderInfo);
	else if (window->orderInfo.fieldFlags & WINDOW_ORDER_TYPE_NOTIFY)
		return update_recv_notification_icon_info_order(update, s, &window->orderInfo);
	else if (window->orderInfo.fieldFlags & WINDOW_ORDER_TYPE_DESKTOP)
		return update_recv_desktop_info_order(update, s, &window->orderInfo);

	return TRUE;
}

