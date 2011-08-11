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

#include "rail_core.h"
#include "rail_main.h"

static void rail_plugin_process_connect(rdpSvcPlugin* plugin)
{
	railPlugin* rail_plugin = (railPlugin*)plugin;

	DEBUG_RAIL("rail_plugin_process_connect() called.");
	rail_core_on_channel_connected(rail_plugin->session);
}

static void rail_plugin_process_terminate(rdpSvcPlugin* plugin)
{
	DEBUG_RAIL("rail_plugin_process_terminate\n");
	xfree(plugin);
}

static void rail_plugin_send_vchannel_data(void* rail_plugin_object, void* data, size_t length)
{
	STREAM* s = NULL;
	railPlugin* plugin = (railPlugin*) rail_plugin_object;

	s = stream_new(length);
	stream_write(s, data, length);

	svc_plugin_send((rdpSvcPlugin*) plugin, s);
}

static void rail_plugin_process_received_vchannel_data(rdpSvcPlugin* plugin, STREAM* data_in)
{
	railPlugin* rail_plugin = (railPlugin*) plugin;

	DEBUG_RAIL("rail_plugin_process_receive: size=%d", stream_get_size(data_in));

	rail_vchannel_process_received_vchannel_data(rail_plugin->session, data_in);
	stream_free(data_in);
}

static void on_free_rail_vchannel_event(FRDP_EVENT* event)
{
	assert(event->event_type == FRDP_EVENT_TYPE_RAIL_VCHANNEL_2_UI);

	RAIL_VCHANNEL_EVENT* rail_event = (RAIL_VCHANNEL_EVENT*)event->user_data;

	if (rail_event->event_id == RAIL_VCHANNEL_EVENT_APP_RESPONSE_RECEIVED)
		xfree((void*)rail_event->param.app_response_info.application_id);

	if (rail_event->event_id == RAIL_VCHANNEL_EVENT_EXEC_RESULT_RETURNED)
		xfree((void*)rail_event->param.exec_result_info.exe_or_file);

	xfree(rail_event);
}

static void rail_plugin_send_vchannel_event(void* rail_plugin_object, RAIL_VCHANNEL_EVENT* event)
{
	railPlugin* plugin = (railPlugin*) rail_plugin_object;
	RAIL_VCHANNEL_EVENT* payload = NULL;
	FRDP_EVENT* out_event = NULL;

	payload = xnew(RAIL_VCHANNEL_EVENT);
	memset(payload, 0, sizeof(RAIL_VCHANNEL_EVENT));
	memcpy(payload, event, sizeof(RAIL_VCHANNEL_EVENT));

	out_event = freerdp_event_new(FRDP_EVENT_CLASS_RAIL, FRDP_EVENT_TYPE_RAIL_VCHANNEL_2_UI, on_free_rail_vchannel_event, payload);

	svc_plugin_send_event((rdpSvcPlugin*) plugin, out_event);
}

static void rail_plugin_process_event(rdpSvcPlugin* plugin, FRDP_EVENT* event)
{
	RAIL_UI_EVENT* rail_ui_event = NULL;
	railPlugin* rail_plugin = NULL;

	DEBUG_RAIL("rail_plugin_process_event: event_type=%d\n", event->event_type);

	rail_plugin = (railPlugin*)plugin;
	rail_ui_event = (RAIL_UI_EVENT*)event->user_data;

	if (event->event_type == FRDP_EVENT_TYPE_RAIL_UI_2_VCHANNEL)
		rail_core_handle_ui_event(rail_plugin->session, rail_ui_event);

	freerdp_event_free(event);
}

int VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	railPlugin* rail;

	DEBUG_RAIL("RAIL plugin VirtualChannelEntry started.");

	rail = (railPlugin*) xzalloc(sizeof(railPlugin));

	rail->plugin.channel_def.options = CHANNEL_OPTION_INITIALIZED |
		CHANNEL_OPTION_ENCRYPT_RDP | CHANNEL_OPTION_COMPRESS_RDP |
		CHANNEL_OPTION_SHOW_PROTOCOL;
	strcpy(rail->plugin.channel_def.name, "rail");

	rail->plugin.connect_callback = rail_plugin_process_connect;
	rail->plugin.terminate_callback = rail_plugin_process_terminate;

	rail->plugin.receive_callback = rail_plugin_process_received_vchannel_data;
	rail->plugin.event_callback = rail_plugin_process_event;

	rail->rail_event_sender.event_sender_object = rail;
	rail->rail_event_sender.send_rail_vchannel_event = rail_plugin_send_vchannel_event;

	rail->rail_data_sender.data_sender_object  = rail;
	rail->rail_data_sender.send_rail_vchannel_data =
			rail_plugin_send_vchannel_data;

	rail->session = rail_core_session_new(&rail->rail_data_sender, &rail->rail_event_sender);

	svc_plugin_init((rdpSvcPlugin*) rail, pEntryPoints);

	DEBUG_RAIL("RAIL plugin VirtualChannelEntry finished.");

	return 1;
}

