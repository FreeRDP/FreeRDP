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

CliprdrClientContext* cliprdr_get_client_interface(cliprdrPlugin* cliprdr)
{
	CliprdrClientContext* pInterface;
	rdpSvcPlugin* plugin = (rdpSvcPlugin*) cliprdr;
	pInterface = (CliprdrClientContext*) plugin->channel_entry_points.pInterface;
	return pInterface;
}

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
	CliprdrClientContext* context;

	context = cliprdr_get_client_interface(cliprdr);

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

	if (context->custom)
	{
		CLIPRDR_CAPABILITIES capabilities;
		CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet;

		capabilities.cCapabilitiesSets = 1;
		capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET*) &(generalCapabilitySet);

		generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
		generalCapabilitySet.capabilitySetLength = 12;

		generalCapabilitySet.version = version;
		generalCapabilitySet.generalFlags = generalFlags;

		if (context->ServerCapabilities)
			context->ServerCapabilities(context, &capabilities);
	}
	else
	{
		RDP_CB_CLIP_CAPS* caps_event;

		caps_event = (RDP_CB_CLIP_CAPS*) freerdp_event_new(CliprdrChannel_Class, CliprdrChannel_ClipCaps, NULL, NULL);
		caps_event->capabilities = generalFlags;

		svc_plugin_send_event((rdpSvcPlugin*) cliprdr, (wMessage*) caps_event);
	}
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

	flags = CB_USE_LONG_FORMAT_NAMES
#ifdef _WIN32
			| CB_STREAM_FILECLIP_ENABLED
			| CB_FILECLIP_NO_FILE_PATHS
#endif
		;

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
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);

	if (context->custom)
	{
		CLIPRDR_MONITOR_READY monitorReady;

		monitorReady.msgType = CB_MONITOR_READY;
		monitorReady.msgFlags = flags;
		monitorReady.dataLen = length;

		if (context->MonitorReady)
			context->MonitorReady(context, &monitorReady);
	}
	else
	{
		RDP_CB_MONITOR_READY_EVENT* event;

		if (cliprdr->received_caps)
			cliprdr_send_clip_caps(cliprdr);

		event = (RDP_CB_MONITOR_READY_EVENT*) freerdp_event_new(CliprdrChannel_Class, CliprdrChannel_MonitorReady, NULL, NULL);

		svc_plugin_send_event((rdpSvcPlugin*) cliprdr, (wMessage*) event);
	}
}

static void cliprdr_process_filecontents_request(cliprdrPlugin* cliprdr, wStream* s, UINT32 length, UINT16 flags)
{
	RDP_CB_FILECONTENTS_REQUEST_EVENT* cb_event;

	cb_event = (RDP_CB_FILECONTENTS_REQUEST_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_FilecontentsRequest, NULL, NULL);

	Stream_Read_UINT32(s, cb_event->streamId);
	Stream_Read_UINT32(s, cb_event->lindex);
	Stream_Read_UINT32(s, cb_event->dwFlags);
	Stream_Read_UINT32(s, cb_event->nPositionLow);
	Stream_Read_UINT32(s, cb_event->nPositionHigh);
	Stream_Read_UINT32(s, cb_event->cbRequested);
	//Stream_Read_UINT32(s, cb_event->clipDataId);

	svc_plugin_send_event((rdpSvcPlugin*) cliprdr, (wMessage*) cb_event);
}

static void cliprdr_process_filecontents_response(cliprdrPlugin* cliprdr, wStream* s, UINT32 length, UINT16 flags)
{
	RDP_CB_FILECONTENTS_RESPONSE_EVENT* cb_event;

	cb_event = (RDP_CB_FILECONTENTS_RESPONSE_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_FilecontentsResponse, NULL, NULL);

	Stream_Read_UINT32(s, cb_event->streamId);

	if (length > 0)
	{
		cb_event->size = length - 4;
		cb_event->data = (BYTE*) malloc(cb_event->size);
		CopyMemory(cb_event->data, Stream_Pointer(s), cb_event->size);
	}

	svc_plugin_send_event((rdpSvcPlugin*) cliprdr, (wMessage*) cb_event);
}

