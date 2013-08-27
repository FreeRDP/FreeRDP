/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Display Update Virtual Channel Extension
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
#include <winpr/synch.h>
#include <winpr/print.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/sysinfo.h>
#include <winpr/cmdline.h>
#include <winpr/collections.h>

#include <freerdp/addin.h>

#include "disp_main.h"

struct _DISP_CHANNEL_CALLBACK
{
	IWTSVirtualChannelCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	IWTSVirtualChannel* channel;
};
typedef struct _DISP_CHANNEL_CALLBACK DISP_CHANNEL_CALLBACK;

struct _DISP_LISTENER_CALLBACK
{
	IWTSListenerCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	DISP_CHANNEL_CALLBACK* channel_callback;
};
typedef struct _DISP_LISTENER_CALLBACK DISP_LISTENER_CALLBACK;

struct _DISP_PLUGIN
{
	IWTSPlugin iface;

	IWTSListener* listener;
	DISP_LISTENER_CALLBACK* listener_callback;

	UINT32 MaxNumMonitors;
	UINT32 MaxMonitorWidth;
	UINT32 MaxMonitorHeight;
};
typedef struct _DISP_PLUGIN DISP_PLUGIN;

int disp_send_display_control_monitor_layout_pdu(DISP_CHANNEL_CALLBACK* callback, UINT32 NumMonitors, DISPLAY_CONTROL_MONITOR_LAYOUT* Monitors)
{
	int index;
	int status;
	wStream* s;
	UINT32 type;
	UINT32 length;
	DISP_PLUGIN* disp;
	UINT32 MonitorLayoutSize;

	disp = (DISP_PLUGIN*) callback->plugin;

#ifdef DISP_PREVIEW
	MonitorLayoutSize = 32;
#else
	MonitorLayoutSize = 40;
#endif

	length = 8 + 8 + (NumMonitors * MonitorLayoutSize);

	type = DISPLAY_CONTROL_PDU_TYPE_MONITOR_LAYOUT;

	s = Stream_New(NULL, length);

	Stream_Write_UINT32(s, type); /* Type (4 bytes) */
	Stream_Write_UINT32(s, length); /* Length (4 bytes) */

	if (NumMonitors > disp->MaxNumMonitors)
		NumMonitors = disp->MaxNumMonitors;

#ifdef DISP_PREVIEW
	Stream_Write_UINT32(s, NumMonitors); /* NumMonitors (4 bytes) */
#else
	Stream_Write_UINT32(s, MonitorLayoutSize); /* MonitorLayoutSize (4 bytes) */
#endif

	Stream_Write_UINT32(s, NumMonitors); /* NumMonitors (4 bytes) */

	//fprintf(stderr, "NumMonitors: %d\n", NumMonitors);

	for (index = 0; index < NumMonitors; index++)
	{
		Monitors[index].Width -= (Monitors[index].Width % 2);

		if (Monitors[index].Width < 200)
			Monitors[index].Width = 200;

		if (Monitors[index].Width > disp->MaxMonitorWidth)
			Monitors[index].Width = disp->MaxMonitorWidth;

		if (Monitors[index].Height < 200)
			Monitors[index].Height = 200;

		if (Monitors[index].Height > disp->MaxMonitorHeight)
			Monitors[index].Height = disp->MaxMonitorHeight;

		Stream_Write_UINT32(s, Monitors[index].Flags); /* Flags (4 bytes) */
		Stream_Write_UINT32(s, Monitors[index].Left); /* Left (4 bytes) */
		Stream_Write_UINT32(s, Monitors[index].Top); /* Top (4 bytes) */
		Stream_Write_UINT32(s, Monitors[index].Width); /* Width (4 bytes) */
		Stream_Write_UINT32(s, Monitors[index].Height); /* Height (4 bytes) */
		Stream_Write_UINT32(s, Monitors[index].PhysicalWidth); /* PhysicalWidth (4 bytes) */
		Stream_Write_UINT32(s, Monitors[index].PhysicalHeight); /* PhysicalHeight (4 bytes) */
		Stream_Write_UINT32(s, Monitors[index].Orientation); /* Orientation (4 bytes) */

#if 0
		fprintf(stderr, "\t: Flags: 0x%04X\n", Monitors[index].Flags);
		fprintf(stderr, "\t: Left: %d\n", Monitors[index].Left);
		fprintf(stderr, "\t: Top: %d\n", Monitors[index].Top);
		fprintf(stderr, "\t: Width: %d\n", Monitors[index].Width);
		fprintf(stderr, "\t: Height: %d\n", Monitors[index].Height);
		fprintf(stderr, "\t: PhysicalWidth: %d\n", Monitors[index].PhysicalWidth);
		fprintf(stderr, "\t: PhysicalHeight: %d\n", Monitors[index].PhysicalHeight);
		fprintf(stderr, "\t: Orientation: %d\n", Monitors[index].Orientation);
#endif

#ifndef DISP_PREVIEW
		Stream_Write_UINT32(s, Monitors[index].DesktopScaleFactor); /* DesktopScaleFactor (4 bytes) */
		Stream_Write_UINT32(s, Monitors[index].DeviceScaleFactor); /* DeviceScaleFactor (4 bytes) */
#endif
	}

	Stream_SealLength(s);

	status = callback->channel->Write(callback->channel, Stream_Length(s), Stream_Buffer(s), NULL);

	Stream_Free(s, TRUE);

	return status;
}

