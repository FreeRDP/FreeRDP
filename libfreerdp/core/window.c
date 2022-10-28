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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/assert.h>

#include <freerdp/log.h>

#include "window.h"

#define TAG FREERDP_TAG("core.window")

static void update_free_window_icon_info(ICON_INFO* iconInfo);

BOOL rail_read_unicode_string(wStream* s, RAIL_UNICODE_STRING* unicode_string)
{
	UINT16 new_len;
	BYTE* new_str;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
		return FALSE;

	Stream_Read_UINT16(s, new_len); /* cbString (2 bytes) */

	if (!Stream_CheckAndLogRequiredLength(TAG, s, new_len))
		return FALSE;

	if (!new_len)
	{
		free(unicode_string->string);
		unicode_string->string = NULL;
		unicode_string->length = 0;
		return TRUE;
	}

	new_str = (BYTE*)realloc(unicode_string->string, new_len);

	if (!new_str)
	{
		free(unicode_string->string);
		unicode_string->string = NULL;
		return FALSE;
	}

	unicode_string->string = new_str;
	unicode_string->length = new_len;
	Stream_Read(s, unicode_string->string, unicode_string->length);
	return TRUE;
}

BOOL utf8_string_to_rail_string(const char* string, RAIL_UNICODE_STRING* unicode_string)
{
	WCHAR* buffer = NULL;
	size_t len = 0;
	free(unicode_string->string);
	unicode_string->string = NULL;
	unicode_string->length = 0;

	if (!string || strlen(string) < 1)
		return TRUE;

	buffer = ConvertUtf8ToWCharAlloc(string, &len);

	if (!buffer || (len * sizeof(WCHAR) > UINT16_MAX))
	{
		free(buffer);
		return FALSE;
	}

	unicode_string->string = (BYTE*)buffer;
	unicode_string->length = (UINT16)len * sizeof(WCHAR);
	return TRUE;
}

/* See [MS-RDPERP] 2.2.1.2.3 Icon Info (TS_ICON_INFO) */
static BOOL update_read_icon_info(wStream* s, ICON_INFO* iconInfo)
{
	BYTE* newBitMask = NULL;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	Stream_Read_UINT16(s, iconInfo->cacheEntry); /* cacheEntry (2 bytes) */
	Stream_Read_UINT8(s, iconInfo->cacheId);     /* cacheId (1 byte) */
	Stream_Read_UINT8(s, iconInfo->bpp);         /* bpp (1 byte) */

	if ((iconInfo->bpp < 1) || (iconInfo->bpp > 32))
	{
		WLog_ERR(TAG, "invalid bpp value %" PRIu32 "", iconInfo->bpp);
		return FALSE;
	}

	Stream_Read_UINT16(s, iconInfo->width);  /* width (2 bytes) */
	Stream_Read_UINT16(s, iconInfo->height); /* height (2 bytes) */

	/* cbColorTable is only present when bpp is 1, 4 or 8 */
	switch (iconInfo->bpp)
	{
		case 1:
		case 4:
		case 8:
			if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
				return FALSE;

			Stream_Read_UINT16(s, iconInfo->cbColorTable); /* cbColorTable (2 bytes) */
			break;

		default:
			iconInfo->cbColorTable = 0;
			break;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT16(s, iconInfo->cbBitsMask);  /* cbBitsMask (2 bytes) */
	Stream_Read_UINT16(s, iconInfo->cbBitsColor); /* cbBitsColor (2 bytes) */

	/* bitsMask */
	if (iconInfo->cbBitsMask > 0)
	{
		newBitMask = (BYTE*)realloc(iconInfo->bitsMask, iconInfo->cbBitsMask);

		if (!newBitMask)
		{
			free(iconInfo->bitsMask);
			iconInfo->bitsMask = NULL;
			return FALSE;
		}

		iconInfo->bitsMask = newBitMask;
		if (!Stream_CheckAndLogRequiredLength(TAG, s, iconInfo->cbBitsMask))
			return FALSE;
		Stream_Read(s, iconInfo->bitsMask, iconInfo->cbBitsMask);
	}
	else
	{
		free(iconInfo->bitsMask);
		iconInfo->bitsMask = NULL;
		iconInfo->cbBitsMask = 0;
	}

	/* colorTable */
	if (iconInfo->cbColorTable > 0)
	{
		BYTE* new_tab;
		new_tab = (BYTE*)realloc(iconInfo->colorTable, iconInfo->cbColorTable);

		if (!new_tab)
		{
			free(iconInfo->colorTable);
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
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, iconInfo->cbColorTable))
			return FALSE;
		Stream_Read(s, iconInfo->colorTable, iconInfo->cbColorTable);
	}

	/* bitsColor */
	if (iconInfo->cbBitsColor > 0)
	{
		newBitMask = (BYTE*)realloc(iconInfo->bitsColor, iconInfo->cbBitsColor);

		if (!newBitMask)
		{
			free(iconInfo->bitsColor);
			iconInfo->bitsColor = NULL;
			return FALSE;
		}

		iconInfo->bitsColor = newBitMask;
		if (!Stream_CheckAndLogRequiredLength(TAG, s, iconInfo->cbBitsColor))
			return FALSE;
		Stream_Read(s, iconInfo->bitsColor, iconInfo->cbBitsColor);
	}
	else
	{
		free(iconInfo->bitsColor);
		iconInfo->bitsColor = NULL;
		iconInfo->cbBitsColor = 0;
	}
	return TRUE;
}

