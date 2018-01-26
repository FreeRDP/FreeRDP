/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Optimized Remoting Virtual Channel Extension
 *
 * Copyright 2017 David Fort <contact@hardening-consulting.com>
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
#include <winpr/synch.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <winpr/cmdline.h>
#include <winpr/collections.h>

#include <freerdp/addin.h>
#include <freerdp/client/video.h>
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("video.client")

#include "video_main.h"

struct _VIDEO_CHANNEL_CALLBACK
{
	IWTSVirtualChannelCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	IWTSVirtualChannel* channel;
};
typedef struct _VIDEO_CHANNEL_CALLBACK VIDEO_CHANNEL_CALLBACK;

struct _VIDEO_LISTENER_CALLBACK
{
	IWTSListenerCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	VIDEO_CHANNEL_CALLBACK* channel_callback;
};
typedef struct _VIDEO_LISTENER_CALLBACK VIDEO_LISTENER_CALLBACK;

struct _VIDEO_PLUGIN
{
	IWTSPlugin wtsPlugin;

	IWTSListener* controlListener;
	IWTSListener* dataListener;
	VIDEO_LISTENER_CALLBACK* control_callback;
	VIDEO_LISTENER_CALLBACK* data_callback;

	VideoClientContext *context;
};
typedef struct _VIDEO_PLUGIN VIDEO_PLUGIN;

static const char *video_command_name(BYTE cmd)
{
	switch(cmd)
	{
	case TSMM_START_PRESENTATION:
		return "start";
	case TSMM_STOP_PRESENTATION:
		return "stop";
	default:
		return "<unknown>";
	}
}

static UINT video_read_tsmm_presentation_req(VideoClientContext *context, wStream *s)
{
	TSMM_PRESENTATION_REQUEST req;
	UINT ret = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 60)
	{
		WLog_ERR(TAG, "not enough bytes for a TSMM_PRESENTATION_REQUEST");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT8(s, req.PresentationId);
	Stream_Read_UINT8(s, req.Version);
	Stream_Read_UINT8(s, req.Command);
	Stream_Read_UINT8(s, req.FrameRate); /* FrameRate - reserved and ignored */

	Stream_Seek_UINT16(s); /* AverageBitrateKbps reserved and ignored */
	Stream_Seek_UINT16(s); /* reserved */

	Stream_Read_UINT32(s, req.SourceWidth);
	Stream_Read_UINT32(s, req.SourceHeight);
	Stream_Read_UINT32(s, req.ScaledWidth);
	Stream_Read_UINT32(s, req.ScaledHeight);
	Stream_Read_UINT64(s, req.hnsTimestampOffset);
	Stream_Read_UINT64(s, req.GeometryMappingId);
	Stream_Read(s, req.VideoSubtypeId, 16);

	Stream_Read_UINT32(s, req.cbExtra);

	if (Stream_GetRemainingLength(s) < req.cbExtra)
	{
		WLog_ERR(TAG, "not enough bytes for cbExtra of TSMM_PRESENTATION_REQUEST");
		return ERROR_INVALID_DATA;
	}

	req.pExtraData = Stream_Pointer(s);

	WLog_DBG(TAG, "presentationReq: id:%"PRIu8" version:%"PRIu8" command:%s srcWidth/srcHeight=%"PRIu32"x%"PRIu32
			" scaled Width/Height=%"PRIu32"x%"PRIu32" timestamp=%"PRIu64" mappingId=%"PRIx64"",
			req.PresentationId, req.Version, video_command_name(req.Command),
			req.SourceWidth, req.SourceHeight, req.ScaledWidth, req.ScaledHeight,
			req.hnsTimestampOffset, req.GeometryMappingId);

	if (context->PresentationRequest)
		ret = context->PresentationRequest(context, &req);

	return ret;
}

