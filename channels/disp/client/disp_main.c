/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Display Update Virtual Channel Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2016 David PHAM-VAN <d.phamvan@inuvika.com>
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

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/synch.h>
#include <winpr/print.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/sysinfo.h>
#include <winpr/cmdline.h>
#include <winpr/collections.h>

#include <freerdp/addin.h>
#include <freerdp/client/channels.h>

#include "disp_main.h"
#include "../disp_common.h"

typedef struct
{
	IWTSPlugin iface;

	IWTSListener* listener;
	GENERIC_LISTENER_CALLBACK* listener_callback;

	UINT32 MaxNumMonitors;
	UINT32 MaxMonitorAreaFactorA;
	UINT32 MaxMonitorAreaFactorB;
	BOOL initialized;
} DISP_PLUGIN;

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
disp_send_display_control_monitor_layout_pdu(GENERIC_CHANNEL_CALLBACK* callback, UINT32 NumMonitors,
                                             const DISPLAY_CONTROL_MONITOR_LAYOUT* Monitors)
{
	UINT status;
	wStream* s;
	UINT32 index;
	DISP_PLUGIN* disp;
	UINT32 MonitorLayoutSize;
	DISPLAY_CONTROL_HEADER header = { 0 };

	WINPR_ASSERT(callback);
	WINPR_ASSERT(Monitors || (NumMonitors == 0));

	disp = (DISP_PLUGIN*)callback->plugin;
	WINPR_ASSERT(disp);

	MonitorLayoutSize = DISPLAY_CONTROL_MONITOR_LAYOUT_SIZE;
	header.length = 8 + 8 + (NumMonitors * MonitorLayoutSize);
	header.type = DISPLAY_CONTROL_PDU_TYPE_MONITOR_LAYOUT;

	s = Stream_New(NULL, header.length);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if ((status = disp_write_header(s, &header)))
	{
		WLog_ERR(TAG, "Failed to write header with error %" PRIu32 "!", status);
		goto out;
	}

	if (NumMonitors > disp->MaxNumMonitors)
		NumMonitors = disp->MaxNumMonitors;

	Stream_Write_UINT32(s, MonitorLayoutSize); /* MonitorLayoutSize (4 bytes) */
	Stream_Write_UINT32(s, NumMonitors);       /* NumMonitors (4 bytes) */
	WLog_DBG(TAG, "%s: NumMonitors=%" PRIu32 "", __FUNCTION__, NumMonitors);

	for (index = 0; index < NumMonitors; index++)
	{
		DISPLAY_CONTROL_MONITOR_LAYOUT current = Monitors[index];
		current.Width -= (current.Width % 2);

		if (current.Width < 200)
			current.Width = 200;

		if (current.Width > 8192)
			current.Width = 8192;

		if (current.Width % 2)
			current.Width++;

		if (current.Height < 200)
			current.Height = 200;

		if (current.Height > 8192)
			current.Height = 8192;

		Stream_Write_UINT32(s, current.Flags);              /* Flags (4 bytes) */
		Stream_Write_UINT32(s, current.Left);               /* Left (4 bytes) */
		Stream_Write_UINT32(s, current.Top);                /* Top (4 bytes) */
		Stream_Write_UINT32(s, current.Width);              /* Width (4 bytes) */
		Stream_Write_UINT32(s, current.Height);             /* Height (4 bytes) */
		Stream_Write_UINT32(s, current.PhysicalWidth);      /* PhysicalWidth (4 bytes) */
		Stream_Write_UINT32(s, current.PhysicalHeight);     /* PhysicalHeight (4 bytes) */
		Stream_Write_UINT32(s, current.Orientation);        /* Orientation (4 bytes) */
		Stream_Write_UINT32(s, current.DesktopScaleFactor); /* DesktopScaleFactor (4 bytes) */
		Stream_Write_UINT32(s, current.DeviceScaleFactor);  /* DeviceScaleFactor (4 bytes) */
		WLog_DBG(TAG,
		         "\t%d : Flags: 0x%08" PRIX32 " Left/Top: (%" PRId32 ",%" PRId32 ") W/H=%" PRIu32
		         "x%" PRIu32 ")",
		         index, current.Flags, current.Left, current.Top, current.Width, current.Height);
		WLog_DBG(TAG,
		         "\t   PhysicalWidth: %" PRIu32 " PhysicalHeight: %" PRIu32 " Orientation: %" PRIu32
		         "",
		         current.PhysicalWidth, current.PhysicalHeight, current.Orientation);
	}

out:
	Stream_SealLength(s);
	status = callback->channel->Write(callback->channel, (UINT32)Stream_Length(s), Stream_Buffer(s),
	                                  NULL);
	Stream_Free(s, TRUE);
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT disp_recv_display_control_caps_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	DISP_PLUGIN* disp;
	DispClientContext* context;
	UINT ret = CHANNEL_RC_OK;

	WINPR_ASSERT(callback);
	WINPR_ASSERT(s);

	disp = (DISP_PLUGIN*)callback->plugin;
	WINPR_ASSERT(disp);

	context = (DispClientContext*)disp->iface.pInterface;
	WINPR_ASSERT(context);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, disp->MaxNumMonitors);        /* MaxNumMonitors (4 bytes) */
	Stream_Read_UINT32(s, disp->MaxMonitorAreaFactorA); /* MaxMonitorAreaFactorA (4 bytes) */
	Stream_Read_UINT32(s, disp->MaxMonitorAreaFactorB); /* MaxMonitorAreaFactorB (4 bytes) */

	if (context->DisplayControlCaps)
		ret = context->DisplayControlCaps(context, disp->MaxNumMonitors,
		                                  disp->MaxMonitorAreaFactorA, disp->MaxMonitorAreaFactorB);

	return ret;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT disp_recv_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT32 error;
	DISPLAY_CONTROL_HEADER header = { 0 };

	WINPR_ASSERT(callback);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	if ((error = disp_read_header(s, &header)))
	{
		WLog_ERR(TAG, "disp_read_header failed with error %" PRIu32 "!", error);
		return error;
	}

	if (!Stream_EnsureRemainingCapacity(s, header.length))
	{
		WLog_ERR(TAG, "not enough remaining data");
		return ERROR_INVALID_DATA;
	}

	switch (header.type)
	{
		case DISPLAY_CONTROL_PDU_TYPE_CAPS:
			return disp_recv_display_control_caps_pdu(callback, s);

		default:
			WLog_ERR(TAG, "Type %" PRIu32 " not recognized!", header.type);
			return ERROR_INTERNAL_ERROR;
	}
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT disp_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* data)
{
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
	return disp_recv_pdu(callback, data);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT disp_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	free(pChannelCallback);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT disp_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
                                           IWTSVirtualChannel* pChannel, BYTE* Data, BOOL* pbAccept,
                                           IWTSVirtualChannelCallback** ppCallback)
{
	GENERIC_CHANNEL_CALLBACK* callback;
	GENERIC_LISTENER_CALLBACK* listener_callback = (GENERIC_LISTENER_CALLBACK*)pListenerCallback;

	WINPR_ASSERT(listener_callback);
	WINPR_ASSERT(pChannel);
	WINPR_ASSERT(pbAccept);
	WINPR_ASSERT(ppCallback);

	callback = (GENERIC_CHANNEL_CALLBACK*)calloc(1, sizeof(GENERIC_CHANNEL_CALLBACK));

	if (!callback)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	callback->iface.OnDataReceived = disp_on_data_received;
	callback->iface.OnClose = disp_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;
	listener_callback->channel_callback = callback;
	*ppCallback = (IWTSVirtualChannelCallback*)callback;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT disp_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	UINT status;
	DISP_PLUGIN* disp = (DISP_PLUGIN*)pPlugin;

	WINPR_ASSERT(disp);
	WINPR_ASSERT(pChannelMgr);

	if (disp->initialized)
	{
		WLog_ERR(TAG, "[%s] channel initialized twice, aborting", DISP_DVC_CHANNEL_NAME);
		return ERROR_INVALID_DATA;
	}
	disp->listener_callback =
	    (GENERIC_LISTENER_CALLBACK*)calloc(1, sizeof(GENERIC_LISTENER_CALLBACK));

	if (!disp->listener_callback)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	disp->listener_callback->iface.OnNewChannelConnection = disp_on_new_channel_connection;
	disp->listener_callback->plugin = pPlugin;
	disp->listener_callback->channel_mgr = pChannelMgr;
	status = pChannelMgr->CreateListener(pChannelMgr, DISP_DVC_CHANNEL_NAME, 0,
	                                     &disp->listener_callback->iface, &(disp->listener));
	disp->listener->pInterface = disp->iface.pInterface;

	disp->initialized = status == CHANNEL_RC_OK;
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT disp_plugin_terminated(IWTSPlugin* pPlugin)
{
	DISP_PLUGIN* disp = (DISP_PLUGIN*)pPlugin;

	WINPR_ASSERT(disp);

	if (disp->listener_callback)
	{
		IWTSVirtualChannelManager* mgr = disp->listener_callback->channel_mgr;
		if (mgr)
			IFCALL(mgr->DestroyListener, mgr, disp->listener);
		free(disp->listener_callback);
	}

	free(disp->iface.pInterface);
	free(pPlugin);
	return CHANNEL_RC_OK;
}

/**
 * Channel Client Interface
 */

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT disp_send_monitor_layout(DispClientContext* context, UINT32 NumMonitors,
                                     DISPLAY_CONTROL_MONITOR_LAYOUT* Monitors)
{
	DISP_PLUGIN* disp;
	GENERIC_CHANNEL_CALLBACK* callback;

	WINPR_ASSERT(context);

	disp = (DISP_PLUGIN*)context->handle;
	WINPR_ASSERT(disp);

	callback = disp->listener_callback->channel_callback;

	return disp_send_display_control_monitor_layout_pdu(callback, NumMonitors, Monitors);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT disp_DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	UINT error = CHANNEL_RC_OK;
	DISP_PLUGIN* disp;
	DispClientContext* context;

	WINPR_ASSERT(pEntryPoints);

	disp = (DISP_PLUGIN*)pEntryPoints->GetPlugin(pEntryPoints, DISP_CHANNEL_NAME);

	if (!disp)
	{
		disp = (DISP_PLUGIN*)calloc(1, sizeof(DISP_PLUGIN));

		if (!disp)
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		disp->iface.Initialize = disp_plugin_initialize;
		disp->iface.Connected = NULL;
		disp->iface.Disconnected = NULL;
		disp->iface.Terminated = disp_plugin_terminated;
		disp->MaxNumMonitors = 16;
		disp->MaxMonitorAreaFactorA = 8192;
		disp->MaxMonitorAreaFactorB = 8192;
		context = (DispClientContext*)calloc(1, sizeof(DispClientContext));

		if (!context)
		{
			WLog_ERR(TAG, "calloc failed!");
			free(disp);
			return CHANNEL_RC_NO_MEMORY;
		}

		context->handle = (void*)disp;
		context->SendMonitorLayout = disp_send_monitor_layout;
		disp->iface.pInterface = (void*)context;
		error = pEntryPoints->RegisterPlugin(pEntryPoints, DISP_CHANNEL_NAME, &disp->iface);
	}
	else
	{
		WLog_ERR(TAG, "could not get disp Plugin.");
		return CHANNEL_RC_BAD_CHANNEL;
	}

	return error;
}
