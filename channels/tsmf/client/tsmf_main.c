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

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/cmdline.h>

#include <freerdp/client/tsmf.h>

#include "tsmf_types.h"
#include "tsmf_constants.h"
#include "tsmf_ifman.h"
#include "tsmf_media.h"

#include "tsmf_main.h"

void tsmf_playback_ack(IWTSVirtualChannelCallback *pChannelCallback,
			UINT32 message_id, UINT64 duration, UINT32 data_size)
{
	wStream *s;
	int status = -1;
	TSMF_CHANNEL_CALLBACK *callback = (TSMF_CHANNEL_CALLBACK *) pChannelCallback;

	s = Stream_New(NULL, 32);

	Stream_Write_UINT32(s, TSMF_INTERFACE_CLIENT_NOTIFICATIONS | STREAM_ID_PROXY);
	Stream_Write_UINT32(s, message_id);
	Stream_Write_UINT32(s, PLAYBACK_ACK); /* FunctionId */
	Stream_Write_UINT32(s, callback->stream_id); /* StreamId */
	Stream_Write_UINT64(s, duration); /* DataDuration */
	Stream_Write_UINT64(s, data_size); /* cbData */

	DEBUG_TSMF("response size %d", (int) Stream_GetPosition(s));

	if (!callback || !callback->channel || !callback->channel->Write)
	{
		WLog_ERR(TAG, "callback=%p, channel=%p, write=%p", callback,
				   callback ? callback->channel : NULL,
				   (callback && callback->channel) ? callback->channel->Write : NULL);
	}
	else
	{
		status = callback->channel->Write(callback->channel,
				Stream_GetPosition(s), Stream_Buffer(s), NULL);
	}

	if (status)
	{
		WLog_ERR(TAG, "response error %d", status);
	}

	Stream_Free(s, TRUE);
}

static int tsmf_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream *data)
{
	int length;
	wStream *input;
	wStream *output;
	int status = -1;
	TSMF_IFMAN ifman;
	UINT32 MessageId;
	UINT32 FunctionId;
	UINT32 InterfaceId;
	TSMF_CHANNEL_CALLBACK* callback = (TSMF_CHANNEL_CALLBACK*) pChannelCallback;
	UINT32 cbSize = Stream_GetRemainingLength(data);

	/* 2.2.1 Shared Message Header (SHARED_MSG_HEADER) */
	if (cbSize < 12)
	{
		WLog_ERR(TAG, "invalid size. cbSize=%d", cbSize);
		return 1;
	}

	input = data;
	output = Stream_New(NULL, 256);
	Stream_Seek(output, 8);
	Stream_Read_UINT32(input, InterfaceId); /* InterfaceId (4 bytes) */
	Stream_Read_UINT32(input, MessageId); /* MessageId (4 bytes) */
	Stream_Read_UINT32(input, FunctionId); /* FunctionId (4 bytes) */

	DEBUG_TSMF("cbSize=%d InterfaceId=0x%X MessageId=0x%X FunctionId=0x%X",
			   cbSize, InterfaceId, MessageId, FunctionId);

	ZeroMemory(&ifman, sizeof(TSMF_IFMAN));
	ifman.channel_callback = pChannelCallback;
	ifman.decoder_name = ((TSMF_PLUGIN*) callback->plugin)->decoder_name;
	ifman.audio_name = ((TSMF_PLUGIN*) callback->plugin)->audio_name;
	ifman.audio_device = ((TSMF_PLUGIN*) callback->plugin)->audio_device;
	CopyMemory(ifman.presentation_id, callback->presentation_id, GUID_SIZE);
	ifman.stream_id = callback->stream_id;
	ifman.message_id = MessageId;
	ifman.input = input;
	ifman.input_size = cbSize - 12;
	ifman.output = output;
	ifman.output_pending = FALSE;
	ifman.output_interface_id = InterfaceId;

	//fprintf(stderr, "InterfaceId: 0x%04X MessageId: 0x%04X FunctionId: 0x%04X\n", InterfaceId, MessageId, FunctionId);

	switch (InterfaceId)
	{
		case TSMF_INTERFACE_CAPABILITIES | STREAM_ID_NONE:
			switch (FunctionId)
			{
				case RIM_EXCHANGE_CAPABILITY_REQUEST:
					status = tsmf_ifman_rim_exchange_capability_request(&ifman);
					break;
				case RIMCALL_RELEASE:
				case RIMCALL_QUERYINTERFACE:
					break;
				default:
					break;
			}
			break;

		case TSMF_INTERFACE_DEFAULT | STREAM_ID_PROXY:
			switch (FunctionId)
			{
				case SET_CHANNEL_PARAMS:
					CopyMemory(callback->presentation_id, Stream_Pointer(input), GUID_SIZE);
					Stream_Seek(input, GUID_SIZE);
					Stream_Read_UINT32(input, callback->stream_id);
					DEBUG_TSMF("SET_CHANNEL_PARAMS StreamId=%d", callback->stream_id);
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
				case RIMCALL_RELEASE:
				case RIMCALL_QUERYINTERFACE:
					break;
				default:
					break;
			}
			break;

		default:
			break;
	}

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
			WLog_ERR(TAG, "Unknown InterfaceId: 0x%04X MessageId: 0x%04X FunctionId: 0x%04X\n", InterfaceId, MessageId, FunctionId);
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
		DEBUG_TSMF("response size %d", length);
		status = callback->channel->Write(callback->channel, length, Stream_Buffer(output), NULL);

		if (status)
		{
			WLog_ERR(TAG, "response error %d", status);
		}
	}

	Stream_Free(output, TRUE);
	return status;
}

