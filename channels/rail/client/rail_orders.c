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

#include <winpr/crt.h>

#include <freerdp/channels/log.h>

#include "rail_orders.h"

static BOOL rail_read_unicode_string(wStream* s, RAIL_UNICODE_STRING* unicode_string)
{
	UINT16 new_length;

	if (Stream_GetRemainingLength(s) < 2)
		return FALSE;

	Stream_Read_UINT16(s, new_length); /* cbString (2 bytes) */

	if (Stream_GetRemainingLength(s) < new_length)
		return FALSE;

	if (!unicode_string->string)
	{
		unicode_string->string = (BYTE*) malloc(new_length);
		if (!unicode_string->string)
			return FALSE;
		unicode_string->length = new_length;
	}
	else
	{
		BYTE *new_str;

		new_str = (BYTE*) realloc(unicode_string->string, new_length);
		if (!new_str)
			return FALSE;
		unicode_string->string = new_str;
		unicode_string->length = new_length;
	}

	Stream_Read(s, unicode_string->string, unicode_string->length);

	return TRUE;
}

static void rail_write_unicode_string(wStream* s, RAIL_UNICODE_STRING* unicode_string)
{
	Stream_EnsureRemainingCapacity(s, 2 + unicode_string->length);
	Stream_Write_UINT16(s, unicode_string->length); /* cbString (2 bytes) */
	Stream_Write(s, unicode_string->string, unicode_string->length); /* string */
}

static void rail_write_unicode_string_value(wStream* s, RAIL_UNICODE_STRING* unicode_string)
{
	if (unicode_string->length > 0)
	{
		Stream_EnsureRemainingCapacity(s, unicode_string->length);
		Stream_Write(s, unicode_string->string, unicode_string->length); /* string */
	}
}

void rail_send_pdu(railPlugin* rail, wStream* s, UINT16 orderType)
{
	UINT16 orderLength;

	orderLength = (UINT16) Stream_GetPosition(s);
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
	Stream_Read(s, (BYTE*) &(getAppidResp->applicationId), 512); /* applicationId (256 UNICODE chars) */

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

BOOL rail_recv_handshake_order(railPlugin* rail, RAIL_HANDSHAKE_ORDER* handshake, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);

	if (!rail_read_handshake_order(s, handshake))
		return FALSE;

	if (context->custom)
	{
		IFCALL(context->ServerHandshake, context, handshake);
		return TRUE;
	}

	return TRUE;
}

BOOL rail_recv_handshake_ex_order(railPlugin* rail, RAIL_HANDSHAKE_EX_ORDER* handshakeEx, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);

	if (!rail_read_handshake_ex_order(s, handshakeEx))
		return FALSE;

	if (context->custom)
	{
		IFCALL(context->ClientHandshakeEx, context, handshakeEx);
	}

	return TRUE;
}

BOOL rail_recv_exec_result_order(railPlugin* rail, RAIL_EXEC_RESULT_ORDER* execResult, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);

	ZeroMemory(execResult, sizeof(RAIL_EXEC_RESULT_ORDER));

	if (!rail_read_server_exec_result_order(s, execResult))
		return FALSE;

	if (context->custom)
	{
		IFCALL(context->ServerExecuteResult, context, execResult);
	}

	return TRUE;
}

BOOL rail_recv_server_sysparam_order(railPlugin* rail, RAIL_SYSPARAM_ORDER* sysparam, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);

	if (!rail_read_server_sysparam_order(s, sysparam))
		return FALSE;

	if (context->custom)
	{
		IFCALL(context->ServerSystemParam, context, sysparam);
	}

	return TRUE;
}

BOOL rail_recv_server_minmaxinfo_order(railPlugin* rail, RAIL_MINMAXINFO_ORDER* minMaxInfo, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);

	if (!rail_read_server_minmaxinfo_order(s, minMaxInfo))
		return FALSE;

	if (context->custom)
	{
		IFCALL(context->ServerMinMaxInfo, context, minMaxInfo);
	}

	return TRUE;
}

BOOL rail_recv_server_localmovesize_order(railPlugin* rail, RAIL_LOCALMOVESIZE_ORDER* localMoveSize, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);

	if (!rail_read_server_localmovesize_order(s, localMoveSize))
		return FALSE;

	if (context->custom)
	{
		IFCALL(context->ServerLocalMoveSize, context, localMoveSize);
	}

	return TRUE;
}

BOOL rail_recv_server_get_appid_resp_order(railPlugin* rail, RAIL_GET_APPID_RESP_ORDER* getAppIdResp, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);

	if (!rail_read_server_get_appid_resp_order(s, getAppIdResp))
		return FALSE;

	if (context->custom)
	{
		IFCALL(context->ServerGetAppIdResponse, context, getAppIdResp);
	}

	return TRUE;
}

