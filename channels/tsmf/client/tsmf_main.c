/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Redirection Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

BOOL tsmf_send_eos_response(IWTSVirtualChannelCallback* pChannelCallback, UINT32 message_id)
{
	wStream* s = NULL;
	int status = -1;
	TSMF_CHANNEL_CALLBACK* callback = (TSMF_CHANNEL_CALLBACK*) pChannelCallback;

	if (!callback)
	{
		DEBUG_TSMF("No callback reference - unable to send eos response!");
		return FALSE;
	}

	if (callback && callback->stream_id && callback->channel && callback->channel->Write)
	{
		s = Stream_New(NULL, 24);
		if (!s)
			return FALSE;
		Stream_Write_UINT32(s, TSMF_INTERFACE_CLIENT_NOTIFICATIONS | STREAM_ID_PROXY);
		Stream_Write_UINT32(s, message_id);
		Stream_Write_UINT32(s, CLIENT_EVENT_NOTIFICATION); /* FunctionId */
		Stream_Write_UINT32(s, callback->stream_id); /* StreamId */
		Stream_Write_UINT32(s, TSMM_CLIENT_EVENT_ENDOFSTREAM); /* EventId */
		Stream_Write_UINT32(s, 0); /* cbData */
		DEBUG_TSMF("EOS response size %i", Stream_GetPosition(s));

		status = callback->channel->Write(callback->channel, Stream_GetPosition(s), Stream_Buffer(s), NULL);
		if (status)
		{
			WLog_ERR(TAG, "response error %d", status);
		}
		Stream_Free(s, TRUE);
	}

	return (status == 0);
}

