/*
   FreeRDP: A Remote Desktop Protocol client.
   Remote Applications Integrated Locally (RAIL) Orders

   Copyright 2009 Marc-Andre Moreau <marcandre.moreau@gmail.com>
   Copyright 2011 Roman Barabanov <romanbarabanov@gmail.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <freerdp/utils/rail.h>
#include <freerdp/utils/memory.h>

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

uint8 RAIL_ORDER_TYPE_STRINGS[][32] =
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

void rail_string_to_unicode_string(rdpRail* rail, char* string, UNICODE_STRING* unicode_string)
{
	char* buffer;
	size_t length = 0;

	unicode_string->string = NULL;
	unicode_string->length = 0;

	if (strlen(string) < 1)
		return;

	buffer = freerdp_uniconv_out(rail->uniconv, string, &length);

	unicode_string->string = (uint8*) buffer;
	unicode_string->length = (uint16) length;
}

void rail_read_pdu_header(STREAM* s, uint16* orderType, uint16* orderLength)
{
	stream_read_uint16(s, *orderType); /* orderType (2 bytes) */
	stream_read_uint16(s, *orderLength); /* orderLength (2 bytes) */
}

void rail_write_pdu_header(STREAM* s, uint16 orderType, uint16 orderLength)
{
	stream_write_uint16(s, orderType); /* orderType (2 bytes) */
	stream_write_uint16(s, orderLength); /* orderLength (2 bytes) */
}

STREAM* rail_pdu_init(int length)
{
	STREAM* s;
	s = stream_new(length + RAIL_PDU_HEADER_LENGTH);
	stream_seek(s, RAIL_PDU_HEADER_LENGTH);
	return s;
}

void rail_send_pdu(rdpRail* rail, STREAM* s, uint16 orderType)
{
	uint16 orderLength;

	orderLength = stream_get_length(s);
	stream_set_pos(s, 0);

	rail_write_pdu_header(s, orderType, orderLength);
	stream_set_pos(s, orderLength);

	/* send */
	printf("Sending %s PDU, length:%d\n",
			RAIL_ORDER_TYPE_STRINGS[((orderType & 0xF0) >> 3) + (orderType & 0x0F)], orderLength);

	rail->data_sender->send_rail_vchannel_data(rail->data_sender->data_sender_object, s->data, orderLength);
}

void rail_write_high_contrast(STREAM* s, HIGH_CONTRAST* high_contrast)
{
	high_contrast->colorSchemeLength = high_contrast->colorScheme.length + 2;
	stream_write_uint32(s, high_contrast->flags); /* flags (4 bytes) */
	stream_write_uint32(s, high_contrast->colorSchemeLength); /* colorSchemeLength (4 bytes) */
	rail_write_unicode_string(s, &high_contrast->colorScheme); /* colorScheme */
}

void rail_read_handshake_order(STREAM* s, RAIL_HANDSHAKE_ORDER* handshake)
{
	stream_read_uint32(s, handshake->buildNumber); /* buildNumber (4 bytes) */
}

void rail_read_server_exec_result_order(STREAM* s, RAIL_EXEC_RESULT_ORDER* exec_result)
{
	stream_read_uint16(s, exec_result->flags); /* flags (2 bytes) */
	stream_read_uint16(s, exec_result->execResult); /* execResult (2 bytes) */
	stream_read_uint32(s, exec_result->rawResult); /* rawResult (4 bytes) */
	stream_seek_uint16(s); /* padding (2 bytes) */
	rail_read_unicode_string(s, &exec_result->exeOrFile); /* exeOrFile */
}

void rail_read_server_sysparam_order(STREAM* s, RAIL_SYSPARAM_ORDER* sysparam)
{
	uint8 body;
	stream_read_uint32(s, sysparam->systemParam); /* systemParam (4 bytes) */
	stream_read_uint8(s, body); /* body (1 byte) */
	sysparam->value = (body != 0) ? True : False;
}