BOOL rail_recv_langbar_info_order(railPlugin* rail, RAIL_LANGBAR_INFO_ORDER* langBarInfo, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);

	if (!rail_read_langbar_info_order(s, langBarInfo))
		return FALSE;

	if (context->custom)
	{
		IFCALL(context->ServerLanguageBarInfo, context, langBarInfo);
	}

	return TRUE;
}

BOOL rail_order_recv(railPlugin* rail, wStream* s)
{
	UINT16 orderType;
	UINT16 orderLength;

	if (!rail_read_pdu_header(s, &orderType, &orderLength))
		return FALSE;

	WLog_Print(rail->log, WLOG_DEBUG, "Received %s PDU, length: %d",
			RAIL_ORDER_TYPE_STRINGS[((orderType & 0xF0) >> 3) + (orderType & 0x0F)], orderLength);

	switch (orderType)
	{
		case RDP_RAIL_ORDER_HANDSHAKE:
		{
			RAIL_HANDSHAKE_ORDER handshake;
			return rail_recv_handshake_order(rail, &handshake, s);
		}

		case RDP_RAIL_ORDER_HANDSHAKE_EX:
		{
			RAIL_HANDSHAKE_EX_ORDER handshakeEx;
			return rail_recv_handshake_ex_order(rail, &handshakeEx, s);
		}

		case RDP_RAIL_ORDER_EXEC_RESULT:
		{
			RAIL_EXEC_RESULT_ORDER execResult;
			return rail_recv_exec_result_order(rail, &execResult, s);
		}

		case RDP_RAIL_ORDER_SYSPARAM:
		{
			RAIL_SYSPARAM_ORDER sysparam;
			return rail_recv_server_sysparam_order(rail, &sysparam, s);
		}

		case RDP_RAIL_ORDER_MINMAXINFO:
		{
			RAIL_MINMAXINFO_ORDER minMaxInfo;
			return rail_recv_server_minmaxinfo_order(rail, &minMaxInfo, s);
		}

		case RDP_RAIL_ORDER_LOCALMOVESIZE:
		{
			RAIL_LOCALMOVESIZE_ORDER localMoveSize;
			return rail_recv_server_localmovesize_order(rail, &localMoveSize, s);
		}

		case RDP_RAIL_ORDER_GET_APPID_RESP:
		{
			RAIL_GET_APPID_RESP_ORDER getAppIdResp;
			return rail_recv_server_get_appid_resp_order(rail, &getAppIdResp, s);
		}

		case RDP_RAIL_ORDER_LANGBARINFO:
		{
			RAIL_LANGBAR_INFO_ORDER langBarInfo;
			return rail_recv_langbar_info_order(rail, &langBarInfo, s);
		}

		default:
			WLog_ERR(TAG,  "Unknown RAIL PDU order reveived.");
			break;
	}

	return TRUE;
}

void rail_send_handshake_order(railPlugin* rail, RAIL_HANDSHAKE_ORDER* handshake)
{
	wStream* s;
	s = rail_pdu_init(RAIL_HANDSHAKE_ORDER_LENGTH);
	rail_write_handshake_order(s, handshake);
	rail_send_pdu(rail, s, RDP_RAIL_ORDER_HANDSHAKE);
	Stream_Free(s, TRUE);
}

void rail_send_handshake_ex_order(railPlugin* rail, RAIL_HANDSHAKE_EX_ORDER* handshakeEx)
{
	wStream* s;
	s = rail_pdu_init(RAIL_HANDSHAKE_EX_ORDER_LENGTH);
	rail_write_handshake_ex_order(s, handshakeEx);
	rail_send_pdu(rail, s, RDP_RAIL_ORDER_HANDSHAKE_EX);
	Stream_Free(s, TRUE);
}

void rail_send_client_status_order(railPlugin* rail, RAIL_CLIENT_STATUS_ORDER* clientStatus)
{
	wStream* s;
	s = rail_pdu_init(RAIL_CLIENT_STATUS_ORDER_LENGTH);
	rail_write_client_status_order(s, clientStatus);
	rail_send_pdu(rail, s, RDP_RAIL_ORDER_CLIENTSTATUS);
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

void rail_send_client_sysparam_order(railPlugin* rail, RAIL_SYSPARAM_ORDER* sysparam)
{
	wStream* s;
	int length;

	length = RAIL_SYSPARAM_ORDER_LENGTH;

	switch (sysparam->param)
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
			length += sysparam->highContrast.colorSchemeLength + 10;
			break;
	}

	s = rail_pdu_init(RAIL_SYSPARAM_ORDER_LENGTH + 8);
	rail_write_client_sysparam_order(s, sysparam);
	rail_send_pdu(rail, s, RDP_RAIL_ORDER_SYSPARAM);
	Stream_Free(s, TRUE);
}

