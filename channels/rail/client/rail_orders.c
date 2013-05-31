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

#define RAIL_ORDER_TYPE_EXEC			0x0001
#define RAIL_ORDER_TYPE_ACTIVATE		0x0002
#define RAIL_ORDER_TYPE_SYSPARAM		0x0003
#define RAIL_ORDER_TYPE_SYSCOMMAND		0x0004
#define RAIL_ORDER_TYPE_HANDSHAKE		0x0005
#define RAIL_ORDER_TYPE_NOTIFY_EVENT		0x0006
#define RAIL_ORDER_TYPE_WINDOW_MOVE		0x0008
#define RAIL_ORDER_TYPE_LOCALMOVESIZE		0x0009
#define RAIL_ORDER_TYPE_MINMAXINFO		0x000A
#define RAIL_ORDER_TYPE_CLIENT_STATUS		0x000B
#define RAIL_ORDER_TYPE_SYSMENU			0x000C
#define RAIL_ORDER_TYPE_LANGBAR_INFO		0x000D
#define RAIL_ORDER_TYPE_EXEC_RESULT		0x0080
#define RAIL_ORDER_TYPE_GET_APPID_REQ		0x000E
#define RAIL_ORDER_TYPE_GET_APPID_RESP		0x000F

static const char* const RAIL_ORDER_TYPE_STRINGS[] =
{
	"",
	"Execute",
	"Activate",
	"System Parameters Update",
	"System Command",
	"Handshake",
	"Notify Event",
	"",
	"Window Move",
	"Local Move/Size",
	"Min Max Info",
	"Client Status",
	"System Menu",
	"Language Bar Info",
	"Get Application ID Request",
	"Get Application ID Response",
	"Execute Result"
};

void rail_string_to_unicode_string(rdpRailOrder* rail_order, char* string, RAIL_UNICODE_STRING* unicode_string)
{
	WCHAR* buffer = NULL;
	int length = 0;

	if (unicode_string->string != NULL)
		free(unicode_string->string);

	unicode_string->string = NULL;
	unicode_string->length = 0;

	if (string == NULL || strlen(string) < 1)
		return;

	length = ConvertToUnicode(CP_UTF8, 0, string, -1, &buffer, 0) * 2;

	unicode_string->string = (BYTE*) buffer;
	unicode_string->length = (UINT16) length;
}

BOOL rail_read_pdu_header(wStream* s, UINT16* orderType, UINT16* orderLength)
{
	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;
	Stream_Read_UINT16(s, *orderType); /* orderType (2 bytes) */
	Stream_Read_UINT16(s, *orderLength); /* orderLength (2 bytes) */
	return TRUE;
}

void rail_write_pdu_header(wStream* s, UINT16 orderType, UINT16 orderLength)
{
	Stream_Write_UINT16(s, orderType); /* orderType (2 bytes) */
	Stream_Write_UINT16(s, orderLength); /* orderLength (2 bytes) */
}

wStream* rail_pdu_init(int length)
{
	wStream* s;
	s = Stream_New(NULL, length + RAIL_PDU_HEADER_LENGTH);
	Stream_Seek(s, RAIL_PDU_HEADER_LENGTH);
	return s;
}

void rail_send_pdu(rdpRailOrder* rail_order, wStream* s, UINT16 orderType)
{
	UINT16 orderLength;

	orderLength = Stream_GetPosition(s);
	Stream_SetPosition(s, 0);

	rail_write_pdu_header(s, orderType, orderLength);
	Stream_SetPosition(s, orderLength);

	/* send */
	DEBUG_RAIL("Sending %s PDU, length:%d",
			RAIL_ORDER_TYPE_STRINGS[((orderType & 0xF0) >> 3) + (orderType & 0x0F)], orderLength);

	rail_send_channel_data(rail_order->plugin, Stream_Buffer(s), orderLength);
}

void rail_write_high_contrast(wStream* s, HIGH_CONTRAST* high_contrast)
{
	high_contrast->colorSchemeLength = high_contrast->colorScheme.length + 2;
	Stream_Write_UINT32(s, high_contrast->flags); /* flags (4 bytes) */
	Stream_Write_UINT32(s, high_contrast->colorSchemeLength); /* colorSchemeLength (4 bytes) */
	rail_write_unicode_string(s, &high_contrast->colorScheme); /* colorScheme */
}

