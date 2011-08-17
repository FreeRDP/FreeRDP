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

static void rail_send_channel_data(void* rail_object, void* data, size_t length)
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

static void rail_send_channel_event(void* rail_object, RAIL_CHANNEL_EVENT* event)
{
	railPlugin* plugin = (railPlugin*) rail_object;
	RAIL_CHANNEL_EVENT* payload = NULL;
	RDP_EVENT* out_event = NULL;

	payload = xnew(RAIL_CHANNEL_EVENT);
	memset(payload, 0, sizeof(RAIL_CHANNEL_EVENT));
	memcpy(payload, event, sizeof(RAIL_CHANNEL_EVENT));

	out_event = freerdp_event_new(RDP_EVENT_CLASS_RAIL, RDP_EVENT_TYPE_RAIL_CHANNEL, on_free_rail_channel_event, payload);

	svc_plugin_send_event((rdpSvcPlugin*) plugin, out_event);
}

static void rail_process_connect(rdpSvcPlugin* plugin)
{
	railPlugin* rail = (railPlugin*) plugin;

	rail->rail_event_sender.event_sender_object = rail;
	rail->rail_event_sender.send_rail_vchannel_event = rail_send_channel_event;

	rail->rail_data_sender.data_sender_object  = rail;
	rail->rail_data_sender.send_rail_vchannel_data = rail_send_channel_data;

	rail->rail_order = rail_order_new();
	rail->rail_order->plugin_data = (RDP_PLUGIN_DATA*)plugin->channel_entry_points.pExtendedData;
	rail->rail_order->data_sender = &(rail->rail_data_sender);
	rail->rail_order->event_sender = &(rail->rail_event_sender);
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

static void rail_process_event(rdpSvcPlugin* plugin, RDP_EVENT* event)
{
	RAIL_CLIENT_EVENT* rail_ui_event = NULL;
	railPlugin* rail = NULL;

	rail = (railPlugin*)plugin;
	rail_ui_event = (RAIL_CLIENT_EVENT*)event->user_data;

	freerdp_event_free(event);
}

DEFINE_SVC_PLUGIN(rail, "rail", 
	CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP |
	CHANNEL_OPTION_COMPRESS_RDP | CHANNEL_OPTION_SHOW_PROTOCOL)