void rail_send_client_sysparams_order(railPlugin* rail, RAIL_SYSPARAM_ORDER* sysparam)
{
	if (sysparam->params & SPI_MASK_SET_HIGH_CONTRAST)
	{
		sysparam->param = SPI_SET_HIGH_CONTRAST;
		rail_send_client_sysparam_order(rail, sysparam);
	}

	if (sysparam->params & SPI_MASK_TASKBAR_POS)
	{
		sysparam->param = SPI_TASKBAR_POS;
		rail_send_client_sysparam_order(rail, sysparam);
	}

	if (sysparam->params & SPI_MASK_SET_MOUSE_BUTTON_SWAP)
	{
		sysparam->param = SPI_SET_MOUSE_BUTTON_SWAP;
		rail_send_client_sysparam_order(rail, sysparam);
	}

	if (sysparam->params & SPI_MASK_SET_KEYBOARD_PREF)
	{
		sysparam->param = SPI_SET_KEYBOARD_PREF;
		rail_send_client_sysparam_order(rail, sysparam);
	}

	if (sysparam->params & SPI_MASK_SET_DRAG_FULL_WINDOWS)
	{
		sysparam->param = SPI_SET_DRAG_FULL_WINDOWS;
		rail_send_client_sysparam_order(rail, sysparam);
	}

	if (sysparam->params & SPI_MASK_SET_KEYBOARD_CUES)
	{
		sysparam->param = SPI_SET_KEYBOARD_CUES;
		rail_send_client_sysparam_order(rail, sysparam);
	}

	if (sysparam->params & SPI_MASK_SET_WORK_AREA)
	{
		sysparam->param = SPI_SET_WORK_AREA;
		rail_send_client_sysparam_order(rail, sysparam);
	}
}

void rail_send_client_activate_order(railPlugin* rail, RAIL_ACTIVATE_ORDER* activate)
{
	wStream* s;
	s = rail_pdu_init(RAIL_ACTIVATE_ORDER_LENGTH);
	rail_write_client_activate_order(s, activate);
	rail_send_pdu(rail, s, RDP_RAIL_ORDER_ACTIVATE);
	Stream_Free(s, TRUE);
}

void rail_send_client_sysmenu_order(railPlugin* rail, RAIL_SYSMENU_ORDER* sysmenu)
{
	wStream* s;
	s = rail_pdu_init(RAIL_SYSMENU_ORDER_LENGTH);
	rail_write_client_sysmenu_order(s, sysmenu);
	rail_send_pdu(rail, s, RDP_RAIL_ORDER_SYSMENU);
	Stream_Free(s, TRUE);
}

void rail_send_client_syscommand_order(railPlugin* rail, RAIL_SYSCOMMAND_ORDER* syscommand)
{
	wStream* s;
	s = rail_pdu_init(RAIL_SYSCOMMAND_ORDER_LENGTH);
	rail_write_client_syscommand_order(s, syscommand);
	rail_send_pdu(rail, s, RDP_RAIL_ORDER_SYSCOMMAND);
	Stream_Free(s, TRUE);
}

void rail_send_client_notify_event_order(railPlugin* rail, RAIL_NOTIFY_EVENT_ORDER* notifyEvent)
{
	wStream* s;
	s = rail_pdu_init(RAIL_NOTIFY_EVENT_ORDER_LENGTH);
	rail_write_client_notify_event_order(s, notifyEvent);
	rail_send_pdu(rail, s, RDP_RAIL_ORDER_NOTIFY_EVENT);
	Stream_Free(s, TRUE);
}

void rail_send_client_window_move_order(railPlugin* rail, RAIL_WINDOW_MOVE_ORDER* windowMove)
{
	wStream* s;
	s = rail_pdu_init(RAIL_WINDOW_MOVE_ORDER_LENGTH);
	rail_write_client_window_move_order(s, windowMove);
	rail_send_pdu(rail, s, RDP_RAIL_ORDER_WINDOWMOVE);
	Stream_Free(s, TRUE);
}

void rail_send_client_get_appid_req_order(railPlugin* rail, RAIL_GET_APPID_REQ_ORDER* getAppIdReq)
{
	wStream* s;
	s = rail_pdu_init(RAIL_GET_APPID_REQ_ORDER_LENGTH);
	rail_write_client_get_appid_req_order(s, getAppIdReq);
	rail_send_pdu(rail, s, RDP_RAIL_ORDER_GET_APPID_REQ);
	Stream_Free(s, TRUE);
}

void rail_send_client_langbar_info_order(railPlugin* rail, RAIL_LANGBAR_INFO_ORDER* langBarInfo)
{
	wStream* s;
	s = rail_pdu_init(RAIL_LANGBAR_INFO_ORDER_LENGTH);
	rail_write_langbar_info_order(s, langBarInfo);
	rail_send_pdu(rail, s, RDP_RAIL_ORDER_LANGBARINFO);
	Stream_Free(s, TRUE);
}
