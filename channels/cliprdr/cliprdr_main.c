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
#include <freerdp/types.h>
#include <freerdp/constants.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/unicode.h>
#include <freerdp/utils/svc_plugin.h>
#include <freerdp/plugins/cliprdr.h>

#include "cliprdr_constants.h"
#include "cliprdr_main.h"
#include "cliprdr_format.h"

static const char* const CB_MSG_TYPE_STRINGS[] =
{
	"",
	"CB_MONITOR_READY",
	"CB_FORMAT_LIST",
	"CB_FORMAT_LIST_RESPONSE",
	"CB_FORMAT_DATA_REQUEST",
	"CB_FORMAT_DATA_RESPONSE",
	"CB_TEMP_DIRECTORY",
	"CB_CLIP_CAPS",
	"CB_FILECONTENTS_REQUEST",
	"CB_FILECONTENTS_RESPONSE",
	"CB_LOCK_CLIPDATA"
	"CB_UNLOCK_CLIPDATA"
};

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

	svc_plugin_send((rdpSvcPlugin*) cliprdr, s);
}

static void cliprdr_process_connect(rdpSvcPlugin* plugin)
{
	DEBUG_CLIPRDR("connecting");

	((cliprdrPlugin*) plugin)->uniconv = freerdp_uniconv_new();
}

void cliprdr_print_general_capability_flags(uint32 flags)
{
	printf("generalFlags (0x%08X) {\n", flags);

	if (flags & CB_USE_LONG_FORMAT_NAMES)
		printf("\tCB_USE_LONG_FORMAT_NAMES\n");
	if (flags & CB_STREAM_FILECLIP_ENABLED)
		printf("\tCB_STREAM_FILECLIP_ENABLED\n");
	if (flags & CB_FILECLIP_NO_FILE_PATHS)
		printf("\tCB_FILECLIP_NO_FILE_PATHS\n");
	if (flags & CB_CAN_LOCK_CLIPDATA)
		printf("\tCB_CAN_LOCK_CLIPDATA\n");

	printf("}\n");
}

static void cliprdr_process_general_capability(cliprdrPlugin* cliprdr, STREAM* s)
{
	uint32 version;
	uint32 generalFlags;

	stream_read_uint32(s, version); /* version (4 bytes) */
	stream_read_uint32(s, generalFlags); /* generalFlags (4 bytes) */

	DEBUG_CLIPRDR("Version: %d", version);

#ifdef WITH_DEBUG_CLIPRDR
	cliprdr_print_general_capability_flags(generalFlags);
#endif

	if (generalFlags & CB_USE_LONG_FORMAT_NAMES)
		cliprdr->use_long_format_names = true;

	if (generalFlags & CB_STREAM_FILECLIP_ENABLED)
		cliprdr->stream_fileclip_enabled = true;

	if (generalFlags & CB_FILECLIP_NO_FILE_PATHS)
		cliprdr->fileclip_no_file_paths = true;

	if (generalFlags & CB_CAN_LOCK_CLIPDATA)
		cliprdr->can_lock_clipdata = true;

	cliprdr->received_caps = true;
}

static void cliprdr_process_clip_caps(cliprdrPlugin* cliprdr, STREAM* s, uint16 length, uint16 flags)
{
	int i;
	uint16 lengthCapability;
	uint16 cCapabilitiesSets;
	uint16 capabilitySetType;

	stream_read_uint16(s, cCapabilitiesSets); /* cCapabilitiesSets (2 bytes) */
	stream_seek_uint16(s); /* pad1 (2 bytes) */

	DEBUG_CLIPRDR("cCapabilitiesSets %d", cCapabilitiesSets);

	for (i = 0; i < cCapabilitiesSets; i++)
	{
		stream_read_uint16(s, capabilitySetType); /* capabilitySetType (2 bytes) */
		stream_read_uint16(s, lengthCapability); /* lengthCapability (2 bytes) */

		switch (capabilitySetType)
		{
			case CB_CAPSTYPE_GENERAL:
				cliprdr_process_general_capability(cliprdr, s);
				break;

			default:
				DEBUG_WARN("unknown cliprdr capability set: %d", capabilitySetType);
				break;
		}
	}
}