static void cliprdr_process_lock_clipdata(cliprdrPlugin* cliprdr, wStream* s, UINT32 length, UINT16 flags)
{
	RDP_CB_LOCK_CLIPDATA_EVENT* cb_event;

	cb_event = (RDP_CB_LOCK_CLIPDATA_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_LockClipdata, NULL, NULL);

	Stream_Read_UINT32(s, cb_event->clipDataId);

	svc_plugin_send_event((rdpSvcPlugin*) cliprdr, (wMessage*) cb_event);
}

static void cliprdr_process_unlock_clipdata(cliprdrPlugin* cliprdr, wStream* s, UINT32 length, UINT16 flags)
{
	RDP_CB_UNLOCK_CLIPDATA_EVENT* cb_event;

	cb_event = (RDP_CB_UNLOCK_CLIPDATA_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_UnLockClipdata, NULL, NULL);

	Stream_Read_UINT32(s, cb_event->clipDataId);

	svc_plugin_send_event((rdpSvcPlugin*) cliprdr, (wMessage*) cb_event);
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

		case CB_FILECONTENTS_REQUEST:
			cliprdr_process_filecontents_request(cliprdr, s, dataLen, msgFlags);
			break;

		case CB_FILECONTENTS_RESPONSE:
			cliprdr_process_filecontents_response(cliprdr, s, dataLen, msgFlags);
			break;

		case CB_LOCK_CLIPDATA:
			cliprdr_process_lock_clipdata(cliprdr, s, dataLen, msgFlags);
			break;

		case CB_UNLOCK_CLIPDATA:
			cliprdr_process_unlock_clipdata(cliprdr, s, dataLen, msgFlags);
			break;

		default:
			DEBUG_WARN("unknown msgType %d", msgType);
			break;
	}
}

static void cliprdr_process_filecontents_request_event(cliprdrPlugin* plugin, RDP_CB_FILECONTENTS_REQUEST_EVENT * event)
{
	wStream *s;
	DEBUG_CLIPRDR("Sending File Contents Request.");

	s = cliprdr_packet_new(CB_FILECONTENTS_REQUEST, 0, 24);

	Stream_Write_UINT32(s, event->streamId);
	Stream_Write_UINT32(s, event->lindex);
	Stream_Write_UINT32(s, event->dwFlags);
	Stream_Write_UINT32(s, event->nPositionLow);
	Stream_Write_UINT32(s, event->nPositionHigh);
	Stream_Write_UINT32(s, event->cbRequested);
	//Stream_Write_UINT32(s, event->clipDataId);

	cliprdr_packet_send(plugin, s);
}

static void cliprdr_process_filecontents_response_event(cliprdrPlugin* plugin, RDP_CB_FILECONTENTS_RESPONSE_EVENT * event)
{
	wStream* s;

	DEBUG_CLIPRDR("Sending file contents response with size = %d", event->size);

	if (event->size > 0)
	{
		s = cliprdr_packet_new(CB_FILECONTENTS_RESPONSE, CB_RESPONSE_OK, event->size + 4);
		Stream_Write_UINT32(s, event->streamId);
		Stream_Write(s, event->data, event->size);
	}
	else
	{
		s = cliprdr_packet_new(CB_FILECONTENTS_RESPONSE, CB_RESPONSE_FAIL, 0);
	}

	cliprdr_packet_send(plugin, s);
}

static void cliprdr_process_lock_clipdata_event(cliprdrPlugin* plugin, RDP_CB_LOCK_CLIPDATA_EVENT * event)
{
	wStream* s;

	DEBUG_CLIPRDR("Sending Lock Request");

	s = cliprdr_packet_new(CB_LOCK_CLIPDATA, 0, 4);
	Stream_Write_UINT32(s, event->clipDataId);

	cliprdr_packet_send(plugin, s);
}

static void cliprdr_process_unlock_clipdata_event(cliprdrPlugin* plugin, RDP_CB_UNLOCK_CLIPDATA_EVENT * event)
{
	wStream* s;

	DEBUG_CLIPRDR("Sending UnLock Request");

	s = cliprdr_packet_new(CB_UNLOCK_CLIPDATA, 0, 4);
	Stream_Write_UINT32(s, event->clipDataId);

	cliprdr_packet_send(plugin, s);
}

