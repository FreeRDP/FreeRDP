/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Echo Virtual Channel Extension
 *
 * Copyright 2013 Christian Hofstaedtler
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

#include <stdio.h>
#include <stdlib.h>

#include <winpr/crt.h>
#include <winpr/stream.h>

#include "echo_main.h"
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("echo.client")

typedef struct _ECHO_LISTENER_CALLBACK ECHO_LISTENER_CALLBACK;
struct _ECHO_LISTENER_CALLBACK
{
	IWTSListenerCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
};

typedef struct _ECHO_CHANNEL_CALLBACK ECHO_CHANNEL_CALLBACK;
struct _ECHO_CHANNEL_CALLBACK
{
	IWTSVirtualChannelCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	IWTSVirtualChannel* channel;
};

typedef struct _ECHO_PLUGIN ECHO_PLUGIN;
struct _ECHO_PLUGIN
{
	IWTSPlugin iface;

	ECHO_LISTENER_CALLBACK* listener_callback;
};

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT echo_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream *data)
{
	ECHO_CHANNEL_CALLBACK* callback = (ECHO_CHANNEL_CALLBACK*) pChannelCallback;
	BYTE* pBuffer = Stream_Pointer(data);
	UINT32 cbSize = Stream_GetRemainingLength(data);

	/* echo back what we have received. ECHO does not have any message IDs. */
	return callback->channel->Write(callback->channel, cbSize, pBuffer, NULL);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT echo_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	ECHO_CHANNEL_CALLBACK* callback = (ECHO_CHANNEL_CALLBACK*) pChannelCallback;

	free(callback);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT echo_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
	IWTSVirtualChannel* pChannel, BYTE* Data, BOOL* pbAccept,
	IWTSVirtualChannelCallback** ppCallback)
{
	ECHO_CHANNEL_CALLBACK* callback;
	ECHO_LISTENER_CALLBACK* listener_callback = (ECHO_LISTENER_CALLBACK*) pListenerCallback;

	callback = (ECHO_CHANNEL_CALLBACK*) calloc(1, sizeof(ECHO_CHANNEL_CALLBACK));

	if (!callback)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	callback->iface.OnDataReceived = echo_on_data_received;
	callback->iface.OnClose = echo_on_close;
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
static UINT echo_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	ECHO_PLUGIN* echo = (ECHO_PLUGIN*) pPlugin;

	echo->listener_callback = (ECHO_LISTENER_CALLBACK*) calloc(1, sizeof(ECHO_LISTENER_CALLBACK));

	if (!echo->listener_callback)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	echo->listener_callback->iface.OnNewChannelConnection = echo_on_new_channel_connection;
	echo->listener_callback->plugin = pPlugin;
	echo->listener_callback->channel_mgr = pChannelMgr;

	return pChannelMgr->CreateListener(pChannelMgr, "ECHO", 0,
		(IWTSListenerCallback*) echo->listener_callback, NULL);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT echo_plugin_terminated(IWTSPlugin* pPlugin)
{
	ECHO_PLUGIN* echo = (ECHO_PLUGIN*) pPlugin;

	free(echo);

	return CHANNEL_RC_OK;
}

#ifdef BUILTIN_CHANNELS
#define DVCPluginEntry		echo_DVCPluginEntry
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
	UINT status = CHANNEL_RC_OK;
	ECHO_PLUGIN* echo;

	echo = (ECHO_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "echo");

	if (!echo)
	{
		echo = (ECHO_PLUGIN*) calloc(1, sizeof(ECHO_PLUGIN));

		if (!echo)
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		echo->iface.Initialize = echo_plugin_initialize;
		echo->iface.Connected = NULL;
		echo->iface.Disconnected = NULL;
		echo->iface.Terminated = echo_plugin_terminated;

		status = pEntryPoints->RegisterPlugin(pEntryPoints, "echo", (IWTSPlugin*) echo);
	}

	return status;
}
