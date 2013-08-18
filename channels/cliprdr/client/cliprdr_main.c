/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/print.h>

#include <freerdp/types.h>
#include <freerdp/constants.h>
#include <freerdp/utils/svc_plugin.h>
#include <freerdp/client/cliprdr.h>

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

wStream* cliprdr_packet_new(UINT16 msgType, UINT16 msgFlags, UINT32 dataLen)
{
	wStream* s;

	s = Stream_New(NULL, dataLen + 8);
	Stream_Write_UINT16(s, msgType);
	Stream_Write_UINT16(s, msgFlags);
	/* Write actual length after the entire packet has been constructed. */
	Stream_Seek(s, 4);

	return s;
}

void cliprdr_packet_send(cliprdrPlugin* cliprdr, wStream* s)
{
	int pos;
	UINT32 dataLen;

	pos = Stream_GetPosition(s);
	dataLen = pos - 8;
	Stream_SetPosition(s, 4);
	Stream_Write_UINT32(s, dataLen);
	Stream_SetPosition(s, pos);

#ifdef WITH_DEBUG_CLIPRDR
	printf("Cliprdr Sending (%d bytes)\n", dataLen + 8);
	winpr_HexDump(Stream_Buffer(s), dataLen + 8);
#endif

	svc_plugin_send((rdpSvcPlugin*) cliprdr, s);
}

static void cliprdr_process_connect(rdpSvcPlugin* plugin)
{
	DEBUG_CLIPRDR("connecting");
}

void cliprdr_print_general_capability_flags(UINT32 flags)
{
	fprintf(stderr, "generalFlags (0x%08X) {\n", flags);

	if (flags & CB_USE_LONG_FORMAT_NAMES)
		fprintf(stderr, "\tCB_USE_LONG_FORMAT_NAMES\n");
	if (flags & CB_STREAM_FILECLIP_ENABLED)
		fprintf(stderr, "\tCB_STREAM_FILECLIP_ENABLED\n");
	if (flags & CB_FILECLIP_NO_FILE_PATHS)
		fprintf(stderr, "\tCB_FILECLIP_NO_FILE_PATHS\n");
	if (flags & CB_CAN_LOCK_CLIPDATA)
		fprintf(stderr, "\tCB_CAN_LOCK_CLIPDATA\n");

	fprintf(stderr, "}\n");
}

static void cliprdr_process_general_capability(cliprdrPlugin* cliprdr, wStream* s)
{
	UINT32 version;
	UINT32 generalFlags;

	Stream_Read_UINT32(s, version); /* version (4 bytes) */
	Stream_Read_UINT32(s, generalFlags); /* generalFlags (4 bytes) */

	DEBUG_CLIPRDR("Version: %d", version);

#ifdef WITH_DEBUG_CLIPRDR
	cliprdr_print_general_capability_flags(generalFlags);
#endif

	if (generalFlags & CB_USE_LONG_FORMAT_NAMES)
		cliprdr->use_long_format_names = TRUE;

	if (generalFlags & CB_STREAM_FILECLIP_ENABLED)
		cliprdr->stream_fileclip_enabled = TRUE;

	if (generalFlags & CB_FILECLIP_NO_FILE_PATHS)
		cliprdr->fileclip_no_file_paths = TRUE;

	if (generalFlags & CB_CAN_LOCK_CLIPDATA)
		cliprdr->can_lock_clipdata = TRUE;

	cliprdr->received_caps = TRUE;
}