static void cliprdr_process_tempdir_event(cliprdrPlugin* plugin, RDP_CB_TEMPDIR_EVENT * event)
{
	wStream* s;

	DEBUG_CLIPRDR("Sending Temporary Directory.");
	s = cliprdr_packet_new(CB_TEMP_DIRECTORY, 0, 520);

	Stream_Write(s, event->dirname, 520);

	cliprdr_packet_send(plugin, s);
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

		case CliprdrChannel_FilecontentsRequest:
			cliprdr_process_filecontents_request_event((cliprdrPlugin*) plugin, (RDP_CB_FILECONTENTS_REQUEST_EVENT *) event);
			break;

		case CliprdrChannel_FilecontentsResponse:
			cliprdr_process_filecontents_response_event((cliprdrPlugin*) plugin, (RDP_CB_FILECONTENTS_RESPONSE_EVENT *) event);
			break;

		case CliprdrChannel_LockClipdata:
			cliprdr_process_lock_clipdata_event((cliprdrPlugin*) plugin, (RDP_CB_LOCK_CLIPDATA_EVENT *) event);
			break;

		case CliprdrChannel_UnLockClipdata:
			cliprdr_process_unlock_clipdata_event((cliprdrPlugin*) plugin, (RDP_CB_UNLOCK_CLIPDATA_EVENT *) event);
			break;

		case CliprdrChannel_TemporaryDirectory:
			cliprdr_process_tempdir_event((cliprdrPlugin*) plugin, (RDP_CB_TEMPDIR_EVENT *) event);
			break;

		default:
			DEBUG_WARN("unknown event type %d", GetMessageType(event->id));
			break;
	}

	freerdp_event_free(event);
}

static void cliprdr_process_terminate(rdpSvcPlugin* plugin)
{
	svc_plugin_terminate(plugin);
	free(plugin);
}

/**
 * Callback Interface
 */

int cliprdr_client_capabilities(CliprdrClientContext* context, CLIPRDR_CAPABILITIES* capabilities)
{
	wStream* s;
	CLIPRDR_GENERAL_CAPABILITY_SET* generalCapabilitySet;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;

	s = cliprdr_packet_new(CB_CLIP_CAPS, 0, 4 + CB_CAPSTYPE_GENERAL_LEN);

	Stream_Write_UINT16(s, 1); /* cCapabilitiesSets */
	Stream_Write_UINT16(s, 0); /* pad1 */

	generalCapabilitySet = (CLIPRDR_GENERAL_CAPABILITY_SET*) capabilities->capabilitySets;

	Stream_Write_UINT16(s, generalCapabilitySet->capabilitySetType); /* capabilitySetType */
	Stream_Write_UINT16(s, generalCapabilitySet->capabilitySetLength); /* lengthCapability */
	Stream_Write_UINT32(s, generalCapabilitySet->version); /* version */
	Stream_Write_UINT32(s, generalCapabilitySet->generalFlags); /* generalFlags */

	cliprdr_packet_send(cliprdr, s);

	return 0;
}

int cliprdr_client_format_list(CliprdrClientContext* context, CLIPRDR_FORMAT_LIST* formatList)
{
	wStream* s;
	UINT32 index;
	int length = 0;
	int formatNameSize;
	CLIPRDR_FORMAT* format;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;

	for (index = 0; index < formatList->cFormats; index++)
	{
		format = (CLIPRDR_FORMAT*) &(formatList->formats[index]);

		length += 4;

		formatNameSize = 2;

		if (format->formatName)
			formatNameSize = MultiByteToWideChar(CP_UTF8, 0, format->formatName, -1, NULL, 0) * 2;

		length += formatNameSize;
	}

	s = cliprdr_packet_new(CB_FORMAT_LIST, 0, length);

	for (index = 0; index < formatList->cFormats; index++)
	{
		format = (CLIPRDR_FORMAT*) &(formatList->formats[index]);

		Stream_Write_UINT32(s, format->formatId); /* formatId (4 bytes) */

		if (format->formatName)
		{
			int cchWideChar;
			LPWSTR lpWideCharStr;

			lpWideCharStr = (LPWSTR) Stream_Pointer(s);
			cchWideChar = (Stream_Capacity(s) - Stream_GetPosition(s)) / 2;

			formatNameSize = MultiByteToWideChar(CP_UTF8, 0,
					format->formatName, -1, lpWideCharStr, cchWideChar) * 2;

			Stream_Seek(s, formatNameSize);
		}
		else
		{
			Stream_Write_UINT16(s, 0);
		}
	}

	cliprdr_packet_send(cliprdr, s);

	return 0;
}

