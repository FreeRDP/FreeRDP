/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Clipboard Virtual Channel
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
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

#include "cliprdr_constants.h"
#include "cliprdr_main.h"
#include "cliprdr_format.h"

STREAM* cliprdr_packet_new(uint16 msgType, uint16 msgFlags, uint32 dataLen)
{
	STREAM* data_out;

	data_out = stream_new(dataLen + 8);
	stream_write_uint16(data_out, msgType);
	stream_write_uint16(data_out, msgFlags);
	/* Write actual length after the entire packet has been constructed. */
	stream_seek(data_out, 4);

	return data_out;
}

void cliprdr_packet_send(cliprdrPlugin* cliprdr, STREAM* data_out)
{
	int pos;
	uint32 dataLen;

	pos = stream_get_pos(data_out);
	dataLen = pos - 8;
	stream_set_pos(data_out, 4);
	stream_write_uint32(data_out, dataLen);
	stream_set_pos(data_out, pos);

	svc_plugin_send((rdpSvcPlugin*)cliprdr, data_out);
}

static void cliprdr_process_connect(rdpSvcPlugin* plugin)
{
	DEBUG_SVC("connecting");
}

static void cliprdr_process_clip_caps(cliprdrPlugin* cliprdr, STREAM* data_in)
{
	uint16 cCapabilitiesSets;

	stream_read_uint16(data_in, cCapabilitiesSets);
	DEBUG_SVC("cCapabilitiesSets %d", cCapabilitiesSets);
}

static void cliprdr_send_clip_caps(cliprdrPlugin* cliprdr)
{
	STREAM* data_out;

	data_out = cliprdr_packet_new(CB_CLIP_CAPS, 0, 4 + CB_CAPSTYPE_GENERAL_LEN);

	stream_write_uint16(data_out, 1); /* cCapabilitiesSets */
	stream_write_uint16(data_out, 0); /* pad1 */
	stream_write_uint16(data_out, CB_CAPSTYPE_GENERAL); /* capabilitySetType */
	stream_write_uint16(data_out, CB_CAPSTYPE_GENERAL_LEN); /* lengthCapability */
	stream_write_uint32(data_out, CB_CAPS_VERSION_2); /* version */
	stream_write_uint32(data_out, 0); /* generalFlags */

	cliprdr_packet_send(cliprdr, data_out);
}

static void cliprdr_process_monitor_ready(cliprdrPlugin* cliprdr)
{
	FRDP_EVENT* event;

	cliprdr_send_clip_caps(cliprdr);

	event = freerdp_event_new(FRDP_EVENT_TYPE_CB_SYNC, NULL, NULL);
	svc_plugin_send_event((rdpSvcPlugin*)cliprdr, event);
}

static void cliprdr_process_receive(rdpSvcPlugin* plugin, STREAM* data_in)
{
	cliprdrPlugin* cliprdr = (cliprdrPlugin*)plugin;
	uint16 msgType;
	uint16 msgFlags;
	uint32 dataLen;

	stream_read_uint16(data_in, msgType);
	stream_read_uint16(data_in, msgFlags);
	stream_read_uint32(data_in, dataLen);

	DEBUG_SVC("msgType %d msgFlags %d dataLen %d", msgType, msgFlags, dataLen);

	switch (msgType)
	{
		case CB_CLIP_CAPS:
			cliprdr_process_clip_caps(cliprdr, data_in);
			break;

		case CB_MONITOR_READY:
			cliprdr_process_monitor_ready(cliprdr);
			break;

		case CB_FORMAT_LIST:
			cliprdr_process_format_list(cliprdr, data_in, dataLen);
			break;

		case CB_FORMAT_LIST_RESPONSE:
			break;

		default:
			DEBUG_WARN("unknown msgType %d", msgType);
			break;
	}

	stream_free(data_in);
}

static void cliprdr_process_event(rdpSvcPlugin* plugin, FRDP_EVENT* event)
{
	switch (event->event_type)
	{
		case FRDP_EVENT_TYPE_CB_FORMAT_LIST:
			cliprdr_process_format_list_event((cliprdrPlugin*)plugin, (FRDP_CB_FORMAT_LIST_EVENT*)event);
			break;

		default:
			DEBUG_WARN("unknown event type %d", event->event_type);
			break;
	}
	freerdp_event_free(event);
}

static void cliprdr_process_terminate(rdpSvcPlugin* plugin)
{
	xfree(plugin);
}

int VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	cliprdrPlugin* cliprdr;

	cliprdr = (cliprdrPlugin*)xmalloc(sizeof(cliprdrPlugin));
	memset(cliprdr, 0, sizeof(cliprdrPlugin));

	cliprdr->plugin.channel_def.options = CHANNEL_OPTION_INITIALIZED |
		CHANNEL_OPTION_ENCRYPT_RDP | CHANNEL_OPTION_COMPRESS_RDP |
		CHANNEL_OPTION_SHOW_PROTOCOL;
	strcpy(cliprdr->plugin.channel_def.name, "cliprdr");

	cliprdr->plugin.connect_callback = cliprdr_process_connect;
	cliprdr->plugin.receive_callback = cliprdr_process_receive;
	cliprdr->plugin.event_callback = cliprdr_process_event;
	cliprdr->plugin.terminate_callback = cliprdr_process_terminate;

	svc_plugin_init((rdpSvcPlugin*)cliprdr, pEntryPoints);

	return 1;
}