void rail_read_server_minmaxinfo_order(STREAM* s, RAIL_MINMAXINFO_ORDER* minmaxinfo)
{
	stream_read_uint32(s, minmaxinfo->windowId); /* windowId (4 bytes) */
	stream_read_uint16(s, minmaxinfo->maxWidth); /* maxWidth (2 bytes) */
	stream_read_uint16(s, minmaxinfo->maxHeight); /* maxHeight (2 bytes) */
	stream_read_uint16(s, minmaxinfo->maxPosX); /* maxPosX (2 bytes) */
	stream_read_uint16(s, minmaxinfo->maxPosY); /* maxPosY (2 bytes) */
	stream_read_uint16(s, minmaxinfo->minTrackWidth); /* minTrackWidth (2 bytes) */
	stream_read_uint16(s, minmaxinfo->minTrackHeight); /* minTrackHeight (2 bytes) */
	stream_read_uint16(s, minmaxinfo->maxTrackWidth); /* maxTrackWidth (2 bytes) */
	stream_read_uint16(s, minmaxinfo->maxTrackHeight); /* maxTrackHeight (2 bytes) */
}

void rail_read_server_localmovesize_order(STREAM* s, RAIL_LOCALMOVESIZE_ORDER* localmovesize)
{
	uint16 isMoveSizeStart;
	stream_read_uint32(s, localmovesize->windowId); /* windowId (4 bytes) */

	stream_read_uint16(s, isMoveSizeStart); /* isMoveSizeStart (2 bytes) */
	localmovesize->isMoveSizeStart = (isMoveSizeStart != 0) ? True : False;

	stream_read_uint16(s, localmovesize->moveSizeType); /* moveSizeType (2 bytes) */
	stream_read_uint16(s, localmovesize->posX); /* posX (2 bytes) */
	stream_read_uint16(s, localmovesize->posY); /* posY (2 bytes) */
}

void rail_read_server_get_appid_resp_order(STREAM* s, RAIL_GET_APPID_RESP_ORDER* get_appid_resp)
{
	stream_read_uint32(s, get_appid_resp->windowId); /* windowId (4 bytes) */

	get_appid_resp->applicationId.length = 256;
	stream_read(s, get_appid_resp->applicationId.string, 256); /* applicationId (256 bytes) */
}

void rail_read_langbar_info_order(STREAM* s, RAIL_LANGBAR_INFO_ORDER* langbar_info)
{
	stream_read_uint32(s, langbar_info->languageBarStatus); /* languageBarStatus (4 bytes) */
}

void rail_write_handshake_order(STREAM* s, RAIL_HANDSHAKE_ORDER* handshake)
{
	stream_write_uint32(s, handshake->buildNumber); /* buildNumber (4 bytes) */
}

void rail_write_client_status_order(STREAM* s, RAIL_CLIENT_STATUS_ORDER* client_status)
{
	stream_write_uint32(s, client_status->flags); /* flags (4 bytes) */
}

void rail_write_client_exec_order(STREAM* s, RAIL_EXEC_ORDER* exec)
{
	stream_write_uint16(s, exec->flags); /* flags (2 bytes) */
	stream_write_uint16(s, exec->exeOrFile.length); /* exeOrFileLength (2 bytes) */
	stream_write_uint16(s, exec->workingDir.length); /* workingDirLength (2 bytes) */
	stream_write_uint16(s, exec->arguments.length); /* argumentsLength (2 bytes) */
	rail_write_unicode_string_value(s, &exec->exeOrFile); /* exeOrFile */
	rail_write_unicode_string_value(s, &exec->workingDir); /* workingDir */
	rail_write_unicode_string_value(s, &exec->arguments); /* arguments */
}

