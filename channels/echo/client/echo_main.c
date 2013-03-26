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
#include <winpr/cmdline.h>

#include <freerdp/addin.h>

#include <winpr/stream.h>

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

static int echo_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, UINT32 cbSize, BYTE* pBuffer)
{
	int error;
	ECHO_CHANNEL_CALLBACK* callback = (ECHO_CHANNEL_CALLBACK*) pChannelCallback;

#ifdef WITH_DEBUG_DVC
	int i = 0;
	char* debug_buffer;
	char* p;

	if (cbSize > 0)
	{
		debug_buffer = (char*) malloc(3 * cbSize);
		ZeroMemory(debug_buffer, 3 * cbSize);

		p = debug_buffer;

		for (i = 0; i < (int) (cbSize - 1); i++)
		{
			sprintf(p, "%02x ", pBuffer[i]);
			p += 3;
		}
		sprintf(p, "%02x", pBuffer[i]);

		DEBUG_DVC("ECHO %d: %s", cbSize, debug_buffer);
		free(debug_buffer);
	}
#endif

	/* echo back what we have received. ECHO does not have any message IDs. */
	error = callback->channel->Write(callback->channel, cbSize, pBuffer, NULL);

	return error;
}

static int echo_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	ECHO_CHANNEL_CALLBACK* callback = (ECHO_CHANNEL_CALLBACK*) pChannelCallback;

	DEBUG_DVC("");

	free(callback);

	return 0;
}

static int echo_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
	IWTSVirtualChannel* pChannel, BYTE* Data, int* pbAccept,
	IWTSVirtualChannelCallback** ppCallback)
{
	ECHO_CHANNEL_CALLBACK* callback;
	ECHO_LISTENER_CALLBACK* listener_callback = (ECHO_LISTENER_CALLBACK*) pListenerCallback;

	DEBUG_DVC("");

	callback = (ECHO_CHANNEL_CALLBACK*) malloc(sizeof(ECHO_CHANNEL_CALLBACK));
	ZeroMemory(callback, sizeof(ECHO_CHANNEL_CALLBACK));

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

	DEBUG_DVC("");

	echo->listener_callback = (ECHO_LISTENER_CALLBACK*) malloc(sizeof(ECHO_LISTENER_CALLBACK));
	ZeroMemory(echo->listener_callback, sizeof(ECHO_LISTENER_CALLBACK));

	echo->listener_callback->iface.OnNewChannelConnection = echo_on_new_channel_connection;
	echo->listener_callback->plugin = pPlugin;
	echo->listener_callback->channel_mgr = pChannelMgr;

	return pChannelMgr->CreateListener(pChannelMgr, "ECHO", 0,
		(IWTSListenerCallback*) echo->listener_callback, NULL);
}

static int echo_plugin_terminated(IWTSPlugin* pPlugin)
{
	ECHO_PLUGIN* echo = (ECHO_PLUGIN*) pPlugin;

	DEBUG_DVC("");

	free(echo);

	return 0;
}

#ifdef STATIC_CHANNELS
#define DVCPluginEntry		echo_DVCPluginEntry
#endif

int DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	int error = 0;
	ECHO_PLUGIN* echo;

	echo = (ECHO_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "echo");

	if (echo == NULL)
	{
		echo = (ECHO_PLUGIN*) malloc(sizeof(ECHO_PLUGIN));
		ZeroMemory(echo, sizeof(ECHO_PLUGIN));

		echo->iface.Initialize = echo_plugin_initialize;
		echo->iface.Connected = NULL;
		echo->iface.Disconnected = NULL;
		echo->iface.Terminated = echo_plugin_terminated;

		error = pEntryPoints->RegisterPlugin(pEntryPoints, "echo", (IWTSPlugin*) echo);
	}

	return error;
}
