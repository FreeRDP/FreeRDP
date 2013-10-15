/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote Applications Integrated Locally (RAIL) Orders
 *
 * Copyright 2009 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>

#include <winpr/crt.h>

#include <freerdp/utils/rail.h>

#include "rail_orders.h"

void rail_send_pdu(railPlugin* rail, wStream* s, UINT16 orderType)
{
	UINT16 orderLength;

	orderLength = Stream_GetPosition(s);
	Stream_SetPosition(s, 0);

	rail_write_pdu_header(s, orderType, orderLength);
	Stream_SetPosition(s, orderLength);

	WLog_Print(rail->log, WLOG_DEBUG, "Sending %s PDU, length: %d",
			RAIL_ORDER_TYPE_STRINGS[((orderType & 0xF0) >> 3) + (orderType & 0x0F)], orderLength);

	rail_send_channel_data(rail, Stream_Buffer(s), orderLength);
}

void rail_write_high_contrast(wStream* s, RAIL_HIGH_CONTRAST* highContrast)
{
	highContrast->colorSchemeLength = highContrast->colorScheme.length + 2;
	Stream_Write_UINT32(s, highContrast->flags); /* flags (4 bytes) */
	Stream_Write_UINT32(s, highContrast->colorSchemeLength); /* colorSchemeLength (4 bytes) */
	rail_write_unicode_string(s, &highContrast->colorScheme); /* colorScheme */
}

BOOL rail_read_server_exec_result_order(wStream* s, RAIL_EXEC_RESULT_ORDER* execResult)
{
	if (Stream_GetRemainingLength(s) < 8)
		return FALSE;

	Stream_Read_UINT16(s, execResult->flags); /* flags (2 bytes) */
	Stream_Read_UINT16(s, execResult->execResult); /* execResult (2 bytes) */
	Stream_Read_UINT32(s, execResult->rawResult); /* rawResult (4 bytes) */
	Stream_Seek_UINT16(s); /* padding (2 bytes) */

	return rail_read_unicode_string(s, &execResult->exeOrFile); /* exeOrFile */
}

BOOL rail_read_server_sysparam_order(wStream* s, RAIL_SYSPARAM_ORDER* sysparam)
{
	BYTE body;

	if (Stream_GetRemainingLength(s) < 5)
		return FALSE;

	Stream_Read_UINT32(s, sysparam->param); /* systemParam (4 bytes) */
	Stream_Read_UINT8(s, body); /* body (1 byte) */

	switch (sysparam->param)
	{
		case SPI_SET_SCREEN_SAVE_ACTIVE:
			sysparam->setScreenSaveActive = (body != 0) ? TRUE : FALSE;
			break;

		case SPI_SET_SCREEN_SAVE_SECURE:
			sysparam->setScreenSaveSecure = (body != 0) ? TRUE : FALSE;
			break;

		default:
			break;
	}

	return TRUE;
}

BOOL rail_read_server_minmaxinfo_order(wStream* s, RAIL_MINMAXINFO_ORDER* minmaxinfo)
{
	if (Stream_GetRemainingLength(s) < 20)
		return FALSE;

	Stream_Read_UINT32(s, minmaxinfo->windowId); /* windowId (4 bytes) */
	Stream_Read_UINT16(s, minmaxinfo->maxWidth); /* maxWidth (2 bytes) */
	Stream_Read_UINT16(s, minmaxinfo->maxHeight); /* maxHeight (2 bytes) */
	Stream_Read_UINT16(s, minmaxinfo->maxPosX); /* maxPosX (2 bytes) */
	Stream_Read_UINT16(s, minmaxinfo->maxPosY); /* maxPosY (2 bytes) */
	Stream_Read_UINT16(s, minmaxinfo->minTrackWidth); /* minTrackWidth (2 bytes) */
	Stream_Read_UINT16(s, minmaxinfo->minTrackHeight); /* minTrackHeight (2 bytes) */
	Stream_Read_UINT16(s, minmaxinfo->maxTrackWidth); /* maxTrackWidth (2 bytes) */
	Stream_Read_UINT16(s, minmaxinfo->maxTrackHeight); /* maxTrackHeight (2 bytes) */

	return TRUE;
}

