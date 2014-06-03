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
#include <winpr/print.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/sysinfo.h>
#include <winpr/cmdline.h>
#include <winpr/collections.h>

#include <freerdp/addin.h>
#include <freerdp/codec/zgfx.h>

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

	ZGFX_CONTEXT* zgfx;
};
typedef struct _RDPGFX_PLUGIN RDPGFX_PLUGIN;

int rdpgfx_send_caps_advertise_pdu(RDPGFX_CHANNEL_CALLBACK* callback)
{
	int status;
	wStream* s;
	UINT16 index;
	RDPGFX_PLUGIN* gfx;
	RDPGFX_HEADER header;
	RDPGFX_CAPSET* capsSet;
	RDPGFX_CAPSET* capsSets[2];
	RDPGFX_CAPS_ADVERTISE_PDU pdu;
	RDPGFX_CAPSET_VERSION8 capset8;
	RDPGFX_CAPSET_VERSION81 capset81;

	gfx = (RDPGFX_PLUGIN*) callback->plugin;

	header.cmdId = RDPGFX_CMDID_CAPSADVERTISE;
	header.flags = 0;

	capset8.version = RDPGFX_CAPVERSION_8;
	capset8.capsDataLength = 4;
	capset8.flags = 0;
	capset8.flags |= RDPGFX_CAPS_FLAG_THINCLIENT;
	//capset8.flags |= RDPGFX_CAPS_FLAG_SMALL_CACHE;

	capset81.version = RDPGFX_CAPVERSION_81;
	//capset81.version = 0x00080103;
	capset81.capsDataLength = 4;
	capset81.flags = 0;
	capset81.flags |= RDPGFX_CAPS_FLAG_THINCLIENT;
	//capset81.flags |= RDPGFX_CAPS_FLAG_SMALL_CACHE;
	//capset81.flags |= RDPGFX_CAPS_FLAG_H264ENABLED;

	pdu.capsSetCount = 0;
	pdu.capsSets = (RDPGFX_CAPSET**) capsSets;

	capsSets[pdu.capsSetCount++] = (RDPGFX_CAPSET*) &capset81;
	capsSets[pdu.capsSetCount++] = (RDPGFX_CAPSET*) &capset8;

	header.pduLength = 8 + 2 + (pdu.capsSetCount * 12);

	fprintf(stderr, "RdpGfxSendCapsAdvertisePdu: %d\n", header.pduLength);

	s = Stream_New(NULL, header.pduLength);

	/* RDPGFX_HEADER */

	Stream_Write_UINT16(s, header.cmdId); /* cmdId (2 bytes) */
	Stream_Write_UINT16(s, header.flags); /* flags (2 bytes) */
	Stream_Write_UINT32(s, header.pduLength); /* pduLength (4 bytes) */

	/* RDPGFX_CAPS_ADVERTISE_PDU */

	Stream_Write_UINT16(s, pdu.capsSetCount); /* capsSetCount (2 bytes) */

	for (index = 0; index < pdu.capsSetCount; index++)
	{
		capsSet = pdu.capsSets[index];

		Stream_Write_UINT32(s, capsSet->version); /* version (4 bytes) */
		Stream_Write_UINT32(s, capsSet->capsDataLength); /* capsDataLength (4 bytes) */
		Stream_Write_UINT32(s, capsSet->capsData); /* capsData (4 bytes) */
	}

	Stream_SealLength(s);

	status = callback->channel->Write(callback->channel, (UINT32) Stream_Length(s), Stream_Buffer(s), NULL);

	Stream_Free(s, TRUE);

	return status;
}

int rdpgfx_recv_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_HEADER header;

	/* RDPGFX_HEADER */

	Stream_Read_UINT16(s, header.cmdId); /* cmdId (2 bytes) */
	Stream_Read_UINT16(s, header.flags); /* flags (2 bytes) */
	Stream_Read_UINT32(s, header.pduLength); /* pduLength (4 bytes) */

	printf("cmdId: 0x%04X flags: 0x%04X pduLength: %d\n",
			header.cmdId, header.flags, header.pduLength);

	return 0;
}

