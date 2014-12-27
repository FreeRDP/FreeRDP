/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Dynamic Virtual Channel Manager
 *
 * Copyright 2010-2011 Vic Lee
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
#include <winpr/stream.h>

#include <freerdp/addin.h>

#include "drdynvc_types.h"
#include "dvcman.h"

static int dvcman_get_configuration(IWTSListener* pListener, void** ppPropertyBag)
{
	*ppPropertyBag = NULL;
	return 1;
}

static int dvcman_create_listener(IWTSVirtualChannelManager* pChannelMgr,
	const char* pszChannelName, UINT32 ulFlags,
	IWTSListenerCallback* pListenerCallback, IWTSListener** ppListener)
{
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;
	DVCMAN_LISTENER* listener;

	if (dvcman->num_listeners < MAX_PLUGINS)
	{
		DEBUG_DVC("%d.%s.", dvcman->num_listeners, pszChannelName);

		listener = (DVCMAN_LISTENER*) malloc(sizeof(DVCMAN_LISTENER));
		ZeroMemory(listener, sizeof(DVCMAN_LISTENER));

		listener->iface.GetConfiguration = dvcman_get_configuration;
		listener->iface.pInterface = NULL;

		listener->dvcman = dvcman;
		listener->channel_name = _strdup(pszChannelName);
		listener->flags = ulFlags;
		listener->listener_callback = pListenerCallback;

		if (ppListener)
			*ppListener = (IWTSListener*) listener;

		dvcman->listeners[dvcman->num_listeners++] = (IWTSListener*) listener;
		
		return 0;
	}
	else
	{
		WLog_WARN(TAG, "Maximum DVC listener number reached.");
		return 1;
	}
}

static int dvcman_register_plugin(IDRDYNVC_ENTRY_POINTS* pEntryPoints, const char* name, IWTSPlugin* pPlugin)
{
	DVCMAN* dvcman = ((DVCMAN_ENTRY_POINTS*) pEntryPoints)->dvcman;

	if (dvcman->num_plugins < MAX_PLUGINS)
	{
		DEBUG_DVC("num_plugins %d", dvcman->num_plugins);
		dvcman->plugin_names[dvcman->num_plugins] = name;
		dvcman->plugins[dvcman->num_plugins++] = pPlugin;
		return 0;
	}
	else
	{
		WLog_WARN(TAG, "Maximum DVC plugin number reached.");
		return 1;
	}
}

IWTSPlugin* dvcman_get_plugin(IDRDYNVC_ENTRY_POINTS* pEntryPoints, const char* name)
{
	int i;
	DVCMAN* dvcman = ((DVCMAN_ENTRY_POINTS*) pEntryPoints)->dvcman;

	for (i = 0; i < dvcman->num_plugins; i++)
	{
		if (dvcman->plugin_names[i] == name ||
			strcmp(dvcman->plugin_names[i], name) == 0)
		{
			return dvcman->plugins[i];
		}
	}

	return NULL;
}

ADDIN_ARGV* dvcman_get_plugin_data(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return ((DVCMAN_ENTRY_POINTS*) pEntryPoints)->args;
}

void* dvcman_get_rdp_settings(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return (void*) ((DVCMAN_ENTRY_POINTS*) pEntryPoints)->settings;
}

UINT32 dvcman_get_channel_id(IWTSVirtualChannel * channel)
{
	return ((DVCMAN_CHANNEL*) channel)->channel_id;
}

IWTSVirtualChannel* dvcman_find_channel_by_id(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId)
{
	int index;
	BOOL found = FALSE;
	DVCMAN_CHANNEL* channel;
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;

	ArrayList_Lock(dvcman->channels);

	index = 0;
	channel = (DVCMAN_CHANNEL*) ArrayList_GetItem(dvcman->channels, index++);

	while (channel)
	{
		if (channel->channel_id == ChannelId)
		{
			found = TRUE;
			break;
		}

		channel = (DVCMAN_CHANNEL*) ArrayList_GetItem(dvcman->channels, index++);
	}

	ArrayList_Unlock(dvcman->channels);

	return (found) ? ((IWTSVirtualChannel*) channel) : NULL;
}

void* dvcman_get_channel_interface_by_name(IWTSVirtualChannelManager* pChannelMgr, const char* ChannelName)
{
	int i;
	BOOL found = FALSE;
	void* pInterface = NULL;
	DVCMAN_LISTENER* listener;
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;

	for (i = 0; i < dvcman->num_listeners; i++)
	{
		listener = (DVCMAN_LISTENER*) dvcman->listeners[i];

		if (strcmp(listener->channel_name, ChannelName) == 0)
		{
			pInterface = listener->iface.pInterface;
			found = TRUE;
			break;
		}
	}

	return (found) ? pInterface : NULL;
}