static int tsmf_on_close(IWTSVirtualChannelCallback *pChannelCallback)
{
	TSMF_STREAM* stream;
	TSMF_PRESENTATION* presentation;
	TSMF_CHANNEL_CALLBACK* callback = (TSMF_CHANNEL_CALLBACK*) pChannelCallback;

	DEBUG_TSMF("");

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

static int tsmf_on_new_channel_connection(IWTSListenerCallback *pListenerCallback,
		IWTSVirtualChannel *pChannel,
		BYTE *Data,
		int *pbAccept,
		IWTSVirtualChannelCallback **ppCallback)
{
	TSMF_CHANNEL_CALLBACK* callback;
	TSMF_LISTENER_CALLBACK* listener_callback = (TSMF_LISTENER_CALLBACK*) pListenerCallback;

	DEBUG_TSMF("");

	callback = (TSMF_CHANNEL_CALLBACK*) calloc(1, sizeof(TSMF_CHANNEL_CALLBACK));

	if (!callback)
		return -1;

	callback->iface.OnDataReceived = tsmf_on_data_received;
	callback->iface.OnClose = tsmf_on_close;
	callback->iface.OnOpen = NULL;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;

	*ppCallback = (IWTSVirtualChannelCallback*) callback;

	return 0;
}

static int tsmf_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	int status;
	TSMF_PLUGIN* tsmf = (TSMF_PLUGIN*) pPlugin;

	DEBUG_TSMF("");

	tsmf->listener_callback = (TSMF_LISTENER_CALLBACK*) calloc(1, sizeof(TSMF_LISTENER_CALLBACK));

	if (!tsmf->listener_callback)
		return -1;

	tsmf->listener_callback->iface.OnNewChannelConnection = tsmf_on_new_channel_connection;
	tsmf->listener_callback->plugin = pPlugin;
	tsmf->listener_callback->channel_mgr = pChannelMgr;

	status = pChannelMgr->CreateListener(pChannelMgr, "TSMF", 0,
			(IWTSListenerCallback*) tsmf->listener_callback, &(tsmf->listener));

	tsmf->listener->pInterface = tsmf->iface.pInterface;

	return status;
}

static int tsmf_plugin_terminated(IWTSPlugin* pPlugin)
{
	TSMF_PLUGIN* tsmf = (TSMF_PLUGIN*) pPlugin;

	DEBUG_TSMF("");

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

static void tsmf_process_addin_args(IWTSPlugin *pPlugin, ADDIN_ARGV *args)
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
	TsmfClientContext* context;

	tsmf = (TSMF_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "tsmf");

	if (!tsmf)
	{
		tsmf = (TSMF_PLUGIN*) calloc(1, sizeof(TSMF_PLUGIN));
		if (!tsmf)
			return -1;

		tsmf->iface.Initialize = tsmf_plugin_initialize;
		tsmf->iface.Connected = NULL;
		tsmf->iface.Disconnected = NULL;
		tsmf->iface.Terminated = tsmf_plugin_terminated;

		context = (TsmfClientContext*) calloc(1, sizeof(TsmfClientContext));
		if (!context)
			goto error_context;

		context->handle = (void*) tsmf;
		tsmf->iface.pInterface = (void*) context;

		if (!tsmf_media_init())
			goto error_init;

		status = pEntryPoints->RegisterPlugin(pEntryPoints, "tsmf", (IWTSPlugin*) tsmf);
	}

	if (status == 0)
	{
		tsmf_process_addin_args((IWTSPlugin*) tsmf, pEntryPoints->GetPluginData(pEntryPoints));
	}

	return status;

error_init:
	free(context);
error_context:
	free(tsmf);
	return -1;
}
