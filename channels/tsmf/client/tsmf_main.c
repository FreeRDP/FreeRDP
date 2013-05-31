/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Redirection Virtual Channel
 *
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
#include <winpr/cmdline.h>

#include <winpr/stream.h>

#include "tsmf_types.h"
#include "tsmf_constants.h"
#include "tsmf_ifman.h"
#include "tsmf_media.h"

#include "tsmf_main.h"

typedef struct _TSMF_LISTENER_CALLBACK TSMF_LISTENER_CALLBACK;

typedef struct _TSMF_CHANNEL_CALLBACK TSMF_CHANNEL_CALLBACK;

typedef struct _TSMF_PLUGIN TSMF_PLUGIN;

struct _TSMF_LISTENER_CALLBACK
{
	IWTSListenerCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
};

struct _TSMF_CHANNEL_CALLBACK
{
	IWTSVirtualChannelCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	IWTSVirtualChannel* channel;

	BYTE presentation_id[16];
	UINT32 stream_id;
};

struct _TSMF_PLUGIN
{
	IWTSPlugin iface;

	TSMF_LISTENER_CALLBACK* listener_callback;

	const char* decoder_name;
	const char* audio_name;
	const char* audio_device;
};

void tsmf_playback_ack(IWTSVirtualChannelCallback* pChannelCallback,
	UINT32 message_id, UINT64 duration, UINT32 data_size)
{
	wStream* s;
	int status;
	TSMF_CHANNEL_CALLBACK* callback = (TSMF_CHANNEL_CALLBACK*) pChannelCallback;

	s = Stream_New(NULL, 32);
	Stream_Write_UINT32(s, TSMF_INTERFACE_CLIENT_NOTIFICATIONS | STREAM_ID_PROXY);
	Stream_Write_UINT32(s, message_id);
	Stream_Write_UINT32(s, PLAYBACK_ACK); /* FunctionId */
	Stream_Write_UINT32(s, callback->stream_id); /* StreamId */
	Stream_Write_UINT64(s, duration); /* DataDuration */
	Stream_Write_UINT64(s, data_size); /* cbData */
	
	DEBUG_DVC("response size %d", (int) Stream_GetPosition(s));
	status = callback->channel->Write(callback->channel, Stream_GetPosition(s), Stream_Buffer(s), NULL);

	if (status)
	{
		DEBUG_WARN("response error %d", status);
	}

	Stream_Free(s, TRUE);
}

BOOL tsmf_push_event(IWTSVirtualChannelCallback* pChannelCallback, wMessage* event)
{
	int status;
	TSMF_CHANNEL_CALLBACK* callback = (TSMF_CHANNEL_CALLBACK*) pChannelCallback;

	status = callback->channel_mgr->PushEvent(callback->channel_mgr, event);

	if (status)
	{
		DEBUG_WARN("response error %d", status);
		return FALSE;
	}

	return TRUE;
}