static void cliprdr_process_clip_caps(cliprdrPlugin* cliprdr, wStream* s, UINT16 length, UINT16 flags)
{
	int i;
	UINT16 lengthCapability;
	UINT16 cCapabilitiesSets;
	UINT16 capabilitySetType;

	Stream_Read_UINT16(s, cCapabilitiesSets); /* cCapabilitiesSets (2 bytes) */
	Stream_Seek_UINT16(s); /* pad1 (2 bytes) */

	DEBUG_CLIPRDR("cCapabilitiesSets %d", cCapabilitiesSets);

	for (i = 0; i < cCapabilitiesSets; i++)
	{
		Stream_Read_UINT16(s, capabilitySetType); /* capabilitySetType (2 bytes) */
		Stream_Read_UINT16(s, lengthCapability); /* lengthCapability (2 bytes) */

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
	wStream* s;
	UINT32 flags;

	s = cliprdr_packet_new(CB_CLIP_CAPS, 0, 4 + CB_CAPSTYPE_GENERAL_LEN);

	DEBUG_CLIPRDR("Sending Capabilities");

	flags = CB_USE_LONG_FORMAT_NAMES;

	Stream_Write_UINT16(s, 1); /* cCapabilitiesSets */
	Stream_Write_UINT16(s, 0); /* pad1 */
	Stream_Write_UINT16(s, CB_CAPSTYPE_GENERAL); /* capabilitySetType */
	Stream_Write_UINT16(s, CB_CAPSTYPE_GENERAL_LEN); /* lengthCapability */
	Stream_Write_UINT32(s, CB_CAPS_VERSION_2); /* version */
	Stream_Write_UINT32(s, flags); /* generalFlags */

	cliprdr_packet_send(cliprdr, s);
}

static void cliprdr_process_monitor_ready(cliprdrPlugin* cliprdr, wStream* s, UINT16 length, UINT16 flags)
{
	wMessage* event;

	if (cliprdr->received_caps)
		cliprdr_send_clip_caps(cliprdr);

	event = freerdp_event_new(CliprdrChannel_Class, CliprdrChannel_MonitorReady, NULL, NULL);

	svc_plugin_send_event((rdpSvcPlugin*) cliprdr, event);
}

static void cliprdr_process_receive(rdpSvcPlugin* plugin, wStream* s)
{
	UINT16 msgType;
	UINT16 msgFlags;
	UINT32 dataLen;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) plugin;

	Stream_Read_UINT16(s, msgType);
	Stream_Read_UINT16(s, msgFlags);
	Stream_Read_UINT32(s, dataLen);

	DEBUG_CLIPRDR("msgType: %s (%d), msgFlags: %d dataLen: %d",
		CB_MSG_TYPE_STRINGS[msgType], msgType, msgFlags, dataLen);

#ifdef WITH_DEBUG_CLIPRDR
	winpr_HexDump(Stream_Buffer(s), dataLen + 8);
#endif

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

	Stream_Free(s, TRUE);
}

static void cliprdr_process_event(rdpSvcPlugin* plugin, wMessage* event)
{
	switch (GetMessageType(event->id))
	{
		case CliprdrChannel_FormatList:
			cliprdr_process_format_list_event((cliprdrPlugin*) plugin, (RDP_CB_FORMAT_LIST_EVENT*) event);
			break;

		case CliprdrChannel_DataRequest:
			cliprdr_process_format_data_request_event((cliprdrPlugin*) plugin, (RDP_CB_DATA_REQUEST_EVENT*) event);
			break;

		case CliprdrChannel_DataResponse:
			cliprdr_process_format_data_response_event((cliprdrPlugin*) plugin, (RDP_CB_DATA_RESPONSE_EVENT*) event);
			break;

		default:
			DEBUG_WARN("unknown event type %d", GetMessageType(event->id));
			break;
	}

	freerdp_event_free(event);
}

static void cliprdr_process_terminate(rdpSvcPlugin* plugin)
{
	free(plugin);
}

/**
 * Callback Interface
 */

int cliprdr_monitor_ready(CliprdrClientContext* context)
{
	return 0;
}

int cliprdr_format_list(CliprdrClientContext* context)
{
	return 0;
}

int cliprdr_data_request(CliprdrClientContext* context)
{
	return 0;
}

int cliprdr_data_response(CliprdrClientContext* context)
{
	return 0;
}

/* cliprdr is always built-in */
#define VirtualChannelEntry	cliprdr_VirtualChannelEntry

int VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	cliprdrPlugin* _p;
	CliprdrClientContext* context;
	CHANNEL_ENTRY_POINTS_EX* pEntryPointsEx;

	_p = (cliprdrPlugin*) malloc(sizeof(cliprdrPlugin));
	ZeroMemory(_p, sizeof(cliprdrPlugin));

	_p->plugin.channel_def.options =
			CHANNEL_OPTION_INITIALIZED |
			CHANNEL_OPTION_ENCRYPT_RDP |
			CHANNEL_OPTION_COMPRESS_RDP |
			CHANNEL_OPTION_SHOW_PROTOCOL;

	strcpy(_p->plugin.channel_def.name, "cliprdr");

	_p->plugin.connect_callback = cliprdr_process_connect;
	_p->plugin.receive_callback = cliprdr_process_receive;
	_p->plugin.event_callback = cliprdr_process_event;
	_p->plugin.terminate_callback = cliprdr_process_terminate;

	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_EX*) pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_EX)) &&
			(pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		context = (CliprdrClientContext*) malloc(sizeof(CliprdrClientContext));

		context->MonitorReady = cliprdr_monitor_ready;
		context->FormatList = cliprdr_format_list;
		context->DataRequest = cliprdr_data_request;
		context->DataResponse = cliprdr_data_response;

		*(pEntryPointsEx->ppInterface) = (void*) context;
	}

	svc_plugin_init((rdpSvcPlugin*) _p, pEntryPoints);

	return 1;
}