static BOOL update_read_cached_icon_info(wStream* s, CACHED_ICON_INFO* cachedIconInfo)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 3))
		return FALSE;

	Stream_Read_UINT16(s, cachedIconInfo->cacheEntry); /* cacheEntry (2 bytes) */
	Stream_Read_UINT8(s, cachedIconInfo->cacheId);     /* cacheId (1 byte) */
	return TRUE;
}

static BOOL update_read_notify_icon_infotip(wStream* s, NOTIFY_ICON_INFOTIP* notifyIconInfoTip)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	Stream_Read_UINT32(s, notifyIconInfoTip->timeout);              /* timeout (4 bytes) */
	Stream_Read_UINT32(s, notifyIconInfoTip->flags);                /* infoFlags (4 bytes) */
	return rail_read_unicode_string(s, &notifyIconInfoTip->text) && /* infoTipText */
	       rail_read_unicode_string(s, &notifyIconInfoTip->title);  /* title */
}

static BOOL update_read_window_state_order(wStream* s, WINDOW_ORDER_INFO* orderInfo,
                                           WINDOW_STATE_ORDER* windowState)
{
	UINT32 i;
	size_t size;
	RECTANGLE_16* newRect;

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_OWNER)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
			return FALSE;

		Stream_Read_UINT32(s, windowState->ownerWindowId); /* ownerWindowId (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_STYLE)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
			return FALSE;

		Stream_Read_UINT32(s, windowState->style);         /* style (4 bytes) */
		Stream_Read_UINT32(s, windowState->extendedStyle); /* extendedStyle (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_SHOW)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
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
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
			return FALSE;

		Stream_Read_INT32(s, windowState->clientOffsetX); /* clientOffsetX (4 bytes) */
		Stream_Read_INT32(s, windowState->clientOffsetY); /* clientOffsetY (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
			return FALSE;

		Stream_Read_UINT32(s, windowState->clientAreaWidth);  /* clientAreaWidth (4 bytes) */
		Stream_Read_UINT32(s, windowState->clientAreaHeight); /* clientAreaHeight (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_RESIZE_MARGIN_X)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
			return FALSE;

		Stream_Read_UINT32(s, windowState->resizeMarginLeft);
		Stream_Read_UINT32(s, windowState->resizeMarginRight);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_RESIZE_MARGIN_Y)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
			return FALSE;

		Stream_Read_UINT32(s, windowState->resizeMarginTop);
		Stream_Read_UINT32(s, windowState->resizeMarginBottom);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_RP_CONTENT)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
			return FALSE;

		Stream_Read_UINT8(s, windowState->RPContent); /* RPContent (1 byte) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ROOT_PARENT)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
			return FALSE;

		Stream_Read_UINT32(s, windowState->rootParentHandle); /* rootParentHandle (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_OFFSET)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
			return FALSE;

		Stream_Read_INT32(s, windowState->windowOffsetX); /* windowOffsetX (4 bytes) */
		Stream_Read_INT32(s, windowState->windowOffsetY); /* windowOffsetY (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_CLIENT_DELTA)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
			return FALSE;

		Stream_Read_INT32(s, windowState->windowClientDeltaX); /* windowClientDeltaX (4 bytes) */
		Stream_Read_INT32(s, windowState->windowClientDeltaY); /* windowClientDeltaY (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
			return FALSE;

		Stream_Read_UINT32(s, windowState->windowWidth);  /* windowWidth (4 bytes) */
		Stream_Read_UINT32(s, windowState->windowHeight); /* windowHeight (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_RECTS)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
			return FALSE;

		Stream_Read_UINT16(s, windowState->numWindowRects); /* numWindowRects (2 bytes) */

		if (windowState->numWindowRects > 0)
		{
			size = sizeof(RECTANGLE_16) * windowState->numWindowRects;
			newRect = (RECTANGLE_16*)realloc(windowState->windowRects, size);

			if (!newRect)
			{
				free(windowState->windowRects);
				windowState->windowRects = NULL;
				return FALSE;
			}

			windowState->windowRects = newRect;

			if (!Stream_CheckAndLogRequiredLength(TAG, s, 8ull * windowState->numWindowRects))
				return FALSE;

			/* windowRects */
			for (i = 0; i < windowState->numWindowRects; i++)
			{
				Stream_Read_UINT16(s, windowState->windowRects[i].left);   /* left (2 bytes) */
				Stream_Read_UINT16(s, windowState->windowRects[i].top);    /* top (2 bytes) */
				Stream_Read_UINT16(s, windowState->windowRects[i].right);  /* right (2 bytes) */
				Stream_Read_UINT16(s, windowState->windowRects[i].bottom); /* bottom (2 bytes) */
			}
		}
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VIS_OFFSET)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
			return FALSE;

		Stream_Read_UINT32(s, windowState->visibleOffsetX); /* visibleOffsetX (4 bytes) */
		Stream_Read_UINT32(s, windowState->visibleOffsetY); /* visibleOffsetY (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
			return FALSE;

		Stream_Read_UINT16(s, windowState->numVisibilityRects); /* numVisibilityRects (2 bytes) */

		if (windowState->numVisibilityRects != 0)
		{
			size = sizeof(RECTANGLE_16) * windowState->numVisibilityRects;
			newRect = (RECTANGLE_16*)realloc(windowState->visibilityRects, size);

			if (!newRect)
			{
				free(windowState->visibilityRects);
				windowState->visibilityRects = NULL;
				return FALSE;
			}

			windowState->visibilityRects = newRect;

			if (!Stream_CheckAndLogRequiredLength(TAG, s, 8ull * windowState->numVisibilityRects))
				return FALSE;

			/* visibilityRects */
			for (i = 0; i < windowState->numVisibilityRects; i++)
			{
				Stream_Read_UINT16(s, windowState->visibilityRects[i].left);  /* left (2 bytes) */
				Stream_Read_UINT16(s, windowState->visibilityRects[i].top);   /* top (2 bytes) */
				Stream_Read_UINT16(s, windowState->visibilityRects[i].right); /* right (2 bytes) */
				Stream_Read_UINT16(s,
				                   windowState->visibilityRects[i].bottom); /* bottom (2 bytes) */
			}
		}
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_OVERLAY_DESCRIPTION)
	{
		if (!rail_read_unicode_string(s, &windowState->OverlayDescription))
			return FALSE;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ICON_OVERLAY_NULL)
	{
		/* no data to be read here */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_TASKBAR_BUTTON)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
			return FALSE;

		Stream_Read_UINT8(s, windowState->TaskbarButton);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ENFORCE_SERVER_ZORDER)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
			return FALSE;

		Stream_Read_UINT8(s, windowState->EnforceServerZOrder);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_APPBAR_STATE)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
			return FALSE;

		Stream_Read_UINT8(s, windowState->AppBarState);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_APPBAR_EDGE)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
			return FALSE;

		Stream_Read_UINT8(s, windowState->AppBarEdge);
	}

	return TRUE;
}

