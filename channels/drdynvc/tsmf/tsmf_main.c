/**
 * FreeRDP: A Remote Desktop Protocol client.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>

#include "drdynvc_types.h"
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

	uint8 presentation_id[16];
	uint32 stream_id;
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
	uint32 message_id, uint64 duration, uint32 data_size)
{
	STREAM* s;
	int error;
	TSMF_CHANNEL_CALLBACK* callback = (TSMF_CHANNEL_CALLBACK*) pChannelCallback;

	s = stream_new(32);
	stream_write_uint32(s, TSMF_INTERFACE_CLIENT_NOTIFICATIONS | STREAM_ID_PROXY);
	stream_write_uint32(s, message_id);
	stream_write_uint32(s, PLAYBACK_ACK); /* FunctionId */
	stream_write_uint32(s, callback->stream_id); /* StreamId */
	stream_write_uint64(s, duration); /* DataDuration */
	stream_write_uint64(s, data_size); /* cbData */
	
	DEBUG_DVC("response size %d", stream_get_length(s));
	error = callback->channel->Write(callback->channel, stream_get_length(s), stream_get_head(s), NULL);
	if (error)
	{
		DEBUG_WARN("response error %d", error);
	}
	stream_free(s);
}

boolean tsmf_push_event(IWTSVirtualChannelCallback* pChannelCallback,
	RDP_EVENT* event)
{
	int error;
	TSMF_CHANNEL_CALLBACK* callback = (TSMF_CHANNEL_CALLBACK*) pChannelCallback;

	error = callback->channel_mgr->PushEvent(callback->channel_mgr, event);
	if (error)
	{
		DEBUG_WARN("response error %d", error);
		return false;
	}
	return true;
}

static int tsmf_on_data_received(IWTSVirtualChannelCallback* pChannelCallback,
	uint32 cbSize,
	uint8* pBuffer)
{
	int length;
	STREAM* input;
	STREAM* output;
	int error = -1;
	TSMF_IFMAN ifman;
	uint32 MessageId;
	uint32 FunctionId;
	uint32 InterfaceId;
	TSMF_CHANNEL_CALLBACK* callback = (TSMF_CHANNEL_CALLBACK*) pChannelCallback;

	/* 2.2.1 Shared Message Header (SHARED_MSG_HEADER) */
	if (cbSize < 12)
	{
		DEBUG_WARN("invalid size. cbSize=%d", cbSize);
		return 1;
	}
	input = stream_new(0);
	stream_attach(input, (uint8*) pBuffer, cbSize);
	output = stream_new(256);
	stream_seek(output, 8);

	stream_read_uint32(input, InterfaceId);
	stream_read_uint32(input, MessageId);
	stream_read_uint32(input, FunctionId);
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
	ifman.output_pending = false;
	ifman.output_interface_id = InterfaceId;

	switch (InterfaceId)
	{
		case TSMF_INTERFACE_CAPABILITIES | STREAM_ID_NONE:

			switch (FunctionId)
			{
				case RIM_EXCHANGE_CAPABILITY_REQUEST:
					error = tsmf_ifman_rim_exchange_capability_request(&ifman);
					break;

				default:
					break;
			}
			break;

		case TSMF_INTERFACE_DEFAULT | STREAM_ID_PROXY:

			switch (FunctionId)
			{
				case SET_CHANNEL_PARAMS:
					memcpy(callback->presentation_id, stream_get_tail(input), 16);
					stream_seek(input, 16);
					stream_read_uint32(input, callback->stream_id);
					DEBUG_DVC("SET_CHANNEL_PARAMS StreamId=%d", callback->stream_id);
					ifman.output_pending = true;
					error = 0;
					break;

				case EXCHANGE_CAPABILITIES_REQ:
					error = tsmf_ifman_exchange_capability_request(&ifman);
					break;

				case CHECK_FORMAT_SUPPORT_REQ:
					error = tsmf_ifman_check_format_support_request(&ifman);
					break;

				case ON_NEW_PRESENTATION:
					error = tsmf_ifman_on_new_presentation(&ifman);
					break;

				case ADD_STREAM:
					error = tsmf_ifman_add_stream(&ifman);
					break;

				case SET_TOPOLOGY_REQ:
					error = tsmf_ifman_set_topology_request(&ifman);
					break;

				case REMOVE_STREAM:
					error = tsmf_ifman_remove_stream(&ifman);
					break;

				case SHUTDOWN_PRESENTATION_REQ:
					error = tsmf_ifman_shutdown_presentation(&ifman);
					break;

				case ON_STREAM_VOLUME:
					error = tsmf_ifman_on_stream_volume(&ifman);
					break;

				case ON_CHANNEL_VOLUME:
					error = tsmf_ifman_on_channel_volume(&ifman);
					break;

				case SET_VIDEO_WINDOW:
					error = tsmf_ifman_set_video_window(&ifman);
					break;

				case UPDATE_GEOMETRY_INFO:
					error = tsmf_ifman_update_geometry_info(&ifman);
					break;

				case SET_ALLOCATOR:
					error = tsmf_ifman_set_allocator(&ifman);
					break;

				case NOTIFY_PREROLL:
					error = tsmf_ifman_notify_preroll(&ifman);
					break;

				case ON_SAMPLE:
					error = tsmf_ifman_on_sample(&ifman);
					break;

				case ON_FLUSH:
					error = tsmf_ifman_on_flush(&ifman);
					break;

				case ON_END_OF_STREAM:
					error = tsmf_ifman_on_end_of_stream(&ifman);
					break;

				case ON_PLAYBACK_STARTED:
					error = tsmf_ifman_on_playback_started(&ifman);
					break;

				case ON_PLAYBACK_PAUSED:
					error = tsmf_ifman_on_playback_paused(&ifman);
					break;

				case ON_PLAYBACK_RESTARTED:
					error = tsmf_ifman_on_playback_restarted(&ifman);
					break;

				case ON_PLAYBACK_STOPPED:
					error = tsmf_ifman_on_playback_stopped(&ifman);
					break;

				case ON_PLAYBACK_RATE_CHANGED:
					error = tsmf_ifman_on_playback_rate_changed(&ifman);
					break;

				default:
					break;
			}
			break;

		default:
			break;
	}

	stream_detach(input);
	stream_free(input);
	input = NULL;
	ifman.input = NULL;

	if (error == -1)
	{
		switch (FunctionId)
		{
			case RIMCALL_RELEASE:
				/* [MS-RDPEXPS] 2.2.2.2 Interface Release (IFACE_RELEASE)
				   This message does not require a reply. */
				error = 0;
				ifman.output_pending = 1;
				break;

			case RIMCALL_QUERYINTERFACE:
				/* [MS-RDPEXPS] 2.2.2.1.2 Query Interface Response (QI_RSP)
				   This message is not supported in this channel. */
				error = 0;
				break;
		}

		if (error == -1)
		{
			DEBUG_WARN("InterfaceId 0x%X FunctionId 0x%X not processed.",
				InterfaceId, FunctionId);
			/* When a request is not implemented we return empty response indicating error */
		}
		error = 0;
	}

	if (error == 0 && !ifman.output_pending)
	{
		/* Response packet does not have FunctionId */
		length = stream_get_length(output);
		stream_set_pos(output, 0);
		stream_write_uint32(output, ifman.output_interface_id);
		stream_write_uint32(output, MessageId);

		DEBUG_DVC("response size %d", length);
		error = callback->channel->Write(callback->channel, length, stream_get_head(output), NULL);
		if (error)
		{
			DEBUG_WARN("response error %d", error);
		}
	}

	stream_free(output);

	return error;
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
	xfree(pChannelCallback);

	return 0;
}