BOOL tsmf_playback_ack(IWTSVirtualChannelCallback *pChannelCallback,
			UINT32 message_id, UINT64 duration, UINT32 data_size)
{
	wStream *s = NULL;
	int status = -1;
	TSMF_CHANNEL_CALLBACK *callback = (TSMF_CHANNEL_CALLBACK *) pChannelCallback;

	s = Stream_New(NULL, 32);
	if (!s)
		return FALSE;

	Stream_Write_UINT32(s, TSMF_INTERFACE_CLIENT_NOTIFICATIONS | STREAM_ID_PROXY);
	Stream_Write_UINT32(s, message_id);
	Stream_Write_UINT32(s, PLAYBACK_ACK); /* FunctionId */
	Stream_Write_UINT32(s, callback->stream_id); /* StreamId */
	Stream_Write_UINT64(s, duration); /* DataDuration */
	Stream_Write_UINT64(s, data_size); /* cbData */

	DEBUG_TSMF("ACK response size %d", (int) Stream_GetPosition(s));

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
	return (status == 0);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT tsmf_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream *data)
{
	int length;
	wStream *input;
	wStream *output;
	UINT error = CHANNEL_RC_OK;
	BOOL processed = FALSE;
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
		return ERROR_INVALID_DATA;
	}

	input = data;
	output = Stream_New(NULL, 256);
	if (!output)
		return ERROR_OUTOFMEMORY;
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
					error = tsmf_ifman_rim_exchange_capability_request(&ifman);
					processed = TRUE;
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
					if (Stream_GetRemainingLength(input) < GUID_SIZE + 4)
					{
						error = ERROR_INVALID_DATA;
						goto out;
					}
					CopyMemory(callback->presentation_id, Stream_Pointer(input), GUID_SIZE);
					Stream_Seek(input, GUID_SIZE);
					Stream_Read_UINT32(input, callback->stream_id);
					DEBUG_TSMF("SET_CHANNEL_PARAMS StreamId=%d", callback->stream_id);
					ifman.output_pending = TRUE;
					processed = TRUE;
					break;
				case EXCHANGE_CAPABILITIES_REQ:
					error = tsmf_ifman_exchange_capability_request(&ifman);
					processed = TRUE;
					break;
				case CHECK_FORMAT_SUPPORT_REQ:
					error = tsmf_ifman_check_format_support_request(&ifman);
					processed = TRUE;
					break;
				case ON_NEW_PRESENTATION:
					error = tsmf_ifman_on_new_presentation(&ifman);
					processed = TRUE;
					break;
				case ADD_STREAM:
					error = tsmf_ifman_add_stream(&ifman, ((TSMF_PLUGIN*) callback->plugin)->rdpcontext);
					processed = TRUE;
					break;
				case SET_TOPOLOGY_REQ:
					error = tsmf_ifman_set_topology_request(&ifman);
					processed = TRUE;
					break;
				case REMOVE_STREAM:
					error = tsmf_ifman_remove_stream(&ifman);
					processed = TRUE;
					break;
				case SET_SOURCE_VIDEO_RECT:
					error = tsmf_ifman_set_source_video_rect(&ifman);
					processed = TRUE;
					break;
				case SHUTDOWN_PRESENTATION_REQ:
					error = tsmf_ifman_shutdown_presentation(&ifman);
					processed = TRUE;
					break;
				case ON_STREAM_VOLUME:
					error = tsmf_ifman_on_stream_volume(&ifman);
					processed = TRUE;
					break;
				case ON_CHANNEL_VOLUME:
					error = tsmf_ifman_on_channel_volume(&ifman);
					processed = TRUE;
					break;
				case SET_VIDEO_WINDOW:
					error = tsmf_ifman_set_video_window(&ifman);
					processed = TRUE;
					break;
				case UPDATE_GEOMETRY_INFO:
					error = tsmf_ifman_update_geometry_info(&ifman);
					processed = TRUE;
					break;
				case SET_ALLOCATOR:
					error = tsmf_ifman_set_allocator(&ifman);
					processed = TRUE;
					break;
				case NOTIFY_PREROLL:
					error = tsmf_ifman_notify_preroll(&ifman);
					processed = TRUE;
					break;
				case ON_SAMPLE:
					error = tsmf_ifman_on_sample(&ifman);
					processed = TRUE;
					break;
				case ON_FLUSH:
					error = tsmf_ifman_on_flush(&ifman);
					processed = TRUE;
					break;
				case ON_END_OF_STREAM:
					error = tsmf_ifman_on_end_of_stream(&ifman);
					processed = TRUE;
					break;
				case ON_PLAYBACK_STARTED:
					error = tsmf_ifman_on_playback_started(&ifman);
					processed = TRUE;
					break;
				case ON_PLAYBACK_PAUSED:
					error = tsmf_ifman_on_playback_paused(&ifman);
					processed = TRUE;
					break;
				case ON_PLAYBACK_RESTARTED:
					error = tsmf_ifman_on_playback_restarted(&ifman);
					processed = TRUE;
					break;
				case ON_PLAYBACK_STOPPED:
					error = tsmf_ifman_on_playback_stopped(&ifman);
					processed = TRUE;
					break;
				case ON_PLAYBACK_RATE_CHANGED:
					error = tsmf_ifman_on_playback_rate_changed(&ifman);
					processed = TRUE;
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

	if (error)
	{
		WLog_ERR(TAG, "ifman data received processing error %d", error);
	}

	if (!processed)
	{
		switch (FunctionId)
		{
			case RIMCALL_RELEASE:
				/* [MS-RDPEXPS] 2.2.2.2 Interface Release (IFACE_RELEASE)
				   This message does not require a reply. */
				processed = TRUE;
				ifman.output_pending = 1;
				break;
			case RIMCALL_QUERYINTERFACE:
				/* [MS-RDPEXPS] 2.2.2.1.2 Query Interface Response (QI_RSP)
				   This message is not supported in this channel. */
				processed = TRUE;
				break;
		}

		if (!processed)
		{
			WLog_ERR(TAG, "Unknown InterfaceId: 0x%04X MessageId: 0x%04X FunctionId: 0x%04X\n", InterfaceId, MessageId, FunctionId);
			/* When a request is not implemented we return empty response indicating error */
		}

		processed = TRUE;
	}

	if (processed && !ifman.output_pending)
	{
		/* Response packet does not have FunctionId */
		length = Stream_GetPosition(output);
		Stream_SetPosition(output, 0);
		Stream_Write_UINT32(output, ifman.output_interface_id);
		Stream_Write_UINT32(output, MessageId);
		DEBUG_TSMF("response size %d", length);
		error = callback->channel->Write(callback->channel, length, Stream_Buffer(output), NULL);

		if (error)
		{
			WLog_ERR(TAG, "response error %d", error);
		}
	}