int cliprdr_client_format_list_response(CliprdrClientContext* context, CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
{
	wStream* s;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;

	formatListResponse->msgType = CB_FORMAT_LIST_RESPONSE;
	formatListResponse->dataLen = 0;

	s = cliprdr_packet_new(formatListResponse->msgType, formatListResponse->msgFlags, formatListResponse->dataLen);
	cliprdr_packet_send(cliprdr, s);

	return 0;
}

int cliprdr_client_format_data_request(CliprdrClientContext* context, CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
	wStream* s;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;

	formatDataRequest->msgType = CB_FORMAT_DATA_REQUEST;
	formatDataRequest->msgFlags = 0;
	formatDataRequest->dataLen = 4;

	s = cliprdr_packet_new(formatDataRequest->msgType, formatDataRequest->msgFlags, formatDataRequest->dataLen);
	Stream_Write_UINT32(s, formatDataRequest->requestedFormatId); /* requestedFormatId (4 bytes) */

	cliprdr_packet_send(cliprdr, s);

	return 0;
}

int cliprdr_client_format_data_response(CliprdrClientContext* context, CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	wStream* s;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;

	formatDataResponse->msgType = CB_FORMAT_DATA_RESPONSE;

	s = cliprdr_packet_new(formatDataResponse->msgType, formatDataResponse->msgFlags, formatDataResponse->dataLen);
	Stream_Write(s, formatDataResponse->requestedFormatData, formatDataResponse->dataLen);

	cliprdr_packet_send(cliprdr, s);

	return 0;
}

/* cliprdr is always built-in */
#define VirtualChannelEntry	cliprdr_VirtualChannelEntry

BOOL VCAPITYPE VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	cliprdrPlugin* cliprdr;
	CliprdrClientContext* context;
	CHANNEL_ENTRY_POINTS_FREERDP* pEntryPointsEx;

	cliprdr = (cliprdrPlugin*) calloc(1, sizeof(cliprdrPlugin));

	cliprdr->plugin.channel_def.options =
			CHANNEL_OPTION_INITIALIZED |
			CHANNEL_OPTION_ENCRYPT_RDP |
			CHANNEL_OPTION_COMPRESS_RDP |
			CHANNEL_OPTION_SHOW_PROTOCOL;

	strcpy(cliprdr->plugin.channel_def.name, "cliprdr");

	cliprdr->plugin.connect_callback = cliprdr_process_connect;
	cliprdr->plugin.receive_callback = cliprdr_process_receive;
	cliprdr->plugin.event_callback = cliprdr_process_event;
	cliprdr->plugin.terminate_callback = cliprdr_process_terminate;

	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP*) pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP)) &&
			(pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		context = (CliprdrClientContext*) calloc(1, sizeof(CliprdrClientContext));

		context->handle = (void*) cliprdr;

		context->ClientCapabilities = cliprdr_client_capabilities;
		context->ClientFormatList = cliprdr_client_format_list;
		context->ClientFormatListResponse = cliprdr_client_format_list_response;
		context->ClientFormatDataRequest = cliprdr_client_format_data_request;
		context->ClientFormatDataResponse = cliprdr_client_format_data_response;

		*(pEntryPointsEx->ppInterface) = (void*) context;
	}

	svc_plugin_init((rdpSvcPlugin*) cliprdr, pEntryPoints);

	return 1;
}