static BOOL update_read_window_icon_order(wStream* s, WINDOW_ORDER_INFO* orderInfo,
                                          WINDOW_ICON_ORDER* window_icon)
{
	WINPR_UNUSED(orderInfo);
	window_icon->iconInfo = (ICON_INFO*)calloc(1, sizeof(ICON_INFO));

	if (!window_icon->iconInfo)
		return FALSE;

	return update_read_icon_info(s, window_icon->iconInfo); /* iconInfo (ICON_INFO) */
}

static BOOL update_read_window_cached_icon_order(wStream* s, WINDOW_ORDER_INFO* orderInfo,
                                                 WINDOW_CACHED_ICON_ORDER* window_cached_icon)
{
	WINPR_UNUSED(orderInfo);
	return update_read_cached_icon_info(
	    s, &window_cached_icon->cachedIcon); /* cachedIcon (CACHED_ICON_INFO) */
}

static void update_read_window_delete_order(wStream* s, WINDOW_ORDER_INFO* orderInfo)
{
	/* window deletion event */
}

static BOOL window_order_supported(const rdpSettings* settings, UINT32 fieldFlags)
{
	const UINT32 mask = (WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE | WINDOW_ORDER_FIELD_RP_CONTENT |
	                     WINDOW_ORDER_FIELD_ROOT_PARENT);
	BOOL dresult;

	if (!settings)
		return FALSE;

	/* See [MS-RDPERP] 2.2.1.1.2 Window List Capability Set */
	dresult = settings->AllowUnanouncedOrdersFromServer;

	switch (settings->RemoteWndSupportLevel)
	{
		case WINDOW_LEVEL_SUPPORTED_EX:
			return TRUE;

		case WINDOW_LEVEL_SUPPORTED:
			return ((fieldFlags & mask) == 0) || dresult;

		case WINDOW_LEVEL_NOT_SUPPORTED:
			return dresult;

		default:
			return dresult;
	}
}