int disp_recv_display_control_caps_pdu(DISP_CHANNEL_CALLBACK* callback, wStream* s)
{
	DISP_PLUGIN* disp;

	disp = (DISP_PLUGIN*) callback->plugin;

	Stream_Read_UINT32(s, disp->MaxNumMonitors); /* MaxNumMonitors (4 bytes) */
	Stream_Read_UINT32(s, disp->MaxMonitorWidth); /* MaxMonitorWidth (4 bytes) */
	Stream_Read_UINT32(s, disp->MaxMonitorHeight); /* MaxMonitorHeight (4 bytes) */

	//fprintf(stderr, "DisplayControlCapsPdu: MaxNumMonitors: %d MaxMonitorWidth: %d MaxMonitorHeight: %d\n",
	//       disp->MaxNumMonitors, disp->MaxMonitorWidth, disp->MaxMonitorHeight);

	return 0;
}

int disp_recv_pdu(DISP_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT32 type;
	UINT32 length;

	Stream_Read_UINT32(s, type); /* Type (4 bytes) */
	Stream_Read_UINT32(s, length); /* Length (4 bytes) */

	//fprintf(stderr, "Type: %d Length: %d\n", type, length);

	switch (type)
	{
		case DISPLAY_CONTROL_PDU_TYPE_CAPS:
			disp_recv_display_control_caps_pdu(callback, s);
			break;

		default:
			break;
	}

	return 0;
}

static int disp_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, UINT32 cbSize, BYTE* pBuffer)
{
	wStream* s;
	int status = 0;
	DISP_CHANNEL_CALLBACK* callback = (DISP_CHANNEL_CALLBACK*) pChannelCallback;

	s = Stream_New(pBuffer, cbSize);

	status = disp_recv_pdu(callback, s);

	Stream_Free(s, FALSE);

	return status;
}

static int disp_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	DISP_CHANNEL_CALLBACK* callback = (DISP_CHANNEL_CALLBACK*) pChannelCallback;

	if (callback)
	{
		free(callback);
	}

	return 0;
}

static int disp_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
	IWTSVirtualChannel* pChannel, BYTE* Data, int* pbAccept,
	IWTSVirtualChannelCallback** ppCallback)
{
	DISP_CHANNEL_CALLBACK* callback;
	DISP_LISTENER_CALLBACK* listener_callback = (DISP_LISTENER_CALLBACK*) pListenerCallback;

	callback = (DISP_CHANNEL_CALLBACK*) malloc(sizeof(DISP_CHANNEL_CALLBACK));
	ZeroMemory(callback, sizeof(DISP_CHANNEL_CALLBACK));

	callback->iface.OnDataReceived = disp_on_data_received;
	callback->iface.OnClose = disp_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;
	listener_callback->channel_callback = callback;

	*ppCallback = (IWTSVirtualChannelCallback*) callback;

	return 0;
}

static int disp_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	int status;
	DISP_PLUGIN* disp = (DISP_PLUGIN*) pPlugin;

	disp->listener_callback = (DISP_LISTENER_CALLBACK*) malloc(sizeof(DISP_LISTENER_CALLBACK));
	ZeroMemory(disp->listener_callback, sizeof(DISP_LISTENER_CALLBACK));

	disp->listener_callback->iface.OnNewChannelConnection = disp_on_new_channel_connection;
	disp->listener_callback->plugin = pPlugin;
	disp->listener_callback->channel_mgr = pChannelMgr;

	status = pChannelMgr->CreateListener(pChannelMgr, DISP_DVC_CHANNEL_NAME, 0,
		(IWTSListenerCallback*) disp->listener_callback, &(disp->listener));

	disp->listener->pInterface = disp->iface.pInterface;

	return status;
}

static int disp_plugin_terminated(IWTSPlugin* pPlugin)
{
	DISP_PLUGIN* disp = (DISP_PLUGIN*) pPlugin;

	if (disp)
	{
		free(disp);
	}

	return 0;
}

/**
 * Channel Client Interface
 */

int disp_send_monitor_layout(DispClientContext* context, UINT32 NumMonitors, DISPLAY_CONTROL_MONITOR_LAYOUT* Monitors)
{
	DISP_PLUGIN* disp = (DISP_PLUGIN*) context->handle;
	DISP_CHANNEL_CALLBACK* callback = disp->listener_callback->channel_callback;

	disp_send_display_control_monitor_layout_pdu(callback, NumMonitors, Monitors);

	return 1;
}

#ifdef STATIC_CHANNELS
#define DVCPluginEntry		disp_DVCPluginEntry
#endif

int DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	int error = 0;
	DISP_PLUGIN* disp;
	DispClientContext* context;

	disp = (DISP_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "disp");

	if (disp == NULL)
	{
		disp = (DISP_PLUGIN*) malloc(sizeof(DISP_PLUGIN));

		if (disp)
		{
			ZeroMemory(disp, sizeof(DISP_PLUGIN));

			disp->iface.Initialize = disp_plugin_initialize;
			disp->iface.Connected = NULL;
			disp->iface.Disconnected = NULL;
			disp->iface.Terminated = disp_plugin_terminated;

			context = (DispClientContext*) malloc(sizeof(DispClientContext));

			context->handle = (void*) disp;

			context->SendMonitorLayout = disp_send_monitor_layout;

			disp->iface.pInterface = (void*) context;

			disp->MaxNumMonitors = 16;
			disp->MaxMonitorWidth = 8192;
			disp->MaxMonitorHeight = 8192;

			error = pEntryPoints->RegisterPlugin(pEntryPoints, "disp", (IWTSPlugin*) disp);
		}
	}

	return error;
}