out:
	Stream_Free(output, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT tsmf_on_close(IWTSVirtualChannelCallback *pChannelCallback)
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
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT tsmf_on_new_channel_connection(IWTSListenerCallback *pListenerCallback,
		IWTSVirtualChannel *pChannel,
		BYTE *Data,
		BOOL *pbAccept,
		IWTSVirtualChannelCallback **ppCallback)
{
	TSMF_CHANNEL_CALLBACK* callback;
	TSMF_LISTENER_CALLBACK* listener_callback = (TSMF_LISTENER_CALLBACK*) pListenerCallback;

	DEBUG_TSMF("");

	callback = (TSMF_CHANNEL_CALLBACK*) calloc(1, sizeof(TSMF_CHANNEL_CALLBACK));

	if (!callback)
		return CHANNEL_RC_NO_MEMORY;

	callback->iface.OnDataReceived = tsmf_on_data_received;
	callback->iface.OnClose = tsmf_on_close;
	callback->iface.OnOpen = NULL;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;

	*ppCallback = (IWTSVirtualChannelCallback*) callback;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT tsmf_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	UINT status;
	TSMF_PLUGIN* tsmf = (TSMF_PLUGIN*) pPlugin;

	DEBUG_TSMF("");

	tsmf->listener_callback = (TSMF_LISTENER_CALLBACK*) calloc(1, sizeof(TSMF_LISTENER_CALLBACK));

	if (!tsmf->listener_callback)
		return CHANNEL_RC_NO_MEMORY;

	tsmf->listener_callback->iface.OnNewChannelConnection = tsmf_on_new_channel_connection;
	tsmf->listener_callback->plugin = pPlugin;
	tsmf->listener_callback->channel_mgr = pChannelMgr;

	status = pChannelMgr->CreateListener(pChannelMgr, "TSMF", 0,
			(IWTSListenerCallback*) tsmf->listener_callback, &(tsmf->listener));

	tsmf->listener->pInterface = tsmf->iface.pInterface;

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT tsmf_plugin_terminated(IWTSPlugin* pPlugin)
{
	TSMF_PLUGIN* tsmf = (TSMF_PLUGIN*) pPlugin;

	DEBUG_TSMF("");

	free(tsmf->listener_callback);
	free(tsmf);

	return CHANNEL_RC_OK;
}

COMMAND_LINE_ARGUMENT_A tsmf_args[] =
{
	{ "sys", COMMAND_LINE_VALUE_REQUIRED, "<subsystem>", NULL, NULL, -1, NULL, "audio subsystem" },
	{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>", NULL, NULL, -1, NULL, "audio device name" },
	{ "decoder", COMMAND_LINE_VALUE_REQUIRED, "<subsystem>", NULL, NULL, -1, NULL, "decoder subsystem" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT tsmf_process_addin_args(IWTSPlugin *pPlugin, ADDIN_ARGV *args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	TSMF_PLUGIN* tsmf = (TSMF_PLUGIN*) pPlugin;

	flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON;

	status = CommandLineParseArgumentsA(args->argc, (const char**) args->argv,
						tsmf_args, flags, tsmf, NULL, NULL);
	if (status != 0)
		return ERROR_INVALID_DATA;

	arg = tsmf_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;
		CommandLineSwitchStart(arg)
		CommandLineSwitchCase(arg, "sys")
		{
			tsmf->audio_name = _strdup(arg->Value);
			if (!tsmf->audio_name)
				return ERROR_OUTOFMEMORY;
		}
		CommandLineSwitchCase(arg, "dev")
		{
			tsmf->audio_device = _strdup(arg->Value);
			if (!tsmf->audio_device)
				return ERROR_OUTOFMEMORY;
		}
		CommandLineSwitchCase(arg, "decoder")
		{
			tsmf->decoder_name = _strdup(arg->Value);
			if (!tsmf->decoder_name)
				return ERROR_OUTOFMEMORY;
		}
		CommandLineSwitchDefault(arg)
		{
		}
		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return CHANNEL_RC_OK;
}

#ifdef STATIC_CHANNELS
#define DVCPluginEntry	tsmf_DVCPluginEntry
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	UINT status = 0;
	TSMF_PLUGIN* tsmf;
	TsmfClientContext* context;
	UINT error = CHANNEL_RC_NO_MEMORY;

	tsmf = (TSMF_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "tsmf");

	if (!tsmf)
	{
		tsmf = (TSMF_PLUGIN*) calloc(1, sizeof(TSMF_PLUGIN));
		if (!tsmf)
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}


		tsmf->iface.Initialize = tsmf_plugin_initialize;
		tsmf->iface.Connected = NULL;
		tsmf->iface.Disconnected = NULL;
		tsmf->iface.Terminated = tsmf_plugin_terminated;
		tsmf->rdpcontext = ((freerdp*)((rdpSettings*) pEntryPoints->GetRdpSettings(pEntryPoints))->instance)->context;

		context = (TsmfClientContext*) calloc(1, sizeof(TsmfClientContext));
		if (!context)
		{
			WLog_ERR(TAG, "calloc failed!");
			goto error_context;
		}


		context->handle = (void*) tsmf;
		tsmf->iface.pInterface = (void*) context;

		if (!tsmf_media_init())
		{
			error = ERROR_INVALID_OPERATION;
			goto error_init;
		}

		status = pEntryPoints->RegisterPlugin(pEntryPoints, "tsmf", (IWTSPlugin*) tsmf);
	}

	if (status == CHANNEL_RC_OK)
	{
		status = tsmf_process_addin_args((IWTSPlugin*) tsmf, pEntryPoints->GetPluginData(pEntryPoints));
	}

	return status;

error_init:
	free(context);
error_context:
	free(tsmf);
	return error;
}