static UINT video_control_send_presentation_response(VideoClientContext *context, TSMM_PRESENTATION_RESPONSE *resp)
{
	BYTE buf[12];
	wStream *s;
	VIDEO_PLUGIN* video = (VIDEO_PLUGIN *)context->handle;
	IWTSVirtualChannel* channel;
	UINT ret;

	s = Stream_New(buf, 12);
	if (!s)
		return CHANNEL_RC_NO_MEMORY;

	Stream_Write_UINT32(s, 12); /* cbSize */
	Stream_Write_UINT32(s, TSMM_PACKET_TYPE_PRESENTATION_RESPONSE); /* PacketType */
	Stream_Write_UINT8(s, resp->PresentationId);
	Stream_Zero(s, 3);
	Stream_SealLength(s);

	channel = video->control_callback->channel_callback->channel;
	ret = channel->Write(channel, 12, buf, NULL);
	Stream_Free(s, FALSE);

	return ret;
}


/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT video_control_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream *s)
{
	VIDEO_CHANNEL_CALLBACK* callback = (VIDEO_CHANNEL_CALLBACK*) pChannelCallback;
	VIDEO_PLUGIN* video;
	VideoClientContext *context;
	UINT ret = CHANNEL_RC_OK;
	UINT32 cbSize, packetType;

	video = (VIDEO_PLUGIN*) callback->plugin;
	context = (VideoClientContext *)video->wtsPlugin.pInterface;

	if (Stream_GetRemainingLength(s) < 4)
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, cbSize);
	if (cbSize < 8 || Stream_GetRemainingLength(s) < (cbSize-4))
	{
		WLog_ERR(TAG, "invalid cbSize");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, packetType);
	switch (packetType)
	{
	case TSMM_PACKET_TYPE_PRESENTATION_REQUEST:
		ret = video_read_tsmm_presentation_req(context, s);
		break;
	default:
		WLog_ERR(TAG, "not expecting packet type %"PRIu32"", packetType);
		ret = ERROR_UNSUPPORTED_TYPE;
		break;
	}

	return ret;
}

static UINT video_control_send_client_notification(VideoClientContext *context, TSMM_CLIENT_NOTIFICATION *notif)
{
	BYTE buf[100];
	wStream *s;
	VIDEO_PLUGIN* video = (VIDEO_PLUGIN *)context->handle;
	IWTSVirtualChannel* channel;
	UINT ret;
	UINT32 cbSize;

	s = Stream_New(buf, 30);
	if (!s)
		return CHANNEL_RC_NO_MEMORY;

	cbSize = 16;
	Stream_Seek_UINT32(s); /* cbSize */
	Stream_Write_UINT32(s, TSMM_PACKET_TYPE_CLIENT_NOTIFICATION); /* PacketType */
	Stream_Write_UINT8(s, notif->PresentationId);
	Stream_Write_UINT8(s, notif->NotificationType);
	Stream_Zero(s, 2);
	if (notif->NotificationType == TSMM_CLIENT_NOTIFICATION_TYPE_FRAMERATE_OVERRIDE)
	{
		Stream_Write_UINT32(s, 16); /* cbData */

		/* TSMM_CLIENT_NOTIFICATION_FRAMERATE_OVERRIDE */
		Stream_Write_UINT32(s, notif->FramerateOverride.Flags);
		Stream_Write_UINT32(s, notif->FramerateOverride.DesiredFrameRate);
		Stream_Zero(s, 4 * 2);

		cbSize += 4 * 4;
	}
	else
	{
		Stream_Write_UINT32(s, 0); /* cbData */
	}

	Stream_SealLength(s);
	Stream_SetPosition(s, 0);
	Stream_Write_UINT32(s, cbSize);
	Stream_Free(s, FALSE);

	channel = video->control_callback->channel_callback->channel;
	ret = channel->Write(channel, cbSize, buf, NULL);

	return ret;
}


