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
	UINT32 MaxMonitorAreaFactorA;
	UINT32 MaxMonitorAreaFactorB;
};
typedef struct _DISP_PLUGIN DISP_PLUGIN;

int disp_send_display_control_monitor_layout_pdu(DISP_CHANNEL_CALLBACK* callback, UINT32 NumMonitors, DISPLAY_CONTROL_MONITOR_LAYOUT* Monitors)
{
	int status;
	wStream* s;
	UINT32 type;
	UINT32 index;
	UINT32 length;
	DISP_PLUGIN* disp;
	UINT32 MonitorLayoutSize;

	disp = (DISP_PLUGIN*) callback->plugin;

	MonitorLayoutSize = 40;

	length = 8 + 8 + (NumMonitors * MonitorLayoutSize);

	type = DISPLAY_CONTROL_PDU_TYPE_MONITOR_LAYOUT;

	s = Stream_New(NULL, length);

	Stream_Write_UINT32(s, type); /* Type (4 bytes) */
	Stream_Write_UINT32(s, length); /* Length (4 bytes) */

	if (NumMonitors > disp->MaxNumMonitors)
		NumMonitors = disp->MaxNumMonitors;

	Stream_Write_UINT32(s, MonitorLayoutSize); /* MonitorLayoutSize (4 bytes) */

	Stream_Write_UINT32(s, NumMonitors); /* NumMonitors (4 bytes) */

	//WLog_ERR(TAG,  "NumMonitors: %d", NumMonitors);

	for (index = 0; index < NumMonitors; index++)
	{
		Monitors[index].Width -= (Monitors[index].Width % 2);

		if (Monitors[index].Width < 200)
			Monitors[index].Width = 200;

		if (Monitors[index].Width > 8192)
			Monitors[index].Width = 8192;

		if (Monitors[index].Width % 2)
			Monitors[index].Width++;

		if (Monitors[index].Height < 200)
			Monitors[index].Height = 200;

		if (Monitors[index].Height > 8192)
			Monitors[index].Height = 8192;

		Stream_Write_UINT32(s, Monitors[index].Flags); /* Flags (4 bytes) */
		Stream_Write_UINT32(s, Monitors[index].Left); /* Left (4 bytes) */
		Stream_Write_UINT32(s, Monitors[index].Top); /* Top (4 bytes) */
		Stream_Write_UINT32(s, Monitors[index].Width); /* Width (4 bytes) */
		Stream_Write_UINT32(s, Monitors[index].Height); /* Height (4 bytes) */
		Stream_Write_UINT32(s, Monitors[index].PhysicalWidth); /* PhysicalWidth (4 bytes) */
		Stream_Write_UINT32(s, Monitors[index].PhysicalHeight); /* PhysicalHeight (4 bytes) */
		Stream_Write_UINT32(s, Monitors[index].Orientation); /* Orientation (4 bytes) */
		Stream_Write_UINT32(s, Monitors[index].DesktopScaleFactor); /* DesktopScaleFactor (4 bytes) */
		Stream_Write_UINT32(s, Monitors[index].DeviceScaleFactor); /* DeviceScaleFactor (4 bytes) */

#if 0
		WLog_DBG(TAG,  "\t: Flags: 0x%04X", Monitors[index].Flags);
		WLog_DBG(TAG,  "\t: Left: %d", Monitors[index].Left);
		WLog_DBG(TAG,  "\t: Top: %d", Monitors[index].Top);
		WLog_DBG(TAG,  "\t: Width: %d", Monitors[index].Width);
		WLog_DBG(TAG,  "\t: Height: %d", Monitors[index].Height);
		WLog_DBG(TAG,  "\t: PhysicalWidth: %d", Monitors[index].PhysicalWidth);
		WLog_DBG(TAG,  "\t: PhysicalHeight: %d", Monitors[index].PhysicalHeight);
		WLog_DBG(TAG,  "\t: Orientation: %d", Monitors[index].Orientation);
#endif
	}

	Stream_SealLength(s);

	status = callback->channel->Write(callback->channel, (UINT32) Stream_Length(s), Stream_Buffer(s), NULL);

	Stream_Free(s, TRUE);

	return status;
}

int disp_recv_display_control_caps_pdu(DISP_CHANNEL_CALLBACK* callback, wStream* s)
{
	DISP_PLUGIN* disp;

	disp = (DISP_PLUGIN*) callback->plugin;

	if (Stream_GetRemainingLength(s) < 12)
		return -1;

	Stream_Read_UINT32(s, disp->MaxNumMonitors); /* MaxNumMonitors (4 bytes) */
	Stream_Read_UINT32(s, disp->MaxMonitorAreaFactorA); /* MaxMonitorAreaFactorA (4 bytes) */
	Stream_Read_UINT32(s, disp->MaxMonitorAreaFactorB); /* MaxMonitorAreaFactorB (4 bytes) */
	//WLog_ERR(TAG,  "DisplayControlCapsPdu: MaxNumMonitors: %d MaxMonitorWidth: %d MaxMonitorHeight: %d",
	//       disp->MaxNumMonitors, disp->MaxMonitorWidth, disp->MaxMonitorHeight);

	return 0;
}

int disp_recv_pdu(DISP_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT32 type;
	UINT32 length;

	if (Stream_GetRemainingLength(s) < 8)
		return -1;

	Stream_Read_UINT32(s, type); /* Type (4 bytes) */
	Stream_Read_UINT32(s, length); /* Length (4 bytes) */

	//WLog_ERR(TAG,  "Type: %d Length: %d", type, length);

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

static int disp_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream *data)
{
	int status = 0;
	DISP_CHANNEL_CALLBACK* callback = (DISP_CHANNEL_CALLBACK*) pChannelCallback;

	status = disp_recv_pdu(callback, data);

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

	callback = (DISP_CHANNEL_CALLBACK*) calloc(1, sizeof(DISP_CHANNEL_CALLBACK));

	if (!callback)
		return -1;

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

	disp->listener_callback = (DISP_LISTENER_CALLBACK*) calloc(1, sizeof(DISP_LISTENER_CALLBACK));

	if (!disp->listener_callback)
		return -1;

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

	if (!disp)
	{
		disp = (DISP_PLUGIN*) calloc(1, sizeof(DISP_PLUGIN));

		if (!disp)
			return -1;

		disp->iface.Initialize = disp_plugin_initialize;
		disp->iface.Connected = NULL;
		disp->iface.Disconnected = NULL;
		disp->iface.Terminated = disp_plugin_terminated;

		context = (DispClientContext*) calloc(1, sizeof(DispClientContext));

		if (!context)
		{
			free(disp);
			return -1;
		}

		context->handle = (void*) disp;

		context->SendMonitorLayout = disp_send_monitor_layout;

		disp->iface.pInterface = (void*) context;

		disp->MaxNumMonitors = 16;
		disp->MaxMonitorAreaFactorA = 8192;
		disp->MaxMonitorAreaFactorB = 8192;

		error = pEntryPoints->RegisterPlugin(pEntryPoints, "disp", (IWTSPlugin*) disp);
	}

	return error;
}
