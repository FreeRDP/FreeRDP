/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include <winpr/stream.h>

#include "librail.h"

#include <freerdp/rail/rail.h>
#include <freerdp/rail/window_list.h>

static void rail_WindowCreate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state)
{
	rdpRail* rail = context->rail;
	window_list_create(rail->list, orderInfo, window_state);
}

static void rail_WindowUpdate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state)
{
	rdpRail* rail = context->rail;
	window_list_update(rail->list, orderInfo, window_state);
}

static void rail_WindowDelete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	rdpRail* rail = context->rail;
	window_list_delete(rail->list, orderInfo);
}

static void rail_WindowIcon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_ICON_ORDER* window_icon)
{
	rdpIcon* icon;
	rdpWindow* window;
	rdpRail* rail = context->rail;

	if (window_icon->iconInfo->cacheEntry != 0xFFFF)
	{
		/* cache icon */
	}

	window = window_list_get_by_id(rail->list, orderInfo->windowId);
	if (!window)
		return ;

	icon = (rdpIcon*) malloc(sizeof(rdpIcon));
	ZeroMemory(icon, sizeof(rdpIcon));

	icon->entry = window_icon->iconInfo;
	icon->big = (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ICON_BIG) ? TRUE : FALSE;

	DEBUG_RAIL("Window Icon: %dx%d@%dbpp cbBitsColor:%d cbBitsMask:%d cbColorTable:%d",
			window_icon->iconInfo->width, window_icon->iconInfo->height, window_icon->iconInfo->bpp,
			window_icon->iconInfo->cbBitsColor, window_icon->iconInfo->cbBitsMask, window_icon->iconInfo->cbColorTable);

	if (icon->big)
		window->bigIcon = icon;
	else
		window->smallIcon = icon;

	IFCALL(rail->rail_SetWindowIcon, rail, window, icon);
}

static void rail_WindowCachedIcon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_CACHED_ICON_ORDER* window_cached_icon)
{

}

static void rail_NotifyIconCreate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notify_icon_state)
{

}

static void rail_NotifyIconUpdate(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notify_icon_state)
{

}

static void rail_NotifyIconDelete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{

}

static void rail_MonitoredDesktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, MONITORED_DESKTOP_ORDER* monitored_desktop)
{

}

//This is used to switch FreeRDP back to showing the full desktop under remote app mode
//to handle cases where the screen is locked, etc. The rail server informs us that it is
//no longer monitoring the desktop. Once the desktop becomes monitored again. The full desktop 
//window will be automatically destroyed and we will switch back into remote app mode.
static void rail_NonMonitoredDesktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	rdpWindow* window;
        rdpRail* rail = context->rail;

        window = window_list_get_by_id(rail->list, orderInfo->windowId);

        IFCALL(rail->rail_DesktopNonMonitored, rail, window);           

        window_list_clear(rail->list);
}



void rail_register_update_callbacks(rdpRail* rail, rdpUpdate* update)
{
	rdpWindowUpdate* window = update->window;

	window->WindowCreate = rail_WindowCreate;
	window->WindowUpdate = rail_WindowUpdate;
	window->WindowDelete = rail_WindowDelete;
	window->WindowIcon = rail_WindowIcon;
	window->WindowCachedIcon = rail_WindowCachedIcon;
	window->NotifyIconCreate = rail_NotifyIconCreate;
	window->NotifyIconUpdate = rail_NotifyIconUpdate;
	window->NotifyIconDelete = rail_NotifyIconDelete;
	window->MonitoredDesktop = rail_MonitoredDesktop;
	window->NonMonitoredDesktop = rail_NonMonitoredDesktop;
}

rdpRail* rail_new(rdpSettings* settings)
{
	rdpRail* rail;

	rail = (rdpRail*) malloc(sizeof(rdpRail));

	if (rail != NULL)
	{
		ZeroMemory(rail, sizeof(rdpRail));

		rail->settings = settings;
		rail->cache = icon_cache_new(rail);
		rail->list = window_list_new(rail);

		rail->clrconv = (CLRCONV*) malloc(sizeof(CLRCONV));
		ZeroMemory(rail->clrconv, sizeof(CLRCONV));
	}

	return rail;
}

void rail_free(rdpRail* rail)
{
	if (rail != NULL)
	{
		icon_cache_free(rail->cache);
		window_list_free(rail->list);
		free(rail->clrconv);
		free(rail);
	}
}