BOOL rail_read_server_localmovesize_order(wStream* s, RAIL_LOCALMOVESIZE_ORDER* localMoveSize)
{
	UINT16 isMoveSizeStart;

	if (Stream_GetRemainingLength(s) < 12)
		return FALSE;

	Stream_Read_UINT32(s, localMoveSize->windowId); /* windowId (4 bytes) */

	Stream_Read_UINT16(s, isMoveSizeStart); /* isMoveSizeStart (2 bytes) */
	localMoveSize->isMoveSizeStart = (isMoveSizeStart != 0) ? TRUE : FALSE;

	Stream_Read_UINT16(s, localMoveSize->moveSizeType); /* moveSizeType (2 bytes) */
	Stream_Read_UINT16(s, localMoveSize->posX); /* posX (2 bytes) */
	Stream_Read_UINT16(s, localMoveSize->posY); /* posY (2 bytes) */

	return TRUE;
}

BOOL rail_read_server_get_appid_resp_order(wStream* s, RAIL_GET_APPID_RESP_ORDER* getAppidResp)
{
	if (Stream_GetRemainingLength(s) < 516)
		return FALSE;

	Stream_Read_UINT32(s, getAppidResp->windowId); /* windowId (4 bytes) */
	Stream_Read(s, &getAppidResp->applicationIdBuffer[0], 512); /* applicationId (256 UNICODE chars) */

	getAppidResp->applicationId.length = 512;
	getAppidResp->applicationId.string = &getAppidResp->applicationIdBuffer[0];

	return TRUE;
}

BOOL rail_read_langbar_info_order(wStream* s, RAIL_LANGBAR_INFO_ORDER* langbarInfo)
{
	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT32(s, langbarInfo->languageBarStatus); /* languageBarStatus (4 bytes) */

	return TRUE;
}

void rail_write_client_status_order(wStream* s, RAIL_CLIENT_STATUS_ORDER* clientStatus)
{
	Stream_Write_UINT32(s, clientStatus->flags); /* flags (4 bytes) */
}

void rail_write_client_exec_order(wStream* s, RAIL_EXEC_ORDER* exec)
{
	Stream_Write_UINT16(s, exec->flags); /* flags (2 bytes) */
	Stream_Write_UINT16(s, exec->exeOrFile.length); /* exeOrFileLength (2 bytes) */
	Stream_Write_UINT16(s, exec->workingDir.length); /* workingDirLength (2 bytes) */
	Stream_Write_UINT16(s, exec->arguments.length); /* argumentsLength (2 bytes) */
	rail_write_unicode_string_value(s, &exec->exeOrFile); /* exeOrFile */
	rail_write_unicode_string_value(s, &exec->workingDir); /* workingDir */
	rail_write_unicode_string_value(s, &exec->arguments); /* arguments */
}

void rail_write_client_sysparam_order(wStream* s, RAIL_SYSPARAM_ORDER* sysparam)
{
	BYTE body;

	Stream_Write_UINT32(s, sysparam->param); /* systemParam (4 bytes) */

	switch (sysparam->param)
	{
		case SPI_SET_DRAG_FULL_WINDOWS:
			body = sysparam->dragFullWindows;
			Stream_Write_UINT8(s, body);
			break;

		case SPI_SET_KEYBOARD_CUES:
			body = sysparam->keyboardCues;
			Stream_Write_UINT8(s, body);
			break;

		case SPI_SET_KEYBOARD_PREF:
			body = sysparam->keyboardPref;
			Stream_Write_UINT8(s, body);
			break;

		case SPI_SET_MOUSE_BUTTON_SWAP:
			body = sysparam->mouseButtonSwap;
			Stream_Write_UINT8(s, body);
			break;

		case SPI_SET_WORK_AREA:
			Stream_Write_UINT16(s, sysparam->workArea.left); /* left (2 bytes) */
			Stream_Write_UINT16(s, sysparam->workArea.top); /* top (2 bytes) */
			Stream_Write_UINT16(s, sysparam->workArea.right); /* right (2 bytes) */
			Stream_Write_UINT16(s, sysparam->workArea.bottom); /* bottom (2 bytes) */
			break;

		case SPI_DISPLAY_CHANGE:
			Stream_Write_UINT16(s, sysparam->displayChange.left); /* left (2 bytes) */
			Stream_Write_UINT16(s, sysparam->displayChange.top); /* top (2 bytes) */
			Stream_Write_UINT16(s, sysparam->displayChange.right); /* right (2 bytes) */
			Stream_Write_UINT16(s, sysparam->displayChange.bottom); /* bottom (2 bytes) */
			break;

		case SPI_TASKBAR_POS:
			Stream_Write_UINT16(s, sysparam->taskbarPos.left); /* left (2 bytes) */
			Stream_Write_UINT16(s, sysparam->taskbarPos.top); /* top (2 bytes) */
			Stream_Write_UINT16(s, sysparam->taskbarPos.right); /* right (2 bytes) */
			Stream_Write_UINT16(s, sysparam->taskbarPos.bottom); /* bottom (2 bytes) */
			break;

		case SPI_SET_HIGH_CONTRAST:
			rail_write_high_contrast(s, &sysparam->highContrast);
			break;
	}
}

