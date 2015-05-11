/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Echo Virtual Channel Extension
 *
 * Copyright 2013 Christian Hofstaedtler
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
#include <winpr/stream.h>
#include <winpr/cmdline.h>

#include <freerdp/addin.h>

#include "echo_main.h"

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

static int echo_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream *data)
{
	int status;
	ECHO_CHANNEL_CALLBACK* callback = (ECHO_CHANNEL_CALLBACK*) pChannelCallback;
	BYTE* pBuffer = Stream_Pointer(data);
	UINT32 cbSize = Stream_GetRemainingLength(data);

	/* echo back what we have received. ECHO does not have any message IDs. */
	status = callback->channel->Write(callback->channel, cbSize, pBuffer, NULL);

	return status;
}

static int echo_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	ECHO_CHANNEL_CALLBACK* callback = (ECHO_CHANNEL_CALLBACK*) pChannelCallback;

	free(callback);

	return 0;
}

static int echo_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
	IWTSVirtualChannel* pChannel, BYTE* Data, int* pbAccept,
	IWTSVirtualChannelCallback** ppCallback)
{
	ECHO_CHANNEL_CALLBACK* callback;
	ECHO_LISTENER_CALLBACK* listener_callback = (ECHO_LISTENER_CALLBACK*) pListenerCallback;

	callback = (ECHO_CHANNEL_CALLBACK*) calloc(1, sizeof(ECHO_CHANNEL_CALLBACK));

	if (!callback)
		return -1;

	callback->iface.OnDataReceived = echo_on_data_received;
	callback->iface.OnClose = echo_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;

	*ppCallback = (IWTSVirtualChannelCallback*) callback;

	return 0;
}

static int echo_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	ECHO_PLUGIN* echo = (ECHO_PLUGIN*) pPlugin;

	echo->listener_callback = (ECHO_LISTENER_CALLBACK*) calloc(1, sizeof(ECHO_LISTENER_CALLBACK));

	if (!echo->listener_callback)
		return -1;

	echo->listener_callback->iface.OnNewChannelConnection = echo_on_new_channel_connection;
	echo->listener_callback->plugin = pPlugin;
	echo->listener_callback->channel_mgr = pChannelMgr;

	return pChannelMgr->CreateListener(pChannelMgr, "ECHO", 0,
		(IWTSListenerCallback*) echo->listener_callback, NULL);
}

static int echo_plugin_terminated(IWTSPlugin* pPlugin)
{
	ECHO_PLUGIN* echo = (ECHO_PLUGIN*) pPlugin;

	free(echo);

	return 0;
}

#ifdef STATIC_CHANNELS
#define DVCPluginEntry		echo_DVCPluginEntry
#endif

int DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	int status = 0;
	ECHO_PLUGIN* echo;

	echo = (ECHO_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "echo");

	if (!echo)
	{
		echo = (ECHO_PLUGIN*) calloc(1, sizeof(ECHO_PLUGIN));

		if (!echo)
			return -1;

		echo->iface.Initialize = echo_plugin_initialize;
		echo->iface.Connected = NULL;
		echo->iface.Disconnected = NULL;
		echo->iface.Terminated = echo_plugin_terminated;

		status = pEntryPoints->RegisterPlugin(pEntryPoints, "echo", (IWTSPlugin*) echo);
	}

	return status;
}