IWTSVirtualChannelManager* dvcman_new(drdynvcPlugin* plugin)
{
	DVCMAN* dvcman;

	dvcman = (DVCMAN*) calloc(1, sizeof(DVCMAN));

	dvcman->iface.CreateListener = dvcman_create_listener;
	dvcman->iface.FindChannelById = dvcman_find_channel_by_id;
	dvcman->iface.GetChannelId = dvcman_get_channel_id;
	dvcman->drdynvc = plugin;
	dvcman->channels = ArrayList_New(TRUE);
	dvcman->pool = StreamPool_New(TRUE, 10);

	return (IWTSVirtualChannelManager*) dvcman;
}

int dvcman_load_addin(IWTSVirtualChannelManager* pChannelMgr, ADDIN_ARGV* args, rdpSettings* settings)
{
	DVCMAN_ENTRY_POINTS entryPoints;
	PDVC_PLUGIN_ENTRY pDVCPluginEntry = NULL;
	WLog_INFO(TAG,  "Loading Dynamic Virtual Channel %s", args->argv[0]);
	pDVCPluginEntry = (PDVC_PLUGIN_ENTRY) freerdp_load_channel_addin_entry(args->argv[0],
			NULL, NULL, FREERDP_ADDIN_CHANNEL_DYNAMIC);

	if (pDVCPluginEntry)
	{
		entryPoints.iface.RegisterPlugin = dvcman_register_plugin;
		entryPoints.iface.GetPlugin = dvcman_get_plugin;
		entryPoints.iface.GetPluginData = dvcman_get_plugin_data;
		entryPoints.iface.GetRdpSettings = dvcman_get_rdp_settings;
		entryPoints.dvcman = (DVCMAN*) pChannelMgr;
		entryPoints.args = args;
		entryPoints.settings = settings;

		pDVCPluginEntry((IDRDYNVC_ENTRY_POINTS*) &entryPoints);
	}

	return 0;
}

static void dvcman_channel_free(DVCMAN_CHANNEL* channel)
{
	if (channel->channel_callback)
		channel->channel_callback->OnClose(channel->channel_callback);

	DeleteCriticalSection(&(channel->lock));

	free(channel);
}

void dvcman_free(IWTSVirtualChannelManager* pChannelMgr)
{
	int i;
	int count;
	IWTSPlugin* pPlugin;
	DVCMAN_LISTENER* listener;
	DVCMAN_CHANNEL* channel;
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;

	ArrayList_Lock(dvcman->channels);

	count = ArrayList_Count(dvcman->channels);

	for (i = 0; i < count; i++)
	{
		channel = (DVCMAN_CHANNEL*) ArrayList_GetItem(dvcman->channels, i);
		dvcman_channel_free(channel);
	}

	ArrayList_Unlock(dvcman->channels);

	ArrayList_Free(dvcman->channels);

	for (i = 0; i < dvcman->num_listeners; i++)
	{
		listener = (DVCMAN_LISTENER*) dvcman->listeners[i];
		free(listener->channel_name);
		free(listener);
	}

	for (i = 0; i < dvcman->num_plugins; i++)
	{
		pPlugin = dvcman->plugins[i];

		if (pPlugin->Terminated)
			pPlugin->Terminated(pPlugin);
	}

	StreamPool_Free(dvcman->pool);
	free(dvcman);
}

int dvcman_init(IWTSVirtualChannelManager* pChannelMgr)
{
	int i;
	IWTSPlugin* pPlugin;
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;

	for (i = 0; i < dvcman->num_plugins; i++)
	{
		pPlugin = dvcman->plugins[i];

		if (pPlugin->Initialize)
			pPlugin->Initialize(pPlugin, pChannelMgr);
	}

	return 0;
}

static int dvcman_write_channel(IWTSVirtualChannel* pChannel, UINT32 cbSize, BYTE* pBuffer, void* pReserved)
{
	int status;
	DVCMAN_CHANNEL* channel = (DVCMAN_CHANNEL*) pChannel;

	EnterCriticalSection(&(channel->lock));

	status = drdynvc_write_data(channel->dvcman->drdynvc, channel->channel_id, pBuffer, cbSize);

	LeaveCriticalSection(&(channel->lock));

	return status;
}

static int dvcman_close_channel_iface(IWTSVirtualChannel* pChannel)
{
	DVCMAN_CHANNEL* channel = (DVCMAN_CHANNEL*) pChannel;
	DVCMAN* dvcman = channel->dvcman;

	DEBUG_DVC("id=%d", channel->channel_id);

	ArrayList_Remove(dvcman->channels, channel);

	dvcman_channel_free(channel);

	return 1;
}