BOOL rail_read_handshake_order(wStream* s, RAIL_HANDSHAKE_ORDER* handshake)
{
	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;
	Stream_Read_UINT32(s, handshake->buildNumber); /* buildNumber (4 bytes) */
	return TRUE;
}

BOOL rail_read_server_exec_result_order(wStream* s, RAIL_EXEC_RESULT_ORDER* exec_result)
{
	if (Stream_GetRemainingLength(s) < 8)
		return FALSE;
	Stream_Read_UINT16(s, exec_result->flags); /* flags (2 bytes) */
	Stream_Read_UINT16(s, exec_result->execResult); /* execResult (2 bytes) */
	Stream_Read_UINT32(s, exec_result->rawResult); /* rawResult (4 bytes) */
	Stream_Seek_UINT16(s); /* padding (2 bytes) */
	return rail_read_unicode_string(s, &exec_result->exeOrFile); /* exeOrFile */
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

BOOL rail_read_server_localmovesize_order(wStream* s, RAIL_LOCALMOVESIZE_ORDER* localmovesize)
{
	UINT16 isMoveSizeStart;
	if (Stream_GetRemainingLength(s) < 12)
		return FALSE;
	Stream_Read_UINT32(s, localmovesize->windowId); /* windowId (4 bytes) */

	Stream_Read_UINT16(s, isMoveSizeStart); /* isMoveSizeStart (2 bytes) */
	localmovesize->isMoveSizeStart = (isMoveSizeStart != 0) ? TRUE : FALSE;

	Stream_Read_UINT16(s, localmovesize->moveSizeType); /* moveSizeType (2 bytes) */
	Stream_Read_UINT16(s, localmovesize->posX); /* posX (2 bytes) */
	Stream_Read_UINT16(s, localmovesize->posY); /* posY (2 bytes) */
	return TRUE;
}

BOOL rail_read_server_get_appid_resp_order(wStream* s, RAIL_GET_APPID_RESP_ORDER* get_appid_resp)
{
	if (Stream_GetRemainingLength(s) < 516)
		return FALSE;
	Stream_Read_UINT32(s, get_appid_resp->windowId); /* windowId (4 bytes) */
	Stream_Read(s, &get_appid_resp->applicationIdBuffer[0], 512); /* applicationId (256 UNICODE chars) */

	get_appid_resp->applicationId.length = 512;
	get_appid_resp->applicationId.string = &get_appid_resp->applicationIdBuffer[0];
	return TRUE;
}

BOOL rail_read_langbar_info_order(wStream* s, RAIL_LANGBAR_INFO_ORDER* langbar_info)
{
	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;
	Stream_Read_UINT32(s, langbar_info->languageBarStatus); /* languageBarStatus (4 bytes) */
	return TRUE;
}

void rail_write_handshake_order(wStream* s, RAIL_HANDSHAKE_ORDER* handshake)
{
	Stream_Write_UINT32(s, handshake->buildNumber); /* buildNumber (4 bytes) */
}

void rail_write_client_status_order(wStream* s, RAIL_CLIENT_STATUS_ORDER* client_status)
{
	Stream_Write_UINT32(s, client_status->flags); /* flags (4 bytes) */
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

void rail_write_client_notify_event_order(wStream* s, RAIL_NOTIFY_EVENT_ORDER* notify_event)
{
	Stream_Write_UINT32(s, notify_event->windowId); /* windowId (4 bytes) */
	Stream_Write_UINT32(s, notify_event->notifyIconId); /* notifyIconId (4 bytes) */
	Stream_Write_UINT32(s, notify_event->message); /* notifyIconId (4 bytes) */
}

void rail_write_client_window_move_order(wStream* s, RAIL_WINDOW_MOVE_ORDER* window_move)
{
	Stream_Write_UINT32(s, window_move->windowId); /* windowId (4 bytes) */
	Stream_Write_UINT16(s, window_move->left); /* left (2 bytes) */
	Stream_Write_UINT16(s, window_move->top); /* top (2 bytes) */
	Stream_Write_UINT16(s, window_move->right); /* right (2 bytes) */
	Stream_Write_UINT16(s, window_move->bottom); /* bottom (2 bytes) */
}

void rail_write_client_get_appid_req_order(wStream* s, RAIL_GET_APPID_REQ_ORDER* get_appid_req)
{
	Stream_Write_UINT32(s, get_appid_req->windowId); /* windowId (4 bytes) */
}

void rail_write_langbar_info_order(wStream* s, RAIL_LANGBAR_INFO_ORDER* langbar_info)
{
	Stream_Write_UINT32(s, langbar_info->languageBarStatus); /* languageBarStatus (4 bytes) */
}

BOOL rail_recv_handshake_order(rdpRailOrder* rail_order, wStream* s)
{
	if (!rail_read_handshake_order(s, &rail_order->handshake))
		return FALSE;

	rail_order->handshake.buildNumber = 0x00001DB0;
	rail_send_handshake_order(rail_order);

	rail_order->client_status.flags = RAIL_CLIENTSTATUS_ALLOWLOCALMOVESIZE;
	rail_send_client_status_order(rail_order);

	/* sysparam update */

	rail_order->sysparam.params = 0;

	rail_order->sysparam.params |= SPI_MASK_SET_HIGH_CONTRAST;
	rail_order->sysparam.highContrast.colorScheme.string = NULL;
	rail_order->sysparam.highContrast.colorScheme.length = 0;
	rail_order->sysparam.highContrast.flags = 0x7E;

	rail_order->sysparam.params |= SPI_MASK_SET_MOUSE_BUTTON_SWAP;
	rail_order->sysparam.mouseButtonSwap = FALSE;

	rail_order->sysparam.params |= SPI_MASK_SET_KEYBOARD_PREF;
	rail_order->sysparam.keyboardPref = FALSE;

	rail_order->sysparam.params |= SPI_MASK_SET_DRAG_FULL_WINDOWS;
	rail_order->sysparam.dragFullWindows = FALSE;

	rail_order->sysparam.params |= SPI_MASK_SET_KEYBOARD_CUES;
	rail_order->sysparam.keyboardCues = FALSE;

	rail_order->sysparam.params |= SPI_MASK_SET_WORK_AREA;
	rail_order->sysparam.workArea.left = 0;
	rail_order->sysparam.workArea.top = 0;
	rail_order->sysparam.workArea.right = 1024;
	rail_order->sysparam.workArea.bottom = 768;

	rail_send_channel_event(rail_order->plugin,
			RailChannel_GetSystemParam, &rail_order->sysparam);

	return TRUE;
}

BOOL rail_recv_exec_result_order(rdpRailOrder* rail_order, wStream* s)
{
	if (!rail_read_server_exec_result_order(s, &rail_order->exec_result))
		return FALSE;

	rail_send_channel_event(rail_order->plugin,
		RailChannel_ServerExecuteResult, &rail_order->exec_result);

	return TRUE;
}

BOOL rail_recv_server_sysparam_order(rdpRailOrder* rail_order, wStream* s)
{
	if (!rail_read_server_sysparam_order(s, &rail_order->sysparam))
		return FALSE;

	rail_send_channel_event(rail_order->plugin,
		RailChannel_ServerSystemParam, &rail_order->sysparam);

	return TRUE;
}

BOOL rail_recv_server_minmaxinfo_order(rdpRailOrder* rail_order, wStream* s)
{
	if (!rail_read_server_minmaxinfo_order(s, &rail_order->minmaxinfo))
		return FALSE;

	rail_send_channel_event(rail_order->plugin,
		RailChannel_ServerMinMaxInfo, &rail_order->minmaxinfo);

	return TRUE;
}

BOOL rail_recv_server_localmovesize_order(rdpRailOrder* rail_order, wStream* s)
{
	if (!rail_read_server_localmovesize_order(s, &rail_order->localmovesize))
		return FALSE;

	rail_send_channel_event(rail_order->plugin,
		RailChannel_ServerLocalMoveSize, &rail_order->localmovesize);

	return TRUE;
}

BOOL rail_recv_server_get_appid_resp_order(rdpRailOrder* rail_order, wStream* s)
{
	if (!rail_read_server_get_appid_resp_order(s, &rail_order->get_appid_resp))
		return FALSE;

	rail_send_channel_event(rail_order->plugin,
		RailChannel_ServerGetAppIdResponse, &rail_order->get_appid_resp);

	return TRUE;
}

BOOL rail_recv_langbar_info_order(rdpRailOrder* rail_order, wStream* s)
{
	if (!rail_read_langbar_info_order(s, &rail_order->langbar_info))
		return FALSE;

	rail_send_channel_event(rail_order->plugin,
		RailChannel_ServerLanguageBarInfo, &rail_order->langbar_info);

	return TRUE;
}

BOOL rail_order_recv(rdpRailOrder* rail_order, wStream* s)
{
	UINT16 orderType;
	UINT16 orderLength;

	if (!rail_read_pdu_header(s, &orderType, &orderLength))
		return FALSE;

	DEBUG_RAIL("Received %s PDU, length:%d",
			RAIL_ORDER_TYPE_STRINGS[((orderType & 0xF0) >> 3) + (orderType & 0x0F)], orderLength);

	switch (orderType)
	{
		case RDP_RAIL_ORDER_HANDSHAKE:
			return rail_recv_handshake_order(rail_order, s);

		case RDP_RAIL_ORDER_EXEC_RESULT:
			return rail_recv_exec_result_order(rail_order, s);

		case RDP_RAIL_ORDER_SYSPARAM:
			return rail_recv_server_sysparam_order(rail_order, s);

		case RDP_RAIL_ORDER_MINMAXINFO:
			return rail_recv_server_minmaxinfo_order(rail_order, s);

		case RDP_RAIL_ORDER_LOCALMOVESIZE:
			return rail_recv_server_localmovesize_order(rail_order, s);

		case RDP_RAIL_ORDER_GET_APPID_RESP:
			return rail_recv_server_get_appid_resp_order(rail_order, s);

		case RDP_RAIL_ORDER_LANGBARINFO:
			return rail_recv_langbar_info_order(rail_order, s);

		default:
			fprintf(stderr, "Unknown RAIL PDU order reveived.");
			break;
	}
	return TRUE;
}

void rail_send_handshake_order(rdpRailOrder* rail_order)
{
	wStream* s;
	s = rail_pdu_init(RAIL_HANDSHAKE_ORDER_LENGTH);
	rail_write_handshake_order(s, &rail_order->handshake);
	rail_send_pdu(rail_order, s, RAIL_ORDER_TYPE_HANDSHAKE);
	Stream_Free(s, TRUE);
}

void rail_send_client_status_order(rdpRailOrder* rail_order)
{
	wStream* s;
	s = rail_pdu_init(RAIL_CLIENT_STATUS_ORDER_LENGTH);
	rail_write_client_status_order(s, &rail_order->client_status);
	rail_send_pdu(rail_order, s, RAIL_ORDER_TYPE_CLIENT_STATUS);
	Stream_Free(s, TRUE);
}

void rail_send_client_exec_order(rdpRailOrder* rail_order)
{
	wStream* s;
	int length;

	length = RAIL_EXEC_ORDER_LENGTH +
			rail_order->exec.exeOrFile.length +
			rail_order->exec.workingDir.length +
			rail_order->exec.arguments.length;

	s = rail_pdu_init(RAIL_EXEC_ORDER_LENGTH);
	rail_write_client_exec_order(s, &rail_order->exec);
	rail_send_pdu(rail_order, s, RAIL_ORDER_TYPE_EXEC);
	Stream_Free(s, TRUE);
}

void rail_send_client_sysparam_order(rdpRailOrder* rail_order)
{
	wStream* s;
	int length;

	length = RAIL_SYSPARAM_ORDER_LENGTH;

	switch (rail_order->sysparam.param)
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
			length += rail_order->sysparam.highContrast.colorSchemeLength + 10;
			break;
	}

	s = rail_pdu_init(RAIL_SYSPARAM_ORDER_LENGTH + 8);
	rail_write_client_sysparam_order(s, &rail_order->sysparam);
	rail_send_pdu(rail_order, s, RAIL_ORDER_TYPE_SYSPARAM);
	Stream_Free(s, TRUE);
}

void rail_send_client_sysparams_order(rdpRailOrder* rail_order)
{
	if (rail_order->sysparam.params & SPI_MASK_SET_HIGH_CONTRAST)
	{
		rail_order->sysparam.param = SPI_SET_HIGH_CONTRAST;
		rail_send_client_sysparam_order(rail_order);
	}

	if (rail_order->sysparam.params & SPI_MASK_TASKBAR_POS)
	{
		rail_order->sysparam.param = SPI_TASKBAR_POS;
		rail_send_client_sysparam_order(rail_order);
	}

	if (rail_order->sysparam.params & SPI_MASK_SET_MOUSE_BUTTON_SWAP)
	{
		rail_order->sysparam.param = SPI_SET_MOUSE_BUTTON_SWAP;
		rail_send_client_sysparam_order(rail_order);
	}

	if (rail_order->sysparam.params & SPI_MASK_SET_KEYBOARD_PREF)
	{
		rail_order->sysparam.param = SPI_SET_KEYBOARD_PREF;
		rail_send_client_sysparam_order(rail_order);
	}

	if (rail_order->sysparam.params & SPI_MASK_SET_DRAG_FULL_WINDOWS)
	{
		rail_order->sysparam.param = SPI_SET_DRAG_FULL_WINDOWS;
		rail_send_client_sysparam_order(rail_order);
	}

	if (rail_order->sysparam.params & SPI_MASK_SET_KEYBOARD_CUES)
	{
		rail_order->sysparam.param = SPI_SET_KEYBOARD_CUES;
		rail_send_client_sysparam_order(rail_order);
	}

	if (rail_order->sysparam.params & SPI_MASK_SET_WORK_AREA)
	{
		rail_order->sysparam.param = SPI_SET_WORK_AREA;
		rail_send_client_sysparam_order(rail_order);
	}
}

void rail_send_client_activate_order(rdpRailOrder* rail_order)
{
	wStream* s;
	s = rail_pdu_init(RAIL_ACTIVATE_ORDER_LENGTH);
	rail_write_client_activate_order(s, &rail_order->activate);
	rail_send_pdu(rail_order, s, RAIL_ORDER_TYPE_ACTIVATE);
	Stream_Free(s, TRUE);
}

void rail_send_client_sysmenu_order(rdpRailOrder* rail_order)
{
	wStream* s;
	s = rail_pdu_init(RAIL_SYSMENU_ORDER_LENGTH);
	rail_write_client_sysmenu_order(s, &rail_order->sysmenu);
	rail_send_pdu(rail_order, s, RAIL_ORDER_TYPE_SYSMENU);
	Stream_Free(s, TRUE);
}

void rail_send_client_syscommand_order(rdpRailOrder* rail_order)
{
	wStream* s;
	s = rail_pdu_init(RAIL_SYSCOMMAND_ORDER_LENGTH);
	rail_write_client_syscommand_order(s, &rail_order->syscommand);
	rail_send_pdu(rail_order, s, RAIL_ORDER_TYPE_SYSCOMMAND);
	Stream_Free(s, TRUE);
}

void rail_send_client_notify_event_order(rdpRailOrder* rail_order)
{
	wStream* s;
	s = rail_pdu_init(RAIL_NOTIFY_EVENT_ORDER_LENGTH);
	rail_write_client_notify_event_order(s, &rail_order->notify_event);
	rail_send_pdu(rail_order, s, RAIL_ORDER_TYPE_NOTIFY_EVENT);
	Stream_Free(s, TRUE);
}

void rail_send_client_window_move_order(rdpRailOrder* rail_order)
{
	wStream* s;
	s = rail_pdu_init(RAIL_WINDOW_MOVE_ORDER_LENGTH);
	rail_write_client_window_move_order(s, &rail_order->window_move);
	rail_send_pdu(rail_order, s, RAIL_ORDER_TYPE_WINDOW_MOVE);
	Stream_Free(s, TRUE);
}

void rail_send_client_get_appid_req_order(rdpRailOrder* rail_order)
{
	wStream* s;
	s = rail_pdu_init(RAIL_GET_APPID_REQ_ORDER_LENGTH);
	rail_write_client_get_appid_req_order(s, &rail_order->get_appid_req);
	rail_send_pdu(rail_order, s, RAIL_ORDER_TYPE_GET_APPID_REQ);
	Stream_Free(s, TRUE);
}

void rail_send_client_langbar_info_order(rdpRailOrder* rail_order)
{
	wStream* s;
	s = rail_pdu_init(RAIL_LANGBAR_INFO_ORDER_LENGTH);
	rail_write_langbar_info_order(s, &rail_order->langbar_info);
	rail_send_pdu(rail_order, s, RAIL_ORDER_TYPE_LANGBAR_INFO);
	Stream_Free(s, TRUE);
}

rdpRailOrder* rail_order_new()
{
	rdpRailOrder* rail_order;

	rail_order = (rdpRailOrder*) malloc(sizeof(rdpRailOrder));
	ZeroMemory(rail_order, sizeof(rdpRailOrder));

	if (rail_order != NULL)
	{

	}

	return rail_order;
}

void rail_order_free(rdpRailOrder* rail_order)
{
	if (rail_order != NULL)
	{

		free(rail_order);
	}
}