void rail_write_client_activate_order(wStream* s, RAIL_ACTIVATE_ORDER* activate)
{
	BYTE enabled;

	Stream_Write_UINT32(s, activate->windowId); /* windowId (4 bytes) */

	enabled = activate->enabled;
	Stream_Write_UINT8(s, enabled); /* enabled (1 byte) */
}

void rail_write_client_sysmenu_order(wStream* s, RAIL_SYSMENU_ORDER* sysmenu)
{
	Stream_Write_UINT32(s, sysmenu->windowId); /* windowId (4 bytes) */
	Stream_Write_UINT16(s, sysmenu->left); /* left (2 bytes) */
	Stream_Write_UINT16(s, sysmenu->top); /* top (2 bytes) */
}

void rail_write_client_syscommand_order(wStream* s, RAIL_SYSCOMMAND_ORDER* syscommand)
{
	Stream_Write_UINT32(s, syscommand->windowId); /* windowId (4 bytes) */
	Stream_Write_UINT16(s, syscommand->command); /* command (2 bytes) */
}

void rail_write_client_notify_event_order(wStream* s, RAIL_NOTIFY_EVENT_ORDER* notifyEvent)
{
	Stream_Write_UINT32(s, notifyEvent->windowId); /* windowId (4 bytes) */
	Stream_Write_UINT32(s, notifyEvent->notifyIconId); /* notifyIconId (4 bytes) */
	Stream_Write_UINT32(s, notifyEvent->message); /* notifyIconId (4 bytes) */
}

void rail_write_client_window_move_order(wStream* s, RAIL_WINDOW_MOVE_ORDER* windowMove)
{
	Stream_Write_UINT32(s, windowMove->windowId); /* windowId (4 bytes) */
	Stream_Write_UINT16(s, windowMove->left); /* left (2 bytes) */
	Stream_Write_UINT16(s, windowMove->top); /* top (2 bytes) */
	Stream_Write_UINT16(s, windowMove->right); /* right (2 bytes) */
	Stream_Write_UINT16(s, windowMove->bottom); /* bottom (2 bytes) */
}

void rail_write_client_get_appid_req_order(wStream* s, RAIL_GET_APPID_REQ_ORDER* getAppidReq)
{
	Stream_Write_UINT32(s, getAppidReq->windowId); /* windowId (4 bytes) */
}

void rail_write_langbar_info_order(wStream* s, RAIL_LANGBAR_INFO_ORDER* langbarInfo)
{
	Stream_Write_UINT32(s, langbarInfo->languageBarStatus); /* languageBarStatus (4 bytes) */
}

BOOL rail_recv_handshake_order(rdpRailOrder* railOrder, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(railOrder->plugin);

	if (!rail_read_handshake_order(s, &railOrder->handshake))
		return FALSE;

	railOrder->handshake.buildNumber = 0x00001DB0;
	rail_send_handshake_order(railOrder);

	railOrder->client_status.flags = RAIL_CLIENTSTATUS_ALLOWLOCALMOVESIZE;
	rail_send_client_status_order(railOrder);

	/* sysparam update */

	railOrder->sysparam.params = 0;

	railOrder->sysparam.params |= SPI_MASK_SET_HIGH_CONTRAST;
	railOrder->sysparam.highContrast.colorScheme.string = NULL;
	railOrder->sysparam.highContrast.colorScheme.length = 0;
	railOrder->sysparam.highContrast.flags = 0x7E;

	railOrder->sysparam.params |= SPI_MASK_SET_MOUSE_BUTTON_SWAP;
	railOrder->sysparam.mouseButtonSwap = FALSE;

	railOrder->sysparam.params |= SPI_MASK_SET_KEYBOARD_PREF;
	railOrder->sysparam.keyboardPref = FALSE;

	railOrder->sysparam.params |= SPI_MASK_SET_DRAG_FULL_WINDOWS;
	railOrder->sysparam.dragFullWindows = FALSE;

	railOrder->sysparam.params |= SPI_MASK_SET_KEYBOARD_CUES;
	railOrder->sysparam.keyboardCues = FALSE;

	railOrder->sysparam.params |= SPI_MASK_SET_WORK_AREA;
	railOrder->sysparam.workArea.left = 0;
	railOrder->sysparam.workArea.top = 0;
	railOrder->sysparam.workArea.right = 1024;
	railOrder->sysparam.workArea.bottom = 768;

	if (context->custom)
	{
		RAIL_SYSPARAM_ORDER sysparam;
		IFCALL(context->GetSystemParam, context, &sysparam);
	}
	else
	{
		rail_send_channel_event(railOrder->plugin,
			RailChannel_GetSystemParam, &railOrder->sysparam);
	}

	return TRUE;
}

