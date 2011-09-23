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
#include <freerdp/plugins/cliprdr.h>

#include "cliprdr_constants.h"
#include "cliprdr_main.h"
#include "cliprdr_format.h"

STREAM* cliprdr_packet_new(uint16 msgType, uint16 msgFlags, uint32 dataLen)
{
	STREAM* s;

	s = stream_new(dataLen + 8);
	stream_write_uint16(s, msgType);
	stream_write_uint16(s, msgFlags);
	/* Write actual length after the entire packet has been constructed. */
	stream_seek(s, 4);

	return s;
}

void cliprdr_packet_send(cliprdrPlugin* cliprdr, STREAM* s)
{
	int pos;
	uint32 dataLen;

	pos = stream_get_pos(s);
	dataLen = pos - 8;
	stream_set_pos(s, 4);
	stream_write_uint32(s, dataLen);
	stream_set_pos(s, pos);

	svc_plugin_send((rdpSvcPlugin*)cliprdr, s);
}

static void cliprdr_process_connect(rdpSvcPlugin* plugin)
{
	DEBUG_SVC("connecting");
}

static void cliprdr_process_clip_caps(cliprdrPlugin* cliprdr, STREAM* s)
{
	uint16 cCapabilitiesSets;

	stream_read_uint16(s, cCapabilitiesSets);
	DEBUG_SVC("cCapabilitiesSets %d", cCapabilitiesSets);
}

static void cliprdr_send_clip_caps(cliprdrPlugin* cliprdr)
{
	STREAM* s;

	s = cliprdr_packet_new(CB_CLIP_CAPS, 0, 4 + CB_CAPSTYPE_GENERAL_LEN);

	stream_write_uint16(s, 1); /* cCapabilitiesSets */
	stream_write_uint16(s, 0); /* pad1 */
	stream_write_uint16(s, CB_CAPSTYPE_GENERAL); /* capabilitySetType */
	stream_write_uint16(s, CB_CAPSTYPE_GENERAL_LEN); /* lengthCapability */
	stream_write_uint32(s, CB_CAPS_VERSION_2); /* version */
	stream_write_uint32(s, 0); /* generalFlags */

	cliprdr_packet_send(cliprdr, s);
}

static void cliprdr_process_monitor_ready(cliprdrPlugin* cliprdr)
{
	RDP_EVENT* event;

	cliprdr_send_clip_caps(cliprdr);

	event = freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR, RDP_EVENT_TYPE_CB_SYNC, NULL, NULL);
	svc_plugin_send_event((rdpSvcPlugin*)cliprdr, event);
}

static void cliprdr_process_receive(rdpSvcPlugin* plugin, STREAM* s)
{
	cliprdrPlugin* cliprdr = (cliprdrPlugin*)plugin;
	uint16 msgType;
	uint16 msgFlags;
	uint32 dataLen;

	stream_read_uint16(s, msgType);
	stream_read_uint16(s, msgFlags);
	stream_read_uint32(s, dataLen);

	DEBUG_SVC("msgType %d msgFlags %d dataLen %d", msgType, msgFlags, dataLen);

	switch (msgType)
	{
		case CB_CLIP_CAPS:
			cliprdr_process_clip_caps(cliprdr, s);
			break;

		case CB_MONITOR_READY:
			cliprdr_process_monitor_ready(cliprdr);
			break;

		case CB_FORMAT_LIST:
			cliprdr_process_format_list(cliprdr, s, dataLen);
			break;

		case CB_FORMAT_LIST_RESPONSE:
			cliprdr_process_format_list_response(cliprdr, msgFlags);
			break;

		case CB_FORMAT_DATA_REQUEST:
			cliprdr_process_format_data_request(cliprdr, s);
			break;

		case CB_FORMAT_DATA_RESPONSE:
			cliprdr_process_format_data_response(cliprdr, s, dataLen);
			break;

		default:
			DEBUG_WARN("unknown msgType %d", msgType);
			break;
	}

	stream_free(s);
}

static void cliprdr_process_event(rdpSvcPlugin* plugin, RDP_EVENT* event)
{
	switch (event->event_type)
	{
		case RDP_EVENT_TYPE_CB_FORMAT_LIST:
			cliprdr_process_format_list_event((cliprdrPlugin*)plugin, (RDP_CB_FORMAT_LIST_EVENT*)event);
			break;

		case RDP_EVENT_TYPE_CB_DATA_REQUEST:
			cliprdr_process_format_data_request_event((cliprdrPlugin*)plugin, (RDP_CB_DATA_REQUEST_EVENT*)event);
			break;

		case RDP_EVENT_TYPE_CB_DATA_RESPONSE:
			cliprdr_process_format_data_response_event((cliprdrPlugin*)plugin, (RDP_CB_DATA_RESPONSE_EVENT*)event);
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

DEFINE_SVC_PLUGIN(cliprdr, "cliprdr",
	CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP |
	CHANNEL_OPTION_COMPRESS_RDP | CHANNEL_OPTION_SHOW_PROTOCOL)
