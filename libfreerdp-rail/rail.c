/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Remote Applications Integrated Locally (RAIL)
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>

#include "librail.h"

#include <freerdp/rail/rail.h>
#include <freerdp/rail/window_list.h>

static void rail_WindowCreate(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state)
{
	rdpRail* rail = update->context->rail;
	window_list_create(rail->list, orderInfo, window_state);
}

static void rail_WindowUpdate(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state)
{
	rdpRail* rail = update->context->rail;
	window_list_update(rail->list, orderInfo, window_state);
}

static void rail_WindowDelete(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo)
{
	rdpRail* rail = update->context->rail;
	window_list_delete(rail->list, orderInfo);
}

static void rail_WindowIcon(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo, WINDOW_ICON_ORDER* window_icon)
{
	rdpIcon* icon;
	rdpWindow* window;
	rdpRail* rail = update->context->rail;

	if (window_icon->iconInfo->cacheEntry != 0xFFFF)
	{
		/* cache icon */
	}

	window = window_list_get_by_id(rail->list, orderInfo->windowId);

	icon = (rdpIcon*) xzalloc(sizeof(rdpIcon));
	icon->entry = window_icon->iconInfo;
	icon->big = (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ICON_BIG) ? true : false;

	DEBUG_RAIL("Window Icon: %dx%d@%dbpp cbBitsColor:%d cbBitsMask:%d cbColorTable:%d",
			window_icon->iconInfo->width, window_icon->iconInfo->height, window_icon->iconInfo->bpp,
			window_icon->iconInfo->cbBitsColor, window_icon->iconInfo->cbBitsMask, window_icon->iconInfo->cbColorTable);

	if (icon->big)
		window->bigIcon = icon;
	else
		window->smallIcon = icon;

	IFCALL(rail->rail_SetWindowIcon, rail, window, icon);
}

static void rail_WindowCachedIcon(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo, WINDOW_CACHED_ICON_ORDER* window_cached_icon)
{

}

static void rail_NotifyIconCreate(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notify_icon_state)
{

}

static void rail_NotifyIconUpdate(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notify_icon_state)
{

}

static void rail_NotifyIconDelete(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo)
{

}

static void rail_MonitoredDesktop(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo, MONITORED_DESKTOP_ORDER* monitored_desktop)
{

}

static void rail_NonMonitoredDesktop(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo)
{

}

void rail_register_update_callbacks(rdpRail* rail, rdpUpdate* update)
{
	update->WindowCreate = rail_WindowCreate;
	update->WindowUpdate = rail_WindowUpdate;
	update->WindowDelete = rail_WindowDelete;
	update->WindowIcon = rail_WindowIcon;
	update->WindowCachedIcon = rail_WindowCachedIcon;
	update->NotifyIconCreate = rail_NotifyIconCreate;
	update->NotifyIconUpdate = rail_NotifyIconUpdate;
	update->NotifyIconDelete = rail_NotifyIconDelete;
	update->MonitoredDesktop = rail_MonitoredDesktop;
	update->NonMonitoredDesktop = rail_NonMonitoredDesktop;
}

rdpRail* rail_new(rdpSettings* settings)
{
	rdpRail* rail;

	rail = (rdpRail*) xzalloc(sizeof(rdpRail));

	if (rail != NULL)
	{
		rail->settings = settings;
		rail->cache = icon_cache_new(rail);
		rail->list = window_list_new(rail);
		rail->uniconv = freerdp_uniconv_new();
		rail->clrconv = (CLRCONV*) xzalloc(sizeof(CLRCONV));
	}

	return rail;
}

void rail_free(rdpRail* rail)
{
	if (rail != NULL)
	{
		icon_cache_free(rail->cache);
		window_list_free(rail->list);
		freerdp_uniconv_free(rail->uniconv);
		xfree(rail->clrconv);
		xfree(rail);
	}
}
