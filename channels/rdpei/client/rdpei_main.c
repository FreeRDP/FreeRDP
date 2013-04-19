/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Input Virtual Channel Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "rdpei_main.h"

typedef struct _RDPEI_LISTENER_CALLBACK RDPEI_LISTENER_CALLBACK;
struct _RDPEI_LISTENER_CALLBACK
{
	IWTSListenerCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
};

typedef struct _RDPEI_CHANNEL_CALLBACK RDPEI_CHANNEL_CALLBACK;
struct _RDPEI_CHANNEL_CALLBACK
{
	IWTSVirtualChannelCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	IWTSVirtualChannel* channel;
};

typedef struct _RDPEI_PLUGIN RDPEI_PLUGIN;
struct _RDPEI_PLUGIN
{
	IWTSPlugin iface;

	RDPEI_LISTENER_CALLBACK* listener_callback;
};

static int rdpei_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, UINT32 cbSize, BYTE* pBuffer)
{
	int status = 0;
	//RDPEI_CHANNEL_CALLBACK* callback = (RDPEI_CHANNEL_CALLBACK*) pChannelCallback;

	return status;
}

static int rdpei_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	RDPEI_CHANNEL_CALLBACK* callback = (RDPEI_CHANNEL_CALLBACK*) pChannelCallback;

	DEBUG_DVC("");

	free(callback);

	return 0;
}

static int rdpei_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
	IWTSVirtualChannel* pChannel, BYTE* Data, int* pbAccept,
	IWTSVirtualChannelCallback** ppCallback)
{
	RDPEI_CHANNEL_CALLBACK* callback;
	RDPEI_LISTENER_CALLBACK* listener_callback = (RDPEI_LISTENER_CALLBACK*) pListenerCallback;

	DEBUG_DVC("");

	callback = (RDPEI_CHANNEL_CALLBACK*) malloc(sizeof(RDPEI_CHANNEL_CALLBACK));
	ZeroMemory(callback, sizeof(RDPEI_CHANNEL_CALLBACK));

	callback->iface.OnDataReceived = rdpei_on_data_received;
	callback->iface.OnClose = rdpei_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;

	*ppCallback = (IWTSVirtualChannelCallback*) callback;

	return 0;
}

static int rdpei_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	RDPEI_PLUGIN* echo = (RDPEI_PLUGIN*) pPlugin;

	DEBUG_DVC("");

	echo->listener_callback = (RDPEI_LISTENER_CALLBACK*) malloc(sizeof(RDPEI_LISTENER_CALLBACK));
	ZeroMemory(echo->listener_callback, sizeof(RDPEI_LISTENER_CALLBACK));

	echo->listener_callback->iface.OnNewChannelConnection = rdpei_on_new_channel_connection;
	echo->listener_callback->plugin = pPlugin;
	echo->listener_callback->channel_mgr = pChannelMgr;

	return pChannelMgr->CreateListener(pChannelMgr, "Microsoft::Windows::RDS::Input", 0,
		(IWTSListenerCallback*) echo->listener_callback, NULL);
}

static int rdpei_plugin_terminated(IWTSPlugin* pPlugin)
{
	RDPEI_PLUGIN* echo = (RDPEI_PLUGIN*) pPlugin;

	DEBUG_DVC("");

	free(echo);

	return 0;
}

#ifdef STATIC_CHANNELS
#define DVCPluginEntry		rdpei_DVCPluginEntry
#endif

int DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	int error = 0;
	RDPEI_PLUGIN* rdpei;

	rdpei = (RDPEI_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "rdpei");

	if (rdpei == NULL)
	{
		rdpei = (RDPEI_PLUGIN*) malloc(sizeof(RDPEI_PLUGIN));
		ZeroMemory(rdpei, sizeof(RDPEI_PLUGIN));

		rdpei->iface.Initialize = rdpei_plugin_initialize;
		rdpei->iface.Connected = NULL;
		rdpei->iface.Disconnected = NULL;
		rdpei->iface.Terminated = rdpei_plugin_terminated;

		error = pEntryPoints->RegisterPlugin(pEntryPoints, "rdpei", (IWTSPlugin*) rdpei);
	}

	return error;
}