void rail_write_client_sysparam_order(STREAM* s, RAIL_SYSPARAM_ORDER* sysparam)
{
	uint8 body;
	stream_write_uint32(s, sysparam->systemParam); /* systemParam (4 bytes) */

	switch (sysparam->systemParam)
	{
		case SPI_SET_DRAG_FULL_WINDOWS:
		case SPI_SET_KEYBOARD_CUES:
		case SPI_SET_KEYBOARD_PREF:
		case SPI_SET_MOUSE_BUTTON_SWAP:
			body = sysparam->value;
			stream_write_uint8(s, body);
			break;

		case SPI_SET_WORK_AREA:
		case SPI_DISPLAY_CHANGE:
		case SPI_TASKBAR_POS:
			rail_write_rectangle_16(s, &sysparam->rectangle);
			break;

		case SPI_SET_HIGH_CONTRAST:
			rail_write_high_contrast(s, &sysparam->highContrast);
			break;
	}
}

void rail_write_client_activate_order(STREAM* s, RAIL_ACTIVATE_ORDER* activate)
{
	uint8 enabled;

	stream_write_uint32(s, activate->windowId); /* windowId (4 bytes) */

	enabled = activate->enabled;
	stream_write_uint8(s, enabled); /* enabled (1 byte) */
}

void rail_write_client_sysmenu_order(STREAM* s, RAIL_SYSMENU_ORDER* sysmenu)
{
	stream_write_uint32(s, sysmenu->windowId); /* windowId (4 bytes) */
	stream_write_uint16(s, sysmenu->left); /* left (2 bytes) */
	stream_write_uint16(s, sysmenu->top); /* top (2 bytes) */
}

void rail_write_client_syscommand_order(STREAM* s, RAIL_SYSCOMMAND_ORDER* syscommand)
{
	stream_write_uint32(s, syscommand->windowId); /* windowId (4 bytes) */
	stream_write_uint16(s, syscommand->command); /* command (2 bytes) */
}

void rail_write_client_notify_event_order(STREAM* s, RAIL_NOTIFY_EVENT_ORDER* notify_event)
{
	stream_write_uint32(s, notify_event->windowId); /* windowId (4 bytes) */
	stream_write_uint32(s, notify_event->notifyIconId); /* notifyIconId (4 bytes) */
	stream_write_uint32(s, notify_event->message); /* notifyIconId (4 bytes) */
}

void rail_write_client_window_move_order(STREAM* s, RAIL_WINDOW_MOVE_ORDER* window_move)
{
	stream_write_uint32(s, window_move->windowId); /* windowId (4 bytes) */
	stream_write_uint16(s, window_move->left); /* left (2 bytes) */
	stream_write_uint16(s, window_move->top); /* top (2 bytes) */
	stream_write_uint16(s, window_move->right); /* right (2 bytes) */
	stream_write_uint16(s, window_move->bottom); /* bottom (2 bytes) */
}

void rail_write_client_get_appid_req_order(STREAM* s, RAIL_GET_APPID_REQ_ORDER* get_appid_req)
{
	stream_write_uint32(s, get_appid_req->windowId); /* windowId (4 bytes) */
}

void rail_write_langbar_info_order(STREAM* s, RAIL_LANGBAR_INFO_ORDER* langbar_info)
{
	stream_write_uint32(s, langbar_info->languageBarStatus); /* languageBarStatus (4 bytes) */
}

