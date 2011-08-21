/**
 * FreeRDP: A Remote Desktop Protocol Client
 * X11 RAIL
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

#include <freerdp/utils/event.h>
#include <freerdp/utils/hexdump.h>
#include <freerdp/rail/rail.h>


#include "xf_window.h"
#include "xf_rail.h"

void xf_rail_paint(xfInfo* xfi, rdpRail* rail, uint32 ileft, uint32 itop, uint32 iright, uint32 ibottom)
{
	xfWindow* xfw;
	rdpWindow* window;
	boolean intersect;
	uint32 uwidth, uheight;
	uint32 uleft, utop, uright, ubottom;
	uint32 wleft, wtop, wright, wbottom;

	window_list_rewind(rail->list);

	while (window_list_has_next(rail->list))
	{
		window = window_list_get_next(rail->list);
		xfw = (xfWindow*) window->extra;

		wleft = window->windowOffsetX;
		wtop = window->windowOffsetY;
		wright = window->windowOffsetX + window->windowWidth - 1;
		wbottom = window->windowOffsetY + window->windowHeight - 1;

		intersect = ((iright > wleft) && (ileft < wright) &&
				(ibottom > wtop) && (itop < wbottom)) ? True : False;

		uleft = (ileft > wleft) ? ileft : wleft;
		utop = (itop > wtop) ? itop : wtop;
		uright = (iright < wright) ? iright : wright;
		ubottom = (ibottom < wbottom) ? ibottom : wbottom;
		uwidth = uright - uleft + 1;
		uheight = ubottom - utop + 1;

		if (intersect)
		{
			XPutImage(xfi->display, xfi->primary, xfw->gc, xfi->image,
					uleft, utop, uleft, utop, uwidth, uheight);

			XCopyArea(xfi->display, xfi->primary, xfw->handle, xfw->gc,
					uleft, utop, uwidth, uheight,
					uleft - window->windowOffsetX,
					utop - window->windowOffsetY);
		}
	}

	XFlush(xfi->display);
}

void xf_rail_CreateWindow(rdpRail* rail, rdpWindow* window)
{
	xfInfo* xfi;
	xfWindow* xfw;

	xfi = (xfInfo*) rail->extra;

	xfw = xf_CreateWindow((xfInfo*) rail->extra,
			window->windowOffsetX + xfi->workArea.x,
			window->windowOffsetY + xfi->workArea.y,
			window->windowWidth, window->windowHeight, window->title);

	window->extra = (void*) xfw;
	window->extraId = (void*) xfw->handle;
}

void xf_rail_MoveWindow(rdpRail* rail, rdpWindow* window)
{
	xfInfo* xfi;
	xfWindow* xfw;

	xfi = (xfInfo*) rail->extra;
	xfw = (xfWindow*) window->extra;

	xf_MoveWindow((xfInfo*) rail->extra, xfw,
			window->windowOffsetX + xfi->workArea.x,
			window->windowOffsetY + xfi->workArea.y,
			window->windowWidth, window->windowHeight);
}

void xf_rail_SetWindowText(rdpRail* rail, rdpWindow* window)
{
	xfInfo* xfi;
	xfWindow* xfw;

	xfi = (xfInfo*) rail->extra;
	xfw = (xfWindow*) window->extra;

	XStoreName(xfi->display, xfw->handle, window->title);
}

void xf_rail_DestroyWindow(rdpRail* rail, rdpWindow* window)
{
	xfWindow* xfw;
	xfw = (xfWindow*) window->extra;
	xf_DestroyWindow((xfInfo*) rail->extra, xfw);
}

void xf_rail_register_callbacks(xfInfo* xfi, rdpRail* rail)
{
	rail->extra = (void*) xfi;
	rail->CreateWindow = xf_rail_CreateWindow;
	rail->MoveWindow = xf_rail_MoveWindow;
	rail->SetWindowText = xf_rail_SetWindowText;
	rail->DestroyWindow = xf_rail_DestroyWindow;
}

void xf_process_rail_get_sysparams_event(xfInfo* xfi, rdpChanMan* chanman, RDP_EVENT* event)
{
	RDP_EVENT* new_event;
	RAIL_SYSPARAM_ORDER* sysparam;

	sysparam = (RAIL_SYSPARAM_ORDER*) event->user_data;

	sysparam->workArea.left = xfi->workArea.x;
	sysparam->workArea.top = xfi->workArea.y;
	sysparam->workArea.right = xfi->workArea.x + xfi->workArea.width;
	sysparam->workArea.bottom = xfi->workArea.y + xfi->workArea.height;

	sysparam->displayChange.left = xfi->workArea.x;
	sysparam->displayChange.top = xfi->workArea.y;
	sysparam->displayChange.right = xfi->workArea.x + xfi->workArea.width;
	sysparam->displayChange.bottom = xfi->workArea.y + xfi->workArea.height;

	sysparam->taskbarPos.left = 0;
	sysparam->taskbarPos.top = 0;
	sysparam->taskbarPos.right = 0;
	sysparam->taskbarPos.bottom = 0;

	new_event = freerdp_event_new(RDP_EVENT_CLASS_RAIL,
			RDP_EVENT_TYPE_RAIL_CLIENT_SET_SYSPARAMS, NULL, sysparam);

	freerdp_chanman_send_event(chanman, new_event);
}

const char* error_code_names[] =
{
		"RAIL_EXEC_S_OK",
		"RAIL_EXEC_E_HOOK_NOT_LOADED",
		"RAIL_EXEC_E_DECODE_FAILED",
		"RAIL_EXEC_E_NOT_IN_ALLOWLIST",
		"RAIL_EXEC_E_FILE_NOT_FOUND",
		"RAIL_EXEC_E_FAIL",
		"RAIL_EXEC_E_SESSION_LOCKED"
};

void xf_process_rail_exec_result_event(xfInfo* xfi, rdpChanMan* chanman, RDP_EVENT* event)
{
	RAIL_EXEC_RESULT_ORDER* exec_result;

	exec_result = (RAIL_EXEC_RESULT_ORDER*) event->user_data;

	if (exec_result->execResult != RAIL_EXEC_S_OK)
	{
		printf("RAIL exec error: execResult=%s NtError=0x%X\n",
			error_code_names[exec_result->execResult], exec_result->rawResult);
	}
}

void xf_process_rail_server_sysparam_event(xfInfo* xfi, rdpChanMan* chanman, RDP_EVENT* event)
{
	RAIL_SYSPARAM_ORDER* sysparam = (RAIL_SYSPARAM_ORDER*)event->user_data;

	switch (sysparam->param)
	{
	case SPI_SET_SCREEN_SAVE_ACTIVE:
		printf("xf_process_rail_server_sysparam_event: Server System Param PDU: setScreenSaveActive=%d\n", sysparam->setScreenSaveActive);
		break;
	case SPI_SET_SCREEN_SAVE_SECURE:
		printf("xf_process_rail_server_sysparam_event: Server System Param PDU: setScreenSaveSecure=%d\n", sysparam->setScreenSaveSecure);
		break;
	}
}

void xf_process_rail_server_minmaxinfo_event(xfInfo* xfi, rdpChanMan* chanman, RDP_EVENT* event)
{
	RAIL_MINMAXINFO_ORDER* minmax = (RAIL_MINMAXINFO_ORDER*)event->user_data;

	printf("Server Min Max Info PDU: windowId=0x%X "
		"maxWidth=%d maxHeight=%d maxPosX=%d maxPosY=%d "
		"minTrackWidth=%d minTrackHeight=%d maxTrackWidth=%d maxTrackHeight=%d\n",
		minmax->windowId,
		minmax->maxWidth,
		minmax->maxHeight,
		minmax->maxPosX,
		minmax->maxPosY,
		minmax->minTrackWidth,
		minmax->minTrackHeight,
		minmax->maxTrackWidth,
		minmax->maxTrackHeight
		);
}

void xf_process_rail_server_localmovesize_event(xfInfo* xfi, rdpChanMan* chanman, RDP_EVENT* event)
{
	RAIL_LOCALMOVESIZE_ORDER* movesize = (RAIL_LOCALMOVESIZE_ORDER*)event->user_data;
	const char* movetype_names[] =
	{
	"(invalid)",
	"RAIL_WMSZ_LEFT",
	"RAIL_WMSZ_RIGHT",
	"RAIL_WMSZ_TOP",
	"RAIL_WMSZ_TOPLEFT",
	"RAIL_WMSZ_TOPRIGHT",
	"RAIL_WMSZ_BOTTOM",
	"RAIL_WMSZ_BOTTOMLEFT",
	"RAIL_WMSZ_BOTTOMRIGHT",
	"RAIL_WMSZ_MOVE",
	"RAIL_WMSZ_KEYMOVE",
	"RAIL_WMSZ_KEYSIZE",
	};

	printf("Server Local MoveSize PDU: windowId=0x%X "
		"isMoveSizeStart=%d moveSizeType=%s PosX=%d PosY=%d\n",
		movesize->windowId,
		movesize->isMoveSizeStart,
		movetype_names[movesize->moveSizeType],
		movesize->posX,
		movesize->posY
		);
}

void xf_process_rail_appid_resp_event(xfInfo* xfi, rdpChanMan* chanman, RDP_EVENT* event)
{
	RAIL_GET_APPID_RESP_ORDER* appid_resp =
		(RAIL_GET_APPID_RESP_ORDER*)event->user_data;

	printf("Server Application ID Response PDU: windowId=0x%X "
		"applicationId=(length=%d dump)\n",
		appid_resp->windowId,
		appid_resp->applicationId.length
		);
	freerdp_hexdump(appid_resp->applicationId.string, appid_resp->applicationId.length);
}

void xf_process_rail_langbarinfo_event(xfInfo* xfi, rdpChanMan* chanman, RDP_EVENT* event)
{
	RAIL_LANGBAR_INFO_ORDER* langbar =
		(RAIL_LANGBAR_INFO_ORDER*)event->user_data;

	printf("Language Bar Information PDU: languageBarStatus=0x%X\n",
		langbar->languageBarStatus
		);
}

void xf_process_rail_event(xfInfo* xfi, rdpChanMan* chanman, RDP_EVENT* event)
{
	switch (event->event_type)
	{
		case RDP_EVENT_TYPE_RAIL_CHANNEL_GET_SYSPARAMS:
			xf_process_rail_get_sysparams_event(xfi, chanman, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CHANNEL_EXEC_RESULTS:
			xf_process_rail_exec_result_event(xfi, chanman, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CHANNEL_SERVER_SYSPARAM:
			xf_process_rail_server_sysparam_event(xfi, chanman, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CHANNEL_SERVER_MINMAXINFO:
			xf_process_rail_server_minmaxinfo_event(xfi, chanman, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CHANNEL_SERVER_LOCALMOVESIZE:
			xf_process_rail_server_localmovesize_event(xfi, chanman, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CHANNEL_APPID_RESP:
			xf_process_rail_appid_resp_event(xfi, chanman, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CHANNEL_LANGBARINFO:
			xf_process_rail_langbarinfo_event(xfi, chanman, event);
			break;

		default:
			break;
	}
}
