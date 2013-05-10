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

#include <freerdp/utils/rail.h>

#include "window.h"

BOOL update_read_icon_info(wStream* s, ICON_INFO* icon_info)
{
	if(Stream_GetRemainingLength(s) < 8)
		return FALSE;
	Stream_Read_UINT16(s, icon_info->cacheEntry); /* cacheEntry (2 bytes) */
	Stream_Read_UINT8(s, icon_info->cacheId); /* cacheId (1 byte) */
	Stream_Read_UINT8(s, icon_info->bpp); /* bpp (1 byte) */
	Stream_Read_UINT16(s, icon_info->width); /* width (2 bytes) */
	Stream_Read_UINT16(s, icon_info->height); /* height (2 bytes) */

	/* cbColorTable is only present when bpp is 1, 2 or 4 */
	if (icon_info->bpp == 1 || icon_info->bpp == 2 || icon_info->bpp == 4) {
		if(Stream_GetRemainingLength(s) < 2)
			return FALSE;
		Stream_Read_UINT16(s, icon_info->cbColorTable); /* cbColorTable (2 bytes) */
	} else {
		icon_info->cbColorTable = 0;
	}

	if(Stream_GetRemainingLength(s) < 4)
		return FALSE;
	Stream_Read_UINT16(s, icon_info->cbBitsMask); /* cbBitsMask (2 bytes) */
	Stream_Read_UINT16(s, icon_info->cbBitsColor); /* cbBitsColor (2 bytes) */

	if(Stream_GetRemainingLength(s) < icon_info->cbBitsMask + icon_info->cbBitsColor)
		return FALSE;

	/* bitsMask */
	if (icon_info->bitsMask == NULL)
		icon_info->bitsMask = (BYTE*) malloc(icon_info->cbBitsMask);
	else
		icon_info->bitsMask = (BYTE*) realloc(icon_info->bitsMask, icon_info->cbBitsMask);
	Stream_Read(s, icon_info->bitsMask, icon_info->cbBitsMask);

	/* colorTable */
	if (icon_info->colorTable == NULL)
		icon_info->colorTable = (BYTE*) malloc(icon_info->cbColorTable);
	else
		icon_info->colorTable = (BYTE*) realloc(icon_info->colorTable, icon_info->cbColorTable);
	Stream_Read(s, icon_info->colorTable, icon_info->cbColorTable);

	/* bitsColor */
	if (icon_info->bitsColor == NULL)
		icon_info->bitsColor = (BYTE*) malloc(icon_info->cbBitsColor);
	else
		icon_info->bitsColor = (BYTE*) realloc(icon_info->bitsColor, icon_info->cbBitsColor);
	Stream_Read(s, icon_info->bitsColor, icon_info->cbBitsColor);
	return TRUE;
}

BOOL update_read_cached_icon_info(wStream* s, CACHED_ICON_INFO* cached_icon_info)
{
	if(Stream_GetRemainingLength(s) < 3)
		return FALSE;
	Stream_Read_UINT16(s, cached_icon_info->cacheEntry); /* cacheEntry (2 bytes) */
	Stream_Read_UINT8(s, cached_icon_info->cacheId); /* cacheId (1 byte) */
	return TRUE;
}

BOOL update_read_notify_icon_infotip(wStream* s, NOTIFY_ICON_INFOTIP* notify_icon_infotip)
{
	if(Stream_GetRemainingLength(s) < 8)
		return FALSE;
	Stream_Read_UINT32(s, notify_icon_infotip->timeout); /* timeout (4 bytes) */
	Stream_Read_UINT32(s, notify_icon_infotip->flags); /* infoFlags (4 bytes) */
	return rail_read_unicode_string(s, &notify_icon_infotip->text) && /* infoTipText */
			rail_read_unicode_string(s, &notify_icon_infotip->title); /* title */
}