BOOL rail_recv_exec_result_order(rdpRailOrder* railOrder, wStream* s)
{
	RAIL_EXEC_RESULT_ORDER execResult;
	RailClientContext* context = rail_get_client_interface(railOrder->plugin);

	ZeroMemory(&execResult, sizeof(RAIL_EXEC_RESULT_ORDER));

	if (!rail_read_server_exec_result_order(s, &execResult))
		return FALSE;

	if (context->custom)
	{
		IFCALL(context->ServerExecuteResult, context, &execResult);
	}
	else
	{
		rail_send_channel_event(railOrder->plugin,
			RailChannel_ServerExecuteResult, &execResult);
	}

	return TRUE;
}

BOOL rail_recv_server_sysparam_order(rdpRailOrder* railOrder, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(railOrder->plugin);

	if (!rail_read_server_sysparam_order(s, &railOrder->sysparam))
		return FALSE;

	if (context->custom)
	{
		IFCALL(context->ServerSystemParam, context, &railOrder->sysparam);
	}
	else
	{
		rail_send_channel_event(railOrder->plugin,
			RailChannel_ServerSystemParam, &railOrder->sysparam);
	}

	return TRUE;
}

BOOL rail_recv_server_minmaxinfo_order(rdpRailOrder* railOrder, wStream* s)
{
	if (!rail_read_server_minmaxinfo_order(s, &railOrder->minmaxinfo))
		return FALSE;

	rail_send_channel_event(railOrder->plugin,
		RailChannel_ServerMinMaxInfo, &railOrder->minmaxinfo);

	return TRUE;
}

BOOL rail_recv_server_localmovesize_order(rdpRailOrder* railOrder, wStream* s)
{
	if (!rail_read_server_localmovesize_order(s, &railOrder->localmovesize))
		return FALSE;

	rail_send_channel_event(railOrder->plugin,
		RailChannel_ServerLocalMoveSize, &railOrder->localmovesize);

	return TRUE;
}

BOOL rail_recv_server_get_appid_resp_order(rdpRailOrder* railOrder, wStream* s)
{
	if (!rail_read_server_get_appid_resp_order(s, &railOrder->get_appid_resp))
		return FALSE;

	rail_send_channel_event(railOrder->plugin,
		RailChannel_ServerGetAppIdResponse, &railOrder->get_appid_resp);

	return TRUE;
}

BOOL rail_recv_langbar_info_order(rdpRailOrder* railOrder, wStream* s)
{
	if (!rail_read_langbar_info_order(s, &railOrder->langbar_info))
		return FALSE;

	rail_send_channel_event(railOrder->plugin,
		RailChannel_ServerLanguageBarInfo, &railOrder->langbar_info);

	return TRUE;
}