static int tsmf_on_data_received(IWTSVirtualChannelCallback* pChannelCallback,
	UINT32 cbSize,
	BYTE* pBuffer)
{
	int length;
	wStream* input;
	wStream* output;
	int status = -1;
	TSMF_IFMAN ifman;
	UINT32 MessageId;
	UINT32 FunctionId;
	UINT32 InterfaceId;
	TSMF_CHANNEL_CALLBACK* callback = (TSMF_CHANNEL_CALLBACK*) pChannelCallback;

	/* 2.2.1 Shared Message Header (SHARED_MSG_HEADER) */
	if (cbSize < 12)
	{
		DEBUG_WARN("invalid size. cbSize=%d", cbSize);
		return 1;
	}

	input = Stream_New((BYTE*) pBuffer, cbSize);
	output = Stream_New(NULL, 256);
	Stream_Seek(output, 8);

	Stream_Read_UINT32(input, InterfaceId);
	Stream_Read_UINT32(input, MessageId);
	Stream_Read_UINT32(input, FunctionId);
	DEBUG_DVC("cbSize=%d InterfaceId=0x%X MessageId=0x%X FunctionId=0x%X",
		cbSize, InterfaceId, MessageId, FunctionId);

	memset(&ifman, 0, sizeof(TSMF_IFMAN));
	ifman.channel_callback = pChannelCallback;
	ifman.decoder_name = ((TSMF_PLUGIN*) callback->plugin)->decoder_name;
	ifman.audio_name = ((TSMF_PLUGIN*) callback->plugin)->audio_name;
	ifman.audio_device = ((TSMF_PLUGIN*) callback->plugin)->audio_device;
	memcpy(ifman.presentation_id, callback->presentation_id, 16);
	ifman.stream_id = callback->stream_id;
	ifman.message_id = MessageId;
	ifman.input = input;
	ifman.input_size = cbSize - 12;
	ifman.output = output;
	ifman.output_pending = FALSE;
	ifman.output_interface_id = InterfaceId;

	switch (InterfaceId)
	{
		case TSMF_INTERFACE_CAPABILITIES | STREAM_ID_NONE:

			switch (FunctionId)
			{
				case RIM_EXCHANGE_CAPABILITY_REQUEST:
					status = tsmf_ifman_rim_exchange_capability_request(&ifman);
					break;

				default:
					break;
			}
			break;

		case TSMF_INTERFACE_DEFAULT | STREAM_ID_PROXY:

			switch (FunctionId)
			{
				case SET_CHANNEL_PARAMS:
					memcpy(callback->presentation_id, Stream_Pointer(input), 16);
					Stream_Seek(input, 16);
					Stream_Read_UINT32(input, callback->stream_id);
					DEBUG_DVC("SET_CHANNEL_PARAMS StreamId=%d", callback->stream_id);
					ifman.output_pending = TRUE;
					status = 0;
					break;

				case EXCHANGE_CAPABILITIES_REQ:
					status = tsmf_ifman_exchange_capability_request(&ifman);
					break;

				case CHECK_FORMAT_SUPPORT_REQ:
					status = tsmf_ifman_check_format_support_request(&ifman);
					break;

				case ON_NEW_PRESENTATION:
					status = tsmf_ifman_on_new_presentation(&ifman);
					break;

				case ADD_STREAM:
					status = tsmf_ifman_add_stream(&ifman);
					break;

				case SET_TOPOLOGY_REQ:
					status = tsmf_ifman_set_topology_request(&ifman);
					break;

				case REMOVE_STREAM:
					status = tsmf_ifman_remove_stream(&ifman);
					break;

				case SET_SOURCE_VIDEO_RECT:
					status = tsmf_ifman_set_source_video_rect(&ifman);
					break;

				case SHUTDOWN_PRESENTATION_REQ:
					status = tsmf_ifman_shutdown_presentation(&ifman);
					break;

				case ON_STREAM_VOLUME:
					status = tsmf_ifman_on_stream_volume(&ifman);
					break;

				case ON_CHANNEL_VOLUME:
					status = tsmf_ifman_on_channel_volume(&ifman);
					break;

				case SET_VIDEO_WINDOW:
					status = tsmf_ifman_set_video_window(&ifman);
					break;

				case UPDATE_GEOMETRY_INFO:
					status = tsmf_ifman_update_geometry_info(&ifman);
					break;

				case SET_ALLOCATOR:
					status = tsmf_ifman_set_allocator(&ifman);
					break;

				case NOTIFY_PREROLL:
					status = tsmf_ifman_notify_preroll(&ifman);
					break;

				case ON_SAMPLE:
					status = tsmf_ifman_on_sample(&ifman);
					break;

				case ON_FLUSH:
					status = tsmf_ifman_on_flush(&ifman);
					break;

				case ON_END_OF_STREAM:
					status = tsmf_ifman_on_end_of_stream(&ifman);
					break;

				case ON_PLAYBACK_STARTED:
					status = tsmf_ifman_on_playback_started(&ifman);
					break;

				case ON_PLAYBACK_PAUSED:
					status = tsmf_ifman_on_playback_paused(&ifman);
					break;

				case ON_PLAYBACK_RESTARTED:
					status = tsmf_ifman_on_playback_restarted(&ifman);
					break;

				case ON_PLAYBACK_STOPPED:
					status = tsmf_ifman_on_playback_stopped(&ifman);
					break;

				case ON_PLAYBACK_RATE_CHANGED:
					status = tsmf_ifman_on_playback_rate_changed(&ifman);
					break;

				default:
					break;
			}
			break;

		default:
			break;
	}

	Stream_Free(input, FALSE);
	input = NULL;
	ifman.input = NULL;

	if (status == -1)
	{
		switch (FunctionId)
		{
			case RIMCALL_RELEASE:
				/* [MS-RDPEXPS] 2.2.2.2 Interface Release (IFACE_RELEASE)
				   This message does not require a reply. */
				status = 0;
				ifman.output_pending = 1;
				break;

			case RIMCALL_QUERYINTERFACE:
				/* [MS-RDPEXPS] 2.2.2.1.2 Query Interface Response (QI_RSP)
				   This message is not supported in this channel. */
				status = 0;
				break;
		}

		if (status == -1)
		{
			DEBUG_WARN("InterfaceId 0x%X FunctionId 0x%X not processed.",
				InterfaceId, FunctionId);
			/* When a request is not implemented we return empty response indicating error */
		}
		status = 0;
	}

	if (status == 0 && !ifman.output_pending)
	{
		/* Response packet does not have FunctionId */
		length = Stream_GetPosition(output);
		Stream_SetPosition(output, 0);
		Stream_Write_UINT32(output, ifman.output_interface_id);
		Stream_Write_UINT32(output, MessageId);

		DEBUG_DVC("response size %d", length);
		status = callback->channel->Write(callback->channel, length, Stream_Buffer(output), NULL);
		if (status)
		{
			DEBUG_WARN("response error %d", status);
		}
	}

	Stream_Free(output, TRUE);

	return status;
}

