/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Graphics Pipeline Extension
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/sysinfo.h>
#include <winpr/cmdline.h>
#include <winpr/collections.h>

#include <freerdp/addin.h>

#include "rdpgfx_common.h"

#include "rdpgfx_main.h"

struct _RDPGFX_CHANNEL_CALLBACK
{
	IWTSVirtualChannelCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	IWTSVirtualChannel* channel;
};
typedef struct _RDPGFX_CHANNEL_CALLBACK RDPGFX_CHANNEL_CALLBACK;

struct _RDPGFX_LISTENER_CALLBACK
{
	IWTSListenerCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	RDPGFX_CHANNEL_CALLBACK* channel_callback;
};
typedef struct _RDPGFX_LISTENER_CALLBACK RDPGFX_LISTENER_CALLBACK;

struct _RDPGFX_PLUGIN
{
	IWTSPlugin iface;

	IWTSListener* listener;
	RDPGFX_LISTENER_CALLBACK* listener_callback;
};
typedef struct _RDPGFX_PLUGIN RDPGFX_PLUGIN;

int rdpgfx_recv_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	return 0;
}

static int rdpgfx_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, UINT32 cbSize, BYTE* pBuffer)
{
	wStream* s;
	int status = 0;
	RDPGFX_CHANNEL_CALLBACK* callback = (RDPGFX_CHANNEL_CALLBACK*) pChannelCallback;

	fprintf(stderr, "RdpGfxOnDataReceived\n");

	s = Stream_New(pBuffer, cbSize);

	status = rdpgfx_recv_pdu(callback, s);

	Stream_Free(s, FALSE);

	return status;
}

static int rdpgfx_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	RDPGFX_CHANNEL_CALLBACK* callback = (RDPGFX_CHANNEL_CALLBACK*) pChannelCallback;

	free(callback);

	return 0;
}

static int rdpgfx_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
	IWTSVirtualChannel* pChannel, BYTE* Data, int* pbAccept,
	IWTSVirtualChannelCallback** ppCallback)
{
	RDPGFX_CHANNEL_CALLBACK* callback;
	RDPGFX_LISTENER_CALLBACK* listener_callback = (RDPGFX_LISTENER_CALLBACK*) pListenerCallback;

	callback = (RDPGFX_CHANNEL_CALLBACK*) malloc(sizeof(RDPGFX_CHANNEL_CALLBACK));
	ZeroMemory(callback, sizeof(RDPGFX_CHANNEL_CALLBACK));

	callback->iface.OnDataReceived = rdpgfx_on_data_received;
	callback->iface.OnClose = rdpgfx_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;
	listener_callback->channel_callback = callback;

	*ppCallback = (IWTSVirtualChannelCallback*) callback;

	fprintf(stderr, "RdpGfxOnNewChannelConnection\n");

	return 0;
}

static int rdpgfx_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	int status;
	RDPGFX_PLUGIN* rdpgfx = (RDPGFX_PLUGIN*) pPlugin;

	rdpgfx->listener_callback = (RDPGFX_LISTENER_CALLBACK*) malloc(sizeof(RDPGFX_LISTENER_CALLBACK));
	ZeroMemory(rdpgfx->listener_callback, sizeof(RDPGFX_LISTENER_CALLBACK));

	rdpgfx->listener_callback->iface.OnNewChannelConnection = rdpgfx_on_new_channel_connection;
	rdpgfx->listener_callback->plugin = pPlugin;
	rdpgfx->listener_callback->channel_mgr = pChannelMgr;

	status = pChannelMgr->CreateListener(pChannelMgr, RDPGFX_DVC_CHANNEL_NAME, 0,
		(IWTSListenerCallback*) rdpgfx->listener_callback, &(rdpgfx->listener));

	rdpgfx->listener->pInterface = rdpgfx->iface.pInterface;

	fprintf(stderr, "RdpGfxInitialize\n");

	return status;
}

static int rdpgfx_plugin_terminated(IWTSPlugin* pPlugin)
{
	RDPGFX_PLUGIN* rdpgfx = (RDPGFX_PLUGIN*) pPlugin;

	if (rdpgfx->listener_callback)
		free(rdpgfx->listener_callback);

	free(rdpgfx);

	return 0;
}

/**
 * Channel Client Interface
 */

UINT32 rdpgfx_get_version(RdpgfxClientContext* context)
{
	//RDPGFX_PLUGIN* rdpgfx = (RDPGFX_PLUGIN*) context->handle;
	return 0;
}

#ifdef STATIC_CHANNELS
#define DVCPluginEntry		rdpgfx_DVCPluginEntry
#endif

int DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	int error = 0;
	RDPGFX_PLUGIN* rdpgfx;
	RdpgfxClientContext* context;

	rdpgfx = (RDPGFX_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "rdpgfx");

	if (!rdpgfx)
	{
		rdpgfx = (RDPGFX_PLUGIN*) malloc(sizeof(RDPGFX_PLUGIN));
		ZeroMemory(rdpgfx, sizeof(RDPGFX_PLUGIN));

		rdpgfx->iface.Initialize = rdpgfx_plugin_initialize;
		rdpgfx->iface.Connected = NULL;
		rdpgfx->iface.Disconnected = NULL;
		rdpgfx->iface.Terminated = rdpgfx_plugin_terminated;

		context = (RdpgfxClientContext*) malloc(sizeof(RdpgfxClientContext));

		context->handle = (void*) rdpgfx;
		context->GetVersion = rdpgfx_get_version;

		rdpgfx->iface.pInterface = (void*) context;

		fprintf(stderr, "RdpGfxDVCPluginEntry\n");

		error = pEntryPoints->RegisterPlugin(pEntryPoints, "rdpgfx", (IWTSPlugin*) rdpgfx);
	}

	return error;
}