#define DUMP_APPEND(buffer, size, ...)            \
	do                                            \
	{                                             \
		char* b = (buffer);                       \
		size_t s = (size);                        \
		size_t pos = strnlen(b, s);               \
		_snprintf(&b[pos], s - pos, __VA_ARGS__); \
	} while (0)

static void dump_window_state_order(wLog* log, const char* msg, const WINDOW_ORDER_INFO* order,
                                    const WINDOW_STATE_ORDER* state)
{
	char buffer[3000] = { 0 };
	const size_t bufferSize = sizeof(buffer) - 1;

	_snprintf(buffer, bufferSize, "%s windowId=0x%" PRIu32 "", msg, order->windowId);

	if (order->fieldFlags & WINDOW_ORDER_FIELD_OWNER)
		DUMP_APPEND(buffer, bufferSize, " owner=0x%" PRIx32 "", state->ownerWindowId);
	if (order->fieldFlags & WINDOW_ORDER_FIELD_STYLE)
	{
		DUMP_APPEND(buffer, bufferSize, " [ex]style=<0x%" PRIx32 ", 0x%" PRIx32 "", state->style,
		            state->extendedStyle);
		if (state->style & WS_POPUP)
			DUMP_APPEND(buffer, bufferSize, " popup");
		if (state->style & WS_VISIBLE)
			DUMP_APPEND(buffer, bufferSize, " visible");
		if (state->style & WS_THICKFRAME)
			DUMP_APPEND(buffer, bufferSize, " thickframe");
		if (state->style & WS_BORDER)
			DUMP_APPEND(buffer, bufferSize, " border");
		if (state->style & WS_CAPTION)
			DUMP_APPEND(buffer, bufferSize, " caption");

		if (state->extendedStyle & WS_EX_NOACTIVATE)
			DUMP_APPEND(buffer, bufferSize, " noactivate");
		if (state->extendedStyle & WS_EX_TOOLWINDOW)
			DUMP_APPEND(buffer, bufferSize, " toolWindow");
		if (state->extendedStyle & WS_EX_TOPMOST)
			DUMP_APPEND(buffer, bufferSize, " topMost");

		DUMP_APPEND(buffer, bufferSize, ">");
	}

	if (order->fieldFlags & WINDOW_ORDER_FIELD_SHOW)
	{
		const char* showStr;
		switch (state->showState)
		{
			case 0:
				showStr = "hidden";
				break;
			case 2:
				showStr = "minimized";
				break;
			case 3:
				showStr = "maximized";
				break;
			case 5:
				showStr = "current";
				break;
			default:
				showStr = "<unknown>";
				break;
		}
		DUMP_APPEND(buffer, bufferSize, " show=%s", showStr);
	}

	if (order->fieldFlags & WINDOW_ORDER_FIELD_TITLE)
		DUMP_APPEND(buffer, bufferSize, " title");
	if (order->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET)
		DUMP_APPEND(buffer, bufferSize, " clientOffset=(%" PRId32 ",%" PRId32 ")",
		            state->clientOffsetX, state->clientOffsetY);
	if (order->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE)
		DUMP_APPEND(buffer, bufferSize, " clientAreaWidth=%" PRIu32 " clientAreaHeight=%" PRIu32 "",
		            state->clientAreaWidth, state->clientAreaHeight);
	if (order->fieldFlags & WINDOW_ORDER_FIELD_RESIZE_MARGIN_X)
		DUMP_APPEND(buffer, bufferSize,
		            " resizeMarginLeft=%" PRIu32 " resizeMarginRight=%" PRIu32 "",
		            state->resizeMarginLeft, state->resizeMarginRight);
	if (order->fieldFlags & WINDOW_ORDER_FIELD_RESIZE_MARGIN_Y)
		DUMP_APPEND(buffer, bufferSize,
		            " resizeMarginTop=%" PRIu32 " resizeMarginBottom=%" PRIu32 "",
		            state->resizeMarginTop, state->resizeMarginBottom);
	if (order->fieldFlags & WINDOW_ORDER_FIELD_RP_CONTENT)
		DUMP_APPEND(buffer, bufferSize, " rpContent=0x%" PRIx32 "", state->RPContent);
	if (order->fieldFlags & WINDOW_ORDER_FIELD_ROOT_PARENT)
		DUMP_APPEND(buffer, bufferSize, " rootParent=0x%" PRIx32 "", state->rootParentHandle);
	if (order->fieldFlags & WINDOW_ORDER_FIELD_WND_OFFSET)
		DUMP_APPEND(buffer, bufferSize, " windowOffset=(%" PRId32 ",%" PRId32 ")",
		            state->windowOffsetX, state->windowOffsetY);
	if (order->fieldFlags & WINDOW_ORDER_FIELD_WND_CLIENT_DELTA)
		DUMP_APPEND(buffer, bufferSize, " windowClientDelta=(%" PRId32 ",%" PRId32 ")",
		            state->windowClientDeltaX, state->windowClientDeltaY);
	if (order->fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE)
		DUMP_APPEND(buffer, bufferSize, " windowWidth=%" PRIu32 " windowHeight=%" PRIu32 "",
		            state->windowWidth, state->windowHeight);

	if (order->fieldFlags & WINDOW_ORDER_FIELD_WND_RECTS)
	{
		UINT32 i;
		DUMP_APPEND(buffer, bufferSize, " windowRects=(");
		for (i = 0; i < state->numWindowRects; i++)
		{
			DUMP_APPEND(buffer, bufferSize, "(%" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16 ")",
			            state->windowRects[i].left, state->windowRects[i].top,
			            state->windowRects[i].right, state->windowRects[i].bottom);
		}
		DUMP_APPEND(buffer, bufferSize, ")");
	}

	if (order->fieldFlags & WINDOW_ORDER_FIELD_VIS_OFFSET)
		DUMP_APPEND(buffer, bufferSize, " visibleOffset=(%" PRId32 ",%" PRId32 ")",
		            state->visibleOffsetX, state->visibleOffsetY);

	if (order->fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY)
	{
		UINT32 i;
		DUMP_APPEND(buffer, bufferSize, " visibilityRects=(");
		for (i = 0; i < state->numVisibilityRects; i++)
		{
			DUMP_APPEND(buffer, bufferSize, "(%" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16 ")",
			            state->visibilityRects[i].left, state->visibilityRects[i].top,
			            state->visibilityRects[i].right, state->visibilityRects[i].bottom);
		}
		DUMP_APPEND(buffer, bufferSize, ")");
	}

	if (order->fieldFlags & WINDOW_ORDER_FIELD_OVERLAY_DESCRIPTION)
		DUMP_APPEND(buffer, bufferSize, " overlayDescr");

	if (order->fieldFlags & WINDOW_ORDER_FIELD_ICON_OVERLAY_NULL)
		DUMP_APPEND(buffer, bufferSize, " iconOverlayNull");

	if (order->fieldFlags & WINDOW_ORDER_FIELD_TASKBAR_BUTTON)
		DUMP_APPEND(buffer, bufferSize, " taskBarButton=0x%" PRIx8 "", state->TaskbarButton);

	if (order->fieldFlags & WINDOW_ORDER_FIELD_ENFORCE_SERVER_ZORDER)
		DUMP_APPEND(buffer, bufferSize, " enforceServerZOrder=0x%" PRIx8 "",
		            state->EnforceServerZOrder);
	if (order->fieldFlags & WINDOW_ORDER_FIELD_APPBAR_STATE)
		DUMP_APPEND(buffer, bufferSize, " appBarState=0x%" PRIx8 "", state->AppBarState);
	if (order->fieldFlags & WINDOW_ORDER_FIELD_APPBAR_EDGE)
	{
		const char* appBarEdgeStr;
		switch (state->AppBarEdge)
		{
			case 0:
				appBarEdgeStr = "left";
				break;
			case 1:
				appBarEdgeStr = "top";
				break;
			case 2:
				appBarEdgeStr = "right";
				break;
			case 3:
				appBarEdgeStr = "bottom";
				break;
			default:
				appBarEdgeStr = "<unknown>";
				break;
		}
		DUMP_APPEND(buffer, bufferSize, " appBarEdge=%s", appBarEdgeStr);
	}

	WLog_Print(log, WLOG_DEBUG, buffer);
}

