/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote Applications Integrated Locally (RAIL)
 *
 * Copyright 2009 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2011 Roman Barabanov <romanbarabanov@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include <freerdp/channels/log.h>

#include "rail_main.h"

#define TAG CHANNELS_TAG("rail.client")

UINT rail_read_server_exec_result_order(wStream* s, RAIL_EXEC_RESULT_ORDER* exec_result);
UINT rail_read_server_sysparam_order(wStream* s, RAIL_SYSPARAM_ORDER* sysparam);
UINT rail_read_server_minmaxinfo_order(wStream* s, RAIL_MINMAXINFO_ORDER* minmaxinfo);
UINT rail_read_server_localmovesize_order(wStream* s, RAIL_LOCALMOVESIZE_ORDER* localmovesize);
UINT rail_read_server_get_appid_resp_order(wStream* s, RAIL_GET_APPID_RESP_ORDER* get_appid_resp);
UINT rail_read_langbar_info_order(wStream* s, RAIL_LANGBAR_INFO_ORDER* langbar_info);

void rail_write_client_status_order(wStream* s, RAIL_CLIENT_STATUS_ORDER* client_status);
UINT rail_write_client_exec_order(wStream* s, RAIL_EXEC_ORDER* exec);
UINT rail_write_client_sysparam_order(wStream* s, RAIL_SYSPARAM_ORDER* sysparam);
void rail_write_client_activate_order(wStream* s, RAIL_ACTIVATE_ORDER* activate);
void rail_write_client_sysmenu_order(wStream* s, RAIL_SYSMENU_ORDER* sysmenu);
void rail_write_client_syscommand_order(wStream* s, RAIL_SYSCOMMAND_ORDER* syscommand);
void rail_write_client_notify_event_order(wStream* s, RAIL_NOTIFY_EVENT_ORDER* notify_event);
void rail_write_client_window_move_order(wStream* s, RAIL_WINDOW_MOVE_ORDER* window_move);
void rail_write_client_get_appid_req_order(wStream* s, RAIL_GET_APPID_REQ_ORDER* get_appid_req);
void rail_write_langbar_info_order(wStream* s, RAIL_LANGBAR_INFO_ORDER* langbar_info);

UINT rail_order_recv(railPlugin* rail, wStream* s);
UINT rail_send_pdu(railPlugin* rail, wStream* s, UINT16 orderType);

UINT rail_send_handshake_order(railPlugin* rail, RAIL_HANDSHAKE_ORDER* handshake);
UINT rail_send_handshake_ex_order(railPlugin* rail, RAIL_HANDSHAKE_EX_ORDER* handshakeEx);
UINT rail_send_client_status_order(railPlugin* rail, RAIL_CLIENT_STATUS_ORDER* clientStatus);
UINT rail_send_client_exec_order(railPlugin* rail, RAIL_EXEC_ORDER* exec);
UINT rail_send_client_sysparam_order(railPlugin* rail, RAIL_SYSPARAM_ORDER* sysparam);
UINT rail_send_client_sysparams_order(railPlugin* rail, RAIL_SYSPARAM_ORDER* sysparam);
UINT rail_send_client_activate_order(railPlugin* rail, RAIL_ACTIVATE_ORDER* activate);
UINT rail_send_client_sysmenu_order(railPlugin* rail, RAIL_SYSMENU_ORDER* sysmenu);
UINT rail_send_client_syscommand_order(railPlugin* rail, RAIL_SYSCOMMAND_ORDER* syscommand);
UINT rail_send_client_notify_event_order(railPlugin* rail, RAIL_NOTIFY_EVENT_ORDER* notifyEvent);
UINT rail_send_client_window_move_order(railPlugin* rail, RAIL_WINDOW_MOVE_ORDER* windowMove);
UINT rail_send_client_get_appid_req_order(railPlugin* rail, RAIL_GET_APPID_REQ_ORDER* getAppIdReq);
UINT rail_send_client_langbar_info_order(railPlugin* rail, RAIL_LANGBAR_INFO_ORDER* langBarInfo);

#endif /* __RAIL_ORDERS_H */