static void cliprdr_send_clip_caps(cliprdrPlugin* cliprdr)
{
	STREAM* s;
	uint32 flags;

	s = cliprdr_packet_new(CB_CLIP_CAPS, 0, 4 + CB_CAPSTYPE_GENERAL_LEN);

	DEBUG_CLIPRDR("Sending Capabilities");

	flags = CB_USE_LONG_FORMAT_NAMES;

	stream_write_uint16(s, 1); /* cCapabilitiesSets */
	stream_write_uint16(s, 0); /* pad1 */
	stream_write_uint16(s, CB_CAPSTYPE_GENERAL); /* capabilitySetType */
	stream_write_uint16(s, CB_CAPSTYPE_GENERAL_LEN); /* lengthCapability */
	stream_write_uint32(s, CB_CAPS_VERSION_2); /* version */
	stream_write_uint32(s, flags); /* generalFlags */

	cliprdr_packet_send(cliprdr, s);
}

static void cliprdr_process_monitor_ready(cliprdrPlugin* cliprdr, STREAM* s, uint16 length, uint16 flags)
{
	RDP_EVENT* event;

	if (cliprdr->received_caps)
		cliprdr_send_clip_caps(cliprdr);

	event = freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR, RDP_EVENT_TYPE_CB_MONITOR_READY, NULL, NULL);
	svc_plugin_send_event((rdpSvcPlugin*) cliprdr, event);
}

static void cliprdr_process_receive(rdpSvcPlugin* plugin, STREAM* s)
{
	uint16 msgType;
	uint16 msgFlags;
	uint32 dataLen;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) plugin;

	stream_read_uint16(s, msgType);
	stream_read_uint16(s, msgFlags);
	stream_read_uint32(s, dataLen);

	DEBUG_CLIPRDR("msgType: %s (%d), msgFlags: %d dataLen: %d",
		CB_MSG_TYPE_STRINGS[msgType], msgType, msgFlags, dataLen);

	switch (msgType)
	{
		case CB_CLIP_CAPS:
			cliprdr_process_clip_caps(cliprdr, s, dataLen, msgFlags);
			break;

		case CB_MONITOR_READY:
			cliprdr_process_monitor_ready(cliprdr, s, dataLen, msgFlags);
			break;

		case CB_FORMAT_LIST:
			cliprdr_process_format_list(cliprdr, s, dataLen, msgFlags);
			break;

		case CB_FORMAT_LIST_RESPONSE:
			cliprdr_process_format_list_response(cliprdr, s, dataLen, msgFlags);
			break;

		case CB_FORMAT_DATA_REQUEST:
			cliprdr_process_format_data_request(cliprdr, s, dataLen, msgFlags);
			break;

		case CB_FORMAT_DATA_RESPONSE:
			cliprdr_process_format_data_response(cliprdr, s, dataLen, msgFlags);
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
			cliprdr_process_format_list_event((cliprdrPlugin*) plugin, (RDP_CB_FORMAT_LIST_EVENT*) event);
			break;

		case RDP_EVENT_TYPE_CB_DATA_REQUEST:
			cliprdr_process_format_data_request_event((cliprdrPlugin*) plugin, (RDP_CB_DATA_REQUEST_EVENT*) event);
			break;

		case RDP_EVENT_TYPE_CB_DATA_RESPONSE:
			cliprdr_process_format_data_response_event((cliprdrPlugin*) plugin, (RDP_CB_DATA_RESPONSE_EVENT*) event);
			break;

		default:
			DEBUG_WARN("unknown event type %d", event->event_type);
			break;
	}

	freerdp_event_free(event);
}

static void cliprdr_process_terminate(rdpSvcPlugin* plugin)
{
	cliprdrPlugin* cliprdr_plugin = (cliprdrPlugin*) plugin;

	if (cliprdr_plugin->uniconv != NULL)
		freerdp_uniconv_free(cliprdr_plugin->uniconv);

	xfree(plugin);
}

DEFINE_SVC_PLUGIN(cliprdr, "cliprdr",
	CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP |
	CHANNEL_OPTION_COMPRESS_RDP | CHANNEL_OPTION_SHOW_PROTOCOL)