static int tsmf_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
	IWTSVirtualChannel* pChannel,
	uint8* Data,
	int* pbAccept,
	IWTSVirtualChannelCallback** ppCallback)
{
	TSMF_CHANNEL_CALLBACK* callback;
	TSMF_LISTENER_CALLBACK* listener_callback = (TSMF_LISTENER_CALLBACK*) pListenerCallback;

	DEBUG_DVC("");

	callback = xnew(TSMF_CHANNEL_CALLBACK);
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

	tsmf->listener_callback = xnew(TSMF_LISTENER_CALLBACK);
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
		xfree(tsmf->listener_callback);
	xfree(tsmf);

	return 0;
}

static void tsmf_process_plugin_data(IWTSPlugin* pPlugin, RDP_PLUGIN_DATA* data)
{
	TSMF_PLUGIN* tsmf = (TSMF_PLUGIN*) pPlugin;

	while (data && data->size > 0)
	{
		if (data->data[0] && ( strcmp((char*)data->data[0], "tsmf") == 0 || strstr((char*)data->data[0], "/tsmf.") != NULL) )
		{
			if (data->data[1] && strcmp((char*)data->data[1], "decoder") == 0)
			{
				tsmf->decoder_name = data->data[2];
			}
			else if (data->data[1] && strcmp((char*)data->data[1], "audio") == 0)
			{
				tsmf->audio_name = data->data[2];
				tsmf->audio_device = data->data[3];
			}
		}
		
		data = (RDP_PLUGIN_DATA*)(((void*)data) + data->size);
	}
}

int DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	TSMF_PLUGIN * tsmf;
	int error = 0;

	tsmf = (TSMF_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "tsmf");
	if (tsmf == NULL)
	{
		tsmf = xnew(TSMF_PLUGIN);

		tsmf->iface.Initialize = tsmf_plugin_initialize;
		tsmf->iface.Connected = NULL;
		tsmf->iface.Disconnected = NULL;
		tsmf->iface.Terminated = tsmf_plugin_terminated;
		error = pEntryPoints->RegisterPlugin(pEntryPoints, "tsmf", (IWTSPlugin*) tsmf);

		tsmf_media_init();
	}
	if (error == 0)
	{
		tsmf_process_plugin_data((IWTSPlugin*) tsmf,
			pEntryPoints->GetPluginData(pEntryPoints));
	}
	return error;
}

