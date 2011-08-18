/**
 * FreeRDP: A Remote Desktop Protocol client.
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

#define RAIL_PDU_HEADER_LENGTH			4

/* Fixed length of PDUs, excluding variable lengths */
#define RAIL_HANDSHAKE_ORDER_LENGTH		4 /* fixed */
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

void rail_string_to_unicode_string(rdpRailOrder* rail_order, char* string, UNICODE_STRING* unicode_string);

void rail_read_handshake_order(STREAM* s, RAIL_HANDSHAKE_ORDER* handshake);
void rail_read_server_exec_result_order(STREAM* s, RAIL_EXEC_RESULT_ORDER* exec_result);
void rail_read_server_sysparam_order(STREAM* s, RAIL_SYSPARAM_ORDER* sysparam);
void rail_read_server_minmaxinfo_order(STREAM* s, RAIL_MINMAXINFO_ORDER* minmaxinfo);
void rail_read_server_localmovesize_order(STREAM* s, RAIL_LOCALMOVESIZE_ORDER* localmovesize);
void rail_read_server_get_appid_resp_order(STREAM* s, RAIL_GET_APPID_RESP_ORDER* get_appid_resp);
void rail_read_langbar_info_order(STREAM* s, RAIL_LANGBAR_INFO_ORDER* langbar_info);

void rail_write_handshake_order(STREAM* s, RAIL_HANDSHAKE_ORDER* handshake);
void rail_write_client_status_order(STREAM* s, RAIL_CLIENT_STATUS_ORDER* client_status);
void rail_write_client_exec_order(STREAM* s, RAIL_EXEC_ORDER* exec);
void rail_write_client_sysparam_order(STREAM* s, RAIL_SYSPARAM_ORDER* sysparam);
void rail_write_client_activate_order(STREAM* s, RAIL_ACTIVATE_ORDER* activate);
void rail_write_client_sysmenu_order(STREAM* s, RAIL_SYSMENU_ORDER* sysmenu);
void rail_write_client_syscommand_order(STREAM* s, RAIL_SYSCOMMAND_ORDER* syscommand);
void rail_write_client_notify_event_order(STREAM* s, RAIL_NOTIFY_EVENT_ORDER* notify_event);
void rail_write_client_window_move_order(STREAM* s, RAIL_WINDOW_MOVE_ORDER* window_move);
void rail_write_client_get_appid_req_order(STREAM* s, RAIL_GET_APPID_REQ_ORDER* get_appid_req);
void rail_write_langbar_info_order(STREAM* s, RAIL_LANGBAR_INFO_ORDER* langbar_info);

void rail_order_recv(rdpRailOrder* rail_order, STREAM* s);

void rail_send_handshake_order(rdpRailOrder* rail_order);
void rail_send_client_status_order(rdpRailOrder* rail_order);
void rail_send_client_exec_order(rdpRailOrder* rail_order);
void rail_send_client_sysparam_order(rdpRailOrder* rail_order);
void rail_send_client_sysparams_order(rdpRailOrder* rail_order);
void rail_send_client_activate_order(rdpRailOrder* rail_order);
void rail_send_client_sysmenu_order(rdpRailOrder* rail_order);
void rail_send_client_syscommand_order(rdpRailOrder* rail_order);
void rail_send_client_notify_event_order(rdpRailOrder* rail_order);
void rail_send_client_window_move_order(rdpRailOrder* rail_order);
void rail_send_client_get_appid_req_order(rdpRailOrder* rail_order);
void rail_send_client_langbar_info_order(rdpRailOrder* rail_order);

rdpRailOrder* rail_order_new();
void rail_order_free(rdpRailOrder* rail_order);

#endif /* __RAIL_ORDERS_H */