BOOL rail_order_recv(rdpRailOrder* railOrder, wStream* s)
{
	UINT16 orderType;
	UINT16 orderLength;

	if (!rail_read_pdu_header(s, &orderType, &orderLength))
		return FALSE;

	WLog_Print(((railPlugin*) railOrder->plugin)->log, WLOG_DEBUG, "Received %s PDU, length: %d",
			RAIL_ORDER_TYPE_STRINGS[((orderType & 0xF0) >> 3) + (orderType & 0x0F)], orderLength);

	switch (orderType)
	{
		case RDP_RAIL_ORDER_HANDSHAKE:
			return rail_recv_handshake_order(railOrder, s);

		case RDP_RAIL_ORDER_EXEC_RESULT:
			return rail_recv_exec_result_order(railOrder, s);

		case RDP_RAIL_ORDER_SYSPARAM:
			return rail_recv_server_sysparam_order(railOrder, s);

		case RDP_RAIL_ORDER_MINMAXINFO:
			return rail_recv_server_minmaxinfo_order(railOrder, s);

		case RDP_RAIL_ORDER_LOCALMOVESIZE:
			return rail_recv_server_localmovesize_order(railOrder, s);

		case RDP_RAIL_ORDER_GET_APPID_RESP:
			return rail_recv_server_get_appid_resp_order(railOrder, s);

		case RDP_RAIL_ORDER_LANGBARINFO:
			return rail_recv_langbar_info_order(railOrder, s);

		default:
			fprintf(stderr, "Unknown RAIL PDU order reveived.");
			break;
	}

	return TRUE;
}

void rail_send_handshake_order(rdpRailOrder* railOrder)
{
	wStream* s;
	s = rail_pdu_init(RAIL_HANDSHAKE_ORDER_LENGTH);
	rail_write_handshake_order(s, &railOrder->handshake);
	rail_send_pdu((railPlugin*) railOrder->plugin, s, RDP_RAIL_ORDER_HANDSHAKE);
	Stream_Free(s, TRUE);
}

void rail_send_client_status_order(rdpRailOrder* railOrder)
{
	wStream* s;
	s = rail_pdu_init(RAIL_CLIENT_STATUS_ORDER_LENGTH);
	rail_write_client_status_order(s, &railOrder->client_status);
	rail_send_pdu((railPlugin*) railOrder->plugin, s, RDP_RAIL_ORDER_CLIENTSTATUS);
	Stream_Free(s, TRUE);
}

void rail_send_client_exec_order(railPlugin* rail, RAIL_EXEC_ORDER* exec)
{
	wStream* s;
	int length;

	length = RAIL_EXEC_ORDER_LENGTH +
			exec->exeOrFile.length +
			exec->workingDir.length +
			exec->arguments.length;

	s = rail_pdu_init(RAIL_EXEC_ORDER_LENGTH);
	rail_write_client_exec_order(s, exec);
	rail_send_pdu(rail, s, RDP_RAIL_ORDER_EXEC);
	Stream_Free(s, TRUE);
}

void rail_send_client_sysparam_order(rdpRailOrder* railOrder)
{
	wStream* s;
	int length;

	length = RAIL_SYSPARAM_ORDER_LENGTH;

	switch (railOrder->sysparam.param)
	{
		case SPI_SET_DRAG_FULL_WINDOWS:
		case SPI_SET_KEYBOARD_CUES:
		case SPI_SET_KEYBOARD_PREF:
		case SPI_SET_MOUSE_BUTTON_SWAP:
			length += 1;
			break;

		case SPI_SET_WORK_AREA:
		case SPI_DISPLAY_CHANGE:
		case SPI_TASKBAR_POS:
			length += 8;
			break;

		case SPI_SET_HIGH_CONTRAST:
			length += railOrder->sysparam.highContrast.colorSchemeLength + 10;
			break;
	}

	s = rail_pdu_init(RAIL_SYSPARAM_ORDER_LENGTH + 8);
	rail_write_client_sysparam_order(s, &railOrder->sysparam);
	rail_send_pdu((railPlugin*) railOrder->plugin, s, RDP_RAIL_ORDER_SYSPARAM);
	Stream_Free(s, TRUE);
}

void rail_send_client_sysparams_order(rdpRailOrder* railOrder)
{
	if (railOrder->sysparam.params & SPI_MASK_SET_HIGH_CONTRAST)
	{
		railOrder->sysparam.param = SPI_SET_HIGH_CONTRAST;
		rail_send_client_sysparam_order(railOrder);
	}

	if (railOrder->sysparam.params & SPI_MASK_TASKBAR_POS)
	{
		railOrder->sysparam.param = SPI_TASKBAR_POS;
		rail_send_client_sysparam_order(railOrder);
	}

	if (railOrder->sysparam.params & SPI_MASK_SET_MOUSE_BUTTON_SWAP)
	{
		railOrder->sysparam.param = SPI_SET_MOUSE_BUTTON_SWAP;
		rail_send_client_sysparam_order(railOrder);
	}

	if (railOrder->sysparam.params & SPI_MASK_SET_KEYBOARD_PREF)
	{
		railOrder->sysparam.param = SPI_SET_KEYBOARD_PREF;
		rail_send_client_sysparam_order(railOrder);
	}

	if (railOrder->sysparam.params & SPI_MASK_SET_DRAG_FULL_WINDOWS)
	{
		railOrder->sysparam.param = SPI_SET_DRAG_FULL_WINDOWS;
		rail_send_client_sysparam_order(railOrder);
	}

	if (railOrder->sysparam.params & SPI_MASK_SET_KEYBOARD_CUES)
	{
		railOrder->sysparam.param = SPI_SET_KEYBOARD_CUES;
		rail_send_client_sysparam_order(railOrder);
	}

	if (railOrder->sysparam.params & SPI_MASK_SET_WORK_AREA)
	{
		railOrder->sysparam.param = SPI_SET_WORK_AREA;
		rail_send_client_sysparam_order(railOrder);
	}
}

