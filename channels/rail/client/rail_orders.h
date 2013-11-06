/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote Applications Integrated Locally (RAIL)
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

#ifndef __RAIL_ORDERS_H
#define	__RAIL_ORDERS_H

#include "rail_main.h"

BOOL rail_read_server_exec_result_order(wStream* s, RAIL_EXEC_RESULT_ORDER* exec_result);
BOOL rail_read_server_sysparam_order(wStream* s, RAIL_SYSPARAM_ORDER* sysparam);
BOOL rail_read_server_minmaxinfo_order(wStream* s, RAIL_MINMAXINFO_ORDER* minmaxinfo);
BOOL rail_read_server_localmovesize_order(wStream* s, RAIL_LOCALMOVESIZE_ORDER* localmovesize);
BOOL rail_read_server_get_appid_resp_order(wStream* s, RAIL_GET_APPID_RESP_ORDER* get_appid_resp);
BOOL rail_read_langbar_info_order(wStream* s, RAIL_LANGBAR_INFO_ORDER* langbar_info);

void rail_write_client_status_order(wStream* s, RAIL_CLIENT_STATUS_ORDER* client_status);
void rail_write_client_exec_order(wStream* s, RAIL_EXEC_ORDER* exec);
void rail_write_client_sysparam_order(wStream* s, RAIL_SYSPARAM_ORDER* sysparam);
void rail_write_client_activate_order(wStream* s, RAIL_ACTIVATE_ORDER* activate);
void rail_write_client_sysmenu_order(wStream* s, RAIL_SYSMENU_ORDER* sysmenu);
void rail_write_client_syscommand_order(wStream* s, RAIL_SYSCOMMAND_ORDER* syscommand);
void rail_write_client_notify_event_order(wStream* s, RAIL_NOTIFY_EVENT_ORDER* notify_event);
void rail_write_client_window_move_order(wStream* s, RAIL_WINDOW_MOVE_ORDER* window_move);
void rail_write_client_get_appid_req_order(wStream* s, RAIL_GET_APPID_REQ_ORDER* get_appid_req);
void rail_write_langbar_info_order(wStream* s, RAIL_LANGBAR_INFO_ORDER* langbar_info);

BOOL rail_order_recv(railPlugin* rail, wStream* s);

void rail_send_pdu(railPlugin* rail, wStream* s, UINT16 orderType);

void rail_send_handshake_order(railPlugin* rail, RAIL_HANDSHAKE_ORDER* handshake);
void rail_send_handshake_ex_order(railPlugin* rail, RAIL_HANDSHAKE_EX_ORDER* handshakeEx);
void rail_send_client_status_order(railPlugin* rail, RAIL_CLIENT_STATUS_ORDER* clientStatus);
void rail_send_client_exec_order(railPlugin* rail, RAIL_EXEC_ORDER* exec);
void rail_send_client_sysparam_order(railPlugin* rail, RAIL_SYSPARAM_ORDER* sysparam);
void rail_send_client_sysparams_order(railPlugin* rail, RAIL_SYSPARAM_ORDER* sysparam);
void rail_send_client_activate_order(railPlugin* rail, RAIL_ACTIVATE_ORDER* activate);
void rail_send_client_sysmenu_order(railPlugin* rail, RAIL_SYSMENU_ORDER* sysmenu);
void rail_send_client_syscommand_order(railPlugin* rail, RAIL_SYSCOMMAND_ORDER* syscommand);
void rail_send_client_notify_event_order(railPlugin* rail, RAIL_NOTIFY_EVENT_ORDER* notifyEvent);
void rail_send_client_window_move_order(railPlugin* rail, RAIL_WINDOW_MOVE_ORDER* windowMove);
void rail_send_client_get_appid_req_order(railPlugin* rail, RAIL_GET_APPID_REQ_ORDER* getAppIdReq);
void rail_send_client_langbar_info_order(railPlugin* rail, RAIL_LANGBAR_INFO_ORDER* langBarInfo);

rdpRailOrder* rail_order_new(void);
void rail_order_free(rdpRailOrder* railOrder);

#endif /* __RAIL_ORDERS_H */