void rail_recv_handshake_order(rdpRail* rail, STREAM* s)
{
	rail_read_handshake_order(s, &rail->handshake);

	rail->handshake.buildNumber = 0x00001DB1;
	rail_send_handshake_order(rail);

	rail->client_status.flags = RAIL_CLIENTSTATUS_ALLOWLOCALMOVESIZE;
	rail_send_client_status_order(rail);

	/* sysparam update */

	rail->sysparam.systemParam = SPI_SET_HIGH_CONTRAST;
	rail->sysparam.highContrast.colorScheme.string = NULL;
	rail->sysparam.highContrast.colorScheme.length = 0;
	rail->sysparam.highContrast.flags = 0x7E;
	rail_send_client_sysparam_order(rail);

	rail->sysparam.systemParam = SPI_TASKBAR_POS;
	rail->sysparam.rectangle.left = 0;
	rail->sysparam.rectangle.top = 0;
	rail->sysparam.rectangle.right = 1024;
	rail->sysparam.rectangle.bottom = 29;
	rail_send_client_sysparam_order(rail);

	rail->sysparam.systemParam = SPI_SET_MOUSE_BUTTON_SWAP;
	rail->sysparam.value = False;
	rail_send_client_sysparam_order(rail);

	rail->sysparam.systemParam = SPI_SET_KEYBOARD_PREF;
	rail->sysparam.value = False;
	rail_send_client_sysparam_order(rail);

	rail->sysparam.systemParam = SPI_SET_DRAG_FULL_WINDOWS;
	rail->sysparam.value = False;
	rail_send_client_sysparam_order(rail);

	rail->sysparam.systemParam = SPI_SET_KEYBOARD_CUES;
	rail->sysparam.value = False;
	rail_send_client_sysparam_order(rail);

	rail->sysparam.systemParam = SPI_SET_WORK_AREA;
	rail->sysparam.rectangle.left = 0;
	rail->sysparam.rectangle.top = 0;
	rail->sysparam.rectangle.right = 1024;
	rail->sysparam.rectangle.bottom = 768;
	rail_send_client_sysparam_order(rail);

	/* execute */

	rail->exec.flags =
			RAIL_EXEC_FLAG_EXPAND_WORKINGDIRECTORY |
			RAIL_EXEC_FLAG_EXPAND_ARGUMENTS;

	rail_string_to_unicode_string(rail, "||cmd", &rail->exec.exeOrFile);
	rail_string_to_unicode_string(rail, "", &rail->exec.workingDir);
	rail_string_to_unicode_string(rail, "", &rail->exec.arguments);

	rail_send_client_exec_order(rail);
}

void rail_order_recv(rdpRail* rail, STREAM* s)
{
	uint16 orderType;
	uint16 orderLength;

	rail_read_pdu_header(s, &orderType, &orderLength);

	printf("Received %s PDU, length:%d\n",
			RAIL_ORDER_TYPE_STRINGS[((orderType & 0xF0) >> 3) + (orderType & 0x0F)], orderLength);

	switch (orderType)
	{
		case RDP_RAIL_ORDER_HANDSHAKE:
			rail_recv_handshake_order(rail, s);
			break;

		case RDP_RAIL_ORDER_EXEC_RESULT:
			rail_read_server_exec_result_order(s, &rail->exec_result);
			break;

		case RDP_RAIL_ORDER_SYSPARAM:
			rail_read_server_sysparam_order(s, &rail->sysparam);
			break;

		case RDP_RAIL_ORDER_MINMAXINFO:
			rail_read_server_minmaxinfo_order(s, &rail->minmaxinfo);
			break;

		case RDP_RAIL_ORDER_LOCALMOVESIZE:
			rail_read_server_localmovesize_order(s, &rail->localmovesize);
			break;

		case RDP_RAIL_ORDER_GET_APPID_RESP:
			rail_read_server_get_appid_resp_order(s, &rail->get_appid_resp);
			break;

		case RDP_RAIL_ORDER_LANGBARINFO:
			rail_read_langbar_info_order(s, &rail->langbar_info);
			break;

		default:
			break;
	}
}

void rail_send_handshake_order(rdpRail* rail)
{
	STREAM* s;
	s = rail_pdu_init(RAIL_HANDSHAKE_ORDER_LENGTH);
	rail_write_handshake_order(s, &rail->handshake);
	rail_send_pdu(rail, s, RAIL_ORDER_TYPE_HANDSHAKE);
}

void rail_send_client_status_order(rdpRail* rail)
{
	STREAM* s;
	s = rail_pdu_init(RAIL_CLIENT_STATUS_ORDER_LENGTH);
	rail_write_client_status_order(s, &rail->client_status);
	rail_send_pdu(rail, s, RAIL_ORDER_TYPE_CLIENT_STATUS);
}

