/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#ifndef __RAIL_MAIN_H
#define	__RAIL_MAIN_H

#include <freerdp/rail.h>
#include <freerdp/utils/debug.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/svc_plugin.h>

struct rdp_rail_order
{
	UNICONV* uniconv;
	RDP_PLUGIN_DATA* plugin_data;
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

struct rail_plugin
{
	rdpSvcPlugin plugin;
	rdpRailOrder* rail_order;
};
typedef struct rail_plugin railPlugin;

void rail_send_channel_event(void* rail_object, uint16 event_type, void* param);
void rail_send_channel_data(void* rail_object, void* data, size_t length);

#ifdef WITH_DEBUG_RAIL
#define DEBUG_RAIL(fmt, ...) DEBUG_CLASS(RAIL, fmt, ## __VA_ARGS__)
#else
#define DEBUG_RAIL(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* __RAIL_MAIN_H */