static BOOL update_recv_window_info_order(rdpUpdate* update, wStream* s,
                                          WINDOW_ORDER_INFO* orderInfo)
{
	rdp_update_internal* up = update_cast(update);
	rdpContext* context = update->context;
	rdpWindowUpdate* window = update->window;

	BOOL result = TRUE;

	WINPR_ASSERT(s);
	WINPR_ASSERT(context);
	WINPR_ASSERT(window);
	WINPR_ASSERT(orderInfo);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, orderInfo->windowId); /* windowId (4 bytes) */

	if (orderInfo->fieldFlags & WINDOW_ORDER_ICON)
	{
		WINDOW_ICON_ORDER window_icon = { 0 };
		result = update_read_window_icon_order(s, orderInfo, &window_icon);

		if (result)
		{
			WLog_Print(up->log, WLOG_DEBUG, "WindowIcon windowId=0x%" PRIx32 "",
			           orderInfo->windowId);
			IFCALLRET(window->WindowIcon, result, context, orderInfo, &window_icon);
		}

		update_free_window_icon_info(window_icon.iconInfo);
		free(window_icon.iconInfo);
	}
	else if (orderInfo->fieldFlags & WINDOW_ORDER_CACHED_ICON)
	{
		WINDOW_CACHED_ICON_ORDER window_cached_icon = { 0 };
		result = update_read_window_cached_icon_order(s, orderInfo, &window_cached_icon);

		if (result)
		{
			WLog_Print(up->log, WLOG_DEBUG, "WindowCachedIcon windowId=0x%" PRIx32 "",
			           orderInfo->windowId);
			IFCALLRET(window->WindowCachedIcon, result, context, orderInfo, &window_cached_icon);
		}
	}
	else if (orderInfo->fieldFlags & WINDOW_ORDER_STATE_DELETED)
	{
		update_read_window_delete_order(s, orderInfo);
		WLog_Print(up->log, WLOG_DEBUG, "WindowDelete windowId=0x%" PRIx32 "", orderInfo->windowId);
		IFCALLRET(window->WindowDelete, result, context, orderInfo);
	}
	else
	{
		WINDOW_STATE_ORDER windowState = { 0 };
		result = update_read_window_state_order(s, orderInfo, &windowState);

		if (result)
		{
			if (orderInfo->fieldFlags & WINDOW_ORDER_STATE_NEW)
			{
				dump_window_state_order(up->log, "WindowCreate", orderInfo, &windowState);
				IFCALLRET(window->WindowCreate, result, context, orderInfo, &windowState);
			}
			else
			{
				dump_window_state_order(up->log, "WindowUpdate", orderInfo, &windowState);
				IFCALLRET(window->WindowUpdate, result, context, orderInfo, &windowState);
			}

			update_free_window_state(&windowState);
		}
	}

	return result;
}