void rail_send_client_exec_order(rdpRail* rail)
{
	STREAM* s;
	int length;

	length = RAIL_EXEC_ORDER_LENGTH +
			rail->exec.exeOrFile.length +
			rail->exec.workingDir.length +
			rail->exec.arguments.length;

	s = rail_pdu_init(RAIL_EXEC_ORDER_LENGTH);
	rail_write_client_exec_order(s, &rail->exec);
	rail_send_pdu(rail, s, RAIL_ORDER_TYPE_EXEC);
}

void rail_send_client_sysparam_order(rdpRail* rail)
{
	STREAM* s;
	int length;

	length = RAIL_SYSPARAM_ORDER_LENGTH;

	switch (rail->sysparam.systemParam)
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
			length += rail->sysparam.highContrast.colorSchemeLength + 10;
			break;
	}

	s = rail_pdu_init(RAIL_SYSPARAM_ORDER_LENGTH);
	rail_write_client_sysparam_order(s, &rail->sysparam);
	rail_send_pdu(rail, s, RAIL_ORDER_TYPE_SYSPARAM);
}

void rail_send_client_activate_order(rdpRail* rail)
{
	STREAM* s;
	s = rail_pdu_init(RAIL_ACTIVATE_ORDER_LENGTH);
	rail_write_client_activate_order(s, &rail->activate);
	rail_send_pdu(rail, s, RAIL_ORDER_TYPE_ACTIVATE);
}

void rail_send_client_sysmenu_order(rdpRail* rail)
{
	STREAM* s;
	s = rail_pdu_init(RAIL_SYSMENU_ORDER_LENGTH);
	rail_write_client_sysmenu_order(s, &rail->sysmenu);
	rail_send_pdu(rail, s, RAIL_ORDER_TYPE_SYSMENU);
}

void rail_send_client_syscommand_order(rdpRail* rail)
{
	STREAM* s;
	s = rail_pdu_init(RAIL_SYSCOMMAND_ORDER_LENGTH);
	rail_write_client_syscommand_order(s, &rail->syscommand);
	rail_send_pdu(rail, s, RAIL_ORDER_TYPE_SYSCOMMAND);
}

void rail_send_client_notify_event_order(rdpRail* rail)
{
	STREAM* s;
	s = rail_pdu_init(RAIL_NOTIFY_EVENT_ORDER_LENGTH);
	rail_write_client_notify_event_order(s, &rail->notify_event);
	rail_send_pdu(rail, s, RAIL_ORDER_TYPE_NOTIFY_EVENT);
}

void rail_send_client_window_move_order(rdpRail* rail)
{
	STREAM* s;
	s = rail_pdu_init(RAIL_WINDOW_MOVE_ORDER_LENGTH);
	rail_write_client_window_move_order(s, &rail->window_move);
	rail_send_pdu(rail, s, RAIL_ORDER_TYPE_WINDOW_MOVE);
}

void rail_send_client_get_appid_req_order(rdpRail* rail)
{
	STREAM* s;
	s = rail_pdu_init(RAIL_GET_APPID_REQ_ORDER_LENGTH);
	rail_write_client_get_appid_req_order(s, &rail->get_appid_req);
	rail_send_pdu(rail, s, RAIL_ORDER_TYPE_GET_APPID_REQ);
}

void rail_send_client_langbar_info_order(rdpRail* rail)
{
	STREAM* s;
	s = rail_pdu_init(RAIL_LANGBAR_INFO_ORDER_LENGTH);
	rail_write_langbar_info_order(s, &rail->langbar_info);
	rail_send_pdu(rail, s, RAIL_ORDER_TYPE_LANGBAR_INFO);
}

rdpRail* rail_new()
{
	rdpRail* rail;

	rail = (rdpRail*) xzalloc(sizeof(rdpRail));

	if (rail != NULL)
	{
		rail->uniconv = freerdp_uniconv_new();
	}

	return rail;
}

void rail_free(rdpRail* rail)
{
	if (rail != NULL)
	{
		freerdp_uniconv_free(rail->uniconv);
		xfree(rail);
	}
}

