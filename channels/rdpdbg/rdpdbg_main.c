/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Debugging Virtual Channel
 *
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

typedef struct rdpdbg_plugin rdpdbgPlugin;
struct rdpdbg_plugin
{
	rdpSvcPlugin plugin;
};

static void rdpdbg_process_connect(rdpSvcPlugin* plugin)
{
	DEBUG_WARN("connecting");
}

static void rdpdbg_process_receive(rdpSvcPlugin* plugin, STREAM* data_in)
{
	STREAM* data_out;

	DEBUG_WARN("size %d", stream_get_size(data_in));
	stream_free(data_in);

	data_out = stream_new(8);
	stream_write(data_out, "senddata", 8);
	svc_plugin_send(plugin, data_out);
}

static void rdpdbg_process_event(rdpSvcPlugin* plugin, FRDP_EVENT* event)
{
	DEBUG_WARN("event_type %d", event->event_type);
	freerdp_event_free(event);

	event = freerdp_event_new(FRDP_EVENT_TYPE_DEBUG, NULL, NULL);
	svc_plugin_send_event(plugin, event);
}

static void rdpdbg_process_terminate(rdpSvcPlugin* plugin)
{
	DEBUG_WARN("terminating");
	xfree(plugin);
}

int VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	rdpdbgPlugin* rdpdbg;

	rdpdbg = (rdpdbgPlugin*)xmalloc(sizeof(rdpdbgPlugin));
	memset(rdpdbg, 0, sizeof(rdpdbgPlugin));

	rdpdbg->plugin.channel_def.options = CHANNEL_OPTION_INITIALIZED |
		CHANNEL_OPTION_ENCRYPT_RDP | CHANNEL_OPTION_COMPRESS_RDP |
		CHANNEL_OPTION_SHOW_PROTOCOL;
	strcpy(rdpdbg->plugin.channel_def.name, "rdpdbg");

	rdpdbg->plugin.connect_callback = rdpdbg_process_connect;
	rdpdbg->plugin.receive_callback = rdpdbg_process_receive;
	rdpdbg->plugin.event_callback = rdpdbg_process_event;
	rdpdbg->plugin.terminate_callback = rdpdbg_process_terminate;

	svc_plugin_init((rdpSvcPlugin*)rdpdbg, pEntryPoints);

	return 1;
}
