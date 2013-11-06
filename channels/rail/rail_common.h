/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RAIL Virtual Channel Plugin
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2011 Roman Barabanov <romanbarabanov@gmail.com>
 * Copyright 2011 Vic Lee
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

#ifndef FREERDP_CHANNEL_RAIL_COMMON_H
#define FREERDP_CHANNEL_RAIL_COMMON_H

#include <freerdp/rail.h>

extern const char* const RAIL_ORDER_TYPE_STRINGS[];

#define RAIL_PDU_HEADER_LENGTH			4

/* Fixed length of PDUs, excluding variable lengths */
#define RAIL_HANDSHAKE_ORDER_LENGTH		4 /* fixed */
#define RAIL_HANDSHAKE_EX_ORDER_LENGTH		8 /* fixed */
#define RAIL_CLIENT_STATUS_ORDER_LENGTH		4 /* fixed */
#define RAIL_EXEC_ORDER_LENGTH			8 /* variable */
#define RAIL_SYSPARAM_ORDER_LENGTH		4 /* variable */
#define RAIL_ACTIVATE_ORDER_LENGTH		5 /* fixed */
#define RAIL_SYSMENU_ORDER_LENGTH		8 /* fixed */
#define RAIL_SYSCOMMAND_ORDER_LENGTH		6 /* fixed */
#define RAIL_NOTIFY_EVENT_ORDER_LENGTH		12 /* fixed */
#define RAIL_WINDOW_MOVE_ORDER_LENGTH		12 /* fixed */
#define RAIL_GET_APPID_REQ_ORDER_LENGTH		4 /* fixed */
#define RAIL_LANGBAR_INFO_ORDER_LENGTH		4 /* fixed */

struct rdp_rail_order
{
	rdpSettings* settings;
	void* plugin;
	RAIL_HANDSHAKE_ORDER handshake;
	RAIL_CLIENT_STATUS_ORDER client_status;
	RAIL_EXEC_ORDER exec;
	RAIL_EXEC_RESULT_ORDER exec_result;
	RAIL_SYSPARAM_ORDER sysparam;
	RAIL_ACTIVATE_ORDER activate;
	RAIL_SYSMENU_ORDER sysmenu;
	RAIL_SYSCOMMAND_ORDER syscommand;
	RAIL_NOTIFY_EVENT_ORDER notify_event;
	RAIL_MINMAXINFO_ORDER minmaxinfo;
	RAIL_LOCALMOVESIZE_ORDER localmovesize;
	RAIL_WINDOW_MOVE_ORDER window_move;
	RAIL_LANGBAR_INFO_ORDER langbar_info;
	RAIL_GET_APPID_REQ_ORDER get_appid_req;
	RAIL_GET_APPID_RESP_ORDER get_appid_resp;
};
typedef struct rdp_rail_order rdpRailOrder;


void rail_string_to_unicode_string(char* string, RAIL_UNICODE_STRING* unicode_string);
BOOL rail_read_handshake_order(wStream* s, RAIL_HANDSHAKE_ORDER* handshake);
void rail_write_handshake_order(wStream* s, RAIL_HANDSHAKE_ORDER* handshake);
BOOL rail_read_handshake_ex_order(wStream* s, RAIL_HANDSHAKE_EX_ORDER* handshakeEx);
void rail_write_handshake_ex_order(wStream* s, RAIL_HANDSHAKE_EX_ORDER* handshakeEx);

wStream* rail_pdu_init(int length);
BOOL rail_read_pdu_header(wStream* s, UINT16* orderType, UINT16* orderLength);
void rail_write_pdu_header(wStream* s, UINT16 orderType, UINT16 orderLength);

#endif /* FREERDP_CHANNEL_RAIL_COMMON_H */