int dvcman_create_channel(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId, const char* ChannelName)
{
	int i;
	int bAccept;
	DVCMAN_LISTENER* listener;
	DVCMAN_CHANNEL* channel;
	DrdynvcClientContext* context;
	IWTSVirtualChannelCallback* pCallback;
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;

	channel = (DVCMAN_CHANNEL*) calloc(1, sizeof(DVCMAN_CHANNEL));

	if (!channel)
		return -1;

	channel->dvcman = dvcman;
	channel->channel_id = ChannelId;
	channel->channel_name = _strdup(ChannelName);

	for (i = 0; i < dvcman->num_listeners; i++)
	{
		listener = (DVCMAN_LISTENER*) dvcman->listeners[i];

		if (strcmp(listener->channel_name, ChannelName) == 0)
		{
			channel->iface.Write = dvcman_write_channel;
			channel->iface.Close = dvcman_close_channel_iface;

			InitializeCriticalSection(&(channel->lock));

			bAccept = 1;
			pCallback = NULL;

			if (listener->listener_callback->OnNewChannelConnection(listener->listener_callback,
				(IWTSVirtualChannel*) channel, NULL, &bAccept, &pCallback) == 0 && bAccept == 1)
			{
				DEBUG_DVC("listener %s created new channel %d",
					  listener->channel_name, channel->channel_id);

				channel->status = 0;
				channel->channel_callback = pCallback;
				channel->pInterface = listener->iface.pInterface;

				ArrayList_Add(dvcman->channels, channel);

				context = dvcman->drdynvc->context;
				IFCALL(context->OnChannelConnected, context, ChannelName, listener->iface.pInterface);

				return 0;
			}
			else
			{
				WLog_ERR(TAG, "channel rejected by plugin");
				free(channel);
				return 1;
			}
		}
	}

	free(channel);
	return 1;
}

int dvcman_open_channel(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId)
{
	DVCMAN_CHANNEL* channel;
	IWTSVirtualChannelCallback* pCallback;

	channel = (DVCMAN_CHANNEL*) dvcman_find_channel_by_id(pChannelMgr, ChannelId);

	if (!channel)
	{
		WLog_ERR(TAG, "ChannelId %d not found!", ChannelId);
		return 1;
	}

	if (channel->status == 0)
	{
		pCallback = channel->channel_callback;
		if (pCallback->OnOpen)
			pCallback->OnOpen(pCallback);
	}

	return 0;
}

int dvcman_close_channel(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId)
{
	DVCMAN_CHANNEL* channel;
	IWTSVirtualChannel* ichannel;
	DrdynvcClientContext* context;
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;

	channel = (DVCMAN_CHANNEL*) dvcman_find_channel_by_id(pChannelMgr, ChannelId);

	if (!channel)
	{
		WLog_ERR(TAG, "ChannelId %d not found!", ChannelId);
		return 1;
	}

	if (channel->dvc_data)
	{
		Stream_Release(channel->dvc_data);
		channel->dvc_data = NULL;
	}

	if (channel->status == 0)
	{
		context = dvcman->drdynvc->context;

		IFCALL(context->OnChannelDisconnected, context, channel->channel_name, channel->pInterface);

		free(channel->channel_name);
		channel->channel_name = NULL;

		DEBUG_DVC("dvcman_close_channel: channel %d closed", ChannelId);
		ichannel = (IWTSVirtualChannel*) channel;
		ichannel->Close(ichannel);
	}

	return 0;
}

int dvcman_receive_channel_data_first(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId, UINT32 length)
{
	DVCMAN_CHANNEL* channel;

	channel = (DVCMAN_CHANNEL*) dvcman_find_channel_by_id(pChannelMgr, ChannelId);

	if (!channel)
	{
		WLog_ERR(TAG, "ChannelId %d not found!", ChannelId);
		return 1;
	}

	if (channel->dvc_data)
		Stream_Release(channel->dvc_data);

	channel->dvc_data = StreamPool_Take(channel->dvcman->pool, length);
	channel->dvc_data_length = length;

	return 0;
}

int dvcman_receive_channel_data(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId, wStream* data)
{
	int status = 0;
	DVCMAN_CHANNEL* channel;
	UINT32 dataSize = Stream_GetRemainingLength(data);

	channel = (DVCMAN_CHANNEL*) dvcman_find_channel_by_id(pChannelMgr, ChannelId);

	if (!channel)
	{
		WLog_ERR(TAG, "ChannelId %d not found!", ChannelId);
		return 1;
	}

	if (channel->dvc_data)
	{
		/* Fragmented data */
		if (Stream_GetPosition(channel->dvc_data) + dataSize > (UINT32) Stream_Capacity(channel->dvc_data))
		{
			WLog_ERR(TAG, "data exceeding declared length!");
			Stream_Release(channel->dvc_data);
			channel->dvc_data = NULL;
			return 1;
		}

		Stream_Write(channel->dvc_data, Stream_Pointer(data), dataSize);

		if (((size_t) Stream_GetPosition(channel->dvc_data)) >= channel->dvc_data_length)
		{
			Stream_SealLength(channel->dvc_data);
			Stream_SetPosition(channel->dvc_data, 0);
			status = channel->channel_callback->OnDataReceived(channel->channel_callback, channel->dvc_data);
			Stream_Release(channel->dvc_data);
			channel->dvc_data = NULL;
		}
	}
	else
	{
		status = channel->channel_callback->OnDataReceived(channel->channel_callback, data);
	}

	return status;
}

