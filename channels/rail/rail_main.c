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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/constants.h>
#include <freerdp/types.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/svc_plugin.h>
#include <freerdp/utils/rail.h>
#include <freerdp/rail.h>

#include "rail_orders.h"
#include "rail_main.h"

void rail_send_channel_data(void* rail_object, void* data, size_t length)
{
	STREAM* s = NULL;
	railPlugin* plugin = (railPlugin*) rail_object;

	s = stream_new(length);
	stream_write(s, data, length);

	svc_plugin_send((rdpSvcPlugin*) plugin, s);
}

static void on_free_rail_channel_event(RDP_EVENT* event)
{
	if (event->event_class == RDP_EVENT_CLASS_RAIL)
	{
		rail_free_cloned_order(event->event_type, event->user_data);
	}
}

void rail_send_channel_event(void* rail_object, uint16 event_type, void* param)
{
	void * payload = NULL;
	RDP_EVENT* out_event = NULL;
	railPlugin* plugin = (railPlugin*) rail_object;

	payload = rail_clone_order(event_type, param);

	if (payload != NULL)
	{
		out_event = freerdp_event_new(RDP_EVENT_CLASS_RAIL, event_type,
			on_free_rail_channel_event, payload);

		svc_plugin_send_event((rdpSvcPlugin*) plugin, out_event);
	}
}

static void rail_process_connect(rdpSvcPlugin* plugin)
{
	railPlugin* rail = (railPlugin*) plugin;

	rail->rail_order = rail_order_new();
	rail->rail_order->plugin_data = (RDP_PLUGIN_DATA*)plugin->channel_entry_points.pExtendedData;
	rail->rail_order->plugin = rail;
}

static void rail_process_terminate(rdpSvcPlugin* plugin)
{

}

static void rail_process_receive(rdpSvcPlugin* plugin, STREAM* s)
{
	railPlugin* rail = (railPlugin*) plugin;
	rail_order_recv(rail->rail_order, s);
	stream_free(s);
}

static void rail_process_plugin_data(rdpRailOrder* rail_order, RDP_PLUGIN_DATA* data)
{
	char* exeOrFile;

	exeOrFile = (char*) data->data[0];

	if (strlen(exeOrFile) >= 2)
	{
		if (strncmp(exeOrFile, "||", 2) != 0)
			rail_order->exec.flags |= RAIL_EXEC_FLAG_FILE;
	}

	rail_string_to_unicode_string(rail_order, (char*) data->data[0], &rail_order->exec.exeOrFile);
	rail_string_to_unicode_string(rail_order, (char*) data->data[1], &rail_order->exec.workingDir);
	rail_string_to_unicode_string(rail_order, (char*) data->data[2], &rail_order->exec.arguments);

	rail_send_client_exec_order(rail_order);
}

static void rail_recv_set_sysparams_event(rdpRailOrder* rail_order, RDP_EVENT* event)
{
	RDP_PLUGIN_DATA* data;
	RAIL_SYSPARAM_ORDER* sysparam;

	/* Send System Parameters */

	sysparam = (RAIL_SYSPARAM_ORDER*)event->user_data;
	memmove(&rail_order->sysparam, sysparam, sizeof(RAIL_SYSPARAM_ORDER));

	rail_send_client_sysparams_order(rail_order);

	/* execute */

	rail_order->exec.flags = RAIL_EXEC_FLAG_EXPAND_ARGUMENTS;

	data = rail_order->plugin_data;
	while (data && data->size > 0)
	{
		rail_process_plugin_data(rail_order, data);
		data = (RDP_PLUGIN_DATA*)(((void*) data) + data->size);
	}
}

static void rail_recv_exec_remote_app_event(rdpRailOrder* rail_order, RDP_EVENT* event)
{
	RDP_PLUGIN_DATA* data = (RDP_PLUGIN_DATA*) event->user_data;

	rail_process_plugin_data(rail_order, data);
}

static void rail_recv_activate_event(rdpRailOrder* rail_order, RDP_EVENT* event)
{
	RAIL_ACTIVATE_ORDER* activate = (RAIL_ACTIVATE_ORDER*) event->user_data;

	memcpy(&rail_order->activate, activate, sizeof(RAIL_ACTIVATE_ORDER));
	rail_send_client_activate_order(rail_order);
}