void rail_send_client_activate_order(rdpRailOrder* railOrder)
{
	wStream* s;
	s = rail_pdu_init(RAIL_ACTIVATE_ORDER_LENGTH);
	rail_write_client_activate_order(s, &railOrder->activate);
	rail_send_pdu((railPlugin*) railOrder->plugin, s, RDP_RAIL_ORDER_ACTIVATE);
	Stream_Free(s, TRUE);
}

void rail_send_client_sysmenu_order(rdpRailOrder* railOrder)
{
	wStream* s;
	s = rail_pdu_init(RAIL_SYSMENU_ORDER_LENGTH);
	rail_write_client_sysmenu_order(s, &railOrder->sysmenu);
	rail_send_pdu((railPlugin*) railOrder->plugin, s, RDP_RAIL_ORDER_SYSMENU);
	Stream_Free(s, TRUE);
}

void rail_send_client_syscommand_order(rdpRailOrder* railOrder)
{
	wStream* s;
	s = rail_pdu_init(RAIL_SYSCOMMAND_ORDER_LENGTH);
	rail_write_client_syscommand_order(s, &railOrder->syscommand);
	rail_send_pdu((railPlugin*) railOrder->plugin, s, RDP_RAIL_ORDER_SYSCOMMAND);
	Stream_Free(s, TRUE);
}

void rail_send_client_notify_event_order(rdpRailOrder* railOrder)
{
	wStream* s;
	s = rail_pdu_init(RAIL_NOTIFY_EVENT_ORDER_LENGTH);
	rail_write_client_notify_event_order(s, &railOrder->notify_event);
	rail_send_pdu((railPlugin*) railOrder->plugin, s, RDP_RAIL_ORDER_NOTIFY_EVENT);
	Stream_Free(s, TRUE);
}

void rail_send_client_window_move_order(rdpRailOrder* railOrder)
{
	wStream* s;
	s = rail_pdu_init(RAIL_WINDOW_MOVE_ORDER_LENGTH);
	rail_write_client_window_move_order(s, &railOrder->window_move);
	rail_send_pdu((railPlugin*) railOrder->plugin, s, RDP_RAIL_ORDER_WINDOWMOVE);
	Stream_Free(s, TRUE);
}

void rail_send_client_get_appid_req_order(rdpRailOrder* railOrder)
{
	wStream* s;
	s = rail_pdu_init(RAIL_GET_APPID_REQ_ORDER_LENGTH);
	rail_write_client_get_appid_req_order(s, &railOrder->get_appid_req);
	rail_send_pdu((railPlugin*) railOrder->plugin, s, RDP_RAIL_ORDER_GET_APPID_REQ);
	Stream_Free(s, TRUE);
}

void rail_send_client_langbar_info_order(rdpRailOrder* railOrder)
{
	wStream* s;
	s = rail_pdu_init(RAIL_LANGBAR_INFO_ORDER_LENGTH);
	rail_write_langbar_info_order(s, &railOrder->langbar_info);
	rail_send_pdu((railPlugin*) railOrder->plugin, s, RDP_RAIL_ORDER_LANGBARINFO);
	Stream_Free(s, TRUE);
}

rdpRailOrder* rail_order_new()
{
	rdpRailOrder* railOrder;

	railOrder = (rdpRailOrder*) malloc(sizeof(rdpRailOrder));

	if (railOrder)
	{
		ZeroMemory(railOrder, sizeof(rdpRailOrder));
	}

	return railOrder;
}

void rail_order_free(rdpRailOrder* railOrder)
{
	if (railOrder)
	{
		free(railOrder);
	}
}