BOOL update_read_window_state_order(wStream* s, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state)
{
	int i;
	int size;

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_OWNER) {
		if(Stream_GetRemainingLength(s) < 4)
			return FALSE;
		Stream_Read_UINT32(s, window_state->ownerWindowId); /* ownerWindowId (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_STYLE)
	{
		if(Stream_GetRemainingLength(s) < 8)
			return FALSE;
		Stream_Read_UINT32(s, window_state->style); /* style (4 bytes) */
		Stream_Read_UINT32(s, window_state->extendedStyle); /* extendedStyle (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_SHOW) {
		if(Stream_GetRemainingLength(s) < 1)
			return FALSE;
		Stream_Read_UINT8(s, window_state->showState); /* showState (1 byte) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_TITLE) {
		if(!rail_read_unicode_string(s, &window_state->titleInfo)) /* titleInfo */
			return FALSE;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET)
	{
		if(Stream_GetRemainingLength(s) < 4)
			return FALSE;
		Stream_Read_UINT32(s, window_state->clientOffsetX); /* clientOffsetX (4 bytes) */
		Stream_Read_UINT32(s, window_state->clientOffsetY); /* clientOffsetY (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE)
	{
		if(Stream_GetRemainingLength(s) < 4)
			return FALSE;
		Stream_Read_UINT32(s, window_state->clientAreaWidth); /* clientAreaWidth (4 bytes) */
		Stream_Read_UINT32(s, window_state->clientAreaHeight); /* clientAreaHeight (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_RP_CONTENT) {
		if(Stream_GetRemainingLength(s) < 1)
			return FALSE;
		Stream_Read_UINT8(s, window_state->RPContent); /* RPContent (1 byte) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ROOT_PARENT) {
		if(Stream_GetRemainingLength(s) < 4)
			return FALSE;
		Stream_Read_UINT32(s, window_state->rootParentHandle);/* rootParentHandle (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_OFFSET)
	{
		if(Stream_GetRemainingLength(s) < 8)
			return FALSE;
		Stream_Read_UINT32(s, window_state->windowOffsetX); /* windowOffsetX (4 bytes) */
		Stream_Read_UINT32(s, window_state->windowOffsetY); /* windowOffsetY (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_CLIENT_DELTA)
	{
		if(Stream_GetRemainingLength(s) < 8)
			return FALSE;
		Stream_Read_UINT32(s, window_state->windowClientDeltaX); /* windowClientDeltaX (4 bytes) */
		Stream_Read_UINT32(s, window_state->windowClientDeltaY); /* windowClientDeltaY (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE)
	{
		if(Stream_GetRemainingLength(s) < 8)
			return FALSE;
		Stream_Read_UINT32(s, window_state->windowWidth); /* windowWidth (4 bytes) */
		Stream_Read_UINT32(s, window_state->windowHeight); /* windowHeight (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_RECTS)
	{
		if(Stream_GetRemainingLength(s) < 2)
			return FALSE;
		Stream_Read_UINT16(s, window_state->numWindowRects); /* numWindowRects (2 bytes) */

		size = sizeof(RECTANGLE_16) * window_state->numWindowRects;
		window_state->windowRects = (RECTANGLE_16*) malloc(size);

		if(Stream_GetRemainingLength(s) < 8 * window_state->numWindowRects)
			return FALSE;

		/* windowRects */
		for (i = 0; i < (int) window_state->numWindowRects; i++)
		{
			Stream_Read_UINT16(s, window_state->windowRects[i].left); /* left (2 bytes) */
			Stream_Read_UINT16(s, window_state->windowRects[i].top); /* top (2 bytes) */
			Stream_Read_UINT16(s, window_state->windowRects[i].right); /* right (2 bytes) */
			Stream_Read_UINT16(s, window_state->windowRects[i].bottom); /* bottom (2 bytes) */
		}
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VIS_OFFSET)
	{
		if(Stream_GetRemainingLength(s) < 4)
			return FALSE;
		Stream_Read_UINT32(s, window_state->visibleOffsetX); /* visibleOffsetX (4 bytes) */
		Stream_Read_UINT32(s, window_state->visibleOffsetY); /* visibleOffsetY (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY)
	{
		if(Stream_GetRemainingLength(s) < 2)
			return FALSE;
		Stream_Read_UINT16(s, window_state->numVisibilityRects); /* numVisibilityRects (2 bytes) */

		size = sizeof(RECTANGLE_16) * window_state->numVisibilityRects;
		window_state->visibilityRects = (RECTANGLE_16*) malloc(size);

		if(Stream_GetRemainingLength(s) < window_state->numVisibilityRects * 8)
			return FALSE;

		/* visibilityRects */
		for (i = 0; i < (int) window_state->numVisibilityRects; i++)
		{
			Stream_Read_UINT16(s, window_state->visibilityRects[i].left); /* left (2 bytes) */
			Stream_Read_UINT16(s, window_state->visibilityRects[i].top); /* top (2 bytes) */
			Stream_Read_UINT16(s, window_state->visibilityRects[i].right); /* right (2 bytes) */
			Stream_Read_UINT16(s, window_state->visibilityRects[i].bottom); /* bottom (2 bytes) */
		}
	}
	return TRUE;
}

BOOL update_read_window_icon_order(wStream* s, WINDOW_ORDER_INFO* orderInfo, WINDOW_ICON_ORDER* window_icon)
{
	window_icon->iconInfo = (ICON_INFO*) malloc(sizeof(ICON_INFO));
	ZeroMemory(window_icon->iconInfo, sizeof(ICON_INFO));

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

	if(Stream_GetRemainingLength(s) < 4)
		return FALSE;
	Stream_Read_UINT32(s, orderInfo->windowId); /* windowId (4 bytes) */

	if (orderInfo->fieldFlags & WINDOW_ORDER_ICON)
	{
		DEBUG_WND("Window Icon Order");
		if(!update_read_window_icon_order(s, orderInfo, &window->window_icon))
			return FALSE;
		IFCALL(window->WindowIcon, context, orderInfo, &window->window_icon);
	}
	else if (orderInfo->fieldFlags & WINDOW_ORDER_CACHED_ICON)
	{
		DEBUG_WND("Window Cached Icon Order");
		if(!update_read_window_cached_icon_order(s, orderInfo, &window->window_cached_icon))
			return FALSE;
		IFCALL(window->WindowCachedIcon, context, orderInfo, &window->window_cached_icon);
	}
	else if (orderInfo->fieldFlags & WINDOW_ORDER_STATE_DELETED)
	{
		DEBUG_WND("Window Deleted Order");
		update_read_window_delete_order(s, orderInfo);
		IFCALL(window->WindowDelete, context, orderInfo);
	}
	else
	{
		DEBUG_WND("Window State Order");
		if(!update_read_window_state_order(s, orderInfo, &window->window_state))
			return FALSE;

		if (orderInfo->fieldFlags & WINDOW_ORDER_STATE_NEW)
			IFCALL(window->WindowCreate, context, orderInfo, &window->window_state);
		else
			IFCALL(window->WindowUpdate, context, orderInfo, &window->window_state);
	}
	return TRUE;
}

