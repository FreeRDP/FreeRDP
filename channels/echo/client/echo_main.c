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

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>

#include <winpr/crt.h>
#include <winpr/stream.h>

#include "echo_main.h"
#include <freerdp/channels/log.h>
#include <freerdp/channels/echo.h>

#define TAG CHANNELS_TAG("echo.client")

typedef struct
{
	IWTSListenerCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
} ECHO_LISTENER_CALLBACK;

typedef struct
{
	IWTSVirtualChannelCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	IWTSVirtualChannel* channel;
} ECHO_CHANNEL_CALLBACK;

typedef struct
{
	IWTSPlugin iface;

	ECHO_LISTENER_CALLBACK* listener_callback;
	IWTSListener* listener;
	BOOL initialized;
} ECHO_PLUGIN;

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT echo_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* data)
{
	ECHO_CHANNEL_CALLBACK* callback = (ECHO_CHANNEL_CALLBACK*)pChannelCallback;
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
	ECHO_CHANNEL_CALLBACK* callback = (ECHO_CHANNEL_CALLBACK*)pChannelCallback;

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
	ECHO_LISTENER_CALLBACK* listener_callback = (ECHO_LISTENER_CALLBACK*)pListenerCallback;

	callback = (ECHO_CHANNEL_CALLBACK*)calloc(1, sizeof(ECHO_CHANNEL_CALLBACK));

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

	*ppCallback = (IWTSVirtualChannelCallback*)callback;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT echo_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	UINT status;
	ECHO_PLUGIN* echo = (ECHO_PLUGIN*)pPlugin;
	if (echo->initialized)
	{
		WLog_ERR(TAG, "[%s] channel initialized twice, aborting", ECHO_DVC_CHANNEL_NAME);
		return ERROR_INVALID_DATA;
	}
	echo->listener_callback = (ECHO_LISTENER_CALLBACK*)calloc(1, sizeof(ECHO_LISTENER_CALLBACK));

	if (!echo->listener_callback)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	echo->listener_callback->iface.OnNewChannelConnection = echo_on_new_channel_connection;
	echo->listener_callback->plugin = pPlugin;
	echo->listener_callback->channel_mgr = pChannelMgr;

	status = pChannelMgr->CreateListener(pChannelMgr, ECHO_DVC_CHANNEL_NAME, 0,
	                                     &echo->listener_callback->iface, &echo->listener);

	echo->initialized = status == CHANNEL_RC_OK;
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT echo_plugin_terminated(IWTSPlugin* pPlugin)
{
	ECHO_PLUGIN* echo = (ECHO_PLUGIN*)pPlugin;
	if (echo && echo->listener_callback)
	{
		IWTSVirtualChannelManager* mgr = echo->listener_callback->channel_mgr;
		if (mgr)
			IFCALL(mgr->DestroyListener, mgr, echo->listener);
	}
	free(echo);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT echo_DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	UINT status = CHANNEL_RC_OK;
	ECHO_PLUGIN* echo;

	echo = (ECHO_PLUGIN*)pEntryPoints->GetPlugin(pEntryPoints, "echo");

	if (!echo)
	{
		echo = (ECHO_PLUGIN*)calloc(1, sizeof(ECHO_PLUGIN));

		if (!echo)
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		echo->iface.Initialize = echo_plugin_initialize;
		echo->iface.Connected = NULL;
		echo->iface.Disconnected = NULL;
		echo->iface.Terminated = echo_plugin_terminated;

		status = pEntryPoints->RegisterPlugin(pEntryPoints, "echo", &echo->iface);
	}

	return status;
}