static int rdpgfx_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, UINT32 cbSize, BYTE* pBuffer)
{
	wStream* s;
	int status = 0;
	UINT32 DstSize = 0;
	BYTE* pDstData = NULL;
	RDPGFX_PLUGIN* gfx = NULL;
	RDPGFX_CHANNEL_CALLBACK* callback = (RDPGFX_CHANNEL_CALLBACK*) pChannelCallback;

	gfx = (RDPGFX_PLUGIN*) callback->plugin;

	fprintf(stderr, "RdpGfxOnDataReceived: cbSize: %d\n", cbSize);

	status = zgfx_decompress(gfx->zgfx, pBuffer, cbSize, &pDstData, &DstSize, 0);

	if (status < 0)
	{
		printf("zgfx_decompress failure! status: %d\n", status);
		return 0;
	}

	s = Stream_New(pDstData, DstSize);

	status = rdpgfx_recv_pdu(callback, s);

	Stream_Free(s, TRUE);

	return status;
}

static int rdpgfx_on_open(IWTSVirtualChannelCallback* pChannelCallback)
{
	RDPGFX_CHANNEL_CALLBACK* callback = (RDPGFX_CHANNEL_CALLBACK*) pChannelCallback;

	fprintf(stderr, "RdpGfxOnOpen\n");

	rdpgfx_send_caps_advertise_pdu(callback);

	return 0;
}

static int rdpgfx_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	RDPGFX_CHANNEL_CALLBACK* callback = (RDPGFX_CHANNEL_CALLBACK*) pChannelCallback;

	fprintf(stderr, "RdpGfxOnClose\n");

	free(callback);

	return 0;
}

static int rdpgfx_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
	IWTSVirtualChannel* pChannel, BYTE* Data, int* pbAccept,
	IWTSVirtualChannelCallback** ppCallback)
{
	RDPGFX_CHANNEL_CALLBACK* callback;
	RDPGFX_LISTENER_CALLBACK* listener_callback = (RDPGFX_LISTENER_CALLBACK*) pListenerCallback;

	callback = (RDPGFX_CHANNEL_CALLBACK*) calloc(1, sizeof(RDPGFX_CHANNEL_CALLBACK));

	callback->iface.OnDataReceived = rdpgfx_on_data_received;
	callback->iface.OnOpen = rdpgfx_on_open;
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

	rdpgfx->listener_callback = (RDPGFX_LISTENER_CALLBACK*) calloc(1, sizeof(RDPGFX_LISTENER_CALLBACK));

	if (!rdpgfx->listener_callback)
		return -1;

	rdpgfx->listener_callback->iface.OnNewChannelConnection = rdpgfx_on_new_channel_connection;
	rdpgfx->listener_callback->plugin = pPlugin;
	rdpgfx->listener_callback->channel_mgr = pChannelMgr;

	status = pChannelMgr->CreateListener(pChannelMgr, RDPGFX_DVC_CHANNEL_NAME, 0,
		(IWTSListenerCallback*) rdpgfx->listener_callback, &(rdpgfx->listener));

	rdpgfx->listener->pInterface = rdpgfx->iface.pInterface;

	fprintf(stderr, "RdpGfxInitialize: %d\n", status);

	return status;
}

static int rdpgfx_plugin_terminated(IWTSPlugin* pPlugin)
{
	RDPGFX_PLUGIN* rdpgfx = (RDPGFX_PLUGIN*) pPlugin;

	if (rdpgfx->listener_callback)
		free(rdpgfx->listener_callback);

	zgfx_context_free(rdpgfx->zgfx);

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
		rdpgfx = (RDPGFX_PLUGIN*) calloc(1, sizeof(RDPGFX_PLUGIN));

		if (!rdpgfx)
			return -1;

		rdpgfx->iface.Initialize = rdpgfx_plugin_initialize;
		rdpgfx->iface.Connected = NULL;
		rdpgfx->iface.Disconnected = NULL;
		rdpgfx->iface.Terminated = rdpgfx_plugin_terminated;

		context = (RdpgfxClientContext*) calloc(1, sizeof(RdpgfxClientContext));

		if (!context)
			return -1;

		context->handle = (void*) rdpgfx;
		context->GetVersion = rdpgfx_get_version;

		rdpgfx->iface.pInterface = (void*) context;

		rdpgfx->zgfx = zgfx_context_new(FALSE);

		error = pEntryPoints->RegisterPlugin(pEntryPoints, "rdpgfx", (IWTSPlugin*) rdpgfx);
	}

	return error;
}