static void rail_recv_sysmenu_event(rdpRailOrder* rail_order, RDP_EVENT* event)
{
	RAIL_SYSMENU_ORDER* sysmenu = (RAIL_SYSMENU_ORDER*) event->user_data;

	memcpy(&rail_order->sysmenu, sysmenu, sizeof(RAIL_SYSMENU_ORDER));
	rail_send_client_sysmenu_order(rail_order);
}

static void rail_recv_syscommand_event(rdpRailOrder* rail_order, RDP_EVENT* event)
{
	RAIL_SYSCOMMAND_ORDER* syscommand = (RAIL_SYSCOMMAND_ORDER*) event->user_data;

	memcpy(&rail_order->syscommand, syscommand, sizeof(RAIL_SYSCOMMAND_ORDER));
	rail_send_client_syscommand_order(rail_order);
}

static void rail_recv_notify_event(rdpRailOrder* rail_order, RDP_EVENT* event)
{
	RAIL_NOTIFY_EVENT_ORDER* notify = (RAIL_NOTIFY_EVENT_ORDER*) event->user_data;

	memcpy(&rail_order->notify_event, notify, sizeof(RAIL_NOTIFY_EVENT_ORDER));
	rail_send_client_notify_event_order(rail_order);
}

static void rail_recv_window_move_event(rdpRailOrder* rail_order, RDP_EVENT* event)
{
	RAIL_WINDOW_MOVE_ORDER* window_move = (RAIL_WINDOW_MOVE_ORDER*) event->user_data;

	memcpy(&rail_order->window_move, window_move, sizeof(RAIL_WINDOW_MOVE_ORDER));
	rail_send_client_window_move_order(rail_order);
}

static void rail_recv_app_req_event(rdpRailOrder* rail_order, RDP_EVENT* event)
{
	RAIL_GET_APPID_REQ_ORDER* get_appid_req = (RAIL_GET_APPID_REQ_ORDER*) event->user_data;

	memcpy(&rail_order->get_appid_req, get_appid_req, sizeof(RAIL_GET_APPID_REQ_ORDER));
	rail_send_client_get_appid_req_order(rail_order);
}

static void rail_recv_langbarinfo_event(rdpRailOrder* rail_order, RDP_EVENT* event)
{
	RAIL_LANGBAR_INFO_ORDER* langbar_info = (RAIL_LANGBAR_INFO_ORDER*) event->user_data;

	memcpy(&rail_order->langbar_info, langbar_info, sizeof(RAIL_LANGBAR_INFO_ORDER));
	rail_send_client_langbar_info_order(rail_order);
}

static void rail_process_event(rdpSvcPlugin* plugin, RDP_EVENT* event)
{
	railPlugin* rail = NULL;
	rail = (railPlugin*) plugin;

	switch (event->event_type)
	{
		case RDP_EVENT_TYPE_RAIL_CLIENT_SET_SYSPARAMS:
			rail_recv_set_sysparams_event(rail->rail_order, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CLIENT_EXEC_REMOTE_APP:
			rail_recv_exec_remote_app_event(rail->rail_order, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CLIENT_ACTIVATE:
			rail_recv_activate_event(rail->rail_order, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CLIENT_SYSMENU:
			rail_recv_sysmenu_event(rail->rail_order, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CLIENT_SYSCOMMAND:
			rail_recv_syscommand_event(rail->rail_order, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CLIENT_NOTIFY_EVENT:
			rail_recv_notify_event(rail->rail_order, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CLIENT_WINDOW_MOVE:
			rail_recv_window_move_event(rail->rail_order, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CLIENT_APPID_REQ:
			rail_recv_app_req_event(rail->rail_order, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CLIENT_LANGBARINFO:
			rail_recv_langbarinfo_event(rail->rail_order, event);
			break;

		default:
			break;
	}

	freerdp_event_free(event);
}

DEFINE_SVC_PLUGIN(rail, "rail", 
	CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP |
	CHANNEL_OPTION_COMPRESS_RDP | CHANNEL_OPTION_SHOW_PROTOCOL)