static void update_notify_icon_state_order_free(NOTIFY_ICON_STATE_ORDER* notify)
{
	free(notify->toolTip.string);
	free(notify->infoTip.text.string);
	free(notify->infoTip.title.string);
	update_free_window_icon_info(&notify->icon);
	memset(notify, 0, sizeof(NOTIFY_ICON_STATE_ORDER));
}

static BOOL update_read_notification_icon_state_order(wStream* s, WINDOW_ORDER_INFO* orderInfo,
                                                      NOTIFY_ICON_STATE_ORDER* notify_icon_state)
{
	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_VERSION)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
			return FALSE;

		Stream_Read_UINT32(s, notify_icon_state->version); /* version (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_TIP)
	{
		if (!rail_read_unicode_string(s,
		                              &notify_icon_state->toolTip)) /* toolTip (UNICODE_STRING) */
			return FALSE;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_INFO_TIP)
	{
		if (!update_read_notify_icon_infotip(
		        s, &notify_icon_state->infoTip)) /* infoTip (NOTIFY_ICON_INFOTIP) */
			return FALSE;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_STATE)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
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
		if (!update_read_cached_icon_info(
		        s, &notify_icon_state->cachedIcon)) /* cachedIcon (CACHED_ICON_INFO) */
			return FALSE;
	}

	return TRUE;
}