static UINT video_data_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream *s)
{
	VIDEO_CHANNEL_CALLBACK* callback = (VIDEO_CHANNEL_CALLBACK*) pChannelCallback;
	VIDEO_PLUGIN* video;
	VideoClientContext *context;
	UINT32 cbSize, packetType;
	TSMM_VIDEO_DATA data;

	video = (VIDEO_PLUGIN*) callback->plugin;
	context = (VideoClientContext *)video->wtsPlugin.pInterface;

	if (Stream_GetRemainingLength(s) < 4)
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, cbSize);
	if (cbSize < 8 || Stream_GetRemainingLength(s) < (cbSize-4))
	{
		WLog_ERR(TAG, "invalid cbSize");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, packetType);
	if (packetType != TSMM_PACKET_TYPE_VIDEO_DATA)
	{
		WLog_ERR(TAG, "only expecting VIDEO_DATA on the data channel");
		return ERROR_INVALID_DATA;
	}

	if (Stream_GetRemainingLength(s) < 32)
	{
		WLog_ERR(TAG, "not enough bytes for a TSMM_VIDEO_DATA");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT8(s, data.PresentationId);
	Stream_Read_UINT8(s, data.Version);
	Stream_Read_UINT8(s, data.Flags);
	Stream_Seek_UINT8(s); /* reserved */
	Stream_Read_UINT64(s, data.hnsTimestamp);
	Stream_Read_UINT64(s, data.hnsDuration);
	Stream_Read_UINT16(s, data.CurrentPacketIndex);
	Stream_Read_UINT16(s, data.PacketsInSample);
	Stream_Read_UINT32(s, data.SampleNumber);
	Stream_Read_UINT32(s, data.cbSample);
	data.pSample = Stream_Pointer(s);

	WLog_DBG(TAG, "videoData: id:%"PRIu8" version:%"PRIu8" flags:0x%"PRIx8" timestamp=%"PRIu64" duration=%"PRIu64
			" curPacketIndex:%"PRIu16" packetInSample:%"PRIu16" sampleNumber:%"PRIu32" cbSample:%"PRIu32"",
			data.PresentationId, data.Version, data.Flags, data.hnsTimestamp, data.hnsDuration,
			data.CurrentPacketIndex, data.PacketsInSample, data.SampleNumber, data.cbSample);

	if (context->VideoData)
		return context->VideoData(context, &data);

	return CHANNEL_RC_OK;
}


/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT video_control_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	free(pChannelCallback);
	return CHANNEL_RC_OK;
}

static UINT video_data_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	free(pChannelCallback);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT video_control_on_new_channel_connection(IWTSListenerCallback* listenerCallback,
	IWTSVirtualChannel* channel, BYTE* Data, BOOL* pbAccept,
	IWTSVirtualChannelCallback** ppCallback)
{
	VIDEO_CHANNEL_CALLBACK* callback;
	VIDEO_LISTENER_CALLBACK* listener_callback = (VIDEO_LISTENER_CALLBACK*) listenerCallback;

	callback = (VIDEO_CHANNEL_CALLBACK*) calloc(1, sizeof(VIDEO_CHANNEL_CALLBACK));
	if (!callback)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	callback->iface.OnDataReceived = video_control_on_data_received;
	callback->iface.OnClose = video_control_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = channel;
	listener_callback->channel_callback = callback;

	*ppCallback = (IWTSVirtualChannelCallback*) callback;

	return CHANNEL_RC_OK;
}

static UINT video_data_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
	IWTSVirtualChannel* pChannel, BYTE* Data, BOOL* pbAccept,
	IWTSVirtualChannelCallback** ppCallback)
{
	VIDEO_CHANNEL_CALLBACK* callback;
	VIDEO_LISTENER_CALLBACK* listener_callback = (VIDEO_LISTENER_CALLBACK*) pListenerCallback;

	callback = (VIDEO_CHANNEL_CALLBACK*) calloc(1, sizeof(VIDEO_CHANNEL_CALLBACK));
	if (!callback)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	callback->iface.OnDataReceived = video_data_on_data_received;
	callback->iface.OnClose = video_data_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;
	listener_callback->channel_callback = callback;

	*ppCallback = (IWTSVirtualChannelCallback*) callback;

	return CHANNEL_RC_OK;
}


