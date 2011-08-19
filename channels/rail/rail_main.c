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

}

void rail_send_channel_event(void* rail_object, uint16 event_type, void* param)
{
	RDP_EVENT* out_event = NULL;
	railPlugin* plugin = (railPlugin*) rail_object;
	out_event = freerdp_event_new(RDP_EVENT_CLASS_RAIL, event_type, on_free_rail_channel_event, param);
	svc_plugin_send_event((rdpSvcPlugin*) plugin, out_event);
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

void rail_recv_set_sysparams_event(rdpRailOrder* rail_order, RDP_EVENT* event)
{
	RDP_PLUGIN_DATA* data;

	/* Send System Parameters */

	rail_send_client_sysparams_order(rail_order);

	/* execute */

	rail_order->exec.flags =
			RAIL_EXEC_FLAG_EXPAND_WORKINGDIRECTORY |
			RAIL_EXEC_FLAG_EXPAND_ARGUMENTS;

	data = rail_order->plugin_data;
	while (data && data->size > 0)
	{
		rail_string_to_unicode_string(rail_order, (char*)data->data[0], &rail_order->exec.exeOrFile);
		rail_string_to_unicode_string(rail_order, (char*)data->data[1], &rail_order->exec.workingDir);
		rail_string_to_unicode_string(rail_order, (char*)data->data[2], &rail_order->exec.arguments);

		rail_send_client_exec_order(rail_order);

		data = (RDP_PLUGIN_DATA*)(((void*)data) + data->size);
	}
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

		default:
			break;
	}

	freerdp_event_free(event);
}

DEFINE_SVC_PLUGIN(rail, "rail", 
	CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP |
	CHANNEL_OPTION_COMPRESS_RDP | CHANNEL_OPTION_SHOW_PROTOCOL)