static void update_read_notification_icon_delete_order(wStream* s, WINDOW_ORDER_INFO* orderInfo)
{
	/* notification icon deletion event */
}

static BOOL update_recv_notification_icon_info_order(rdpUpdate* update, wStream* s,
                                                     WINDOW_ORDER_INFO* orderInfo)
{
	rdp_update_internal* up = update_cast(update);
	rdpContext* context = update->context;
	rdpWindowUpdate* window = update->window;
	BOOL result = TRUE;

	WINPR_ASSERT(s);
	WINPR_ASSERT(orderInfo);
	WINPR_ASSERT(context);
	WINPR_ASSERT(window);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	Stream_Read_UINT32(s, orderInfo->windowId);     /* windowId (4 bytes) */
	Stream_Read_UINT32(s, orderInfo->notifyIconId); /* notifyIconId (4 bytes) */

	if (orderInfo->fieldFlags & WINDOW_ORDER_STATE_DELETED)
	{
		update_read_notification_icon_delete_order(s, orderInfo);
		WLog_Print(up->log, WLOG_DEBUG, "NotifyIconDelete");
		IFCALLRET(window->NotifyIconDelete, result, context, orderInfo);
	}
	else
	{
		NOTIFY_ICON_STATE_ORDER notify_icon_state = { 0 };
		result = update_read_notification_icon_state_order(s, orderInfo, &notify_icon_state);

		if (!result)
			goto fail;

		if (orderInfo->fieldFlags & WINDOW_ORDER_STATE_NEW)
		{
			WLog_Print(up->log, WLOG_DEBUG, "NotifyIconCreate");
			IFCALLRET(window->NotifyIconCreate, result, context, orderInfo, &notify_icon_state);
		}
		else
		{
			WLog_Print(up->log, WLOG_DEBUG, "NotifyIconUpdate");
			IFCALLRET(window->NotifyIconUpdate, result, context, orderInfo, &notify_icon_state);
		}
	fail:
		update_notify_icon_state_order_free(&notify_icon_state);
	}

	return result;
}

static BOOL update_read_desktop_actively_monitored_order(wStream* s, WINDOW_ORDER_INFO* orderInfo,
                                                         MONITORED_DESKTOP_ORDER* monitored_desktop)
{
	int i;
	int size;

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_DESKTOP_ACTIVE_WND)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
			return FALSE;

		Stream_Read_UINT32(s, monitored_desktop->activeWindowId); /* activeWindowId (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_DESKTOP_ZORDER)
	{
		UINT32* newid;

		if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
			return FALSE;

		Stream_Read_UINT8(s, monitored_desktop->numWindowIds); /* numWindowIds (1 byte) */

		if (!Stream_CheckAndLogRequiredLength(TAG, s, 4ull * monitored_desktop->numWindowIds))
			return FALSE;

		if (monitored_desktop->numWindowIds > 0)
		{
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

static void update_read_desktop_non_monitored_order(wStream* s, WINDOW_ORDER_INFO* orderInfo)
{
	/* non-monitored desktop notification event */
}

static void dump_monitored_desktop(wLog* log, const char* msg, const WINDOW_ORDER_INFO* orderInfo,
                                   const MONITORED_DESKTOP_ORDER* monitored)
{
	char buffer[1000] = { 0 };
	const size_t bufferSize = sizeof(buffer) - 1;

	DUMP_APPEND(buffer, bufferSize, "%s", msg);

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_DESKTOP_ACTIVE_WND)
		DUMP_APPEND(buffer, bufferSize, " activeWindowId=0x%" PRIx32 "", monitored->activeWindowId);

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_DESKTOP_ZORDER)
	{
		UINT32 i;

		DUMP_APPEND(buffer, bufferSize, " windows=(");
		for (i = 0; i < monitored->numWindowIds; i++)
		{
			DUMP_APPEND(buffer, bufferSize, "0x%" PRIx32 ",", monitored->windowIds[i]);
		}
		DUMP_APPEND(buffer, bufferSize, ")");
	}
	WLog_Print(log, WLOG_DEBUG, buffer);
}