/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT video_plugin_initialize(IWTSPlugin* plugin, IWTSVirtualChannelManager* channelMgr)
{
	UINT status;
	VIDEO_PLUGIN* video = (VIDEO_PLUGIN *)plugin;
	VIDEO_LISTENER_CALLBACK *callback;

	video->control_callback = callback = (VIDEO_LISTENER_CALLBACK*) calloc(1, sizeof(VIDEO_LISTENER_CALLBACK));
	if (!callback)
	{
		WLog_ERR(TAG, "calloc for control callback failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	callback->iface.OnNewChannelConnection = video_control_on_new_channel_connection;
	callback->plugin = plugin;
	callback->channel_mgr = channelMgr;

	status = channelMgr->CreateListener(channelMgr, VIDEO_CONTROL_DVC_CHANNEL_NAME, 0,
		(IWTSListenerCallback*)callback, &(video->controlListener));

	if (status != CHANNEL_RC_OK)
		return status;
	video->controlListener->pInterface = video->wtsPlugin.pInterface;


	video->data_callback = callback = (VIDEO_LISTENER_CALLBACK*) calloc(1, sizeof(VIDEO_LISTENER_CALLBACK));
	if (!callback)
	{
		WLog_ERR(TAG, "calloc for data callback failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	callback->iface.OnNewChannelConnection = video_data_on_new_channel_connection;
	callback->plugin = plugin;
	callback->channel_mgr = channelMgr;

	status = channelMgr->CreateListener(channelMgr, VIDEO_DATA_DVC_CHANNEL_NAME, 0,
		(IWTSListenerCallback*)callback, &(video->dataListener));

	if (status == CHANNEL_RC_OK)
		video->dataListener->pInterface = video->wtsPlugin.pInterface;

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT video_plugin_terminated(IWTSPlugin* pPlugin)
{
	VIDEO_PLUGIN* video = (VIDEO_PLUGIN*) pPlugin;

	free(video->control_callback);
	free(video->data_callback);
	free(video->wtsPlugin.pInterface);
	free(pPlugin);
	return CHANNEL_RC_OK;
}

/**
 * Channel Client Interface
 */


#ifdef BUILTIN_CHANNELS
#define DVCPluginEntry		video_DVCPluginEntry
#else
#define DVCPluginEntry		FREERDP_API DVCPluginEntry
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	UINT error = CHANNEL_RC_OK;
	VIDEO_PLUGIN* videoPlugin;
	VideoClientContext* context;

	videoPlugin = (VIDEO_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "video");
	if (!videoPlugin)
	{
		videoPlugin = (VIDEO_PLUGIN*) calloc(1, sizeof(VIDEO_PLUGIN));
		if (!videoPlugin)
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		videoPlugin->wtsPlugin.Initialize = video_plugin_initialize;
		videoPlugin->wtsPlugin.Connected = NULL;
		videoPlugin->wtsPlugin.Disconnected = NULL;
		videoPlugin->wtsPlugin.Terminated = video_plugin_terminated;

		context = (VideoClientContext*) calloc(1, sizeof(VideoClientContext));
		if (!context)
		{
			WLog_ERR(TAG, "calloc failed!");
			free(videoPlugin);
			return CHANNEL_RC_NO_MEMORY;
		}

		context->handle = (void*) videoPlugin;
		context->PresentationResponse = video_control_send_presentation_response;
		context->ClientNotification = video_control_send_client_notification;

		videoPlugin->wtsPlugin.pInterface = (void*) context;
		videoPlugin->context = context;

		error = pEntryPoints->RegisterPlugin(pEntryPoints, "video", (IWTSPlugin*) videoPlugin);
	}
	else
	{
		WLog_ERR(TAG, "could not get video Plugin.");
		return CHANNEL_RC_BAD_CHANNEL;
	}

	return error;
}