BOOL update_read_notification_icon_state_order(wStream* s, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notify_icon_state)
{
	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_VERSION) {
		if(Stream_GetRemainingLength(s) < 4)
			return FALSE;
		Stream_Read_UINT32(s, notify_icon_state->version); /* version (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_TIP) {
		if(!rail_read_unicode_string(s, &notify_icon_state->toolTip)) /* toolTip (UNICODE_STRING) */
			return FALSE;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_INFO_TIP) {
		if(!update_read_notify_icon_infotip(s, &notify_icon_state->infoTip)) /* infoTip (NOTIFY_ICON_INFOTIP) */
			return FALSE;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_STATE) {
		if(Stream_GetRemainingLength(s) < 4)
			return FALSE;
		Stream_Read_UINT32(s, notify_icon_state->state); /* state (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_ICON) {
		if(!update_read_icon_info(s, &notify_icon_state->icon)) /* icon (ICON_INFO) */
			return FALSE;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_CACHED_ICON) {
		if(!update_read_cached_icon_info(s, &notify_icon_state->cachedIcon)) /* cachedIcon (CACHED_ICON_INFO) */
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

	if(Stream_GetRemainingLength(s) < 8)
		return FALSE;
	Stream_Read_UINT32(s, orderInfo->windowId); /* windowId (4 bytes) */
	Stream_Read_UINT32(s, orderInfo->notifyIconId); /* notifyIconId (4 bytes) */

	if (orderInfo->fieldFlags & WINDOW_ORDER_STATE_DELETED)
	{
		DEBUG_WND("Delete Notification Icon Deleted Order");
		update_read_notification_icon_delete_order(s, orderInfo);
		IFCALL(window->NotifyIconDelete, context, orderInfo);
	}
	else
	{
		DEBUG_WND("Notification Icon State Order");
		if(!update_read_notification_icon_state_order(s, orderInfo, &window->notify_icon_state))
			return FALSE;

		if (orderInfo->fieldFlags & WINDOW_ORDER_STATE_NEW)
			IFCALL(window->NotifyIconCreate, context, orderInfo, &window->notify_icon_state);
		else
			IFCALL(window->NotifyIconUpdate, context, orderInfo, &window->notify_icon_state);
	}
	return TRUE;
}

BOOL update_read_desktop_actively_monitored_order(wStream* s, WINDOW_ORDER_INFO* orderInfo, MONITORED_DESKTOP_ORDER* monitored_desktop)
{
	int i;
	int size;

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_DESKTOP_ACTIVE_WND) {
		if(Stream_GetRemainingLength(s) < 4)
			return FALSE;
		Stream_Read_UINT32(s, monitored_desktop->activeWindowId); /* activeWindowId (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_DESKTOP_ZORDER)
	{
		if(Stream_GetRemainingLength(s) < 1)
			return FALSE;
		Stream_Read_UINT8(s, monitored_desktop->numWindowIds); /* numWindowIds (1 byte) */

		if(Stream_GetRemainingLength(s) < 4 * monitored_desktop->numWindowIds)
			return FALSE;

		size = sizeof(UINT32) * monitored_desktop->numWindowIds;

		if (monitored_desktop->windowIds == NULL)
			monitored_desktop->windowIds = (UINT32*) malloc(size);
		else
			monitored_desktop->windowIds = (UINT32*) realloc(monitored_desktop->windowIds, size);

		/* windowIds */
		for (i = 0; i < (int) monitored_desktop->numWindowIds; i++)
		{
			Stream_Read_UINT32(s, monitored_desktop->windowIds[i]);
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

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_DESKTOP_NONE)
	{
		DEBUG_WND("Non-Monitored Desktop Order");
		update_read_desktop_non_monitored_order(s, orderInfo);
		IFCALL(window->NonMonitoredDesktop, context, orderInfo);
	}
	else
	{
		DEBUG_WND("Actively Monitored Desktop Order");
		if(!update_read_desktop_actively_monitored_order(s, orderInfo, &window->monitored_desktop))
			return FALSE;
		IFCALL(window->MonitoredDesktop, context, orderInfo, &window->monitored_desktop);
	}
	return TRUE;
}

BOOL update_recv_altsec_window_order(rdpUpdate* update, wStream* s)
{
	UINT16 orderSize;
	rdpWindowUpdate* window = update->window;

	if(Stream_GetRemainingLength(s) < 6)
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