static int tsmf_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	TSMF_STREAM* stream;
	TSMF_PRESENTATION* presentation;
	TSMF_CHANNEL_CALLBACK* callback = (TSMF_CHANNEL_CALLBACK*) pChannelCallback;

	DEBUG_DVC("");

	if (callback->stream_id)
	{
		presentation = tsmf_presentation_find_by_id(callback->presentation_id);

		if (presentation)
		{
			stream = tsmf_stream_find_by_id(presentation, callback->stream_id);

			if (stream)
				tsmf_stream_free(stream);
		}
	}

	free(pChannelCallback);

	return 0;
}

static int tsmf_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
	IWTSVirtualChannel* pChannel,
	BYTE* Data,
	int* pbAccept,
	IWTSVirtualChannelCallback** ppCallback)
{
	TSMF_CHANNEL_CALLBACK* callback;
	TSMF_LISTENER_CALLBACK* listener_callback = (TSMF_LISTENER_CALLBACK*) pListenerCallback;

	DEBUG_DVC("");

	callback = (TSMF_CHANNEL_CALLBACK*) malloc(sizeof(TSMF_CHANNEL_CALLBACK));
	ZeroMemory(callback, sizeof(TSMF_CHANNEL_CALLBACK));

	callback->iface.OnDataReceived = tsmf_on_data_received;
	callback->iface.OnClose = tsmf_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;
	*ppCallback = (IWTSVirtualChannelCallback*) callback;

	return 0;
}

static int tsmf_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	TSMF_PLUGIN* tsmf = (TSMF_PLUGIN*) pPlugin;

	DEBUG_DVC("");

	tsmf->listener_callback = (TSMF_LISTENER_CALLBACK*) malloc(sizeof(TSMF_LISTENER_CALLBACK));
	ZeroMemory(tsmf->listener_callback, sizeof(TSMF_LISTENER_CALLBACK));

	tsmf->listener_callback->iface.OnNewChannelConnection = tsmf_on_new_channel_connection;
	tsmf->listener_callback->plugin = pPlugin;
	tsmf->listener_callback->channel_mgr = pChannelMgr;

	return pChannelMgr->CreateListener(pChannelMgr, "TSMF", 0,
		(IWTSListenerCallback*) tsmf->listener_callback, NULL);
}

static int tsmf_plugin_terminated(IWTSPlugin* pPlugin)
{
	TSMF_PLUGIN* tsmf = (TSMF_PLUGIN*) pPlugin;

	DEBUG_DVC("");

	if (tsmf->listener_callback)
		free(tsmf->listener_callback);
	free(tsmf);

	return 0;
}

COMMAND_LINE_ARGUMENT_A tsmf_args[] =
{
	{ "audio", COMMAND_LINE_VALUE_REQUIRED, "<subsystem>", NULL, NULL, -1, NULL, "audio subsystem" },
	{ "audio-dev", COMMAND_LINE_VALUE_REQUIRED, "<device>", NULL, NULL, -1, NULL, "audio device name" },
	{ "decoder", COMMAND_LINE_VALUE_REQUIRED, "<subsystem>", NULL, NULL, -1, NULL, "decoder subsystem" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

static void tsmf_process_addin_args(IWTSPlugin* pPlugin, ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	TSMF_PLUGIN* tsmf = (TSMF_PLUGIN*) pPlugin;

	flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON;

	status = CommandLineParseArgumentsA(args->argc, (const char**) args->argv,
			tsmf_args, flags, tsmf, NULL, NULL);

	arg = tsmf_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg)

		CommandLineSwitchCase(arg, "audio")
		{
			tsmf->audio_name = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "audio-dev")
		{
			tsmf->audio_device = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "decoder")
		{
			tsmf->decoder_name = _strdup(arg->Value);
		}
		CommandLineSwitchDefault(arg)
		{

		}

		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);
}

#ifdef STATIC_CHANNELS
#define DVCPluginEntry	tsmf_DVCPluginEntry
#endif

int DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	int status = 0;
	TSMF_PLUGIN* tsmf;

	tsmf = (TSMF_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "tsmf");

	if (tsmf == NULL)
	{
		tsmf = (TSMF_PLUGIN*) malloc(sizeof(TSMF_PLUGIN));
		ZeroMemory(tsmf, sizeof(TSMF_PLUGIN));

		tsmf->iface.Initialize = tsmf_plugin_initialize;
		tsmf->iface.Connected = NULL;
		tsmf->iface.Disconnected = NULL;
		tsmf->iface.Terminated = tsmf_plugin_terminated;
		status = pEntryPoints->RegisterPlugin(pEntryPoints, "tsmf", (IWTSPlugin*) tsmf);

		tsmf_media_init();
	}

	if (status == 0)
	{
		tsmf_process_addin_args((IWTSPlugin*) tsmf, pEntryPoints->GetPluginData(pEntryPoints));
	}

	return status;
}