static BOOL update_recv_desktop_info_order(rdpUpdate* update, wStream* s,
                                           WINDOW_ORDER_INFO* orderInfo)
{
	rdp_update_internal* up = update_cast(update);
	rdpContext* context = update->context;
	rdpWindowUpdate* window = update->window;
	BOOL result = TRUE;

	WINPR_ASSERT(s);
	WINPR_ASSERT(orderInfo);
	WINPR_ASSERT(context);
	WINPR_ASSERT(window);

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_DESKTOP_NONE)
	{
		update_read_desktop_non_monitored_order(s, orderInfo);
		WLog_Print(up->log, WLOG_DEBUG, "NonMonitoredDesktop, windowId=0x%" PRIx32 "",
		           orderInfo->windowId);
		IFCALLRET(window->NonMonitoredDesktop, result, context, orderInfo);
	}
	else
	{
		MONITORED_DESKTOP_ORDER monitored_desktop = { 0 };
		result = update_read_desktop_actively_monitored_order(s, orderInfo, &monitored_desktop);

		if (result)
		{
			dump_monitored_desktop(up->log, "ActivelyMonitoredDesktop", orderInfo,
			                       &monitored_desktop);
			IFCALLRET(window->MonitoredDesktop, result, context, orderInfo, &monitored_desktop);
		}

		free(monitored_desktop.windowIds);
	}

	return result;
}

void update_free_window_icon_info(ICON_INFO* iconInfo)
{
	if (!iconInfo)
		return;

	free(iconInfo->bitsColor);
	iconInfo->bitsColor = NULL;
	free(iconInfo->bitsMask);
	iconInfo->bitsMask = NULL;
	free(iconInfo->colorTable);
	iconInfo->colorTable = NULL;
}

BOOL update_recv_altsec_window_order(rdpUpdate* update, wStream* s)
{
	BOOL rc = TRUE;
	size_t remaining;
	UINT16 orderSize;
	WINDOW_ORDER_INFO orderInfo = { 0 };
	rdp_update_internal* up = update_cast(update);

	remaining = Stream_GetRemainingLength(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 6))
		return FALSE;

	Stream_Read_UINT16(s, orderSize);            /* orderSize (2 bytes) */
	Stream_Read_UINT32(s, orderInfo.fieldFlags); /* FieldsPresentFlags (4 bytes) */

	if (remaining + 1 < orderSize)
	{
		WLog_Print(up->log, WLOG_ERROR, "Stream short orderSize");
		return FALSE;
	}

	if (!window_order_supported(update->context->settings, orderInfo.fieldFlags))
	{
		WLog_INFO(TAG, "Window order %08" PRIx32 " not supported!", orderInfo.fieldFlags);
		return FALSE;
	}

	if (orderInfo.fieldFlags & WINDOW_ORDER_TYPE_WINDOW)
		rc = update_recv_window_info_order(update, s, &orderInfo);
	else if (orderInfo.fieldFlags & WINDOW_ORDER_TYPE_NOTIFY)
		rc = update_recv_notification_icon_info_order(update, s, &orderInfo);
	else if (orderInfo.fieldFlags & WINDOW_ORDER_TYPE_DESKTOP)
		rc = update_recv_desktop_info_order(update, s, &orderInfo);

	if (!rc)
		WLog_Print(up->log, WLOG_ERROR, "windoworder flags %08" PRIx32 " failed",
		           orderInfo.fieldFlags);

	return rc;
}
